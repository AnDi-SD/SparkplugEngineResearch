using WinxCoop.RuntimeProbe;
using System.Security.Cryptography;
using WinxCoop.SessionHost;

namespace WinxCoop.SessionRuntimeChecks;

internal static class SessionStartupChecks
{
    internal static void ColdQualificationAndPeerWait()
    {
        // The measured cold SHA and later Guest SHA from the failed E7 run
        // are deterministic elapsed-time inputs, not new disk measurements.
        var budget = new SessionLifecycleBudget(600, 1000);
        long qualified = 1000 + 829173;
        ChecksProgram.Require(!budget.StopDue(qualified), "Cold full SHA consumed the native game budget.");
        budget.CompleteQualification(qualified);
        long peerMatched = qualified + 807688;
        ChecksProgram.Require(!budget.StopDue(peerMatched) && budget.NativeGameStartedAt is null,
            "Waiting for the actual Guest profile consumed native lifetime.");
        budget.BeginNativeGame(peerMatched);
        ChecksProgram.Require(!budget.StopDue(peerMatched + 509999) && budget.StopDue(peerMatched + 510000),
            "Native game stop no longer preserves the selected cleanup reserve.");
        ExpectTimeout(() => budget.Check(peerMatched + 690000));
        ChecksProgram.Require(budget.NativeSupervisorMilliseconds == 3420000 && budget.NativeSupervisorMilliseconds <= 3700000,
            "Native supervisor no longer finitely encloses all admitted phases.");
    }

    internal static void PhaseDeadlines()
    {
        var qualification = new SessionLifecycleBudget(60, 1000);
        ChecksProgram.Reject(() => qualification.BeginNativeGame(1001), "Unqualified game started its native budget.");
        qualification.Check(1200999);
        ExpectTimeout(() => qualification.CompleteQualification(1201000));
        var peerWait = new SessionLifecycleBudget(60, 1000);
        peerWait.CompleteQualification(2000);
        ChecksProgram.Reject(() => peerWait.CompleteQualification(2001), "Local qualification was repeated.");
        peerWait.Check(1501999);
        ExpectTimeout(() => peerWait.BeginNativeGame(1502000));
        var shortGame = new SessionLifecycleBudget(60, 1000);
        shortGame.CompleteQualification(2000); shortGame.BeginNativeGame(3000);
        ChecksProgram.Require(!shortGame.StopDue(17999) && shortGame.StopDue(18000), "Minimum native budget changed.");
        ChecksProgram.Reject(() => shortGame.BeginNativeGame(18001), "Restart reset the overall native game budget.");
        ChecksProgram.Reject(() => shortGame.Check(18000), "A local monotonic clock rollback was accepted.");
    }

    // Pure elapsed-time inputs only: no sleeps, native owners or game run.
    internal static void InteractiveLifetimeBoundaries()
    {
        ChecksProgram.Require(InteractiveSessionLimits.DefaultSeconds == 7200 &&
            InteractiveSessionLimits.MaximumSeconds == 14400, "Interactive defaults changed.");
        ChecksProgram.Reject(() => _ = new SessionLifecycleBudget(601, 0), "Legacy Alfea accepted a long lifetime.");
        ChecksProgram.Reject(() => _ = new SessionLifecycleBudget(14401, 0, true), "Interactive maximum was unbounded.");
        ChecksProgram.Reject(() => _ = new SessionLifecycleBudget(59, 0, true), "Interactive minimum changed.");
        foreach (int seconds in new[] { 60, 7200, 14400 })
        {
            var budget = new SessionLifecycleBudget(seconds, 1000, true);
            budget.CompleteQualification(2000); budget.BeginNativeGame(3000);
            long stop = 3000L + Math.Max(15, seconds - 90) * 1000L;
            ChecksProgram.Require(!budget.StopDue(stop - 1) && budget.StopDue(stop), "Interactive stop boundary changed.");
            ChecksProgram.Reject(() => budget.BeginNativeGame(stop + 1), "Interactive generation reset the global native clock.");
            ExpectTimeout(() => budget.Check(3000L + (seconds + 90) * 1000L));
            ChecksProgram.Require(budget.NativeSupervisorMilliseconds == checked((1200 + 1500 + seconds + 120) * 1000) &&
                budget.NativeSupervisorMilliseconds <= InteractiveSessionLimits.NativeParentMaximumMilliseconds,
                "Finite XP parent no longer encloses the interactive game and cleanup.");
            ChecksProgram.Require(InteractiveSessionLimits.InputSamples(seconds) < uint.MaxValue - 8 &&
                InteractiveSessionLimits.Snapshots(seconds) < uint.MaxValue &&
                InteractiveSessionLimits.ActorJournalBytes > int.MaxValue,
                "Long-session counter or on-disk long arithmetic changed.");
        }
        var parent = new SessionLifecycleBudget(14400, 1000, true);
        ChecksProgram.Require(parent.NativeSupervisorMilliseconds == 17220000,
            "Maximum interactive parent budget changed its exact finite XP bound.");
    }

