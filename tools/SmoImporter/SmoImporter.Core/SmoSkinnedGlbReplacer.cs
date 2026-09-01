using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.Formats.Png;
using SixLabors.ImageSharp.PixelFormats;
using SmoViewer.Core;

namespace SmoImporter.Core;

public sealed record GlbBoneRemap(
    string DonorBoneName,
    string TargetBoneName,
    string Reason);

public sealed record GlbSkinTransferPlan(
    SmoSkeletonCompatibility Compatibility,
    int MeshCount,
    int MaterialGroupCount,
    int JointCount,
    int ActiveJointCount,
    int DifferentBindPoseJointCount,
    IReadOnlyList<string> MatchedBoneNames,
    IReadOnlyList<GlbBoneRemap> RemappedBones,
    IReadOnlyList<string> UnusedGlbJoints,
    IReadOnlyList<string> TargetBonesWithoutWeights,
    IReadOnlyList<string> Messages)
{
    public bool CanReplace => Compatibility != SmoSkeletonCompatibility.Incompatible;
}

public sealed record GlbSkinTransferResult(
    string OutputPath,
    int MeshSlotCount,
    int VertexCount,
    int TriangleCount,
    int PaletteCount,
    long FileSize,
    string Sha256);

/// <summary>
/// Controls how prepared skinned geometry is moved onto the target skeleton.
/// <see cref="PreservePreparedGeometry"/> is the non-mutating API default for
/// models already authored in the exact target bind pose. When bind poses differ,
/// production callers should explicitly use <see cref="RetargetToGameBindPose"/>:
/// it performs exactly one donor-bind-to-target-bind conversion and still writes
/// only target bone references and target inverse-bind matrices to the SMO.
/// </summary>
public enum SkinnedGeometryTransferMode
{
    PreservePreparedGeometry = 0,
    RetargetToGameBindPose = 1
}

/// <summary>
/// Selects the native material family for one imported renderable. <see cref="Auto"/>
/// preserves the existing texture-alpha classifier. <see cref="OpaqueOverlay"/>
/// keeps the renderable in an independent post-body draw unit but uses the
/// canonical opaque Bloom material state. <see cref="TransparentSurface"/>
/// explicitly selects the native princess texture-alpha material state.
/// </summary>
public enum SkinnedRenderableMaterialMode
{
    Auto = 0,
    OpaqueOverlay = 1,
    TransparentSurface = 2
}

public sealed record SkinnedRenderableMaterialOverride(
    int SourceMeshKey,
    SkinnedRenderableMaterialMode Mode);

/// <summary>
/// Immutable per-renderable material decisions for skinned import. Keys are
/// indices in the caller's <see cref="ImportedScene.Meshes"/> collection and
/// remain stable through material grouping and native branch preparation.
/// </summary>
public sealed class SkinnedRenderableMaterialProfile
{
    private readonly IReadOnlyDictionary<int, SkinnedRenderableMaterialMode> _modes;
    private readonly IReadOnlyList<SkinnedRenderableMaterialOverride> _overrides;
    private readonly string? _sourceFingerprint;

    public SkinnedRenderableMaterialProfile(
        ImportedScene donor,
        IEnumerable<SkinnedRenderableMaterialOverride> overrides)
    {
        ArgumentNullException.ThrowIfNull(donor);
        ArgumentNullException.ThrowIfNull(overrides);
        SkinnedRenderableMaterialOverride[] ordered = overrides
            .OrderBy(item => item.SourceMeshKey)
            .ToArray();
        var modes = new Dictionary<int, SkinnedRenderableMaterialMode>();
        foreach (SkinnedRenderableMaterialOverride item in ordered)
        {
            if (item.SourceMeshKey < 0)
                throw new ArgumentOutOfRangeException(
                    nameof(overrides), item.SourceMeshKey,
                    "A source mesh key cannot be negative.");
            if (!Enum.IsDefined(item.Mode))
                throw new ArgumentOutOfRangeException(
                    nameof(overrides), item.Mode,
                    "A renderable material mode is not defined.");
            if (!modes.TryAdd(item.SourceMeshKey, item.Mode))
                throw new ArgumentException(
                    $"Source mesh key {item.SourceMeshKey} has more than one material override.",
                    nameof(overrides));
        }
        _modes = modes;
        _overrides = Array.AsReadOnly(ordered);
        _sourceFingerprint = ComputeSourceFingerprint(donor);
    }

    private SkinnedRenderableMaterialProfile()
    {
        _modes = new Dictionary<int, SkinnedRenderableMaterialMode>();
        _overrides = Array.Empty<SkinnedRenderableMaterialOverride>();
    }

    public static SkinnedRenderableMaterialProfile Default { get; } = new();

    public IReadOnlyList<SkinnedRenderableMaterialOverride> Overrides => _overrides;

    public bool HasExplicitOverrides =>
        _modes.Values.Any(mode => mode != SkinnedRenderableMaterialMode.Auto);

    internal SkinnedRenderableMaterialMode GetMode(int sourceMeshKey) =>
        _modes.GetValueOrDefault(sourceMeshKey, SkinnedRenderableMaterialMode.Auto);

    internal void ValidateSource(ImportedScene donor)
    {
        ArgumentNullException.ThrowIfNull(donor);
        if (_sourceFingerprint is null)
        {
            if (_overrides.Count != 0)
                throw new InvalidOperationException(
                    "An unbound renderable material profile cannot contain overrides.");
            return;
        }
        string actual = ComputeSourceFingerprint(donor);
        if (!string.Equals(_sourceFingerprint, actual, StringComparison.Ordinal))
        {
            throw new InvalidOperationException(
                "The renderable material profile belongs to a different donor scene. " +
                "Recreate it after importing or preparing the current donor.");
        }
    }

    private static string ComputeSourceFingerprint(ImportedScene donor)
    {
        using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        AppendString(hash,
            TargetRigAutomaticPoseFitter.ComputeDonorGeometryFingerprint(donor));
        AppendInt(hash, donor.Materials.Count);
        foreach (ImportedMaterial material in donor.Materials)
        {
            AppendString(hash, material.Name);
            AppendString(hash, material.BaseColorTextureName ?? string.Empty);
            AppendInt(hash, material.BaseColorTextureIndex);
            AppendInt(hash, (int)material.AlphaMode);
            AppendInt(hash, BitConverter.SingleToInt32Bits(material.AlphaCutoff));
        }
        AppendInt(hash, donor.Textures.Count);
        foreach (ImportedTexture texture in donor.Textures)
        {
            AppendString(hash, texture.Name);
            AppendString(hash, texture.MimeType);
            AppendInt(hash, texture.Width);
            AppendInt(hash, texture.Height);
            hash.AppendData(SHA256.HashData(texture.Data));
        }
        return Convert.ToHexString(hash.GetHashAndReset());
    }

    private static void AppendString(IncrementalHash hash, string value)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(value ?? string.Empty);
        AppendInt(hash, bytes.Length);
        hash.AppendData(bytes);
    }

    private static void AppendInt(IncrementalHash hash, int value)
    {
        Span<byte> bytes = stackalloc byte[sizeof(int)];
        BinaryPrimitives.WriteInt32LittleEndian(bytes, value);
        hash.AppendData(bytes);
    }
}

/// <summary>
/// Native Sparkplug <c>FinalBlendOp</c> values accepted as opaque material
/// templates by production writers. Alpha is intentionally not synthesized
/// here: it is a complete consumer/graph contract rather than a flag or a
/// material-only conversion.
/// </summary>
internal static class SmoFinalBlendOperation
{
    public static bool IsKnownOpaque(uint operation) => operation is 0x0 or 0x2;
}

internal sealed class PaletteSearchLimitException(string message)
    : InvalidOperationException(message);

/// <summary>
/// Skinned GLB/FBX/OBJ transfer onto a native target skeleton. Every operation
/// reconstructs the visual graph and physically removes every old target mesh,
/// material and texture; only the native skeleton/service state is retained.
/// </summary>
public static class SmoSkinnedGlbReplacer
{
    public static GlbSkinTransferPlan Analyze(
        SmoDocument target,
        ImportedScene donor,
        SkinnedRenderableMaterialProfile? materialProfile = null,
        CancellationToken cancellationToken = default) =>
        SmoSkinnedVisualGraphPipeline.AnalyzeSkinnedModel(
            target, donor, materialProfile, cancellationToken);

    /// <summary>
    /// Builds the external-space visual equivalent of the geometry that
    /// <see cref="Replace"/> will serialize. This is intended for a static
    /// WYSIWYG preview: it performs the same bind-pose conversion as the writer
    /// without changing either the donor scene or the target document. Skinning
    /// is intentionally removed from the returned preview meshes so that the
    /// already-converted positions cannot accidentally be deformed a second time.
    /// </summary>
    public static ImportedScene PrepareGeometryPreview(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform transform,
        SkinnedGeometryTransferMode transferMode,
        SkinnedRenderableMaterialProfile? materialProfile = null) =>
        SmoSkinnedVisualGraphPipeline.PrepareSkinnedModelGeometryPreview(
            target, donor, transform, transferMode, materialProfile);

