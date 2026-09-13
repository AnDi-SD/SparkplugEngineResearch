using System.Buffers.Binary;
using System.Security.Cryptography;

namespace SmoNativeValidator.Core;

public enum ExecutableCheckpointKind
{
    FindMediaPath,
    BuildAssetPath,
    ResourceLoad,
    FfpsHeaderValidation,
    FfpsMagicAccepted,
    FfpsVersionAccepted,
    BloomBodyLoad,
    BloomSnowSpecialCase,
    BloomHairLoad,
    BloomHairSetup,
    NodeAttachment,
    BloomHairRuntimeUpdate,
    TransformTrackBindingWrite,
    TransformTrackEvaluate,
    GameFlowStateEntered,
    GameFlowStateExiting,
    GameFlowStateResumed,
    StringComparisonCall
}

public enum StringComparisonCallMode
{
    None,
    DirectCdeclCall,
    CdeclImportThunk,
    ObfuscatedThiscallTailCall,
    DirectThiscallRangeCompare
}

public enum StringComparisonFunction
{
    None,
    MsvcrStricmp,
    MsvcrStrcmpi,
    Kernel32LstrcmpiA,
    MsvcpBasicStringCstrCompare,
    MsvcpBasicStringRangeCompare,
    MsvcpCharTraitsCompare,
    MsvcrStrncmp
}

/// <summary>
/// A byte signature whose wildcard bytes are ignored when matching. The text
/// form uses two hexadecimal digits for a fixed byte and ?? for a wildcard.
/// </summary>
public sealed class ExecutableBytePattern
{
    private readonly byte[] _bytes;
    private readonly byte[] _mask;

    private ExecutableBytePattern(string text, byte[] bytes, byte[] mask)
    {
        Text = text;
        _bytes = bytes;
        _mask = mask;
    }

    public string Text { get; }
    public int Length => _bytes.Length;

    public static ExecutableBytePattern Parse(string text)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(text);
        string[] tokens = text.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (tokens.Length == 0)
            throw new FormatException("An executable signature cannot be empty.");

        byte[] bytes = new byte[tokens.Length];
        byte[] mask = new byte[tokens.Length];
        for (int index = 0; index < tokens.Length; index++)
        {
            string token = tokens[index];
            if (token is "?" or "??")
                continue;
            if (token.Length != 2 || !byte.TryParse(
                    token,
                    System.Globalization.NumberStyles.HexNumber,
                    System.Globalization.CultureInfo.InvariantCulture,
                    out bytes[index]))
            {
                throw new FormatException($"Invalid signature byte '{token}'.");
            }
            mask[index] = byte.MaxValue;
        }

        if (!mask.Contains(byte.MaxValue))
            throw new FormatException("An executable signature must contain a fixed byte.");
        return new ExecutableBytePattern(string.Join(' ', tokens), bytes, mask);
    }

    public bool IsMatch(ReadOnlySpan<byte> candidate)
    {
        if (candidate.Length < _bytes.Length)
            return false;
        for (int index = 0; index < _bytes.Length; index++)
        {
            if (_mask[index] != 0 && candidate[index] != _bytes[index])
                return false;
        }
        return true;
    }

    internal (int Offset, ReadOnlyMemory<byte> Bytes) GetLongestFixedRun()
    {
        int bestStart = 0;
        int bestLength = 0;
        int currentStart = 0;
        int currentLength = 0;
        for (int index = 0; index <= _mask.Length; index++)
        {
            if (index < _mask.Length && _mask[index] != 0)
            {
                if (currentLength == 0)
                    currentStart = index;
                currentLength++;
                continue;
            }

            if (currentLength > bestLength)
            {
                bestStart = currentStart;
                bestLength = currentLength;
            }
            currentLength = 0;
        }
        return (bestStart, _bytes.AsMemory(bestStart, bestLength));
    }
}

public sealed record ExecutableCheckpoint(
    string Id,
    uint RelativeVirtualAddress,
    ExecutableCheckpointKind Kind,
    ExecutableBytePattern VerificationPattern,
    NativeValidationStage Stage,
    bool Required,
    bool BloomSpecific = false,
    bool StringComparisonSpecific = false,
    bool SceneReadySpecific = false,
    StringComparisonCallMode StringComparisonMode = StringComparisonCallMode.None,
    StringComparisonFunction StringComparisonFunction = StringComparisonFunction.None);