    internal static void CompleteShaAndCancellation()
    {
        byte[] bytes = new byte[2 * 1024 * 1024 + 17];
        new Random(280007).NextBytes(bytes);
        string expected = Convert.ToHexString(SHA256.HashData(bytes));
        using (var stream = new ShortReadStream(bytes))
            ChecksProgram.Require(GameProfile.Hash(stream) == expected && stream.BytesRead == bytes.Length,
                "Complete SHA changed under short reads or skipped the final bytes.");
        bytes[^1] ^= 0x40;
        using (var stream = new ShortReadStream(bytes))
            ChecksProgram.Require(GameProfile.Hash(stream) != expected, "Full SHA ignored changed trailing resource bytes.");
        using var cancelled = new ShortReadStream(bytes);
        int checkpoints = 0; bool cancelledObserved = false;
        try { _ = GameProfile.Hash(cancelled, checkpoint: () => { if (++checkpoints == 7) throw new OperationCanceledException(); }); }
        catch (OperationCanceledException) { cancelledObserved = true; }
        ChecksProgram.Require(cancelledObserved && cancelled.BytesRead is > 0 && cancelled.BytesRead < bytes.Length,
            "Stop/lease checkpoint did not interrupt an in-progress resource SHA.");
    }

    internal static void LightweightSelection(string output)
    {
        string root = Path.Combine(output, "selection-fixture");
        Directory.CreateDirectory(Path.Combine(root, "Media")); Directory.CreateDirectory(Path.Combine(root, "Shaders"));
        File.WriteAllText(Path.Combine(root, "WinxCoop-preparation.json"), "{}");
        File.WriteAllText(Path.Combine(root, "winx.ini"), "startLevel=28\n");
        byte[] unsupportedExecutable = [0, 1, 2, 3];
        File.WriteAllBytes(Path.Combine(root, "WinxClub.exe"), unsupportedExecutable);
        var selected = new GameSelection(root);
        ChecksProgram.Require(selected.GameRoot == root &&
            Convert.ToHexString(SHA256.HashData(unsupportedExecutable)) != GameProfile.OriginalExecutableSha256,
            "Lightweight selection unexpectedly granted original-executable qualification.");
        File.WriteAllText(Path.Combine(root, "winx.ini"), "startLevel=47\n");
        ChecksProgram.Reject(() => _ = new GameSelection(root), "Selection broadened the admitted Alfea28 profile.");
        File.WriteAllText(Path.Combine(root, "winx.ini"), "startLevel=28\nstartLevel=28\n");
        ChecksProgram.Reject(() => _ = new GameSelection(root), "Ambiguous startup configuration was accepted.");
    }

    private static void ExpectTimeout(Action operation)
    {
        try { operation(); }
        catch (TimeoutException) { return; }
        throw new InvalidDataException("A bounded startup/game phase exceeded its exact deadline.");
    }

    private sealed class ShortReadStream(byte[] bytes) : MemoryStream(bytes, writable: false)
    {
        internal int BytesRead { get; private set; }
        public override int Read(byte[] buffer, int offset, int count)
        {
            int read = base.Read(buffer, offset, Math.Min(count, 4093));
            BytesRead += read; return read;
        }
    }
}
