using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed partial class NativeTemplateProbe
{
    private readonly bool _collisionDetectorSecondaryRoleEnabled;
    private CollisionDetectorSecondaryRole? _collisionDetectorSecondaryRole;
    private bool _storyDamageObservationInstalled;
    private uint _storyDamagePackets;

    private void InstallStoryDamageObservation()
    {
        if (!_sharedPlayerProfile.Story || _deathBattleFixture || _storyDamageObservationInstalled) return;
        _storyDamageObservationInstalled = true;
        Hook(0x4FE750, "story-damage-packet-readonly", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (_sharedHealthAlias?.Active != true || _storyDamagePackets >= 16 ||
                context.Ecx != _traits && context.Ecx != Pointer(_localCharacter + 0x138)) return;
            uint message = U(context.Esp + 4), sender = U(message + 0x10);
            ++_storyDamagePackets;
            _log(new { @event = "story-damage-packet-readonly", sequence = _storyDamagePackets,
                receiver = Hex(context.Ecx), caller = Hex(U(context.Esp)), message = Hex(message),
                messageId = Hex(U(message)), sender = Hex(sender), senderTable = sender >= 0x10000 ? Hex(U(sender)) : null,
                senderPrefix64 = sender >= 0x10000 ? Convert.ToHexString(Read(sender, 64)) : null,
                payload18 = U(message + 0x18), payload1C = Hex(U(message + 0x1C)),
                healthBefore = U(context.Ecx + 0x124), primaryPositionHex = Convert.ToHexString(Read(_localRoot + 0x20, 12)),
                ownPositionHex = Convert.ToHexString(Read(_ownRoot + 0x20, 12)),
                runtimeWrites = 0, nativeCallAdded = false, sourceSemanticsClaimed = false });
        }, preserveDuringHealthBattle: true);
    }

    private void ObserveCollisionDetectorAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (!_collisionDetectorSecondaryRoleEnabled) return;
        InstallStoryDamageObservation();
        Require(_sharedPlayerProfile.Story && _sharedHealthAlias?.Active == true && TargetRegionFixture &&
            _targetRegionConfigured && _idleActive && _stage == 6 && !_dependentInterval,
            "Ghoul role has no admitted story/native TargetRegion/common-HP graph.");
        if (_collisionDetectorSecondaryRole is null)
        {
            _collisionDetectorSecondaryRole = new(_host, _image, _log);
            _collisionDetectorSecondaryRole.BindAtWorld(tid, ref context, _instance, _ownRoot, _move, _scene, _sharedHealthAlias!);
        }
        VerifyCollisionDetectorAtWorld(tid, ref context);
    }

    private void VerifyCollisionDetectorAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (_collisionDetectorSecondaryRole?.Active == true) _collisionDetectorSecondaryRole.VerifyAtWorld(tid, ref context);
    }

    private void RetireCollisionDetectorBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        if (_collisionDetectorSecondaryRole?.Active == true) _collisionDetectorSecondaryRole.RetireBeforeOwnDelete(tid, ref context);
    }
}
