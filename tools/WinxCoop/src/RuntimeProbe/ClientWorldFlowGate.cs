using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Our finite Guest Battle47 flow policy. Original PlayingLevel update retains
/// input/pause/presentation; original Challenge/Battle progression stays Host-only.
/// The existing ClientNativeGate owns publication and restoration of every patch.
/// </summary>
internal sealed class ClientWorldFlowGate(DebuggerHost host, PEReader image, Action<object> log)
{
    internal const uint BattleTable = 0x7105C8, BattleUpdate = 0x5F3230, PlayingUpdate = 0x5F76D0;
    private const uint BattleExit = 0x5F2300, WaveCall = 0x5F324B, WaveTick = 0x5F2D20;
    private const string BattleUpdateHash = "04346031E0586739DC440EA913504B31FC384214BC499B23FF621CE8019946BE";
    private const string PlayingUpdateHash = "BED3EC576F3A96F51D26ECB5342A1FC74099D51CC173A78435B69FCD7E4A0C95";
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;
    private ClientNativeGate? _gate;
    private uint _flow, _playing, _scene, _worlds, _phase = uint.MaxValue, _battleState = uint.MaxValue;
    private uint _waveAtForward, _finishedAtForward, _enemyCountAtForward;
    private bool _configured, _forwarded, _retired;

    internal bool Forwarded => _forwarded;
    internal object Summary => new
    {
        configured = _configured, forwardedToOriginalPlayingUpdate = _forwarded, retired = _retired,
        worlds = _worlds, challengePhase1BC = _phase, battleState188 = _battleState,
        waveAtForward = _waveAtForward, finishedWavesAtForward = _finishedAtForward,
        localEnemyCountAtForward = _enemyCountAtForward,
        originalInitialCountdownPreserved = true, firstActiveFrameWaveCallWithheld = true,
        nativePointerSlot = "0x007105F8", originalUpdate = "0x005F3230", presentationUpdate = "0x005F76D0",
        patchOwner = "ClientNativeGate", gamePhaseOrTimerWrites = 0, profileOrRewardWrites = 0,
        newNativeCodeOrBreakpoints = 0, hostStateCopied = false,
        fullGameFlowAuthorityClaimed = false, runtimeQualificationClaimed = false
    };

    internal void Configure(ClientNativeGate gate)
    {
        EnsureStop(host.MainThreadId);
        Require(!_configured && !_retired && !gate.Enabled, "Guest flow policy must configure before native gate publication.");
        ValidateOriginalFunction(BattleUpdate, 0x27, BattleUpdateHash);
        ValidateOriginalFunction(PlayingUpdate, 0x1A8, PlayingUpdateHash);
        Require(ImageWord(BattleTable + 0x30) == BattleUpdate && U(BattleTable + 0x30) == BattleUpdate &&
            ImageWord(0x710874 + 0x30) == PlayingUpdate,
            "Guest Battle/Playing update slots are not the pinned original bool RET4 pair.");
        // ChallengeBase may finish its countdown and activate Battle in this
        // same frame. Withhold the following Battle wave call even before the
        // next WorldRET can publish the narrower presentation-only slot.
        gate.AddSkipCall("world-battle-wave-tick", WaveCall, Convert.FromHexString("E8D0FAFFFF"),
            WaveTick, WaveCall + 5, 0);
        _gate = gate; _configured = true;
        log(new { @event = "client-world-flow-configured", countdownNative = true,
            waveCall = Hex(WaveCall), waveCallNext = Hex(WaveCall + 5), callArgumentBytes = 0,
            flowFieldWrites = 0, patchOwner = "ClientNativeGate" });
    }