    /// <summary>
    /// Replaces the complete skinned visual graph with donor meshes, materials
    /// and separate texture resources.
    /// </summary>
    public static GlbSkinTransferResult Replace(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform transform,
        string outputPath,
        SkinnedGeometryTransferMode transferMode =
            SkinnedGeometryTransferMode.PreservePreparedGeometry,
        SkinnedRenderableMaterialProfile? materialProfile = null,
        CancellationToken cancellationToken = default) =>
        SmoSkinnedVisualGraphPipeline.RebuildSkinnedModel(
            target, donor, transform, outputPath, transferMode,
            materialProfile, cancellationToken);
}

internal static partial class SmoSkinnedVisualGraphPipeline
{
    private sealed record GlbPlanContext(
        GlbSkinTransferPlan PublicPlan,
        ImportedScene PreparedDonor,
        ImportedSkeleton Skeleton,
        IReadOnlyDictionary<string, string> BoneRemap,
        IReadOnlyDictionary<string, Matrix4x4> TargetBindWorld,
        IReadOnlyDictionary<string, Matrix4x4> TargetInverseBind,
        IReadOnlyList<ImportedGroup> SourceGroups);

    private sealed record ImportedGroup(
        int MaterialIndex,
        int SourceTextureIndex,
        string? ReferencedTextureName,
        IReadOnlyList<ImportedMesh> Meshes,
        int TriangleCount);

    private sealed record ImportedTransferMesh(
        int Key,
        string Name,
        Vector3[] Positions,
        Vector3[] Normals,
        Vector2[] TextureCoordinates,
        uint[] DiffuseColorsArgb,
        uint[] TriangleIndices,
        ImportedSkinning Skinning)
    {
        public Vector2[] SecondaryTextureCoordinates { get; init; } = [];
    }

    internal static GlbSkinTransferPlan AnalyzeSkinnedModel(
        SmoDocument target,
        ImportedScene donor,
        SkinnedRenderableMaterialProfile? materialProfile,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        materialProfile ??= SkinnedRenderableMaterialProfile.Default;
        GlbPlanContext context = BuildGlbPlan(
            target,
            donor,
            materialProfile,
            cancellationToken: cancellationToken);
        return context.PublicPlan;
    }

    internal static ImportedScene PrepareSkinnedModelGeometryPreview(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform transform,
        SkinnedGeometryTransferMode transferMode,
        SkinnedRenderableMaterialProfile? materialProfile)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        ArgumentNullException.ThrowIfNull(transform);
        if (!Enum.IsDefined(transferMode))
            throw new ArgumentOutOfRangeException(nameof(transferMode), transferMode, null);
        if (transferMode == SkinnedGeometryTransferMode.PreservePreparedGeometry &&
            !ApproximatelyEqual(transform.Matrix, Matrix4x4.Identity, 0.000001f))
        {
            throw new InvalidOperationException(
                "PreservePreparedGeometry requires an identity transform. " +
                "Scale, rotation and translation would move geometry without moving " +
                "the preserved target skeleton and animation pivots.");
        }

        materialProfile ??= SkinnedRenderableMaterialProfile.Default;
        GlbPlanContext context = BuildGlbPlan(
            target, donor, materialProfile);
        if (!context.PublicPlan.CanReplace)
        {
            throw new InvalidOperationException(
                "Skinned GLB is incompatible with the target SMO:\n" +
                string.Join("\n", context.PublicPlan.Messages.Select(message => "- " + message)));
        }

