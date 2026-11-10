using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32.SafeHandles;
using WinxCoop.SessionRuntime;
using P = WinxCoop.SessionRuntime.ProgressionNative;

namespace WinxCoop.SessionHost;

// Synchronous per-machine owner. Native Progression owns all network phases;
// this class supplies actual local game receipts and owns only local children.
internal static class SessionWorker
{
    internal static int Run(SessionConfiguration config) => new Owner(config).Run();

    private sealed class EarlyStop : Exception { }
    private sealed class Generation(BootstrapPeer.Assignment assignment, string directory, LocalChannels channels, RoleActor actor)
    {
        internal BootstrapPeer.Assignment Assignment = assignment;
        internal readonly string Directory = directory;
        internal readonly LocalChannels Channels = channels;
        internal readonly RoleActor Actor = actor;
        internal EntrySnapshotCapture? Entry;
        internal P.RetirementReceipt? Retirement;
        internal ulong ObservedHostPeerAppliedRevisionAtReady;
        internal bool Disposed;
    }

    private sealed class Owner(SessionConfiguration config)
    {
        private static readonly JsonSerializerOptions Json = new(SessionConfiguration.Json) { IncludeFields = true };
        private static readonly JsonSerializerOptions JournalJson = new(Json) { WriteIndented = false };
        private readonly long _started = Environment.TickCount64;
        private readonly List<Generation> _generations = [];
        private readonly List<object> _runningReceipts = [], _focusAttempts = [], _checkpointHistory = [];
        private readonly Dictionary<(uint Kind, ulong Operation), BootstrapPeer.OwnerNotice> _notices = [];
        private RuntimeBundle? _bundle;
        private GameProfile? _game;
        private RuntimeProcessPolicy? _policy;
        private UiCommandReader? _ui;
        private NativeCheckpointStore? _checkpoint;
        private BootstrapPeer? _peer;
        private SessionDiscovery? _advertisement;
        private SessionDiscovery.DiscoveryStatus? _finalAdvertisement;
        private P.Session? _core;
        private Generation? _current;
        private BootstrapPeer.Assignment _assignment;
        private P.Decision? _prepared;
        private P.Status? _finalCore;
        private BootstrapPeer.Status? _finalBootstrap;
        private UiCommand? _pendingUi;
        private byte[]? _outgoing;
        private FileStream? _journal;
        private SessionStatusJournal.Writer? _statusWriter;
        private string? _template, _error, _runningReceiptFile;
        private string? _advertisementError;
        private string _advertisementSkipReason = "";
        private string _phase = "starting", _message = "Подготавливаем сеанс…", _stopReason = "";
        private ulong _lastUiOperation, _controlSent, _controlReceived, _peerRetiredOperation;
        private uint _nativeControlPort;
        private uint _advertisedTcpPort;
        private bool _advertisementAttempted, _advertisementOccupied;
        private long _lastStatus;
        private long _statusWriteAttempts, _statusWriteTotalMilliseconds, _statusWriteMaximumMilliseconds;
        private bool _running, _everReady, _acceptUi, _stopRequested, _closing, _endedNotice,
            _transportEnded, _graceful, _success, _expectedEarlyStop, _sessionEndedSent, _sessionEndedDrained;

        internal int Run()
        {
            try
            {
                config.Validate(); Require(config.Worker && Directory.Exists(config.CaseDirectory), "Worker requires its prepared local case.");
                _journal = new FileStream(Path.Combine(config.CaseDirectory, "worker-events.jsonl"), FileMode.CreateNew, FileAccess.Write, FileShare.Read);
                _statusWriter = new(config.CaseDirectory);
                _ui = new(config); _bundle = new(config.BundleDirectory); _bundle.InstallNativeResolver();
                _policy = RuntimeProcessPolicy.CreateE7(_bundle.DirectoryPath, config.CaseDirectory, config.JobName, _bundle.ImagePins);
                VerifyOwnJob(config.JobName);
                SetPhase("qualifying", "Проверяем игру…");
                _game = new(config, _bundle, message => { ObserveUi(); if (_stopRequested) throw new EarlyStop(); SetPhase("qualifying", message); });
                _checkpoint = new(_game, config.CaseDirectory);
                if (config.Host) _template = SessionTemplateBuilder.Create(_game.Media, Path.Combine(config.CaseDirectory, "template"));
                _peer = new(config, _game.CompatibilityDigest);
                _nativeControlPort = _peer.Snapshot.BoundPort;
                StartAdvertisement();
                SetPhase(config.Host ? "listening" : "connecting", config.Host
                    ? "Лобби открыто. Можно запускать игру." + (_advertisementError is null ? "" : " Поиск в локальной сети недоступен; подключайтесь по адресу.")
                    : "Подключаемся к серверу…");
                if (config.Host) RunHost(); else RunGuest();
                _success = true;
            }
            catch (EarlyStop)
            {
                _expectedEarlyStop = true; _success = true; _running = false;
                Log("bounded-stop-before-native-ready", new { reason = _stopReason, actualNativeRetirement = false });
            }
            catch (Exception error)
            {
                _error = error.ToString(); _success = false; _running = false;
                try { _core?.Stop(P.Terminal.Disconnected); } catch (Exception) { }
                Log("worker-failed", new { error = _error });
            }
            finally
            {
                _acceptUi = false; _running = false;
                try { CleanupOwned(); }
                catch (Exception cleanup) { RecordFinalizationFailure("local-owned-cleanup", cleanup); }
                try
                {
                    try
                    {
                        SetPhase(_success ? "completed" : "failed", _success ? "Сеанс завершён." : "Сеанс завершён с ошибкой. Подробности сохранены.");
                    }
                    catch (Exception status) { RecordFinalizationFailure("final-status", status); }
                    try { _statusWriter?.Dispose(); }
                    catch (Exception statusClose) { RecordFinalizationFailure("final-status-journal-close", statusClose); }
                    // The exact native parent still needs the worker's failure
                    // report when its replaceable UI status remains unavailable.
                    try { WriteFinal(); }
                    catch (Exception report) { RecordFinalizationFailure("final-report", report); }
                }
                finally { _statusWriter?.Dispose(); _journal?.Dispose(); _ui?.Dispose(); _game?.Dispose(); _bundle?.Dispose(); }
            }
            return _success ? 0 : 4;
        }

