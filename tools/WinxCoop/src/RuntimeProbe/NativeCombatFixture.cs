using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Explicit native combat fixture: Profile510 through original4E3970(true),
/// optionally the proven story-case1 DWORD6E4 assignment. This is neither a
/// full cinematic replay nor a maximum-unlock or production progression API.
/// </summary>
internal sealed class NativeCombatFixture
{
    private const uint CombatSetter = 0x4E3970, StageStore = 0x4EB6E8;
    private const string CombatSetterBytes = "8B442404BA010000003BC27509889110050000C20400";
    private const string StageStoreBytes = "C780E406000001000000";
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly bool _prepareFirstMagicStage;
    private readonly int _ownerThread;
    private uint _profile, _record, _traits, _character, _root, _scene, _directFixtureWrites;
    private byte[] _healthBefore = [], _healthAfter = [];
    private bool _begun, _completed, _failed;

    internal NativeCombatFixture(DebuggerHost host, PEReader image, Action<object> log, bool prepareFirstMagicStage)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _prepareFirstMagicStage = prepareFirstMagicStage;
        _ownerThread = Environment.CurrentManagedThreadId;
    }

    internal bool Begun => _begun;
    internal bool Completed => _completed;
    internal uint DirectFixtureWrites => _directFixtureWrites;
    internal object Summary => new
    {
        begun = _begun, completed = _completed, terminalFailure = _failed,
        firstMagicStageFixtureRequested = _prepareFirstMagicStage,
        profile = Hex(_profile), record = Hex(_record), traits = Hex(_traits),
        character = Hex(_character), root = Hex(_root), scene = Hex(_scene),
        nativeCombatFunction = Hex(CombatSetter), nativeArgument = 1,
        directFixtureWrites = _directFixtureWrites, directFixtureWriteTarget = "Profile DWORD6E4, only0->1",
        healthBeforeRaw = Convert.ToHexString(_healthBefore), healthAfterRaw = Convert.ToHexString(_healthAfter),
        nativeReturnValueMeaningClaimed = false, fullStoryCinematicReplayed = false,
        maximumUpgradeApplied = false, runtimeCombatQualificationClaimed = false, fullE3Complete = false
    };

    // true means Begin owns the current stop; the caller must return without
    // rearming it. The completion callback invokes resume exactly once. A later
    // real WorldRET can proceed after this method returns false for Completed.
    internal bool TryActivate(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        uint profile, uint record, uint traits, Action resume)
    {
        Ensure(tid);
        ArgumentNullException.ThrowIfNull(calls);
        ArgumentNullException.ThrowIfNull(resume);
        Require(context.Eip == NativeCallDispatcher.WorldReturn && _host.ImageBase == 0x400000 &&
            _host.ProcessHandle != IntPtr.Zero && !_failed,
            "Combat fixture requires its owned original WorldRET without a prior failure.");
        if (_completed) return false; // Do not read historical owner fields after completion.
        Require(!_begun && !calls.Pending, "Combat fixture activation is already begun or another native call is pending.");
        try
        {
            _profile = profile; _record = record; _traits = traits;
            _character = Pointer(_traits + 0x18C); _root = Pointer(_record + 0x24); _scene = Pointer(_root + 0x3C);
            VerifyOwners(); Pin(CombatSetter, CombatSetterBytes);
            byte previous = Read(_profile + 0x510, 1)[0];
            _healthBefore = Read(_traits + 0x124, 16);
            _begun = true;
            calls.Begin(_host, tid, ref context, CombatSetter, _profile, [1], false, _ =>
            {
                Ensure(tid);
                try
                {
                    VerifyOwners();
                    Require(Read(_profile + 0x510, 1)[0] == 1 && Read(_traits + 0x124, 16).SequenceEqual(_healthBefore),
                        "Native combat fixture activation lost its owners or changed health.");
                    if (_prepareFirstMagicStage)
                    {
                        // Original story case1 follows the typed Profile getter
                        // with MOV DWORD[EAX+6E4],1. Reproduce only that assignment.
                        Pin(StageStore, StageStoreBytes);
                        uint beforeStage = U(_profile + 0x6E4);
                        Require(beforeStage is 0 or 1 && _directFixtureWrites == 0,
                            "First magic fixture requires a cold stage0/1 profile.");
                        if (beforeStage == 0)
                        {
                            Win32Native.WriteMemory(_host.ProcessHandle, _profile + 0x6E4, BitConverter.GetBytes(1u), executable: false);
                            ++_directFixtureWrites;
                        }
                        Require(U(_profile + 0x6E4) == 1, "First magic fixture profile write/readback differs.");
                        _log(new { @event = "shared-state-first-magic-fixture-initialized",
                            profile = Hex(_profile), sourceInstruction = Hex(StageStore),
                            originalInstruction = StageStoreBytes, previous = beforeStage, current = 1,
                            directFixtureWrites = _directFixtureWrites, fullStoryCinematicReplayed = false,
                            substitution = "Only proven story-case1 profile assignment; no story/Flow replay or maximum upgrade" });
                    }
                    VerifyOwners(); _healthAfter = Read(_traits + 0x124, 16);
                    Require(_healthAfter.SequenceEqual(_healthBefore) && Read(_profile + 0x510, 1)[0] == 1,
                        "Combat fixture completion changed native health or permission after readback.");
                    _completed = true;
                    _log(new { @event = "shared-state-native-combat-activated", function = Hex(CombatSetter),
                        profile = Hex(_profile), previous, current = 1, nativeArgument = 1,
                        fullStoryCinematicReplayed = false, profileStage6E4Raw = U(_profile + 0x6E4),
                        directFieldWrites = _directFixtureWrites, nativeReturnValueMeaningClaimed = false });
                    resume();
                }
                catch { _failed = true; throw; }
            });
            return true;
        }
        catch { _failed = true; throw; }
    }

    private void VerifyOwners() => Require(
        U(0x765AD4) == _profile && U(_profile) == 0x6F5A44 && U(_profile + 0x2B4) == _record &&
        U(_record) == 0x710F80 && U(_record + 0x24) == _root && U(_root) == 0x6DC4F4 &&
        U(_root + 0x3C) == _scene && U(_traits) == 0x6F6990 && U(_traits + 0x24) == _record &&
        U(_traits + 0x18C) == _character && U(_character) == 0x6F6438 &&
        U(_character + 0x24) == _record && U(_character + 0x18) == _root && U(_character + 0x138) == _traits,
        "Combat fixture lost its typed canonical Profile/Instance/Root/Character/PlayerTraits owners.");

    private void Pin(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(_image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, expected.Length).ToArray().SequenceEqual(expected) &&
            Read(address, expected.Length).SequenceEqual(expected),
            $"Combat fixture source instruction differs at {Hex(address)}.");
    }
    private uint Pointer(uint at) { uint value = U(at); Require(value >= 0x10000, "Combat fixture native pointer is null."); return value; }
    private uint U(uint at) => BitConverter.ToUInt32(Read(at, 4));
    private byte[] Read(uint at, int count)
    {
        Ensure(_host.MainThreadId);
        Require(at >= 0x10000 && count is > 0 and <= 32, "Combat fixture read exceeds its bounded contract.");
        return Win32Native.ReadMemory(_host.ProcessHandle, at, count);
    }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId &&
        Environment.CurrentManagedThreadId == _ownerThread, "Combat fixture requires its owned main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
