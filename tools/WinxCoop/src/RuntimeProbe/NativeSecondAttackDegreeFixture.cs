using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Guarded native tutorial-selector53 fixture. It permits only the visible
/// bitmap428 bridge's OR00200000 at DWORD42C, preserving rank/permission/HP.
/// The protected API's internal body and held-attack gameplay are not inferred.
/// </summary>
internal sealed class NativeSecondAttackDegreeFixture
{
    private const uint Setter = 0x4E38B0, Selector = 53, ThunkSlot = 0x13B176C, ThunkTarget = 0x1401310, VisibleBridge = 0x401300;
    private const int ProfileBytes = 0x2CB8, BitmapOffset = 0x42C;
    private const uint DegreeMask = 0x00200000;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly int _ownerThread;
    private uint _profile, _record, _traits, _character, _root, _scene, _runtimeThunk, _runtimeThunkAfter;
    private uint _bitmapBefore, _bitmapAfter;
    private byte _permissionBefore, _permissionAfter, _energyEnabledBefore, _energyEnabledAfter;
    private byte[] _healthBefore = [], _healthAfter = [];
    private int[] _changedOffsets = [];
    private bool _begun, _completed, _failed;

    internal NativeSecondAttackDegreeFixture(DebuggerHost host, PEReader image, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _ownerThread = Environment.CurrentManagedThreadId;
    }

    internal bool Begun => _begun;
    internal bool Completed => _completed;
    internal object Summary => new
    {
        begun = _begun, completed = _completed, terminalFailure = _failed,
        nativeFunction = Hex(Setter), nativeSelector = Selector, profile = Hex(_profile), record = Hex(_record),
        traits = Hex(_traits), character = Hex(_character), root = Hex(_root), scene = Hex(_scene),
        protectedThunkSlot = Hex(ThunkSlot), runtimeThunkTarget = Hex(_runtimeThunk), runtimeThunkTargetAfter = Hex(_runtimeThunkAfter),
        profileSnapshotBytes = ProfileBytes, bitmapOffset = Hex(BitmapOffset), allowedMask = Hex(DegreeMask),
        bitmapBefore = Hex(_bitmapBefore), bitmapAfter = Hex(_bitmapAfter),
        changedProfileOffsets = _changedOffsets.Select(offset => Hex((uint)offset)).ToArray(),
        permission510Before = _permissionBefore, permission510After = _permissionAfter,
        energyEnabled1A0Before = _energyEnabledBefore, energyEnabled1A0After = _energyEnabledAfter,
        healthBeforeRaw = Convert.ToHexString(_healthBefore), healthAfterRaw = Convert.ToHexString(_healthAfter),
        requiredPreservedRank6E4 = 1, rawProfileWrites = 0, resourceArithmeticReimplemented = false,
        protectedBodySemanticsFullyRecovered = false, nativeReturnValueMeaningClaimed = false,
        globalBossModeBroadcastAdded = false, fullStoryCinematicReplayed = false,
        heldAttackGameplayQualified = false, fullE3Complete = false
    };