        private void RunHost()
        {
            _acceptUi = true;
            Until(() => _pendingUi is { Action: "Start", Operation: > 0 }, 600, "server Start");
            _pendingUi = null; _acceptUi = false;
            _assignment = NewAssignment(null);
            Generation first = StartHost(_assignment, null, null, null, 1); _assignment = first.Assignment;
            Publish(first, null);
            CreateCore(_assignment); InitialReady(first);
            while (true)
            {
                _acceptUi = true; _running = true; SetPhase("running", "Можно играть.");
                Until(() => _stopRequested || _pendingUi is not null || HasNotice(5, null), 600, "server action");
                UiCommand? command = _pendingUi; _pendingUi = null; _acceptUi = false;
                bool shutdown = _stopRequested || HasNotice(5, null);
                if (!shutdown && command?.Action == "Start") { Log("ui-command-ignored", new { reason = "already-running", command.Operation }); continue; }
                bool load = !shutdown && command?.Action == "Load";
                if (load) _checkpoint!.VerifySlot();
                bool save = !load && (!shutdown || !_checkpoint!.Available);
                if (HostTransition(shutdown, save)) return;
            }
        }

        private void RunGuest()
        {
            BootstrapPeer.Assignment assignment = default; byte[] seed = [], witness = [], checkpoint = [];
            SetPhase("waiting-server", "Ожидаем запуска игры сервером…");
            Until(() => _peer!.TryRead(out assignment, out seed, out witness, out checkpoint), 180, "initial authoritative bootstrap");
            ValidateAssignment(assignment, 1); _assignment = assignment;
            CreateCore(assignment);
            Generation first = StartGuest(assignment, seed, witness); InitialReady(first);
            while (true)
            {
                _running = true; SetPhase("running", "Можно играть.");
                bool leaveSent = false;
                Until(() => _prepared is not null, 600, "server Prepare", () =>
                {
                    if (_stopRequested && !leaveSent)
                    { leaveSent = SendNotice(5, Math.Max(1, _lastUiOperation)); SetPhase("waiting-stop", "Ожидаем завершения сеанса сервером…"); }
                });
                if (GuestTransition()) return;
            }
        }

        private Generation StartHost(BootstrapPeer.Assignment assignment, byte[]? seed, byte[]? witness, string? savedSha, uint expectedOrdinal)
        {
            SetPhase("starting-game", "Запускаем игру…");
            Generation value = StartLocal(assignment, seed, witness, savedSha);
            using var decoder = new EntrySnapshotReader(_bundle!.DirectoryPath, _bundle.Hash("WinxCoop.ActorProbe.dll"), _bundle.Hash("SmoNativeValidator.Core.dll"));
            EntrySnapshotCapture? capture = null;
            Until(() => value.Actor.NativeReady && value.Actor.HeldNeutral &&
                (capture = decoder.TryRead(value.Channels.CaptureIdentity(28, expectedOrdinal))) is not null,
                90, "actual local Host entry/body/neutral");
            value.Entry = capture!;
            Require(seed is null || capture!.Seed.AsSpan().SequenceEqual(seed) && capture.Witness.AsSpan().SequenceEqual(witness),
                "Restarted Host changed the original authoritative seed/witness.");
            value.Assignment.InputPort = checked((uint)value.Channels.InputPort); value.Assignment.WorldPort = checked((uint)value.Channels.WorldPort);
            return value;
        }

        private Generation StartGuest(BootstrapPeer.Assignment assignment, byte[] seed, byte[] witness)
        {
            SetPhase("starting-game", "Загружаем уровень сервера…");
            return StartLocal(assignment, seed, witness, null);
        }

        private Generation StartLocal(BootstrapPeer.Assignment assignment, byte[]? seed, byte[]? witness, string? savedSha)
        {
            ValidateAssignment(assignment, assignment.LevelEpoch);
            string directory = Path.Combine(config.CaseDirectory, "generation-" + assignment.Generation.ToString("D4", CultureInfo.InvariantCulture));
            Require(!Directory.Exists(directory) && !File.Exists(directory), "Native generation output must be fresh."); Directory.CreateDirectory(directory);
            var channels = new LocalChannels(config, _bundle!, _policy!, assignment, directory);
            try { channels.Start(Pump); }
            catch { try { channels.StopOwned(); channels.Dispose(); } catch (Exception) { } throw; }
            RoleActor actor;
            try
            {
                JsonObject profile = SessionActorProfiles.Alfea(config, _game!, assignment.LevelEpoch, assignment.Generation, _template, seed, witness, savedSha);
                actor = new(config, _bundle!, _policy!, channels, profile, assignment.LevelEpoch, assignment.Generation, directory);
            }
            catch { channels.StopOwned(); channels.Dispose(); throw; }
            var value = new Generation(assignment, directory, channels, actor); _generations.Add(value); _current = value;
            actor.Start();
            Until(() => actor.Last("process-start") is not null, 15, "owned game process launch");
            FocusNewGame(value);
            return value;
        }

        private void Publish(Generation value, byte[]? checkpoint)
        {
            Require(config.Host && value.Entry is not null, "Only the actual Host entry owner can publish bootstrap bytes.");
            EntrySnapshotCapture entry = value.Entry!;
            if (_peer!.Snapshot.ProfileMatched != 1)
            {
                SetPhase("waiting-for-client", "Ожидаем клиента…");
                Until(() => _peer.Snapshot.ProfileMatched == 1, 180, "actual peer compatibility/profile handshake");
            }
            Until(() => _peer!.Publish(value.Assignment, entry.Seed, entry.Witness, checkpoint), 15, "authoritative bootstrap publication");
            _assignment = value.Assignment; _notices.Clear();
            Log("authoritative-bootstrap-published", new { assignment = _assignment, entry = entry.Evidence,
                checkpointBytes = checkpoint?.Length ?? 0, noNativeReadinessClaimed = true });
        }

        private void CreateCore(BootstrapPeer.Assignment initial)
        {
            Require(_core is null, "The native progression session is stable across process generations.");
            _core = new(new P.CreateArgs
            {
                Role = config.Host ? P.Role.Host : P.Role.Guest,
                Connection = new() { SessionId = initial.SessionId, ScenarioId = initial.ScenarioId, Token = initial.InputToken },
                InitialScene = initial.Scene, InitialDescriptor = NativeRetirementReceipts.Descriptor,
                InitialChannels = initial.Channels, TransitionTimeoutMilliseconds = 180000
            });
        }

