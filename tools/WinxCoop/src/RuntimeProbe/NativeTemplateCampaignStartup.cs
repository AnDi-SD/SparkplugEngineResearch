using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// Initial Host Gardenia1 only. Wait BEFORE the template baseline/allocation for
// the original scripted Cinematic push/pop and a later original Playing Update.
// No movie, flow request, input, readiness or native return is manufactured.
internal sealed partial class NativeTemplateProbe
{
    private readonly bool _campaignInitialStartup;
    private bool _campaignStartupWaitingLogged, _campaignStartupAdmitted, _campaignStartupCallPending;
    private bool _campaignCinematicEnterReturned, _campaignCinematicExitReturned, _campaignPlayingUpdateReturned;
    private uint _campaignStartupFlow, _campaignStartupPlaying, _campaignStartupCinematic, _campaignStartupManager;
    private uint _campaignStartupPlayingDepth, _campaignStartupCycles;

    private void InstallCampaignStartupWitnesses()
    {
        if (!_campaignInitialStartup) return;
        // These are original thiscall RET4 methods, not adapter calls. The
        // native FlowManager caller owns the one argument and the receiver.
        Hook(0x59E410, "CampaignStartup.Cinematic.Enter", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (_campaignStartupAdmitted) return;
            uint flow = Pointer(0x755294); Table(flow, 0x702D2C);
            uint depth = U(flow + 0x1AC);
            Require(depth <= 20 && U(flow + 0x1B0) == 74 &&
                U(flow + 0x15C + depth * 4) == ctx.Ecx,
                "Campaign startup Enter is not the actual top Cinematic state.");
            // Original intro/menu cinematics may have no Playing underlay.
            // They do not belong to this scene-specific construction witness.
            // Once Playing1 has been watched, all previous hard guards apply.
            if (_campaignStartupFlow == 0)
            {
                if (depth == 0) return;
                uint underlay = U(flow + 0x15C + (depth - 1) * 4);
                if (underlay < 0x10000 || U(underlay) != 0x710874 || U(underlay + 0x10) != 1) return;
            }
            Require(!_campaignStartupCallPending && _stage == 0 && _stream == 0 && _instance == 0 &&
                depth >= 1 && ++_campaignStartupCycles <= 8,
                "Campaign startup Cinematic overlaps template construction or exceeds its bound.");
            uint playing = Pointer(flow + 0x15C + (depth - 1) * 4);
            Table(playing, 0x710874);
            Require(U(playing + 0x10) == 1, "Campaign startup Cinematic has no retained Gardenia1 Playing owner.");
            uint manager = Pointer(0x765AE4); Table(manager, 0x6F5B0C);
            Require(U(manager + 0x20) == ctx.Ecx && U(flow + 0x140) == ctx.Ecx,
                "Campaign startup Cinematic receiver differs from the original manager and state registry.");
            if (_campaignStartupFlow != 0)
                Require(flow == _campaignStartupFlow && playing == _campaignStartupPlaying &&
                    ctx.Ecx == _campaignStartupCinematic && manager == _campaignStartupManager &&
                    depth - 1 == _campaignStartupPlayingDepth, "Campaign startup native owner identities changed.");
            _campaignStartupFlow = flow; _campaignStartupPlaying = playing;
            _campaignStartupCinematic = ctx.Ecx; _campaignStartupManager = manager;
            _campaignStartupPlayingDepth = depth - 1;
            _campaignCinematicEnterReturned = _campaignCinematicExitReturned = _campaignPlayingUpdateReturned = false;
            ObserveCampaignStartupReturn(tid, ctx, "Cinematic.Enter", 0x596542, 0x703CA4, 74, () =>
            {
                Require(Read(_campaignStartupCinematic + 0x84, 1)[0] == 1,
                    "Original Cinematic Enter did not complete its native active flag.");
                _campaignCinematicEnterReturned = true;
                _log(new { @event = "campaign-startup-cinematic-enter-returned", cycle = _campaignStartupCycles,
                    originalShowCinematics = Read(_campaignStartupManager + 0x2C, 1)[0] != 0,
                    actualOriginalRet4 = true, nativeCallsAdded = 0, nativeReadinessClaimed = false });
            });
        });
        Hook(0x59E450, "CampaignStartup.Cinematic.Exit", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (_campaignStartupAdmitted || _campaignStartupFlow == 0) return;
            Require(_campaignCinematicEnterReturned && !_campaignCinematicExitReturned && ctx.Ecx == _campaignStartupCinematic,
                "Campaign startup Exit has no exact preceding original Cinematic Enter.");
            ObserveCampaignStartupReturn(tid, ctx, "Cinematic.Exit", 0x596574, 0x703CA4, 74, () =>
            {
                Require(Read(_campaignStartupCinematic + 0x84, 1)[0] == 0 &&
                    Read(_campaignStartupCinematic + 0xE0, 1)[0] == 0,
                    "Original Cinematic Exit did not complete its native flags.");
                _campaignCinematicExitReturned = true;
                _log(new { @event = "campaign-startup-cinematic-exit-returned", cycle = _campaignStartupCycles,
                    actualOriginalRet4 = true, nativeCallsAdded = 0, nativeReadinessClaimed = false });
            });
        });
        Hook(0x5F76D0, "CampaignStartup.Playing.Update", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (_campaignStartupAdmitted || !_campaignCinematicExitReturned || _campaignPlayingUpdateReturned) return;
            // The native FlowManager can update an underlay with active=0.
            // Observation alone never admits that frame or interrupts the cycle.
            if (U(ctx.Esp + 4) != 1 || !CampaignStartupPlayingSettled()) return;
            Require(ctx.Ecx == _campaignStartupPlaying,
                "Campaign startup active update changed the retained Gardenia1 Playing owner.");
            ObserveCampaignStartupReturn(tid, ctx, "Playing.Update", 0x596819, 0x710874, 1, () =>
            {
                _campaignPlayingUpdateReturned = true;
                _log(new { @event = "campaign-startup-playing-update-returned", cycle = _campaignStartupCycles,
                    afterOriginalCinematicExit = true, actualOriginalRet4 = true,
                    nativeCallsAdded = 0, nativeReadinessClaimed = false });
            });
        });
    }

    private void ObserveCampaignStartupReturn(uint tid, Win32Native.X86Context entry, string name,
        uint caller, uint table, uint id, Action completed)
    {
        Require(!_campaignStartupCallPending && _stage == 0 && _stream == 0 && _instance == 0 &&
            tid == _host.MainThreadId && U(entry.Esp) == caller,
            "Campaign startup witness is outside its original caller/frame or precedes unsafe template ownership.");
        uint receiver = entry.Ecx, esp = entry.Esp; Table(receiver, table);
        Require(U(receiver + 0x10) == id, "Campaign startup native state type/ID differs.");
        VerifyFunction(caller);
        _campaignStartupCallPending = true;
        _host.Breakpoints.AddReturn(caller, tid, "CampaignStartup." + name + ".returned",
            (uint rt, uint ra, ref Win32Native.X86Context rc) =>
            {
                Require(_campaignStartupCallPending && rt == tid && ra == caller && rc.Eip == caller &&
                    rc.Esp == checked(esp + 8) && U(0x755294) == _campaignStartupFlow &&
                    U(0x765AE4) == _campaignStartupManager && U(_campaignStartupFlow + 0x140) == _campaignStartupCinematic,
                    "Campaign startup original RET4 did not preserve its admitted owner/frame.");
                Table(receiver, table); Require(U(receiver + 0x10) == id, "Campaign startup native state changed before its original return.");
                _campaignStartupCallPending = false; completed();
            });
    }

    private bool CampaignStartupPlayingSettled()
    {
        Require(U(0x755294) == _campaignStartupFlow && U(0x765AE4) == _campaignStartupManager,
            "Campaign startup native flow/manager owner changed.");
        return U(_campaignStartupFlow + 0x1B0) == 1 && U(_campaignStartupFlow + 0x1B8) == 0 &&
            U(_campaignStartupFlow + 0x1AC) == _campaignStartupPlayingDepth &&
            U(_campaignStartupFlow + 0x15C + _campaignStartupPlayingDepth * 4) == _campaignStartupPlaying;
    }

    private bool AdmitCampaignStartupAtWorld()
    {
        if (_campaignStartupAdmitted) return true;
        Require(_stage == 0 && _stream == 0 && _instance == 0 && !_dependentInterval,
            "Campaign startup admission must precede all template baseline/allocation.");
        if (!_campaignStartupWaitingLogged)
        {
            _campaignStartupWaitingLogged = true;
            _log(new { @event = "campaign-startup-awaiting-original-cinematic-cycle", requiredLevel = 1,
                nativeCallsAdded = 0, nativeReadinessClaimed = false, templateBaselineCaptured = false });
        }
        if (!_campaignCinematicEnterReturned || !_campaignCinematicExitReturned ||
            !_campaignPlayingUpdateReturned || _campaignStartupCallPending || !CampaignStartupPlayingSettled()) return false;
        // A next scripted cue may be queued in the manager before it reaches
        // flow.pending. Let that original cycle run before baseline capture.
        if (Read(_campaignStartupManager + 0x1C, 1)[0] != 0 ||
            Read(_campaignStartupManager + 0x24, 1)[0] != 0) return false;
        Table(_campaignStartupPlaying, 0x710874);
        Require(U(_campaignStartupPlaying + 0x10) == 1, "Campaign startup resumed another Playing owner.");
        uint input = Pointer(0x7552A0); Table(input, 0x6F5508);
        uint device = Pointer(input + 0x1C); Table(device, 0x7116CC);
        if (U(device + 0x14) != 3 || Read(Pointer(0x755298) + 0x40, 1)[0] != 0) return false;
        _campaignStartupAdmitted = true;
        _log(new { @event = "campaign-startup-template-admitted", cycle = _campaignStartupCycles,
            actualOriginalCinematicEnterAndExit = true, actualLaterPlayingUpdateReturn = true,
            exactPlayingPendingZeroAtWorld = true, nativeCallsAdded = 0, nativeReadinessClaimed = false });
        return true; // CaptureLocal and the unchanged strict construction guard run next.
    }
}
