using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// One owned process publishes the original seven entry probes once. Fresh
/// scene scopes receive those callbacks without stacking LIFO return probes or
/// republishing INT3 over another scope's live pins. This does not initiate load.
/// </summary>
internal sealed class NativeWorldEntryRouter
{
    private const uint MaximumOrdinals = 8, MaximumCallbacks = 100000;
    private static readonly uint[] Sites = [NativeWorldEntryFrame.LoadEntry, 0x4EEF30, 0x4EEAF0,
        0x4EE5B0, 0x4EE70D, 0x5F9A2F, 0x5F9A58];
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly WorldStateRole _role;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _owner;
    private bool _installed, _retired, _failed;
    private uint _nativeOrdinal, _scopes, _callbacks;
    private string _failure = "";

    internal NativeWorldEntryRouter(DebuggerHost host, PEReader image, Action<object> log, WorldStateRole role)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image); ArgumentNullException.ThrowIfNull(log);
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 && host.MainThreadId != 0 &&
            host.ImageBase == 0x400000 && role is WorldStateRole.Host or WorldStateRole.Guest,
            "Native entry router requires its explicit role and owned original debug stop.");
        _host = host; _image = image; _log = log; _role = role; _process = host.ProcessHandle;
        _pid = host.ProcessPid; _thread = host.MainThreadId; _owner = Environment.CurrentManagedThreadId;
    }
    internal uint NativeEntryOrdinal => _nativeOrdinal;
    internal NativeWorldEntrySeed? CurrentScope { get; private set; }
    internal object Summary => new
    {
        role = _role.ToString(), installed = _installed, retired = _retired, failed = _failed, failure = _failure,
        processNativeEntryOrdinal = _nativeOrdinal, maximumNativeOrdinals = MaximumOrdinals,
        attachedScopes = _scopes, callbacks = _callbacks, maximumCallbacks = MaximumCallbacks,
        sitesPublishedOnce = _installed ? Sites.Length : 0, currentScope = CurrentScope?.Summary,
        repeatedScopeHookPublication = false, nativeCallsAdded = 0, nativeContextWrites = 0,
        oldScopeRetirementStopsProcessRouter = false, nextSceneOrReadyClaimed = false,
        breakpointRestorationOwner = "SoftwareBreakpointManager on dormant hit or process-session Dispose"
    };

    internal void InstallStopped()
    {
        EnsureStop(_thread); Require(!_installed && !_retired && !_failed,
            "Native entry process router publication is single-use.");
        Require(Convert.ToHexString(SHA256.HashData(_image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "Native entry process router requires the exact original PC image.");
        foreach (uint site in Sites)
            Require(!_host.Breakpoints.Contains(site) && Read(site, 8).SequenceEqual(Image(site, 8)),
                "Native entry process router site already belongs to another owner or differs from the original image.");
        Pin(0x5F9617, "E86475FAFF8D4C2414894544");
        Pin(0x5F9931, "8B4D448B551051528BC8E8F055EFFF8BCD"); Pin(0x4EF22B, "C20800");
        _installed = true;
        foreach (uint site in Sites) Arm(site);
        _log(new { @event = "world-entry-process-router-installed", role = _role.ToString(), sites = Sites.Select(Hex).ToArray(),
            processPid = _pid, originalSitePinsVerifiedOnce = true, currentScopeAttached = false,
            nativeCallsAdded = 0, nativeContextWrites = 0 });
    }

    internal void VerifyScopeConstruction(DebuggerHost host, PEReader image, WorldStateRole role, uint nativeOrdinalBase)
    {
        EnsureStop(_thread);
        Require(_installed && !_retired && !_failed && ReferenceEquals(host, _host) && ReferenceEquals(image, _image) &&
            role == _role && nativeOrdinalBase == _nativeOrdinal && nativeOrdinalBase < MaximumOrdinals,
            "Routed entry construction changed its validated process/image/role or native ordinal base.");
    }
    internal void AttachScope(NativeWorldEntrySeed scope)
    {
        ArgumentNullException.ThrowIfNull(scope); EnsureStop(_thread);
        Require(_installed && !_retired && !_failed && _scopes < MaximumOrdinals && !ReferenceEquals(scope, CurrentScope),
            "Native entry router cannot replace a retired process owner or reattach the same scope.");
        if (CurrentScope is { } previous)
            Require(previous.Retired && !previous.Failed && !previous.HasPendingOriginalEntry && previous.EntryCompleted && previous.WorldQualified,
                "Native entry router replacement requires its old completed native scope to be explicitly retired.");
        scope.AttachRoutedScope(this, _nativeOrdinal, firstScope: _scopes == 0);
        CurrentScope = scope; ++_scopes;
        _log(new { @event = "world-entry-process-router-scope-swapped", scopeOrdinal = _scopes,
            processNativeOrdinalBase = _nativeOrdinal, expectedNextOriginalOrdinal = checked(_nativeOrdinal + 1),
            originalSiteProbesRepublished = 0, oldScopePayloadsMutated = false });
    }

    internal void ObserveWorld(uint tid, ref Win32Native.X86Context context, in SharedProfileScene scene)
    {
        if (_retired || _failed) return;
        EnsureStop(tid);
        if (CurrentScope is { Retired: false } scope) scope.ObserveAtWorld(tid, ref context, scene);
    }
    internal void RetireProcess()
    {
        EnsureOwner(); if (_retired) return;
        Require(CurrentScope is null || !CurrentScope.HasPendingOriginalEntry,
            "Native entry process router cannot retire an unmatched original entry frame.");
        CurrentScope?.RetireCached(); _retired = true;
        _log(new { @event = "world-entry-process-router-retired", processNativeEntryOrdinal = _nativeOrdinal,
            attachedScopes = _scopes, immediateSiteRestorationClaimed = false, nativeReads = 0, nativeWrites = 0 });
    }
    private void Arm(uint site) => _host.Breakpoints.AddReturn(site, _thread,
        "EntryRouter." + Hex(site), (uint tid, uint at, ref Win32Native.X86Context context) => Site(tid, at, ref context));

    private void Site(uint tid, uint at, ref Win32Native.X86Context context)
    {
        if (_retired || _failed) return;
        try
        {
            EnsureStop(tid);
            Require(_installed && Sites.Contains(at) && context.Eip == at && ++_callbacks <= MaximumCallbacks &&
                Read(at, 8).SequenceEqual(Image(at, 8)),
                "Native entry process router left its original instruction or bounded callback cohort.");
            NativeWorldEntrySeed? scope = CurrentScope;
            if (at == NativeWorldEntryFrame.LoadEntry)
            {
                Require(context.Ecx >= 0x10000 && (context.Ecx & 3) == 0,
                    "Native entry process router saw a null or unaligned original receiver.");
                uint table = U(context.Ecx);
                if (table is 0x710874 or 0x7105C8)
                {
                    uint level = U(context.Ecx + 0x10);
                    Require(scope is { Retired: false, Failed: false } && scope.Level == level &&
                        (table == 0x710874 && (level == 28 || level == 3 && scope.StoryLevel3Authorized) ||
                            table == 0x7105C8 && level == 47) &&
                        _nativeOrdinal < MaximumOrdinals,
                        "Original native load has no live authorized bounded entry scope or exhausts its process ordinal.");
                    ++_nativeOrdinal;
                }
            }
            if (scope is { Retired: false }) scope.DispatchRoutedSite(this, at, tid, ref context, _nativeOrdinal);
            if (!_retired && !_failed) Arm(at);
        }
        catch (Exception error) { _failed = true; _failure = error.Message; throw; }
    }
    private void Pin(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(Image(address, expected.Length).SequenceEqual(expected) && Read(address, expected.Length).SequenceEqual(expected),
            "Original native entry process-router source pin differs.");
    }
    private void EnsureOwner() => Require(Environment.CurrentManagedThreadId == _owner,
        "Native entry process router changed its synchronous owner thread.");
    private void EnsureStop(uint tid)
    {
        EnsureOwner(); Require(_host.IsAtDebugStop && _host.ProcessHandle == _process && _host.ProcessPid == _pid &&
            _host.MainThreadId == _thread && tid == _thread && _host.ImageBase == 0x400000,
            "Native entry process router lost its owned original main-thread stop.");
    }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int bytes) => Win32Native.ReadMemory(_process, address, bytes);
    private byte[] Image(uint address, int bytes) => _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, bytes).ToArray();
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