public sealed record ExecutableProfile
{
    public required string Id { get; init; }
    public required string DisplayName { get; init; }
    public required uint PreferredImageBase { get; init; }
    public required IReadOnlyList<ExecutableCheckpoint> Checkpoints { get; init; }
    public string? Notes { get; init; }
}

public sealed record ExecutableSignatureResolution
{
    public required string Id { get; init; }
    public required bool Required { get; init; }
    public required IReadOnlyList<uint> CandidateRvas { get; init; }
    public uint? ResolvedRva { get; init; }
    public string? Diagnostic { get; init; }
}

public sealed record ExecutableIdentification
{
    public required string ExecutablePath { get; init; }
    /// <summary>Diagnostic only. This value never decides compatibility.</summary>
    public required string Sha256 { get; init; }
    public required long FileLength { get; init; }
    public required bool IsSupported { get; init; }
    public ExecutableProfile? Profile { get; init; }
    public string? Error { get; init; }
    public ushort? PeMachine { get; init; }
    public IReadOnlyList<ExecutableSignatureResolution> SignatureResolutions { get; init; } = [];
}

public static class ExecutableProfileCatalog
{
    public const string SignatureProfileId = "winx-pc-x86-signature-resolved";

    private static readonly SignatureDefinition[] IndependentSignatures =
    [
        Sig("CP01", ExecutableCheckpointKind.FindMediaPath, required: false,
            NativeValidationStage.MediaPathInitialization,
            "81 EC 38 01 00 00 A1 ?? ?? ?? ?? 55 8B E9 89 84 24 38 01 00 00 " +
            "8B 45 14 85 C0 74 ?? 50 E8 ?? ?? ?? ?? 83 C4 04 8D 44 24 08 50 68 3F 00 0F 00"),
        Sig("CP02", ExecutableCheckpointKind.BuildAssetPath, required: true,
            NativeValidationStage.AssetPathBuild,
            "81 EC 30 01 00 00 A1 ?? ?? ?? ?? 53 55 56 8B E9 8B 4D 14 8D 74 24 0C " +
            "89 84 24 38 01 00 00 57 2B F1 8A 11 88 14 0E 41 84 D2 75 F6"),
        Sig("CP03", ExecutableCheckpointKind.ResourceLoad, required: true,
            NativeValidationStage.ResourceLoad,
            "56 57 E8 ?? ?? ?? ?? 8B 7C 24 0C 8B F0 8B 06 57 6A 01 8B CE FF 50 20 84 C0 " +
            "75 ?? 57 E9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 08 5F 33 C0 5E C2 04 00"),
        Sig("CP04", ExecutableCheckpointKind.BloomBodyLoad, required: false,
            NativeValidationStage.SceneInitialization,
            "53 8B D9 8B 83 B4 02 00 00 85 C0 0F 84 ?? ?? ?? ?? 8B 43 18 85 C0 0F 85 ?? ?? ?? ?? " +
            "A1 ?? ?? ?? ?? 85 C0 75 ?? E8 ?? ?? ?? ?? A3 ?? ?? ?? ??", bloom: true),
        Sig("CP05", ExecutableCheckpointKind.BloomSnowSpecialCase, required: false,
            NativeValidationStage.SceneInitialization,
            [
                "BF ?? ?? ?? ?? B9 0F 00 00 00 33 C0 F3 A6 5F 5E 74 ?? 8B CB E8 ?? ?? ?? ?? " +
                "8B 4B 18 8B 93 B4 02 00 00",
                "8B 83 E8 06 00 00 BA 21 00 00 00 0F A3 C2 5F 5E 72 ?? 8B CB E8 ?? ?? ?? ?? " +
                "8B 4B 18 8B 93 B4 02 00 00"
            ], bloom: true),
        Sig("CP06", ExecutableCheckpointKind.BloomHairLoad, required: false,
            NativeValidationStage.SceneInitialization,
            "81 EC 04 01 00 00 A1 ?? ?? ?? ?? 89 84 24 00 01 00 00 A1 ?? ?? ?? ?? 85 C0 56 8B F1 " +
            "75 ?? E8 ?? ?? ?? ?? A3 ?? ?? ?? ?? 6A 01 6A 02", bloom: true),
        Sig("CP07", ExecutableCheckpointKind.BloomHairSetup, required: false,
            NativeValidationStage.SceneInitialization,
            "83 EC 0C 56 8B F1 E8 ?? ?? ?? ?? 8B 86 18 01 00 00 85 C0 74 ?? 8B 4E 1C 50 " +
            "E8 ?? ?? ?? ?? 8B 4E 1C 8B 01 6A 00 FF 50 30", bloom: true),
        Sig("CP08", ExecutableCheckpointKind.NodeAttachment, required: false,
            NativeValidationStage.SceneInitialization,
            "6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 51 56 8B 74 24 18 " +
            "39 4E 2C 89 4C 24 04 0F 84 ?? ?? ?? ?? 66 FF 46 08", bloom: true),
        Sig("CP09", ExecutableCheckpointKind.BloomHairRuntimeUpdate, required: false,
            NativeValidationStage.Runtime,
            [
                "81 EC 98 03 00 00 56 8B F1 8B 46 18 85 C0 0F 84 ?? ?? ?? ?? 8B 86 E8 06 00 00 " +
                "83 F8 05 0F 84 ?? ?? ?? ?? 83 F8 01 0F 84 ?? ?? ?? ??",
                "81 EC 98 03 00 00 56 8B F1 8B 46 18 85 C0 0F 84 ?? ?? ?? ?? 8B 86 E8 06 00 00 " +
                "BA 23 00 00 00 0F A3 C2 0F 82 ?? ?? ?? ?? 90 90 90 90"
            ], bloom: true),
        // spTransformTrackEval binding result write. The breakpoint is placed on
        // the MOV itself, after the name lookup returned in EAX and before the
        // evaluator's +0x10 field is updated. It is enabled only together with an
        // explicit string probe and is never part of a validation verdict.
        Sig("CP10", ExecutableCheckpointKind.TransformTrackBindingWrite, required: false,
            NativeValidationStage.Runtime,
            "89 47 10 5F 5E 5B C2 04 00 5E 33 C0 5B C2 04 00",
            stringProbe: true),
        Sig("CP11", ExecutableCheckpointKind.TransformTrackEvaluate, required: false,
            NativeValidationStage.Runtime,
            "81 EC D0 00 00 00 53 55 33 C0 8B D1 8B 4A 14 56 57 33 FF",
            stringProbe: true),
        // wxGameFlowController state callbacks. These are opt-in because they
        // are verdict checkpoints only for contextual scene-ready validation.
        Sig("CP12", ExecutableCheckpointKind.GameFlowStateEntered, required: false,
            NativeValidationStage.Runtime,
            "81 EC 04 01 00 00 A1 ?? ?? ?? ?? 56 8B F1 89 84 24 04 01 00 00 " +
            "8B 86 AC 01 00 00 57 8B BC 86 5C 01 00 00",
            sceneReady: true),
        Sig("CP13", ExecutableCheckpointKind.GameFlowStateExiting, required: false,
            NativeValidationStage.Runtime,
            "8B 11 FF 52 24 8B CE E8 ?? ?? ?? ?? 8B 86 AC 01 00 00 33 C9 " +
            "89 8C 86 5C 01 00 00",
            sceneReady: true),
        Sig("CP14", ExecutableCheckpointKind.GameFlowStateResumed, required: false,
            NativeValidationStage.Runtime,
            "68 ?? ?? ?? ?? 8B CE 89 96 B0 01 00 00 E8 ?? ?? ?? ?? " +
            "E9 ?? ?? ?? ?? 89 8E B0 01 00 00",
            sceneReady: true)
    ];

