using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Separate neutral acceptance window after two counted native body/hair worlds.
/// This lifetime window is not another finite movement or jump proof.
/// </summary>
internal sealed partial class NativeTemplateProbe
{
    private const uint BodyLiveWorldLimit = 36000;
    private bool _bodyLiveHolding, _bodyLiveAppearanceVerified;
    private uint _bodyLiveWorlds, _bodyLivePostUnloadWorlds;
    private long _bodyLiveDeadline, _bodyLiveStarted, _bodyMenuHeldSince, _bodyMenuHeldMilliseconds;
    private int _bodyLiveDurationSeconds;

    private bool BodyLiveMenuSurvivalSucceeded => _bodyMenuHeldMilliseconds >= 30000;
    internal object BodyLiveSummary => new
    {
        enabled = _bodyLive, holding = _bodyLiveHolding, liveWorlds = _bodyLiveWorlds,
        postUnloadObservedWorlds = _bodyLivePostUnloadWorlds,
        worldLimit = BodyLiveWorldLimit, configuredWallClockSeconds = _bodyLiveDurationSeconds,
        countedInitializationWorlds = _idleWorldStops,
        observedLiveMilliseconds = _bodyLiveStarted == 0 ? 0 : Math.Max(0, Environment.TickCount64 - _bodyLiveStarted),
        continuousQualifiedMenuMilliseconds = _bodyMenuHeldMilliseconds,
        requiredQualifiedMenuMilliseconds = 30000, menuSurvivalPassed = BodyLiveMenuSurvivalSucceeded,
        privateInputWrites = 0, finiteMovementProofClaimed = false,
        matchedNativeMaterialAmbientVerified = _bodyLiveAppearanceVerified,
        ownGameplayHookInventoryHeldDuringLiveWindow = _bodyLiveHolding
    };

    private void ConfigureBodyLive(ProbeConfiguration config)
    {
        _bodyLive = config.Mode == "template-bloom-body-hair-live";
        if (!_bodyLive) return;
        Require(_layout.IsBloomBody && config.DurationSeconds is >= 5 and <= 600,
            "Body-live requires the exact Bloom-body input and bounded duration.");
        _bodyLiveDurationSeconds = config.DurationSeconds;
        _bodyLiveDeadline = checked(Environment.TickCount64 + (long)config.DurationSeconds * 1000);
    }

    private bool SkipBodyLiveOwnObservation(uint receiver) => _bodyLiveHolding && !_bodyUnloadStarted &&
        _instance != 0 && receiver != 0 &&
        (receiver == _character || receiver == _move || receiver == _machine || receiver == _animator || receiver == _traits);

    private void BeginBodyLiveHold()
    {
        Require(_bodyLive && !_bodyLiveHolding && _stage == 6 && _idleActive && !_dependentInterval &&
            _idleWorldStops == 2 && _ownCharacterTicks == 2 && _ownMoveUpdates == 2 &&
            _ownBloomNotifyTicks == 2 && _ownBloomUpdates == 2 && _bloomRetentionFailure.Length == 0 &&
            _commandMovement is null && _hairEnabled && HairModeSucceeded && _bloomBodyGraphVerified && _nearPositioned,
            "Body-live cannot begin before its two counted native worlds, hair cleanup and neutral bindings.");
        CaptureRetainedBloomBodyGraph("before-separate-neutral-live-hold");
        Require(_bodyLiveAppearanceVerified, "Body-live native material ambient is not qualified against the local body/hair.");
        Require(VerifyBodyLifetimeGameplayReady(), "Body-live acceptance window must begin in the captured playing state.");
        VerifyBodyNeutralForLifetime();
        RetainBodyTemplateOwner();
        _idleChecksPassed = true;
        _bodyLiveHolding = true;
        _bodyLiveStarted = Environment.TickCount64;
        _log(new { @event = "template-body-live-hold-begin", countedWorlds = _idleWorldStops,
            ownCharacterTicks = _ownCharacterTicks, ownMoveUpdates = _ownMoveUpdates,
            ownBloomNotifyTicks = _ownBloomNotifyTicks, ownBloomUpdates = _ownBloomUpdates,
            worldLimit = BodyLiveWorldLimit, configuredWallClockSeconds = _bodyLiveDurationSeconds,
            finiteMovementProbe = false, nativePrivateInputWrites = 0,
            nativeGameplayContinuesWithoutPerCallbackInventory = true });
        ObserveBodyLiveWorld();
    }

