using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own lifecycle policy for an explicitly enabled retained Bloom-body case.
/// It is separate from command readiness and never skips stock unload code.
/// </summary>
internal sealed partial class NativeTemplateProbe
{
    // Kept false until an explicit body-live mode assigns this in the main
    // constructor. Existing bounded modes retain their original strict policy.
    private bool _bodyLive = false;
    private uint _bodyLifetimeAnimationManager, _bodyLifetimeGameTimer;
    private byte _bodyLastNativePaused;
    private uint _bodyNativePauseTransitions;
    private bool _bodyLifetimeCaptured, _bodyLifetimePaused, _bodyPauseObserved;
    private long _bodyPauseStarted, _bodyPauseMilliseconds, _bodyLongestResumedPauseMilliseconds;
    private long _bodyNativePauseStarted, _bodyLongestCurrentNativePauseMilliseconds;
    private uint _bodyPauseTransitions, _bodyResumeTransitions;
    private bool _bodyUnloadStarted, _bodyUnloadBatchCompleted, _bodyUnloadResourcesReleased;
    private uint _bodyUnloadEntry, _bodyPostUnloadWorlds, _bodyMenuWorlds;
    private uint[] _bodyDeletedEntities = [];
    private object? _bodyUnloadWitness;
    private bool _bodyTemplateOwnerHeld;
    private ushort _bodyTemplateLastObservedReferences;
    private uint _bodyTemplateManagerMembership, _bodyTemplateOwnerWrites;

    internal bool BodyUnloadStarted => _bodyUnloadStarted;
    internal bool BodyLifetimePaused => _bodyLifetimePaused;
    internal bool BodyLifetimeSucceeded => _bodyLive && _bodyPauseObserved && _bodyResumeTransitions >= 1 &&
        _bodyLongestResumedPauseMilliseconds >= 5000 &&
        _bodyUnloadBatchCompleted && _bodyUnloadResourcesReleased && _bodyMenuWorlds >= 2 && _cleaned &&
        !_bodyTemplateOwnerHeld && _bodyTemplateOwnerWrites == 2;
    internal object BodyLifetimeSummary => new
    {
        enabled = _bodyLive,
        captured = _bodyLifetimeCaptured,
        paused = _bodyLifetimePaused,
        pauseObserved = _bodyPauseObserved,
        pauseTransitions = _bodyPauseTransitions,
        resumeTransitions = _bodyResumeTransitions,
        completedPauseMilliseconds = _bodyPauseMilliseconds,
        capturedGameTimer = Hex(_bodyLifetimeGameTimer), nativePaused = _bodyLastNativePaused,
        nativePauseTransitions = _bodyNativePauseTransitions,
        longestResumedNativePauseMilliseconds = _bodyLongestResumedPauseMilliseconds,
        requiredResumedNativePauseMilliseconds = 5000,
        nativePauseQualification = "same typed GameTimer +40 byte1, then actual ready resume",
        notReadyDurationIncludesOtherTransitions = true,
        unloadStarted = _bodyUnloadStarted,
        unloadEntry = Hex(_bodyUnloadEntry),
        nativeBatchCompleted = _bodyUnloadBatchCompleted,
        resourcesReleasedAtLaterWorld = _bodyUnloadResourcesReleased,
        postUnloadWorlds = _bodyPostUnloadWorlds,
        observedMenuWorlds = _bodyMenuWorlds,
        succeeded = BodyLifetimeSucceeded,
        witness = _bodyUnloadWitness,
        localPlayerReadAfterUnload = false,
        templateAdapterOwnerHeld = _bodyTemplateOwnerHeld,
        templateAdapterOwnerWrites = _bodyTemplateOwnerWrites,
        templateLastObservedReferences = _bodyTemplateLastObservedReferences,
        templateLastObservedManagerMembership = _bodyTemplateManagerMembership
    };