        private void InitialReady(Generation value)
        {
            SetPhase("synchronizing", "Синхронизируем уровень…");
            bool observed = false; P.AppliedReceipt applied = default;
            Until(() =>
            {
                if (!observed)
                {
                    if (!config.Host)
                    {
                        if (!GuestApplied(value, out applied)) return false;
                        _peer!.ObserveReady(_assignment, true, applied.Revision, applied.NativeTick, true); observed = true;
                    }
                    else
                    {
                        BootstrapPeer.ReadyState ready = _peer!.Readiness;
                        ulong actualPeerApplied = value.Channels.HostPeerAppliedRevision;
                        if (ready.PeerReady == 0 || !value.Actor.HeldNeutral || !value.Actor.PrivateNeutral ||
                            actualPeerApplied < ready.PeerRevision) return false;
                        value.ObservedHostPeerAppliedRevisionAtReady = actualPeerApplied;
                        _peer.ObserveReady(_assignment, false, ready.PeerRevision, ready.PeerNativeTick, true); observed = true;
                    }
                }
                return _peer!.Readiness.BothReady == 1;
            }, 120, "both actual local-owner initial readiness receipts");
            ResumeActor(value); _everReady = true; _running = true;
            PreserveRunningReceipt(value, applied, initial: true);
        }

        private bool GuestApplied(Generation value, out P.AppliedReceipt applied)
        {
            applied = default;
            if (!value.Actor.NativeReady || !value.Actor.HeldNeutral || value.Actor.Count("world-guest-snapshot-native-applied") < 2 ||
                !value.Channels.TryReadGuestApplied(out applied)) return false;
            // This bit comes only from the separately observed native InputRET.
            applied.FreshNeutralInputBoundaryObserved = 1; return true;
        }

        private void ResumeActor(Generation value)
        {
            Require(_peer!.Readiness.BothReady == 1 && _core!.Snapshot.InputAllowed == 1,
                "Native game input cannot open before both distinct readiness/native progression barriers.");
            value.Actor.Command("Resume", 0);
            Until(() => value.Actor.Last("progression-actor-command")?["action"]?.GetValue<string>() == "Resume", 10, "actual local native Resume observation");
        }

        private bool HostTransition(bool shutdown, bool save)
        {
            Generation old = _current!; _running = false; _closing = shutdown;
            SetPhase(shutdown ? "stopping" : "restarting", shutdown ? "Завершаем сеанс…" : "Перезапускаем уровень…");
            ulong operation = checked(_core!.Snapshot.CompletedOperation + 1);
            var decision = new P.Decision { Operation = operation, Previous = _assignment.Scene,
                Next = new() { LevelEpoch = checked(_assignment.LevelEpoch + 1), Generation = checked(_assignment.Generation + 1) },
                Reason = P.Reason.Reload, Destination = NativeRetirementReceipts.Descriptor };
            int primaryBefore = old.Actor.Count("progression-input-native-neutral"), privateBefore = old.Actor.Count("template-progression-neutral-native-applied");
            _prepared = decision; _core.Begin(decision, BootstrapPeer.Now); old.Actor.Command("Hold", 0);
            Until(() => old.Actor.Count("progression-input-native-neutral") > primaryBefore &&
                old.Actor.Count("template-progression-neutral-native-applied") > privateBefore, 20, "fresh Host primary/private native neutral intervals");
            if (save)
            {
                // The Guest must still supply real held-neutral InputRET records.
                // Prepared Waiting stops that stream and cannot authorize this save.
                old.Actor.Command("Save", operation);
                Until(() => old.Actor.Last("progression-save-capture-writer-completed") is not null, 60, "original Host current-state capture and native writer");
                _checkpoint!.Capture(old.Actor, operation); _checkpointHistory.Add(_checkpoint.Evidence!);
            }
            else
            {
                // Preserve the actual same-operation native neutral interval
                // before prepared Waiting deliberately closes the source stream.
                _checkpoint!.VerifySlot(); old.Actor.Command("PrepareReload", operation);
                Until(() => old.Actor.Last("template-progression-pre-waiting-reload-prepared") is JsonObject receipt &&
                    NativeRetirementReceipts.U(receipt, "operation") == operation &&
                    NativeRetirementReceipts.B(receipt, "actualPrivateNativeNeutral") &&
                    NativeRetirementReceipts.B(receipt, "actualCurrentConnectionNativeWorldApplied"),
                    20, "actual same-operation pre-Waiting Host native reload preparation");
            }
            Until(() => SendNotice(3, operation), 10, "Host actual input-fenced notice");
            Until(() => HasNotice(1, operation), 30, "actual Guest prepared native wait");
            _checkpoint!.VerifySlot(); old.Actor.Command("Transition", operation);
            Until(() => old.Actor.Last("progression-native-saved-reload-requested") is not null, 20, "original whole native checkpoint load request");
            _checkpoint.VerifyNativeLoadRequest(old.Actor);
            Until(() => SendNotice(2, operation), 10, "actual Host request-issued notice");
            Until(() => old.Actor.Completed, 90, "Host original Exit and later native resource retirement");
            P.RetirementReceipt retired = NativeRetirementReceipts.Build(old.Actor, decision.Previous); old.Retirement = retired;
            JsonObject report = old.Actor.SuccessfulReport(), observer = NativeRetirementReceipts.Object(report, "observer");
            byte[] seed = Convert.FromBase64String(NativeRetirementReceipts.S(observer, "progressionNextSeed"));
            byte[] witness = Convert.FromBase64String(NativeRetirementReceipts.S(observer, "progressionNextWitness"));
            uint ordinal = checked((uint)NativeRetirementReceipts.U(NativeRetirementReceipts.Object(NativeRetirementReceipts.Object(observer, "progressionEntryRouter"), "currentScope"), "hostEntryOrdinal"));
            old.Channels.WaitStop(Pump); _core.ObserveRetired(retired, BootstrapPeer.Now); CloseGeneration(old);
            Until(() => _peerRetiredOperation == operation && _core.Snapshot.Phase == P.Phase.Preparing,
                30, "both actual native Retired receipts");
            if (shutdown || _stopRequested || HasNotice(5, null))
            {
                _closing = true; Until(() => SendNotice(4, operation), 10, "Host session-ended notice after both native retirements");
                _sessionEndedSent = true;
                DrainClosing(); _graceful = true; return true;
            }
            BootstrapPeer.Assignment next = NewAssignment(_assignment);
            Require(next.LevelEpoch == decision.Next.LevelEpoch && next.Generation == decision.Next.Generation, "Fresh channel generation differs from native decision.");
            _checkpoint.VerifySlot();
            Generation current = StartHost(next, seed, witness, _checkpoint.Sha256, ordinal);
            _assignment = current.Assignment;
            P.EntryReceipt entered = NativeRetirementReceipts.HostEntry(current.Actor, current.Entry!, _assignment);
            _core.ObserveHostEntry(entered, BootstrapPeer.Now); Publish(current, _checkpoint.Bytes);
            ReadyAfterRestart(current, null, null); _prepared = null; return false;
        }