    // A true return transfers this stop to NativeCallDispatcher. Resume only
    // rearms it; any next native call must wait for a later real WorldRET.
    internal bool TryActivate(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        uint profile, uint record, uint traits, Action resume)
    {
        Ensure(tid); ArgumentNullException.ThrowIfNull(calls); ArgumentNullException.ThrowIfNull(resume);
        Require(context.Eip == NativeCallDispatcher.WorldReturn && _host.ImageBase == 0x400000 &&
            _host.ProcessHandle != IntPtr.Zero && !_failed,
            "Second-degree fixture requires its owned original WorldRET without a prior failure.");
        if (_completed) return false; // No historical owner reads after completion.
        Require(!_begun && !calls.Pending, "Second-degree fixture is already begun or another native call is pending.");
        try
        {
            _profile = profile; _record = record; _traits = traits;
            _character = Pointer(_traits + 0x18C); _root = Pointer(_record + 0x24); _scene = Pointer(_root + 0x3C);
            VerifyOwners();
            Pin(0x4E38B0, "FF256C173B01");
            Pin(0x13B176C, "10134001", requireLive: false); // The loader may resolve only this slot to the exact bridge below.
            Pin(0x5E019B, "A1D45A760085C0750AE8E710E2FFA3D45A76008B8E9C000000518BC8E8F436F0FF");
            Pin(0x401300, "568BF18B4C24088BC19983E21F03C2C1F80581E11F0000807908490B0DDC323B0141BA01000000D3E2099486280400005EC20400");
            Pin(0x5255FC, "6A35E88D5CFBFF8BC8E866E2FBFF");
            Pin(0x5256EA, "6A35E89F5BFBFF8BC8E878E1FBFF");
            _runtimeThunk = VerifyThunkRoute("before-native-call");
            Require(U(_profile + 0x6E4) == 1, "Second-degree fixture requires the unchanged rank1 cohort.");
            _permissionBefore = Read(_profile + 0x510, 1)[0];
            _energyEnabledBefore = Read(_traits + 0x1A0, 1)[0];
            Require(_permissionBefore <= 1 && _energyEnabledBefore <= 1, "Second-degree fixture owner flags are not native boolean inputs.");
            _healthBefore = Read(_traits + 0x124, 16);
            byte[] before = Read(_profile, ProfileBytes), expected = (byte[])before.Clone();
            _bitmapBefore = BitConverter.ToUInt32(before, BitmapOffset);
            expected[BitmapOffset + 2] |= 0x20; // Host-only expected snapshot; never written to the target.
            _begun = true;
            _log(new { @event = "native-second-attack-degree-fixture", name = "begin",
                data = new { function = Hex(Setter), selector = Selector, profile = Hex(_profile),
                    bitmapBefore = Hex(_bitmapBefore), alreadySet = (_bitmapBefore & DegreeMask) != 0,
                    rank6E4 = 1, permission510 = _permissionBefore, energyEnabled1A0 = _energyEnabledBefore,
                    runtimeThunkTarget = Hex(_runtimeThunk), rawProfileWrites = 0 } });
            calls.Begin(_host, tid, ref context, Setter, _profile, [Selector], false, _ =>
            {
                Ensure(tid);
                try
                {
                    VerifyOwners();
                    byte[] after = Read(_profile, ProfileBytes);
                    _bitmapAfter = BitConverter.ToUInt32(after, BitmapOffset);
                    _changedOffsets = Enumerable.Range(0, ProfileBytes).Where(offset => before[offset] != after[offset]).ToArray();
                    _permissionAfter = Read(_profile + 0x510, 1)[0];
                    _energyEnabledAfter = Read(_traits + 0x1A0, 1)[0];
                    _healthAfter = Read(_traits + 0x124, 16);
                    _runtimeThunkAfter = VerifyThunkRoute("after-native-return");
                    Require(after.SequenceEqual(expected) && _bitmapAfter == (_bitmapBefore | DegreeMask) &&
                        U(_profile + 0x6E4) == 1 && _permissionAfter == _permissionBefore &&
                        _energyEnabledAfter == _energyEnabledBefore && _healthAfter.SequenceEqual(_healthBefore),
                        "Native selector53 changed more than its allowed Profile bit or altered rank/permission/HP/energy-enable/thunk identity.");
                    _completed = true;
                    _log(new { @event = "native-second-attack-degree-fixture", name = "completed",
                        data = new { function = Hex(Setter), selector = Selector, profile = Hex(_profile),
                            bitmapBefore = Hex(_bitmapBefore), bitmapAfter = Hex(_bitmapAfter),
                            changedProfileOffsets = _changedOffsets.Select(offset => Hex((uint)offset)).ToArray(),
                            fullProfileMatchedAllowedOr = true, rank6E4 = 1,
                            permission510Before = _permissionBefore, permission510After = _permissionAfter,
                            energyEnabled1A0Before = _energyEnabledBefore, energyEnabled1A0After = _energyEnabledAfter,
                            healthUnchanged = true, rawProfileWrites = 0, heldAttackGameplayClaimed = false } });
                    resume();
                }
                catch { _failed = true; throw; }
            });
            return true;
        }
        catch { _failed = true; throw; }
    }

