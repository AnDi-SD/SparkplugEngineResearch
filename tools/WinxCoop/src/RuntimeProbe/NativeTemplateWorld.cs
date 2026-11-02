using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed partial class NativeTemplateProbe
{
    // Borrowed addresses never leave this stopped-process reader. Admission is
    // independent of input-credit/Diary pause, which must not reset world sync.
    internal bool WorldStateBodyReady => _networkInputFixture && _networkInputStarted &&
        !_networkInputRetired && !_bodyUnloadStarted && !_sharedBattleUnloadStarted &&
        _stage == 6 && _idleActive && !_dependentInterval;

    internal IReadOnlyList<WorldPresentationActorState> ReadWorldStateProjectiles(uint tid,
        ref Win32Native.X86Context context)
    {
        if (_stingerSecondaryRole?.Active != true) return Array.Empty<WorldPresentationActorState>();
        Require(WorldStateBodyReady && _level == 47 && _host.IsAtDebugStop && tid == _host.MainThreadId &&
            context.Eip == NativeCallDispatcher.WorldReturn,
            "Projectile world capture requires the retained Battle47 native body interval.");
        return _stingerSecondaryRole.CaptureActiveAtWorld(tid, ref context);
    }

    internal (WorldPresentationNativeRig Primary, WorldPresentationNativeRig Secondary, SharedProfileScene Scene)
        ReadWorldStateBodies(uint tid, ref Win32Native.X86Context context,
            WorldPresentationEntityReader reader, uint epoch, uint generation)
    {
        Require(WorldStateBodyReady && _host.IsAtDebugStop && tid == _host.MainThreadId &&
            context.Eip == NativeCallDispatcher.WorldReturn, "World capture requires the retained native body interval.");
        var primary = reader.ReadRigAtWorld(tid, ref context, _record, WorldPresentationActorState.BloomType);
        var secondary = reader.ReadRigAtWorld(tid, ref context, _instance, WorldPresentationActorState.BloomType);
        Require(primary.Scene == _scene && secondary.Scene == _scene && primary.Root == _localRoot &&
            secondary.Root == _ownRoot, "World capture bodies changed scene/root ownership.");
        return (primary, secondary, new(epoch, generation, _level, _active, U(_active), _scene));
    }
}