    // Factory registration is the manager's reference, not an adapter owner.
    // Native manager Clear(1) decrements +08 and deletes at zero. A separate
    // adapter reference keeps this template alive across ordinary menu exits.
    private void RetainBodyTemplateOwner()
    {
        Require(_bodyLive && !_bodyLiveHolding && !_bodyUnloadStarted && !_bodyTemplateOwnerHeld &&
            _stage == 6 && !_dependentInterval && _instance != 0 && _bodyTemplateOwnerWrites == 0,
            "Template retain requires the initialized explicit body-live interval.");
        Table(_template, 0x703D24);
        Require(Refs(_template) == 1 && CountBodyTemplateManagerMembership() == 1 && U(_template + 4) == 0,
            "Template retain requires its sole original manager registration owner.");
        WriteBodyTemplateReferences(2);
        _bodyTemplateOwnerHeld = true;
        _bodyTemplateLastObservedReferences = 2; _bodyTemplateManagerMembership = 1;
        _log(new { @event = "template-body-adapter-template-retained", template = Hex(_template),
            previousReferences = 1, references = 2, managerMembership = 1, adapterOwners = 1,
            writes = _bodyTemplateOwnerWrites, nativeEntityFieldsWritten = 0 });
    }

    private uint CountBodyTemplateManagerMembership()
    {
        Require(U(0x75DC64) == _templateManager, "Template registration manager identity changed.");
        Table(_templateManager, 0x6E77E0);
        uint head = Pointer(_templateManager + 0x18), count = U(_templateManager + 0x1C);
        Require(count <= 4096, "Template registration list exceeds its explicit diagnostic bound.");
        uint link = U(head), previous = head, matches = 0;
        var seen = new HashSet<uint>();
        for (uint index = 0; index < count; ++index)
        {
            Require(link >= 0x10000 && link != head && seen.Add(link) && U(link + 4) == previous,
                "Template registration list is inconsistent or cyclic.");
            if (U(link + 8) == _template) ++matches;
            previous = link; link = U(link);
        }
        Require(link == head && U(head + 4) == previous && matches <= 1,
            "Template registration count/sentinel or unique membership mismatch.");
        return matches;
    }

    private void VerifyBodyTemplateOwner(bool forceManagerObservation = false)
    {
        Require(_bodyLive && _bodyTemplateOwnerHeld && !_bodyUnloadStarted && _bodyTemplateOwnerWrites == 1,
            "Template ownership observation requires exactly one adapter retain.");
        Table(_template, 0x703D24);
        ushort references = Refs(_template);
        // Reobserve the entire stopped list at capture, a reference transition,
        // and unload. The steady frame path reads only the owned template's
        // table/counter; it does not add a full registry walk to every frame.
        if (forceManagerObservation || references != _bodyTemplateLastObservedReferences)
        {
            uint membership = CountBodyTemplateManagerMembership();
            Require(references == 1 + membership, "Template reference transition has an unqualified owner.");
            _log(new { @event = "template-body-adapter-template-owner-observed", template = Hex(_template),
                previousReferences = _bodyTemplateLastObservedReferences, references,
                managerMembership = membership, adapterOwners = 1, forcedManagerObservation = forceManagerObservation });
            _bodyTemplateLastObservedReferences = references; _bodyTemplateManagerMembership = membership;
        }
        Require(references == 1 + _bodyTemplateManagerMembership && references is 1 or 2 && U(_template + 4) == 0,
            "Template lost its exact adapter/manager reference accounting.");
    }

    private void ReleaseBodyTemplateOwnerForDeletion()
    {
        VerifyBodyTemplateOwner(forceManagerObservation: true);
        ushort previous = Refs(_template), remaining = checked((ushort)(previous - 1));
        WriteBodyTemplateReferences(remaining);
        _bodyTemplateOwnerHeld = false; _bodyTemplateLastObservedReferences = remaining;
        _log(new { @event = "template-body-adapter-template-released", template = Hex(_template),
            previousReferences = previous, references = remaining,
            managerMembership = _bodyTemplateManagerMembership, adapterOwners = 0,
            writes = _bodyTemplateOwnerWrites, nativeDeletingDestructorStillRequired = true });
    }

