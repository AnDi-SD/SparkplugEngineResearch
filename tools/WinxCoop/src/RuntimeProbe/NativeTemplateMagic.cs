using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed partial class NativeTemplateProbe
{
    private readonly bool _sharedMagicEnergyEnabled;
    private SharedMagicEnergyAlias? _sharedMagicEnergy;
    private readonly bool _secondAttackDegreeFixtureEnabled;
    private NativeSecondAttackDegreeFixture? _secondAttackDegreeFixture;

    private bool TryPrepareSecondAttackDegree(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume)
    {
        if (!_secondAttackDegreeFixtureEnabled) return false;
        Require(_templateCombatFixture?.Completed == true && _stage == 5 && !_dependentInterval && _instance == 0,
            "Second attack degree fixture must follow first-stage combat preparation before own construction.");
        _secondAttackDegreeFixture ??= new(_host, _image, _log);
        return _secondAttackDegreeFixture.TryActivate(tid, ref context, calls, _profile, _record,
            Pointer(_localCharacter + 0x138), resume);
    }

    private void ObserveSharedMagicAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (!_sharedMagicEnergyEnabled) return;
        Require(_sharedHealthAlias?.Active == true && _idleActive && _stage == 6 && !_dependentInterval,
            "Shared magic has no active initialized own graph and reset guard.");
        if (_sharedMagicEnergy is null)
        {
            _sharedMagicEnergy = new(_host, _image, _log);
            _sharedMagicEnergy.BindAtWorld(tid, ref context, _record, Pointer(_localCharacter + 0x138),
                _localCharacter, _localMachine, _instance, _traits, _character, _machine);
        }
        VerifySharedMagicAtWorld(tid, ref context);
    }

    private void VerifySharedMagicAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (_sharedMagicEnergy?.Active == true) _sharedMagicEnergy.VerifyAtWorld(tid, ref context);
    }

    private void RetireSharedMagicBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        if (_sharedMagicEnergy?.Active == true) _sharedMagicEnergy.RetireBeforeOwnDelete(tid, ref context);
    }
}
