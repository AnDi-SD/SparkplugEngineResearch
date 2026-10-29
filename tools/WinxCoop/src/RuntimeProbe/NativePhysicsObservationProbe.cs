using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Own read-only brackets of original candidate-correction calls.</summary>
internal sealed class NativePhysicsObservationProbe(
    DebuggerHost host, uint root, uint move, Func<bool> ownMoveActive,
    Func<uint> ownUpdate, Action verifyOwnership, Func<uint, int, byte[]> read,
    Func<uint, uint> word, Action<uint, string, BreakpointCallback> hook, Action<object> log)
{
    private const uint EventLimit = 2048;
    private readonly Stage _collision = new("candidate-correction-4F9D30", 0x4FA9DD, 0x4FA9E2);
    private readonly Stage _surface = new("surface-helper-4F9190", 0x4FA9E4, 0x4FA9E9);
    private uint _events, _queryReturns;
    private bool _installed;

    private sealed class Stage(string name, uint before, uint after)
    {
        internal readonly string Name = name;
        internal readonly uint Before = before, After = after;
        internal uint Esp, Update, Calls, Corrections;
        internal Pose? Start;
    }

    private sealed record Pose(float[] Candidate, float[] RootLocal, float[] RootWorld,
        float[] Rates, uint Parent, uint Scene, uint Platform, uint DirtyWord);

    internal object Summary => new
    {
        installed = _installed, readOnly = true, events = _events, eventLimit = EventLimit,
        collisionCorrectionCalls = _collision.Calls, collisionCandidateChanges = _collision.Corrections,
        surfaceHelperCalls = _surface.Calls, surfaceCandidateChanges = _surface.Corrections,
        collisionQueryReturnObservations = _queryReturns,
        pending = _collision.Start is not null || _surface.Start is not null,
        nativeGroundingQualified = false, obstacleBlockingQualified = false,
        nativeQueryReturnMeaningAssigned = false, rawPositionOrControllerWrites = 0
    };

    internal void Install()
    {
        Require(!_installed && host.IsAtDebugStop, "Physics observation must be installed once at an owned stop.");
        _installed = true;
        foreach (Stage stage in new[] { _collision, _surface })
        {
            hook(stage.Before, "OwnPhysics." + stage.Name + ".before", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
            {
                if (!ownMoveActive() || ctx.Esi != move) return;
                Require(ownMoveActive() && tid == host.MainThreadId && ctx.Eip == at && ctx.Ecx == move && stage.Start is null,
                    "Original physics call is outside its own paired Move update.");
                verifyOwnership(); stage.Esp = ctx.Esp; stage.Update = ownUpdate(); stage.Start = Capture();
                Emit("own-physics-before", new { stage = stage.Name, update = stage.Update, callSite = Hex(at),
                    espBeforeCall = Hex(ctx.Esp), receiverEcx = Hex(ctx.Ecx), pose = stage.Start });
            });
            hook(stage.After, "OwnPhysics." + stage.Name + ".after", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
            {
                if (!ownMoveActive() || ctx.Esi != move) return;
                Require(ownMoveActive() && tid == host.MainThreadId && ctx.Eip == at && stage.Start is not null &&
                    ctx.Esp == stage.Esp && ownUpdate() == stage.Update,
                    "Original physics callsite return lost its exact frame/update pairing.");
                verifyOwnership();
                Pose before = stage.Start ?? throw new InvalidDataException("Missing physics callsite baseline.");
                Pose after = Capture();
                double[] correction = Enumerable.Range(0, 3).Select(i => (double)after.Candidate[i] - before.Candidate[i]).ToArray();
                bool changed = correction.Any(v => Math.Abs(v) > 0.0001);
                ++stage.Calls; if (changed) ++stage.Corrections;
                stage.Start = null;
                Emit("own-physics-after", new { stage = stage.Name, update = stage.Update, returnSite = Hex(at),
                    espAfterOriginalRet = Hex(ctx.Esp), expectedCallsiteEsp = Hex(stage.Esp), before, after,
                    candidateCorrection = correction, candidateChanged = changed, originalReturnEax = Hex(ctx.Eax),
                    returnMeaningAssigned = false, frameChanged = before.Parent != after.Parent || before.Platform != after.Platform,
                    finalMoveReturnStillRequired = true });
            });
        }
        hook(0x4F9F18, "OwnPhysics.collision-query-result", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!ownMoveActive() || ctx.Esi != move) return;
            Require(ownMoveActive() && tid == host.MainThreadId && ctx.Eip == at &&
                _collision.Start is not null && _collision.Update == ownUpdate(),
                "Native collision query return escaped its candidate-correction bracket.");
            verifyOwnership(); ++_queryReturns;
            Emit("own-physics-query-return", new { update = ownUpdate(), originalEax = Hex(ctx.Eax),
                nextNativeStoreOffset = "Move+1BD0", rawResultMeaningAssigned = false,
                adapterReadsOrWritesAt1BD0 = false });
        });
    }

    internal void VerifyNoPending()
    {
        Require(_collision.Start is null && _surface.Start is null,
            "Physics observation retained an unmatched native call at the original Move return.");
    }

    private Pose Capture()
    {
        float[] rates = new[] { Float(move + 0x1B0), Float(move + 0x1B4), Float(move + 0x1B8) };
        Require(rates.All(float.IsFinite), "Observed native physics rates are nonfinite.");
        return new(Position(move + 0x12C), Position(root + 0x20), Position(root + 0x74), rates,
            word(root + 0x2C), word(root + 0x3C), word(move + 0x1E4), word(root + 0xB0));
    }

    private float[] Position(uint address)
    {
        byte[] bytes = read(address, 12);
        float[] values = Enumerable.Range(0, 3).Select(i => BitConverter.ToSingle(bytes, i * 4)).ToArray();
        Require(values.All(v => float.IsFinite(v) && MathF.Abs(v) <= 10000000), "Observed physics candidate exceeds its finite bound.");
        return values;
    }
    private float Float(uint address) => BitConverter.ToSingle(read(address, 4));
    private void Emit(string name, object data)
    {
        Require(++_events <= EventLimit, "Read-only physics observation cap exceeded.");
        log(new { @event = name, ordinal = _events, data });
    }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
    private static string Hex(uint value) => $"0x{value:X8}";
}