    private static readonly ExecutableBytePattern FfpsEntryPattern = ExecutableBytePattern.Parse(
        "56 8B 74 24 08 81 3E 46 46 50 53 74 ?? A1 ?? ?? ?? ?? 85 C0 C7 05 ?? ?? ?? ?? " +
        "01 00 01 10 75 ?? E8 ?? ?? ?? ?? A3 ?? ?? ?? ?? 68 74 02 00 00");
    private static readonly ExecutableBytePattern FfpsMagicPattern = ExecutableBytePattern.Parse(
        "83 7E 04 26 74 ?? A1 ?? ?? ?? ?? 85 C0 C7 05 ?? ?? ?? ?? 02 00 01 10 75 ?? " +
        "E8 ?? ?? ?? ?? A3 ?? ?? ?? ?? 68 7C 02 00 00");
    private static readonly ExecutableBytePattern FfpsVersionPattern = ExecutableBytePattern.Parse(
        "8B 4C 24 0C 8B 01 8D 54 24 08 52 FF 50 3C 84 C0 75 ?? A1 ?? ?? ?? ?? 50 " +
        "68 ?? ?? ?? ?? 6A 04 E8 ?? ?? ?? ?? 83 C4 0C 32 C0 5E C2 08 00");
    // PC retail import thunk: MSVCR71!_stricmp at IAT VA 0x006D9288.
    // These probes are optional and only enabled by an explicit runtime string
    // probe. Runtime verification prevents their use on builds with a moved IAT.
    private static readonly ExecutableBytePattern StringComparisonCallPattern =
        ExecutableBytePattern.Parse("FF 15 88 92 6D 00");
    private static readonly ExecutableBytePattern StringComparisonThunkPattern =
        ExecutableBytePattern.Parse("FF 25 88 92 6D 00");
    private static readonly ExecutableBytePattern StrcmpiCallPattern =
        ExecutableBytePattern.Parse("FF 15 80 92 6D 00");
    private static readonly ExecutableBytePattern StrcmpiThunkPattern =
        ExecutableBytePattern.Parse("FF 25 80 92 6D 00");
    private static readonly ExecutableBytePattern LstrcmpiCallPattern =
        ExecutableBytePattern.Parse("FF 15 B0 90 6D 00");
    private static readonly ExecutableBytePattern LstrcmpiThunkPattern =
        ExecutableBytePattern.Parse("FF 25 B0 90 6D 00");
    private static readonly ExecutableBytePattern BasicStringCstrPushRetPattern =
        ExecutableBytePattern.Parse("FF 35 7C 91 6D 00 C3");
    private static readonly ExecutableBytePattern BasicStringCstrJumpPattern =
        ExecutableBytePattern.Parse("FF 25 7C 91 6D 00");
    private static readonly ExecutableBytePattern BasicStringRangeCallPattern =
        ExecutableBytePattern.Parse("FF 15 CC 91 6D 00");
    private static readonly ExecutableBytePattern CharTraitsCompareCallPattern =
        ExecutableBytePattern.Parse("FF 15 1C 92 6D 00");
    private static readonly ExecutableBytePattern CharTraitsCompareThunkPattern =
        ExecutableBytePattern.Parse("FF 25 1C 92 6D 00");
    private static readonly ExecutableBytePattern StrncmpCallPattern =
        ExecutableBytePattern.Parse("FF 15 98 93 6D 00");
    private static readonly ExecutableBytePattern StrncmpThunkPattern =
        ExecutableBytePattern.Parse("FF 25 98 93 6D 00");

