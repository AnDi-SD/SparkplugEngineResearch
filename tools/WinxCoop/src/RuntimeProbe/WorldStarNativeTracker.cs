using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own read-only observer of original Star Launch/Award calls. The host assigns
/// an opaque generation only after a paired native Launch return. The guest
/// audits that its quarantined scene runs neither native operation. This class
/// does not simulate a pickup, call native code, or own native storage.
/// </summary>
internal sealed class WorldStarNativeTracker
{
    private const uint LaunchEntry = 0x58CA50, LaunchRet = 0x58CC8D, AwardEntry = 0x58C270;
    private const uint BallisticEntry = 0x58C170, BallisticRet = 0x58C265;
    private const int MaximumBallisticCalls = 2;
    private const uint StarTable = 0x702160, NodeTable = 0x6DC4F4, SceneTable = 0x6E7358;
    private const uint ProfileSlot = 0x765AD4, ProfileTable = 0x6F5A44;
    private const uint MaximumOperations = 100000, MaximumIdentities = 4096;
    private const int MaximumSamples = 64, MaximumPoolSlots = 250;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly WorldStateRole _role;
    private readonly int _ownerThread;
    private readonly uint _pid, _thread;
    private readonly IntPtr _process;
    private readonly Dictionary<uint, Activation> _activations = [];
    private readonly Dictionary<uint, ObservedPoolSlot> _observedPoolSlots = [];
    private readonly HashSet<ulong> _reportedInactiveActivations = [];
    private readonly WorldStarPresentationQuarantine? _positionQuarantine;
    private readonly List<object> _samples = [];
    private LaunchFrame? _launch;
    private AwardFrame? _award;
    private Activation? _ballisticActivation;
    private BallisticFrame? _ballistic;
    private uint _ballisticEntries, _ballisticReturns;
    private bool _ballisticArmed;
    private uint _scene, _parent, _epoch, _profile, _primaryInstance, _primaryRoot, _manager;
    private uint _launchEntries, _launchReturns, _awardEntries, _awardReturns, _worlds;
    private uint _ignoredPreBindGuestLaunches, _ignoredPreBindAwards, _guestNativeViolations;
    private uint _retireEntry, _publishedRecords;
    private uint _lastAwardBefore, _lastAwardAfter;
    private long _lastAwardDelta;
    private ulong _nextActivation, _lastAwardActivation;
    private bool _installed, _bound, _retired, _failed;

    private sealed record Activation(ulong Id, uint Star, uint Node);
    private sealed record ObservedPoolSlot(ulong ActivationId, uint Star, uint Node, uint Category, uint Slot, uint Address);
    private sealed record LaunchFrame(uint Star, uint Node, uint Esp, uint Caller, uint Esi, uint Edi,
        byte[] CallerBytes);
    private sealed record AwardFrame(uint Star, uint Node, uint Esp, uint Caller, uint Esi, uint Edi,
        uint CounterBefore, ulong ActivationId);
    private sealed record BallisticFrame(Activation Activation, uint Esp, uint Caller, uint Output,
        uint Esi, uint Edi, uint Ebx, uint Ebp, byte[] CallerBytes, byte[] Arguments, uint Ordinal);