        private bool GuestTransition()
        {
            Generation old = _current!; P.Decision decision = _prepared!.Value; _running = false;
            SetPhase("restarting", "Ожидаем перехода сервера…");
            int before = old.Actor.Count("progression-input-native-neutral"); old.Actor.Command("Hold", 0);
            Until(() => old.Actor.Count("progression-input-native-neutral") > before && HasNotice(3, decision.Operation),
                90, "fresh Guest native neutral and Host private fence/save");
            old.Actor.Command("Hold", decision.Operation);
            Until(() => old.Actor.Last("progression-guest-prepared-native-wait") is JsonObject prepared &&
                NativeRetirementReceipts.U(prepared, "operation") == decision.Operation &&
                NativeRetirementReceipts.B(prepared, "actualNativeWorldPreviouslyApplied") &&
                NativeRetirementReceipts.B(prepared, "bindingsAndQuarantineRetained"), 20, "actual Guest native prepared Waiting");
            Until(() => SendNotice(1, decision.Operation), 10, "Guest actual prepared notice");
            Until(() => HasNotice(2, decision.Operation), 90, "Host original checkpoint request before Guest retirement");
            old.Actor.Command("Retire", decision.Operation);
            Until(() => old.Actor.Completed, 90, "Guest original Exit and later native resources");
            P.RetirementReceipt retired = NativeRetirementReceipts.Build(old.Actor, decision.Previous); old.Retirement = retired;
            old.Channels.WaitStop(Pump); _core!.ObserveRetired(retired, BootstrapPeer.Now); CloseGeneration(old);
            BootstrapPeer.Assignment next = default; byte[] seed = [], witness = [], checkpoint = [];
            Until(() => HasNotice(4, decision.Operation) || _peer!.TryRead(out next, out seed, out witness, out checkpoint),
                180, "next actual bootstrap or fully retired session end");
            if (HasNotice(4, decision.Operation))
            { _closing = true; DrainClosing(); _graceful = true; return true; }
            ValidateAssignment(next, decision.Next.LevelEpoch);
            Require(next.Generation == decision.Next.Generation, "Bootstrap generation differs from native Prepare.");
            P.Message authorization = default;
            Until(() => _core.TryTakeLoad(BootstrapPeer.Now, out authorization), 20, "exactly-once native Load authorization");
            Require(authorization.Decision.Operation == decision.Operation && authorization.Decision.Next.Equals(next.Scene) &&
                authorization.Channels.Equals(next.Channels) && authorization.Seed.ToBytes().AsSpan().SequenceEqual(SHA256.HashData(seed)) &&
                authorization.Witness.ToBytes().AsSpan().SequenceEqual(SHA256.HashData(witness)),
                "Native Load authorization differs from validated transferred bytes/channel identities.");
            _assignment = next; _notices.Clear();
            Generation current = StartGuest(next, seed, witness); ReadyAfterRestart(current, seed, witness);
            _prepared = null; return false;
        }

        private void ReadyAfterRestart(Generation value, byte[]? seed, byte[]? witness)
        {
            SetPhase("synchronizing", "Синхронизируем новый уровень…");
            bool readyObserved = false, appliedObserved = false; P.AppliedReceipt applied = default;
            Until(() =>
            {
                if (!readyObserved)
                {
                    if (!config.Host)
                    {
                        if (!GuestApplied(value, out applied)) return false;
                        _peer!.ObserveReady(_assignment, true, applied.Revision, applied.NativeTick, true); readyObserved = true;
                    }
                    else
                    {
                        BootstrapPeer.ReadyState ready = _peer!.Readiness;
                        ulong actualPeerApplied = value.Channels.HostPeerAppliedRevision;
                        if (ready.PeerReady == 0 || !value.Actor.HeldNeutral || !value.Actor.PrivateNeutral ||
                            actualPeerApplied < ready.PeerRevision) return false;
                        value.ObservedHostPeerAppliedRevisionAtReady = actualPeerApplied;
                        _peer.ObserveReady(_assignment, false, ready.PeerRevision, ready.PeerNativeTick, true); readyObserved = true;
                    }
                }
                if (_peer!.Readiness.BothReady != 1) return false;
                if (!config.Host && !appliedObserved)
                {
                    P.EntryReceipt entered = NativeRetirementReceipts.GuestEntry(value.Actor, _assignment, seed!, witness!);
                    _core!.ObserveGuestApplied(entered, applied, BootstrapPeer.Now); appliedObserved = true;
                }
                return _core!.Snapshot.InputAllowed == 1 && _core.Snapshot.Phase == P.Phase.Running;
            }, 120, "new actual native full apply/neutral and committed progression Resume");
            ResumeActor(value); _running = true; PreserveRunningReceipt(value, applied, initial: false);
        }

