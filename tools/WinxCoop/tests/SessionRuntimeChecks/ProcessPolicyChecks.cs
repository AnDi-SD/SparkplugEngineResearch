using System.Security.Cryptography;
using WinxCoop.SessionRuntime;

namespace WinxCoop.SessionRuntimeChecks;

internal static class ProcessPolicyChecks
{
    internal static void Run(string output)
    {
        string fixture = Path.Combine(output, "policy-fixture");
        string bundle = Path.Combine(fixture, "bundle"), local = Path.Combine(fixture, "case");
        Directory.CreateDirectory(bundle); Directory.CreateDirectory(local);
        string image = Path.Combine(bundle, "pinned-child.exe"); File.WriteAllBytes(image, [1, 2, 3, 4]);
        string hash = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(image)));
        var supplied = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) { [image] = hash };
        const string job = @"Local\WinxCoop.E7.fixture";
        var policy = RuntimeProcessPolicy.CreateE7(bundle, local, job, supplied);
        supplied[image] = new string('0', 64);
        ChecksProgram.Require(policy.ValidateImage(image) == image && policy.JobName == job,
            "Caller dictionary mutation changed the immutable runtime pin.");
        ChecksProgram.Reject(() => policy.ValidateImage(Path.Combine(bundle, "unlisted.exe")), "Unlisted image accepted.");
        ChecksProgram.Reject(() => policy.ValidateOutput(Path.Combine(local, "..", "sibling")), "Output escaped its exact root.");
        ChecksProgram.Reject(() => RuntimeProcessPolicy.CreateE7(bundle, local, @"Local\WinxCoop.E6.wrong", new Dictionary<string, string> { [image] = hash }), "E6 Job admitted as E7.");
        ChecksProgram.Reject(() => RuntimeProcessPolicy.CreateE7(bundle, local, job, new Dictionary<string, string> { [Path.Combine(fixture, "bundle-sibling", "x.exe")] = hash }), "Bundle prefix sibling admitted.");
        File.WriteAllBytes(image, [5, 6, 7, 8]);
        ChecksProgram.Reject(() => policy.ValidateImage(image), "Changed image bytes retained an old admission.");
        File.WriteAllBytes(image, [1, 2, 3, 4]);
        using (policy.OpenPinnedImage(image))
        {
            bool rejected = false;
            try { using var mutation = new FileStream(image, FileMode.Open, FileAccess.Write, FileShare.ReadWrite); }
            catch (IOException) { rejected = true; }
            ChecksProgram.Require(rejected, "Pinned launch image permitted a concurrent write.");
        }
        ChecksProgram.Require(policy.ValidateOutput(Path.Combine(local, "owned-child")) == Path.Combine(local, "owned-child"), "Valid local output rejected.");
    }
}
