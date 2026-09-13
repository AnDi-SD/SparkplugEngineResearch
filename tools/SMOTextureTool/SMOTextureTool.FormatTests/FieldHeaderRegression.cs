using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using TextureDocument = SMOTextureTool.Core.SmoDocument;

internal static class FieldHeaderRegression
{
    private delegate bool HeaderReader(ReadOnlySpan<byte> data, int offset,
        out int fieldType, out int headerSize, out uint payloadSize);

    public static int Run(string outputDirectory)
    {
        if (Directory.Exists(outputDirectory))
            throw new InvalidOperationException("Use a new output directory to preserve prior evidence.");
        Directory.CreateDirectory(outputDirectory);
        // A typed delegate exercises the actual private caller boundary without
        // boxing ReadOnlySpan or adding a public API solely for the regression.
        var read = typeof(TextureDocument).GetMethod("TryReadDataBlockHeader",
            BindingFlags.NonPublic | BindingFlags.Static)?.CreateDelegate<HeaderReader>()
            ?? throw new MissingMethodException("TextureTool's field-reader boundary was not found.");
        Case[] cases =
        [
            new("empty-form-encoded-id", [0x0a], 0, 10, 1, 0),
            new("fixed-one", [0x2a, 0x55], 0, 10, 1, 1),
            new("fixed-two", [0x4a, 0x55, 0xaa], 0, 10, 1, 2),
            new("fixed-four", [0x6a, 1, 2, 3, 4], 0, 10, 1, 4),
            new("fixed-eight", [0x8a, 1, 2, 3, 4, 5, 6, 7, 8], 0, 10, 1, 8),
            new("uint8-reservation", [0xaa, 3, 1, 2, 3], 0, 10, 2, 3),
            new("uint16-reservation", [0xca, 3, 0, 1, 2, 3], 0, 10, 3, 3),
            new("uint32-reservation", [0xea, 3, 0, 0, 0, 1, 2, 3], 0, 10, 5, 3),
            new("extended-id", [0xff, 200, 1, 0, 0, 0, 0x55], 0, 200, 6, 1),
            new("nonzero-offset", [0x55, 0xaa, 3, 1, 2, 3], 1, 10, 2, 3),
            new("empty-input", [], 0),
            new("negative-offset", [0x2a, 0x55], -1),
            new("end-offset", [0x2a, 0x55], 2),
            new("truncated-extended-id", [0xdf], 0),
            new("truncated-uint8-size", [0xaa], 0),
            new("truncated-uint16-size", [0xca, 1], 0),
            new("truncated-uint32-size", [0xea, 1, 0, 0], 0),
            new("truncated-payload", [0xea, 3, 0, 0, 0, 1, 2], 0),
            new("uint32-ffffffff-rejects-without-overflow", [0xea, 0xff, 0xff, 0xff, 0xff], 0)
        ];
        int failures = 0;
        var observations = new List<object>();
        foreach (var item in cases)
        {
            bool accepted = false;
            int field = 0, headerSize = 0;
            uint payloadSize = 0;
            Exception? failure = null;
            try { accepted = read(item.Bytes, item.Offset, out field, out headerSize, out payloadSize); }
            catch (Exception error) { failure = error; }
            bool expectsAccepted = item.FieldType.HasValue;
            // Callers inspect outputs only on success. Rejected fields must
            // return false, including oversized UInt32 lengths, without throwing.
            bool passed = failure is null && accepted == expectsAccepted &&
                (!expectsAccepted || (field == item.FieldType && headerSize == item.HeaderSize && payloadSize == item.PayloadSize));
            if (!passed) ++failures;
            observations.Add(new { name = item.Name, passed, bytesHex = Convert.ToHexString(item.Bytes),
                offset = item.Offset, expectedAccepted = expectsAccepted,
                expectedField = item.FieldType, expectedHeaderSize = item.HeaderSize,
                expectedPayloadSize = item.PayloadSize, accepted, field, headerSize, payloadSize,
                exceptionType = failure?.GetType().FullName, exceptionMessage = failure?.Message });
        }
        string nativePath = Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll");
        File.WriteAllText(Path.Combine(outputDirectory, "report.json"), JsonSerializer.Serialize(new
        {
            status = failures == 0 ? "passed" : "failed", checks = cases.Length, failures,
            scope = "TextureTool's actual private header-reader boundary; independent explicit wire examples, no game execution",
            textureToolAssembly = new { path = typeof(TextureDocument).Assembly.Location,
                sha256 = HashFile(typeof(TextureDocument).Assembly.Location) },
            testAssemblySha256 = HashFile(typeof(FieldHeaderRegression).Assembly.Location),
            nativeDll = new { path = nativePath, sha256 = File.Exists(nativePath) ? HashFile(nativePath) : null },
            cases = observations
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"{(failures == 0 ? "PASS" : "FAIL")} TextureTool field headers: {cases.Length} checks, {failures} failures");
        return failures == 0 ? 0 : 1;
    }

    private sealed record Case(string Name, byte[] Bytes, int Offset,
        int? FieldType = null, int? HeaderSize = null, uint? PayloadSize = null);

    private static string HashFile(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(stream));
    }
}
