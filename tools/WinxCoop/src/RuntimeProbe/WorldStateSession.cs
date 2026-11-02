using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Composition of the isolated WCS1 channel and owned native readers/presentation.</summary>
internal sealed class WorldStateSession : IDisposable
{
    private readonly ProbeConfiguration _config;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private WorldStateIdentity _identity;
    private WorldStateClient _client;
    private readonly SharedProfileState _profile;
    private readonly WorldPresentationEntityReader _reader;
    private readonly Dictionary<uint, uint> _hostIds = [];
    private readonly Dictionary<uint, WorldPresentationVisual> _visuals = [];
    private readonly Dictionary<uint, WorldPresentationAnimation> _animations = [];
    private readonly Dictionary<uint, WorldPresentationBinding> _bindings = [];
    private WorldPresentationPoseApplier? _poses;
    private WorldStateDelivery? _pending;
    private SharedProfileScene _scene;
    private WorldPresentationNativeRig? _local;
    private bool _bound, _retired, _poseApplied;
    private uint _nextId = 3, _revision, _captures, _applications, _nativeTicks;
    private long _lastCapture;
    private readonly Queue<WorldPresentationNativeRig> _stockEnemiesToHide = [];
    private WorldPresentationCleanupPlan? _removal;
    private uint _removingId;
    private int _removalCall;
    private uint _stockHidden, _removed;
    private uint _flow, _transitionWorlds;
    private ulong _hostNativeApplied;
    private bool _peerRetired;
    private WorldPresentationStarPoolReader? _starInventory;
    private readonly WorldStarNativeTracker _starTracker;
    private WorldPresentationStarMirror? _starMirror;
    private WorldPresentationProjectileMirror? _projectileMirror;
    private byte[]? _admittedEntrySeed, _admittedEntryWitness;
    private uint _transportAttempt = 1;
    private object? _previousTransport;
    internal NativeWorldEntrySeed? Entry { private get; set; }
    internal WorldStateSession(ProbeConfiguration config, DebuggerHost host, PEReader image, Action<object> log)
    {
        _config = config; _host = host; _image = image; _log = log;
        _identity = config.DiagnosticWorldState ?? throw new InvalidDataException("Missing WorldState identity.");
        _client = new(_identity, log); _profile = new(host, log, _identity.Role == WorldStateRole.Guest);
        _reader = new(host, image);
        _starTracker = new(host, image, log, _identity.Role);
        _starTracker.InstallStopped();
    }
    internal bool Retired => _retired;
    internal bool HostTransportDetachedForReconnect => _identity.Role == WorldStateRole.Host && !_retired &&
        _bound && _peerRetired && _hostNativeApplied > 0 &&
        _client.Terminal is WorldStateTerminal.ActorClosed or WorldStateTerminal.PeerClosed;
    internal bool CurrentConnectionNativeApplied => !_retired && !_peerRetired && _hostNativeApplied > 0;
    internal bool GoalPassed => (!_config.EnableWorldEntry || Entry?.WorldQualified == true) &&
        (_identity.Role == WorldStateRole.Host ? _hostNativeApplied > 0 : _applications >= 2);
    internal object Summary => new { role = _identity.Role.ToString(), bound = _bound, retired = _retired,
        captures = _captures, applications = _applications, hostObservedNativeAppliedRevision = _hostNativeApplied,
        peerRetiredAfterNativeApply = _peerRetired, goalPassed = GoalPassed,
        transportAttempt = _transportAttempt, previousTransport = _previousTransport,
        nativeTransitionWorldsDeferred = _transitionWorlds,
        nativeObservationTicks = _nativeTicks, originalGuestEnemiesHidden = _stockHidden,
        passiveEnemiesRemoved = _removed, profile = _profile.Summary, ipc = _client.Summary,
        starPoolInventory = _starInventory?.Summary,
        starPresentationExcludedByNonAcceptanceMenuDiagnostic = _config.IsolatedMenuInputDiagnostic,
        starNativeTracker = _starTracker.Summary, starPresentation = _starMirror?.Summary,
        projectilePresentation = _projectileMirror?.Summary,
        entry = Entry?.Summary, authoritativeEntrySeedAdmitted = _admittedEntrySeed is not null,
        poses = _poses?.Summary, visuals = _visuals.Values.Select(v => v.Summary).ToArray(),
        animations = _animations.Values.Select(v => v.Summary).ToArray(),
        nativePointersTransmitted = false, physicsPredictionAdded = false };