    public static async Task<ExecutableIdentification> IdentifyAsync(
        string executablePath,
        CancellationToken cancellationToken = default)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(executablePath);
        string fullPath = Path.GetFullPath(executablePath);
        if (!File.Exists(fullPath))
            return Failed(fullPath, "Executable file does not exist.");

        try
        {
            byte[] image = await File.ReadAllBytesAsync(fullPath, cancellationToken).ConfigureAwait(false);
            cancellationToken.ThrowIfCancellationRequested();
            string sha256 = Convert.ToHexString(SHA256.HashData(image));
            if (!PeImage.TryParse(image, out PeImage? pe, out string? peError))
                return Failed(fullPath, peError ?? "The file is not a valid PE executable.", image.Length, sha256);
            PeImage parsedPe = pe!;
            if (parsedPe.Machine != 0x014C || parsedPe.OptionalHeaderMagic != 0x010B)
            {
                return Failed(fullPath,
                    $"Only a 32-bit x86 PE game can be instrumented (machine 0x{parsedPe.Machine:X4}, optional header 0x{parsedPe.OptionalHeaderMagic:X4}).",
                    image.Length, sha256, parsedPe.Machine);
            }

            ResolutionBuilder builder = new(image, parsedPe);
            foreach (SignatureDefinition definition in IndependentSignatures)
                builder.Resolve(definition);
            builder.ResolveFfpsChain();
            builder.ResolveAllStringComparisonCalls();

            IReadOnlyList<ExecutableSignatureResolution> resolutions = builder.Resolutions;
            ExecutableSignatureResolution[] unresolvedRequired = resolutions
                .Where(resolution => resolution.Required && resolution.ResolvedRva is null)
                .ToArray();
            bool supported = unresolvedRequired.Length == 0;
            string? error = supported
                ? null
                : "Internal loader instrumentation could not be resolved: " +
                  string.Join("; ", unresolvedRequired.Select(resolution =>
                      $"{resolution.Id} ({resolution.Diagnostic})")) + ".";
            ExecutableProfile? profile = supported
                ? new ExecutableProfile
                {
                    Id = SignatureProfileId,
                    DisplayName = "Winx Club PC — внутренний код загрузчика распознан",
                    PreferredImageBase = parsedPe.PreferredImageBase,
                    Checkpoints = builder.Checkpoints,
                    Notes = "Resolved from masked x86 code signatures; SHA-256 and file length are diagnostic only."
                }
                : null;

            return new ExecutableIdentification
            {
                ExecutablePath = fullPath,
                Sha256 = sha256,
                FileLength = image.Length,
                IsSupported = supported,
                Profile = profile,
                Error = error,
                PeMachine = parsedPe.Machine,
                SignatureResolutions = resolutions
            };
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            return Failed(fullPath, exception.Message);
        }
    }

    private static ExecutableIdentification Failed(
        string path,
        string error,
        long length = 0,
        string sha256 = "",
        ushort? machine = null) => new()
    {
        ExecutablePath = path,
        Sha256 = sha256,
        FileLength = length,
        IsSupported = false,
        Error = error,
        PeMachine = machine
    };

    private static SignatureDefinition Sig(
        string id,
        ExecutableCheckpointKind kind,
        bool required,
        NativeValidationStage stage,
        string pattern,
        bool bloom = false,
        bool stringProbe = false,
        bool sceneReady = false) =>
        Sig(id, kind, required, stage, [pattern], bloom, stringProbe, sceneReady);

    private static SignatureDefinition Sig(
        string id,
        ExecutableCheckpointKind kind,
        bool required,
        NativeValidationStage stage,
        IReadOnlyList<string> patterns,
        bool bloom = false,
        bool stringProbe = false,
        bool sceneReady = false) => new(
            id,
            kind,
            required,
            stage,
            patterns.Select(ExecutableBytePattern.Parse).ToArray(),
            bloom,
            stringProbe,
            sceneReady);

    private sealed record SignatureDefinition(
        string Id,
        ExecutableCheckpointKind Kind,
        bool Required,
        NativeValidationStage Stage,
        IReadOnlyList<ExecutableBytePattern> Patterns,
        bool BloomSpecific,
        bool StringComparisonSpecific,
        bool SceneReadySpecific);

    private sealed class ResolutionBuilder(byte[] image, PeImage pe)
    {
        private readonly List<ExecutableSignatureResolution> _resolutions = [];
        private readonly List<ExecutableCheckpoint> _checkpoints = [];

        internal IReadOnlyList<ExecutableSignatureResolution> Resolutions => _resolutions;
        internal IReadOnlyList<ExecutableCheckpoint> Checkpoints => _checkpoints;

        internal void Resolve(SignatureDefinition definition)
        {
            List<PatternHit> hits = [];
            foreach (ExecutableBytePattern pattern in definition.Patterns)
                hits.AddRange(pe.Find(image, pattern));
            PatternHit[] candidates = hits
                .GroupBy(hit => hit.Rva)
                .Select(group => group.First())
                .OrderBy(hit => hit.Rva)
                .ToArray();

            PatternHit? resolved = candidates.Length == 1 ? candidates[0] : null;
            string? diagnostic = candidates.Length switch
            {
                0 => "signature not found",
                1 => null,
                _ => $"ambiguous signature; candidates: {string.Join(", ", candidates.Select(hit => $"RVA 0x{hit.Rva:X8}"))}"
            };
            _resolutions.Add(new ExecutableSignatureResolution
            {
                Id = definition.Id,
                Required = definition.Required,
                CandidateRvas = candidates.Select(hit => hit.Rva).ToArray(),
                ResolvedRva = resolved?.Rva,
                Diagnostic = diagnostic
            });
            if (resolved is not null)
            {
                _checkpoints.Add(new ExecutableCheckpoint(
                    definition.Id,
                    resolved.Rva,
                    definition.Kind,
                    resolved.Pattern,
                    definition.Stage,
                    definition.Required,
                    definition.BloomSpecific,
                    definition.StringComparisonSpecific,
                    definition.SceneReadySpecific));
            }
        }

        internal void ResolveFfpsChain()
        {
            PatternHit[] entries = pe.Find(image, FfpsEntryPattern)
                .Where(IsValidFfpsChain)
                .OrderBy(hit => hit.Rva)
                .ToArray();
            string? diagnostic = entries.Length switch
            {
                0 => "semantic FFPS magic/version branch chain not found",
                1 => null,
                _ => $"ambiguous FFPS chain; candidates: {string.Join(", ", entries.Select(hit => $"RVA 0x{hit.Rva:X8}"))}"
            };
            _resolutions.Add(new ExecutableSignatureResolution
            {
                Id = "FFPS",
                Required = true,
                CandidateRvas = entries.Select(hit => hit.Rva).ToArray(),
                ResolvedRva = entries.Length == 1 ? entries[0].Rva : null,
                Diagnostic = diagnostic
            });
            if (entries.Length != 1)
                return;

            PatternHit entry = entries[0];
            uint magicRva = DecodeShortBranchTarget(entry.Rva, 11);
            uint versionRva = DecodeShortBranchTarget(magicRva, 4);
            _checkpoints.Add(new ExecutableCheckpoint(
                "FFPS01", entry.Rva, ExecutableCheckpointKind.FfpsHeaderValidation,
                FfpsEntryPattern, NativeValidationStage.FfpsHeader, Required: true));
            _checkpoints.Add(new ExecutableCheckpoint(
                "FFPS02", magicRva, ExecutableCheckpointKind.FfpsMagicAccepted,
                FfpsMagicPattern, NativeValidationStage.FfpsHeader, Required: true));
            _checkpoints.Add(new ExecutableCheckpoint(
                "FFPS03", versionRva, ExecutableCheckpointKind.FfpsVersionAccepted,
                FfpsVersionPattern, NativeValidationStage.FfpsVersion, Required: true));
        }

        internal void ResolveAllStringComparisonCalls()
        {
            AddStringComparisonCheckpoints(
                "STRICMP",
                StringComparisonCallPattern,
                StringComparisonThunkPattern,
                StringComparisonFunction.MsvcrStricmp);
            AddStringComparisonCheckpoints(
                "STRCMPI",
                StrcmpiCallPattern,
                StrcmpiThunkPattern,
                StringComparisonFunction.MsvcrStrcmpi);
            AddStringComparisonCheckpoints(
                "LSTRCMPI",
                LstrcmpiCallPattern,
                LstrcmpiThunkPattern,
                StringComparisonFunction.Kernel32LstrcmpiA);
            AddStringComparisonPattern(
                "BSCSTRPUSHRET",
                pe.Find(image, BasicStringCstrPushRetPattern),
                StringComparisonCallMode.ObfuscatedThiscallTailCall,
                StringComparisonFunction.MsvcpBasicStringCstrCompare);
            AddStringComparisonPattern(
                "BSCSTRJUMP",
                pe.Find(image, BasicStringCstrJumpPattern),
                StringComparisonCallMode.ObfuscatedThiscallTailCall,
                StringComparisonFunction.MsvcpBasicStringCstrCompare);
            AddStringComparisonPattern(
                "BSRANGECMP",
                pe.Find(image, BasicStringRangeCallPattern),
                StringComparisonCallMode.DirectThiscallRangeCompare,
                StringComparisonFunction.MsvcpBasicStringRangeCompare);
            AddStringComparisonCheckpoints(
                "CHARTRAITSCMP",
                CharTraitsCompareCallPattern,
                CharTraitsCompareThunkPattern,
                StringComparisonFunction.MsvcpCharTraitsCompare);
            AddStringComparisonCheckpoints(
                "STRNCMP",
                StrncmpCallPattern,
                StrncmpThunkPattern,
                StringComparisonFunction.MsvcrStrncmp);
        }

        private void AddStringComparisonCheckpoints(
            string idPrefix,
            ExecutableBytePattern directPattern,
            ExecutableBytePattern thunkPattern,
            StringComparisonFunction function)
        {
            AddStringComparisonPattern(
                idPrefix,
                pe.Find(image, directPattern),
                StringComparisonCallMode.DirectCdeclCall,
                function);
            AddStringComparisonPattern(
                $"{idPrefix}THUNK",
                pe.Find(image, thunkPattern),
                StringComparisonCallMode.CdeclImportThunk,
                function);
        }

        private void AddStringComparisonPattern(
            string idPrefix,
            IEnumerable<PatternHit> candidates,
            StringComparisonCallMode mode,
            StringComparisonFunction function)
        {
            PatternHit[] hits = candidates.OrderBy(hit => hit.Rva).ToArray();
            for (int index = 0; index < hits.Length; index++)
            {
                PatternHit hit = hits[index];
                _checkpoints.Add(new ExecutableCheckpoint(
                    $"{idPrefix}{index + 1:D2}",
                    hit.Rva,
                    ExecutableCheckpointKind.StringComparisonCall,
                    hit.Pattern,
                    NativeValidationStage.Runtime,
                    Required: false,
                    BloomSpecific: false,
                    StringComparisonSpecific: true,
                    StringComparisonMode: mode,
                    StringComparisonFunction: function));
            }
        }

        private bool IsValidFfpsChain(PatternHit entry)
        {
            if (!TryDecodeShortBranchTarget(entry.Rva, 11, out uint magicRva) ||
                !pe.TryGetFileOffset(magicRva, FfpsMagicPattern.Length, out int magicOffset) ||
                !FfpsMagicPattern.IsMatch(image.AsSpan(magicOffset)))
            {
                return false;
            }
            if (!TryDecodeShortBranchTarget(magicRva, 4, out uint versionRva) ||
                !pe.TryGetFileOffset(versionRva, FfpsVersionPattern.Length, out int versionOffset) ||
                !FfpsVersionPattern.IsMatch(image.AsSpan(versionOffset)))
            {
                return false;
            }
            return entry.Rva < magicRva && magicRva < versionRva && versionRva - entry.Rva < 0x400;
        }

        private uint DecodeShortBranchTarget(uint instructionRva, int opcodeOffset)
        {
            if (!TryDecodeShortBranchTarget(instructionRva, opcodeOffset, out uint target))
                throw new InvalidDataException("The previously validated FFPS branch could not be decoded.");
            return target;
        }

        private bool TryDecodeShortBranchTarget(uint instructionRva, int opcodeOffset, out uint target)
        {
            target = 0;
            uint opcodeRva = checked(instructionRva + (uint)opcodeOffset);
            if (!pe.TryGetFileOffset(opcodeRva, 2, out int offset) || image[offset] != 0x74)
                return false;
            int relative = unchecked((sbyte)image[offset + 1]);
            long candidate = (long)opcodeRva + 2 + relative;
            if (candidate < 0 || candidate > uint.MaxValue)
                return false;
            target = (uint)candidate;
            return true;
        }
    }

    private sealed record PatternHit(uint Rva, ExecutableBytePattern Pattern);

    private sealed class PeImage
    {
        private const uint ImageScnMemExecute = 0x20000000;
        private readonly IReadOnlyList<PeSection> _sections;

        private PeImage(
            ushort machine,
            ushort optionalHeaderMagic,
            uint preferredImageBase,
            IReadOnlyList<PeSection> sections)
        {
            Machine = machine;
            OptionalHeaderMagic = optionalHeaderMagic;
            PreferredImageBase = preferredImageBase;
            _sections = sections;
        }

        internal ushort Machine { get; }
        internal ushort OptionalHeaderMagic { get; }
        internal uint PreferredImageBase { get; }

        internal static bool TryParse(byte[] image, out PeImage? pe, out string? error)
        {
            pe = null;
            error = null;
            if (image.Length < 0x40 || image[0] != (byte)'M' || image[1] != (byte)'Z')
            {
                error = "The file does not have an MZ header.";
                return false;
            }
            int peOffset = BinaryPrimitives.ReadInt32LittleEndian(image.AsSpan(0x3C, 4));
            if (peOffset < 0 || peOffset > image.Length - 24 ||
                !image.AsSpan(peOffset, 4).SequenceEqual("PE\0\0"u8))
            {
                error = "The file does not have a valid PE header.";
                return false;
            }

            ushort machine = BinaryPrimitives.ReadUInt16LittleEndian(image.AsSpan(peOffset + 4, 2));
            ushort sectionCount = BinaryPrimitives.ReadUInt16LittleEndian(image.AsSpan(peOffset + 6, 2));
            ushort optionalSize = BinaryPrimitives.ReadUInt16LittleEndian(image.AsSpan(peOffset + 20, 2));
            int optionalOffset = peOffset + 24;
            if (optionalSize < 32 || optionalOffset > image.Length - optionalSize)
            {
                error = "The PE optional header is truncated.";
                return false;
            }
            ushort magic = BinaryPrimitives.ReadUInt16LittleEndian(image.AsSpan(optionalOffset, 2));
            uint imageBase = BinaryPrimitives.ReadUInt32LittleEndian(image.AsSpan(optionalOffset + 28, 4));
            int sectionOffset = optionalOffset + optionalSize;
            if (sectionCount == 0 || sectionCount > 96 ||
                sectionOffset > image.Length - checked(sectionCount * 40))
            {
                error = "The PE section table is missing or truncated.";
                return false;
            }

            List<PeSection> sections = [];
            for (int index = 0; index < sectionCount; index++)
            {
                int offset = sectionOffset + index * 40;
                uint virtualSize = BinaryPrimitives.ReadUInt32LittleEndian(image.AsSpan(offset + 8, 4));
                uint virtualAddress = BinaryPrimitives.ReadUInt32LittleEndian(image.AsSpan(offset + 12, 4));
                uint rawSize = BinaryPrimitives.ReadUInt32LittleEndian(image.AsSpan(offset + 16, 4));
                uint rawOffset = BinaryPrimitives.ReadUInt32LittleEndian(image.AsSpan(offset + 20, 4));
                uint characteristics = BinaryPrimitives.ReadUInt32LittleEndian(image.AsSpan(offset + 36, 4));
                if (rawSize == 0)
                    continue;
                if (rawOffset > image.Length || rawSize > image.Length - rawOffset)
                {
                    error = $"PE section {index} points outside the file.";
                    return false;
                }
                sections.Add(new PeSection(
                    virtualAddress,
                    virtualSize,
                    checked((int)rawOffset),
                    checked((int)rawSize),
                    (characteristics & ImageScnMemExecute) != 0));
            }
            if (!sections.Any(section => section.Executable))
            {
                error = "The PE file has no executable sections.";
                return false;
            }

            pe = new PeImage(machine, magic, imageBase, sections);
            return true;
        }

        internal IReadOnlyList<PatternHit> Find(byte[] image, ExecutableBytePattern pattern)
        {
            List<PatternHit> hits = [];
            (int anchorOffset, ReadOnlyMemory<byte> anchorMemory) = pattern.GetLongestFixedRun();
            ReadOnlySpan<byte> anchor = anchorMemory.Span;
            foreach (PeSection section in _sections.Where(section => section.Executable))
            {
                ReadOnlySpan<byte> data = image.AsSpan(section.RawOffset, section.RawSize);
                int searchOffset = 0;
                while (searchOffset <= data.Length - anchor.Length)
                {
                    int relativeAnchor = data[searchOffset..].IndexOf(anchor);
                    if (relativeAnchor < 0)
                        break;
                    int candidateOffset = searchOffset + relativeAnchor - anchorOffset;
                    if (candidateOffset >= 0 &&
                        candidateOffset <= data.Length - pattern.Length &&
                        pattern.IsMatch(data[candidateOffset..]))
                    {
                        hits.Add(new PatternHit(
                            checked(section.VirtualAddress + (uint)candidateOffset), pattern));
                    }
                    searchOffset += relativeAnchor + 1;
                }
            }
            return hits;
        }

        internal bool TryGetFileOffset(uint rva, int byteCount, out int fileOffset)
        {
            foreach (PeSection section in _sections)
            {
                if (rva < section.VirtualAddress)
                    continue;
                uint relative = rva - section.VirtualAddress;
                if (relative <= section.RawSize && byteCount <= section.RawSize - relative)
                {
                    fileOffset = checked(section.RawOffset + (int)relative);
                    return true;
                }
            }
            fileOffset = 0;
            return false;
        }

        private sealed record PeSection(
            uint VirtualAddress,
            uint VirtualSize,
            int RawOffset,
            int RawSize,
            bool Executable);
    }
}