        // Remaining methods keep UI leases, native carrier, receipts and child
        // cleanup separate; none of them implements the Progression codec/FSM.
        private void Pump()
        {
            ObserveUi();
            if (Environment.TickCount64 - _started >= (long)(config.Seconds + 90) * 1000)
                throw new TimeoutException("Owned session exhausted its cleanup reserve.");
            if (Environment.TickCount64 - _started >= (long)Math.Max(15, config.Seconds - 90) * 1000)
            { _stopRequested = true; if (_stopReason.Length == 0) _stopReason = "time-limit"; }
            if (_stopRequested && !_everReady) throw new EarlyStop();
            if (_current is { Disposed: false } current) current.Actor.Tick();
            if (_peer is not null && !_transportEnded)
            {
                bool live = _peer.Pump();
                // Native carrier drains positive frames before reporting EOF.
                // In particular, consume the final trusted SessionEnded notice
                // before classifying a peer's subsequent close as unexpected.
                for (int index = 0; index < 16 && _peer.TryReadNotice(out var notice); ++index)
                {
                    Require(notice.Role == (config.Host ? 2u : 1u) && notice.SessionId == _assignment.SessionId &&
                        notice.OldEpoch == _assignment.LevelEpoch && notice.OldGeneration == _assignment.Generation && notice.OperationId != 0,
                        "Received owner notice differs from the active assigned generation.");
                    Require(_notices.Count < 64 || _notices.ContainsKey((notice.Kind, notice.OperationId)), "Owner notice cache exceeded its bound.");
                    _notices[(notice.Kind, notice.OperationId)] = notice;
                    if (!config.Host && notice.Kind == 4) _endedNotice = true;
                    Log("peer-owner-notice", notice);
                }
                for (int index = 0; index < 16 && _peer.TryReadControl(out byte[] wire); ++index)
                {
                    Require(_core is not null, "Progression control arrived before its trusted initial bootstrap.");
                    P.Message accepted = _core!.Receive(wire, BootstrapPeer.Now); ++_controlReceived;
                    if (!config.Host && accepted.Kind == P.Kind.Prepare && _core.Snapshot.Phase == P.Phase.Preparing &&
                        accepted.Decision.Operation > _core.Snapshot.CompletedOperation)
                    {
                        Require(_everReady && accepted.Decision.Previous.Equals(_assignment.Scene) &&
                            accepted.Decision.Next.LevelEpoch == _assignment.LevelEpoch + 1 &&
                            accepted.Decision.Next.Generation == _assignment.Generation + 1 &&
                            accepted.Decision.Destination.Equals(NativeRetirementReceipts.Descriptor),
                            "Actual native Prepare cannot broaden the admitted running generation/profile.");
                        if (_prepared is not null) Require(_prepared.Value.Equals(accepted.Decision), "Active native decision changed.");
                        _prepared = accepted.Decision;
                    }
                    if (config.Host && accepted.Kind == P.Kind.Retired && _prepared is { } active &&
                        accepted.Decision.Equals(active) && _core.Snapshot.Terminal == P.Terminal.None && _core.Snapshot.InputAllowed == 0)
                        _peerRetiredOperation = accepted.Decision.Operation;
                    Log("native-progression-received", new { accepted.Kind, accepted.Decision.Operation,
                        accepted.RetirementMask, nativeStatus = _core.Snapshot });
                }
                if (!live)
                {
                    _core?.Stop(P.Terminal.Disconnected);
                    if (_current is null && (!config.Host && _endedNotice || config.Host && _closing && _sessionEndedSent && _sessionEndedDrained))
                        _transportEnded = true;
                    else throw new IOException("Peer transport closed before the complete native retirement/session-end boundary.");
                }
            }
            if (_core is not null && !_transportEnded)
            {
                Require(_core.Poll(BootstrapPeer.Now), "Native progression deadline/terminal: " + _core.Snapshot.Terminal);
                for (int index = 0; index < 8; ++index)
                {
                    if (_outgoing is null && _core.TryPeekWire(BootstrapPeer.Now, out byte[] wire, out _)) _outgoing = wire;
                    if (_outgoing is null || !_peer!.SendControl(_outgoing)) break;
                    // Native carrier now owns the full immutable244-byte copy.
                    // This is not a claim that the socket send or remote native
                    // application completed; those have separate owner gates.
                    _core.Commit(BootstrapPeer.Now); _outgoing = null; ++_controlSent;
                    Log("native-progression-carrier-owned", new { nativeStatus = _core.Snapshot,
                        immutableCarrierCopyAccepted = true, physicalSocketCompletionClaimed = false });
                }
            }
            PumpAdvertisement(); WriteStatus();
        }

        private void StartAdvertisement()
        {
            Require(_advertisement is null && !_advertisementAttempted, "Discovery advertisement has one local owner.");
            if (!config.Host) { _advertisementSkipReason = "guest-role"; return; }
            BootstrapPeer.Status actual = _peer!.Snapshot;
            Require(actual.Role == 1 && actual.Terminal == 0 && actual.BoundPort is > 0 and <= ushort.MaxValue && actual.OwnedSockets > 0,
                "A lobby can advertise only the actual native Host TCP listener.");
            // The discovery service replies on all interfaces. A listener
            // restricted to one address must not advertise unreachable replies
            // from another interface; the product UI uses INADDR_ANY.
            if (actual.BoundIpv4 != 0)
            {
                _advertisementSkipReason = actual.BoundIpv4 >> 24 == 127 ? "loopback-only-listener" : "interface-specific-listener";
                Log("lan-advertisement-skipped", new { reason = _advertisementSkipReason, actual.BoundIpv4, actual.BoundPort });
                return;
            }
            _advertisementAttempted = true; _advertisedTcpPort = actual.BoundPort;
            try
            {
                _advertisement = new SessionDiscovery();
                _advertisement.StartHosting(checked((ushort)actual.BoundPort), "WinxCoop");
                _advertisementOccupied = actual.ConnectionId != 0;
                _advertisement.SetOccupied(_advertisementOccupied);
                Log("lan-advertisement-started", new { actual.BoundIpv4, actual.BoundPort,
                    actualNativeTcpConnection = actual.ConnectionId, occupied = _advertisementOccupied,
                    gameplayReadinessClaimed = false });
            }
            catch (Exception error) { AdvertisementFailed(error); CloseAdvertisement(); }
        }