        donor = context.PreparedDonor;
        ImportedTransferMesh[] transferred = BuildImportedTransferMeshes(
            donor, context, transform, transferMode);
        ImportedMesh[] meshes = transferred
            .OrderBy(mesh => mesh.Key)
            .Select(mesh =>
            {
                ImportedMesh source = donor.Meshes[mesh.Key];
                return source with
                {
                    Positions = mesh.Positions,
                    Normals = mesh.Normals,
                    TextureCoordinates = mesh.TextureCoordinates,
                    TriangleIndices = mesh.TriangleIndices,
                    DiffuseColorsArgb = mesh.DiffuseColorsArgb,
                    Skinning = null
                };
            })
            .ToArray();
        return donor with { Meshes = meshes };
    }

    internal static GlbSkinTransferResult RebuildSkinnedModel(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform transform,
        string outputPath,
        SkinnedGeometryTransferMode transferMode,
        SkinnedRenderableMaterialProfile? materialProfile,
        CancellationToken cancellationToken = default)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        cancellationToken.ThrowIfCancellationRequested();
        materialProfile ??= SkinnedRenderableMaterialProfile.Default;

        GlbPlanContext context = BuildGlbPlan(
            target,
            donor,
            materialProfile,
            cancellationToken: cancellationToken);
        if (!context.PublicPlan.CanReplace)
            throw new InvalidOperationException(
                "Skinned GLB несовместим с target SMO:\n" +
                string.Join("\n", context.PublicPlan.Messages.Select(message => "• " + message)));

        if (!Enum.IsDefined(transferMode))
            throw new ArgumentOutOfRangeException(nameof(transferMode), transferMode, null);
        if (transferMode == SkinnedGeometryTransferMode.PreservePreparedGeometry &&
            !ApproximatelyEqual(transform.Matrix, Matrix4x4.Identity, 0.000001f))
        {
            throw new InvalidOperationException(
                "PreservePreparedGeometry requires an identity transform. " +
                "Scale, rotation and translation would move geometry without moving " +
                "the preserved target skeleton and animation pivots.");
        }

        donor = context.PreparedDonor;
        cancellationToken.ThrowIfCancellationRequested();
        ImportedTransferMesh[] transferMeshes = BuildImportedTransferMeshes(
                donor, context, transform, transferMode, cancellationToken)
            .Select(ConvertImportedMeshToSmoSpace)
            .ToArray();
        cancellationToken.ThrowIfCancellationRequested();
        return RebuildSkinnedVisualGraph(
            target,
            donor,
            context,
            transferMeshes,
            materialProfile,
            transferMode,
            outputPath,
            cancellationToken);

    }

    private static GlbPlanContext BuildGlbPlan(
        SmoDocument target,
        ImportedScene donor,
        SkinnedRenderableMaterialProfile materialProfile,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        ArgumentNullException.ThrowIfNull(materialProfile);
        SmoProductionPlatformGuard.EnsurePcWritable(target);
        var errors = new List<string>();
        var warnings = new List<string>();
        try
        {
            ValidateRenderableMaterialProfile(donor, materialProfile);
            if (materialProfile.HasExplicitOverrides)
            {
                donor = ApplyOpaqueOverlayTextureContract(donor, materialProfile);
                warnings.Add(
                    $"Applied {materialProfile.Overrides.Count(item => item.Mode != SkinnedRenderableMaterialMode.Auto)} " +
                    "explicit renderable material decision(s); Auto classification is unchanged " +
                    "for every other source mesh.");
            }
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          ArgumentException)
        {
            errors.Add("Renderable material profile is invalid: " + exception.Message);
        }
        if (donor.Meshes.Count == 0)
            errors.Add("GLB не содержит meshes.");
        if (donor.Meshes.Any(mesh => mesh.Skinning is null))
            errors.Add("Все GLB primitives должны содержать JOINTS_0/WEIGHTS_0 и ссылаться на skin.");
        ImportedSkeleton[] sourceSkeletons = donor.Meshes
            .Select(mesh => mesh.Skinning?.Skeleton)
            .Where(value => value is not null)
            .Cast<ImportedSkeleton>()
            .ToArray();
        if (sourceSkeletons.Length == 0)
        {
            errors.Add("GLB не содержит поддерживаемый skin.");
            return EmptyGlbPlan(donor, errors);
        }
        ImportedSkeleton skeleton = BuildCanonicalImportedSkeleton(
            sourceSkeletons, errors);

        HashSet<string> usedJoints = GetUsedImportedJoints(donor, errors);
        Dictionary<string, SmoObjectEntry> targetNodes = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Node &&
                !string.IsNullOrWhiteSpace(entry.Name))
            .GroupBy(entry => entry.Name, StringComparer.Ordinal)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single(),
                StringComparer.Ordinal);
        IReadOnlyDictionary<int, Matrix4x4> targetBindByIndex =
            SmoSkinBindingResolver.ResolveBindWorldMatrices(target);
        var targetBindSmo = targetBindByIndex
            .Where(item => target.Objects[item.Key].TypeHash == SmoClassIds.Node)
            .ToDictionary(item => target.Objects[item.Key].Name, item => item.Value,
                StringComparer.Ordinal);
        var targetBind = targetBindSmo.ToDictionary(
            item => item.Key,
            item => ReflectMatrix(item.Value),
            StringComparer.Ordinal);
        var targetInverse = new Dictionary<string, Matrix4x4>(StringComparer.Ordinal);
        foreach ((string name, Matrix4x4 bind) in targetBindSmo)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (Matrix4x4.Invert(bind, out Matrix4x4 inverse))
                targetInverse[name] = inverse;
        }

        var donorBind = new Dictionary<string, Matrix4x4>(StringComparer.Ordinal);
        for (int index = 0; index < skeleton.JointNames.Count; index++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (!Matrix4x4.Invert(
                    skeleton.InverseBindMatrices[index], out Matrix4x4 bind) || !IsFinite(bind))
                errors.Add($"Joint {skeleton.JointNames[index]} имеет необратимую inverse bind matrix.");
            else
                donorBind[skeleton.JointNames[index]] = bind;
        }

        var remap = new Dictionary<string, string>(StringComparer.Ordinal);
        var matched = new List<string>();
        var remapped = new List<GlbBoneRemap>();
        foreach (string donorName in usedJoints.Order(StringComparer.Ordinal))
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (targetBind.ContainsKey(donorName))
            {
                remap[donorName] = donorName;
                matched.Add(donorName);
                continue;
            }
            if (!donorBind.TryGetValue(donorName, out Matrix4x4 donorMatrix) ||
                targetBind.Count == 0)
            {
                errors.Add($"Для служебной GLB-кости \"{donorName}\" не найден bind-space fallback.");
                continue;
            }
            string? semanticFallback = donorName switch
            {
                "C-lowerRoot" or "neutral_bone" when targetBind.ContainsKey("Pelvis") => "Pelvis",
                "C-upperRoot" when targetBind.ContainsKey("Spine_01") => "Spine_01",
                _ => null
            };
            if (semanticFallback is null)
            {
                string reason = targetNodes.ContainsKey(donorName)
                    ? "кость есть в target graph, но отсутствует в deform-палитрах"
                    : "кость отсутствует в target graph";
                errors.Add(
                    $"Для GLB-кости \"{donorName}\" нет безопасного автоматического соответствия: {reason}.");
                continue;
            }
            string fallback = semanticFallback;
            remap[donorName] = fallback;
            remapped.Add(new GlbBoneRemap(
                donorName, fallback,
                "служебный root joint; подтверждённая deform-пара"));
        }

        int differentBindPose = 0;
        foreach ((string donorName, string targetName) in remap)
            if (donorBind.TryGetValue(donorName, out Matrix4x4 donorMatrix) &&
                targetBind.TryGetValue(targetName, out Matrix4x4 targetMatrix) &&
                !ApproximatelyEqual(donorMatrix, targetMatrix, 0.01f))
                differentBindPose++;
        if (differentBindPose > 0)
            warnings.Add(
                $"У {differentBindPose} активных joints отличается bind pose. " +
                "PreservePreparedGeometry оставит подготовленную позу без изменений; " +
                "RetargetToGameBindPose явно переведёт geometry в bind pose игры.");
        if (remapped.Count > 0)
            warnings.Add(
                $"{remapped.Count} служебных joints будут перенаправлены: " +
                string.Join(", ", remapped.Select(item =>
                    $"{item.DonorBoneName}→{item.TargetBoneName}")) + ".");

        TargetVisualGroup[] targetGroups;
        try
        {
            targetGroups = ReadTargetVisualGroups(
                target, SmoTextureBindingResolver.ResolveAll(target));
        }
        catch (Exception exception) when (exception is InvalidDataException or InvalidOperationException)
        {
            errors.Add("Target visual graph не поддерживается: " + exception.Message);
            targetGroups = [];
        }
        ImportedGroup[] donorGroups;
        var alphaBlendTextureIndices = new HashSet<int>();
        ValidateImportedTextureIndices(donor, errors);
        donorGroups = BuildImportedGroups(donor, warnings);
        int[] referencedTextureIndices = donorGroups
            .Select(group => group.SourceTextureIndex)
            .Where(index => index >= 0)
            .Distinct()
            .Order()
            .ToArray();
        foreach (int textureIndex in referencedTextureIndices)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if ((uint)textureIndex >= (uint)donor.Textures.Count)
                continue;
            try
            {
                if (!ImportedTextureImageTools.TextureContainsTransparency(
                        donor.Textures[textureIndex]))
                {
                    continue;
                }
                alphaBlendTextureIndices.Add(textureIndex);
            }
            catch (Exception exception) when (exception is InvalidDataException or
                                              UnknownImageFormatException or
                                              InvalidImageContentException or
                                              NotSupportedException)
            {
                errors.Add(
                    $"Cannot inspect donor texture {textureIndex} " +
                    $"'{donor.Textures[textureIndex].Name}' for transparency: " +
                    exception.Message);
            }
        }
        if (alphaBlendTextureIndices.Count > 0)
        {
            warnings.Add(
                $"Detected real transparency in {alphaBlendTextureIndices.Count} " +
                "donor base-color texture(s). Their RGBA pixels will be transferred, " +
                "and only UV-connected geometry that samples transparent texels will " +
                "use native alpha material/spSkin branches in Auto mode.");
        }

        if (errors.Count == 0)
        {
            if (targetGroups.Length == 0)
            {
                errors.Add(
                    $"GLB material groups ({donorGroups.Length}) cannot be assigned: " +
                    "the target has no skinned visual groups.");
            }
            else if (donorGroups.Length == 0)
            {
                errors.Add("The donor has no material groups to assign.");
            }
            else
            {
                try
                {
                    ValidateNativeImportedGroupAppend(
                        target,
                        donor,
                        skeleton,
                        remap,
                        targetBind,
                        targetInverse,
                        targetGroups,
                        donorGroups,
                        materialProfile,
                        cancellationToken);
                    warnings.Add(
                        $"All {donorGroups.Length} donor material groups will be " +
                        "written as independent native TextureData/material/spSkin " +
                        "branches. The old target MeshData, spMaterialData and " +
                        "TextureData resources will be physically removed; source " +
                        "UVs and texture files remain separate.");
                }
                catch (PaletteSearchLimitException)
                {
                    throw;
                }
                catch (Exception exception) when (exception is InvalidDataException or
                                                  InvalidOperationException or
                                                  NotSupportedException or
                                                  OverflowException)
                {
                    errors.Add(
                        "Clean native per-texture graph rebuild failed: " +
                        exception.Message);
                }
            }
        }

        string[] unused = skeleton.JointNames.Except(usedJoints, StringComparer.Ordinal)
            .Order(StringComparer.Ordinal).ToArray();
        string[] unweightedTarget = targetBind.Keys
            .Except(remap.Values, StringComparer.Ordinal)
            .Order(StringComparer.Ordinal).ToArray();
        if (unused.Length > 0)
            warnings.Add($"{unused.Length} GLB joints не имеют активных weights и не участвуют в переносе.");
        SmoSkeletonCompatibility compatibility = errors.Count > 0
            ? SmoSkeletonCompatibility.Incompatible
            : warnings.Count > 0
                ? SmoSkeletonCompatibility.CompatibleWithWarnings
                : SmoSkeletonCompatibility.Exact;
        var publicPlan = new GlbSkinTransferPlan(
            compatibility,
            donor.Meshes.Count,
            donorGroups.Length,
            skeleton.JointNames.Count,
            usedJoints.Count,
            differentBindPose,
            matched,
            remapped,
            unused,
            unweightedTarget,
            errors.Concat(warnings).ToArray());
        return new GlbPlanContext(
            publicPlan,
            donor,
            skeleton,
            remap,
            targetBind,
            targetInverse,
            donorGroups);
    }

    private static ImportedSkeleton BuildCanonicalImportedSkeleton(
        IReadOnlyList<ImportedSkeleton> sourceSkeletons,
        ICollection<string> errors)
    {
        ImportedSkeleton[] uniqueSources = sourceSkeletons
            .Distinct<ImportedSkeleton>(ReferenceEqualityComparer.Instance)
            .ToArray();
        if (uniqueSources.Length == 1 &&
            uniqueSources[0].JointNames.Count ==
            uniqueSources[0].InverseBindMatrices.Count)
        {
            // Keep the hierarchy/bind metadata supplied by GlbModelReader when
            // every primitive already shares one skeleton object.
            return uniqueSources[0];
        }

        var slotsByName = new Dictionary<string, int>(StringComparer.Ordinal);
        var names = new List<string>();
        var inverseBindMatrices = new List<Matrix4x4>();
        foreach (ImportedSkeleton source in uniqueSources)
        {
            if (source.JointNames.Count != source.InverseBindMatrices.Count)
            {
                errors.Add(
                    $"Skin {source.Name} has {source.JointNames.Count} joint names but " +
                    $"{source.InverseBindMatrices.Count} inverse-bind matrices.");
                continue;
            }

            for (int joint = 0; joint < source.JointNames.Count; joint++)
            {
                string name = source.JointNames[joint];
                Matrix4x4 inverseBind = source.InverseBindMatrices[joint];
                if (string.IsNullOrWhiteSpace(name))
                {
                    errors.Add($"Skin {source.Name} contains an unnamed joint at slot {joint}.");
                    continue;
                }
                if (slotsByName.TryGetValue(name, out int canonicalSlot))
                {
                    if (!ApproximatelyEqual(
                            inverseBindMatrices[canonicalSlot], inverseBind, 0.0001f))
                    {
                        errors.Add(
                            $"Joint {name} is shared by several skins with conflicting " +
                            "inverse-bind matrices.");
                    }
                    continue;
                }

                slotsByName.Add(name, names.Count);
                names.Add(name);
                inverseBindMatrices.Add(inverseBind);
            }
        }

        return new ImportedSkeleton(
            uniqueSources.Length == 1 ? uniqueSources[0].Name : "canonical_scene_skin",
            names,
            inverseBindMatrices);
    }

    private static GlbPlanContext EmptyGlbPlan(
        ImportedScene donor,
        IReadOnlyList<string> errors)
    {
        var emptySkeleton = new ImportedSkeleton("missing", [], []);
        return new GlbPlanContext(
            new GlbSkinTransferPlan(
                SmoSkeletonCompatibility.Incompatible,
                donor.Meshes.Count, 0, 0, 0, 0,
                [], [], [], [], errors.ToArray()),
            donor,
            emptySkeleton,
            new Dictionary<string, string>(),
            new Dictionary<string, Matrix4x4>(),
            new Dictionary<string, Matrix4x4>(),
            []);
    }

    private static ImportedGroup BuildImportedGroup(
        ImportedScene donor,
        IEnumerable<ImportedMesh> sourceMeshes)
    {
        ImportedMesh[] meshes = sourceMeshes.ToArray();
        ImportedMesh first = meshes[0];
        int sourceTextureIndex = GetImportedVisualGroupTextureIndex(donor, first);
        return new ImportedGroup(
            meshes.Min(mesh => mesh.MaterialIndex),
            sourceTextureIndex,
            sourceTextureIndex == -1
                ? GetImportedVisualGroupTextureName(donor, first)
                : null,
            meshes,
            meshes.Sum(mesh => mesh.TriangleIndices.Length / 3));
    }

    private static ImportedGroup[] BuildImportedGroups(
        ImportedScene donor,
        ICollection<string> warnings)
    {
        IGrouping<string, ImportedMesh>[] groupedDonorMeshes = donor.Meshes
            .GroupBy(
                mesh => GetImportedVisualGroupKey(donor, mesh),
                StringComparer.OrdinalIgnoreCase)
            .ToArray();
        foreach (IGrouping<string, ImportedMesh> group in groupedDonorMeshes)
        {
            int[] materialIndices = group.Select(mesh => mesh.MaterialIndex)
                .Distinct()
                .Order()
                .ToArray();
            string? sharedTextureName = GetImportedVisualGroupTextureDisplayName(
                donor, group.First());
            if (sharedTextureName is not null && materialIndices.Length > 1)
            {
                warnings.Add(
                    $"Material states {string.Join(", ", materialIndices)} use the shared source image " +
                    $"\"{sharedTextureName}\" and are merged into one primary visual group; " +
                    "the target primary material state is preserved.");
            }
        }
        return groupedDonorMeshes
            .Select(group => BuildImportedGroup(donor, group))
            .OrderByDescending(group => group.TriangleCount)
            .ThenBy(group => group.MaterialIndex)
            .ToArray();
    }

    private static void ValidateNativeImportedGroupAppend(
        SmoDocument target,
        ImportedScene donor,
        ImportedSkeleton skeleton,
        IReadOnlyDictionary<string, string> boneRemap,
        IReadOnlyDictionary<string, Matrix4x4> targetBind,
        IReadOnlyDictionary<string, Matrix4x4> targetInverse,
        IReadOnlyList<TargetVisualGroup> targetGroups,
        IReadOnlyList<ImportedGroup> donorGroups,
        SkinnedRenderableMaterialProfile materialProfile,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        TargetVisualGroup templateGroup =
            OrderTargetGroupsByPreference(target, targetGroups)[0];
        uint textureTemplateObjectId = target.Objects[templateGroup.TextureObjectIndex].Id;
        var dryContext = new GlbPlanContext(
            new GlbSkinTransferPlan(
                SmoSkeletonCompatibility.Exact,
                donor.Meshes.Count,
                donorGroups.Count,
                skeleton.JointNames.Count,
                0,
                0,
                [],
                [],
                [],
                [],
                []),
            donor,
            skeleton,
            boneRemap,
            targetBind,
            targetInverse,
            donorGroups);
        ImportedTransferMesh[] dryMeshes = BuildImportedTransferMeshes(
                donor,
                dryContext,
                ReplacementTransform.Identity,
                SkinnedGeometryTransferMode.PreservePreparedGeometry,
                cancellationToken)
            .Select(ConvertImportedMeshToSmoSpace)
            .ToArray();

        foreach (ImportedGroup group in donorGroups)
        {
            cancellationToken.ThrowIfCancellationRequested();
            ImportedTexture sourceTexture = ResolveImportedGroupTexture(donor, group) ??
                throw new InvalidDataException(
                    $"Material group {group.MaterialIndex} has no donor texture for " +
                    "an independent native TextureData branch.");
            SmoSkinnedBranchSourceMesh[] meshes = group.Meshes
                .Select(mesh => dryMeshes.Single(value => value.Key ==
                    GetImportedMeshIndex(donor.Meshes, mesh)))
                .Select(ToSkinnedBranchSourceMesh)
                .ToArray();
            SmoSkinnedRenderableOpacityPlan opacity =
                SmoSkinnedBranchSplitBuilder.ClassifyRenderables(
                    meshes, sourceTexture, materialProfile);
            meshes = SmoSkinnedBranchSplitBuilder
                .ConformCloseSurfaceNormals(meshes, opacity)
                .Meshes;
            _ = SmoSkinnedBranchSplitBuilder.Analyze(
                target,
                textureTemplateObjectId,
                meshes,
                opacity,
                boneRemap,
                targetInverse,
                includeOpaqueBody: true);
        }
    }

    private static TargetVisualGroup[] OrderTargetGroupsByPreference(
        SmoDocument target,
        IReadOnlyList<TargetVisualGroup> targetGroups)
    {
        TargetVisualGroup[] ordered = targetGroups
            .OrderByDescending(group => group.Meshes.Sum(mesh => mesh.Mesh.TriangleCount))
            .ThenBy(group => group.TextureObjectIndex)
            .ToArray();
        TargetVisualGroup primary = ordered
            .FirstOrDefault(group => !IsRigidTargetGroup(target, group)) ??
            ordered[0];
        return
        [
            primary,
            .. ordered.Where(group =>
                group.TextureObjectIndex != primary.TextureObjectIndex)
        ];
    }

    private static void ValidateRenderableMaterialProfile(
        ImportedScene donor,
        SkinnedRenderableMaterialProfile materialProfile)
    {
        materialProfile.ValidateSource(donor);
        foreach (SkinnedRenderableMaterialOverride item in materialProfile.Overrides)
        {
            if ((uint)item.SourceMeshKey >= (uint)donor.Meshes.Count)
            {
                throw new InvalidDataException(
                    $"Source mesh key {item.SourceMeshKey} is outside the donor mesh " +
                    $"catalog (count {donor.Meshes.Count}).");
            }
        }
    }

    /// <summary>
    /// Makes the alpha channel of an explicitly opaque source texture group
    /// fully opaque before texture preparation. This preserves RGB
    /// authored under zero alpha (notably opaque face-patch backgrounds).
    /// A mixed-mode texture group is rejected because mutating its shared alpha
    /// would silently change another renderable's material contract.
    /// </summary>
    private static ImportedScene ApplyOpaqueOverlayTextureContract(
        ImportedScene donor,
        SkinnedRenderableMaterialProfile materialProfile)
    {
        int[] opaqueKeys = materialProfile.Overrides
            .Where(item => item.Mode == SkinnedRenderableMaterialMode.OpaqueOverlay)
            .Select(item => item.SourceMeshKey)
            .ToArray();
        if (opaqueKeys.Length == 0)
            return donor;

        var forcedTextureIndices = new HashSet<int>();
        var sourceMeshes = donor.Meshes
            .Select((mesh, key) => (Mesh: mesh, Key: key))
            .ToArray();
        foreach (IGrouping<string, (ImportedMesh Mesh, int Key)> group in sourceMeshes
                     .GroupBy(
                         item => GetImportedVisualGroupKey(donor, item.Mesh),
                         StringComparer.OrdinalIgnoreCase))
        {
            bool anyOpaque = group.Any(item =>
                materialProfile.GetMode(item.Key) ==
                SkinnedRenderableMaterialMode.OpaqueOverlay);
            if (!anyOpaque)
                continue;
            if (group.Any(item => materialProfile.GetMode(item.Key) !=
                                  SkinnedRenderableMaterialMode.OpaqueOverlay))
            {
                throw new InvalidOperationException(
                    "OpaqueOverlay can only force alpha for a source texture group " +
                    "whose every renderable is explicitly OpaqueOverlay; group " +
                    $"'{group.Key}' mixes source mesh keys " +
                    string.Join(", ", group.Select(item => item.Key).Order()) + ".");
            }

            ImportedGroup importedGroup = BuildImportedGroup(
                donor, group.Select(item => item.Mesh));
            ImportedTexture sourceTexture = ResolveImportedGroupTexture(
                donor, importedGroup) ?? throw new InvalidDataException(
                $"OpaqueOverlay texture group '{group.Key}' has no source image.");
            int textureIndex = importedGroup.SourceTextureIndex;
            if (textureIndex < 0)
            {
                int[] matches = donor.Textures
                    .Select((texture, index) => (texture, index))
                    .Where(item => ReferenceEquals(item.texture, sourceTexture) ||
                                   item.texture.Equals(sourceTexture))
                    .Select(item => item.index)
                    .ToArray();
                if (matches.Length != 1)
                {
                    throw new InvalidDataException(
                        $"OpaqueOverlay texture group '{group.Key}' does not resolve " +
                        "to one donor texture catalog entry.");
                }
                textureIndex = matches[0];
            }
            if ((uint)textureIndex >= (uint)donor.Textures.Count)
            {
                throw new InvalidDataException(
                    $"OpaqueOverlay texture group '{group.Key}' references invalid " +
                    $"texture index {textureIndex}.");
            }
            forcedTextureIndices.Add(textureIndex);
        }

        ImportedTexture[] textures = donor.Textures.ToArray();
        foreach (int textureIndex in forcedTextureIndices.Order())
        {
            ImportedTexture source = textures[textureIndex];
            using Image<Rgba32> image = Image.Load<Rgba32>(source.Data);
            if (image.Width != source.Width || image.Height != source.Height)
            {
                throw new InvalidDataException(
                    $"Texture {source.Name} declares {source.Width}x{source.Height}, " +
                    $"but its image is {image.Width}x{image.Height}.");
            }
            image.ProcessPixelRows(accessor =>
            {
                for (int y = 0; y < accessor.Height; y++)
                {
                    Span<Rgba32> row = accessor.GetRowSpan(y);
                    for (int x = 0; x < row.Length; x++)
                        row[x].A = byte.MaxValue;
                }
            });
            using var encoded = new MemoryStream();
            image.SaveAsPng(encoded, new PngEncoder
            {
                ColorType = PngColorType.RgbWithAlpha
            });
            textures[textureIndex] = source with { Data = encoded.ToArray() };
        }
        return donor with { EmbeddedTextures = Array.AsReadOnly(textures) };
    }

    private static void ValidateImportedTextureIndices(
        ImportedScene donor,
        ICollection<string> errors)
    {
        foreach (int materialIndex in donor.Meshes
                     .Select(mesh => mesh.MaterialIndex)
                     .Where(index => (uint)index < (uint)donor.Materials.Count)
                     .Distinct()
                     .Order())
        {
            ImportedMaterial material = donor.Materials[materialIndex];
            int textureIndex = material.BaseColorTextureIndex;
            if (textureIndex >= -1 && textureIndex < donor.Textures.Count)
                continue;
            errors.Add(
                $"GLB material [{materialIndex}] {material.Name} references invalid " +
                $"base-color texture index {textureIndex}; donor texture count is " +
                $"{donor.Textures.Count}.");
        }
    }

    private static string GetImportedVisualGroupKey(
        ImportedScene donor,
        ImportedMesh mesh)
    {
        int textureIndex = GetImportedVisualGroupTextureIndex(donor, mesh);
        if (textureIndex >= 0)
        {
            return "texture-index:" + textureIndex.ToString(
                System.Globalization.CultureInfo.InvariantCulture);
        }
        if (textureIndex < -1)
        {
            return "invalid-texture-index:" + textureIndex.ToString(
                System.Globalization.CultureInfo.InvariantCulture) +
                ":material:" + mesh.MaterialIndex.ToString(
                    System.Globalization.CultureInfo.InvariantCulture);
        }

        string? textureName = GetImportedVisualGroupTextureName(donor, mesh);
        return textureName is null
            ? "material:" + mesh.MaterialIndex.ToString(
                System.Globalization.CultureInfo.InvariantCulture)
            : "referenced-texture-name:" + textureName;
    }

    private static int GetImportedVisualGroupTextureIndex(
        ImportedScene donor,
        ImportedMesh mesh) =>
        (uint)mesh.MaterialIndex < (uint)donor.Materials.Count
            ? donor.Materials[mesh.MaterialIndex].BaseColorTextureIndex
            : -1;

    private static string? GetImportedVisualGroupTextureName(
        ImportedScene donor,
        ImportedMesh mesh)
    {
        if ((uint)mesh.MaterialIndex >= (uint)donor.Materials.Count)
            return null;
        ImportedMaterial material = donor.Materials[mesh.MaterialIndex];
        if (material.BaseColorTextureIndex != -1 ||
            string.IsNullOrWhiteSpace(material.BaseColorTextureName))
        {
            return null;
        }
        string sourceName = material.BaseColorTextureName.Trim();
        string fileName = Path.GetFileName(sourceName);
        return string.IsNullOrWhiteSpace(fileName) ? sourceName : fileName;
    }

    private static string? GetImportedVisualGroupTextureDisplayName(
        ImportedScene donor,
        ImportedMesh mesh)
    {
        int textureIndex = GetImportedVisualGroupTextureIndex(donor, mesh);
        if ((uint)textureIndex < (uint)donor.Textures.Count)
            return donor.Textures[textureIndex].Name;
        return textureIndex == -1
            ? GetImportedVisualGroupTextureName(donor, mesh)
            : null;
    }

    private static HashSet<string> GetUsedImportedJoints(
        ImportedScene donor,
        List<string> errors)
    {
        var result = new HashSet<string>(StringComparer.Ordinal);
        foreach (ImportedMesh mesh in donor.Meshes)
        {
            ImportedSkinning? skinning = mesh.Skinning;
            if (skinning is null)
                continue;
            if (skinning.JointIndices.Length != mesh.Positions.Length ||
                skinning.Weights.Length != mesh.Positions.Length)
            {
                errors.Add($"Mesh {mesh.Name}: skin attribute count differs from POSITION.");
                continue;
            }
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
                Vector4 weights = skinning.Weights[vertex];
                ImportedJointIndices joints = skinning.JointIndices[vertex];
                float total = 0;
                Add(weights.X, joints.X);
                Add(weights.Y, joints.Y);
                Add(weights.Z, joints.Z);
                Add(weights.W, joints.W);
                if (!float.IsFinite(total) || total <= WeightEpsilon)
                    errors.Add($"Mesh {mesh.Name} vertex {vertex} has no valid positive weights.");

                void Add(float weight, ushort joint)
                {
                    if (!float.IsFinite(weight) || weight < 0)
                    {
                        total = float.NaN;
                        return;
                    }
                    if (weight <= WeightEpsilon)
                        return;
                    total += weight;
                    if (joint >= skinning.Skeleton.JointNames.Count)
                        errors.Add($"Mesh {mesh.Name} vertex {vertex} uses invalid joint {joint}.");
                    else
                        result.Add(skinning.Skeleton.JointNames[joint]);
                }
            }
        }
        return result;
    }

    private static ImportedTransferMesh[] BuildImportedTransferMeshes(
        ImportedScene donor,
        GlbPlanContext context,
        ReplacementTransform transform,
        SkinnedGeometryTransferMode transferMode,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        Matrix4x4 global = transform.Matrix;
        Matrix4x4 normalGlobal = Matrix4x4.Invert(global, out Matrix4x4 inverseGlobal)
            ? Matrix4x4.Transpose(inverseGlobal)
            : global;
        return donor.Meshes.Select((mesh, meshIndex) =>
        {
            cancellationToken.ThrowIfCancellationRequested();
            ImportedSkinning skinning = mesh.Skinning ?? throw new InvalidOperationException(
                $"Mesh {mesh.Name} has no skinning data.");
            Vector3[] normals = mesh.Normals.Length == mesh.Positions.Length
                ? mesh.Normals.ToArray()
                : GenerateImportedSmoothNormals(mesh);
            var positions = new Vector3[mesh.Positions.Length];
            var transferredNormals = new Vector3[mesh.Positions.Length];
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
                if ((vertex & 0x0FFF) == 0)
                    cancellationToken.ThrowIfCancellationRequested();
                Vector3 position = mesh.Positions[vertex];
                Vector3 normal = normals[vertex];
                if (transferMode == SkinnedGeometryTransferMode.RetargetToGameBindPose)
                {
                    position = Vector3.Zero;
                    normal = Vector3.Zero;
                    Vector4 weights = skinning.Weights[vertex];
                    ImportedJointIndices joints = skinning.JointIndices[vertex];
                    float weightTotal = Positive(weights.X) + Positive(weights.Y) +
                        Positive(weights.Z) + Positive(weights.W);
                    if (!float.IsFinite(weightTotal) || weightTotal <= WeightEpsilon)
                        throw new InvalidDataException(
                            $"Mesh {mesh.Name} vertex {vertex} has no valid positive weights.");
                    Add(weights.X, joints.X);
                    Add(weights.Y, joints.Y);
                    Add(weights.Z, joints.Z);
                    Add(weights.W, joints.W);

                    void Add(float weight, ushort jointIndex)
                    {
                        if (weight <= WeightEpsilon)
                            return;
                        weight /= weightTotal;
                        string donorName = skinning.Skeleton.JointNames[jointIndex];
                        string targetName = context.BoneRemap[donorName];
                        Matrix4x4 rebase = skinning.Skeleton.InverseBindMatrices[jointIndex] *
                            context.TargetBindWorld[targetName];
                        position += Vector3.Transform(mesh.Positions[vertex], rebase) * weight;
                        Matrix4x4 normalMatrix = Matrix4x4.Invert(
                                rebase, out Matrix4x4 inverseRebase)
                            ? Matrix4x4.Transpose(inverseRebase)
                            : rebase;
                        normal += Vector3.TransformNormal(normals[vertex], normalMatrix) * weight;
                    }

                    static float Positive(float value) =>
                        float.IsFinite(value) && value > WeightEpsilon ? value : 0f;
                }
                if (transferMode == SkinnedGeometryTransferMode.PreservePreparedGeometry)
                {
                    // Preserve means preserve: donor inverse-bind matrices and
                    // node rest transforms must have no effect on authored mesh
                    // attributes. The identity-transform precondition above makes
                    // these assignments bit-preserving for supplied normals.
                    positions[vertex] = position;
                    transferredNormals[vertex] = normal;
                }
                else
                {
                    positions[vertex] = Vector3.Transform(position, global);
                    Vector3 transformedNormal = Vector3.TransformNormal(normal, normalGlobal);
                    transferredNormals[vertex] = transformedNormal.LengthSquared() > 1e-20f
                        ? Vector3.Normalize(transformedNormal)
                        : Vector3.UnitY;
                }
            }
            return new ImportedTransferMesh(
                meshIndex,
                mesh.Name,
                positions,
                transferredNormals,
                mesh.TextureCoordinates,
                mesh.DiffuseColors,
                mesh.TriangleIndices,
                skinning)
            {
                SecondaryTextureCoordinates = mesh.SecondaryTextureCoordinates
            };
        }).ToArray();
    }

    private static ImportedTransferMesh ConvertImportedMeshToSmoSpace(
        ImportedTransferMesh mesh)
    {
        Vector3[] positions = mesh.Positions
            .Select(value => new Vector3(value.X, value.Y, -value.Z))
            .ToArray();
        Vector3[] normals = mesh.Normals
            .Select(value => new Vector3(value.X, value.Y, -value.Z))
            .ToArray();
        uint[] triangles = mesh.TriangleIndices.ToArray();
        for (int index = 0; index < triangles.Length; index += 3)
        {
            (triangles[index + 1], triangles[index + 2]) =
                (triangles[index + 2], triangles[index + 1]);
        }
        return mesh with
        {
            Positions = positions,
            Normals = normals,
            TriangleIndices = triangles
        };
    }

    private static SmoSkinnedBranchSourceMesh ToSkinnedBranchSourceMesh(
        ImportedTransferMesh mesh) => new(
        mesh.Key,
        mesh.Name,
        mesh.Positions,
        mesh.Normals,
        mesh.TextureCoordinates,
        mesh.DiffuseColorsArgb,
        mesh.TriangleIndices,
        mesh.Skinning)
    {
        SecondaryTextureCoordinates = mesh.SecondaryTextureCoordinates
    };

    private static Matrix4x4 ReflectMatrix(Matrix4x4 value)
    {
        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        return reflection * value * reflection;
    }

    private static uint? GetParentObjectId(
        SmoDocument document,
        SmoObjectEntry entry) => entry.ParentIndex is int parentIndex
        ? document.Objects[parentIndex].Id
        : null;

    private static void VerifyTargetNodeInvariants(
        SmoDocument target,
        SmoDocument output,
        ICollection<string> errors,
        IReadOnlySet<uint>? removedTargetObjectIds = null)
    {
        removedTargetObjectIds ??= new HashSet<uint>();
        string[] targetLinks = SmoNodeHierarchy.Decode(target).Links
            .Where(link =>
                !removedTargetObjectIds.Contains(
                    target.Objects[link.ParentObjectIndex].Id) &&
                !removedTargetObjectIds.Contains(link.ChildObjectId))
            .Select(link =>
                $"{target.Objects[link.ParentObjectIndex].Id}:{link.ChildObjectId}")
            .Order(StringComparer.Ordinal)
            .ToArray();
        string[] outputLinks = SmoNodeHierarchy.Decode(output).Links
            .Select(link =>
                $"{output.Objects[link.ParentObjectIndex].Id}:{link.ChildObjectId}")
            .Order(StringComparer.Ordinal)
            .ToArray();
        if (!targetLinks.SequenceEqual(outputLinks, StringComparer.Ordinal))
            errors.Add("target node hierarchy links changed");
        Dictionary<uint, SmoObjectEntry> outputById = output.Objects
            .ToDictionary(entry => entry.Id);

        foreach (SmoObjectEntry targetEntry in target.Objects.Where(entry =>
                     (entry.TypeHash is SmoClassIds.Node or
                         SmoClassIds.RenderNode or SmoClassIds.Model) &&
                     !removedTargetObjectIds.Contains(entry.Id)))
        {
            if (!outputById.TryGetValue(
                    targetEntry.Id, out SmoObjectEntry? outputEntry))
            {
                errors.Add(
                    $"target node ID {targetEntry.Id} {targetEntry.Name} is missing");
                continue;
            }
            bool hasTargetTransform = SmoNodeTransformDecoder.TryDecode(
                target, targetEntry, out SmoNodeTransform? targetTransform);
            bool hasOutputTransform = SmoNodeTransformDecoder.TryDecode(
                output, outputEntry, out SmoNodeTransform? outputTransform);
            if (hasTargetTransform != hasOutputTransform ||
                hasTargetTransform &&
                (targetTransform is null || outputTransform is null ||
                 !ApproximatelyEqual(
                     targetTransform.LocalMatrix,
                     outputTransform.LocalMatrix,
                     0.000001f)))
            {
                errors.Add(
                    $"target node [{targetEntry.Index}] {targetEntry.Name} TRS changed");
            }
        }

        foreach (SmoObjectEntry targetEntry in target.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.StaticRenderObject &&
                     !removedTargetObjectIds.Contains(entry.Id)))
        {
            if (!outputById.TryGetValue(
                    targetEntry.Id, out SmoObjectEntry? outputEntry))
            {
                errors.Add(
                    $"target static render object ID {targetEntry.Id} is missing");
                continue;
            }
            bool hasTargetTransform = SmoStaticRenderObjectTransformDecoder.TryDecode(
                target, targetEntry, out Matrix4x4 targetTransform);
            bool hasOutputTransform = SmoStaticRenderObjectTransformDecoder.TryDecode(
                output, outputEntry, out Matrix4x4 outputTransform);
            if (hasTargetTransform != hasOutputTransform ||
                hasTargetTransform && !ApproximatelyEqual(
                    targetTransform, outputTransform, 0.000001f))
            {
                errors.Add(
                    $"target static render object [{targetEntry.Index}] transform changed");
            }
        }
    }

    private static ImportedTexture? ResolveImportedGroupTexture(
        ImportedScene donor,
        ImportedGroup group)
    {
        if ((uint)group.SourceTextureIndex < (uint)donor.Textures.Count)
            return donor.Textures[group.SourceTextureIndex];
        if (group.SourceTextureIndex != -1)
        {
            throw new InvalidDataException(
                $"Material group {group.MaterialIndex} references invalid base-color " +
                $"texture index {group.SourceTextureIndex}; donor texture count is " +
                $"{donor.Textures.Count}.");
        }
        if (string.IsNullOrWhiteSpace(group.ReferencedTextureName))
            return null;

        string referencedName = NormalizeImportedTextureName(group.ReferencedTextureName);
        int[] exactMatches = donor.Textures
            .Select((texture, index) => (texture, index))
            .Where(item => string.Equals(
                NormalizeImportedTextureName(item.texture.Name),
                referencedName,
                StringComparison.OrdinalIgnoreCase))
            .Select(item => item.index)
            .ToArray();
        if (exactMatches.Length == 1)
            return donor.Textures[exactMatches[0]];
        if (exactMatches.Length > 1)
        {
            throw new InvalidDataException(
                $"Base-color texture name \"{group.ReferencedTextureName}\" for " +
                $"material group {group.MaterialIndex} is ambiguous in the donor texture catalog.");
        }

        string referencedStem = Path.GetFileNameWithoutExtension(referencedName);
        int[] stemMatches = donor.Textures
            .Select((texture, index) => (texture, index))
            .Where(item => string.Equals(
                Path.GetFileNameWithoutExtension(
                    NormalizeImportedTextureName(item.texture.Name)),
                referencedStem,
                StringComparison.OrdinalIgnoreCase))
            .Select(item => item.index)
            .ToArray();
        if (stemMatches.Length == 1)
            return donor.Textures[stemMatches[0]];
        if (stemMatches.Length > 1)
        {
            throw new InvalidDataException(
                $"Base-color texture name \"{group.ReferencedTextureName}\" for " +
                $"material group {group.MaterialIndex} is ambiguous in the donor texture catalog.");
        }
        throw new InvalidDataException(
            $"Base-color texture \"{group.ReferencedTextureName}\" for material " +
            $"group {group.MaterialIndex} was not found in the donor texture catalog.");
    }

    private static string NormalizeImportedTextureName(string name)
    {
        string trimmed = name.Trim();
        string fileName = Path.GetFileName(trimmed);
        return string.IsNullOrWhiteSpace(fileName) ? trimmed : fileName;
    }

    private static void VerifyTargetPaletteBindMatrices(
        SmoDocument target,
        SmoDocument output,
        ICollection<string> errors)
    {
        IReadOnlyDictionary<int, Matrix4x4> targetBindWorld =
            SmoSkinBindingResolver.ResolveBindWorldMatrices(target);
        var canonicalInverse = new Dictionary<uint, Matrix4x4>();
        foreach ((int nodeIndex, Matrix4x4 bindWorld) in targetBindWorld)
        {
            if (Matrix4x4.Invert(bindWorld, out Matrix4x4 inverseBind) &&
                IsFinite(inverseBind))
            {
                canonicalInverse[target.Objects[nodeIndex].Id] = inverseBind;
            }
        }

        Dictionary<uint, Matrix4x4[]> originalMatrices = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .Select(entry => SmoSkinDecoder.TryDecode(
                target, entry, out SmoSkin? skin, out _) ? skin : null)
            .Where(skin => skin is not null)
            .SelectMany(skin => skin!.Bones)
            .GroupBy(bone => target.Objects[bone.NodeObjectIndex].Id)
            .ToDictionary(
                group => group.Key,
                group => group.Select(bone => bone.InverseBindMatrix).ToArray());

        var reported = new HashSet<string>(StringComparer.Ordinal);
        foreach (SmoObjectEntry skinEntry in output.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            if (!SmoSkinDecoder.TryDecode(
                    output, skinEntry, out SmoSkin? skin, out string error) ||
                skin is null)
            {
                errors.Add($"output skin [{skinEntry.Index}] invalid: {error}");
                continue;
            }

            foreach (SmoSkinBone bone in skin.Bones)
            {
                uint nodeId = output.Objects[bone.NodeObjectIndex].Id;
                bool canonical = canonicalInverse.TryGetValue(
                        nodeId, out Matrix4x4 expected) &&
                    ApproximatelyEqual(bone.InverseBindMatrix, expected, 0.0001f);
                bool preservedOriginal = originalMatrices.TryGetValue(
                        nodeId, out Matrix4x4[]? candidates) &&
                    candidates.Any(candidate => ApproximatelyEqual(
                        bone.InverseBindMatrix, candidate, 0.0001f));
                if (canonical || preservedOriginal)
                    continue;

                string name = output.Objects[bone.NodeObjectIndex].Name;
                string key = $"{skinEntry.Index}:{name}";
                if (reported.Add(key))
                {
                    errors.Add(
                        $"skin [{skinEntry.Index}] bone {name} contains a non-target inverse bind matrix");
                }
            }
        }
    }

    private static void VerifyTargetBindFrameIdentity(
        SmoDocument target,
        SmoDocument output,
        ICollection<string> errors)
    {
        IReadOnlyDictionary<uint, Matrix4x4> targetBindWorld =
            SmoSkinBindingResolver.ResolveBindWorldMatrices(target)
                .ToDictionary(
                    item => target.Objects[item.Key].Id,
                    item => item.Value);
        foreach (SmoObjectEntry meshEntry in output.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MeshData))
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(output, meshEntry);
            if (!mesh.HasSkinningData)
                continue;
            SmoObjectEntry skinEntry = FindParentSkin(output, meshEntry);
            if (!SmoSkinDecoder.TryDecode(
                    output, skinEntry, out SmoSkin? skin, out string skinError) ||
                skin is null)
            {
                errors.Add($"skin [{skinEntry.Index}] invalid for bind-frame check: {skinError}");
                continue;
            }

            for (int vertex = 0; vertex < mesh.VertexCount; vertex++)
            {
                Vector4 weights = mesh.BlendWeights[vertex];
                SmoBlendIndices indices = mesh.BlendIndices[vertex];
                Vector3 position = mesh.Positions[vertex];
                if (!IsFinite(position) ||
                    !float.IsFinite(weights.X) || !float.IsFinite(weights.Y) ||
                    !float.IsFinite(weights.Z) || !float.IsFinite(weights.W))
                {
                    errors.Add(
                        $"mesh [{meshEntry.Index}] vertex {vertex} has non-finite bind data");
                    break;
                }

                Vector3 restored = Vector3.Zero;
                float total = 0;
                bool failed = false;
                Add(weights.X, indices.X);
                Add(weights.Y, indices.Y);
                Add(weights.Z, indices.Z);
                Add(weights.W, indices.W);
                if (failed)
                    break;
                if (total <= WeightEpsilon)
                {
                    errors.Add(
                        $"mesh [{meshEntry.Index}] vertex {vertex} has no bind-frame influences");
                    break;
                }

                restored /= total;
                float tolerance = 0.0001f * MathF.Max(1f, position.Length());
                if (!IsFinite(restored) ||
                    Vector3.Distance(restored, position) > tolerance)
                {
                    errors.Add(
                        $"mesh [{meshEntry.Index}] vertex {vertex} is not identity in target bind frame");
                    break;
                }

                void Add(float weight, byte paletteIndex)
                {
                    if (weight < 0)
                    {
                        failed = true;
                        errors.Add(
                            $"mesh [{meshEntry.Index}] vertex {vertex} has a negative bind weight");
                        return;
                    }
                    if (weight <= WeightEpsilon)
                        return;
                    if (paletteIndex >= skin.Bones.Count)
                    {
                        failed = true;
                        errors.Add(
                            $"mesh [{meshEntry.Index}] vertex {vertex} has an invalid bind influence");
                        return;
                    }
                    SmoSkinBone bone = skin.Bones[paletteIndex];
                    uint nodeId = output.Objects[bone.NodeObjectIndex].Id;
                    if (!targetBindWorld.TryGetValue(
                            nodeId, out Matrix4x4 bindWorld))
                    {
                        failed = true;
                        errors.Add(
                            $"mesh [{meshEntry.Index}] uses a bone without a canonical target bind matrix");
                        return;
                    }
                    restored += Vector3.Transform(
                        position, bone.InverseBindMatrix * bindWorld) * weight;
                    total += weight;
                }
            }
        }
    }

    private readonly record struct PreparedGeometryFingerprint(
        int TriangleCount,
        string Sha256);

    private enum GeometryWeightSemantics
    {
        NormalizeForWriter,
        AlreadyWriterPacked
    }

    private static void VerifyPreparedGeometryFingerprint(
        ImportedScene donor,
        SmoDocument output,
        IReadOnlyDictionary<string, string> boneRemap,
        ICollection<string> errors)
    {
        PreparedGeometryFingerprint expected = FingerprintImportedGeometry(
            donor, boneRemap);
        PreparedGeometryFingerprint actual = FingerprintOutputGeometry(output);
        if (expected != actual)
        {
            errors.Add(
                "prepared geometry fingerprint changed " +
                $"(donor {expected.TriangleCount}/{expected.Sha256}, " +
                $"output {actual.TriangleCount}/{actual.Sha256})");
        }
    }

    private static PreparedGeometryFingerprint FingerprintImportedGeometry(
        ImportedScene donor,
        IReadOnlyDictionary<string, string> boneRemap)
    {
        var triangles = new List<string>();
        foreach (ImportedMesh mesh in donor.Meshes)
        {
            ImportedSkinning skinning = mesh.Skinning ??
                throw new InvalidDataException(
                    $"Mesh {mesh.Name} has no skinning for geometry fingerprint.");
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                string first = ImportedCorner(
                    mesh, skinning, checked((int)mesh.TriangleIndices[index]), boneRemap);
                string third = ImportedCorner(
                    mesh, skinning, checked((int)mesh.TriangleIndices[index + 2]), boneRemap);
                string second = ImportedCorner(
                    mesh, skinning, checked((int)mesh.TriangleIndices[index + 1]), boneRemap);
                // glTF and SMO use opposite handedness. The writer reflects Z
                // and reverses every face, so the expected fingerprint must be
                // expressed in the serialized SMO coordinate system as well.
                triangles.Add(CanonicalTriangle(first, third, second));
            }
        }
        return HashGeometryTriangles(triangles);
    }

    private static PreparedGeometryFingerprint FingerprintOutputGeometry(
        SmoDocument output)
    {
        var triangles = new List<string>();
        foreach (SmoObjectEntry meshEntry in output.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MeshData))
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(output, meshEntry);
            SmoObjectEntry skinEntry = FindParentSkin(output, meshEntry);
            if (!SmoSkinDecoder.TryDecode(
                    output, skinEntry, out SmoSkin? skin, out string skinError) ||
                skin is null)
            {
                throw new InvalidDataException(
                    $"Geometry fingerprint cannot decode skin [{skinEntry.Index}]: {skinError}");
            }
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                int firstIndex = checked((int)mesh.TriangleIndices[index]);
                int secondIndex = checked((int)mesh.TriangleIndices[index + 1]);
                int thirdIndex = checked((int)mesh.TriangleIndices[index + 2]);
                if (firstIndex == secondIndex || secondIndex == thirdIndex ||
                    firstIndex == thirdIndex)
                {
                    continue;
                }

                string first = OutputCorner(output, mesh, skin, firstIndex);
                string second = OutputCorner(output, mesh, skin, secondIndex);
                string third = OutputCorner(output, mesh, skin, thirdIndex);
                triangles.Add(CanonicalTriangle(first, second, third));
            }
        }
        return HashGeometryTriangles(triangles);
    }

    private static string ImportedCorner(
        ImportedMesh mesh,
        ImportedSkinning skinning,
        int vertex,
        IReadOnlyDictionary<string, string> boneRemap)
    {
        Vector2 uv = mesh.TextureCoordinates.Length == mesh.Positions.Length
            ? mesh.TextureCoordinates[vertex]
            : Vector2.Zero;
        Vector2 uv1 = SmoSkinnedUvTransfer.SecondaryOrPrimary(
            mesh.TextureCoordinates,
            mesh.SecondaryTextureCoordinates,
            vertex,
            mesh.Positions.Length);
        var influences = new Dictionary<string, float>(StringComparer.Ordinal);
        Vector4 weights = skinning.Weights[vertex];
        ImportedJointIndices joints = skinning.JointIndices[vertex];
        Add(weights.X, joints.X);
        Add(weights.Y, joints.Y);
        Add(weights.Z, joints.Z);
        Add(weights.W, joints.W);
        Vector3 source = mesh.Positions[vertex];
        return GeometryCorner(
            new Vector3(source.X, source.Y, -source.Z),
            uv,
            uv1,
            influences,
            GeometryWeightSemantics.NormalizeForWriter);

        void Add(float weight, ushort joint)
        {
            if (weight <= WeightEpsilon)
                return;
            string donorName = skinning.Skeleton.JointNames[joint];
            string targetName = boneRemap[donorName];
            influences[targetName] = influences.GetValueOrDefault(targetName) + weight;
        }
    }

    private static string OutputCorner(
        SmoDocument output,
        SmoMesh mesh,
        SmoSkin skin,
        int vertex)
    {
        Vector2 uv = mesh.HasTextureCoordinates
            ? mesh.TextureCoordinates[vertex]
            : Vector2.Zero;
        Vector2 uv1 = mesh.HasTextureCoordinates1
            ? mesh.TextureCoordinates1[vertex]
            : uv;
        var influences = new Dictionary<string, float>(StringComparer.Ordinal);
        Vector4 weights = mesh.BlendWeights[vertex];
        SmoBlendIndices joints = mesh.BlendIndices[vertex];
        Add(weights.X, joints.X);
        Add(weights.Y, joints.Y);
        Add(weights.Z, joints.Z);
        Add(weights.W, joints.W);
        return GeometryCorner(
            mesh.Positions[vertex],
            uv,
            uv1,
            influences,
            GeometryWeightSemantics.AlreadyWriterPacked);

        void Add(float weight, byte paletteIndex)
        {
            if (weight <= WeightEpsilon)
                return;
            if (paletteIndex >= skin.Bones.Count)
                throw new InvalidDataException(
                    $"Geometry fingerprint found palette index {paletteIndex} outside its skin.");
            string name = output.Objects[skin.Bones[paletteIndex].NodeObjectIndex].Name;
            influences[name] = influences.GetValueOrDefault(name) + weight;
        }
    }

    private static string GeometryCorner(
        Vector3 position,
        Vector2 uv,
        Vector2 uv1,
        IReadOnlyDictionary<string, float> influences,
        GeometryWeightSemantics weightSemantics)
    {
        if (!IsFinite(position) ||
            !float.IsFinite(uv.X) || !float.IsFinite(uv.Y) ||
            !float.IsFinite(uv1.X) || !float.IsFinite(uv1.Y))
            throw new InvalidDataException(
                "Prepared geometry fingerprint encountered a non-finite position or UV.");
        float total = influences.Values.Sum();
        if (!float.IsFinite(total) || total <= WeightEpsilon)
            throw new InvalidDataException(
                "Prepared geometry fingerprint encountered invalid skin weights.");

        string weights = string.Join(",", influences
            .OrderBy(item => item.Key, StringComparer.Ordinal)
            .Select(item =>
            {
                // Imported values are the pre-serialization influences and must
                // reproduce the writer's one float32 normalization. Decoded SMO
                // values are already those packed floats; normalizing them again
                // changes values at the 1e-6 quantization boundary whenever their
                // float32 sum is not exactly one.
                float writerPacked = weightSemantics ==
                    GeometryWeightSemantics.NormalizeForWriter
                        ? item.Value / total
                        : item.Value;
                int quantized = checked((int)MathF.Round(
                    writerPacked * 1_000_000f));
                return item.Key + ":" + quantized.ToString(
                    System.Globalization.CultureInfo.InvariantCulture);
            }));
        return string.Join(",",
            FloatBits(position.X), FloatBits(position.Y), FloatBits(position.Z),
            FloatBits(uv.X), FloatBits(uv.Y),
            FloatBits(uv1.X), FloatBits(uv1.Y), weights);
    }

    private static string CanonicalTriangle(
        string first,
        string second,
        string third)
    {
        string one = first + "\u001F" + second + "\u001F" + third;
        string two = second + "\u001F" + third + "\u001F" + first;
        string three = third + "\u001F" + first + "\u001F" + second;
        string result = StringComparer.Ordinal.Compare(one, two) <= 0 ? one : two;
        return StringComparer.Ordinal.Compare(result, three) <= 0 ? result : three;
    }

    private static PreparedGeometryFingerprint HashGeometryTriangles(
        IEnumerable<string> triangles)
    {
        string[] ordered = triangles.Order(StringComparer.Ordinal).ToArray();
        using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        Span<byte> length = stackalloc byte[sizeof(int)];
        foreach (string triangle in ordered)
        {
            byte[] bytes = System.Text.Encoding.UTF8.GetBytes(triangle);
            BinaryPrimitives.WriteInt32LittleEndian(length, bytes.Length);
            hash.AppendData(length);
            hash.AppendData(bytes);
        }
        return new PreparedGeometryFingerprint(
            ordered.Length, Convert.ToHexString(hash.GetHashAndReset()));
    }

    private static string FloatBits(float value) =>
        BitConverter.SingleToInt32Bits(value).ToString(
            "X8", System.Globalization.CultureInfo.InvariantCulture);

    private static bool IsRigidTargetGroup(
        SmoDocument target,
        TargetVisualGroup group) =>
        group.Meshes
            .SelectMany(mesh => mesh.Skin.Bones)
            .Select(bone => target.Objects[bone.NodeObjectIndex].Name)
            .Distinct(StringComparer.Ordinal)
            .Take(2)
            .Count() == 1;

    private static Vector3[] GenerateImportedSmoothNormals(ImportedMesh mesh)
    {
        var result = new Vector3[mesh.Positions.Length];
        for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
        {
            int first = checked((int)mesh.TriangleIndices[index]);
            int second = checked((int)mesh.TriangleIndices[index + 1]);
            int third = checked((int)mesh.TriangleIndices[index + 2]);
            Vector3 face = Vector3.Cross(
                mesh.Positions[second] - mesh.Positions[first],
                mesh.Positions[third] - mesh.Positions[first]);
            result[first] += face;
            result[second] += face;
            result[third] += face;
        }
        for (int index = 0; index < result.Length; index++)
            result[index] = result[index].LengthSquared() > 1e-20f
                ? Vector3.Normalize(result[index]) : Vector3.UnitY;
        return result;
    }

    private static bool ApproximatelyEqual(
        Matrix4x4 left,
        Matrix4x4 right,
        float epsilon)
    {
        float[] cells =
        [
            left.M11 - right.M11, left.M12 - right.M12,
            left.M13 - right.M13, left.M14 - right.M14,
            left.M21 - right.M21, left.M22 - right.M22,
            left.M23 - right.M23, left.M24 - right.M24,
            left.M31 - right.M31, left.M32 - right.M32,
            left.M33 - right.M33, left.M34 - right.M34,
            left.M41 - right.M41, left.M42 - right.M42,
            left.M43 - right.M43, left.M44 - right.M44
        ];
        return cells.All(value => MathF.Abs(value) <= epsilon);
    }

    private static int GetImportedMeshIndex(
        IReadOnlyList<ImportedMesh> meshes,
        ImportedMesh selected)
    {
        for (int index = 0; index < meshes.Count; index++)
            if (ReferenceEquals(meshes[index], selected))
                return index;
        throw new InvalidOperationException("Imported mesh is absent from its source scene.");
    }

    private static int CountImportedNonDegenerateTriangles(SmoMesh mesh) =>
        Enumerable.Range(0, mesh.TriangleIndices.Length / 3).Count(triangle =>
        {
            uint first = mesh.TriangleIndices[triangle * 3];
            uint second = mesh.TriangleIndices[triangle * 3 + 1];
            uint third = mesh.TriangleIndices[triangle * 3 + 2];
            return first != second && second != third && first != third;
        });

    private static bool IsFinite(Matrix4x4 value) =>
        float.IsFinite(value.M11) && float.IsFinite(value.M12) &&
        float.IsFinite(value.M13) && float.IsFinite(value.M14) &&
        float.IsFinite(value.M21) && float.IsFinite(value.M22) &&
        float.IsFinite(value.M23) && float.IsFinite(value.M24) &&
        float.IsFinite(value.M31) && float.IsFinite(value.M32) &&
        float.IsFinite(value.M33) && float.IsFinite(value.M34) &&
        float.IsFinite(value.M41) && float.IsFinite(value.M42) &&
        float.IsFinite(value.M43) && float.IsFinite(value.M44);

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);

    private static void WriteVector2(Span<byte> data, int offset, Vector2 value)
    {
        WriteSingle(data, offset, value.X);
        WriteSingle(data, offset + 4, value.Y);
    }

    private static void WriteVector3(Span<byte> data, int offset, Vector3 value)
    {
        WriteSingle(data, offset, value.X);
        WriteSingle(data, offset + 4, value.Y);
        WriteSingle(data, offset + 8, value.Z);
    }

    private static void WriteVector4(Span<byte> data, int offset, Vector4 value)
    {
        WriteSingle(data, offset, value.X);
        WriteSingle(data, offset + 4, value.Y);
        WriteSingle(data, offset + 8, value.Z);
        WriteSingle(data, offset + 12, value.W);
    }

    private static void WriteSingle(Span<byte> data, int offset, float value) =>
        BinaryPrimitives.WriteInt32LittleEndian(
            data[offset..], BitConverter.SingleToInt32Bits(value));
}
