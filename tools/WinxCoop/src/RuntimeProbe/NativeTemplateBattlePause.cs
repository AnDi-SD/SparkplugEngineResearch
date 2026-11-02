using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Explicit one-shot E5 diagnostic pacing through the original Diary UI request.</summary>
internal sealed partial class NativeTemplateProbe
{
    private const uint DiagnosticBattlePauseEntry = 0x5953E0, DiagnosticBattlePauseRet8 = 0x595439;
    private const uint DiagnosticFlowTable = 0x702D2C, DiagnosticFlowGetter = 0x595DB0;
    private const uint DiagnosticFlowTypeRecord = 0x766088, DiagnosticFlowClass = 0x078B20E8;
    private const uint DiagnosticDiaryTable = 0x70ED30, DiagnosticDiaryGetter = 0x5D53C0;
    private const uint DiagnosticDiaryTypeRecord = 0x767FD8, DiagnosticDiaryClass = 0x0734C578;
    private bool _diagnosticBattlePauseConfigured, _diagnosticBattlePauseEnabled;
    private bool _diagnosticBattlePauseReturned, _diagnosticBattlePauseSettled;
    private uint _diagnosticBattlePauseCalls;
    private string _diagnosticBattlePauseFailure = "";
    private DiagnosticBattlePauseRequest? _diagnosticBattlePauseRequest;

    private sealed record DiagnosticBattlePauseRequest(uint Flow, uint PlayingState, uint GameTimer,
        uint Stack, uint OriginalWorldEsp, uint OriginalWorldCaller, uint ActualWorld,
        uint CompletedOrdinaryActions, uint SecondaryHealthCommits, uint OwnNativeDamageSends,
        long RequestedAtMilliseconds, byte[] NativeQueueBefore);

    internal bool DiagnosticBattlePauseQualified => _diagnosticBattlePauseEnabled &&
        _diagnosticBattlePauseCalls == 1 && _diagnosticBattlePauseReturned && _diagnosticBattlePauseSettled &&
        _diagnosticBattlePauseFailure.Length == 0;
    internal object DiagnosticBattlePauseSummary => new
    {
        enabled = _diagnosticBattlePauseEnabled, configured = _diagnosticBattlePauseConfigured,
        nativeCalls = _diagnosticBattlePauseCalls, maximumNativeCalls = 1,
        originalNativeRequestReturned = _diagnosticBattlePauseReturned,
        settledOriginalDiaryPauseObserved = _diagnosticBattlePauseSettled,
        qualified = DiagnosticBattlePauseQualified, failure = _diagnosticBattlePauseFailure,
        request = _diagnosticBattlePauseRequest,
        nativeRequest = "Flow5953E0(61,0)/originalRET8/queue[61,0,1]",
        diagnosticPacingOnly = true, multiplayerPauseProtocolImplemented = false,
        directFlowOrTimerWrites = 0, directCommandWrites = 0, nativeFsmOrHealthWrites = 0,
        queuedRequestAloneCountedAsPause = false, worldProjectileAckClaimedByThisFixture = false
    };

    internal void ConfigureDiagnosticBattlePause(ProbeConfiguration config)
    {
        Require(!_diagnosticBattlePauseConfigured, "Diagnostic Battle pause was configured twice.");
        _diagnosticBattlePauseConfigured = true; _diagnosticBattlePauseEnabled = config.DiagnosticAutoBattlePauseFixture;
        if (!_diagnosticBattlePauseEnabled) return;
        Require(_networkInputFixture && _ordinaryNetwork && _sharedBattleLifetimeEnabled && _healthBattleFixture &&
            _stingerSecondaryRoleEnabled && config.TemplateMosquitoPlayerTargets &&
            config.Mode == "template-bloom-body-hair-idle" && config.TemplateSharedPlayerProfile == "battle-01-network-input" &&
            config.DiagnosticHostInput?.Role == DiagnosticInputRole.Host && config.DiagnosticGuestInput is null &&
            config.DiagnosticWorldState?.Role == WorldStateRole.Host && !config.IsolatedMenuInputDiagnostic,
            "Diagnostic Battle pause is restricted to its explicitly selected two-game native targeting Host cohort.");
    }