        private void PumpAdvertisement()
        {
            if (_advertisement is null) return;
            if (_closing || _transportEnded) { CloseAdvertisement(); return; }
            BootstrapPeer.Status actual = _peer!.Snapshot;
            if (actual.Terminal != 0) { CloseAdvertisement(); return; }
            try
            {
                bool occupied = actual.ConnectionId != 0;
                if (occupied != _advertisementOccupied)
                {
                    _advertisement.SetOccupied(occupied); _advertisementOccupied = occupied;
                    Log("lan-advertisement-occupancy", new { occupied, actualNativeTcpConnection = actual.ConnectionId,
                        gameplayReadinessClaimed = false });
                }
                _advertisement.Pump();
            }
            catch (Exception error) { AdvertisementFailed(error); CloseAdvertisement(); }
        }

        private void AdvertisementFailed(Exception error)
        {
            _advertisementError = (_advertisementError is null ? "" : _advertisementError + "\n") + error;
            Log("lan-advertisement-unavailable", new { error = error.ToString(), manualAddressConnectionRemainsAvailable = true,
                gameplayReadinessClaimed = false });
        }

        private void CloseAdvertisement()
        {
            if (_advertisement is null) return;
            _advertisement.Dispose(); _finalAdvertisement = _advertisement.Status; _advertisement = null;
            SessionDiscovery.DiscoveryStatus final = _finalAdvertisement.Value;
            Require(final.Hosting == 0 && final.Searching == 0 && final.OwnedSockets == 0 &&
                final.SocketsOpened == final.SocketsClosed && final.WsaStarts == final.WsaCleanups &&
                final.CloseErrors == 0 && final.WsaCleanupErrors == 0, "Native LAN discovery resources did not close cleanly.");
            Log("lan-advertisement-closed", new { nativeFinalStatus = final, nativeSocketAndWsaBalancesVerified = true });
        }

        private void ObserveUi()
        {
            if (_ui is null) return;
            UiCommand command = _ui.Poll();
            if (command.Operation <= _lastUiOperation) return;
            _lastUiOperation = command.Operation;
            if (command.Action == "Stop")
            { _stopRequested = true; _stopReason = "user"; Log("ui-stop-requested", new { command.Operation }); return; }
            Require(config.Host, "Only the server UI controls start/checkpoint/reload.");
            if (_acceptUi) _pendingUi = command;
            else Log("ui-command-ignored", new { command.Operation, command.Action, reason = "local-native-operation-in-progress" });
        }

        private void Until(Func<bool> condition, int seconds, string label, Action? waiting = null)
        {
            long end = Environment.TickCount64 + checked(seconds * 1000L);
            while (true)
            {
                // A coordinated pre-ready Stop may already have closed the
                // peer's MMF. Observe the owned UI command before a predicate
                // reads that terminal mapping; it authorizes only early abort.
                ObserveUi();
                if (_stopRequested && !_everReady) throw new EarlyStop();
                if (condition()) return;
                Require(Environment.TickCount64 < end, "Timed out: " + label);
                Pump(); waiting?.Invoke(); Thread.Sleep(5);
            }
        }

        private bool SendNotice(uint kind, ulong operation) => _peer!.SendNotice(_assignment, !config.Host, kind, operation);
        private bool HasNotice(uint kind, ulong? operation) => operation is { } exact ? _notices.ContainsKey((kind, exact)) : _notices.Keys.Any(key => key.Kind == kind);

        private BootstrapPeer.Assignment NewAssignment(BootstrapPeer.Assignment? previous)
        {
            var used = new HashSet<ulong>();
            if (previous is { } old)
                foreach (ulong value in new[] { old.SessionId, old.InputToken, old.WorldToken, old.HostInputNonce, old.HostWorldNonce, old.GuestInputNonce, old.GuestWorldNonce }) used.Add(value);
            ulong Fresh()
            {
                for (int attempt = 0; attempt < 32; ++attempt)
                {
                    ulong value = BitConverter.ToUInt64(RandomNumberGenerator.GetBytes(8), 0);
                    if (value != 0 && used.Add(value)) return value;
                }
                throw new InvalidDataException("Cannot generate a fresh bounded channel identity.");
            }
            ulong input;
            do { input = checked((ulong)RandomNumberGenerator.GetInt32(1, 65536)); } while (!used.Add(input));
            return new()
            {
                AbiVersion = 1, StructBytes = (uint)Marshal.SizeOf<BootstrapPeer.Assignment>(), SessionId = Fresh(),
                ScenarioId = previous?.ScenarioId ?? 280007, InputToken = input, WorldToken = Fresh(),
                HostInputNonce = Fresh(), HostWorldNonce = Fresh(), GuestInputNonce = Fresh(), GuestWorldNonce = Fresh(),
                LevelEpoch = previous is null ? 1 : checked(previous.Value.LevelEpoch + 1),
                Generation = previous is null ? 1 : checked(previous.Value.Generation + 1), Level = 28, Entry = 0,
                ControlPort = _peer!.Snapshot.BoundPort
            };
        }

        private static void ValidateAssignment(BootstrapPeer.Assignment value, uint expectedEpoch)
        {
            Require(value.Level == 28 && value.Entry == 0 && value.LevelEpoch == expectedEpoch && value.Generation == expectedEpoch &&
                value.SessionId != 0 && value.ScenarioId != 0 && value.InputToken is > 0 and <= ushort.MaxValue && value.WorldToken != 0 &&
                value.HostInputNonce != 0 && value.HostWorldNonce != 0 && value.GuestInputNonce != 0 && value.GuestWorldNonce != 0,
                "Native bootstrap does not name the admitted Alfea28 generation.");
        }

