using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Read-only native Star award/retirement witness paired to the eligibility
/// adapter's exact current query. It observes canonical Profile bytes and never
/// supplies resource arithmetic, another award call or a retirement decision.
/// </summary>
internal sealed class NativeStarAwardObservation
{
    private const uint AwardEntry = 0x58C270, StarTable = 0x702160, ProfileTable = 0x6F5A44;
    private const int ProfileBytes = 0x2CB8, CounterOffset = 0x6F0;
    private const uint MaximumAwards = 256, MaximumQueries = 100000;
    private const int MaximumDetailedSamples = 64;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly int _ownerThread;
    private readonly HashSet<uint> _awardedQueries = [];
    private readonly Dictionary<string, uint> _counts = [];
    private readonly List<object> _samples = [];
    private StarSecondaryEligibility? _eligibility;
    private AwardFrame? _pendingAward, _awaitingQuery;
    private uint _profile, _primaryInstance, _primaryRoot, _scene;
    private uint _awardsBegun, _awardsReturned, _lastCompletedQuery, _queriesCompleted, _worlds, _retireEntry;
    private uint _primaryAwards, _secondaryAwards;
    private bool _bound, _retired, _failed;

    private sealed record ByteChange(uint Offset, byte Before, byte After);
    private sealed class AwardFrame(StarSecondaryEligibility.QueryIdentity query, uint esp, uint caller,
        uint esi, uint edi, uint value, byte[] before)
    {
        internal readonly StarSecondaryEligibility.QueryIdentity Query = query;
        internal readonly uint Esp = esp, Caller = caller, Esi = esi, Edi = edi, Value = value;
        internal byte[] Before = before;
        internal readonly uint CounterBefore = BitConverter.ToUInt32(before, CounterOffset);
        internal uint CounterAfter;
        internal long ObservedDelta;
        internal ByteChange[] Changes = [];
        internal bool Returned;
    }

    internal NativeStarAwardObservation(DebuggerHost host, PEReader image, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _ownerThread = Environment.CurrentManagedThreadId;
    }