    internal void ObserveHost(uint tid, ref Win32Native.X86Context context, NativeTemplateProbe template,
        NativeCallDispatcher calls)
    {
        if (_retired || _peerRetired || !template.WorldStateBodyReady || calls.Pending) return;
        if (!_client.SetReady(true))
        {
            if (_hostNativeApplied > 0 && _client.Terminal is WorldStateTerminal.ActorClosed or WorldStateTerminal.PeerClosed)
            { _peerRetired = true; _log(new { @event = "world-host-peer-retired-after-native-apply", revision = _hostNativeApplied }); return; }
            throw new InvalidDataException("World Host channel lost its native owner/lease before a qualified apply.");
        }
        if (_client.TryReadBridgeStatus(out var status)) _hostNativeApplied = Math.Max(_hostNativeApplied, status.AppliedRevision);
        if (!CanPresentAtWorld()) return;
        var bodies = template.ReadWorldStateBodies(tid, ref context, _reader, _identity.LevelEpoch, _identity.Generation);
        if (!_bound) Bind(tid, ref context, bodies.Scene, bodies.Primary.Parent);
        ++_nativeTicks;
        if (Environment.TickCount64 - _lastCapture < 50) return;
        _lastCapture = Environment.TickCount64;
        uint revision = checked(++_revision);
        WorldPresentationActorState[] players = [
            _reader.CaptureAtWorld(tid, ref context, bodies.Primary, 1),
            _reader.CaptureAtWorld(tid, ref context, bodies.Secondary, 2)];
        List<WorldPresentationActorState> entities = [];
        if (_scene.Level == 47)
            foreach (var rig in _reader.ReadBattleMosquitoesAtWorld(tid, ref context, _scene.Scene,
                bodies.Primary.Instance, bodies.Secondary.Instance))
            {
                if (!_hostIds.TryGetValue(rig.Instance, out uint id))
                { id = _nextId++; Require(id < 65536, "World stable-ID budget exceeded."); _hostIds.Add(rig.Instance, id); }
                entities.Add(_reader.CaptureAtWorld(tid, ref context, rig, id));
            }
        var presentIds = entities.Select(e => e.StableId).ToHashSet();
        foreach (uint owner in _hostIds.Where(p => !presentIds.Contains(p.Value)).Select(p => p.Key).ToArray())
            _hostIds.Remove(owner); // Reused native address receives a fresh stable ID.
        IReadOnlyList<WorldPresentationActorState> projectiles = _scene.Level == 47
            ? template.ReadWorldStateProjectiles(tid, ref context) : Array.Empty<WorldPresentationActorState>();
        Require(projectiles.Count <= 6 && entities.Count + projectiles.Count <= 64 &&
            projectiles.All(value => value.TypeId == WorldPresentationActorState.StingerType && value.StableId >= 65536),
            "World Host projectiles exceed the bounded original Battle47 pool or collide with enemy IDs.");
        entities.AddRange(projectiles);
        byte[] shared = _profile.CaptureAtWorld(tid, ref context, revision).CopyPayload();
        uint timer = U(0x755298); uint flags = Win32Native.ReadMemory(_host.ProcessHandle, timer + 0x40, 1)[0] == 1 ? 1u : 0u;
        IReadOnlyList<WorldStarState> stars = _scene.Level == 47 && !_config.IsolatedMenuInputDiagnostic
            ? _starTracker.CaptureAtWorld(tid, ref context, _starInventory!.ReadActiveAtWorld(tid, ref context, _scene.Scene))
            : Array.Empty<WorldStarState>();
        byte[]? entryWitness = null, entrySeed = null;
        if (_config.EnableWorldEntry)
        {
            Require(Entry?.WorldQualified == true, "World Host snapshot precedes its qualified original native entry.");
            entryWitness = Entry!.GetWitness(); entrySeed = Entry.GetSeed();
        }
        var snapshot = new WorldStateSnapshot(revision, _nativeTicks, flags, players, entities, shared,
            entryWitness, entrySeed, stars);
        if (_client.TryPublish(snapshot))
        {
            ++_captures;
            _log(new { @event = "world-host-snapshot-hash", revision, nativeTick = _nativeTicks,
                sha256 = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(WorldStateContract.PackLocalSnapshot(snapshot))),
                profileSha256 = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(shared)) });
            if (_captures <= 4 || _captures % 100 == 0)
                _log(new { @event = "world-host-snapshot-published", revision, nativeTick = _nativeTicks,
                    bodyCount = players.Length, entityCount = entities.Count, projectileCount = projectiles.Count,
                    starCount = stars.Count, flags, sharedProfileBytes = shared.Length,
                    bodies = players, entities });
        }
    }

    internal HostWorldTransportRebindWitness PrepareHostTransportRebindAtWorld(uint tid,
        ref Win32Native.X86Context context, NativeTemplateProbe template, NativeCallDispatcher calls)
    {
        Require(_transportAttempt == 1 && HostTransportDetachedForReconnect && !calls.Pending && _pending is null &&
            _host.IsAtDebugStop && tid == _host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn &&
            template.WorldStateBodyReady && CanPresentAtWorld(), "Host World transport cannot rebind outside its retained idle qualified scene.");
        var bodies = template.ReadWorldStateBodies(tid, ref context, _reader, _identity.LevelEpoch, _identity.Generation);
        Require(bodies.Scene == _scene, "Host World reconnect changed its original bound scene/Playing identity.");
        return new(_identity, _host.DebugStopSequence, _scene, bodies.Primary.Instance, bodies.Primary.Root,
            bodies.Secondary.Instance, bodies.Secondary.Root, bodies.Primary.Parent);
    }

    internal void CommitHostTransportRebindAtWorld(uint tid, ref Win32Native.X86Context context,
        WorldStateIdentity next, WorldStateClient client, HostWorldTransportRebindWitness witness,
        NativeTemplateProbe template, NativeCallDispatcher calls)
    {
        var actual = PrepareHostTransportRebindAtWorld(tid, ref context, template, calls);
        Require(actual == witness && _host.DebugStopSequence == witness.DebugStop &&
            next.Role == WorldStateRole.Host && next.SessionId != _identity.SessionId && next.ScenarioId == _identity.ScenarioId &&
            next.LevelEpoch == _identity.LevelEpoch && next.Generation == _identity.Generation &&
            client.Identity == next && !client.TerminalOrRetired,
            "Host World transport commit lost its exact original native witness or fresh endpoint.");
        _previousTransport = new { attempt = _transportAttempt, identity = _identity, ipc = _client.Summary,
            nativeAppliedRevision = _hostNativeApplied };
        _client.Dispose(); _client = client; _identity = next;
        _transportAttempt = 2; _peerRetired = false; _hostNativeApplied = 0; _lastCapture = 0;
        // Entity IDs are local to the fresh transport. Never confuse native
        // address reuse while disconnected with a still-live previous instance.
        _hostIds.Clear(); _nextId = 3;
        // Profile/Entry/Star/native bodies stay bound; World revision/tick continue.
    }

    internal void ObserveGuest(uint tid, ref Win32Native.X86Context context, ClientPresentationSession presentation,
        NativeVisualActor bloom, NativeCallDispatcher calls, Action resume)
    {
        if (_retired || !presentation.WorldQuarantineActive || calls.Pending) return;
        RefreshGuestAtWorld(tid, ref context, presentation);
        if (!CanPresentAtWorld()) return;
        if (!_bound)
        {
            uint profile = U(0x765AD4), instance = U(profile + 0x2B4);
            _local = _reader.ReadRigAtWorld(tid, ref context, instance, WorldPresentationActorState.BloomType);
            uint flow = U(0x755294), count = U(flow + 0x1AC); Require(count <= 20, "World Guest flow bound exceeded.");
            uint level = _config.WorldBattlePresentation ? 47u : 28u;
            uint table = _config.WorldBattlePresentation ? 0x7105C8u : 0x710874u;
            uint playing = 0;
            for (uint i = 0; i <= count; ++i) { uint item = U(flow + 0x15C + i * 4); if (U(item) == table && U(item + 0x10) == level) playing = item; }
            Require(playing != 0, "World Guest retained playing scene not found.");
            Bind(tid, ref context, new(_identity.LevelEpoch, _identity.Generation, level, playing, table, _local.Scene), _local.Parent);
            _poses = new(_host, () => presentation.WorldQuarantineActive, _log);
            _poses.BeginEpochAtWorld(tid, ref context, _identity.LevelEpoch, _local.Scene);
            if (level == 47)
            {
                _starMirror = new(_host, _image, () => presentation.WorldQuarantineActive, _log);
                _starMirror.BeginEpochAtWorld(tid, ref context, _identity.LevelEpoch, _local.Scene, _local.Parent);
            }
            AddBinding(tid, ref context, new(2, WorldPresentationActorState.BloomType, _local.Root,
                _local.Actor, _local.Scene, _local.Parent, false, _local), presentation);
            if (level == 47)
            {
                IReadOnlyList<WorldPresentationNativeRig> stockEnemies =
                    _reader.ReadBattleMosquitoesAtWorld(tid, ref context, _local.Scene, _local.Instance, 0);
                _projectileMirror = new(_host, _image, () => presentation.WorldQuarantineActive, _log);
                _projectileMirror.BeginEpochAtWorld(tid, ref context, _identity.LevelEpoch, _local.Scene, stockEnemies);
                foreach (var enemy in stockEnemies)
                    _stockEnemiesToHide.Enqueue(enemy);
            }
        }
        if (_stockEnemiesToHide.TryDequeue(out var hidden))
        {
            uint fn = 0x421640;
            byte[] pin = _image.GetSectionData(checked((int)(fn - 0x400000))).GetContent(0, 8).ToArray();
            Require(Win32Native.ReadMemory(_host.ProcessHandle, fn, pin.Length).SequenceEqual(pin), "Original Node enable function changed.");
            calls.Begin(_host, tid, ref context, fn, hidden.Root, [0, 1], false, _ =>
            {
                Require((U(hidden.Root + 0xB0) & 0x200) == 0, "Guest native recursive Node disable failed.");
                ++_stockHidden; _log(new { @event = "world-guest-original-enemy-hidden", nativeRecursiveNodeDisable = true,
                    originalInstanceRetainedUnderQuarantine = true, ordinal = _stockHidden }); resume();
            });
            return;
        }
        if (!bloom.WorldReady) return;
        if (!_bindings.ContainsKey(1)) AddBinding(tid, ref context, bloom.WorldBinding, presentation);
        if (_pending is null && _client.TryRead(out WorldStateDelivery delivery))
        { _pending = delivery; _poseApplied = false; }
        if (_pending is null) return;
        WorldStateSnapshot snapshot = _pending.Snapshot;
        VerifyAuthoritativeEntry(snapshot);
        IReadOnlyList<WorldStarState> stars = snapshot.Stars ??
            throw new InvalidDataException("World Guest snapshot omitted its explicit complete Star block.");
        Require(_scene.Level == 47 || stars.Count == 0,
            "World Guest Star presentation is admitted only in its qualified Battle47 scene.");
        WorldPresentationActorState[] enemies = snapshot.Entities.Where(value => value.TypeId == WorldPresentationActorState.MosquitoType).ToArray();
        WorldPresentationActorState[] projectiles = snapshot.Entities.Where(value => value.TypeId == WorldPresentationActorState.StingerType).ToArray();
        Require(snapshot.Bodies.Count == 2 && snapshot.Bodies.Any(b => b.StableId == 1) &&
            snapshot.Bodies.Any(b => b.StableId == 2) && snapshot.Bodies.All(b => b.TypeId == WorldPresentationActorState.BloomType) &&
            snapshot.Entities.Count <= 64 && enemies.Length <= 16 && projectiles.Length <= 6 &&
            enemies.Length + projectiles.Length == snapshot.Entities.Count &&
            enemies.All(value => value.StableId > 2 && value.StableId < 65536) &&
            projectiles.All(value => value.StableId >= 65536) && (_scene.Level == 47 || projectiles.Length == 0),
            "World Guest snapshot is outside its two-body/16-Mosquito/six-Stinger native cohort.");
        uint[] removed = _bindings.Keys.Where(id => id > 2 && !enemies.Any(e => e.StableId == id)).ToArray();
        if (_removal is not null || removed.Length > 0)
        {
            if (_removal is null)
            {
                _removingId = removed[0];
                _animations[_removingId].RetireBeforeUnload(tid, ref context, false);
                _animations.Remove(_removingId); _poses!.UnbindAtWorld(tid, ref context, _removingId);
                _bindings.Remove(_removingId);
                _removal = _visuals[_removingId].PrepareCleanupAtStop(tid, ref context); _removalCall = 0;
                var plan = _removal;
                BreakpointCallback witness = null!;
                witness = (uint wt, uint wa, ref Win32Native.X86Context wc) =>
                {
                    if (wc.Ecx == plan.DeletedRoot) plan.ObserveDeletion(wt, wc);
                    else _host.Breakpoints.AddReturn(wa, wt, "World.remove-enemy", witness);
                };
                _host.Breakpoints.AddReturn(plan.DeletingEntry, tid, "World.remove-enemy", witness);
            }
            var operation = _removal.Calls[_removalCall];
            calls.Begin(_host, tid, ref context, operation.Function, operation.ThisPointer, operation.Arguments,
                operation.Cdecl, _ =>
                {
                    if (++_removalCall == _removal.Calls.Count)
                    { _removal.Complete(); _removal = null; ++_removed; }
                    resume();
                });
            return;
        }
        // New passive entities are created on the original thread. The full
        // delivery remains pending until all of them and their clips are ready.
        foreach (var entity in enemies)
        {
            if (_bindings.ContainsKey(entity.StableId)) continue;
            if (!_visuals.TryGetValue(entity.StableId, out var visual))
            { visual = new(_host, _image, _log, _config.MediaDirectory, entity.StableId); _visuals.Add(entity.StableId, visual); }
            if (visual.StepCreateAtWorld(tid, ref context, calls, resume, _local!.Parent, _local.Scene, _local.Root, entity)) return;
            if (!visual.Ready) return;
            AddBinding(tid, ref context, visual.BindingAtWorld(tid, ref context), presentation);
        }
        if (!_poseApplied)
        {
            Require(_applications < 100000 && snapshot.Revision <= uint.MaxValue, "World native apply budget exceeded.");
            _profile.ApplyAtWorld(tid, ref context, new(_scene.LevelEpoch, _scene.BodyGeneration, _scene.Level,
                (uint)snapshot.Revision, snapshot.SharedProfile));
            foreach (var actor in snapshot.Bodies.Concat(enemies))
                _poses!.ApplyAtWorld(tid, ref context, _identity.LevelEpoch, actor);
            presentation.AcceptAuthoritativeRootAtWorld(tid, ref context);
            _poseApplied = true;
        }
        foreach (var actor in snapshot.Bodies.Concat(enemies))
        {
            var animation = _animations[actor.StableId];
            if (animation.StepAtWorld(tid, ref context, actor.Animation, calls, resume)) return;
            if (!animation.IsApplied(actor.Animation)) return;
        }
        if (_starMirror is not null)
        {
            _starMirror.BeginApplyAtWorld(tid, ref context, _identity.LevelEpoch, snapshot.Revision, stars);
            if (_starMirror.StepAtWorld(tid, ref context, calls, resume)) return;
            Require(_starMirror.ApplyComplete && _starMirror.AppliedRevision == snapshot.Revision,
                "World Star native apply is incomplete for this exact pending delivery.");
        }
        if (_projectileMirror is not null)
        {
            _projectileMirror.BeginApplyAtWorld(tid, ref context, _identity.LevelEpoch, snapshot.Revision, projectiles);
            if (_projectileMirror.StepAtWorld(tid, ref context, calls, resume)) return;
            Require(_projectileMirror.ApplyComplete && _projectileMirror.AppliedRevision == snapshot.Revision,
                "World projectile native visibility is incomplete for this exact pending delivery.");
        }
        Require(_client.Acknowledge(_pending), "World native applied ACK rejected its exact pending delivery.");
        ++_applications;
        _log(new { @event = "world-guest-snapshot-native-applied", revision = snapshot.Revision,
            sha256 = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(WorldStateContract.PackLocalSnapshot(snapshot))),
            profileSha256 = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(snapshot.SharedProfile)),
            snapshot.NativeTick, snapshot.Flags, nativeWorldBoundary = true, profileReadbackVerified = true,
            bodies = snapshot.Bodies, entities = snapshot.Entities, stars,
            starNativeVisibilityComplete = _starMirror is null || _starMirror.ApplyComplete,
            projectileNativeVisibilityComplete = _projectileMirror is null || _projectileMirror.ApplyComplete,
            nativeAppliedCount = _applications });
        _pending = null; _poseApplied = false;
    }

    internal void RefreshGuestAtWorld(uint tid, ref Win32Native.X86Context context, ClientPresentationSession presentation)
    {
        Require(!_retired && presentation.WorldQuarantineActive && _host.IsAtDebugStop &&
            tid == _host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn,
            "World Guest lease requires its actual quarantined native WorldRET.");
        Require(_client.SetReady(true), "World Guest channel lost its native owner/lease.");
        _poses?.ObserveFollowingWorldCaches(tid, ref context);
        _starMirror?.ObserveFollowingWorldCaches(tid, ref context);
        _projectileMirror?.ObserveFollowingWorldCaches(tid, ref context);
        if (_pending is { } pending)
            Require(_client.TryReadBridgeStatus(out var status) &&
                (status.Phase is WorldStatePhase.Connected or WorldStatePhase.Ready) &&
                status.ConnectionToken == pending.ConnectionToken && status.SyncEpoch == pending.SyncEpoch &&
                status.ReceivedRevision == pending.Snapshot.Revision,
                "World pending apply lost its exact live epoch/token/revision before native work.");
    }
    private void AddBinding(uint tid, ref Win32Native.X86Context context, WorldPresentationBinding binding,
        ClientPresentationSession presentation)
    {
        _poses!.BindAtWorld(tid, ref context, binding); _bindings.Add(binding.StableId, binding);
        _animations.Add(binding.StableId, new(_host, _image, () => presentation.WorldQuarantineActive, _log, binding));
    }
    private void Bind(uint tid, ref Win32Native.X86Context context, SharedProfileScene scene, uint parent)
    {
        _scene = scene; _flow = U(0x755294); _profile.BindAtWorld(tid, ref context, scene); _bound = true;
        _log(new { @event = "world-native-scene-bound", role = _identity.Role.ToString(), scene.Level,
            scene.LevelEpoch, scene.BodyGeneration, nativeAddressesExcludedFromWire = true });
        if (scene.Level == 47)
        {
            // This scene's inactive borrowed pools were separately qualified.
            // Failure is terminal now that Stars participate in a complete ACK.
            var inventory = new WorldPresentationStarPoolReader(_host, _image);
            try
            {
                long started = Environment.TickCount64;
                var observed = inventory.InventorySlotsAtWorld(tid, ref context, scene.Scene, inspectInactiveNodes: true);
                _starInventory = inventory;
                Require(observed.RepeatedStarAddresses == 0 && observed.RepeatedNodeAddresses == 0,
                    "World Star inventory contains aliased pool objects/Nodes.");
                _starTracker.BindAtWorld(tid, ref context, scene.Scene, parent, scene.LevelEpoch);
                _log(new { @event = "world-star-pool-inventory", role = _identity.Role.ToString(),
                    elapsedMilliseconds = Environment.TickCount64 - started, observed.OptionalPool55,
                    totalSlots = observed.Entries.Count, nonnullSlots = observed.Entries.Count(value => value.Star != 0),
                    activeSlots = observed.Entries.Count(value => value.Active40 != 0),
                    nodesInScene = observed.Entries.Count(value => value.Node?.InExpectedScene == true),
                    observed.RepeatedStarAddresses, observed.RepeatedNodeAddresses,
                    samples = observed.Entries.Where(value => value.Star != 0).GroupBy(value => value.Category)
                        .SelectMany(group => group.Take(3)).ToArray(), nativeWrites = 0, mirrorQualified = false });
            }
            catch (Exception error) when (error is InvalidDataException or System.ComponentModel.Win32Exception)
            {
                _log(new { @event = "world-star-pool-inventory-unqualified", role = _identity.Role.ToString(),
                    error = error.ToString(), nativeWrites = 0, snapshotAcknowledgmentPermitted = false });
                throw;
            }
        }
    }
    private void VerifyAuthoritativeEntry(WorldStateSnapshot snapshot)
    {
        if (!_config.EnableWorldEntry) return;
        byte[]? seed = snapshot.EntrySeed, witness = snapshot.EntryWitness;
        Require(seed is not null && witness is not null && Entry?.WorldQualified == true &&
            _config.WorldEntrySeed is not null && _config.WorldEntryExpectedWitness is not null,
            "World Guest authoritative entry lacks the admitted seed/native witness.");
        Require(seed!.SequenceEqual(_config.WorldEntrySeed!) && witness!.SequenceEqual(_config.WorldEntryExpectedWitness!),
            "World Guest snapshot changed the configured initial authoritative entry seed/witness.");
        WorldEntrySeedState state = WorldEntrySeedState.Unpack(seed!);
        Require(state.LevelEpoch == _identity.LevelEpoch && state.Generation == _identity.Generation && state.Level == _scene.Level,
            "World Guest authoritative entry belongs to another native scene epoch/generation.");
        Entry!.VerifyExpectedWitness(witness!);
        if (_admittedEntrySeed is null)
        { _admittedEntrySeed = (byte[])seed!.Clone(); _admittedEntryWitness = (byte[])witness!.Clone(); }
        else Require(seed!.SequenceEqual(_admittedEntrySeed) && witness!.SequenceEqual(_admittedEntryWitness!),
            "A later World snapshot replaced the already admitted initial native entry.");
    }
    internal bool CanPresentAtWorld()
    {
        uint flow = U(0x755294);
        Require(flow >= 0x10000 && (!_bound || flow == _flow), "World native flow owner changed.");
        uint pending = U(flow + 0x1B8), stack = U(flow + 0x1AC);
        Require(stack <= 20, "World transition stack exceeds its native bound.");
        uint top = U(flow + 0x15C + stack * 4);
        Require(top >= 0x10000, "World transition has no typed top state.");
        if (pending != 0 || U(flow + 0x1B0) != U(top + 0x10))
        {
            ++_transitionWorlds;
            if (_transitionWorlds <= 2 || _transitionWorlds % 1200 == 0)
                _log(new { @event = "world-native-transition-deferred", pending,
                    current = U(flow + 0x1B0), nativeWorlds = _transitionWorlds,
                    snapshotApplied = false, nativeApplyAcknowledged = false, worldEpochRearmed = false });
            return false;
        }
        return true;
    }

    internal bool TryBeginGuestUnload(uint tid, ref Win32Native.X86Context context, uint entry, byte firstByte,
        NativeVisualActor? bloom, NativeCallDispatcher calls, ClientPresentationSession presentation, Action retireInput)
    {
        if (_retired || _identity.Role != WorldStateRole.Guest || !_bound) return false;
        if (entry != (_scene.Level == 47 ? 0x5F2300u : 0x5F86C0u)) return false;
        Require(!calls.Pending, "World unload interrupted a private native transaction.");
        WorldStarRetirementPlan? starPlan = _starMirror?.PrepareRetirementAtStop(tid, ref context, entry);
        WorldProjectileRetirementPlan? projectilePlan = _projectileMirror?.PrepareRetirementAtStop(tid, ref context, entry);
        RetireAtNativeBoundary(tid, ref context, entry);
        List<WorldPresentationCleanupPlan> plans = [];
        if (bloom?.HasActor == true) plans.Add(bloom.PrepareWorldCleanup(_host, tid, ref context, calls.Pending));
        foreach (var visual in _visuals.Values.Where(v => !v.Deleted))
            plans.Add(visual.PrepareCleanupAtStop(tid, ref context));
        Require(presentation.TryBeginUnload(tid, ref context, entry, retireInput),
            "World cleanup did not restore its Guest native quarantine.");
        NativeCallSpec[] cleanupCalls = (starPlan?.Calls ?? Array.Empty<NativeCallSpec>())
            .Concat(projectilePlan?.Calls ?? Array.Empty<NativeCallSpec>())
            .Concat(plans.SelectMany(plan => plan.Calls)).ToArray();
        Require(cleanupCalls.Length <= 80, "World aggregate visual cleanup exceeds its explicit 80-call budget.");
        if (cleanupCalls.Length == 0) { starPlan?.Complete(); projectilePlan?.Complete(); return false; }
        foreach (var group in plans.GroupBy(p => p.DeletingEntry))
        {
            var remaining = group.ToDictionary(p => p.DeletedRoot);
            BreakpointCallback witness = null!;
            witness = (uint wt, uint wa, ref Win32Native.X86Context wc) =>
            {
                if (remaining.Remove(wc.Ecx, out var plan)) plan.ObserveDeletion(wt, wc);
                if (remaining.Count > 0) _host.Breakpoints.AddReturn(wa, wt, "World.aggregate-delete", witness);
            };
            _host.Breakpoints.AddReturn(group.Key, tid, "World.aggregate-delete", witness);
        }
        calls.BeginAtEntry(_host, tid, ref context, entry, firstByte, cleanupCalls,
            _ => { starPlan?.Complete(); projectilePlan?.Complete(); foreach (var plan in plans) plan.Complete(); }, maximumCalls: 80);
        return true;
    }

    internal void RetireAtNativeBoundary(uint tid, ref Win32Native.X86Context context, uint entry)
    {
        if (_retired) return;
        if (_bound)
        {
            foreach (var animation in _animations.Values) animation.RetireBeforeUnload(tid, ref context, false);
            _poses?.RetireBeforeUnload(tid, ref context);
            _profile.RetireAtStoppedBoundary(tid, ref context, entry);
            _starInventory?.RetireAtStop(tid, ref context);
        }
        _starTracker.RetireAtStop(tid, ref context, entry);
        Entry?.RetireCached();
        _client.Retire(); _retired = true; _pending = null;
        _log(new { @event = "world-native-scene-retired", role = _identity.Role.ToString(), entry,
            captures = _captures, applications = _applications, oldOwnerWritesNowForbidden = true });
    }
    internal void ObserveHostSceneExit(uint tid, ref Win32Native.X86Context context, uint entry)
    {
        if (_identity.Role != WorldStateRole.Host || _retired || !_bound ||
            entry != (_scene.Level == 47 ? 0x5F2300u : 0x5F86C0u)) return;
        Require(context.Ecx == _scene.PlayingState, "World Host exit receiver changed its captured scene.");
        RetireAtNativeBoundary(tid, ref context, entry);
    }
    public void Dispose() { _client.Dispose(); }
    private uint U(uint address) => Win32Native.ReadUInt32(_host.ProcessHandle, address);
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
