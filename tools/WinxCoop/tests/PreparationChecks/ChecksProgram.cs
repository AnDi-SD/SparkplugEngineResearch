using System.Security.Cryptography;
using System.Text.Json;
using WinxCoop.SessionHost;

namespace WinxCoop.PreparationChecks;

// The actual production builder is linked by the project. Inputs are explicit
// user-owned resources; only this check's fresh output tree is written.
internal static class ChecksProgram
{
    private sealed record Check(string Name, bool Passed, string? Failure);
    private static int Main(string[] args)
    {
        string? output = null, failure = null, generated = null;
        var checks = new List<Check>();
        try
        {
            Require(args.Length == 6, "Use --media ABS --expected-template ABS --output FRESH_ABS.");
            var values = new Dictionary<string, string>(StringComparer.Ordinal);
            for (int index = 0; index < args.Length; index += 2)
                Require(values.TryAdd(args[index], args[index + 1]), "Repeated preparation check option.");
            Require(values.Count == 3 && values.ContainsKey("--media") && values.ContainsKey("--expected-template") &&
                values.ContainsKey("--output"), "Unknown preparation check option.");
            output = values["--output"]; string media = values["--media"], expected = values["--expected-template"];
            Require(new[] { output, media, expected }.All(Path.IsPathFullyQualified) &&
                !Directory.Exists(output) && !File.Exists(output), "Explicit originals/reference and fresh absolute check output required.");
            string[] originals = new[] { @"Characters\Bloom\StaticSeresa.spt", @"Characters\Bloom\Player.spt",
                @"Characters\Icy\IcyX.spt", @"Characters\Bloom\bloom_jeans.smo" }.Select(relative => Path.Combine(media, relative)).ToArray();
            var originalHashes = originals.ToDictionary(path => path, Hash, StringComparer.OrdinalIgnoreCase);
            byte[] reference = File.ReadAllBytes(expected);
            Require(reference.Length == 7227 && Hash(reference) == SessionTemplateBuilder.TemplateSha256,
                "Explicit historical native-admitted reference differs from the selected recipe.");
            Directory.CreateDirectory(output);
            string derived = Path.Combine(output, "derived-template");
            Run("production recipe reproduces the exact native-admitted7227-byte input", () =>
            {
                generated = SessionTemplateBuilder.Create(media, derived);
                Require(File.ReadAllBytes(generated).AsSpan().SequenceEqual(reference), "C# builder output differs from the historical reference.");
                using JsonDocument receipt = JsonDocument.Parse(File.ReadAllBytes(Path.Combine(derived, "template-recipe.json")));
                JsonElement root = receipt.RootElement;
                Require(root.GetProperty("sha256").GetString() == SessionTemplateBuilder.TemplateSha256 &&
                    root.GetProperty("bytes").GetInt32() == 7227 && root.GetProperty("inputs").GetArrayLength() == 4 &&
                    root.GetProperty("frames").GetArrayLength() == 8 && root.GetProperty("originalInputsRereadUnchanged").GetBoolean(),
                    "Recipe receipt is incomplete or differs from the generated file.");
            });
            Run("existing output is rejected without replacing its bytes", () =>
            {
                Reject(() => SessionTemplateBuilder.Create(media, derived), "fresh template output");
                Require(File.ReadAllBytes(generated!).AsSpan().SequenceEqual(reference), "Existing preparation output was replaced.");
            });
            Run("changed original checksum is rejected before any derived output", () =>
            {
                string invalid = Path.Combine(output, "invalid-media"), destination = Path.Combine(output, "rejected-template");
                string input = Path.Combine(invalid, @"Characters\Bloom\StaticSeresa.spt");
                Directory.CreateDirectory(Path.GetDirectoryName(input)!);
                byte[] changed = File.ReadAllBytes(originals[0]); changed[10] ^= 1; File.WriteAllBytes(input, changed);
                Reject(() => SessionTemplateBuilder.Create(invalid, destination), "checksum differs");
                Require(!Directory.Exists(destination) && !File.Exists(destination), "An unsupported input created template output.");
            });
            Run("output inside the original Media tree is rejected", () =>
            {
                string invalid = Path.Combine(media, "WinxCoop-preparation-forbidden-output");
                bool existed = Directory.Exists(invalid) || File.Exists(invalid);
                Reject(() => SessionTemplateBuilder.Create(media, invalid), "separate trees");
                Require((Directory.Exists(invalid) || File.Exists(invalid)) == existed, "Original Media acquired a check output.");
            });
            Run("all original resources and historical reference remain unchanged", () =>
            {
                Require(originalHashes.All(row => Hash(row.Key) == row.Value) && File.ReadAllBytes(expected).AsSpan().SequenceEqual(reference),
                    "An original or historical reference changed during preparation checks.");
            });
        }
        catch (Exception error) { failure = error.ToString(); }
        bool passed = failure is null && checks.Count == 5 && checks.All(check => check.Passed);
        if (output is not null && Directory.Exists(output))
        {
            using var report = new FileStream(Path.Combine(output, "report.json"), FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            JsonSerializer.Serialize(report, new { schema = 1, passed, failure, checkCount = checks.Count, checks,
                generatedTemplate = generated, recipeSha256 = SessionTemplateBuilder.TemplateSha256,
                gameStarted = false, nativeParseExecuted = false, gameplayAcceptanceClaimed = false,
                originalResourcesDistributed = false }, new JsonSerializerOptions { WriteIndented = true });
        }
        Console.WriteLine(passed ? "Preparation checks PASS (exact local resource recipe only)." : "Preparation checks FAIL: " + failure);
        return passed ? 0 : 1;

        void Run(string name, Action action)
        {
            try { action(); checks.Add(new(name, true, null)); }
            catch (Exception error) { checks.Add(new(name, false, error.ToString())); throw; }
        }
    }
    private static string Hash(string path) => Hash(File.ReadAllBytes(path));
    private static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static void Require(bool condition, string message)
    { if (!condition) throw new InvalidDataException(message); }
    private static void Reject(Action operation, string messageFragment)
    {
        bool rejected = false;
        try { operation(); }
        catch (InvalidDataException error) { rejected = error.Message.Contains(messageFragment, StringComparison.Ordinal); }
        Require(rejected, "Preparation did not reject the expected invalid boundary: " + messageFragment);
    }
}
