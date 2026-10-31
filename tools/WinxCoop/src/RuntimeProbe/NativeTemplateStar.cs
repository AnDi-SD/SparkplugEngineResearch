using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed partial class NativeTemplateProbe
{
    private readonly bool _starSecondaryEligibilityEnabled, _battleCombatFixtureEnabled, _templatePrepareFirstMagicStage;
    private StarSecondaryEligibility? _starSecondaryEligibility;
    private NativeStarAwardObservation? _starAwardObservation;
    private NativeCombatFixture? _templateCombatFixture;
    private readonly bool _starPlacementFixtureEnabled;
    private bool _starPlacementStarted, _starPlacementCompleted;
    private object? _starPlacementWitness;
    private object TemplateStarSummary => new
    {
        enabled = _starSecondaryEligibilityEnabled, eligibility = _starSecondaryEligibility?.Summary,
        awards = _starAwardObservation?.Summary, combatFixture = _templateCombatFixture?.Summary,
        bodyPlacementFixture = new { enabled = _starPlacementFixtureEnabled, started = _starPlacementStarted,
            completed = _starPlacementCompleted, witness = _starPlacementWitness,
            nativeFunction = Hex(0x599200), starOrProfilePositionWrites = 0, shippingAutomaticTeleportClaimed = false },
        sharedMagicEnergyQualified = false, fullE3Complete = false
    };

    private bool TryPlaceSecondaryAtObservedStar(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume)
    {
        if (!_starPlacementFixtureEnabled || _starPlacementStarted || _starSecondaryEligibility?.Active != true ||
            _starAwardObservation?.CompletedSecondaryAwards >= 1) return false;
        uint star = _starSecondaryEligibility.LastSecondaryMissStar, node = _starSecondaryEligibility.LastSecondaryMissNode;
        if (star < 0x10000 || node < 0x10000) return false;
        byte[] position; string space;
        try
        {
            if (U(star) != 0x702160 || Read(star + 0x40, 1)[0] != 1 || U(star + 0xCC) != node ||
                U(node) != 0x6DC4F4 || U(node + 0x3C) != _scene) return false;
            if (U(node + 0x2C) == _parent)
            { position = Read(node + 0x20, 12); space = "native-Star-local-under-common-parent"; }
            else
            {
                byte[] parent = Read(_parent + 0x74, 60);
                float[] identity = [0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1];
                if (!identity.Select((value, index) => BitConverter.ToSingle(parent, index * 4) == value).All(equal => equal)) return false;
                position = Read(node + 0x74, 12); space = "native-Star-world-with-identity-own-parent";
            }
        }
        catch (System.ComponentModel.Win32Exception) { return false; } // Native pool may retire an old candidate.
        Require(Enumerable.Range(0, 3).All(index => float.IsFinite(BitConverter.ToSingle(position, index * 4))),
            "Observed native Star placement has a nonfinite coordinate.");
        VerifyTemplateStarAtWorld(tid, ref context);
        byte[] primaryBefore = Read(_localRoot + 0x20, 12), ownBefore = Read(_ownRoot + 0x20, 12);
        _starPlacementStarted = true;
        _starPlacementWitness = new { star = Hex(star), node = Hex(node), coordinateSpace = space,
            requestedPositionHex = Convert.ToHexString(position), ownBefore = Convert.ToHexString(ownBefore),
            primaryBefore = Convert.ToHexString(primaryBefore), oneFixturePlacementOnly = true };
        Call(tid, ref context, calls, resume, 0x599200, 0,
            [_instance, BitConverter.ToUInt32(position, 0), BitConverter.ToUInt32(position, 4), BitConverter.ToUInt32(position, 8)], true,
            _ =>
            {
                byte[] ownAfter = Read(_ownRoot + 0x20, 12);
                Require(Read(_localRoot + 0x20, 12).SequenceEqual(primaryBefore) &&
                    Enumerable.Range(0, 3).All(index => float.IsFinite(BitConverter.ToSingle(ownAfter, index * 4))),
                    "Native Star fixture placement changed the primary or produced a nonfinite own position.");
                _starPlacementCompleted = true;
                _log(new { @event = "template-star-body-placement-fixture-complete", witness = _starPlacementWitness,
                    nativeFunction = Hex(0x599200), rawRuntimePositionWrites = 0,
                    actualOwnPositionHex = Convert.ToHexString(ownAfter), exactRequestedPositionReadback = ownAfter.SequenceEqual(position),
                    actualSecondaryPickupStillRequired = true });
            });
        return true;
    }

    private bool TryPrepareTemplateBattleCombat(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume)
    {
        if (!_battleCombatFixtureEnabled) return false;
        Require(_healthBattleFixture && _stage == 5 && !_dependentInterval && _instance == 0,
            "Template combat fixture must precede own native construction.");
        _templateCombatFixture ??= new(_host, _image, _log, _templatePrepareFirstMagicStage);
        return _templateCombatFixture.TryActivate(tid, ref context, calls, _profile, _record,
            Pointer(_localCharacter + 0x138), resume);
    }

    private void ObserveTemplateStarAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (!_starSecondaryEligibilityEnabled) return;
        Require(_sharedHealthAlias?.Active == true && _playerTraitsOwnFxReady && _idleActive &&
            _stage == 6 && !_dependentInterval, "Star union has no live own shared-player graph.");
        if (_starSecondaryEligibility is null)
        {
            _starSecondaryEligibility = new(_host, _image, _log);
            _starSecondaryEligibility.BindAtWorld(tid, ref context, _instance, _ownRoot, _scene);
            _starAwardObservation = new(_host, _image, _log);
            _starAwardObservation.BindAtWorld(tid, ref context, _starSecondaryEligibility);
        }
        VerifyTemplateStarAtWorld(tid, ref context);
    }

    private void VerifyTemplateStarAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (_starSecondaryEligibility?.Active == true) _starSecondaryEligibility.VerifyAtWorld(tid, ref context);
        if (_starAwardObservation?.Active == true) _starAwardObservation.VerifyAtWorld(tid, ref context);
    }

    private void RetireTemplateStarBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        if (_starAwardObservation?.Active == true) _starAwardObservation.RetireBeforeOwnDelete(tid, ref context);
        if (_starSecondaryEligibility?.Active == true) _starSecondaryEligibility.RetireBeforeOwnDelete(tid, ref context);
    }
}