    private void WriteBodyTemplateReferences(ushort references)
    {
        Require(_bodyLive && !_bodyUnloadStarted && _host.IsAtDebugStop && _template >= 0x10000 &&
            _bodyTemplateOwnerWrites < 2, "Template reference write is outside its two owned transitions.");
        Table(_template, 0x703D24);
        byte[] bytes = BitConverter.GetBytes(references);
        Win32Native.WriteMemory(_host.ProcessHandle, _template + 8, bytes, executable: false);
        Require(Read(_template + 8, 2).SequenceEqual(bytes), "Template owned reference write/readback mismatch.");
        ++_bodyTemplateOwnerWrites;
    }

    /// <summary>
    /// Called only after the common local-owner checks, while the retained
    /// body is live. False means the same level survives under an overlay.
    /// The caller must hold command/observation phases while false.
    /// </summary>
    private bool VerifyBodyLifetimeGameplayReady()
    {
        Require(_bodyLive && !_bodyUnloadStarted && !_dependentInterval && _stage == 6 &&
            _instance != 0 && _constructing == 0 && _layout.IsBloomBody && _layout.IsDerivedBloom,
            "Retained body lifetime is outside its explicit initialized phase.");
        Require(U(0x755294) == _flow && U(0x765ACC) == _gameManager,
            "Retained body's flow or game-manager owner changed.");
        uint stack = U(_flow + 0x1AC);
        Require(stack <= 20, "Retained body's flow stack is outside its bounded extent.");
        uint top = 0;
        bool retained = false;
        for (uint index = 0; index <= stack; ++index)
        {
            uint state = U(_flow + 0x15C + index * 4);
            if (state == _active) retained = true;
            if (index == stack) top = state;
        }
        Require(retained && top >= 0x10000,
            "Captured playing state is absent beneath the flow overlay or its top is invalid.");
        Table(_active, 0x710874);
        Require(U(_active + 0x10) == _level && _level == 28,
            "Retained body's captured playing state changed type or level.");
        uint currentLevel = U(_flow + 0x1B0), pending = U(_flow + 0x1B8), deviceContext = U(_device + 0x14);
        bool flowReady = top == _active && currentLevel == _level && pending == 0 && deviceContext == 3;
        if (!_bodyLifetimeCaptured)
        {
            Require(flowReady, "Body lifetime must capture before the first overlay or transition.");
            _bodyLifetimeAnimationManager = Pointer(0x765ADC);
            Table(_bodyLifetimeAnimationManager, 0x703470);
            _bodyLifetimeGameTimer = Pointer(0x755298);
            Table(_bodyLifetimeGameTimer, 0x702F94);
            Require(ClassId(_bodyLifetimeGameTimer) == 0x78665312,
                "Body lifetime GameTimer class identity is unqualified.");
            foreach (uint function in new uint[] { 0x5968F0, 0x596900, 0x596910 }) VerifyFunction(function);
            Require(Read(_bodyLifetimeGameTimer + 0x40, 1)[0] == 0,
                "Body lifetime must first capture the typed original GameTimer unpaused.");
            _bodyLastNativePaused = 0;
            _bodyLifetimeCaptured = true;
            _log(new { @event = "template-body-lifetime-captured", playingState = Hex(_active),
                flow = Hex(_flow), level = _level, animationManager = Hex(_bodyLifetimeAnimationManager),
                gameTimer = Hex(_bodyLifetimeGameTimer), timerTable = Hex(0x702F94), timerClassId = Hex(0x78665312),
                nativePaused = 0, deviceContext, nativeTimerCalls = 0, nativeTimerWrites = 0 });
        }
        Require(U(0x765ADC) == _bodyLifetimeAnimationManager && U(0x755298) == _bodyLifetimeGameTimer,
            "Retained body's borrowed animation manager or GameTimer owner changed.");
        Table(_bodyLifetimeAnimationManager, 0x703470); Table(_bodyLifetimeGameTimer, 0x702F94);
        byte nativePaused = Read(_bodyLifetimeGameTimer + 0x40, 1)[0];
        Require(nativePaused is 0 or 1, "Original GameTimer pause byte left its admitted0/1 domain.");
        bool ready = flowReady && nativePaused == 0;
        long now = Environment.TickCount64;
        if (!ready) VerifyBodyNeutralForLifetime();
        if (nativePaused == 1)
        {
            _bodyPauseObserved = true;
            if (_bodyNativePauseStarted == 0) _bodyNativePauseStarted = now;
            _bodyLongestCurrentNativePauseMilliseconds = Math.Max(_bodyLongestCurrentNativePauseMilliseconds,
                Math.Max(0, now - _bodyNativePauseStarted));
        }
        else if (_bodyNativePauseStarted != 0)
        {
            _bodyLongestCurrentNativePauseMilliseconds = Math.Max(_bodyLongestCurrentNativePauseMilliseconds,
                Math.Max(0, now - _bodyNativePauseStarted));
            _bodyNativePauseStarted = 0;
        }
        if (nativePaused != _bodyLastNativePaused)
        {
            Require(++_bodyNativePauseTransitions <= 64, "GameTimer pause transition diagnostic cap exceeded.");
            _log(new { @event = "template-body-native-game-timer-pause-transition", gameTimer = Hex(_bodyLifetimeGameTimer),
                previousNativePaused = _bodyLastNativePaused, nativePaused, transition = _bodyNativePauseTransitions,
                timerActive = Read(_bodyLifetimeGameTimer + 0x18, 1)[0], timerCurrentRaw = Hex(U(_bodyLifetimeGameTimer + 0x1C)),
                timerDeltaRaw = Hex(U(_bodyLifetimeGameTimer + 0x28)), deviceContext,
                capturedPlayingState = Hex(_active), top = Hex(top), topTable = Hex(U(top)), currentLevel, pending,
                longestCurrentNativePauseMilliseconds = _bodyLongestCurrentNativePauseMilliseconds,
                ready, nativeTimerCalls = 0, nativeTimerWrites = 0 });
            _bodyLastNativePaused = nativePaused;
        }
        if (!ready)
        {
            if (!_bodyLifetimePaused)
            {
                _bodyLifetimePaused = true;
                _bodyPauseStarted = now; ++_bodyPauseTransitions;
                _log(new { @event = "template-body-overlay-retained", flow = Hex(_flow), stack,
                    capturedPlayingState = Hex(_active), top = Hex(top), topTable = Hex(U(top)),
                    topId = U(top + 0x10), currentLevel, pending, deviceContext,
                    gameTimer = Hex(_bodyLifetimeGameTimer), nativePaused,
                    timerPauseRequiredForAcceptance = true, topDifferenceNotRequired = true,
                    finiteCommandPhaseAlreadyEnded = true, nativeIntentWrites = 0 });
            }
        }
        else if (_bodyLifetimePaused)
        {
            VerifyBodyNeutralForLifetime();
            _bodyPauseMilliseconds += Math.Max(0, now - _bodyPauseStarted);
            _bodyLongestResumedPauseMilliseconds = Math.Max(_bodyLongestResumedPauseMilliseconds,
                _bodyLongestCurrentNativePauseMilliseconds);
            _bodyNativePauseStarted = 0; _bodyLongestCurrentNativePauseMilliseconds = 0;
            _bodyLifetimePaused = false; ++_bodyResumeTransitions;
            _log(new { @event = "template-body-overlay-resumed", playingState = Hex(_active),
                completedPauseMilliseconds = _bodyPauseMilliseconds,
                longestResumedNativePauseMilliseconds = _bodyLongestResumedPauseMilliseconds,
                gameTimer = Hex(_bodyLifetimeGameTimer), nativePaused, deviceContext, currentLevel, pending,
                nativeIntentWrites = 0, heldInputReplayed = false });
        }
        return ready;
    }