    internal bool Active => _bound && !_retired && !_failed;
    internal uint CompletedPrimaryAwards => _primaryAwards;
    internal uint CompletedSecondaryAwards => _secondaryAwards;
    internal object Summary => new
    {
        bound = _bound, active = Active, retiredBeforeOwnDelete = _retired, terminalFailure = _failed,
        canonicalProfile = Hex(_profile), primaryInstance = Hex(_primaryInstance), primaryRoot = Hex(_primaryRoot),
        scene = Hex(_scene), retireEntry = Hex(_retireEntry), verifiedWorlds = _worlds,
        pendingNativeAwards = _pendingAward is null ? 0 : 1, awaitingPairedQueryReturn = _awaitingQuery is null ? 0 : 1,
        awardsBegun = _awardsBegun, awardsReturned = _awardsReturned, queriesCompleted = _queriesCompleted,
        completedPrimaryAwards = _primaryAwards, completedSecondaryAwards = _secondaryAwards,
        maximumAwards = MaximumAwards, maximumDetailedSamples = MaximumDetailedSamples,
        profileSnapshotBytes = ProfileBytes, counterOffset = Hex(CounterOffset),
        counts = new Dictionary<string, uint>(_counts), samples = _samples.ToArray(),
        profileOrGameFieldWrites = 0, contextWrites = 0, nativeCallsAdded = 0,
        resourceArithmeticReimplemented = false, awardDeltaPredictedFromPayload = false,
        pointerBasedPickupSuppression = false, nativeRetirementUnchanged = true,
        runtimePickupQualificationClaimed = false, fullE3Complete = false
    };

    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context, StarSecondaryEligibility eligibility)
    {
        Ensure(tid); ArgumentNullException.ThrowIfNull(eligibility);
        Require(!_bound && !_retired && !_failed && context.Eip == NativeCallDispatcher.WorldReturn &&
            _host.ImageBase == 0x400000 && _host.ProcessHandle != IntPtr.Zero && eligibility.Active &&
            !eligibility.HasPendingNativePair && eligibility.CurrentQuery is null,
            "Star award observation requires its first owned WorldRET and a bound idle eligibility peer.");
        try
        {
            Require(!_host.Breakpoints.Contains(AwardEntry) && !_host.Breakpoints.Contains(0x58C877) &&
                !_host.Breakpoints.Contains(0x58CE3C), "Star award entry/return already has another owner; multicast is not supported.");
            _eligibility = eligibility; _profile = Pointer(0x765AD4);
            _primaryInstance = Pointer(_profile + 0x2B4); _primaryRoot = Pointer(_primaryInstance + 0x24);
            _scene = Pointer(_primaryRoot + 0x3C); VerifyOwners();
            Pin(0x58C270, "A1D45A760085C0568BF1750AE80F50E7FFA3D45A76008BC8E8C3A1F5FFA1D45A760085C0750AE8F54FE7FFA3D45A76008B4E346A00518BC8E803A1F5FF518BC451C700000000008B56348BC46A2368112800008BCE8910E83429E8FF5EC3");
            Pin(0x58C2CC, "5EC3"); Pin(0x4E63B0, "FF25EC1B3B01");
            Pin(0x58C872, "E8F9F9FFFF"); Pin(0x58CE37, "E834F4FFFF");
            Pin(0x58C877, "8BCEE892F7FFFF"); Pin(0x58CE3C, "8BCEE8CDF1FFFF");
            eligibility.QueryCompleted += QueryCompleted;
            _bound = true; HookAward();
            _log(new { @event = "native-star-award-observation", name = "bound",
                data = new { profile = Hex(_profile), primaryInstance = Hex(_primaryInstance), primaryRoot = Hex(_primaryRoot),
                    scene = Hex(_scene), eligibilityPeerActive = true, currentNativeQuery = false,
                    profileSnapshotBytes = ProfileBytes, actualCounterOffset = Hex(CounterOffset) } });
        }
        catch { _failed = true; if (_eligibility is not null) _eligibility.QueryCompleted -= QueryCompleted; throw; }
    }

    internal void VerifyAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid); Require(context.Eip == NativeCallDispatcher.WorldReturn, "Star award verification requires WorldRET.");
        if (_retired) return;
        Require(Active && _eligibility?.Active == true && !_eligibility.HasPendingNativePair &&
            _pendingAward is null && _awaitingQuery is null, "Star award observation reached WorldRET with an unmatched native operation.");
        VerifyOwners(); Require(++_worlds <= NativeObservationBudget.WorldLimit, "Star award observation world budget exceeded.");
    }

    internal void RetireBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid);
        Require(Active && (context.Eip is NativeCallDispatcher.WorldReturn or 0x5F2300 or 0x5F86C0 or
            0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190) && _eligibility?.Active == true &&
            !_eligibility.HasPendingNativePair && _pendingAward is null && _awaitingQuery is null,
            "Star award observation must retire before its eligibility peer and own native deletion with no pending pair.");
        VerifyOwners(); _eligibility!.QueryCompleted -= QueryCompleted;
        _retireEntry = context.Eip; _retired = true;
        _log(new { @event = "native-star-award-observation", name = "retired-before-own-delete",
            data = new { entry = Hex(context.Eip), pendingNativeAwards = 0, awaitingPairedQueryReturn = 0,
                callbacksDormantBeforeDelete = true, historicalObjectReadsNowForbidden = true } });
    }

    private void AwardEntryStop(uint tid, ref Win32Native.X86Context c)
    {
        VerifyOwners();
        StarSecondaryEligibility.QueryIdentity query = _eligibility!.CurrentQuery ??
            throw new InvalidDataException("Native Star award has no exact current eligibility query.");
        Require(_eligibility.Active && _pendingAward is null && _awaitingQuery is null &&
            query.Sequence > _lastCompletedQuery && query.Profile == _profile && query.Scene == _scene &&
            !query.SecondaryMiss && c.Ecx == query.Star && c.Esi == query.Star && c.Edi == query.Node &&
            U(query.Star) == StarTable && Byte(query.Star + 0x40) == 1 &&
            U(query.Star + 0xCC) == query.Node && U(query.Node) == 0x6DC4F4 && U(query.Node + 0x3C) == _scene,
            "Native Star award is reentrant, unknown, or outside its live exact query/owners.");
        uint caller = U(c.Esp);
        uint frameBytes = query.Path switch { "stationary" => 16, "moving" => 20,
            _ => throw new InvalidDataException("Native Star award has an unknown query path.") };
        uint expectedCaller = query.Path == "stationary" ? 0x58C877u : 0x58CE3Cu;
        Require(caller == expectedCaller && c.Esp == checked(query.EntryEsp - frameBytes) &&
            U(query.EntryEsp) == query.Caller && c.Esp >= 0x10000 && (c.Esp & 3) == 0 &&
            !_host.Breakpoints.Contains(caller) && !_awardedQueries.Contains(query.Sequence) && _awardsBegun < MaximumAwards,
            "Native Star award caller/ESP or single-award query reservation differs.");
        _awardedQueries.Add(query.Sequence); ++_awardsBegun;
        AwardFrame frame = new(query, c.Esp, caller, c.Esi, c.Edi, U(query.Star + 0x34), Read(_profile, ProfileBytes));
        _pendingAward = frame; Add("award.entered");
        _host.Breakpoints.AddReturn(caller, tid, "NativeStarAwardObservation.award.return",
            (uint rt, uint at, ref Win32Native.X86Context returned) => AwardReturn(rt, at, ref returned, frame));
        Sample("award-entry", new { query.Sequence, star = Hex(query.Star), query.Path, query.Retried,
            query.SecondaryMiss, profile = Hex(_profile), caller = Hex(caller), entryEsp = Hex(c.Esp),
            nativeValueRaw = frame.Value, profile6F0Before = frame.CounterBefore });
    }

    private void AwardReturn(uint tid, uint at, ref Win32Native.X86Context c, AwardFrame frame)
    {
        Ensure(tid);
        try
        {
            Require(Active && ReferenceEquals(_pendingAward, frame) && _awaitingQuery is null &&
                _eligibility?.Active == true && _eligibility.CurrentQuery == frame.Query &&
                at == frame.Caller && c.Eip == at && c.Esp == checked(frame.Esp + 4) &&
                c.Esi == frame.Esi && c.Edi == frame.Edi && U(frame.Query.Star) == StarTable &&
                U(frame.Query.Star + 0xCC) == frame.Query.Node && Byte(frame.Query.Star + 0x40) == 1,
                "Native Star award C3 return lost its query, original ESP/registers or live receiver.");
            VerifyOwners(); byte[] after = Read(_profile, ProfileBytes);
            int[] offsets = Enumerable.Range(0, ProfileBytes).Where(i => frame.Before[i] != after[i]).ToArray();
            Require(offsets.Length <= 512, "Native Star award exceeded its bounded canonical Profile difference budget.");
            frame.CounterAfter = BitConverter.ToUInt32(after, CounterOffset);
            frame.ObservedDelta = (long)frame.CounterAfter - frame.CounterBefore;
            frame.Changes = offsets.Select(i => new ByteChange((uint)i, frame.Before[i], after[i])).ToArray();
            frame.Before = []; frame.Returned = true; ++_awardsReturned;
            _pendingAward = null; _awaitingQuery = frame; Add("award.native-returned");
            Sample("award-return", new { frame.Query.Sequence, star = Hex(frame.Query.Star), frame.Query.Path,
                frame.Query.Retried, caller = Hex(at), returnedEsp = Hex(c.Esp), nativeEspDelta = 4,
                preservedEsi = Hex(c.Esi), preservedEdi = Hex(c.Edi), active40 = 1,
                profile6F0Before = frame.CounterBefore, profile6F0After = frame.CounterAfter,
                actualDelta = frame.ObservedDelta, profileBytes = ProfileBytes, changedBytes = frame.Changes.Length,
                changed = frame.Changes, awaitingNativeQueryRetirement = true, awardReturnAlMeaningClaimed = false });
        }
        catch { _failed = true; throw; }
    }

    private void QueryCompleted(uint tid, StarSecondaryEligibility.QueryCompletion completion)
    {
        Ensure(tid);
        try
        {
            StarSecondaryEligibility.QueryIdentity query = completion.Query;
            Require(Active && _eligibility?.Active == true && !_eligibility.HasPendingNativePair &&
                _eligibility.CurrentQuery is null && _pendingAward is null && query.Sequence > _lastCompletedQuery &&
                query.Profile == _profile && query.Scene == _scene && ++_queriesCompleted <= MaximumQueries,
                "Star award query completion is stale, nested or precedes its exact native query return.");
            VerifyOwners();
            if (!completion.GeometryAccepted)
            {
                Require(_awaitingQuery is null && !_awardedQueries.Contains(query.Sequence),
                    "Native Star award occurred in a query whose native geometry did not accept proximity.");
                Add("query.without-proximity-award");
            }
            else
            {
                AwardFrame frame = _awaitingQuery ?? throw new InvalidDataException("Accepted native Star query has no paired award return.");
                Require(frame.Query == query && frame.Returned && _awardedQueries.Contains(query.Sequence) &&
                    !query.SecondaryMiss && completion.NativeReturnAl == 1 && completion.Active40 == 0 &&
                    Byte(query.Star + 0x40) == 0 && U(query.Star) == StarTable && U(query.Star + 0xCC) == query.Node &&
                    frame.CounterAfter > frame.CounterBefore && frame.ObservedDelta > 0 &&
                    U(_profile + CounterOffset) == frame.CounterAfter,
                    "Accepted native Star query lacks exactly one positive canonical award and completed native retirement.");
                if (query.Retried) { ++_secondaryAwards; Add("award.secondary-completed"); }
                else { ++_primaryAwards; Add("award.primary-completed"); }
                _awaitingQuery = null;
                Sample("query-award-completed", new { query.Sequence, star = Hex(query.Star), query.Path,
                    query.Retried, secondaryBodyEligible = query.Retried, exactlyOneNativeAward = true,
                    profile = Hex(_profile), profile6F0Before = frame.CounterBefore, profile6F0After = frame.CounterAfter,
                    actualDelta = frame.ObservedDelta, nativeValueRaw = frame.Value,
                    completion.NativeReturnAl, completion.Active40, completion.StateC8,
                    nativeCaller = Hex(completion.ReturnedCaller), returnedEsp = Hex(completion.ReturnedEsp),
                    nativeRetirementValidatedByQueryPeer = true, resourceArithmeticOrGameFieldWrites = 0 });
            }
            _lastCompletedQuery = query.Sequence;
        }
        catch { _failed = true; throw; }
    }

    private void VerifyOwners() => Require(U(0x765AD4) == _profile && U(_profile) == ProfileTable &&
        U(_profile + 0x2B4) == _primaryInstance && U(_primaryInstance) == 0x710F80 &&
        U(_primaryInstance + 0x24) == _primaryRoot && U(_primaryRoot) == 0x6DC4F4 && U(_primaryRoot + 0x3C) == _scene,
        "Native Star award lost its canonical typed Profile/Instance/Root/scene.");

    private void HookAward()
    {
        byte[] expected = Convert.FromHexString("A1D45A760085C0");
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            Ensure(tid); if (_retired || _failed) return;
            Require(Active && at == AwardEntry && c.Eip == at && Read(at, expected.Length).SequenceEqual(expected),
                "Native Star award callback lost its original instruction or bound lifetime.");
            try { AwardEntryStop(tid, ref c); }
            catch { _failed = true; throw; }
            if (Active) _host.Breakpoints.AddReturn(AwardEntry, tid, "NativeStarAwardObservation.award.entry", repeat);
        };
        _host.Breakpoints.AddReturn(AwardEntry, _host.MainThreadId, "NativeStarAwardObservation.award.entry", repeat);
    }
    private void Pin(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(_image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, expected.Length).ToArray().SequenceEqual(expected) &&
            Read(address, expected.Length).SequenceEqual(expected), $"Native Star award source pin differs at {Hex(address)}.");
    }
    private void Sample(string name, object data)
    {
        if (_samples.Count >= MaximumDetailedSamples) return;
        _samples.Add(new { name, data }); _log(new { @event = "native-star-award-observation", name, data });
    }
    private void Add(string name)
    { uint value = _counts.GetValueOrDefault(name) + 1; Require(value <= MaximumQueries, "Native Star award count budget exceeded."); _counts[name] = value; }
    private uint Pointer(uint at) { uint value = U(at); Require(value >= 0x10000, "Native Star award pointer is null."); return value; }
    private uint U(uint at) => BitConverter.ToUInt32(Read(at, 4));
    private byte Byte(uint at) => Read(at, 1)[0];
    private byte[] Read(uint at, int count)
    {
        Ensure(_host.MainThreadId);
        Require(at >= 0x10000 && count is > 0 and <= ProfileBytes, "Native Star award read exceeds its bounded contract.");
        return Win32Native.ReadMemory(_host.ProcessHandle, at, count);
    }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId &&
        Environment.CurrentManagedThreadId == _ownerThread, "Native Star award observation requires its owned main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
