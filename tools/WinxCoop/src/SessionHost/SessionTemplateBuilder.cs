using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace WinxCoop.SessionHost;

// One pinned raw-frame recipe, derived locally from the user's own Media.
// This does not serialize arbitrary SPT objects or distribute game assets.
internal static class SessionTemplateBuilder
{
    internal const string TemplateSha256 = "70818B42F9592109F276B379176E8F212F1D2F21BCE11D628306E27D53EA15E6";
    internal const string TemplateName = "BloomBody-PlayerTraits-TargetRegion-ProjectileManager.spt";
    private const int HeaderBytes = 76, FrameHeaderBytes = 456, ResultBytes = 7227;
    private const string StaticPath = @"Characters\Bloom\StaticSeresa.spt";
    private const string PlayerPath = @"Characters\Bloom\Player.spt";
    private const string TargetPath = @"Characters\Icy\IcyX.spt";
    private const string BodyPath = @"Characters\Bloom\bloom_jeans.smo";

    internal static string Create(string mediaDirectory, string freshOutputDirectory)
    {
        string media = Absolute(mediaDirectory), output = Absolute(freshOutputDirectory);
        Require(!Within(output, media) && !Within(media, output), "Template output and original Media must be separate trees.");
        OrdinaryAncestors(media);
        Require(!Directory.Exists(output) && !File.Exists(output), "A fresh template output directory is required.");
        string parent = Path.GetDirectoryName(output) ?? throw new InvalidDataException("Template output requires a parent directory.");
        OrdinaryAncestors(parent);

        using var source = PinnedInput.Open(media, StaticPath, 4298,
            "617EF85C33E9E447565CBF83A08FA2476D22F1C24570B59A6D21D4BEA61A7296");
        using var player = PinnedInput.Open(media, PlayerPath, 9773,
            "37FFD448657B2D9C99E12EE743B797FC11880C5C43EE109445ADE94726DF7185");
        using var target = PinnedInput.Open(media, TargetPath, 6503,
            "74E9A0D1428D924EFEA9B4507806C4CE450F4A97A107D67E612ECFF3AC6ECC51");
        using var bodyAsset = PinnedInput.Open(media, BodyPath, 613109,
            "17184190AAB5C45CFE7EB594B6B166A343CD688D99C236FEA3508CA5809B28CA");
        ValidateHeader(source.Bytes, 6); ValidateHeader(player.Bytes, 11); ValidateHeader(target.Bytes, 9);

        byte[] body = Frame(source, 76, 456, 0, 5, -1, null, 0,
            "1D888F58B65738DEC6A45CE0D151E134507534A8AFE0B4318B35317A395B4405");
        Require(AsciiZ(body.AsSpan(4, 256)) == @"Characters\Seresa\Seresa.smo", "Unexpected original body asset field.");
        byte[] assetName = Encoding.ASCII.GetBytes(BodyPath + '\0');
        Require(assetName.Length == 33, "The pinned body substitution must occupy exactly33 bytes.");
        assetName.CopyTo(body, 4); // Preserve every byte after the new NUL.

        byte[] character = Frame(source, 532, 671, 1, 1, -1, 0x0003CC73, 6,
            "334DAE045D94BA1E4DF2E6EBA86B825B345C65A16D5C5138E17BB772DEA08DFB");
        byte[] traits = Frame(player, 1830, 1207, 1, 4, -1, 0x28C56217, 22,
            "AED674B27EFEA8AAF077205E637F83397F07DB6B2949C191633BF81AAF26CB81");
        BinaryPrimitives.WriteUInt32LittleEndian(traits.AsSpan(0x104, 4), 2);
        byte[] stateMachine = Frame(player, 3037, 621, 1, 7, -1, 0x37810000, 4,
            "A121F7A0ACE483E4E462358ED2C00E1FF30AB557312449791D1C8C00ADA5D91B");
        BinaryPrimitives.WriteUInt32LittleEndian(stateMachine.AsSpan(0x104, 4), 0);
        byte[] move = Frame(source, 2697, 583, 1, 4, -1, 0xCD2B2876, 3,
            "EDBEFFEC9925B0E7806523EFB426BB859902CEE199CE104C0B9E06DA5899B2E0");
        byte[] animation = Frame(source, 3280, 1018, 1, 3, -1, 0x680838E4, 17,
            "30479D599A817179B5F43D1C9AE1A141A9684FC8A01B009CA30B126E7E2C3B73");
        byte[] region = Frame(target, 5758, 745, 1, 6, 0, 0x4307193A, 9,
            "B832FAED24423ECE7C6E3095CD22FFEBB289BF9262F58830899C84844FAAAC36");
        Require(AsciiZ(region.AsSpan(0x10C, 64)) == "Spine_02", "Unexpected original TargetRegion parent name.");
        BinaryPrimitives.WriteUInt32LittleEndian(region.AsSpan(0x104, 4), 6);
        BinaryPrimitives.WriteInt32LittleEndian(region.AsSpan(0x108, 4), -1);
        region.AsSpan(0x10C, 64).Clear();
        byte[] projectile = Frame(player, 5942, 1850, 1, 9, -1, 0x551015C7, 39,
            "813851AD9E5960884A8BC4064A035F42BA232DD978C9C99BE2AA433672B20BAF");

        byte[][] frames = [body, character, traits, stateMachine, move, animation, region, projectile];
        int[] offsets = [76, 532, 1203, 2410, 3031, 3614, 4632, 5377];
        uint[] identities = [5, 1, 2, 0, 4, 3, 6, 9];
        byte[] result = new byte[ResultBytes]; source.Bytes.AsSpan(0, HeaderBytes).CopyTo(result);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x44, 4), 8);
        int cursor = HeaderBytes;
        for (int index = 0; index < frames.Length; ++index)
        {
            Require(cursor == offsets[index] && U(frames[index], 0x104) == identities[index], "Pinned output frame ordering changed.");
            frames[index].CopyTo(result, cursor); cursor = checked(cursor + frames[index].Length);
        }
        Require(cursor == ResultBytes && Hash(result) == TemplateSha256, "Derived template differs from the exact native-admitted recipe.");

        // Do not create any output until all original checks and final bytes pass.
        Directory.CreateDirectory(output); OrdinaryAncestors(output);
        string template = Path.Combine(output, TemplateName);
        using (var file = new FileStream(template, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
        { file.Write(result, 0, result.Length); file.Flush(flushToDisk: true); }
        Require(File.ReadAllBytes(template).AsSpan().SequenceEqual(result), "Derived template readback changed.");
        foreach (PinnedInput input in new[] { source, player, target, bodyAsset }) input.VerifyUnchanged();
        using (var receipt = new FileStream(Path.Combine(output, "template-recipe.json"), FileMode.CreateNew, FileAccess.Write, FileShare.Read))
        {
            JsonSerializer.Serialize(receipt, new
            {
                version = 1, recipe = "WinxCoop-owned-pinned-SPT-frame-splice-v1", file = TemplateName,
                bytes = result.Length, sha256 = TemplateSha256,
                inputs = new[] { source, player, target, bodyAsset }.Select(input => new
                    { relativePath = input.Relative, bytes = input.Bytes.Length, sha256 = input.Sha256 }).ToArray(),
                frames = frames.Select((frame, index) => new
                    { identity = identities[index], offset = offsets[index], bytes = frame.Length, sha256 = Hash(frame) }).ToArray(),
                patches = new[] { "header descriptor count6 to8", "body asset ASCIIZ33 bytes; remaining padding retained",
                    "PlayerTraits identity4 to2", "BloomFSM identity7 to0", "TargetRegion parent0 to-1 and parent-name64 cleared" },
                originalInputsRereadUnchanged = true, nativeParseExecuted = false, gameplayAcceptanceClaimed = false,
                gameAssetsBundledWithProgram = false
            }, new JsonSerializerOptions { WriteIndented = true });
            receipt.Flush(flushToDisk: true);
        }
        return template;
    }

    private static byte[] Frame(PinnedInput input, int offset, int bytes, uint type, uint identity, int parent,
        uint? classId, uint propertyCount, string expectedSha256)
    {
        Require(offset >= HeaderBytes && bytes is >= FrameHeaderBytes and <= 8192 && offset <= input.Bytes.Length - bytes,
            "Pinned frame exceeds its original file.");
        byte[] frame = input.Bytes.AsSpan(offset, bytes).ToArray();
        Require(Hash(frame) == expectedSha256 && U(frame, 0) == type && U(frame, 0x104) == identity &&
            BinaryPrimitives.ReadInt32LittleEndian(frame.AsSpan(0x108, 4)) == parent, "Original frame identity/hash changed.");
        if (classId is { } expectedClass)
            Require(type == 1 && bytes >= 480 && U(frame, 456) == 0x74474539 && U(frame, 460) == 2 &&
                U(frame, 464) == expectedClass && U(frame, 468) == bytes - 480 && U(frame, 476) == propertyCount,
                "Original component property frame bounds/class changed.");
        else Require(type == 0 && bytes == FrameHeaderBytes, "Unexpected original asset frame.");
        if (parent == -1) Require(AsciiZ(frame.AsSpan(0x10C, 64)).Length == 0, "Unexpected original root-frame parent name.");
        return frame;
    }

    private static void ValidateHeader(byte[] bytes, uint count) => Require(bytes.Length >= HeaderBytes &&
        U(bytes, 0) == 0xDAB33F00 && U(bytes, 0x44) == count, "Original SPT header differs from the pinned recipe.");
    private static uint U(byte[] bytes, int offset) => BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(offset, 4));
    private static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static string AsciiZ(ReadOnlySpan<byte> bytes)
    {
        int end = bytes.IndexOf((byte)0);
        Require(end >= 0, "Unterminated original fixed string.");
        for (int index = 0; index < end; ++index) Require(bytes[index] is >= 32 and <= 126, "Original fixed field is not bounded ASCII.");
        return Encoding.ASCII.GetString(bytes[..end]);
    }
    private static string Absolute(string path)
    {
        Require(!string.IsNullOrWhiteSpace(path) && Path.IsPathFullyQualified(path), "Absolute local preparation paths are required.");
        string full = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        Require(full.Length is > 3 and <= 220 && full[1] == ':' && full[2] == '\\' &&
            full.All(value => value is >= ' ' and <= '~' && value != '"'), "Use bounded local ASCII drive paths for preparation.");
        return full;
    }
    private static bool Within(string candidate, string directory) => candidate.Equals(directory, StringComparison.OrdinalIgnoreCase) ||
        candidate.StartsWith(directory + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase);
    private static void OrdinaryAncestors(string directory)
    {
        for (string? current = directory; current is not null; current = Path.GetDirectoryName(current))
        {
            Require(Directory.Exists(current) && (File.GetAttributes(current) & FileAttributes.ReparsePoint) == 0,
                "Preparation requires ordinary existing directories without reparse points.");
            if (Path.GetPathRoot(current) == current) break;
        }
    }
    private static void Require(bool condition, string message)
    { if (!condition) throw new InvalidDataException(message); }

    private sealed class PinnedInput : IDisposable
    {
        private readonly FileStream _file;
        internal string Relative { get; }
        internal string Sha256 { get; }
        internal byte[] Bytes { get; }
        private PinnedInput(string relative, FileStream file, byte[] bytes, string sha256)
        { Relative = relative; _file = file; Bytes = bytes; Sha256 = sha256; }
        internal static PinnedInput Open(string media, string relative, int expectedBytes, string expectedSha256)
        {
            string path = Path.GetFullPath(Path.Combine(media, relative));
            Require(Within(path, media), "Original input escaped the selected Media.");
            OrdinaryAncestors(Path.GetDirectoryName(path)!);
            Require(File.Exists(path) && (File.GetAttributes(path) & FileAttributes.ReparsePoint) == 0,
                "Original preparation input is absent or redirected: " + relative);
            var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            try
            {
                Require(file.Length == expectedBytes, "Original preparation input has an unsupported size: " + relative);
                byte[] bytes = ReadAll(file, expectedBytes);
                Require(Hash(bytes) == expectedSha256, "Original preparation input checksum differs: " + relative);
                return new(relative, file, bytes, expectedSha256);
            }
            catch { file.Dispose(); throw; }
        }
        internal void VerifyUnchanged()
        {
            Require(_file.Length == Bytes.Length, "Original preparation input size changed.");
            _file.Position = 0;
            Require(ReadAll(_file, Bytes.Length).AsSpan().SequenceEqual(Bytes), "Original preparation input changed during derivation.");
        }
        private static byte[] ReadAll(FileStream file, int bytes)
        {
            byte[] result = new byte[bytes]; int offset = 0;
            while (offset < result.Length)
            {
                int read = file.Read(result, offset, result.Length - offset);
                if (read == 0) throw new EndOfStreamException("Original preparation input ended during its pinned read.");
                offset = checked(offset + read);
            }
            Require(file.ReadByte() == -1, "Original preparation input grew during its pinned read.");
            return result;
        }
        public void Dispose() => _file.Dispose();
    }
}