    internal void ObserveAtWorld(uint thread, ref Win32Native.X86Context context,
        uint capturedPlaying, uint capturedScene, bool quarantineActive)
    {
        if (_retired || !_configured || !quarantineActive) return;
        EnsureStop(thread);
        Require(context.Eip == NativeCallDispatcher.WorldReturn && _gate is { Enabled: true },
            "Guest flow observation requires the owned quarantined WorldRET.");
        if (_playing == 0)
        {
            _flow = P(0x755294); _playing = capturedPlaying; _scene = capturedScene;
        }
        Require(capturedPlaying == _playing && capturedScene == _scene && U(0x755294) == _flow &&
            U(_playing) == BattleTable && U(_playing + 0x10) == 47 && U(_scene) == 0x6E7358,
            "Guest Battle flow changed its captured original scene/receiver.");
        uint depth = U(_flow + 0x1AC); Require(depth <= 20, "Guest Battle retained-flow bound exceeded.");
        uint retained = 0;
        for (uint index = 0; index <= depth; ++index)
            if (U(_flow + 0x15C + index * 4) == _playing) ++retained;
        Require(retained == 1, "Guest Battle flow is not retained exactly once below its native overlay.");
        Require(++_worlds <= NativeObservationBudget.WorldLimit, "Guest Battle flow observation budget exceeded.");
        uint phase = U(_playing + 0x1BC), state = U(_playing + 0x188);
        if (phase != _phase || state != _battleState || _worlds <= 2 || _worlds % 1000 == 0)
            log(new { @event = "client-world-flow-observed", worlds = _worlds,
                challengePhase1BC = phase, battleState188 = state, current = U(_flow + 0x1B0),
                pending = U(_flow + 0x1B8), depth, wave218 = U(_playing + 0x218),
                finished224 = U(_playing + 0x224), nativeEnemyCount220 = U(_playing + 0x220),
                nativeSceneRetained = true, forwarded = _forwarded, phaseOrTimerWrites = 0 });
        _phase = phase; _battleState = state;
        if (!_forwarded)
        {
            // No synthetic countdown completion or permission write: preserve
            // the original countdown path until the original active tuple.
            if (phase != 2 || state != 1 || U(_flow + 0x1B0) != 47 || U(_flow + 0x1B8) != 0) return;
            _waveAtForward = U(_playing + 0x218); _finishedAtForward = U(_playing + 0x224);
            _enemyCountAtForward = U(_playing + 0x220);
            _gate!.AddOriginalVtableForwardAtWorld(thread, ref context, "world-battle-presentation-update",
                BattleTable, 0x30, BattleUpdate, PlayingUpdate, Image(PlayingUpdate, 16));
            _forwarded = true;
            log(new { @event = "client-world-flow-forwarded", nativeActivePhase2AndBattleState1 = true,
                wave218 = _waveAtForward, finished224 = _finishedAtForward,
                originalCallerAndRet4Preserved = true, originalPlayingPauseHandlingPreserved = true,
                newNativeCode = 0, phaseOrTimerWrites = 0, patchOwner = "ClientNativeGate" });
        }
        Require(U(BattleTable + 0x30) == PlayingUpdate && phase == 2 && state == 1 &&
            U(_playing + 0x218) == _waveAtForward && U(_playing + 0x224) == _finishedAtForward,
            "Guest Battle progression changed after presentation-only update admission.");
    }

    internal void RetireAtStop(uint thread, ref Win32Native.X86Context context, uint entry)
    {
        EnsureStop(thread);
        Require(_configured && !_retired && entry == BattleExit && context.Eip == entry &&
            _playing != 0 && context.Ecx == _playing && U(_playing) == BattleTable,
            "Guest Battle flow retirement requires the exact original receiver before native unload.");
        _retired = true;
        log(new { @event = "client-world-flow-retired-before-unload", entry = Hex(entry),
            patchRestoreOwner = "ClientNativeGate", noHistoricalReceiverReadsAfterRetire = true,
            phaseOrTimerWrites = 0 });
    }

    private void ValidateOriginalFunction(uint address, int bytes, string hash)
    {
        byte[] expected = Image(address, bytes);
        Require(Convert.ToHexString(SHA256.HashData(expected)) == hash &&
            Win32Native.ReadMemory(host.ProcessHandle, address, bytes).SequenceEqual(expected) &&
            expected.AsSpan(expected.Length - 3).SequenceEqual(new byte[] { 0xC2, 0x04, 0x00 }),
            "Guest flow original complete function/RET4 pin differs.");
    }
    private void EnsureStop(uint thread) => Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero &&
        host.ImageBase == 0x400000 && thread == host.MainThreadId &&
        Environment.CurrentManagedThreadId == _ownerThread, "Guest flow policy lost its owned main-thread stop.");
    private byte[] Image(uint address, int bytes) => image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, bytes).ToArray();
    private uint ImageWord(uint address) => BitConverter.ToUInt32(Image(address, 4));
    private uint U(uint address) => Win32Native.ReadUInt32(host.ProcessHandle, address);
    private uint P(uint address) { uint value = U(address); Require(value >= 0x10000 && (value & 3) == 0, "Guest flow owner pointer is not aligned/live."); return value; }
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