    private void ObserveBodyLiveWorld()
    {
        Require(_bodyLive && _bodyLiveHolding && !_bodyUnloadStarted && _stage == 6 && _idleActive &&
            !_dependentInterval && _idleWorldStops == 2 && _commandMovement is null && HairModeSucceeded,
            "Body-live world escaped its separate neutral retained lifetime.");
        Require(Environment.TickCount64 <= _bodyLiveDeadline && ++_bodyLiveWorlds <= BodyLiveWorldLimit,
            "Body-live acceptance exceeded its bounded wall-clock/world budget.");
        VerifyBodyNeutralForLifetime();
        VerifyBodyTemplateOwner();
        Table(_instance, 0x710F80); Table(_ownRoot, VisualGraphReader.NodeVtable);
        Table(_character, 0x6F6438); Table(_move, 0x6F64F8); Table(_machine, 0x6FA288);
        Table(_animator, 0x6F670C); Table(_ownActor, 0x703F80);
        Require(Refs(_instance) == _instanceRefs && Refs(_ownRoot) == 7 &&
            U(_instance + 0x14) == _template && U(_instance + 0x24) == _ownRoot && U(_instance + 0x20) == 6 &&
            U(_ownRoot + 0x2C) == _parent && U(_ownRoot + 0x3C) == _scene &&
            U(_character + 0x24) == _instance && U(_character + 0x18) == _ownRoot &&
            U(_character + 0x124) == _machine && U(_character + 0x128) == _animator &&
            U(_character + 0x12C) == _move && U(_character + 0x130) == _ownCommand &&
            U(_character + 0x138) == _traits && U(_character + 0x148) == 1 && U(_character + 0x14C) == 41 &&
            U(_move + 0x160) == _character && U(_machine + 0x124) == _character && U(_machine + 0x128) == 0 &&
            U(_machine + 0x12C) == _ownCommand && U(_animator + 0x124) == _character &&
            U(_animator + 0x128) == _ownActor && _ownCommand != _command && _ownActor != _localActor,
            "Body-live native instance/private input/component ownership changed.");
        List<Component> components = Components(_instance, 6);
        Require(_ownEntities.Length == 5 && _ownEntities.All(entity => components.Count(component =>
            component.Address == entity && component.Owner == _instance && component.BoundNode == _ownRoot) == 1) &&
            LiveOwnMembership(Memberships(_ownEntities)),
            "Body-live native components lost their exact instance or live manager membership.");
        if (_bodyLiveWorlds <= 2 || _bodyLiveWorlds % 120 == 0)
            _log(new { @event = "template-body-live-world", world = _bodyLiveWorlds,
                countedInitializationWorlds = _idleWorldStops, playingReady = !_bodyLifetimePaused,
                elapsedMilliseconds = Math.Max(0, Environment.TickCount64 - _bodyLiveStarted),
                privateIntentNeutral = true, nativeInputWrites = 0, ownedRootReferences = 7 });
    }

    /// <summary>Called before ActorObserver's historical-player Snapshot route.</summary>
    internal void StepBodyLivePostUnload(DebuggerHost host, uint tid, ref Win32Native.X86Context ctx,
        NativeCallDispatcher calls, bool freshMainMenuObserved)
    {
        Require(_bodyLive && _bodyUnloadStarted, "Flow-only post-unload route requires the explicit body-live mode.");
        Require(Environment.TickCount64 <= _bodyLiveDeadline && ++_bodyLivePostUnloadWorlds <= BodyLiveWorldLimit,
            "Body-live post-unload acceptance exceeded its configured wall-clock budget.");
        _ = ObserveTemplatePostUnloadWorld(host, tid, ref ctx, calls, freshMainMenuObserved, () =>
        {
            // Hair path data/source were released after the first counted world.
            // No late native borrower or separate appearance buffer is installed.
            Require(!HairHasLiveOwnership && _commandMovement is null,
                "Body-live post-unload still owns unfinished hair/command resources.");
        });
        long now = Environment.TickCount64;
        if (freshMainMenuObserved)
        {
            if (_bodyMenuHeldSince == 0) _bodyMenuHeldSince = now;
            _bodyMenuHeldMilliseconds = Math.Max(0, now - _bodyMenuHeldSince);
        }
        else
        {
            _bodyMenuHeldSince = 0;
            _bodyMenuHeldMilliseconds = 0;
        }
        if (BodyLifetimeSucceeded && BodyLiveMenuSurvivalSucceeded)
            _log(new { @event = "template-body-live-acceptance-complete", nativeCleanupPassed = true,
                overlayMilliseconds = _bodyPauseMilliseconds, resumeTransitions = _bodyResumeTransitions,
                continuousQualifiedMenuMilliseconds = _bodyMenuHeldMilliseconds,
                fullE1VisualAcceptanceSuppliedSeparately = true });
    }
}