        private void PreserveRunningReceipt(Generation value, P.AppliedReceipt applied, bool initial)
        {
            BootstrapPeer.ReadyState ready = _peer!.Readiness;
            P.Session core = _core ?? throw new InvalidDataException("Running receipt has no native progression owner.");
            Require(ready.BothReady == 1 && core.Snapshot.InputAllowed == 1, "Running receipt lacks both actual owner/native progression gates.");
            var receipt = new
            {
                role = config.Role, levelEpoch = value.Assignment.LevelEpoch, generation = value.Assignment.Generation,
                assignment = value.Assignment, initial, bootstrapReadiness = ready, nativeProgression = core.Snapshot,
                actualGuestApplied = config.Host ? null : (object)applied,
                actualHostPeerAppliedRevision = config.Host ? value.ObservedHostPeerAppliedRevisionAtReady : 0,
                hostPeerAppliedRevisionObservedAtReadinessDecision = config.Host,
                heldNeutral = value.Actor.Last("progression-input-native-neutral"),
                privateNeutral = config.Host ? value.Actor.Last("template-progression-neutral-native-applied") : null,
                actualEntry = value.Actor.Last("world-entry-native-qualified"), nativeLoadReturn = value.Actor.Last("world-entry-load-returned"),
                actualNativeResume = value.Actor.Last("progression-actor-command"),
                actualNativeFullApplyCount = value.Actor.Count("world-guest-snapshot-native-applied"),
                nativeReceiptsAreFixtures = false, byteAcknowledgmentWasNotReadiness = true
            };
            string file = Path.Combine(value.Directory, "running-receipt.json"); WriteFresh(file, receipt);
            _runningReceipts.Add(new { path = file, sha256 = NativeRetirementReceipts.Hash(File.ReadAllBytes(file)),
                levelEpoch = value.Assignment.LevelEpoch, generation = value.Assignment.Generation });
            _runningReceiptFile = file;
            Log("actual-native-generation-running", new { path = file, levelEpoch = value.Assignment.LevelEpoch, generation = value.Assignment.Generation });
        }

        private void CloseGeneration(Generation value)
        {
            Require(value.Retirement is not null && value.Actor.Completed, "Only actual native retirement can close a generation normally.");
            value.Actor.Dispose(); value.Channels.Dispose(); value.Disposed = true;
            if (ReferenceEquals(_current, value)) _current = null;
        }

        private void DrainClosing()
        {
            _closing = true; long end = Environment.TickCount64 + 2000;
            while (!_transportEnded && _peer!.Snapshot.PendingOutbound != 0)
            { Require(Environment.TickCount64 < end, "Session-ended native carrier did not drain within2s."); Pump(); Thread.Sleep(5); }
            if (config.Host && _sessionEndedSent && !_transportEnded) _sessionEndedDrained = true;
            long settle = Environment.TickCount64 + 250;
            while (!_transportEnded && Environment.TickCount64 < settle) { Pump(); Thread.Sleep(5); }
        }

        private void CleanupOwned()
        {
            List<Exception> errors = [];
            try { CloseAdvertisement(); } catch (Exception error) { errors.Add(error); }
            foreach (Generation value in _generations.Where(value => !value.Disposed))
            {
                try
                {
                    if (value.Actor.Started)
                    {
                        if (!value.Actor.Child.Process.HasExited) value.Actor.Child.StopAsync().GetAwaiter().GetResult();
                        else value.Actor.Child.FinishDrainAsync(TimeSpan.FromSeconds(5)).GetAwaiter().GetResult();
                    }
                }
                catch (Exception error) { errors.Add(error); }
                try { value.Channels.StopOwned(); } catch (Exception error) { errors.Add(error); }
                try { value.Actor.Dispose(); } catch (Exception error) { errors.Add(error); }
                try { value.Channels.Dispose(); value.Disposed = true; } catch (Exception error) { errors.Add(error); }
            }
            _current = null;
            if (_core is not null)
            {
                try { _core.Stop(P.Terminal.Disconnected); _finalCore = _core.Snapshot; _core.Dispose(); }
                catch (Exception error) { errors.Add(error); }
            }
            if (_peer is not null)
            {
                try { _peer.Dispose(); _finalBootstrap = _peer.Snapshot; }
                catch (Exception error) { errors.Add(error); }
            }
            if (errors.Count != 0) throw new AggregateException("Local owned session cleanup failed; native outer Job cleanup remains authoritative.", errors);
        }

        private void SetPhase(string phase, string message)
        { _phase = phase; _message = message; WriteStatus(force: true); }

        private void WriteStatus(bool force = false)
        {
            long now = Environment.TickCount64;
            if (!force && now - _lastStatus < 200) return; _lastStatus = now;
            bool listening = config.Host && _peer is not null && !_transportEnded && _phase == "listening";
            bool actions = config.Host && _running && _acceptUi && !_stopRequested;
            object value = new
            {
                phase = _phase, running = _running, canStart = config.Host && _acceptUi && !_everReady && _current is null,
                canReload = actions, canSave = actions, canLoad = actions && _checkpoint?.Available == true,
                message = _message, error = _error, nativeGamePid = NativeGamePid(),
                listening, nativeControlPort = _nativeControlPort,
                lanAdvertisementActive = _advertisement is not null,
                levelEpoch = _assignment.LevelEpoch, epoch = _assignment.LevelEpoch, generation = _assignment.Generation,
                runningReceiptFile = _running ? _runningReceiptFile : null, monotonicMilliseconds = now,
                serverControlsProgression = true
            };
            long began = Environment.TickCount64;
            try
            {
                Require(_statusWriter is not null, "The local status journal writer was not created.");
                _statusWriter!.Write(value);
            }
            finally
            {
                long elapsed = Environment.TickCount64 - began;
                ++_statusWriteAttempts; _statusWriteTotalMilliseconds += elapsed;
                _statusWriteMaximumMilliseconds = Math.Max(_statusWriteMaximumMilliseconds, elapsed);
            }
        }

        private uint NativeGamePid()
        {
            if (_current is null || _current.Disposed || !_current.Actor.Started || _current.Actor.Child.Process.HasExited) return 0;
            return _current.Actor.Last("process-start")?["processPid"]?.GetValue<uint>() ?? 0;
        }

