using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own bounded eligibility adapter. After a native primary proximity miss,
/// retry the original query argument span once with the separate owned Root.
/// Award, deactivation, distance arithmetic and the canonical Profile stay native.
/// This file is an unintegrated candidate until a separate actual-game witness.
/// </summary>
internal sealed class StarSecondaryEligibility
{
    private const uint StarTable = 0x702160, InstanceTable = 0x710F80, NodeTable = 0x6DC4F4;
    private const uint ProfileTable = 0x6F5A44;
    private const uint EventLimit = 100000, WorldLimit = 36000;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly int _ownerThread;
    private readonly Dictionary<string, uint> _counts = [];
    private readonly List<object> _samples = [];
    private uint _profile, _primaryInstance, _primaryRoot, _ownInstance, _ownRoot, _scene;
    private uint _sequence, _worlds, _retireEntry;
    private uint _lastSecondaryMissStar, _lastSecondaryMissNode;
    private bool _bound, _retired, _terminalFailure;
    private QueryFrame? _pending;

    private sealed record QueryPath(string Name, uint Entry, uint Miss, uint Arguments,
        uint SavedStackBytes, uint ReturnStackBytes, string EntryBytes, string MissBytes);
    private static readonly QueryPath Stationary = new("stationary", 0x58C820, 0x58C884, 0x58C84A,
        12, 4, "51A1D45A7600", "5F32C05E59C3");
    private static readonly QueryPath Moving = new("moving", 0x58CDE0, 0x58CE4D, 0x58CE0F,
        16, 8, "83EC08568BF1", "8B8ECC000000");

    private sealed class QueryFrame(uint id, QueryPath path, uint star, uint node, uint esp,
        uint caller, uint savedEsi, uint savedEdi, uint state, byte flag44, uint argument,
        uint threshold, uint tolerance, byte[] primaryPosition, byte[] ownPosition, byte[] starPosition)
    {
        internal readonly uint Id = id, Star = star, Node = node, Esp = esp, Caller = caller;
        internal readonly uint SavedEsi = savedEsi, SavedEdi = savedEdi, State = state;
        internal readonly uint Argument = argument, Threshold = threshold, Tolerance = tolerance;
        internal readonly byte Flag44 = flag44;
        internal readonly QueryPath Path = path;
        internal readonly byte[] PrimaryPosition = primaryPosition, OwnPosition = ownPosition, StarPosition = starPosition;
        internal bool PrimaryMiss, Retried, SecondaryMiss;
    }

    // Read-only identity for an award observer. It never owns the query's
    // native return breakpoint or changes the eligibility/geometry decision.
    internal sealed record QueryIdentity(uint Sequence, uint Star, uint Node, string Path,
        bool Retried, bool SecondaryMiss, uint EntryEsp, uint Caller, uint Profile, uint Scene);
    internal sealed record QueryCompletion(QueryIdentity Query, bool GeometryAccepted,
        byte NativeReturnAl, byte Active40, uint StateC8, uint ReturnedCaller, uint ReturnedEsp);
    internal event Action<uint, QueryCompletion>? QueryCompleted;
    internal QueryIdentity? CurrentQuery => _pending is null ? null : Identity(_pending);

    private QueryIdentity Identity(QueryFrame frame) => new(frame.Id, frame.Star, frame.Node, frame.Path.Name,
        frame.Retried, frame.SecondaryMiss, frame.Esp, frame.Caller, _profile, _scene);

    internal StarSecondaryEligibility(DebuggerHost host, PEReader image, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _ownerThread = Environment.CurrentManagedThreadId;
    }

