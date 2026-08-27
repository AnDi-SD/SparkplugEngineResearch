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
/// Controls whether skinned import replaces paired target textures or keeps
/// every target <c>spTextureData</c> object byte-identical.
/// </summary>
public enum SkinnedTextureTransferMode
{
    ImportDonor = 0,
    PreserveTarget = 1
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
/// remain stable through the internal atlas preparation step.
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
/// Experimental GLB skin transfer. The complete target SMO graph stays intact;
/// only existing mesh leaves and reference-only skin palettes are rewritten.
/// </summary>
public static class SmoSkinnedGlbReplacer
{
    public static GlbSkinTransferPlan Analyze(
        SmoDocument target,
        ImportedScene donor,
        SkinnedTextureTransferMode textureMode =
            SkinnedTextureTransferMode.ImportDonor,
        SkinnedRenderableMaterialProfile? materialProfile = null,
        CancellationToken cancellationToken = default) =>
        SmoVisualTransplanter.AnalyzeSkinnedGlb(
            target, donor, textureMode, materialProfile, cancellationToken);

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
        SkinnedTextureTransferMode textureMode =
            SkinnedTextureTransferMode.ImportDonor,
        SkinnedRenderableMaterialProfile? materialProfile = null) =>
        SmoVisualTransplanter.PrepareSkinnedGlbGeometryPreview(
            target, donor, transform, transferMode, textureMode, materialProfile);

    /// <summary>
    /// Replaces the skinned geometry and, by default, maps each donor material
    /// texture to its paired target visual group.
    /// </summary>
    /// <param name="texture">
    /// An explicit legacy override applied to every paired target texture.
    /// Pass <see langword="null"/> to use per-material donor textures.
    /// </param>
    public static GlbSkinTransferResult Replace(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform transform,
        string outputPath,
        SkinnedGeometryTransferMode transferMode =
            SkinnedGeometryTransferMode.PreservePreparedGeometry,
        ImportedTexture? texture = null,
        SkinnedTextureTransferMode textureMode =
            SkinnedTextureTransferMode.ImportDonor,
        SkinnedRenderableMaterialProfile? materialProfile = null) =>
        SmoVisualTransplanter.TransplantSkinnedGlb(
            target, donor, transform, outputPath, transferMode, texture,
            textureMode, materialProfile);

    /// <summary>
    /// Compatibility overload for existing callers. New code should pass a
    /// named <see cref="SkinnedGeometryTransferMode"/> value.
    /// </summary>
    public static GlbSkinTransferResult Replace(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform transform,
        string outputPath,
        bool rebaseToTargetBindPose,
        ImportedTexture? texture = null,
        SkinnedTextureTransferMode textureMode =
            SkinnedTextureTransferMode.ImportDonor,
        SkinnedRenderableMaterialProfile? materialProfile = null) =>
        Replace(
            target,
            donor,
            transform,
            outputPath,
            rebaseToTargetBindPose
                ? SkinnedGeometryTransferMode.RetargetToGameBindPose
                : SkinnedGeometryTransferMode.PreservePreparedGeometry,
            texture,
            textureMode,
            materialProfile);
}

internal static partial class SmoVisualTransplanter
{
    private const int MaximumSafeDonorMaterialGroups = 16;
    private const int MaximumSafeTargetMaterialGroups = 32;
    private const int MaximumMaterialAssignmentSearchStates = 4_096;
    private const int MaximumMaterialMergeCombinations = 1_024;
    // Dense modular characters with all five finger lanes can legitimately
    // cross the old 4K state boundary. Keep the independent five-second wall
    // clock and cooperative cancellation as the hard responsiveness guards.
    private const int MaximumPalettePackingSearchStates = 8_192;
    private const int MaximumPaletteRequirementSets = 4_096;
    private const int MaximumMaximalPaletteRequirements = 512;
    private static readonly TimeSpan MaximumPalettePackingWallTime =
        TimeSpan.FromSeconds(10);

    private sealed record GlbPlanContext(
        GlbSkinTransferPlan PublicPlan,
        ImportedScene PreparedDonor,
        ImportedSkeleton Skeleton,
        IReadOnlyDictionary<string, string> BoneRemap,
        IReadOnlyDictionary<string, Matrix4x4> TargetBindWorld,
        IReadOnlyDictionary<string, Matrix4x4> TargetInverseBind,
        IReadOnlyList<ImportedGroupPair> Groups,
        IReadOnlySet<int> FullRgbaTextureIndices,
        IReadOnlySet<int> AlphaBlendTextureIndices);

    private sealed record ImportedGroup(
        int MaterialIndex,
        int SourceTextureIndex,
        string? LegacyTextureName,
        IReadOnlyList<ImportedMesh> Meshes,
        int TriangleCount);

    private sealed record ImportedGroupPair(
        int TargetTextureObjectIndex,
        ImportedGroup Donor);

    private sealed record ImportedGroupPairingPlan(
        IReadOnlyList<ImportedGroupPair> Pairs,
        IReadOnlyList<string> Messages,
        string? Error)
    {
        public bool IsSafe => Error is null;
    }

    private sealed record ImportedMaterialAtlasPlan(
        ImportedTextureAtlasRepackResult Atlas,
        int TargetTextureObjectIndex);

    private sealed record ImportedTextureAssignment(
        byte[] Data,
        bool ReplaceAlpha,
        bool EnableAlphaBlend);

    private sealed record SkinnedAlphaSplitWork(
        int TargetTextureObjectIndex,
        uint TargetTextureObjectId,
        SmoSkinnedBranchSourceMesh[] SourceMeshes,
        SmoSkinnedRenderableOpacityPlan Opacity);

    private sealed record ImportedTransferMesh(
        int Key,
        string Name,
        Vector3[] Positions,
        Vector3[] Normals,
        Vector2[] TextureCoordinates,
        uint[] DiffuseColorsArgb,
        uint[] TriangleIndices,
        ImportedSkinning Skinning);

    private sealed class ImportedTargetSlot
    {
        private readonly Dictionary<(int Mesh, int Vertex), ushort> _vertices = [];

        public ImportedTargetSlot(
            SmoDocument document,
            MeshSource source)
        {
            Source = source;
            PaletteByName = source.Skin.Bones
                .GroupBy(bone => document.Objects[bone.NodeObjectIndex].Name,
                    StringComparer.Ordinal)
                .ToDictionary(
                    group => group.Key,
                    group => checked((byte)group.First().PaletteIndex),
                    StringComparer.Ordinal);
        }

        public MeshSource Source { get; }
        public IReadOnlyDictionary<string, byte> PaletteByName { get; }
        public List<byte[]> VertexRecords { get; } = [];
        public List<ushort> Indices { get; } = [];
        public int TriangleCount { get; private set; }

        public void AddTriangle(
            ImportedTransferMesh mesh,
            IReadOnlyList<int> vertices,
            IReadOnlyDictionary<string, string> boneRemap)
        {
            foreach (int vertex in vertices)
                Indices.Add(GetOrAddVertex(mesh, vertex, boneRemap));
            TriangleCount++;
        }

        public void AddDegenerate()
        {
            byte[] record = new byte[Source.Layout.SerializedStride];
            WriteVector3(record, 0, Vector3.Zero);
            if (Source.Layout.NormalOffset is int normalOffset)
                WriteVector3(record, normalOffset, Vector3.UnitY);
            if (Source.Layout.DiffuseArgbOffset is int diffuseOffset)
                WriteUInt32(record, diffuseOffset, 0x00FFFFFF);
            if (Source.Layout.BlendWeightsOffset is int weightsOffset)
                WriteVector4(record, weightsOffset, new Vector4(1, 0, 0, 0));
            if (Source.Layout.BlendIndicesOffset is int indicesOffset)
                record.AsSpan(indicesOffset, 4).Clear();
            VertexRecords.Add(record);
            Indices.Add(0);
            Indices.Add(0);
            Indices.Add(0);
        }

        private ushort GetOrAddVertex(
            ImportedTransferMesh mesh,
            int vertex,
            IReadOnlyDictionary<string, string> boneRemap)
        {
            var key = (mesh.Key, vertex);
            if (_vertices.TryGetValue(key, out ushort existing))
                return existing;
            if (VertexRecords.Count >= ushort.MaxValue)
                throw new InvalidOperationException(
                    $"Target mesh [{Source.Entry.Index}] exceeds 65,535 vertices.");

            byte[] record = new byte[Source.Layout.SerializedStride];
            WriteVector3(record, 0, mesh.Positions[vertex]);
            if (Source.Layout.NormalOffset is int normalOffset)
                WriteVector3(record, normalOffset, mesh.Normals[vertex]);
            if (Source.Layout.DiffuseArgbOffset is int diffuseOffset)
                WriteUInt32(record, diffuseOffset,
                    mesh.DiffuseColorsArgb.Length == mesh.Positions.Length
                        ? mesh.DiffuseColorsArgb[vertex] : 0xFFFFFFFF);
            Vector2 uv = mesh.TextureCoordinates.Length == mesh.Positions.Length
                ? mesh.TextureCoordinates[vertex] : Vector2.Zero;
            if (Source.Layout.TextureCoordinate0Offset is int uv0Offset)
                WriteVector2(record, uv0Offset, uv);
            if (Source.Layout.TextureCoordinate1Offset is int uv1Offset)
                WriteVector2(record, uv1Offset, uv);

            int weightsOffset = Source.Layout.BlendWeightsOffset ??
                throw new InvalidOperationException(
                    $"Target mesh [{Source.Entry.Index}] has no blend weights.");
            int indicesOffset = Source.Layout.BlendIndicesOffset ??
                throw new InvalidOperationException(
                    $"Target mesh [{Source.Entry.Index}] has no blend indices.");
            Vector4 sourceWeights = mesh.Skinning.Weights[vertex];
            ImportedJointIndices sourceIndices = mesh.Skinning.JointIndices[vertex];
            var mapped = new Dictionary<byte, float>();
            Add(sourceWeights.X, sourceIndices.X);
            Add(sourceWeights.Y, sourceIndices.Y);
            Add(sourceWeights.Z, sourceIndices.Z);
            Add(sourceWeights.W, sourceIndices.W);
            (byte Slot, float Weight)[] influences = mapped
                .OrderByDescending(item => item.Value)
                .ThenBy(item => item.Key)
                .Take(4)
                .Select(item => (item.Key, item.Value))
                .ToArray();
            float total = influences.Sum(item => item.Weight);
            if (total <= WeightEpsilon)
                throw new InvalidDataException(
                    $"Mesh {mesh.Name} vertex {vertex} has no positive skin weights.");
            float[] packedWeights = new float[4];
            byte[] packedIndices = new byte[4];
            for (int influence = 0; influence < influences.Length; influence++)
            {
                packedIndices[influence] = influences[influence].Slot;
                packedWeights[influence] = influences[influence].Weight / total;
            }
            WriteVector4(record, weightsOffset, new Vector4(
                packedWeights[0], packedWeights[1], packedWeights[2], packedWeights[3]));
            packedIndices.CopyTo(record, indicesOffset);

            ushort created = checked((ushort)VertexRecords.Count);
            VertexRecords.Add(record);
            _vertices.Add(key, created);
            return created;

            void Add(float weight, ushort donorJoint)
            {
                if (weight <= WeightEpsilon)
                    return;
                if (donorJoint >= mesh.Skinning.Skeleton.JointNames.Count)
                    throw new InvalidDataException(
                        $"Mesh {mesh.Name} vertex {vertex} references joint {donorJoint} outside its skin.");
                string donorName = mesh.Skinning.Skeleton.JointNames[donorJoint];
                if (!boneRemap.TryGetValue(donorName, out string? targetName) ||
                    !PaletteByName.TryGetValue(targetName, out byte targetSlot))
                    throw new InvalidOperationException(
                        $"Bone {donorName} is absent from target palette [{Source.SkinEntry.Index}].");
                mapped[targetSlot] = mapped.GetValueOrDefault(targetSlot) + weight;
            }
        }
    }