        private void WriteFinal()
        {
            bool retired = _generations.Count != 0 && _generations.All(value => value.Retirement is not null);
            var generations = _generations.Select(value =>
            {
                string reportPath = Path.Combine(value.Actor.Output, "report.json");
                JsonObject? native = value.Actor.Report;
                return new
                {
                    role = config.Role, levelEpoch = value.Assignment.LevelEpoch, generation = value.Assignment.Generation,
                    actorChild = value.Actor.Started ? value.Actor.Child.Report() : null,
                    nativeReportPath = reportPath, nativeReportSha256 = File.Exists(reportPath) ? NativeRetirementReceipts.Hash(File.ReadAllBytes(reportPath)) : null,
                    nativeResult = native?["result"], nativeCalls = native?["observer"]?["nativeCalls"],
                    retirementReceipt = value.Retirement, channels = value.Channels.Report(), localOwnersDisposed = value.Disposed
                };
            }).ToArray();
            WriteFresh(Path.Combine(config.CaseDirectory, "worker-report.json"), new
            {
                kind = "WinxCoop-E7-local-role-worker", success = _success, graceful = _graceful && retired,
                error = _error, actualNativeRetirement = retired, expectedStopBeforeReady = _expectedEarlyStop,
                stopReason = _stopReason, role = config.Role, elapsedMilliseconds = Environment.TickCount64 - _started,
                generations, runningGenerations = _runningReceipts, checkpoints = _checkpointHistory,
                nativeProgression = _finalCore, bootstrap = _finalBootstrap, focusAttempts = _focusAttempts,
                lanDiscovery = new { attempted = _advertisementAttempted, skippedReason = _advertisementSkipReason,
                    advertisedTcpPort = _advertisedTcpPort, error = _advertisementError, nativeFinalStatus = _finalAdvertisement,
                    occupiedMeansActualTcpConnection = true, occupiedMeansNativeGameReady = false },
                controlRecordsSent = _controlSent, controlRecordsReceived = _controlReceived,
                nativeReceiptsAreFixtures = false, controlCommitMeansImmutableCarrierOwnership = true,
                controlCommitClaimsPhysicalSocketCompletion = false, gracefulOriginalGameExitClaimed = false,
                nativeParentTerminatedByWorker = false, remoteProcessOrMappingReads = false,
                originalExecutablePatchedOnDisk = false, originalGameResourcesPatchedOnDisk = false,
                registryWrites = 0, preparedNativeSaveWrittenByOriginalGame = _checkpointHistory.Count != 0,
                statusWriteAttempts = _statusWriteAttempts, statusWriteTotalMilliseconds = _statusWriteTotalMilliseconds,
                statusWriteMaximumMilliseconds = _statusWriteMaximumMilliseconds, statusWriteDurationsIncludeAppendAndFlush = true,
                statusJournal = new { fileName = SessionStatusJournal.FileName, version = 1,
                    maximumBytes = SessionStatusJournal.MaxJournalBytes, maximumRecordBytes = SessionStatusJournal.MaxRecordBytes,
                    successfulRecords = _statusWriter?.RecordsWritten ?? 0, successfulBytes = _statusWriter?.BytesWritten ?? 0,
                    appendFailed = _statusWriter?.Failed ?? false, writerDisposed = _statusWriter?.Closed ?? false,
                    advisoryOnly = true, nativeReadinessOrAcknowledgmentGranted = false },
                replaceableStatusFlushToDisk = false, durableReceiptsFlushToDisk = true,
                nativeOuterJobZeroMustBeCheckedSeparately = true
            });
        }

        private void RecordFinalizationFailure(string stage, Exception error)
        {
            _success = false;
            _error = (_error is null ? "" : _error + "\n") + stage + ": " + error;
            _phase = "failed"; _message = "Сеанс завершён с ошибкой. Подробности сохранены.";
            try { Log("worker-finalization-failed", new { stage, error = error.ToString() }); }
            catch (Exception diagnostic)
            {
                // Preserve the earlier failure and continue to the independent
                // final report; a broken journal cannot grant a successful exit.
                _error += "\nfinalization-diagnostic: " + diagnostic;
            }
        }

        private void Log(string name, object data)
        {
            if (_journal is null) return;
            byte[] bytes = JsonSerializer.SerializeToUtf8Bytes(new { utc = DateTime.UtcNow, @event = name, data }, JournalJson);
            Require(_journal.Length + bytes.Length + 1 <= 32 * 1024 * 1024, "Worker diagnostic journal exceeded32MiB.");
            _journal.Write(bytes, 0, bytes.Length); _journal.WriteByte((byte)'\n'); _journal.Flush();
        }

        private static void WriteFresh(string path, object value, bool flushToDisk = true)
        {
            using var file = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            JsonSerializer.Serialize(file, value, Json); file.Flush(flushToDisk);
        }

        private void FocusNewGame(Generation value)
        {
            JsonObject started = value.Actor.Last("process-start")!;
            Require(NativeRetirementReceipts.S(started, "executable").Equals(_game!.Executable, StringComparison.OrdinalIgnoreCase),
                "New game focus requires the exact owned Actor's original executable.");
            uint pid = checked((uint)NativeRetirementReceipts.U(started, "processPid"));
            IntPtr window = IntPtr.Zero; long end = Environment.TickCount64 + 10000;
            while (window == IntPtr.Zero && Environment.TickCount64 < end)
            {
                EnumWindows((candidate, _) => { GetWindowThreadProcessId(candidate, out uint actual);
                    if (actual != pid || !IsWindowVisible(candidate)) return true; window = candidate; return false; }, IntPtr.Zero);
                if (window == IntPtr.Zero) { Pump(); Thread.Sleep(25); }
            }
            bool shown = window != IntPtr.Zero && ShowWindow(window, 9);
            bool foreground = window != IntPtr.Zero && SetForegroundWindow(window);
            _focusAttempts.Add(new { levelEpoch = value.Assignment.LevelEpoch, generation = value.Assignment.Generation,
                nativeGamePid = pid, windowFound = window != IntPtr.Zero, shown, foreground,
                syntheticKeys = false, readinessInferredFromFocus = false });
        }

        private static void VerifyOwnJob(string name)
        {
            using SafeWaitHandle job = OpenJobObject(4, false, name);
            Require(!job.IsInvalid, "Exact native-parent Job cannot be queried.");
            using Process self = Process.GetCurrentProcess();
            Require(IsProcessInJob(self.SafeHandle, job, out bool member) && member, "Worker is outside its exact native-parent Job.");
        }
        private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
        private delegate bool EnumWindowCallback(IntPtr window, IntPtr context);
        [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowCallback callback, IntPtr context);
        [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr window);
        [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
        [DllImport("user32.dll")] private static extern bool ShowWindow(IntPtr window, int show);
        [DllImport("user32.dll")] private static extern bool SetForegroundWindow(IntPtr window);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] private static extern SafeWaitHandle OpenJobObject(uint access, bool inherit, string name);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool IsProcessInJob(SafeProcessHandle process, SafeWaitHandle job, out bool member);
    }
}