    private void VerifyBodyNeutralForLifetime()
    {
        Require(_commandMovement is null || (!_commandMovement.Active && _commandMovement.Finished && _commandMovement.Passed),
            "Pause or unload cannot interrupt the finite private command experiment.");
        Table(_ownCommand, 0x703090);
        byte[] command = Read(_ownCommand, 0x74);
        Require(new[] { 4, 8, 12, 16, 20 }.All(offset => float.IsFinite(BitConverter.ToSingle(command, offset))) &&
            BitConverter.ToSingle(command, 4) == 0 && command[0x1A] == 0,
            "Retained body requires finite neutral private movement/jump intent before pause or unload.");
    }

    /// <summary>
    /// The observer owns entry signature/receiver checks and deferred rearm.
    /// This method independently rechecks the exact entry and native owners.
    /// </summary>
    internal bool TryBeginUnloadTemplate(DebuggerHost host, uint tid, ref Win32Native.X86Context ctx,
        uint entry, byte firstByte, NativeCallDispatcher calls)
    {
        if (!_bodyLive || !HasLiveOwnership) return false;
        Require(ReferenceEquals(host, _host) && host.IsAtDebugStop && tid == host.MainThreadId &&
            ctx.Eip == entry && (entry is 0x5F86C0 or 0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190),
            "Template unload cleanup requires a qualified owned native entry stop.");
        Require(!_bodyUnloadStarted && _bodyLifetimeCaptured && !_dependentInterval && _stage == 6 && _idleActive &&
            _bloomIntervalVerified && _bloomBodyGraphVerified && _nearPositioned && _bodyLiveHolding &&
            HairModeSucceeded && !HairHasLiveOwnership && _commandMovement is null &&
            _instance != 0 && _constructing == 0 && _template != 0 && _stream != 0 && !calls.Pending,
            "Template unload cleanup cannot nest or interrupt a partial native lifecycle.");
        VerifyFunction(entry);
        Require(Read(entry, 1)[0] == firstByte, "Template unload entry byte mismatch.");
        Require(U(0x765AD4) == _profile && U(0x765ADC) == _bodyLifetimeAnimationManager &&
            U(0x755294) == _flow && U(0x765ACC) == _gameManager && U(0x75DC64) == _templateManager &&
            U(0x755298) == _bodyLifetimeGameTimer,
            "Template unload changed the captured native owners before cleanup.");
        Table(_profile, 0x6F5A44); Table(_bodyLifetimeAnimationManager, 0x703470);
        Table(_bodyLifetimeGameTimer, 0x702F94);
        Require(U(_profile + 0x2B4) == _record && U(_record) == 0x710F80 &&
            U(_record + 0x24) == _localRoot && U(_record + 0x20) == 11 &&
            U(_localRoot + 0x2C) == _parent && U(_localRoot + 0x3C) == _scene &&
            U(0x7552A0) == _input && U(_input + 0x14) == _inputReceiver &&
            U(_input + 0x18) == _command && U(_input + 0x1C) == _device &&
            U(0x755274) == _engine && U(_engine + 0x1C) == _camera &&
            LocalComponentKeys() == _localComponentKeys && U(_localAnimator + 0x128) == _localActor,
            "Template pre-unload local profile/input/camera/component identities changed.");
        if (entry == 0x5F86C0)
        {
            uint stack = U(_flow + 0x1AC);
            Require(stack <= 20 && ctx.Ecx == _active && U(_active) == 0x710874 &&
                U(_flow + 0x15C + stack * 4) == _active,
                "Playing-level unload receiver is not the exact captured active state.");
        }
        else if (entry is 0x4E8AA0 or 0x4E8C80)
            Require(ctx.Ecx == _profile, "Body/profile unload receiver is not the captured profile.");
        else Require(ctx.Ecx == _bodyLifetimeAnimationManager,
            "Animation unload receiver is not the captured borrowed animation manager.");
        VerifyBodyNeutralForLifetime();
        Table(_instance, 0x710F80); Table(_template, 0x703D24); Table(_stream, 0x6E7E50);
        VerifyBodyTemplateOwner(forceManagerObservation: true);
        Require(Refs(_instance) == _instanceRefs && _templateRefs == 1 &&
            U(_template + 4) == 0 && Refs(_stream) == _streamRefs &&
            U(_instance + 0x14) == _template && U(_instance + 0x24) == _ownRoot && U(_instance + 0x20) == 6,
            "Template pre-unload ownership differs from its qualified native deletion baseline.");
        Table(_ownRoot, VisualGraphReader.NodeVtable);
        Require(U(_ownRoot + 0x2C) == _parent && U(_ownRoot + 0x3C) == _scene && Refs(_ownRoot) == 7,
            "Template pre-unload root has unexpected scene links or owners.");
        List<Component> components = Components(_instance, 6);
        Require(_ownEntities.Length == 5 &&
            _ownEntities.All(entity => components.Count(component => component.Address == entity &&
                component.Owner == _instance && component.BoundNode == _ownRoot) == 1),
            "Template pre-unload component owners changed.");
        Table(_character, 0x6F6438); Table(_move, 0x6F64F8); Table(_machine, 0x6FA288);
        Table(_animator, 0x6F670C); Table(_ownActor, 0x703F80);
        Require(U(_character + 0x124) == _machine && U(_character + 0x128) == _animator &&
            U(_character + 0x12C) == _move && U(_character + 0x130) == _ownCommand &&
            U(_character + 0x138) == _traits && U(_character + 0x148) == 1 && U(_character + 0x14C) == 41 &&
            U(_character + 0x134) == 0 && U(_character + 0x13C) == 0 &&
            U(_move + 0x160) == _character && U(_machine + 0x124) == _character &&
            U(_machine + 0x128) == 0 && _observedBloomKind == 0 && U(_machine + 0x12C) == _ownCommand &&
            U(_animator + 0x124) == _character && U(_animator + 0x128) == _ownActor &&
            _ownCommand != _command && _ownActor != _localActor,
            "Template pre-unload private component/input/Actor bindings changed.");
        Membership before = Memberships(_ownEntities);
        Require(LiveOwnMembership(before), "Template pre-unload instance lost its qualified live manager membership.");
        foreach (uint function in new uint[] { 0x5FB350, 0x5A05B0, 0x465730 }) VerifyFunction(function);

        ReleaseBodyTemplateOwnerForDeletion();

        uint deletedInstance = _instance, deletedTemplate = _template, deletedStream = _stream;
        uint deletedRoot = _ownRoot, deletedActor = _ownActor;
        _bodyDeletedEntities = (uint[])_ownEntities.Clone();
        _bodyUnloadEntry = entry; _bodyUnloadStarted = true; _idleActive = false; _stage = 7;
        if (_bodyLifetimePaused)
        {
            _bodyPauseMilliseconds += Math.Max(0, Environment.TickCount64 - _bodyPauseStarted);
            _bodyLifetimePaused = false;
        }
        // No callback occurs between the batch calls. Disable address-based own
        // observation before deletion, while stream/template still keep the
        // transaction visible to HasLiveOwnership and nested-unload guards.
        _instance = 0; _ownRoot = 0; _ownCommand = 0; _ownActor = 0;
        _character = 0; _move = 0; _machine = 0; _animator = 0; _traits = 0;
        _bodyUnloadWitness = new { entry = Hex(entry), receiver = Hex(ctx.Ecx),
            instance = Hex(deletedInstance), template = Hex(deletedTemplate), stream = Hex(deletedStream),
            root = Hex(deletedRoot), actor = Hex(deletedActor), before,
            originalEsp = Hex(ctx.Esp), originalReturn = Hex(U(ctx.Esp)),
            observationInactiveBeforeDelete = true };
        _log(new { @event = "template-body-pre-unload-begin", witness = _bodyUnloadWitness });
        NativeCallSpec[] batch =
        [
            new(0x5FB350, deletedInstance, [1], false),
            new(0x5A05B0, deletedTemplate, [1], false),
            new(0x465730, deletedStream, [1], false)
        ];
        calls.BeginAtEntry(host, tid, ref ctx, entry, firstByte, batch, results =>
        {
            Require(results.Length == 3 && U(0x765ACC) == _gameManager,
                "Template pre-unload batch completion lost its result count or live manager owner.");
            _template = 0; _stream = 0; _buffer = 0;
            Require(!HasLiveOwnership && Memberships(_bodyDeletedEntities).Matches.Length == 0,
                "Template native pre-unload deletion retained an owner or manager membership.");
            _bodyUnloadBatchCompleted = true;
            _log(new { @event = "template-body-pre-unload-cleaned", entry = Hex(entry),
                deletedInstance = Hex(deletedInstance), deletedTemplate = Hex(deletedTemplate), deletedStream = Hex(deletedStream),
                freedBytesRead = false, originalEntryStillNotExecuted = true,
                originalEntryResumesWithSavedContext = true, nativeResults = results });
        });
        return true;
    }