    private void VerifyOwners() => Require(U(0x765AD4) == _profile && U(_profile) == 0x6F5A44 &&
        U(_profile + 0x2B4) == _record && U(_record) == 0x710F80 && U(_record + 0x24) == _root &&
        U(_root) == 0x6DC4F4 && U(_root + 0x3C) == _scene && U(_traits) == 0x6F6990 &&
        U(_traits + 0x24) == _record && U(_traits + 0x18C) == _character && U(_character) == 0x6F6438 &&
        U(_character + 0x24) == _record && U(_character + 0x18) == _root && U(_character + 0x138) == _traits,
        "Second-degree fixture lost its typed canonical Profile/Instance/Root/Character/PlayerTraits owners.");
    private uint VerifyThunkRoute(string boundary)
    {
        uint target = U(ThunkSlot);
        _log(new { @event = "native-second-attack-degree-fixture", name = "protected-route",
            data = new { boundary, slot = Hex(ThunkSlot), fileSlotBytes = Convert.ToHexString(Image(ThunkSlot, 4)),
                liveSlotBytes = TryBytes(ThunkSlot, 4), liveTarget = Hex(target),
                gatewayBytes = TryBytes(Setter, 6), targetBytes32 = TryBytes(target, 32),
                allowedRuntimeTarget = target is ThunkTarget or VisibleBridge, nativeCallAddedHere = false } });
        Require(target is ThunkTarget or VisibleBridge,
            "Second-degree fixture rejected an unknown protected target; bounded bytes were logged and no call is allowed.");
        if (target == ThunkTarget)
        {
            // Identity only: the protected thunk prefix is not a recovered body.
            Pin(0x1401310, "682A13400168BF5E40006844288A009C816C2404049F01009DC3");
        }
        else Pin(0x401300, "568BF18B4C24088BC19983E21F03C2C1F80581E11F0000807908490B0DDC323B0141BA01000000D3E2099486280400005EC20400");
        return target;
    }
    private void Pin(uint address, string hex, bool requireLive = true)
    {
        byte[] expected = Convert.FromHexString(hex);
        byte[] file = Image(address, expected.Length); string? live = TryBytes(address, expected.Length);
        bool fileMatches = file.SequenceEqual(expected), liveMatches = live == Convert.ToHexString(expected);
        if (!fileMatches || requireLive && !liveMatches)
            _log(new { @event = "native-second-attack-degree-fixture", name = "pin-mismatch",
                data = new { address = Hex(address), expected = Convert.ToHexString(expected), file = Convert.ToHexString(file),
                    live, fileMatches, liveMatches, requireLive, nativeContinuationForbidden = true } });
        Require(fileMatches && (!requireLive || liveMatches), $"Second-degree fixture source identity differs at {Hex(address)}.");
    }
    private byte[] Image(uint address, int count) => _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, count).ToArray();
    private string? TryBytes(uint address, int count)
    {
        if (address < 0x10000 || address > uint.MaxValue - (uint)count) return null;
        try { return Convert.ToHexString(Read(address, count)); }
        catch (System.ComponentModel.Win32Exception) { return null; }
    }
    private uint Pointer(uint at) { uint value = U(at); Require(value >= 0x10000, "Second-degree fixture pointer is null."); return value; }
    private uint U(uint at) => BitConverter.ToUInt32(Read(at, 4));
    private byte[] Read(uint at, int count)
    { Ensure(_host.MainThreadId); Require(at >= 0x10000 && count is > 0 and <= ProfileBytes, "Second-degree fixture read exceeds its bound."); return Win32Native.ReadMemory(_host.ProcessHandle, at, count); }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId && Environment.CurrentManagedThreadId == _ownerThread,
        "Second-degree fixture requires its owned main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
