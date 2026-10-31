using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed partial class NativeTemplateProbe
{
    private readonly bool _deathBattleFixture;
    private readonly bool _sharedBattleLifetimeEnabled;
    private readonly int _sharedBattleDurationSeconds;
    private SharedDeathCommandAlias? _sharedDeathCommand;
    private NativeGameOverObservation? _storyGameOver;
    private bool SharedStoryOutcomePassed => !_sharedPlayerProfile.Story || !_deathBattleFixture ||
        _storyGameOver?.StoryGameOverObserved == true && _storyGameOver.StoryGameOverExitReturned;
    private bool SharedBattleGameplayGoalPassed => _bodyInputFixture ? BodyInputFixtureGoalPassed :
        _networkInputFixture ? NetworkInputFixtureGoalPassed :
        _ordinaryAttackFixture ? OrdinaryAttackFixtureGoalPassed : _deathBattleFixture ?
        _sharedDeathCommand?.CompletedPublications == 1 && _sharedHealthAlias?.BoundedDeathCycleObserved == true :
        _starSecondaryEligibilityEnabled ? _starAwardObservation?.CompletedSecondaryAwards >= 1 :
        _sharedMagicEnergyEnabled ? _sharedMagicEnergy?.CommittedSpendTransactions >= 1 :
        _sharedHealthAlias?.CommittedDamageTransactions >= 2 && (!_stingerSecondaryRoleEnabled ||
            _sharedHealthAlias.SecondaryCommittedDamageTransactions >= 1 && _stingerSecondaryRole?.OwnNativeDamageSendCount >= 1) &&
            (!_collisionDetectorSecondaryRoleEnabled || _sharedHealthAlias.SecondaryCommittedDamageTransactions >= 1 &&
                _collisionDetectorSecondaryRole?.OwnNativeDamageSendCount >= 1);

    private void ObserveStoryGameOverAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (!_sharedPlayerProfile.Story || _bodyInputFixture || _networkInputFixture) return;
        if (_storyGameOver is null)
        {
            _storyGameOver = new(_host, _image, _log);
            _storyGameOver.BindAtWorld(tid, ref context, _sharedPlayerProfile.Level);
        }
        VerifyStoryGameOverAtWorld(tid, ref context);
    }

    private void VerifyStoryGameOverAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (_storyGameOver?.Active == true) _storyGameOver.VerifyAtWorld(tid, ref context);
    }

    private void ObserveSharedDeathAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (!_deathBattleFixture) return;
        Require(_sharedHealthAlias?.Active == true && _idleActive && _stage == 6 && !_dependentInterval,
            "Shared death command has no active native common-HP graph.");
        if (_sharedDeathCommand is null)
        {
            _sharedDeathCommand = new(_host, _image, _log);
            _sharedDeathCommand.BindAtWorld(tid, ref context, _record, Pointer(_localCharacter + 0x138),
                _localCharacter, _localMachine, _instance, _traits, _character, _machine, _sharedHealthAlias!);
        }
        VerifySharedDeathAtWorld(tid, ref context);
    }

    private void VerifySharedDeathAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (_sharedDeathCommand?.Active == true) _sharedDeathCommand.VerifyAtWorld(tid, ref context);
    }

    private void RetireSharedDeathBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        if (_sharedDeathCommand?.Active == true) _sharedDeathCommand.RetireBeforeOwnDelete(tid, ref context);
    }

    private void RetireSharedBattleAdaptersAtEntry(uint tid, ref Win32Native.X86Context context)
    {
        RetireTemplateStarBeforeOwnDelete(tid, ref context);
        RetireSharedMagicBeforeOwnDelete(tid, ref context);
        if (_stingerSecondaryRole?.Active == true) _stingerSecondaryRole.RetireBeforeOwnDelete(tid, ref context);
        RetireSharedDeathBeforeOwnDelete(tid, ref context);
        RetireCollisionDetectorBeforeOwnDelete(tid, ref context);
        if (_sharedHealthAlias?.Active == true) _sharedHealthAlias.RetireBeforeOwnUnload(tid, ref context);
    }

    internal void StepSharedBattlePostUnload(DebuggerHost host, uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, bool freshMainMenuObserved)
    {
        VerifyStoryGameOverAtWorld(tid, ref context);
        StepSharedBattlePostUnload(host, tid, ref context, calls, freshMainMenuObserved, () =>
            Require(!HairHasLiveOwnership && _commandMovement is null,
                "Shared battle post-unload retains unfinished hair/command resources."));
        if (_cleaned && _storyGameOver?.Active == true) _storyGameOver.RetireAtFlowBoundary(tid, ref context);
    }
}