    internal bool Active => _bound && !_retired && !_terminalFailure;
    internal bool Retired => _retired;
    internal bool HasPendingNativePair => _pending is not null;
    internal uint LastSecondaryMissStar => _lastSecondaryMissStar;
    internal uint LastSecondaryMissNode => _lastSecondaryMissNode;
    internal object Summary => new
    {
        bound = _bound, active = Active, retiredBeforeOwnDelete = _retired, terminalFailure = _terminalFailure,
        profile = Hex(_profile), primaryInstance = Hex(_primaryInstance), primaryRoot = Hex(_primaryRoot),
        ownInstance = Hex(_ownInstance), ownRoot = Hex(_ownRoot), scene = Hex(_scene),
        pendingNativePairs = _pending is null ? 0 : 1, maximumPendingPairs = 1,
        verifiedWorlds = _worlds, retireEntry = Hex(_retireEntry),
        lastSecondaryMissStar = Hex(_lastSecondaryMissStar), lastSecondaryMissNode = Hex(_lastSecondaryMissNode),
        counts = new Dictionary<string, uint>(_counts), samples = _samples.ToArray(),
        maximumSecondaryRetriesPerNativeFrame = 1, profileOrGameFieldWrites = 0,
        adapterEspWrites = 0, adapterFloatingPointWrites = 0, pickupArithmeticReimplemented = false,
        nativeAwardOrDeactivateCalledByAdapter = false, originalStarUpdateCalledTwice = false,
        changedRegistersOnRetry = new[] { "EAX own Root", "ECX original tolerance bits", "EIP original argument span" },
        nativePrimaryAttractionChanged = false, perBodyDeathPredicateAdded = false,
        runtimePickupQualified = false, fullE3Complete = false
    };

    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context,
        uint ownInstance, uint ownRoot, uint scene)
    {
        Ensure(tid);
        Require(!_bound && !_retired && !_terminalFailure && context.Eip == NativeCallDispatcher.WorldReturn &&
            _host.ImageBase == 0x400000 && _host.ProcessHandle != IntPtr.Zero,
            "Secondary Star eligibility requires its first owned original WorldRET.");
        try
        {
            _profile = Pointer(0x765AD4);
            Require(U(_profile) == ProfileTable, "Secondary Star canonical Profile type differs.");
            _primaryInstance = Pointer(_profile + 0x2B4); _primaryRoot = Pointer(_primaryInstance + 0x24);
            _ownInstance = ownInstance; _ownRoot = ownRoot; _scene = scene;
            VerifyOwners();
            Require(_primaryInstance != _ownInstance && _primaryRoot != _ownRoot,
                "Secondary Star eligibility requires two separate native bodies.");
            Pin(0x597660, "83EC188B44241C");
            Pin(0x59767F, "8B487C");
            Pin(0x5976B1, "E84AFFFFFF83C438C3");
            Pin(0x58C826, "85C0568BF1578BBECC000000750AE8574AE7FFE9586DE9FF8B4E648B80B40200008B4024");
            Pin(0x58CDE6, "FF463CA1D45A760085C0578BBECC000000750AE89244E7FFA3D45A76008B4E648B80B40200008B4024");
            Pin(0x58C84A, "516A005750E80CAE0000");
            Pin(0x58CE0F, "516A005750E847A80000");
            Pin(0x58C854, "DB46388B563883C41085D2");
            Pin(0x58CE19, "DB46388B563883C41085D2");
            Pin(0x58C867, "DED9DFE0F6C441");
            Pin(0x58CE2C, "DED9DFE0F6C441");
            Pin(0x58C85D, "85D27D06D805BCB86D00DED9DFE0F6C4417514");
            Pin(0x58CE22, "85D27D06D805BCB86D00DED9DFE0F6C4417518");
            Pin(0x58CDC3, "E858FAFFFF"); Pin(0x58CDC8, "84C0750E");
            Pin(0x58CF45, "E896FEFFFF"); Pin(0x58CF4A, "84C07445");
            Pin(0x58D004, "E8D7FDFFFF"); Pin(0x58D009, "84C07467");
            Pin(0x58C870, "8BCEE8F9F9FFFF8BCEE892F7FFFF");
            Pin(0x58CE35, "8BCEE834F4FFFF8BCEE8CDF1FFFF");
            Pin(0x58C87E, "5FB0015E59C3");
            Pin(0x58CE43, "5FB0015E83C408C20400");
            Pin(0x58CEF0, "5D5B5FB0015E83C408C20400");
            Pin(0x58CF25, "F6D85D5B5F5E1BC04083C408C20400");
            Pin(0x58CF34, "5D5B5F32C05E83C408C20400");
            Require(ImageWord(StarTable + 0x20) == 0x58D0A0 && U(StarTable + 0x20) == 0x58D0A0,
                "Secondary Star native update table differs.");
            _bound = true;
            Hook(Stationary.Entry, Stationary.EntryBytes, "stationary-entry",
                (uint thread, ref Win32Native.X86Context c) => QueryEntry(thread, ref c, Stationary));
            Hook(Moving.Entry, Moving.EntryBytes, "moving-entry",
                (uint thread, ref Win32Native.X86Context c) => QueryEntry(thread, ref c, Moving));
            Hook(Stationary.Miss, Stationary.MissBytes, "stationary-miss",
                (uint thread, ref Win32Native.X86Context c) => PrimaryMiss(thread, ref c, Stationary));
            Hook(Moving.Miss, Moving.MissBytes, "moving-miss",
                (uint thread, ref Win32Native.X86Context c) => PrimaryMiss(thread, ref c, Moving));
            Sample("bound", new { profile = Hex(_profile), primaryInstance = Hex(_primaryInstance),
                primaryRoot = Hex(_primaryRoot), ownInstance = Hex(_ownInstance), ownRoot = Hex(_ownRoot),
                scene = Hex(_scene), nativeDistanceHelper = Hex(0x597660), sourcePinsOnly = true });
        }
        catch { _terminalFailure = true; throw; }
    }

    internal void VerifyAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid);
        Require(context.Eip == NativeCallDispatcher.WorldReturn, "Secondary Star verification requires WorldRET.");
        if (_retired) return;
        Require(Active && _pending is null, "Secondary Star reached WorldRET with an inactive or unmatched native frame.");
        VerifyOwners(); Require(++_worlds <= WorldLimit, "Secondary Star world budget exceeded.");
    }

    internal void RetireBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid);
        Require(Active && _pending is null && (context.Eip is NativeCallDispatcher.WorldReturn or 0x5F2300 or 0x5F86C0 or 0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190),
            "Secondary Star must retire at a pre-delete WorldRET or original pre-unload entry.");
        VerifyOwners(); _retireEntry = context.Eip; _retired = true;
        Sample("retired-before-own-delete", new { entry = Hex(context.Eip), pendingNativePairs = 0,
            callbacksDormant = true, historicalOwnerReadsNowForbidden = true });
    }

    private void QueryEntry(uint tid, ref Win32Native.X86Context c, QueryPath path)
    {
        VerifyOwners();
        Require(_pending is null && c.Esp >= 0x10000 + path.SavedStackBytes && (c.Esp & 3) == 0,
            "Secondary Star rejects nested or unaligned native query frames.");
        uint star = c.Ecx, caller = U(c.Esp), state = U(star + 0xC8);
        byte flag44 = Read(star + 0x44, 1)[0];
        Require(U(star) == StarTable && Read(star + 0x40, 1)[0] == 1 && c.Esi == star && flag44 <= 1,
            "Secondary Star query is not the exact active native Star receiver.");
        uint argument = ReferenceEquals(path, Moving) ? U(c.Esp + 4) : 0;
        bool sourceAccepted = ReferenceEquals(path, Stationary)
            ? caller == 0x58CDC8 && (state == 4 || (state is 3 or 5) && flag44 != 0)
            : (caller == 0x58CF4A && state == 0 && argument == 0) ||
              (caller == 0x58D009 && (state is 1 or 2) && (argument == 0 || argument == U(star + 0xE4)));
        Require(sourceAccepted, "Secondary Star query caller/state/argument is outside its pinned native paths.");
        uint node = Pointer(star + 0xCC), tolerance = U(star + 0x64);
        Require(U(node) == NodeTable && U(node + 0x3C) == _scene &&
            float.IsFinite(BitConverter.Int32BitsToSingle(unchecked((int)tolerance))),
            "Secondary Star query Node/scene/tolerance differs from its bounded native input.");
        Require(++_sequence <= EventLimit, "Secondary Star native query budget exceeded.");
        QueryFrame frame = new(_sequence, path, star, node, c.Esp, caller, c.Esi, c.Edi,
            state, flag44, argument, U(star + 0x38), tolerance,
            Position(_primaryRoot), Position(_ownRoot), Position(node));
        _pending = frame; Add("query.entered");
        _host.Breakpoints.AddReturn(caller, tid, "StarSecondaryEligibility." + path.Name + ".return",
            (uint returnedTid, uint at, ref Win32Native.X86Context returned) => QueryReturn(returnedTid, at, ref returned, frame));
        if (_sequence <= 8)
            Sample("query-entry", new { sequence = frame.Id, path = path.Name, star = Hex(star), node = Hex(node),
                caller = Hex(caller), esp = Hex(c.Esp), state, flag44, nativeArgument = Hex(argument),
                squaredThreshold = frame.Threshold, toleranceBits = Hex(tolerance) });
    }

    private void PrimaryMiss(uint tid, ref Win32Native.X86Context c, QueryPath path)
    {
        QueryFrame frame = _pending ?? throw new InvalidDataException("Secondary Star miss lacks a paired native entry.");
        VerifyOwners();
        Require(ReferenceEquals(frame.Path, path) && c.Esi == frame.Star && c.Edi == frame.Node &&
            c.Esp == checked(frame.Esp - path.SavedStackBytes) && U(frame.Esp) == frame.Caller &&
            U(c.Esp) == frame.SavedEdi && U(c.Esp + 4) == frame.SavedEsi &&
            U(frame.Star) == StarTable && Read(frame.Star + 0x40, 1)[0] == 1 &&
            U(frame.Star + 0xC8) == frame.State && Read(frame.Star + 0x44, 1)[0] == frame.Flag44 &&
            U(frame.Star + 0xCC) == frame.Node && U(frame.Star + 0x38) == frame.Threshold &&
            U(frame.Star + 0x64) == frame.Tolerance &&
            (ReferenceEquals(path, Stationary) || U(frame.Esp + 4) == frame.Argument) &&
            Position(_primaryRoot).SequenceEqual(frame.PrimaryPosition) && Position(_ownRoot).SequenceEqual(frame.OwnPosition) &&
            Position(frame.Node).SequenceEqual(frame.StarPosition),
            "Secondary Star miss lost its exact native frame, source geometry or owned bodies.");
        if (frame.Retried)
        {
            Require(frame.PrimaryMiss && !frame.SecondaryMiss, "Secondary Star attempted more than one retry in one native frame.");
            frame.SecondaryMiss = true; Add("query.secondary-miss");
            Sample("secondary-miss", new { sequence = frame.Id, path = path.Name, star = Hex(frame.Star),
                originalFailureContinuation = Hex(path.Miss), registersChanged = false });
            return;
        }
        Require(!frame.PrimaryMiss && !frame.SecondaryMiss, "Secondary Star retry frame was already consumed.");
        frame.PrimaryMiss = true; frame.Retried = true; Add("query.primary-miss"); Add("query.secondary-retry");
        // The first native ADD ESP,10h and FCOMPP have already balanced the
        // arguments and FP pair. Reuse original PUSH4/CALL/ADD/compare exactly.
        // Native597660 clobbers ECX, so restore the same raw tolerance argument.
        c.Eax = _ownRoot; c.Ecx = frame.Tolerance; c.Eip = path.Arguments;
        Sample("secondary-retry", new { sequence = frame.Id, path = path.Name, star = Hex(frame.Star),
            caller = Hex(frame.Caller), miss = Hex(path.Miss), resumedAt = Hex(path.Arguments),
            nativeEsp = Hex(c.Esp), bodyRoot = Hex(_ownRoot), starNode = Hex(frame.Node),
            toleranceBits = Hex(frame.Tolerance), squaredThreshold = frame.Threshold,
            espAndFloatingPointStateChangedByAdapter = false, profileOrGameFieldsWritten = 0 });
    }

    private void QueryReturn(uint tid, uint at, ref Win32Native.X86Context c, QueryFrame frame)
    {
        Ensure(tid);
        try
        {
            Require(Active && ReferenceEquals(_pending, frame) && at == frame.Caller && c.Eip == at &&
                c.Esp == checked(frame.Esp + frame.Path.ReturnStackBytes) &&
                c.Esi == frame.SavedEsi && c.Edi == frame.SavedEdi,
                "Secondary Star native return differs in lifetime, caller, ESP or preserved registers.");
            VerifyOwners();
            Require(U(frame.Star) == StarTable && U(frame.Star + 0xCC) == frame.Node,
                "Secondary Star native return lost its original receiver/Node.");
            bool geometryAccepted = !frame.PrimaryMiss || frame.Retried && !frame.SecondaryMiss;
            byte active = Read(frame.Star + 0x40, 1)[0];
            if (geometryAccepted)
            {
                Require((c.Eax & 255) == 1 && active == 0,
                    "Secondary Star proximity success did not complete the original native retirement branch.");
                Add(frame.Retried ? "query.secondary-proximity-accepted" : "query.primary-proximity-accepted");
            }
            Add("query.returned");
            if (frame.Retried || frame.Id <= 8)
                Sample("query-return", new { sequence = frame.Id, path = frame.Path.Name, star = Hex(frame.Star),
                    caller = Hex(at), espMatched = true, retried = frame.Retried, secondaryMiss = frame.SecondaryMiss,
                    proximityAccepted = geometryAccepted, nativeReturnAl = c.Eax & 255, active40 = active,
                    stateC8 = U(frame.Star + 0xC8), canonicalAwardFieldsObservedHere = false,
                    movingFallbackReturnNotTreatedAsPickup = ReferenceEquals(frame.Path, Moving) && !geometryAccepted });
            QueryCompletion completion = new(Identity(frame), geometryAccepted, (byte)(c.Eax & 255),
                active, U(frame.Star + 0xC8), at, c.Esp);
            if (frame.Retried && frame.SecondaryMiss)
            {
                _lastSecondaryMissStar = frame.Star; _lastSecondaryMissNode = frame.Node;
            }
            _pending = null;
            QueryCompleted?.Invoke(tid, completion);
        }
        catch { _terminalFailure = true; throw; }
    }

    private void VerifyOwners()
    {
        Require(_scene >= 0x10000 && _primaryInstance != _ownInstance && _primaryRoot != _ownRoot &&
            U(0x765AD4) == _profile && U(_profile) == ProfileTable && U(_profile + 0x2B4) == _primaryInstance &&
            U(_primaryInstance) == InstanceTable && U(_ownInstance) == InstanceTable &&
            U(_primaryInstance + 0x24) == _primaryRoot && U(_ownInstance + 0x24) == _ownRoot &&
            U(_primaryRoot) == NodeTable && U(_ownRoot) == NodeTable &&
            U(_primaryRoot + 0x3C) == _scene && U(_ownRoot + 0x3C) == _scene,
            "Secondary Star canonical Profile or separate own Instance/Root/scene changed.");
        _ = Position(_primaryRoot); _ = Position(_ownRoot);
    }

    private byte[] Position(uint node)
    {
        byte[] bytes = Read(node + 0x74, 12);
        Require(Enumerable.Range(0, 3).All(i => float.IsFinite(BitConverter.ToSingle(bytes, i * 4)) &&
            MathF.Abs(BitConverter.ToSingle(bytes, i * 4)) <= 10000000),
            "Secondary Star source geometry exceeds its finite bounded world input.");
        return bytes;
    }

    private delegate void Stop(uint tid, ref Win32Native.X86Context context);
    private void Hook(uint address, string hex, string name, Stop callback)
    {
        byte[] expected = Convert.FromHexString(hex); Pin(address, hex);
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            Ensure(tid);
            if (_retired || _terminalFailure) return;
            Require(Active && at == address && c.Eip == address && Read(at, expected.Length).SequenceEqual(expected),
                "Secondary Star callback signature or lifetime differs from its original pin.");
            try { callback(tid, ref c); }
            catch { _terminalFailure = true; throw; }
            if (Active) _host.Breakpoints.AddReturn(address, tid, "StarSecondaryEligibility." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, "StarSecondaryEligibility." + name, repeat);
    }

    private void Pin(uint at, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(Image(at, expected.Length).SequenceEqual(expected) && Read(at, expected.Length).SequenceEqual(expected),
            $"Secondary Star original image/live code pin differs at {Hex(at)}.");
    }
    private byte[] Image(uint at, int count) => _image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, count).ToArray();
    private uint ImageWord(uint at) => BitConverter.ToUInt32(Image(at, 4));
    private uint Pointer(uint at) { uint value = U(at); Require(value >= 0x10000, "Secondary Star native pointer is null."); return value; }
    private uint U(uint at) => BitConverter.ToUInt32(Read(at, 4));
    private byte[] Read(uint at, int count)
    {
        Ensure(_host.MainThreadId);
        Require(at >= 0x10000 && count is > 0 and <= 256, "Secondary Star read exceeds its bounded contract.");
        return Win32Native.ReadMemory(_host.ProcessHandle, at, count);
    }
    private void Add(string name)
    {
        uint count = _counts.GetValueOrDefault(name) + 1;
        Require(count <= EventLimit, "Secondary Star callback count exceeds its bounded budget."); _counts[name] = count;
    }
    private void Sample(string name, object data)
    {
        if (_samples.Count >= 256) return;
        _samples.Add(new { name, data }); _log(new { @event = "star-secondary-eligibility", name, data });
    }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId &&
        Environment.CurrentManagedThreadId == _ownerThread, "Secondary Star requires its owned main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