    internal WorldStarNativeTracker(DebuggerHost host, PEReader image, Action<object> log, WorldStateRole role)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        Require(role is WorldStateRole.Host or WorldStateRole.Guest, "Star tracker role is unknown.");
        _role = role; _ownerThread = Environment.CurrentManagedThreadId;
        if (role == WorldStateRole.Host) _positionQuarantine = new(checked((int)MaximumOperations));
        _pid = host.ProcessPid; _process = host.ProcessHandle; _thread = host.MainThreadId;
        Ensure(_thread);
        Require(host.ImageBase == 0x400000 && Convert.ToHexString(SHA256.HashData(
            image.GetEntireImage().GetContent().ToArray())) == ProbeMain.OriginalSha256,
            "Star tracker requires the pinned original PE.");
    }

    internal bool Active => _installed && _bound && !_retired && !_failed;
    internal bool HasPendingNativePair => _launch is not null || _award is not null || _ballistic is not null;
    internal object Summary => new
    {
        role = _role.ToString(), installed = _installed, bound = _bound, active = Active,
        retiredBeforeNativeUnload = _retired, terminalFailure = _failed, levelEpoch = _epoch,
        pendingNativeLaunches = _launch is null ? 0 : 1, pendingNativeAwards = _award is null ? 0 : 1,
        nativeLaunchEntries = _launchEntries, pairedNativeLaunchReturns = _launchReturns,
        nativeAwardEntries = _awardEntries, pairedNativeAwardReturns = _awardReturns,
        ballisticTraceActivationId = _ballisticActivation?.Id, ballisticEntryArmed = _ballisticArmed,
        pendingNativeBallisticCalls = _ballistic is null ? 0 : 1,
        nativeBallisticEntries = _ballisticEntries, pairedNativeBallisticReturns = _ballisticReturns,
        maximumBallisticCalls = MaximumBallisticCalls, maximumBallisticDetailedLogs = MaximumBallisticCalls * 2,
        lastAwardActivationId = _lastAwardActivation, lastProfile6F0Before = _lastAwardBefore,
        lastProfile6F0After = _lastAwardAfter, lastActualProfile6F0Delta = _lastAwardDelta,
        mintedActivationIds = _nextActivation, rememberedNativeIdentities = _activations.Count,
        ignoredPreBindGuestLaunches = _ignoredPreBindGuestLaunches, ignoredPreBindAwards = _ignoredPreBindAwards,
        guestNativeViolations = _guestNativeViolations, verifiedWorlds = _worlds,
        publishedRecords = _publishedRecords, maximumPoolSlots = MaximumPoolSlots,
        nativeInactivePoolWitnesses = _reportedInactiveActivations.Count, rememberedObservedPoolSlots = _observedPoolSlots.Count,
        cosmeticPositionQuarantine = _positionQuarantine?.Summary,
        maximumWireRecords = WorldStarContract.MaxStars, maximumOperations = MaximumOperations,
        maximumLocalIdentities = MaximumIdentities, maximumDetailedSamples = MaximumSamples,
        retireEntry = Hex(_retireEntry), samples = _samples.ToArray(),
        nativeCallsAdded = 0, nativeWrites = 0, contextWrites = 0, nativeObjectsOwned = 0,
        nativePointersOnWire = false, rewardArithmeticReimplemented = false,
        nativeAwardReturnProvesRetirement = false, runtimePickupQualificationClaimed = false
    };

    // Install before level initialization, so an active Star can never acquire a
    // generation merely because the first World snapshot happened to see it.
    internal void InstallStopped()
    {
        Ensure(_thread);
        Require(!_installed && !_bound && !_retired && !_failed &&
            !_host.Breakpoints.Contains(LaunchEntry) && !_host.Breakpoints.Contains(AwardEntry) &&
            !_host.Breakpoints.Contains(0x58C877) && !_host.Breakpoints.Contains(0x58CE3C),
            "Star native entry/return probes are single-owner and startup-only.");
        try
        {
            byte[] launch = Image(LaunchEntry, 0x23E);
            Require(Convert.ToHexString(SHA256.HashData(launch)) ==
                "01CB31CAED47FC694508B3101564D1DEB76F9AADE0A15F45D1CA071ED39DDAE8" &&
                Read(LaunchEntry, launch.Length).SequenceEqual(launch), "Star native Launch source window differs.");
            Pin(LaunchRet, "C21C00");
            Pin(AwardEntry, "A1D45A760085C0568BF1750AE80F50E7FFA3D45A76008BC8E8C3A1F5FFA1D45A760085C0750AE8F54FE7FFA3D45A76008B4E346A00518BC8E803A1F5FF518BC451C700000000008B56348BC46A2368112800008BCE8910E83429E8FF5EC3");
            Pin(0x58C2CC, "5EC3"); Pin(0x58C872, "E8F9F9FFFF"); Pin(0x58CE37, "E834F4FFFF");
            Pin(0x58C877, "8BCEE892F7FFFF"); Pin(0x58CE3C, "8BCEE8CDF1FFFF");
            PinHash(BallisticEntry, 0xF8, "1883F796BD6357F25778D496EFBF69343C81DA3C428D4290AB5B439FE149B952");
            PinHash(0x58C7B0, 0x56, "3770B5E8CEDFC5DF7635176800FEC5C5E1CE52ABFCDE2DF9504075D1992483E7");
            PinHash(0x58CFA0, 0x45, "E041AB2D8E0158F962562B43C9E8C770BE09946A4A31F2B6DC9A049CA02BEB54");
            Pin(BallisticRet, "C21800");
            _installed = true;
            Hook(LaunchEntry, "launch", Convert.FromHexString("83EC3C8B442458"), LaunchEntryStop);
            Hook(AwardEntry, "award", Convert.FromHexString("A1D45A760085C0"), AwardEntryStop);
            Sample("installed-before-level", new { role = _role.ToString(), nativeCallsAdded = 0, nativeWrites = 0 });
        }
        catch { _failed = true; throw; }
    }

    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context, uint scene, uint parent, uint epoch)
    {
        Ensure(tid);
        Require(_installed && !_bound && !_retired && !_failed && !HasPendingNativePair && epoch != 0 &&
            context.Eip == NativeCallDispatcher.WorldReturn, "Star tracker binds once at an idle original World return.");
        try
        {
            _scene = Pointer(scene); _parent = Pointer(parent); _epoch = epoch;
            _profile = Pointer(U(ProfileSlot)); _primaryInstance = Pointer(U(_profile + 0x2B4));
            _primaryRoot = Pointer(U(_primaryInstance + 0x24));
            VerifyOwners(); _bound = true;
            Sample("bound", new { role = _role.ToString(), epoch, actualLaunchIdsAlreadySeen = _nextActivation,
                guestAuditBeginsAfterCallerQuarantine = _role == WorldStateRole.Guest });
        }
        catch { _failed = true; throw; }
    }

    internal IReadOnlyList<WorldStarState> CaptureAtWorld(uint tid, ref Win32Native.X86Context context,
        WorldPresentationStarPoolObservation observation)
    {
        Ensure(tid); ArgumentNullException.ThrowIfNull(observation);
        Require(Active && _role == WorldStateRole.Host && !HasPendingNativePair &&
            context.Eip == NativeCallDispatcher.WorldReturn && observation.Scene == _scene &&
            observation.DebugStopSequence == _host.DebugStopSequence,
            "Star state capture requires the host's same-stop current World pool observation.");
        try
        {
            VerifyOwners();
            Require(observation.Entries.Count <= MaximumPoolSlots && observation.RepeatedStarAddresses == 0 &&
                observation.RepeatedNodeAddresses == 0 && U(0x765B00) == observation.Manager &&
                U(observation.Manager) == 0x6F61CC && (_manager == 0 || _manager == observation.Manager),
                "Star state capture pool identity, bounds or aliasing differs.");
            _manager = observation.Manager;
            WorldPresentationStarPoolEntry[] entries = observation.Entries.Where(value => value.Active40 != 0).ToArray();
            Require(entries.Length <= WorldStarContract.MaxStars, "Native active Star count exceeds the bounded wire contract.");
            List<WorldStarState> result = []; HashSet<uint> stars = [], nodes = [];
            foreach (var entry in entries)
            {
                var node = entry.Node ?? throw new InvalidDataException("Active native Star has no observed Node.");
                Require(entry.Active40 == 1 && entry.Table == StarTable &&
                    node.Address == entry.NodeCC && node.Table == NodeTable && node.InExpectedScene &&
                    node.Scene == _scene && node.Parent == _parent && stars.Add(entry.Star) && nodes.Add(node.Address),
                    "Active Star pool entry does not belong to the exact bound scene/parent.");
                ValidateStar(entry.Star, entry.NodeCC); ValidateBoundNode(entry.NodeCC);
                Require(Byte(entry.Star + 0x40) == 1 && U(entry.Star + 0xC8) == entry.StateC8 &&
                    U(entry.NodeCC + 0xB0) == node.Flags, "Active Star changed at the owned stopped observation.");
                if (!_activations.TryGetValue(entry.Star, out Activation? activation) || activation.Node != entry.NodeCC)
                    throw new InvalidDataException("An active Star has no paired actual native Launch generation.");
                Require(entry.NativeSlotAddress >= 0x10000 && (entry.NativeSlotAddress & 3) == 0,
                    "Native active Star has no reader-observed fixed pool slot address.");
                var slot = new ObservedPoolSlot(activation.Id, entry.Star, entry.NodeCC, entry.Category, entry.Slot, entry.NativeSlotAddress);
                if (_observedPoolSlots.TryGetValue(entry.Star, out ObservedPoolSlot? previous) && previous.ActivationId == activation.Id)
                    Require(previous == slot, "A native Star activation changed its observed pool slot/Node identity.");
                _observedPoolSlots[entry.Star] = slot;
                uint flags = WorldStarContract.Present | ((node.Flags & 0x200) != 0 ? WorldStarContract.Visible : 0);
                WorldStarState state = new(activation!.Id, entry.Category, entry.Slot, entry.StateC8, flags,
                    node.Local.Position.ToArray(), node.Local.Scale.ToArray(), node.Local.Rotation.ToArray());
                bool include, firstOmission;
                try
                {
                    Require(_positionQuarantine is not null, "Star position projection is Host-capture only.");
                    include = _positionQuarantine!.TryInclude(state, out firstOmission);
                }
                catch (InvalidDataException)
                {
                    _log(new { @event = "world-star-native-scalar-rejected", activationId = activation.Id,
                        entry.Category, entry.Slot, entry.StateC8, flags, nativeStar = Hex(entry.Star),
                        nativeNode = Hex(entry.NodeCC),
                        positionBits = state.Position.Select(value => BitConverter.SingleToUInt32Bits(value).ToString("X8")).ToArray(),
                        scaleBits = state.Scale.Select(value => BitConverter.SingleToUInt32Bits(value).ToString("X8")).ToArray(),
                        rotationBits = state.Rotation.Select(value => BitConverter.SingleToUInt32Bits(value).ToString("X8")).ToArray(),
                        nativeValuesChanged = false, invalidScalarForwarded = false });
                    throw;
                }
                if (!include)
                {
                    if (firstOmission)
                        _log(new { @event = "world-star-native-position-quarantined", activationId = activation.Id,
                            entry.Category, entry.Slot, entry.StateC8, flags, nativeStar = Hex(entry.Star), nativeNode = Hex(entry.NodeCC),
                            positionBits = state.Position.Select(value => BitConverter.SingleToUInt32Bits(value).ToString("X8")).ToArray(),
                            scaleBits = state.Scale.Select(value => BitConverter.SingleToUInt32Bits(value).ToString("X8")).ToArray(),
                            rotationBits = state.Rotation.Select(value => BitConverter.SingleToUInt32Bits(value).ToString("X8")).ToArray(),
                            existingPositionMagnitudeLimit = WorldStarState.MaximumPositionMagnitude,
                            quarantinedActivationCount = _positionQuarantine!.QuarantinedActivationCount,
                            presentationRecordOmitted = true, stickyForThisActivationId = true,
                            nativeOwnershipAndLaunchVerified = true, nativeActiveAndAwardTrackingContinue = true,
                            nativeValuesChanged = false, invalidScalarForwarded = false, nativePhysicsFixed = false });
                    continue;
                }
                result.Add(state);
            }
            ObservePreviouslyActiveNativeRetirements(observation, entries);
            WorldStarContract.Validate(result); VerifyOwners();
            Require(++_worlds <= NativeObservationBudget.WorldLimit, "Star capture World budget exceeded.");
            _publishedRecords = checked(_publishedRecords + (uint)result.Count);
            return Array.AsReadOnly(result.ToArray());
        }
        catch { _failed = true; throw; }
    }

    internal void RetireAtStop(uint tid, ref Win32Native.X86Context context, uint entry)
    {
        Ensure(tid);
        Require(_installed && !_retired && !HasPendingNativePair && entry == context.Eip &&
            entry is NativeCallDispatcher.WorldReturn or 0x5F2300 or 0x5F86C0,
            "Star native tracker must retire at its owned idle World/pre-unload boundary.");
        // No object graph is read here: the caller retires after the guest mirror
        // and before the original native unload. All later callbacks are dormant.
        _retireEntry = entry; _retired = true; _ballisticArmed = false;
        _activations.Clear();
        _observedPoolSlots.Clear();
        Sample("retired-before-native-unload", new { entry = Hex(entry), pendingNativePairs = 0,
            historicalObjectReadsNowForbidden = true, nativeDeletesAdded = 0 });
    }

    private void ObservePreviouslyActiveNativeRetirements(WorldPresentationStarPoolObservation observation,
        IReadOnlyList<WorldPresentationStarPoolEntry> activeEntries)
    {
        HashSet<uint> activeStars = activeEntries.Select(entry => entry.Star).ToHashSet();
        foreach (ObservedPoolSlot slot in _observedPoolSlots.Values)
        {
            if (activeStars.Contains(slot.Star) || _reportedInactiveActivations.Contains(slot.ActivationId) ||
                !_activations.TryGetValue(slot.Star, out Activation? activation) || activation.Id != slot.ActivationId ||
                activation.Node != slot.Node) continue;
            // Active-only observations omit inactive entries. Absence is only
            // the trigger for these current reads; it is never the witness.
            // The slot address came from the reader's actual fixed-pool walk,
            // so this tracker does not reconstruct a second pool layout.
            uint currentSlotPointer = U(slot.Address);
            if (currentSlotPointer != slot.Star) continue;
            byte[] star = Read(slot.Star, 0xD0);
            uint currentTable = BitConverter.ToUInt32(star, 0), currentNode = BitConverter.ToUInt32(star, 0xCC);
            byte active40 = star[0x40];
            if (currentTable != StarTable || currentNode != slot.Node || active40 != 0) continue;
            Require(observation.DebugStopSequence == _host.DebugStopSequence && U(0x765B00) == observation.Manager &&
                observation.Manager == _manager && U(_manager) == 0x6F61CC,
                "Native inactive Star witness lost its same-stop current pool manager.");
            Require(_reportedInactiveActivations.Count < MaximumOperations,
                "Native inactive Star witness operation budget exceeded.");
            _reportedInactiveActivations.Add(slot.ActivationId);
            var data = new { activationId = slot.ActivationId, category = slot.Category, slot = slot.Slot,
                nativeStar = Hex(slot.Star), nativeNode = Hex(slot.Node), rawActive40 = active40,
                currentPoolManager = Hex(_manager), currentPoolSlotAddress = Hex(slot.Address),
                currentPoolSlotPointer = Hex(currentSlotPointer), currentStarTable = Hex(currentTable),
                currentNodeCC = Hex(currentNode), debugStopSequence = observation.DebugStopSequence,
                previouslyObservedActiveAtThisPoolSlot = true, actualPairedNativeLaunch = true,
                inactiveNodeGraphRead = false, nativeCalls = 0, nativeWrites = 0,
                nativeAwardOrCollectionClaimedByThisWitnessAlone = false };
            // Keep the essential once-per-activation witness even when the
            // independent bounded detail-sample list has already filled.
            if (_samples.Count < MaximumSamples) _samples.Add(new { name = "native-inactive-pool-witness", data });
            _log(new { @event = "world-star-native-tracker", name = "native-inactive-pool-witness", data });
        }
    }

    private void LaunchEntryStop(uint tid, ref Win32Native.X86Context context)
    {
        if (_role == WorldStateRole.Guest)
        {
            if (!_bound) { Count(ref _ignoredPreBindGuestLaunches); return; }
            GuestViolation("launch");
        }
        Require(_launch is null && _award is null, "Native Star Launch is reentrant with an unmatched native pair.");
        uint star = Pointer(context.Ecx), node = Pointer(U(star + 0xCC)); ValidateStar(star, node);
        if (_bound) { VerifyOwners(); ValidateBoundNode(node); }
        uint caller = U(context.Esp); Pointer(context.Esp);
        Require(caller >= 0x400005 && !_host.Breakpoints.Contains(caller), "Native Star Launch return site is unavailable.");
        byte[] call = Image(caller - 5, 5), after = Image(caller, 6);
        Require(call[0] == 0xE8 && unchecked(caller + (uint)BitConverter.ToInt32(call, 1)) == LaunchEntry &&
            Read(caller - 5, 5).SequenceEqual(call) && Read(caller, after.Length).SequenceEqual(after),
            "Native Star Launch caller is not an exact original direct call to Launch.");
        Count(ref _launchEntries);
        LaunchFrame frame = new(star, node, context.Esp, caller, context.Esi, context.Edi, after);
        _launch = frame;
        _host.Breakpoints.AddReturn(caller, tid, "WorldStarNativeTracker.launch.return",
            (uint rt, uint at, ref Win32Native.X86Context returned) => LaunchReturn(rt, at, ref returned, frame));
    }

    private void LaunchReturn(uint tid, uint at, ref Win32Native.X86Context context, LaunchFrame frame)
    {
        Ensure(tid); if (_retired || _failed) return;
        try
        {
            Require(ReferenceEquals(_launch, frame) && _award is null && at == frame.Caller && context.Eip == at &&
                context.Esp == checked(frame.Esp + 32) && context.Esi == frame.Esi && context.Edi == frame.Edi &&
                (context.Eax & 0xFF) == 1 && Read(at, frame.CallerBytes.Length).SequenceEqual(frame.CallerBytes),
                "Native Star Launch RET1C return lost its exact caller/ESP/register pairing.");
            ValidateStar(frame.Star, frame.Node);
            Require(Byte(frame.Star + 0x40) == 1, "Native Star Launch returned without its active receiver.");
            if (_bound) { VerifyOwners(); ValidateBoundNode(frame.Node); }
            Require(_nextActivation < ulong.MaxValue && (_activations.ContainsKey(frame.Star) ||
                _activations.Count < MaximumIdentities), "Native Star activation identity budget exhausted.");
            Activation activation = new(++_nextActivation, frame.Star, frame.Node);
            _activations[frame.Star] = activation; Count(ref _launchReturns); _launch = null;
            if (_launchReturns <= 16)
                Sample("native-launch-return", new { activationId = activation.Id, originalCaller = Hex(at),
                    nativeEspDelta = 32, active40 = 1, afterBound = _bound });
            // Observe one genuine native generation, never an already-active
            // pool sample. No entry breakpoint exists before this bound Launch.
            if (_role == WorldStateRole.Host && _bound && _ballisticActivation is null)
            {
                _ballisticActivation = activation;
                ArmBallisticEntry();
            }
        }
        catch { _failed = true; throw; }
    }

    private void ArmBallisticEntry(bool ownedEntryRearm = false)
    {
        Require(Active && _role == WorldStateRole.Host && !_ballisticArmed && _ballistic is null &&
            _ballisticActivation is not null && _ballisticEntries < MaximumBallisticCalls &&
            (ownedEntryRearm || !_host.Breakpoints.Contains(BallisticEntry)),
            "Bounded Star ballistic entry ownership differs.");
        _ballisticArmed = true;
        _host.Breakpoints.AddReturn(BallisticEntry, _thread, "WorldStarNativeTracker.ballistic.entry",
            (uint tid, uint at, ref Win32Native.X86Context context) =>
            {
                Ensure(tid);
                // A retired callback never follows any former Star/Node graph.
                _ballisticArmed = false;
                if (_retired || _failed) return;
                try
                {
                    Require(Active && at == BallisticEntry && context.Eip == at && _ballistic is null,
                        "Star ballistic entry lost its live original stop.");
                    Activation activation = _ballisticActivation!;
                    Require(_activations.TryGetValue(activation.Star, out Activation? boundActivation) &&
                        boundActivation == activation && _launch is null && _award is null,
                        "Star ballistic callback lost its exact current Launch generation or idle native pair.");
                    if (context.Ecx != activation.Star)
                    {
                        // AddReturn consumes this callback only after it
                        // returns. Its same owned site may still be present.
                        // This exemption is only reachable from the verified
                        // main-thread BallisticEntry stop above; initial and
                        // paired-return arming retain the collision guard.
                        ArmBallisticEntry(ownedEntryRearm: true);
                        return;
                    }
                    Require(_launch is null && _award is null &&
                        _activations.TryGetValue(activation.Star, out Activation? current) && current == activation,
                        "Star ballistic receiver has no current paired native Launch generation.");
                    VerifyOwners(); ValidateStar(activation.Star, activation.Node); ValidateBoundNode(activation.Node);
                    byte[] stack = Read(Pointer(context.Esp), 28);
                    uint caller = BitConverter.ToUInt32(stack, 0), output = BitConverter.ToUInt32(stack, 4);
                    Require(caller is 0x58C7EF or 0x58CFDF && context.Ecx == context.Esi &&
                        output == checked(context.Esp + 32) && !_host.Breakpoints.Contains(caller),
                        "Star ballistic call is not an exact original Star receiver/caller-stack output.");
                    byte[] call = Image(caller - 5, 5), after = Image(caller, 6);
                    Require(call[0] == 0xE8 && unchecked(caller + (uint)BitConverter.ToInt32(call, 1)) == BallisticEntry &&
                        Read(caller - 5, 5).SequenceEqual(call) && Read(caller, after.Length).SequenceEqual(after) &&
                        Read(BallisticEntry, 6).SequenceEqual(Convert.FromHexString("83EC0C568BF1")),
                        "Star ballistic original direct call or entry bytes differ.");
                    uint ordinal = ++_ballisticEntries;
                    BallisticFrame frame = new(activation, context.Esp, caller, output, context.Esi, context.Edi,
                        context.Ebx, context.Ebp, after, stack, ordinal);
                    _ballistic = frame;
                    BallisticLog("native-ballistic-entry", frame, stack, null);
                    _host.Breakpoints.AddReturn(caller, tid, "WorldStarNativeTracker.ballistic.return",
                        (uint rt, uint returnedAt, ref Win32Native.X86Context returned) =>
                            BallisticReturn(rt, returnedAt, ref returned, frame));
                }
                catch { _failed = true; throw; }
            });
    }

    private void BallisticReturn(uint tid, uint at, ref Win32Native.X86Context context, BallisticFrame frame)
    {
        Ensure(tid); if (_retired || _failed) return;
        try
        {
            Require(ReferenceEquals(_ballistic, frame) && _launch is null && _award is null && at == frame.Caller &&
                context.Eip == at && context.Esp == checked(frame.Esp + 28) && context.Eax == frame.Output &&
                context.Esi == frame.Esi && context.Edi == frame.Edi && context.Ebx == frame.Ebx && context.Ebp == frame.Ebp &&
                Read(at, frame.CallerBytes.Length).SequenceEqual(frame.CallerBytes) &&
                _activations.TryGetValue(frame.Activation.Star, out Activation? current) && current == frame.Activation,
                "Native Star ballistic RET18 lost exact caller/ESP/output/generation/nonvolatile register pairing.");
            VerifyOwners(); ValidateStar(frame.Activation.Star, frame.Activation.Node); ValidateBoundNode(frame.Activation.Node);
            byte[] output = Read(frame.Output, 12);
            BallisticLog("native-ballistic-return", frame, null, output);
            ++_ballisticReturns; _ballistic = null;
            // Two original calls/four essential logs, then zero further entry
            // breakpoints: no per-frame diagnostic overhead for later motion.
            if (_ballisticEntries < MaximumBallisticCalls) ArmBallisticEntry();
        }
        catch { _failed = true; throw; }
    }

    private void BallisticLog(string name, BallisticFrame frame, byte[]? stack, byte[]? output)
    {
        byte[] arguments = stack ?? frame.Arguments;
        byte[] star = Read(frame.Activation.Star, 0xD0), node = Read(frame.Activation.Node, 0x84);
        uint timer = Pointer(U(0x755298)); byte[] time = Read(timer + 0x1C, 16);
        uint currentTime = BitConverter.ToUInt32(time, 0), state = BitConverter.ToUInt32(star, 0xC8);
        uint argumentTime = BitConverter.ToUInt32(arguments, 24);
        var data = new { activationId = frame.Activation.Id, nativeCallOrdinal = frame.Ordinal,
            nativeStar = Hex(frame.Activation.Star), nativeNode = Hex(frame.Activation.Node),
            originalCaller = Hex(frame.Caller), entryEsp = Hex(frame.Esp), outputPointer = Hex(frame.Output),
            debugStopSequence = _host.DebugStopSequence, levelEpoch = _epoch, ownerScene = Hex(_scene),
            ownerParent = Hex(_parent), actualPairedNativeLaunch = true, exactOriginalStarReceiver = true,
            sameMainThread = _thread, rawActive40 = star[0x40], rawStateC8 = state,
            timerPointer = Hex(timer), timerTable = Hex(U(timer)), timer1C = currentTime,
            timer1CBits = currentTime.ToString("X8"), timer28 = Scalar(time, 12),
            star14Through28Bits = Words(star, 0x14, 6), star18 = BitConverter.ToUInt32(star, 0x18),
            star48Speed = Scalar(star, 0x48), star50Gravity = Scalar(star, 0x50), star58Radius = Scalar(star, 0x58),
            star68DirectionBits = Words(star, 0x68, 3), star74State1VectorBits = Words(star, 0x74, 3),
            starBCPreviousWorldBits = Words(star, 0xBC, 3), node20LocalBits = Words(node, 0x20, 3),
            node74CachedWorldBits = Words(node, 0x74, 3),
            argumentWordsBits = Words(arguments, 4, 6), argumentWordsRetainedFromPairedEntry = stack is null,
            argumentDirectionBits = Words(arguments, 8, 3),
            argumentSpeed = Scalar(arguments, 20), argumentTime,
            unsignedArgumentElapsed = unchecked(currentTime - argumentTime),
            unsignedStar18Elapsed = unchecked(currentTime - BitConverter.ToUInt32(star, 0x18)),
            nativeOutputBits = output is null ? null : Words(output, 0, 3),
            pairedRet18 = output is not null, nativeEspDelta = output is null ? 0 : 28,
            nativeCallsAdded = 0, nativeWrites = 0, contextWrites = 0, elapsedFormulaReimplemented = false };
        if (_samples.Count < MaximumSamples) _samples.Add(new { name, data });
        _log(new { @event = "world-star-native-tracker", name, data });
    }

    private static object Scalar(byte[] bytes, int offset) => new
    {
        bits = BitConverter.ToUInt32(bytes, offset).ToString("X8"),
        floatText = BitConverter.ToSingle(bytes, offset).ToString("R", System.Globalization.CultureInfo.InvariantCulture)
    };
    private static string[] Words(byte[] bytes, int offset, int count) => Enumerable.Range(0, count)
        .Select(index => BitConverter.ToUInt32(bytes, offset + index * 4).ToString("X8")).ToArray();

    private void AwardEntryStop(uint tid, ref Win32Native.X86Context context)
    {
        if (!_bound) { Count(ref _ignoredPreBindAwards); return; }
        if (_role == WorldStateRole.Guest) GuestViolation("award");
        VerifyOwners();
        Require(_launch is null && _award is null && context.Ecx == context.Esi,
            "Native Star Award is reentrant or has no exact ECX/ESI receiver.");
        uint star = Pointer(context.Esi), node = Pointer(context.Edi), caller = U(context.Esp);
        Pointer(context.Esp); ValidateStar(star, node); ValidateBoundNode(node);
        Require(caller is 0x58C877 or 0x58CE3C && Byte(star + 0x40) == 1 &&
            !_host.Breakpoints.Contains(caller), "Native Star Award caller, active receiver or return reservation differs.");
        if (!_activations.TryGetValue(star, out Activation? activation) || activation.Node != node)
            throw new InvalidDataException("Native Star Award has no actual paired Launch generation.");
        Count(ref _awardEntries);
        AwardFrame frame = new(star, node, context.Esp, caller, context.Esi, context.Edi,
            U(_profile + 0x6F0), activation!.Id);
        _award = frame;
        _host.Breakpoints.AddReturn(caller, tid, "WorldStarNativeTracker.award.return",
            (uint rt, uint at, ref Win32Native.X86Context returned) => AwardReturn(rt, at, ref returned, frame));
    }

    private void AwardReturn(uint tid, uint at, ref Win32Native.X86Context context, AwardFrame frame)
    {
        Ensure(tid); if (_retired || _failed) return;
        try
        {
            Require(ReferenceEquals(_award, frame) && _launch is null && at == frame.Caller && context.Eip == at &&
                context.Esp == checked(frame.Esp + 4) && context.Esi == frame.Esi && context.Edi == frame.Edi,
                "Native Star Award C3 return lost its exact caller/ESP/register pairing.");
            VerifyOwners(); ValidateStar(frame.Star, frame.Node); ValidateBoundNode(frame.Node);
            Require(Byte(frame.Star + 0x40) == 1, "Native Star Award C3 receiver changed before original retirement.");
            uint after = U(_profile + 0x6F0); Count(ref _awardReturns); _award = null;
            _lastAwardBefore = frame.CounterBefore; _lastAwardAfter = after;
            _lastAwardDelta = (long)after - frame.CounterBefore; _lastAwardActivation = frame.ActivationId;
            Sample("native-award-return", new { activationId = frame.ActivationId, originalCaller = Hex(at),
                nativeEspDelta = 4, profile6F0Before = frame.CounterBefore, profile6F0After = after,
                actualDelta = (long)after - frame.CounterBefore, amountPredicted = false,
                nativeRetirementStillPending = true, awardReturnAlMeaningClaimed = false });
        }
        catch { _failed = true; throw; }
    }

    private delegate void EntryStop(uint tid, ref Win32Native.X86Context context);
    private void Hook(uint site, string name, byte[] expected, EntryStop entry)
    {
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            Ensure(tid); if (_retired || _failed) return;
            try
            {
                Require(_installed && at == site && context.Eip == at && Read(at, expected.Length).SequenceEqual(expected),
                    "Star native entry callback lost its original instruction or lifetime.");
                entry(tid, ref context);
                if (!_retired && !_failed) _host.Breakpoints.AddReturn(site, tid, $"WorldStarNativeTracker.{name}.entry", repeat);
            }
            catch { _failed = true; throw; }
        };
        _host.Breakpoints.AddReturn(site, _thread, $"WorldStarNativeTracker.{name}.entry", repeat);
    }

    private void GuestViolation(string operation)
    {
        Count(ref _guestNativeViolations);
        Sample("guest-native-operation-after-quarantine", new { operation, levelEpoch = _epoch });
        throw new InvalidDataException($"Quarantined guest ran original Star {operation}; presentation-only audit failed.");
    }
    private void ValidateStar(uint star, uint node)
    {
        Pointer(star); Pointer(node);
        byte[] getter = [0xB8, .. BitConverter.GetBytes(0x7550E8u), 0xC3];
        Require(U(star) == StarTable && U(star + 0xCC) == node && U(node) == NodeTable &&
            U(StarTable + 0x20) == 0x58D0A0 && Read(0x58C420, 6).SequenceEqual(getter) &&
            Image(0x58C420, 6).SequenceEqual(getter) && U(0x7550E8) == WorldPresentationStarPoolReader.StarClassId,
            "Native Star/Node RTTI, update slot, getter or receiver identity differs.");
    }
    private void ValidateBoundNode(uint node) => Require(U(node) == NodeTable && U(node + 0x2C) == _parent &&
        U(node + 0x3C) == _scene, "Native Star Node left its bound scene/common Bloom parent.");
    private void VerifyOwners() => Require(U(_scene) == SceneTable && U(_parent) == NodeTable &&
        U(_parent + 0x3C) == _scene && U(ProfileSlot) == _profile && U(_profile) == ProfileTable &&
        U(_profile + 0x2B4) == _primaryInstance && U(_primaryInstance) == 0x710F80 &&
        U(_primaryInstance + 0x24) == _primaryRoot && U(_primaryRoot) == NodeTable &&
        U(_primaryRoot + 0x2C) == _parent && U(_primaryRoot + 0x3C) == _scene,
        "Star tracker lost canonical typed Profile/Instance/Bloom Root/scene/parent ownership.");
    private void Pin(uint at, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(Image(at, expected.Length).SequenceEqual(expected) && Read(at, expected.Length).SequenceEqual(expected),
            $"Star native source pin differs at {Hex(at)}.");
    }
    private void PinHash(uint at, int count, string hash)
    {
        byte[] expected = Image(at, count);
        Require(Convert.ToHexString(SHA256.HashData(expected)) == hash && Read(at, count).SequenceEqual(expected),
            $"Star native source hash pin differs at {Hex(at)}.");
    }
    private byte[] Image(uint at, int count) => _image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, count).ToArray();
    private uint U(uint at) => BitConverter.ToUInt32(Read(at, 4));
    private byte Byte(uint at) => Read(at, 1)[0];
    private byte[] Read(uint at, int count)
    {
        Ensure(_thread);
        Require(!_retired && at >= 0x10000 && count is > 0 and <= 1024, "Star native observation read exceeds its live bounded contract.");
        return Win32Native.ReadMemory(_process, at, count);
    }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _thread && _thread == _host.MainThreadId &&
        _host.ProcessPid == _pid && _pid != 0 && _host.ProcessHandle == _process && _process != IntPtr.Zero &&
        Environment.CurrentManagedThreadId == _ownerThread, "Star native observation requires its pinned owned main-thread debug stop.");
    private void Sample(string name, object data)
    {
        if (_samples.Count >= MaximumSamples) return;
        _samples.Add(new { name, data }); _log(new { @event = "world-star-native-tracker", name, data });
    }
    private static uint Pointer(uint value) { Require(value >= 0x10000 && (value & 3) == 0, "Star native pointer is null or unaligned."); return value; }
    private static void Count(ref uint value) { Require(value < MaximumOperations, "Star native observation operation budget exceeded."); ++value; }
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