    /// <summary>
    /// Invoke before Step's old local-player reads. menuObserved must come from
    /// the caller's fresh independently qualified menu observation. The action
    /// releases any appearance/other data owners after all native borrows ended.
    /// </summary>
    internal bool ObserveTemplatePostUnloadWorld(DebuggerHost host, uint tid, ref Win32Native.X86Context ctx,
        NativeCallDispatcher calls, bool menuObserved, Action releaseOwnedResources)
    {
        ArgumentNullException.ThrowIfNull(releaseOwnedResources);
        if (!_bodyLive || !_bodyUnloadStarted) return false;
        Require(ReferenceEquals(host, _host) && host.IsAtDebugStop && tid == host.MainThreadId &&
            ctx.Eip == NativeCallDispatcher.WorldReturn && !calls.Pending && _bodyUnloadBatchCompleted,
            "Post-unload template observation requires a later real world stop after batch completion.");
        if (_cleaned) return true;
        calls.ReleaseRetiredAtWorldStop(host, tid, ref ctx);
        Require(!HasLiveOwnership && U(0x765ACC) == _gameManager &&
            Memberships(_bodyDeletedEntities).Matches.Length == 0,
            "Deleted template reappeared in live game-manager membership after stock unload.");
        if (!_bodyUnloadResourcesReleased)
        {
            releaseOwnedResources();
            _bodyUnloadResourcesReleased = true;
            _log(new { @event = "template-body-post-unload-resources-released", nativeBorrowsEnded = true,
                atLaterWorldStop = true, oldLocalPlayerRead = false });
        }
        Require(++_bodyPostUnloadWorlds <= 3600, "Post-unload menu observation exceeded its bounded world limit.");
        if (menuObserved) ++_bodyMenuWorlds;
        else _bodyMenuWorlds = 0;
        if (_bodyPostUnloadWorlds <= 2 || menuObserved && _bodyMenuWorlds <= 2)
            _log(new { @event = "template-body-post-unload-world", world = _bodyPostUnloadWorlds,
                callerQualifiedMenuObserved = menuObserved, consecutiveMenuWorlds = _bodyMenuWorlds,
                nativeOwnersGone = true, oldLocalPlayerRead = false });
        if (_bodyMenuWorlds >= 2)
        {
            _cleaned = true; _stage = 10;
            _log(new { @event = "template-body-lifetime-cleaned", pauseObserved = _bodyPauseObserved,
                completedPauseMilliseconds = _bodyPauseMilliseconds, consecutiveMenuWorlds = _bodyMenuWorlds,
                nativeCleanupAndLaterWorldRelease = true, oldLocalPlayerRead = false,
                bodyLifetimePassed = BodyLifetimeSucceeded });
        }
        return true;
    }
}