    internal static GlbSkinTransferPlan AnalyzeSkinnedGlb(
        SmoDocument target,
        ImportedScene donor,
        SkinnedTextureTransferMode textureTransferMode,
        SkinnedRenderableMaterialProfile? materialProfile,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        materialProfile ??= SkinnedRenderableMaterialProfile.Default;
        GlbPlanContext context = BuildGlbPlan(
            target,
            donor,
            textureTransferMode,
            materialProfile,
            cancellationToken: cancellationToken);
        return context.PublicPlan;
    }

    internal static ImportedScene PrepareSkinnedGlbGeometryPreview(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform transform,
        SkinnedGeometryTransferMode transferMode,
        SkinnedTextureTransferMode textureTransferMode,
        SkinnedRenderableMaterialProfile? materialProfile)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        ArgumentNullException.ThrowIfNull(transform);
        if (!Enum.IsDefined(transferMode))
            throw new ArgumentOutOfRangeException(nameof(transferMode), transferMode, null);
        if (!Enum.IsDefined(textureTransferMode))
        {
            throw new ArgumentOutOfRangeException(
                nameof(textureTransferMode), textureTransferMode, null);
        }
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
            target, donor, textureTransferMode, materialProfile);
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

    internal static GlbSkinTransferResult TransplantSkinnedGlb(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform transform,
        string outputPath,
        SkinnedGeometryTransferMode transferMode,
        ImportedTexture? texture,
        SkinnedTextureTransferMode textureTransferMode,
        SkinnedRenderableMaterialProfile? materialProfile)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        materialProfile ??= SkinnedRenderableMaterialProfile.Default;
        if (!Enum.IsDefined(textureTransferMode))
        {
            throw new ArgumentOutOfRangeException(
                nameof(textureTransferMode), textureTransferMode, null);
        }
        if (textureTransferMode == SkinnedTextureTransferMode.PreserveTarget &&
            texture is not null)
        {
            throw new ArgumentException(
                "An explicit texture cannot be combined with PreserveTarget.",
                nameof(texture));
        }
        if (materialProfile.HasExplicitOverrides &&
            textureTransferMode == SkinnedTextureTransferMode.PreserveTarget)
        {
            throw new ArgumentException(
                "Explicit renderable material modes require ImportDonor texture transfer.",
                nameof(materialProfile));
        }
        if (materialProfile.HasExplicitOverrides && texture is not null)
        {
            throw new ArgumentException(
                "Explicit renderable material modes cannot be combined with one legacy " +
                "texture override; use the donor's per-material textures.",
                nameof(texture));
        }

        GlbPlanContext context = BuildGlbPlan(
            target,
            donor,
            textureTransferMode,
            materialProfile,
            allowMaterialAtlas: texture is null);
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
        IReadOnlyDictionary<int, ImportedTextureAssignment> textureAssignments =
            BuildPairedTextureAssignments(
                donor,
                context.Groups,
                texture,
                textureTransferMode,
                context.FullRgbaTextureIndices,
                context.AlphaBlendTextureIndices);

        ImportedTransferMesh[] transferMeshes = BuildImportedTransferMeshes(
                donor, context, transform, transferMode)
            .Select(ConvertImportedMeshToSmoSpace)
            .ToArray();
        byte[] patchedBytes = target.Data.ToArray();
        VisualGroup[] initialTargetGroups = BuildGroups(
            target, SmoTextureBindingResolver.ResolveAll(target));
        Dictionary<int, ImportedGroupPair> pairsByTexture = context.Groups
            .ToDictionary(pair => pair.TargetTextureObjectIndex);
        var alphaSplits = new Dictionary<int, SkinnedAlphaSplitWork>();

        foreach (VisualGroup targetGroup in initialTargetGroups)
        {
            if (!pairsByTexture.TryGetValue(
                    targetGroup.TextureObjectIndex, out ImportedGroupPair? pair))
                continue;
            ImportedTransferMesh[] groupMeshes = pair.Donor.Meshes.Select(mesh =>
                transferMeshes.Single(value => value.Key ==
                    GetImportedMeshIndex(donor.Meshes, mesh))).ToArray();
            ImportedTransferMesh[] paletteMeshes = groupMeshes;
            bool enableAutoAlpha = textureAssignments.TryGetValue(
                    targetGroup.TextureObjectIndex,
                    out ImportedTextureAssignment? assignment) &&
                assignment.EnableAlphaBlend;
            bool hasExplicitMaterial = groupMeshes.Any(mesh =>
                materialProfile.GetMode(mesh.Key) !=
                SkinnedRenderableMaterialMode.Auto);
            if (enableAutoAlpha || hasExplicitMaterial)
            {
                ImportedTexture sourceTexture =
                    ResolveImportedGroupTexture(donor, pair.Donor) ??
                    throw new InvalidDataException(
                        $"Transparent material group {pair.Donor.MaterialIndex} " +
                        "has no source texture.");
                SmoSkinnedBranchSourceMesh[] splitMeshes = groupMeshes
                    .Select(ToSkinnedBranchSourceMesh)
                    .ToArray();
                SmoSkinnedRenderableOpacityPlan opacity =
                    SmoSkinnedBranchSplitBuilder.ClassifyRenderables(
                        splitMeshes, sourceTexture, materialProfile);
                splitMeshes = SmoSkinnedBranchSplitBuilder
                    .ConformCloseSurfaceNormals(splitMeshes, opacity)
                    .Meshes;
                uint textureObjectId =
                    target.Objects[targetGroup.TextureObjectIndex].Id;
                if (opacity.SeparateBranchTriangleCount > 0)
                {
                    _ = SmoSkinnedBranchSplitBuilder.Analyze(
                        target,
                        textureObjectId,
                        splitMeshes,
                        opacity,
                        context.BoneRemap,
                        context.TargetInverseBind);
                    alphaSplits.Add(
                        targetGroup.TextureObjectIndex,
                        new SkinnedAlphaSplitWork(
                            targetGroup.TextureObjectIndex,
                            textureObjectId,
                            splitMeshes,
                            opacity));
                }
                paletteMeshes = FilterImportedTriangles(
                    groupMeshes, opacity, keepSeparateBranch: false);
            }
            if (paletteMeshes.Any(mesh => mesh.TriangleIndices.Length > 0))
            {
                PatchImportedPalettes(
                    patchedBytes, target, targetGroup, paletteMeshes,
                    context.BoneRemap, context.TargetInverseBind);
            }
        }
        SmoDocument patchedTarget = SmoDocument.Parse(patchedBytes, target.SourcePath);
        VisualGroup[] patchedGroups = BuildGroups(
            patchedTarget, SmoTextureBindingResolver.ResolveAll(patchedTarget));

        var replacements = new List<ObjectReplacement>();
        int triangles = 0;
        int vertices = 0;
        int palettes = 0;
        foreach (VisualGroup targetGroup in patchedGroups)
        {
            ImportedTargetSlot[] slots = targetGroup.Meshes
                .Select(mesh => new ImportedTargetSlot(patchedTarget, mesh)).ToArray();
            if (pairsByTexture.TryGetValue(
                    targetGroup.TextureObjectIndex, out ImportedGroupPair? pair))
            {
                ImportedTransferMesh[] groupMeshes = pair.Donor.Meshes.Select(mesh =>
                    transferMeshes.Single(value => value.Key ==
                        GetImportedMeshIndex(donor.Meshes, mesh))).ToArray();
                alphaSplits.TryGetValue(
                    targetGroup.TextureObjectIndex,
                    out SkinnedAlphaSplitWork? alphaSplit);
                foreach (ImportedTransferMesh mesh in groupMeshes)
                {
                    for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
                    {
                        if (alphaSplit is not null &&
                            alphaSplit.Opacity.IsSeparateBranch(mesh.Key, index / 3))
                        {
                            continue;
                        }
                        int[] sourceVertices =
                        [
                            checked((int)mesh.TriangleIndices[index]),
                            checked((int)mesh.TriangleIndices[index + 1]),
                            checked((int)mesh.TriangleIndices[index + 2])
                        ];
                        HashSet<string> bones = GetImportedTriangleBones(
                            mesh, sourceVertices, context.BoneRemap);
                        ImportedTargetSlot[] candidates = slots
                            .Where(slot => bones.All(slot.PaletteByName.ContainsKey))
                            .ToArray();
                        if (candidates.Length == 0)
                            throw new InvalidOperationException(
                                $"Triangle {index / 3} mesh {mesh.Name} does not fit any planned target palette: " +
                                string.Join(", ", bones.Order(StringComparer.Ordinal)) + ".");
                        ImportedTargetSlot selected = candidates
                            .OrderBy(slot => slot.TriangleCount)
                            .ThenBy(slot => slot.Source.Entry.Index)
                            .First();
                        selected.AddTriangle(mesh, sourceVertices, context.BoneRemap);
                    }
                }
            }
            foreach (ImportedTargetSlot slot in slots)
            {
                if (slot.TriangleCount == 0)
                    slot.AddDegenerate();
                byte[] meshData = BuildImportedMeshObject(slot);
                replacements.Add(new ObjectReplacement(slot.Source.Entry, meshData));
                triangles += slot.TriangleCount;
                vertices += slot.VertexRecords.Count;
                palettes++;
            }
        }

        byte[] output = Repack(patchedTarget, replacements);
        if (textureAssignments.Count > 0)
        {
            output = ReplacePairedTextureData(
                output,
                target.SourcePath,
                textureAssignments);
        }
        var splitResults = new List<SmoSkinnedBranchSplitResult>();
        foreach (SkinnedAlphaSplitWork split in alphaSplits.Values
                     .Where(item => item.Opacity.SeparateBranchTriangleCount > 0)
                     .OrderBy(item => item.TargetTextureObjectId))
        {
            SmoSkinnedBranchSplitResult result =
                SmoSkinnedBranchSplitBuilder.Inject(
                    SmoDocument.Parse(output, target.SourcePath),
                    split.TargetTextureObjectId,
                    split.SourceMeshes,
                    split.Opacity,
                    context.BoneRemap,
                    context.TargetInverseBind);
            output = result.Data;
            triangles += result.TriangleCount;
            vertices += result.VertexCount;
            palettes += result.BranchCount;
            splitResults.Add(result);
        }
        HashSet<int> replacedTextureObjectIndices = textureAssignments.Keys.ToHashSet();
        Dictionary<int, uint> targetTextureIdsByIndex = textureAssignments.Keys
            .ToDictionary(index => index, index => target.Objects[index].Id);
        HashSet<uint> addedObjectIds = splitResults
            .SelectMany(result => result.AddedObjectIds)
            .ToHashSet();

        string fullOutput = Path.GetFullPath(outputPath);
        string? directory = Path.GetDirectoryName(fullOutput);
        if (string.IsNullOrWhiteSpace(directory))
            throw new InvalidOperationException("Не удалось определить папку результата.");
        Directory.CreateDirectory(directory);
        string temporary = Path.Combine(
            directory, $".{Path.GetFileName(fullOutput)}.{Guid.NewGuid():N}.tmp");
        try
        {
            File.WriteAllBytes(temporary, output);
            SmoDocument verified = SmoDocument.Load(temporary);
            var errors = new List<string>();
            if (verified.HasErrors)
                errors.Add("strict parser reported errors");
            if (verified.Objects.Count != target.Objects.Count + addedObjectIds.Count)
                errors.Add("output object count does not match the explicit alpha split");
            HashSet<uint> actualAddedIds = verified.Objects
                .Select(entry => entry.Id)
                .Except(target.Objects.Select(entry => entry.Id))
                .ToHashSet();
            if (!actualAddedIds.SetEquals(addedObjectIds))
                errors.Add("output contains unplanned object identities");
            Dictionary<uint, SmoObjectEntry> verifiedById = verified.Objects
                .ToDictionary(entry => entry.Id);
            foreach (SmoObjectEntry targetEntry in target.Objects)
            {
                if (!verifiedById.TryGetValue(
                        targetEntry.Id, out SmoObjectEntry? verifiedEntry) ||
                    verifiedEntry.Name != targetEntry.Name ||
                    verifiedEntry.TypeHash != targetEntry.TypeHash ||
                    GetParentObjectId(verified, verifiedEntry) !=
                    GetParentObjectId(target, targetEntry))
                {
                    errors.Add(
                        $"target object ID {targetEntry.Id} identity or parent changed");
                }
            }
            int verifiedTriangles = verified.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
                .Select(entry => SmoMeshDecoder.Decode(verified, entry))
                .Sum(CountImportedNonDegenerateTriangles);
            int expectedTriangles = donor.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
            if (verifiedTriangles != expectedTriangles || triangles != expectedTriangles)
                errors.Add($"triangles {verifiedTriangles} != GLB {expectedTriangles}");
            foreach (SmoObjectEntry meshEntry in verified.Objects.Where(entry =>
                         entry.TypeHash == SmoClassIds.MeshData))
            {
                SmoObjectEntry skinEntry = FindParentSkin(verified, meshEntry);
                if (!SmoSkinDecoder.TryDecode(
                        verified, skinEntry, out SmoSkin? skin, out string skinError) || skin is null)
                {
                    errors.Add($"skin [{skinEntry.Index}] invalid: {skinError}");
                    continue;
                }
                SmoMesh mesh = SmoMeshDecoder.Decode(verified, meshEntry);
                for (int vertex = 0; vertex < mesh.VertexCount; vertex++)
                {
                    Vector4 weights = mesh.BlendWeights[vertex];
                    SmoBlendIndices indices = mesh.BlendIndices[vertex];
                    if ((weights.X > WeightEpsilon && indices.X >= skin.Bones.Count) ||
                        (weights.Y > WeightEpsilon && indices.Y >= skin.Bones.Count) ||
                        (weights.Z > WeightEpsilon && indices.Z >= skin.Bones.Count) ||
                        (weights.W > WeightEpsilon && indices.W >= skin.Bones.Count))
                        errors.Add($"mesh [{meshEntry.Index}] has a bone index outside its palette");
                }
            }
            VerifyTargetNodeInvariants(target, verified, errors);
            VerifyRetainedVisualStates(target, verified, errors);
            VerifyTargetPaletteBindMatrices(target, verified, errors);
            VerifyTargetBindFrameIdentity(target, verified, errors);
            VerifyUnreplacedTargetTexturesUnchanged(
                target, verified, replacedTextureObjectIndices, errors);
            VerifyTransferredTexturePixels(
                verified, textureAssignments, targetTextureIdsByIndex, errors);
            if (transferMode == SkinnedGeometryTransferMode.PreservePreparedGeometry)
            {
                VerifyPreparedGeometryFingerprint(
                    donor, verified, context.BoneRemap, errors);
            }
            if (errors.Count > 0)
                throw new InvalidDataException(
                    "Skinned GLB result failed verification: " + string.Join("; ", errors) + ".");
            File.Move(temporary, fullOutput, true);
            return new GlbSkinTransferResult(
                fullOutput,
                verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData),
                vertices,
                verifiedTriangles,
                palettes,
                verified.Data.Length,
                Convert.ToHexString(SHA256.HashData(verified.Data.Span)));
        }
        finally
        {
            if (File.Exists(temporary))
                File.Delete(temporary);
        }
    }

    private static GlbPlanContext BuildGlbPlan(
        SmoDocument target,
        ImportedScene donor,
        SkinnedTextureTransferMode textureTransferMode,
        SkinnedRenderableMaterialProfile materialProfile,
        bool allowMaterialAtlas = true,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        ArgumentNullException.ThrowIfNull(materialProfile);
        if (!Enum.IsDefined(textureTransferMode))
        {
            throw new ArgumentOutOfRangeException(
                nameof(textureTransferMode), textureTransferMode, null);
        }
        var errors = new List<string>();
        var warnings = new List<string>();
        try
        {
            ValidateRenderableMaterialProfile(donor, materialProfile);
            if (materialProfile.HasExplicitOverrides)
            {
                if (textureTransferMode != SkinnedTextureTransferMode.ImportDonor)
                {
                    throw new InvalidOperationException(
                        "Explicit renderable material modes require ImportDonor texture transfer.");
                }
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

        VisualGroup[] targetGroups;
        try
        {
            targetGroups = BuildGroups(target, SmoTextureBindingResolver.ResolveAll(target));
        }
        catch (Exception exception) when (exception is InvalidDataException or InvalidOperationException)
        {
            errors.Add("Target visual graph не поддерживается: " + exception.Message);
            targetGroups = [];
        }
        ImportedGroup[] donorGroups;
        var fullRgbaTextureIndices = new HashSet<int>();
        var alphaBlendTextureIndices = new HashSet<int>();
        if (textureTransferMode == SkinnedTextureTransferMode.PreserveTarget)
        {
            ImportedMesh[] meshes = donor.Meshes.ToArray();
            donorGroups = meshes.Length == 0
                ? []
                :
                [
                    new ImportedGroup(
                        -1,
                        -1,
                        null,
                        meshes,
                        meshes.Sum(mesh => mesh.TriangleIndices.Length / 3))
                ];
            warnings.Add(
                "Donor textures and material groups are ignored: all imported " +
                "primitives use the primary target visual group, and every target " +
                "TextureData remains byte-identical.");
        }
        else
        {
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
                    if (!ImportedTextureAtlasRepacker.TextureContainsTransparency(
                            donor.Textures[textureIndex]))
                    {
                        continue;
                    }
                    fullRgbaTextureIndices.Add(textureIndex);
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
        }

        ImportedGroupPair[] groupPairs = [];
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
                ImportedGroupPairingPlan pairing = BuildSafeImportedGroupPairing(
                    target,
                    donor,
                    skeleton,
                    remap,
                    targetBind,
                    targetInverse,
                    targetGroups,
                    donorGroups,
                    fullRgbaTextureIndices,
                    alphaBlendTextureIndices,
                    materialProfile,
                    cancellationToken: cancellationToken);
                string initialPairingError = pairing.Error ?? string.Empty;
                bool mayBuildAtlas = ShouldAttemptImportedMaterialAtlas(
                    pairing.IsSafe,
                    textureTransferMode,
                    allowMaterialAtlas,
                    donorGroups.Length);
                if (mayBuildAtlas)
                {
                    try
                    {
                        ImportedMaterialAtlasPlan atlasPlan = BuildImportedMaterialAtlas(
                            target, donor, targetGroups, donorGroups);
                        cancellationToken.ThrowIfCancellationRequested();
                        ImportedTextureAtlasRepackResult atlas = atlasPlan.Atlas;
                        donor = atlas.Scene;
                        fullRgbaTextureIndices.Add(atlas.AtlasTextureIndex);
                        if (atlas.UsesTransparency)
                            alphaBlendTextureIndices.Add(atlas.AtlasTextureIndex);
                        warnings.AddRange(atlas.Messages);
                        int validationErrorCount = errors.Count;
                        ValidateImportedTextureIndices(donor, errors);
                        donorGroups = BuildImportedGroups(donor, warnings);
                        if (errors.Count == validationErrorCount)
                        {
                            pairing = BuildSafeImportedGroupPairing(
                                target,
                                donor,
                                skeleton,
                                remap,
                                targetBind,
                                targetInverse,
                                targetGroups,
                                donorGroups,
                                fullRgbaTextureIndices,
                                alphaBlendTextureIndices,
                                materialProfile,
                                atlasPlan.TargetTextureObjectIndex,
                                cancellationToken);
                        }
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
                            initialPairingError + " Donor material atlas fallback failed: " +
                            exception.Message);
                    }
                }
                if (errors.Count == 0)
                {
                    if (!pairing.IsSafe)
                    {
                        string failure = pairing.Error ?? "Material-group pairing failed.";
                        if (!string.IsNullOrWhiteSpace(initialPairingError) &&
                            !string.Equals(
                                failure, initialPairingError, StringComparison.Ordinal))
                        {
                            failure += " Initial assignment failure: " + initialPairingError;
                        }
                        errors.Add(failure);
                    }
                    else
                    {
                        groupPairs = pairing.Pairs.ToArray();
                        warnings.AddRange(pairing.Messages);
                    }
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
            groupPairs,
            fullRgbaTextureIndices,
            alphaBlendTextureIndices);
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
            [],
            new HashSet<int>(),
            new HashSet<int>());
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
                    $"Material states {string.Join(", ", materialIndices)} use the shared atlas " +
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

    private static ImportedMaterialAtlasPlan BuildImportedMaterialAtlas(
        SmoDocument target,
        ImportedScene donor,
        IReadOnlyList<VisualGroup> targetGroups,
        IReadOnlyList<ImportedGroup> donorGroups)
    {
        VisualGroup atlasTarget = OrderTargetGroupsByPreference(target, targetGroups)[0];
        if (atlasTarget.TextureFormat is not (0x32E3 or 0x43E3) ||
            atlasTarget.TextureLayout != SmoTextureLayout.Bgra)
        {
            throw new NotSupportedException(
                $"Primary target texture [{atlasTarget.TextureObjectIndex}] must " +
                "use writable BGRA 0x32E3/0x43E3 for atlas transfer.");
        }
        ImportedTextureAtlasSourceGroup[] atlasSources = donorGroups
            .Select(group => new ImportedTextureAtlasSourceGroup(
                ResolveImportedGroupTexture(donor, group) ??
                    throw new InvalidDataException(
                        $"Material group {group.MaterialIndex} has no texture to atlas."),
                group.Meshes.Select(mesh => GetImportedMeshIndex(donor.Meshes, mesh))
                    .Order()
                    .ToArray(),
                $"material group {group.MaterialIndex}"))
            .ToArray();
        ImportedTextureAtlasRepackResult atlas =
            ImportedTextureAtlasRepacker.RepackToSingleAtlas(
                donor,
                atlasSources,
                atlasTarget.TextureWidth,
                atlasTarget.TextureHeight);
        return new ImportedMaterialAtlasPlan(
            atlas, atlasTarget.TextureObjectIndex);
    }

    private static ImportedGroupPairingPlan BuildSafeImportedGroupPairing(
        SmoDocument target,
        ImportedScene donor,
        ImportedSkeleton skeleton,
        IReadOnlyDictionary<string, string> boneRemap,
        IReadOnlyDictionary<string, Matrix4x4> targetBind,
        IReadOnlyDictionary<string, Matrix4x4> targetInverse,
        IReadOnlyList<VisualGroup> targetGroups,
        IReadOnlyList<ImportedGroup> donorGroups,
        IReadOnlySet<int> fullRgbaTextureIndices,
        IReadOnlySet<int> alphaBlendTextureIndices,
        SkinnedRenderableMaterialProfile materialProfile,
        int? requiredPrimaryTargetTextureObjectIndex = null,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (donorGroups.Count > MaximumSafeDonorMaterialGroups ||
            targetGroups.Count > MaximumSafeTargetMaterialGroups)
        {
            return new ImportedGroupPairingPlan(
                [],
                [],
                $"Safe material assignment supports at most " +
                $"{MaximumSafeDonorMaterialGroups} donor and " +
                $"{MaximumSafeTargetMaterialGroups} target groups, but this " +
                $"model requires {donorGroups.Count} and {targetGroups.Count}. " +
                "The exhaustive assignment was blocked to prevent an " +
                "unbounded calculation; merge materials before importing.");
        }

        ImportedTransferMesh[] dryMeshes;
        try
        {
            dryMeshes = BuildImportedTransferMeshes(
                    donor,
                    new GlbPlanContext(
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
                        [],
                        fullRgbaTextureIndices,
                        alphaBlendTextureIndices),
                    ReplacementTransform.Identity,
                    SkinnedGeometryTransferMode.PreservePreparedGeometry)
                .Select(ConvertImportedMeshToSmoSpace)
                .ToArray();
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          NotSupportedException or
                                          OverflowException)
        {
            return new ImportedGroupPairingPlan(
                [],
                [],
                "Material-group dry-run could not be initialized: " +
                exception.Message);
        }

        ImportedGroupPairingPlan direct = FindSafeImportedGroupMatching(
            target,
            donor,
            targetGroups,
            donorGroups,
            dryMeshes,
            boneRemap,
            targetInverse,
            alphaBlendTextureIndices,
            materialProfile,
            requiredPrimaryTargetTextureObjectIndex,
            cancellationToken);
        if (direct.IsSafe || donorGroups.Count < 2)
            return direct;

        ImportedGroup primary = donorGroups[0];
        ImportedGroup[] sameSource = donorGroups.Skip(1)
            .Where(group => HaveSameImportedTextureSource(primary, group))
            .ToArray();
        if (sameSource.Length == 0)
            return direct;

        int minimumMergeCount = Math.Max(
            1, donorGroups.Count - targetGroups.Count);
        for (int mergeCount = minimumMergeCount;
             mergeCount <= sameSource.Length;
             mergeCount++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            foreach (ImportedGroup[] selected in EnumerateCombinations(
                         sameSource, mergeCount, cancellationToken))
            {
                cancellationToken.ThrowIfCancellationRequested();
                HashSet<ImportedGroup> mergedGroups = selected.ToHashSet();
                ImportedMesh[] mergedMeshes = primary.Meshes
                    .Concat(selected.SelectMany(group => group.Meshes))
                    .ToArray();
                var mergedPrimary = new ImportedGroup(
                    primary.MaterialIndex,
                    primary.SourceTextureIndex,
                    primary.LegacyTextureName,
                    mergedMeshes,
                    mergedMeshes.Sum(mesh => mesh.TriangleIndices.Length / 3));
                ImportedGroup[] mergedDonors =
                [
                    mergedPrimary,
                    .. donorGroups.Skip(1).Where(group =>
                        !mergedGroups.Contains(group))
                ];
                ImportedGroupPairingPlan merged = FindSafeImportedGroupMatching(
                    target,
                    donor,
                    targetGroups,
                    mergedDonors,
                    dryMeshes,
                    boneRemap,
                    targetInverse,
                    alphaBlendTextureIndices,
                    materialProfile,
                    requiredPrimaryTargetTextureObjectIndex,
                    cancellationToken);
                if (!merged.IsSafe)
                    continue;

                string[] messages = selected.Select(group =>
                        $"Material group {group.MaterialIndex} could not keep a separate " +
                        "target visual branch and uses the same source texture as primary " +
                        $"material group {primary.MaterialIndex}. Its geometry was merged into " +
                        "the primary group; separate material/alpha flags are not preserved.")
                    .ToArray();
                return merged with { Messages = messages };
            }
        }
        return direct;
    }

    private static IEnumerable<ImportedGroup[]> EnumerateCombinations(
        IReadOnlyList<ImportedGroup> groups,
        int selectionCount,
        CancellationToken cancellationToken = default)
    {
        if (selectionCount <= 0 || selectionCount > groups.Count)
            yield break;
        int[] indices = Enumerable.Range(0, selectionCount).ToArray();
        int yielded = 0;
        while (true)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (++yielded > MaximumMaterialMergeCombinations)
            {
                throw new InvalidOperationException(
                    $"Material merge search exceeded its safe limit of " +
                    $"{MaximumMaterialMergeCombinations:N0} combinations. " +
                    "The action was blocked; merge equivalent donor materials " +
                    "before importing.");
            }
            yield return indices.Select(index => groups[index]).ToArray();
            int position = selectionCount - 1;
            while (position >= 0 &&
                   indices[position] == groups.Count - selectionCount + position)
            {
                position--;
            }
            if (position < 0)
                yield break;
            indices[position]++;
            for (int index = position + 1; index < selectionCount; index++)
                indices[index] = indices[index - 1] + 1;
        }
    }

    private static ImportedGroupPairingPlan FindSafeImportedGroupMatching(
        SmoDocument target,
        ImportedScene donor,
        IReadOnlyList<VisualGroup> targetGroups,
        IReadOnlyList<ImportedGroup> donorGroups,
        IReadOnlyList<ImportedTransferMesh> dryMeshes,
        IReadOnlyDictionary<string, string> boneRemap,
        IReadOnlyDictionary<string, Matrix4x4> targetInverse,
        IReadOnlySet<int> alphaBlendTextureIndices,
        SkinnedRenderableMaterialProfile materialProfile,
        int? requiredPrimaryTargetTextureObjectIndex,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (donorGroups.Count > targetGroups.Count)
        {
            return new ImportedGroupPairingPlan(
                [],
                [],
                $"No complete safe material-group assignment exists: {donorGroups.Count} " +
                $"donor groups require distinct texture branches, but the target has " +
                $"{targetGroups.Count}.");
        }

        VisualGroup[] orderedTargets = OrderTargetGroupsByPreference(target, targetGroups);
        if (requiredPrimaryTargetTextureObjectIndex is int requiredTarget &&
            !orderedTargets.Any(group =>
                group.TextureObjectIndex == requiredTarget))
        {
            return new ImportedGroupPairingPlan(
                [],
                [],
                $"Required target texture object [{requiredTarget}] is not present in " +
                "the skinned visual-group catalog.");
        }
        var compatible = new bool[donorGroups.Count, orderedTargets.Length];
        var edgeFailures = new string?[donorGroups.Count, orderedTargets.Length];
        byte[] dryRunScratch = GC.AllocateUninitializedArray<byte>(
            target.Data.Length);
        for (int donorIndex = 0; donorIndex < donorGroups.Count; donorIndex++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            for (int targetIndex = 0; targetIndex < orderedTargets.Length; targetIndex++)
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (donorIndex == 0 &&
                    requiredPrimaryTargetTextureObjectIndex is int pinnedTarget &&
                    orderedTargets[targetIndex].TextureObjectIndex != pinnedTarget)
                {
                    edgeFailures[donorIndex, targetIndex] =
                        $"The atlas was built for target texture object [{pinnedTarget}] " +
                        "and cannot be reassigned after UV packing.";
                    continue;
                }
                var pair = new ImportedGroupPair(
                    orderedTargets[targetIndex].TextureObjectIndex,
                    donorGroups[donorIndex]);
                try
                {
                    ValidateImportedGroupPairsDryRun(
                        target,
                        donor,
                        targetGroups,
                        dryMeshes,
                        [pair],
                        boneRemap,
                        targetInverse,
                        alphaBlendTextureIndices,
                        materialProfile,
                        dryRunScratch,
                        cancellationToken);
                    compatible[donorIndex, targetIndex] = true;
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
                    edgeFailures[donorIndex, targetIndex] = exception.Message;
                }
            }
        }

        string? completePlanFailure = null;
        int[]? assignment = FindDeterministicCapabilityMatching(
            compatible,
            targetAssignment =>
            {
                ImportedGroupPair[] candidate = Enumerable.Range(0, donorGroups.Count)
                    .Select(donorIndex => new ImportedGroupPair(
                        orderedTargets[targetAssignment[donorIndex]].TextureObjectIndex,
                        donorGroups[donorIndex]))
                    .ToArray();
                try
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    ValidateImportedGroupPairsDryRun(
                        target,
                        donor,
                        targetGroups,
                        dryMeshes,
                        candidate,
                        boneRemap,
                        targetInverse,
                        alphaBlendTextureIndices,
                        materialProfile,
                        dryRunScratch,
                        cancellationToken);
                    return true;
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
                    completePlanFailure ??= exception.Message;
                    return false;
                }
            },
            cancellationToken);
        if (assignment is not null)
        {
            ImportedGroupPair[] result = Enumerable.Range(0, donorGroups.Count)
                .Select(donorIndex => new ImportedGroupPair(
                    orderedTargets[assignment[donorIndex]].TextureObjectIndex,
                    donorGroups[donorIndex]))
                .ToArray();
            return new ImportedGroupPairingPlan(result, [], null);
        }

        string counts = string.Join(
            ", ",
            Enumerable.Range(0, donorGroups.Count).Select(donorIndex =>
                $"material group {donorGroups[donorIndex].MaterialIndex}=" +
                Enumerable.Range(0, orderedTargets.Length).Count(targetIndex =>
                    compatible[donorIndex, targetIndex])));
        string failure =
            $"No complete safe material-group assignment exists ({donorGroups.Count} " +
            $"donor groups, {targetGroups.Count} target groups). Compatible target " +
            $"counts: {counts}.";
        if (!string.IsNullOrWhiteSpace(completePlanFailure))
            failure += " Full-plan dry-run failed: " + completePlanFailure;
        else
        {
            string? firstEdgeFailure = Enumerable.Range(0, donorGroups.Count)
                .SelectMany(donorIndex => Enumerable.Range(0, orderedTargets.Length)
                    .Select(targetIndex => edgeFailures[donorIndex, targetIndex]))
                .FirstOrDefault(message => !string.IsNullOrWhiteSpace(message));
            if (!string.IsNullOrWhiteSpace(firstEdgeFailure))
                failure += " Example rejected pairing: " + firstEdgeFailure;
        }
        return new ImportedGroupPairingPlan([], [], failure);
    }

    internal static bool ShouldAttemptImportedMaterialAtlas(
        bool directPairingIsSafe,
        SkinnedTextureTransferMode textureTransferMode,
        bool allowMaterialAtlas,
        int donorGroupCount)
    {
        if (donorGroupCount < 0)
            throw new ArgumentOutOfRangeException(nameof(donorGroupCount));
        return !directPairingIsSafe &&
               textureTransferMode == SkinnedTextureTransferMode.ImportDonor &&
               allowMaterialAtlas &&
               donorGroupCount > 1;
    }

    internal static int[]? FindDeterministicCapabilityMatching(
        bool[,] compatibility,
        Func<IReadOnlyList<int>, bool>? validateCompleteAssignment = null,
        CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(compatibility);
        int donorCount = compatibility.GetLength(0);
        int targetCount = compatibility.GetLength(1);
        if (donorCount > targetCount)
            return null;

        var usedTargets = new bool[targetCount];
        var current = new int[donorCount];
        int[]? result = null;
        int visitedStates = 0;
        _ = Search(0);
        return result;

        bool Search(int donorIndex)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (++visitedStates > MaximumMaterialAssignmentSearchStates)
            {
                throw new InvalidOperationException(
                    $"Material assignment search exceeded its safe limit of " +
                    $"{MaximumMaterialAssignmentSearchStates:N0} states. " +
                    "The action was blocked; reduce or merge donor materials.");
            }
            if (donorIndex == donorCount)
            {
                int[] candidate = current.ToArray();
                if (validateCompleteAssignment is not null &&
                    !validateCompleteAssignment(Array.AsReadOnly(candidate)))
                {
                    return false;
                }
                result = candidate;
                return true;
            }

            for (int targetIndex = 0; targetIndex < targetCount; targetIndex++)
            {
                if (usedTargets[targetIndex] ||
                    !compatibility[donorIndex, targetIndex])
                {
                    continue;
                }
                usedTargets[targetIndex] = true;
                current[donorIndex] = targetIndex;
                if (Search(donorIndex + 1))
                    return true;
                usedTargets[targetIndex] = false;
            }
            return false;
        }
    }

    private static void ValidateImportedGroupPairsDryRun(
        SmoDocument target,
        ImportedScene donor,
        IReadOnlyList<VisualGroup> targetGroups,
        IReadOnlyList<ImportedTransferMesh> dryMeshes,
        IReadOnlyList<ImportedGroupPair> pairs,
        IReadOnlyDictionary<string, string> boneRemap,
        IReadOnlyDictionary<string, Matrix4x4> targetInverse,
        IReadOnlySet<int> alphaBlendTextureIndices,
        SkinnedRenderableMaterialProfile materialProfile,
        byte[]? reusableScratch = null,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        byte[] scratch = reusableScratch ??
            GC.AllocateUninitializedArray<byte>(target.Data.Length);
        if (scratch.Length < target.Data.Length)
            throw new ArgumentException(
                "Reusable dry-run buffer is smaller than the target SMO.",
                nameof(reusableScratch));
        target.Data.Span.CopyTo(scratch);
        foreach (ImportedGroupPair pair in pairs)
        {
            cancellationToken.ThrowIfCancellationRequested();
            VisualGroup targetGroup = targetGroups.Single(group =>
                group.TextureObjectIndex == pair.TargetTextureObjectIndex);
            ImportedTransferMesh[] groupMeshes = pair.Donor.Meshes.Select(mesh =>
                    dryMeshes.Single(value => value.Key ==
                        GetImportedMeshIndex(donor.Meshes, mesh)))
                .ToArray();
            ImportedTransferMesh[] paletteMeshes = groupMeshes;
            bool hasExplicitMaterial = groupMeshes.Any(mesh =>
                materialProfile.GetMode(mesh.Key) != SkinnedRenderableMaterialMode.Auto);
            if (alphaBlendTextureIndices.Contains(pair.Donor.SourceTextureIndex) ||
                hasExplicitMaterial)
            {
                ImportedTexture sourceTexture =
                    ResolveImportedGroupTexture(donor, pair.Donor) ??
                    throw new InvalidDataException(
                        $"Transparent material group {pair.Donor.MaterialIndex} " +
                        "has no source texture.");
                SmoSkinnedBranchSourceMesh[] splitMeshes = groupMeshes
                    .Select(ToSkinnedBranchSourceMesh)
                    .ToArray();
                SmoSkinnedRenderableOpacityPlan opacity =
                    SmoSkinnedBranchSplitBuilder.ClassifyRenderables(
                        splitMeshes, sourceTexture, materialProfile);
                splitMeshes = SmoSkinnedBranchSplitBuilder
                    .ConformCloseSurfaceNormals(splitMeshes, opacity)
                    .Meshes;
                if (opacity.SeparateBranchTriangleCount > 0)
                {
                    uint textureObjectId = target.Objects[targetGroup.TextureObjectIndex].Id;
                    _ = SmoSkinnedBranchSplitBuilder.Analyze(
                        target,
                        textureObjectId,
                        splitMeshes,
                        opacity,
                        boneRemap,
                        targetInverse);
                }
                paletteMeshes = FilterImportedTriangles(
                    groupMeshes, opacity, keepSeparateBranch: false);
            }
            if (paletteMeshes.Any(mesh => mesh.TriangleIndices.Length > 0))
            {
                PatchImportedPalettes(
                    scratch,
                    target,
                    targetGroup,
                    paletteMeshes,
                    boneRemap,
                    targetInverse,
                    cancellationToken);
            }
        }
    }

    private static VisualGroup[] OrderTargetGroupsByPreference(
        SmoDocument target,
        IReadOnlyList<VisualGroup> targetGroups)
    {
        VisualGroup[] ordered = targetGroups
            .OrderByDescending(group => group.Meshes.Sum(mesh => mesh.Mesh.TriangleCount))
            .ThenBy(group => group.TextureObjectIndex)
            .ToArray();
        VisualGroup primary = ordered
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
    /// fully opaque before any premultiplied atlas resize. This preserves RGB
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
            : "legacy-texture-name:" + textureName;
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

    private static bool HaveSameImportedTextureSource(
        ImportedGroup left,
        ImportedGroup right)
    {
        if (left.SourceTextureIndex != right.SourceTextureIndex)
            return false;
        return left.SourceTextureIndex != -1 || string.Equals(
            left.LegacyTextureName,
            right.LegacyTextureName,
            StringComparison.OrdinalIgnoreCase);
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
        SkinnedGeometryTransferMode transferMode)
    {
        Matrix4x4 global = transform.Matrix;
        Matrix4x4 normalGlobal = Matrix4x4.Invert(global, out Matrix4x4 inverseGlobal)
            ? Matrix4x4.Transpose(inverseGlobal)
            : global;
        return donor.Meshes.Select((mesh, meshIndex) =>
        {
            ImportedSkinning skinning = mesh.Skinning ?? throw new InvalidOperationException(
                $"Mesh {mesh.Name} has no skinning data.");
            Vector3[] normals = mesh.Normals.Length == mesh.Positions.Length
                ? mesh.Normals.ToArray()
                : GenerateImportedSmoothNormals(mesh);
            var positions = new Vector3[mesh.Positions.Length];
            var transferredNormals = new Vector3[mesh.Positions.Length];
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
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
                skinning);
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
        mesh.Skinning);

    private static ImportedTransferMesh[] FilterImportedTriangles(
        IReadOnlyList<ImportedTransferMesh> meshes,
        SmoSkinnedRenderableOpacityPlan opacity,
        bool keepSeparateBranch) => meshes.Select(mesh => mesh with
        {
            TriangleIndices = Enumerable.Range(
                    0, mesh.TriangleIndices.Length / 3)
                .Where(triangle =>
                    opacity.IsSeparateBranch(mesh.Key, triangle) ==
                    keepSeparateBranch)
                .SelectMany(triangle => mesh.TriangleIndices
                    .Skip(triangle * 3)
                    .Take(3))
                .ToArray()
        }).ToArray();

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
        ICollection<string> errors)
    {
        string[] targetLinks = SmoNodeHierarchy.Decode(target).Links
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
                     entry.TypeHash is SmoClassIds.Node or
                         SmoClassIds.RenderNode or SmoClassIds.Model))
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
                     entry.TypeHash == SmoClassIds.StaticRenderObject))
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

    private static void VerifyRetainedVisualStates(
        SmoDocument target,
        SmoDocument output,
        ICollection<string> errors)
    {
        Dictionary<uint, SmoObjectEntry> outputById = output.Objects
            .ToDictionary(entry => entry.Id);
        foreach (SmoObjectEntry targetMaterial in target.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MaterialData))
        {
            if (!outputById.TryGetValue(
                    targetMaterial.Id, out SmoObjectEntry? outputMaterial))
            {
                continue;
            }
            byte[] targetState = MaterialBytesWithoutTexturePayloads(
                target, targetMaterial);
            byte[] outputState = MaterialBytesWithoutTexturePayloads(
                output, outputMaterial);
            if (!targetState.AsSpan().SequenceEqual(outputState) &&
                !RetainedMaterialDiffersOnlyByTextureResize(
                    target, targetMaterial, output, outputMaterial))
            {
                errors.Add(
                    $"retained material ID {targetMaterial.Id} changed outside texture pixels");
            }
        }

        foreach (SmoObjectEntry targetSkinEntry in target.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            if (!outputById.TryGetValue(
                    targetSkinEntry.Id, out SmoObjectEntry? outputSkinEntry) ||
                !SmoSkinDecoder.TryDecode(
                    target, targetSkinEntry, out SmoSkin? targetSkin, out _) ||
                targetSkin is null ||
                !SmoSkinDecoder.TryDecode(
                    output, outputSkinEntry, out SmoSkin? outputSkin, out _) ||
                outputSkin is null)
            {
                continue;
            }
            if (targetSkin.AlphaSortEnable != outputSkin.AlphaSortEnable ||
                targetSkin.Priority != outputSkin.Priority)
            {
                errors.Add(
                    $"retained skin ID {targetSkinEntry.Id} changed sort/priority state");
            }
        }
    }

    private static bool RetainedMaterialDiffersOnlyByTextureResize(
        SmoDocument target,
        SmoObjectEntry targetMaterial,
        SmoDocument output,
        SmoObjectEntry outputMaterial)
    {
        SmoObjectEntry[] targetTextures = target.Objects.Where(entry =>
                entry.ParentIndex == targetMaterial.Index &&
                entry.TypeHash == SmoClassIds.TextureData)
            .OrderBy(entry => entry.Id)
            .ToArray();
        SmoObjectEntry[] outputTextures = output.Objects.Where(entry =>
                entry.ParentIndex == outputMaterial.Index &&
                entry.TypeHash == SmoClassIds.TextureData)
            .OrderBy(entry => entry.Id)
            .ToArray();
        if (targetTextures.Length == 0 ||
            !targetTextures.Select(entry => entry.Id)
                .SequenceEqual(outputTextures.Select(entry => entry.Id)))
        {
            return false;
        }

        long textureDelta = targetTextures.Zip(outputTextures).Sum(pair =>
            (long)pair.Second.SerializedSize - pair.First.SerializedSize);
        if ((long)outputMaterial.SerializedSize - targetMaterial.SerializedSize !=
            textureDelta)
        {
            return false;
        }

        if (!SmoMaterialRenderState.TryDecode(
                target, targetMaterial, out SmoMaterialRenderStateInfo? targetState) ||
            targetState is null ||
            !SmoMaterialRenderState.TryDecode(
                output, outputMaterial, out SmoMaterialRenderStateInfo? outputState) ||
            outputState is null ||
            targetState.FinalBlendOperation != outputState.FinalBlendOperation ||
            !targetState.MaterialRenderStates.SequenceEqual(
                outputState.MaterialRenderStates))
        {
            return false;
        }

        SMOTextureTool.Core.SmoDocument targetTextureDocument =
            SMOTextureTool.Core.SmoDocument.Parse(target.Data.Span);
        SMOTextureTool.Core.SmoDocument outputTextureDocument =
            SMOTextureTool.Core.SmoDocument.Parse(output.Data.Span);
        Dictionary<int, SMOTextureTool.Core.TextureInfo> targetByOffset =
            targetTextureDocument.Textures.ToDictionary(item => item.BlockOffset);
        Dictionary<int, SMOTextureTool.Core.TextureInfo> outputByOffset =
            outputTextureDocument.Textures.ToDictionary(item => item.BlockOffset);
        foreach ((SmoObjectEntry targetTexture, SmoObjectEntry outputTexture) in
                 targetTextures.Zip(outputTextures))
        {
            if (targetTexture.PhysicalOffset is < 0 or > int.MaxValue ||
                outputTexture.PhysicalOffset is < 0 or > int.MaxValue ||
                !targetByOffset.TryGetValue(
                    checked((int)targetTexture.PhysicalOffset),
                    out SMOTextureTool.Core.TextureInfo? targetInfo) ||
                !outputByOffset.TryGetValue(
                    checked((int)outputTexture.PhysicalOffset),
                    out SMOTextureTool.Core.TextureInfo? outputInfo) ||
                targetInfo.FormatCode != outputInfo.FormatCode ||
                targetInfo.Layout != outputInfo.Layout ||
                !SameRetainedMaterialState(targetInfo.Material, outputInfo.Material))
            {
                return false;
            }
        }
        return true;
    }

    private static bool SameRetainedMaterialState(
        SMOTextureTool.Core.MaterialReferenceInfo? target,
        SMOTextureTool.Core.MaterialReferenceInfo? output) =>
        target is not null && output is not null &&
        target.PassIndex == output.PassIndex &&
        target.LayerIndex == output.LayerIndex &&
        target.LayerClassId == output.LayerClassId &&
        target.FinalBlendOperation == output.FinalBlendOperation &&
        target.MaterialRenderStates.SequenceEqual(output.MaterialRenderStates) &&
        target.LayerTextureStates.SequenceEqual(output.LayerTextureStates);

    private static byte[] MaterialBytesWithoutTexturePayloads(
        SmoDocument document,
        SmoObjectEntry material)
    {
        byte[] data = document.Data.Span.Slice(
                checked((int)material.PhysicalOffset),
                checked((int)material.SerializedSize))
            .ToArray();
        foreach (SmoObjectEntry texture in document.Objects.Where(entry =>
                     entry.ParentIndex == material.Index &&
                     entry.TypeHash == SmoClassIds.TextureData))
        {
            long relative = texture.PhysicalOffset - material.PhysicalOffset;
            if (relative < 0 ||
                relative + texture.SerializedSize > data.Length)
            {
                throw new InvalidDataException(
                    $"Texture ID {texture.Id} is outside retained material ID {material.Id}.");
            }
            data.AsSpan(
                    checked((int)relative),
                    checked((int)texture.SerializedSize))
                .Clear();
        }
        return data;
    }

    private static IReadOnlyDictionary<int, ImportedTextureAssignment>
        BuildPairedTextureAssignments(
        ImportedScene donor,
        IReadOnlyList<ImportedGroupPair> groups,
        ImportedTexture? explicitTexture,
        SkinnedTextureTransferMode textureTransferMode,
        IReadOnlySet<int> fullRgbaTextureIndices,
        IReadOnlySet<int> alphaBlendTextureIndices)
    {
        if (textureTransferMode == SkinnedTextureTransferMode.PreserveTarget)
        {
            if (explicitTexture is not null)
            {
                throw new ArgumentException(
                    "An explicit texture cannot be combined with PreserveTarget.",
                    nameof(explicitTexture));
            }
            return new Dictionary<int, ImportedTextureAssignment>();
        }

        if (explicitTexture is not null)
        {
            if (groups.Count == 0)
            {
                throw new InvalidOperationException(
                    "Skinned import has no paired target texture object to replace.");
            }
            return groups.ToDictionary(
                pair => pair.TargetTextureObjectIndex,
                _ => new ImportedTextureAssignment(
                    explicitTexture.Data,
                    ReplaceAlpha: false,
                    EnableAlphaBlend: false));
        }

        var result = new Dictionary<int, ImportedTextureAssignment>();
        foreach (ImportedGroupPair pair in groups)
        {
            ImportedTexture? sourceTexture = ResolveImportedGroupTexture(donor, pair.Donor);
            if (sourceTexture is null)
                continue;
            bool replaceAlpha = fullRgbaTextureIndices.Contains(
                pair.Donor.SourceTextureIndex);
            bool enableAlphaBlend = alphaBlendTextureIndices.Contains(
                pair.Donor.SourceTextureIndex);
            if (!result.TryAdd(
                    pair.TargetTextureObjectIndex,
                    new ImportedTextureAssignment(
                        sourceTexture.Data,
                        replaceAlpha,
                        enableAlphaBlend)))
            {
                throw new InvalidOperationException(
                    $"Target texture object [{pair.TargetTextureObjectIndex}] is paired " +
                    "with more than one donor visual group.");
            }
        }
        return result;
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
        if (string.IsNullOrWhiteSpace(group.LegacyTextureName))
            return null;

        string legacyName = NormalizeImportedTextureName(group.LegacyTextureName);
        int[] exactMatches = donor.Textures
            .Select((texture, index) => (texture, index))
            .Where(item => string.Equals(
                NormalizeImportedTextureName(item.texture.Name),
                legacyName,
                StringComparison.OrdinalIgnoreCase))
            .Select(item => item.index)
            .ToArray();
        if (exactMatches.Length == 1)
            return donor.Textures[exactMatches[0]];
        if (exactMatches.Length > 1)
        {
            throw new InvalidDataException(
                $"Legacy base-color texture name \"{group.LegacyTextureName}\" for " +
                $"material group {group.MaterialIndex} is ambiguous in the donor texture catalog.");
        }

        string legacyStem = Path.GetFileNameWithoutExtension(legacyName);
        int[] stemMatches = donor.Textures
            .Select((texture, index) => (texture, index))
            .Where(item => string.Equals(
                Path.GetFileNameWithoutExtension(
                    NormalizeImportedTextureName(item.texture.Name)),
                legacyStem,
                StringComparison.OrdinalIgnoreCase))
            .Select(item => item.index)
            .ToArray();
        if (stemMatches.Length == 1)
            return donor.Textures[stemMatches[0]];
        if (stemMatches.Length > 1)
        {
            throw new InvalidDataException(
                $"Legacy base-color texture name \"{group.LegacyTextureName}\" for " +
                $"material group {group.MaterialIndex} is ambiguous in the donor texture catalog.");
        }
        throw new InvalidDataException(
            $"Legacy base-color texture \"{group.LegacyTextureName}\" for material " +
            $"group {group.MaterialIndex} was not found in the donor texture catalog.");
    }

    private static string NormalizeImportedTextureName(string name)
    {
        string trimmed = name.Trim();
        string fileName = Path.GetFileName(trimmed);
        return string.IsNullOrWhiteSpace(fileName) ? trimmed : fileName;
    }

    private static byte[] ReplacePairedTextureData(
        byte[] output,
        string? sourcePath,
        IReadOnlyDictionary<int, ImportedTextureAssignment>
            textureDataByObjectIndex)
    {
        if (textureDataByObjectIndex.Count == 0)
            throw new InvalidOperationException(
                "Skinned import has no paired target texture object to replace.");

        SmoDocument viewerDocument = SmoDocument.Parse(output, sourcePath);
        SMOTextureTool.Core.SmoDocument textureDocument =
            SMOTextureTool.Core.SmoDocument.Parse(output);
        Dictionary<int, SMOTextureTool.Core.TextureInfo> textureByBlockOffset =
            textureDocument.Textures
                .GroupBy(item => item.BlockOffset)
                .Where(group => group.Count() == 1)
                .ToDictionary(group => group.Key, group => group.Single());

        foreach (KeyValuePair<int, ImportedTextureAssignment> replacement in
                 textureDataByObjectIndex.OrderBy(item => item.Key))
        {
            int objectIndex = replacement.Key;
            ImportedTextureAssignment assignment = replacement.Value;
            if ((uint)objectIndex >= (uint)viewerDocument.Objects.Count)
                throw new InvalidDataException(
                    $"Paired texture object index {objectIndex} is outside the output catalog.");
            SmoObjectEntry textureEntry = viewerDocument.Objects[objectIndex];
            if (textureEntry.TypeHash != SmoClassIds.TextureData ||
                textureEntry.PhysicalOffset is < 0 or > int.MaxValue ||
                !textureByBlockOffset.TryGetValue(
                    checked((int)textureEntry.PhysicalOffset),
                    out SMOTextureTool.Core.TextureInfo? textureInfo))
            {
                throw new InvalidDataException(
                    $"Target texture object [{objectIndex}] {textureEntry.Name} " +
                    "cannot be mapped to the fixed-size texture table by physical offset.");
            }
            output = assignment.ReplaceAlpha
                ? FixedSizeTextureWriter.ReplaceRgbaWithoutDownscaling(
                    output, textureInfo.Index, assignment.Data)
                : FixedSizeTextureWriter.ReplaceRgbWithoutDownscaling(
                    output, textureInfo.Index, assignment.Data);
        }
        return output;
    }

    private static void VerifyTransferredTexturePixels(
        SmoDocument output,
        IReadOnlyDictionary<int, ImportedTextureAssignment> assignments,
        IReadOnlyDictionary<int, uint> targetTextureIdsByIndex,
        ICollection<string> errors)
    {
        foreach ((int objectIndex, ImportedTextureAssignment assignment) in
                 assignments.Where(item => item.Value.ReplaceAlpha))
        {
            uint objectId = targetTextureIdsByIndex[objectIndex];
            SmoObjectEntry? entry = output.Objects.SingleOrDefault(candidate =>
                candidate.Id == objectId &&
                candidate.TypeHash == SmoClassIds.TextureData);
            if (entry is null)
            {
                errors.Add(
                    $"transferred RGBA texture ID {objectId} is missing");
                continue;
            }
            if (!SmoTextureDecoder.TryDecode(
                    output, entry, out SmoTexture? texture, out string textureError) ||
                texture is null)
            {
                errors.Add(
                    $"transferred RGBA texture [{objectIndex}] cannot be decoded: " +
                    textureError);
                continue;
            }
            if (!ImportedTextureAtlasRepacker.SerializedBgraMatches(
                    assignment.Data,
                    texture.Width,
                    texture.Height,
                    texture.Bgra32Pixels.Span,
                    out string mismatch,
                    resizeToExpected: true))
            {
                errors.Add(
                    $"transferred RGBA texture [{objectIndex}] failed pixel " +
                    $"roundtrip: {mismatch}");
            }
        }
    }

    private static void VerifyUnreplacedTargetTexturesUnchanged(
        SmoDocument target,
        SmoDocument output,
        IReadOnlySet<int> replacedTextureObjectIndices,
        ICollection<string> errors)
    {
        foreach (SmoObjectEntry targetEntry in target.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.TextureData &&
                     !replacedTextureObjectIndices.Contains(entry.Index)))
        {
            SmoObjectEntry? outputEntry = output.Objects.SingleOrDefault(entry =>
                entry.Id == targetEntry.Id &&
                entry.TypeHash == SmoClassIds.TextureData);
            if (outputEntry is null)
            {
                errors.Add(
                    $"unreplaced target texture ID {targetEntry.Id} is missing");
                continue;
            }
            if (targetEntry.SerializedSize != outputEntry.SerializedSize ||
                targetEntry.SerializedSize > int.MaxValue ||
                !target.Data.Span.Slice(
                        checked((int)targetEntry.PhysicalOffset),
                        checked((int)targetEntry.SerializedSize))
                    .SequenceEqual(output.Data.Span.Slice(
                        checked((int)outputEntry.PhysicalOffset),
                        checked((int)outputEntry.SerializedSize))))
            {
                errors.Add(
                    $"unreplaced target texture [{targetEntry.Index}] {targetEntry.Name} changed");
            }
        }
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
        IReadOnlyDictionary<string, float> influences,
        GeometryWeightSemantics weightSemantics)
    {
        if (!IsFinite(position) || !float.IsFinite(uv.X) || !float.IsFinite(uv.Y))
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
            FloatBits(uv.X), FloatBits(uv.Y), weights);
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
        VisualGroup group) =>
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

    private static void PatchImportedPalettes(
        Span<byte> output,
        SmoDocument target,
        VisualGroup targetGroup,
        IReadOnlyList<ImportedTransferMesh> donorMeshes,
        IReadOnlyDictionary<string, string> boneRemap,
        IReadOnlyDictionary<string, Matrix4x4> targetInverseBind,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        long paletteSearchStarted = System.Diagnostics.Stopwatch.GetTimestamp();
        var slots = targetGroup.Meshes.Select(mesh =>
        {
            string[] originalNames = mesh.Skin.Bones.Select(bone =>
                target.Objects[bone.NodeObjectIndex].Name).ToArray();
            // An inline entry owns a serialized target subtree, so rewriting
            // any slot in that palette would risk replacing target structure.
            // Dedicated one-bone palettes are target-owned attachment slots.
            bool fixedPalette = mesh.Skin.Bones.Any(
                    bone => bone.InlineSerializedSize != 0) ||
                originalNames.Distinct(StringComparer.Ordinal).Count() == 1;
            return new PaletteSlotPlan(
                mesh,
                fixedPalette,
                mesh.Skin.Bones.Count,
                fixedPalette
                    ? originalNames.ToHashSet(StringComparer.Ordinal)
                    : new HashSet<string>(StringComparer.Ordinal));
        })
            .ToArray();
        HashSet<string>[] fixedPalettes = slots
            .Where(slot => slot.Fixed)
            .Select(slot => slot.Bones)
            .ToArray();
        PaletteSlotPlan[] writableSlots = slots
            .Where(slot => !slot.Fixed)
            .ToArray();
        var requirements = new Dictionary<string, HashSet<string>>(
            StringComparer.Ordinal);
        foreach (ImportedTransferMesh mesh in donorMeshes)
        {
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                if ((index & 0x3FFF) == 0)
                    CheckPaletteSearchBudget();
                int[] vertices =
                [
                    checked((int)mesh.TriangleIndices[index]),
                    checked((int)mesh.TriangleIndices[index + 1]),
                    checked((int)mesh.TriangleIndices[index + 2])
                ];
                HashSet<string> bones = GetImportedTriangleBones(mesh, vertices, boneRemap);
                if (!fixedPalettes.Any(palette => bones.IsSubsetOf(palette)))
                {
                    string key = string.Join(
                        "\0", bones.Order(StringComparer.Ordinal));
                    requirements.TryAdd(key, bones);
                    if (requirements.Count > MaximumPaletteRequirementSets)
                    {
                        throw new PaletteSearchLimitException(
                            $"Material group needs more than the safely searchable " +
                            $"palette requirement limit (over " +
                            $"{MaximumPaletteRequirementSets:N0} distinct triangle " +
                            "bone sets). The exact palette search was blocked to " +
                            "protect system responsiveness.");
                    }
                }
            }
        }
        HashSet<string>[] uniqueRequirements = requirements.Values
            .OrderByDescending(bones => bones.Count)
            .ThenBy(
                bones => string.Join("\0", bones.Order(StringComparer.Ordinal)),
                StringComparer.Ordinal)
            .ToArray();
        if (uniqueRequirements.Length > MaximumPaletteRequirementSets)
        {
            throw new PaletteSearchLimitException(
                $"Material group needs more than the safely searchable palette " +
                $"requirement limit ({uniqueRequirements.Length:N0} distinct triangle " +
                $"bone sets; limit {MaximumPaletteRequirementSets:N0}). " +
                "The exact palette search was blocked to protect system responsiveness.");
        }
        // A bin containing a maximal triangle set also contains all of its
        // subsets. Removing proper subsets makes the exact search smaller
        // without changing feasibility.
        var maximal = new List<HashSet<string>>(uniqueRequirements.Length);
        foreach (HashSet<string> candidate in uniqueRequirements)
        {
            CheckPaletteSearchBudget();
            if (!maximal.Any(candidate.IsProperSubsetOf))
                maximal.Add(candidate);
        }
        HashSet<string>[] maximalRequirements = maximal.ToArray();
        if (maximalRequirements.Length > MaximumMaximalPaletteRequirements)
        {
            throw new PaletteSearchLimitException(
                $"Material group needs more than the safely searchable maximal " +
                $"palette requirement limit ({maximalRequirements.Length:N0}; limit " +
                $"{MaximumMaximalPaletteRequirements:N0}). The exact recursive " +
                "palette search was blocked to protect system responsiveness.");
        }
        var bins = writableSlots
            .Select(slot => new HashSet<string>(slot.Bones, StringComparer.Ordinal))
            .ToArray();
        var failedStates = new HashSet<string>(StringComparer.Ordinal);
        int visitedPackingStates = 0;
        var assignedRequirements = new bool[maximalRequirements.Length];
        // A deterministic best-overlap pass solves the common layout without
        // entering the recursive search. It is only a valid-solution fast path:
        // when an early choice paints itself into a corner, the exact search
        // below still starts from the untouched original bins.
        if (!TryPackGreedy() && !TryPack(0))
        {
            throw new InvalidOperationException(
                $"Material group needs more than {writableSlots.Length} writable " +
                "16-bone palettes after preserving fixed target palettes.");
        }
        for (int index = 0; index < writableSlots.Length; index++)
            writableSlots[index].Bones.UnionWith(bins[index]);

        bool TryPack(int assignedCount)
        {
            CheckPaletteSearchBudget();
            if (++visitedPackingStates > MaximumPalettePackingSearchStates)
            {
                throw new PaletteSearchLimitException(
                    $"Material group needs more than the safely searchable 16-bone " +
                    $"palette layout ({MaximumPalettePackingSearchStates:N0} exact " +
                    "packing states inspected). The action was blocked to protect " +
                    "system responsiveness; simplify the generated weights or split " +
                    "the model.");
            }
            if (assignedCount == maximalRequirements.Length)
                return true;
            string state = string.Join(",", Enumerable.Range(
                    0, maximalRequirements.Length)
                .Where(index => !assignedRequirements[index])) + "|" +
                string.Join("|", bins
                .Select((bin, index) =>
                    $"{writableSlots[index].Capacity}:" +
                    string.Join(",", bin.Order(StringComparer.Ordinal)))
                .Order(StringComparer.Ordinal));
            if (!failedStates.Add(state))
                return false;

            int requirementIndex = -1;
            int[] candidateBins = [];
            for (int candidateIndex = 0;
                 candidateIndex < maximalRequirements.Length;
                 candidateIndex++)
            {
                if (assignedRequirements[candidateIndex])
                    continue;

                HashSet<string> candidate = maximalRequirements[candidateIndex];
                int[] feasibleBins = Enumerable.Range(0, bins.Length)
                    .Where(index => bins[index].Union(candidate)
                        .Distinct(StringComparer.Ordinal).Count() <=
                        writableSlots[index].Capacity)
                    .OrderBy(index => candidate.Count(name => !bins[index].Contains(name)))
                    .ThenByDescending(index => candidate.Count(bins[index].Contains))
                    .ThenBy(index => index)
                    .GroupBy(index => writableSlots[index].Capacity + ":" +
                        string.Join(",", bins[index].Order(StringComparer.Ordinal)),
                        StringComparer.Ordinal)
                    .Select(group => group.First())
                    .ToArray();
                if (feasibleBins.Length == 0)
                    return false;
                if (requirementIndex >= 0 &&
                    (feasibleBins.Length > candidateBins.Length ||
                     feasibleBins.Length == candidateBins.Length &&
                     candidate.Count <= maximalRequirements[requirementIndex].Count))
                {
                    continue;
                }

                requirementIndex = candidateIndex;
                candidateBins = feasibleBins;
            }
            if (requirementIndex < 0)
                throw new InvalidOperationException(
                    "Palette search lost its unassigned requirement state.");

            HashSet<string> required = maximalRequirements[requirementIndex];
            assignedRequirements[requirementIndex] = true;
            foreach (int binIndex in candidateBins)
            {
                HashSet<string> bin = bins[binIndex];
                string[] added = required.Where(name => !bin.Contains(name)).ToArray();
                bin.UnionWith(added);
                if (TryPack(assignedCount + 1))
                    return true;
                bin.ExceptWith(added);
            }
            assignedRequirements[requirementIndex] = false;
            return false;
        }

        bool TryPackGreedy()
        {
            HashSet<string>[] candidateBins = bins
                .Select(bin => new HashSet<string>(bin, StringComparer.Ordinal))
                .ToArray();
            foreach (HashSet<string> required in maximalRequirements)
            {
                CheckPaletteSearchBudget();
                int selectedBin = Enumerable.Range(0, candidateBins.Length)
                    .Where(index => candidateBins[index].Union(required)
                        .Distinct(StringComparer.Ordinal).Count() <=
                        writableSlots[index].Capacity)
                    .OrderBy(index => required.Count(name =>
                        !candidateBins[index].Contains(name)))
                    .ThenByDescending(index => required.Count(
                        candidateBins[index].Contains))
                    .ThenByDescending(index => writableSlots[index].Capacity -
                        candidateBins[index].Count)
                    .ThenBy(index => index)
                    .DefaultIfEmpty(-1)
                    .First();
                if (selectedBin < 0)
                    return false;
                candidateBins[selectedBin].UnionWith(required);
            }

            for (int index = 0; index < bins.Length; index++)
            {
                bins[index].Clear();
                bins[index].UnionWith(candidateBins[index]);
            }
            return true;
        }

        void CheckPaletteSearchBudget()
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (System.Diagnostics.Stopwatch.GetElapsedTime(paletteSearchStarted) <=
                MaximumPalettePackingWallTime)
            {
                return;
            }

            throw new PaletteSearchLimitException(
                $"Material group needs more than the safe " +
                $"{MaximumPalettePackingWallTime.TotalSeconds:N0}-second exact " +
                "palette-search budget. The action was blocked to protect system " +
                "responsiveness; simplify the generated weights or split the model.");
        }

        string fallback = boneRemap.Values.Order(StringComparer.Ordinal).First();
        var patchedSkins = new HashSet<int>();
        foreach (PaletteSlotPlan slot in slots.Where(slot => !slot.Fixed))
        {
            if (!patchedSkins.Add(slot.Source.SkinEntry.Index))
                continue;
            List<string> names = slot.Bones.Order(StringComparer.Ordinal).ToList();
            if (names.Count == 0)
                names.Add(fallback);
            if (names.Count > slot.Capacity)
                throw new InvalidOperationException(
                    $"Target skin [{slot.Source.SkinEntry.Index}] palette capacity exceeded.");
            while (names.Count < slot.Source.Skin.Bones.Count)
                names.Add(names[0]);
            PatchImportedPalette(
                output, target, slot.Source.SkinEntry, names, targetInverseBind);
        }
    }

    private sealed record PaletteSlotPlan(
        MeshSource Source,
        bool Fixed,
        int Capacity,
        HashSet<string> Bones);

    private static void PatchImportedPalette(
        Span<byte> output,
        SmoDocument target,
        SmoObjectEntry skinEntry,
        IReadOnlyList<string> names,
        IReadOnlyDictionary<string, Matrix4x4> inverseBindByName)
    {
        ReadOnlySpan<byte> serialized = target.Data.Span.Slice(
            checked((int)skinEntry.PhysicalOffset), checked((int)skinEntry.SerializedSize));
        int fieldOffset = 8;
        SmoDataBlockHeader palette = default;
        bool found = false;
        while (fieldOffset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(
                   serialized, fieldOffset, out SmoDataBlockHeader header))
        {
            if (header.FieldType == 0 && header.PayloadSize >= 8)
            {
                ReadOnlySpan<byte> payload = serialized.Slice(
                    header.PayloadOffset, checked((int)header.PayloadSize));
                if (BinaryPrimitives.ReadUInt32LittleEndian(payload) == 0 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]) == names.Count)
                {
                    palette = header;
                    found = true;
                }
            }
            fieldOffset = checked((int)header.PayloadEnd);
        }
        if (!found)
            throw new InvalidOperationException(
                $"Palette field target skin [{skinEntry.Index}] not found.");

        Dictionary<string, SmoObjectEntry> targetNodes = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Node &&
                !string.IsNullOrWhiteSpace(entry.Name))
            .GroupBy(entry => entry.Name, StringComparer.Ordinal)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single(),
                StringComparer.Ordinal);
        int cursor = checked((int)skinEntry.PhysicalOffset + palette.PayloadOffset + 8);
        foreach (string name in names)
        {
            if (!targetNodes.TryGetValue(name, out SmoObjectEntry? node) ||
                !inverseBindByName.TryGetValue(name, out Matrix4x4 inverseBind))
                throw new InvalidOperationException(
                    $"Target bone {name} has no unique node/inverse bind matrix.");
            uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(output[(cursor + 4)..]);
            if (inlineSize != 0)
                throw new InvalidOperationException(
                    $"Target skin [{skinEntry.Index}] contains inline data in a writable palette.");
            WriteUInt32(output, cursor, node.Id);
            cursor += 8;
            WriteMatrix(output[cursor..], inverseBind);
            cursor += 16 * sizeof(float);
        }
    }

    private static HashSet<string> GetImportedTriangleBones(
        ImportedTransferMesh mesh,
        IReadOnlyList<int> vertices,
        IReadOnlyDictionary<string, string> boneRemap)
    {
        var result = new HashSet<string>(StringComparer.Ordinal);
        foreach (int vertex in vertices)
        {
            Vector4 weights = mesh.Skinning.Weights[vertex];
            ImportedJointIndices joints = mesh.Skinning.JointIndices[vertex];
            Add(weights.X, joints.X);
            Add(weights.Y, joints.Y);
            Add(weights.Z, joints.Z);
            Add(weights.W, joints.W);
        }
        return result;

        void Add(float weight, ushort joint)
        {
            if (weight <= WeightEpsilon)
                return;
            string donorName = mesh.Skinning.Skeleton.JointNames[joint];
            result.Add(boneRemap[donorName]);
        }
    }

    private static byte[] BuildImportedMeshObject(ImportedTargetSlot slot)
    {
        MeshSource template = slot.Source;
        int vertexCount = slot.VertexRecords.Count;
        int indexCount = slot.Indices.Count;
        int indexBytes = checked(indexCount * sizeof(ushort));
        int vertexBytes = checked(vertexCount * template.Mesh.Stride);
        const int preambleSize = 17;
        const int primitiveHeaderSize = 12;
        const int vertexHeaderSize = 12;
        int payloadSize = checked(
            preambleSize + primitiveHeaderSize + indexBytes + vertexHeaderSize + vertexBytes);
        byte[] result = new byte[checked(8 + 5 + payloadSize + 1)];
        WriteUInt32(result, 0, SmoClassIds.MeshData);
        "SBOO"u8.CopyTo(result.AsSpan(4));
        result[8] = SmoMeshDecoder.E1Marker;
        WriteUInt32(result, 9, (uint)payloadSize);
        int payload = 13;
        WriteUInt32(result, payload, template.Mesh.VertexFormat);
        WriteUInt32(result, payload + 4, (uint)vertexCount);
        WriteUInt32(result, payload + 8,
            checked((uint)(vertexCount * template.Mesh.RuntimeStride)));
        WriteUInt32(result, payload + 12, (uint)indexBytes);
        result[payload + 16] = 0;
        int primitive = payload + preambleSize;
        WriteUInt32(result, primitive, SmoMeshDecoder.TriangleListPrimitive);
        WriteUInt32(result, primitive + 4, checked((uint)(indexCount / 3)));
        WriteUInt32(result, primitive + 8, 0);
        int indices = primitive + primitiveHeaderSize;
        for (int index = 0; index < indexCount; index++)
            WriteUInt16(result, indices + index * sizeof(ushort), slot.Indices[index]);
        int vertexHeader = indices + indexBytes;
        WriteUInt32(result, vertexHeader, template.Mesh.VertexFormat);
        WriteUInt32(result, vertexHeader + 4, (uint)vertexCount);
        WriteUInt32(result, vertexHeader + 8, 0);
        int vertices = vertexHeader + vertexHeaderSize;
        foreach (byte[] record in slot.VertexRecords)
        {
            record.CopyTo(result.AsSpan(vertices));
            vertices += record.Length;
        }
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