    internal bool TryBeginDiagnosticBattlePauseAtWorld(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume)
    {
        if (!_diagnosticBattlePauseEnabled || _diagnosticBattlePauseSettled) return false;
        try
        {
            SharedBattleWorld(tid, ref context);
            Require(!calls.Pending && _sharedBattleCaptured && !_sharedBattleUnloadStarted && _stage == 6 &&
                _idleActive && !_dependentInterval && _healthBattleHolding && _healthBattleFastRouteStarted,
                "Diagnostic Diary request escaped the live idle retained Battle World.");
            VerifyDiagnosticBattlePauseOwners();
            bool ready = ReadSharedBattleFlow(out uint top, out uint stack, out uint current, out uint pending, out uint device);
            if (_diagnosticBattlePauseRequest is not null)
            {
                Require(_diagnosticBattlePauseCalls == 1 && _diagnosticBattlePauseReturned,
                    "Diagnostic Diary request reached another World without its paired native completion.");
                Require(_sharedHealthAlias?.Active == true && _sharedHealthAlias.CommittedDamageTransactions <= 4,
                    "Diagnostic Diary transition exceeded the unchanged four-hit fixture budget before its settled pause.");
                if (current == 61 && pending == 0 && stack is >= 2 and <= 20 &&
                    Read(_sharedBattleTimer + 0x40, 1)[0] == 1)
                {
                    Require(U(top) == DiagnosticDiaryTable && U(top + 0x10) == 61 &&
                        U(DiagnosticDiaryTable + 0x10) == DiagnosticDiaryGetter &&
                        U(DiagnosticDiaryTypeRecord) == DiagnosticDiaryClass,
                        "Settled diagnostic pause is not the original registered Diary61 state.");
                    PinDiagnosticBattlePause(DiagnosticDiaryGetter, "B8D87F7600C3");
                    _diagnosticBattlePauseSettled = true;
                    _log(new { @event = "diagnostic-battle-auto-pause-settled", flow = Hex(_flow), playing = Hex(_active),
                        diary = Hex(top), diaryTable = Hex(DiagnosticDiaryTable), current, pending, stack,
                        nativePaused = 1, deviceContext = device, actualWorld = _networkInputWorlds,
                        nativeRequestCalls = _diagnosticBattlePauseCalls, originalBattleSuspendAndTimerPause = true,
                        queuedRequestAloneWasNotPauseProof = true, directFlowTimerCommandFsmHealthWrites = 0 });
                }
                else Require(Environment.TickCount64 - _diagnosticBattlePauseRequest.RequestedAtMilliseconds <= 10000,
                    "Original diagnostic Diary request did not settle within ten seconds; no retry is admitted.");
                return false;
            }
            if (!ready || !NetworkInputFixtureGoalPassed || !NetworkOrdinaryFixtureGoalPassed ||
                _networkOrdinaryCompleted.Count < 2 || _networkOrdinaryAction is not { Completed: true } ||
                _ordinaryAuthorization || _networkOrdinarySend is not null || _networkOrdinaryDamage is not null ||
                _networkOrdinaryAction.ChooserPending || _networkOrdinaryAction.LookupPending ||
                _networkOrdinaryAction.EventPending || _networkOrdinaryAction.ActivationPending ||
                _networkOrdinaryHurtAwaitNeutral || _sharedHealthAlias?.Active != true ||
                _sharedHealthAlias.SecondaryCommittedDamageTransactions < 1 ||
                _stingerSecondaryRole?.Active != true || _stingerSecondaryRole.OwnNativeDamageSendCount < 1 ||
                _mosquitoPlayerTargets?.Active != true) return false;
            Require(_diagnosticBattlePauseCalls == 0 && _sharedHealthAlias.CommittedDamageTransactions <= 4 &&
                Read(_active + 0x17D, 1)[0] == 0,
                "Diagnostic Diary request repeated or escaped its existing damage/menu-inhibition budget.");
            PinDiagnosticBattlePauseSources();
            byte[] queue = Read(_flow + 0x1B8, 120);
            Require(Enumerable.Range(0, 10).All(index => BitConverter.ToUInt32(queue, index * 12) == 0),
                "Diagnostic Diary request requires the original empty ten-record Flow queue.");
            var request = new DiagnosticBattlePauseRequest(_flow, _active, _sharedBattleTimer, stack,
                context.Esp, U(context.Esp), _networkInputWorlds, checked((uint)_networkOrdinaryCompleted.Count),
                _sharedHealthAlias.SecondaryCommittedDamageTransactions, _stingerSecondaryRole.OwnNativeDamageSendCount,
                Environment.TickCount64, queue);
            _diagnosticBattlePauseRequest = request; ++_diagnosticBattlePauseCalls;
            _log(new { @event = "diagnostic-battle-auto-pause-request", request,
                function = Hex(DiagnosticBattlePauseEntry), receiver = Hex(_flow), arguments = new uint[] { 61, 0 },
                originalEscapeCallSite = "0x005F7777..0x005F7786", originalNativeRet = Hex(DiagnosticBattlePauseRet8),
                completedNativeOrdinaryActions = _networkOrdinaryCompleted.Count,
                actualSecondaryHpCommits = request.SecondaryHealthCommits, actualOwnNativeDamageSends = request.OwnNativeDamageSends,
                worldProjectileAckClaimed = false, inputOrFsmOrHealthWrites = 0 });
            calls.Begin(_host, tid, ref context, DiagnosticBattlePauseEntry, _flow, [61, 0], false, _ =>
            {
                try
                {
                    Require(ReferenceEquals(_diagnosticBattlePauseRequest, request) && _diagnosticBattlePauseCalls == 1 &&
                        !_diagnosticBattlePauseReturned && !calls.Pending,
                        "Diagnostic Diary native return lost its one exact owned request.");
                    VerifyDiagnosticBattlePauseOwners();
                    byte[] expected = (byte[])request.NativeQueueBefore.Clone();
                    BitConverter.GetBytes(61u).CopyTo(expected, 0);
                    BitConverter.GetBytes(0u).CopyTo(expected, 4);
                    BitConverter.GetBytes(1u).CopyTo(expected, 8);
                    Require(Read(_flow + 0x1B8, 120).SequenceEqual(expected) && U(_flow + 0x1AC) == request.Stack &&
                        U(_flow + 0x1B0) == 47 && U(_flow + 0x15C + request.Stack * 4) == request.PlayingState &&
                        Read(_sharedBattleTimer + 0x40, 1)[0] == 0,
                        "Original Diary API did not queue exactly [61,0,1] before original World resume.");
                    _diagnosticBattlePauseReturned = true;
                    _log(new { @event = "diagnostic-battle-auto-pause-native-return", flow = Hex(_flow),
                        queuedState = 61, queuedOperation = 0, queuedParameter = 1,
                        exactNativeRet8StackBalancedByDispatcher = true, originalWorldNotYetResumed = true,
                        actualDiaryPauseNotYetClaimed = true, nativeRequestCalls = 1, directFlowTimerCommandFsmHealthWrites = 0 });
                    resume();
                }
                catch (Exception error) { _diagnosticBattlePauseFailure = error.ToString(); throw; }
            });
            return true;
        }
        catch (Exception error) { _diagnosticBattlePauseFailure = error.ToString(); throw; }
    }

    private void VerifyDiagnosticBattlePauseOwners()
    {
        Require(_host.IsAtDebugStop && _host.ProcessHandle == _process && _host.ProcessPid == _pid &&
            Environment.CurrentManagedThreadId == _ownerThread && U(0x755294) == _flow && U(_flow) == DiagnosticFlowTable &&
            U(_flow + 0x10) == 0x702D28 && U(DiagnosticFlowTable + 0x10) == DiagnosticFlowGetter &&
            U(DiagnosticFlowTypeRecord) == DiagnosticFlowClass && U(0x755298) == _sharedBattleTimer &&
            U(_sharedBattleTimer) == 0x702F94 && U(_active) == 0x7105C8 && U(_active + 0x10) == 47,
            "Diagnostic Diary request lost its captured original Flow/Battle/GameTimer owner graph.");
    }

    private void PinDiagnosticBattlePauseSources()
    {
        PinDiagnosticBattlePause(DiagnosticBattlePauseEntry,
            "56578B7C240C83FF338BF17D3EE9F498E7FF85C0750AE86520E7FFA3905275008A484B84C975138B486085C9750C6A006A04578BCEE866FAFFFF6A026A008BCE57E85AFAFFFF5F5EC208006A016A008BCE57E849FAFFFF5F5EC20800");
        PinDiagnosticBattlePause(0x5F7777, "536A3DE8B168E1FF8BC8E85ADCF9FF");
        PinDiagnosticBattlePause(0x6D62F0, "6A0068D05D5900681053750068742F700068A152534168E8208B07B988607600E8DBCCD3FFC3");
        PinDiagnosticBattlePause(0x595BB8, "C7062C2D7000C74610282D7000");
        PinDiagnosticBattlePause(DiagnosticFlowGetter, "B888607600C3");
        PinDiagnosticBattlePause(DiagnosticBattlePauseRet8, "C20800");
        PinDiagnosticBattlePause(DiagnosticDiaryGetter, "B8D87F7600C3");
    }

    private void PinDiagnosticBattlePause(uint address, string expectedHex)
    {
        byte[] expected = Convert.FromHexString(expectedHex);
        Require(_image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, expected.Length).ToArray()
            .SequenceEqual(expected) && Read(address, expected.Length).SequenceEqual(expected),
            $"Original diagnostic Diary source pin differs at0x{address:X8}.");
    }
}
