using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
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
/// Experimental GLB skin transfer. The complete target SMO graph stays intact;
/// only existing mesh leaves and reference-only skin palettes are rewritten.
/// </summary>
public static class SmoSkinnedGlbReplacer
{
    public static GlbSkinTransferPlan Analyze(
        SmoDocument target,
        ImportedScene donor) =>
        SmoVisualTransplanter.AnalyzeSkinnedGlb(target, donor);

    public static GlbSkinTransferResult Replace(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform transform,
        string outputPath,
        SkinnedGeometryTransferMode transferMode =
            SkinnedGeometryTransferMode.PreservePreparedGeometry,
        ImportedTexture? texture = null) =>
        SmoVisualTransplanter.TransplantSkinnedGlb(
            target, donor, transform, outputPath, transferMode, texture);

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
        ImportedTexture? texture = null) =>
        Replace(
            target,
            donor,
            transform,
            outputPath,
            rebaseToTargetBindPose
                ? SkinnedGeometryTransferMode.RetargetToGameBindPose
                : SkinnedGeometryTransferMode.PreservePreparedGeometry,
            texture);
}

internal static partial class SmoVisualTransplanter
{
    private sealed record GlbPlanContext(
        GlbSkinTransferPlan PublicPlan,
        ImportedSkeleton Skeleton,
        IReadOnlyDictionary<string, string> BoneRemap,
        IReadOnlyDictionary<string, Matrix4x4> TargetBindWorld,
        IReadOnlyDictionary<string, Matrix4x4> TargetInverseBind,
        IReadOnlyList<ImportedGroupPair> Groups);

    private sealed record ImportedGroup(
        int MaterialIndex,
        IReadOnlyList<ImportedMesh> Meshes,
        int TriangleCount);

    private sealed record ImportedGroupPair(
        int TargetTextureObjectIndex,
        ImportedGroup Donor);

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
        ImportedScene donor)
    {
        GlbPlanContext context = BuildGlbPlan(target, donor);
        return context.PublicPlan;
    }

    internal static GlbSkinTransferResult TransplantSkinnedGlb(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform transform,
        string outputPath,
        SkinnedGeometryTransferMode transferMode,
        ImportedTexture? texture)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        GlbPlanContext context = BuildGlbPlan(target, donor);
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

        ImportedTransferMesh[] transferMeshes = BuildImportedTransferMeshes(
            donor, context, transform, transferMode);
        byte[] patchedBytes = target.Data.ToArray();
        VisualGroup[] initialTargetGroups = BuildGroups(
            target, SmoTextureBindingResolver.ResolveAll(target));
        Dictionary<int, ImportedGroupPair> pairsByTexture = context.Groups
            .ToDictionary(pair => pair.TargetTextureObjectIndex);

        foreach (VisualGroup targetGroup in initialTargetGroups)
        {
            if (!pairsByTexture.TryGetValue(
                    targetGroup.TextureObjectIndex, out ImportedGroupPair? pair))
                continue;
            ImportedTransferMesh[] groupMeshes = pair.Donor.Meshes.Select(mesh =>
                transferMeshes.Single(value => value.Key ==
                    GetImportedMeshIndex(donor.Meshes, mesh))).ToArray();
            PatchImportedPalettes(
                patchedBytes, target, targetGroup, groupMeshes,
                context.BoneRemap, context.TargetInverseBind);
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
                foreach (ImportedTransferMesh mesh in groupMeshes)
                {
                    for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
                    {
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
        HashSet<int> pairedTextureObjectIndices = context.Groups
            .Select(pair => pair.TargetTextureObjectIndex)
            .ToHashSet();
        if (texture is not null)
        {
            output = ReplacePairedTextureRgb(
                output,
                target.SourcePath,
                pairedTextureObjectIndices,
                texture.Data);
        }

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
            if (verified.Objects.Count != target.Objects.Count)
                errors.Add("target object count changed");
            if (!verified.Objects.Zip(target.Objects).All(pair =>
                    pair.First.Id == pair.Second.Id &&
                    pair.First.Name == pair.Second.Name &&
                    pair.First.TypeHash == pair.Second.TypeHash &&
                    pair.First.ParentIndex == pair.Second.ParentIndex))
                errors.Add("target object identities or parent topology changed");
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
            VerifyTargetPaletteBindMatrices(target, verified, errors);
            VerifyTargetBindFrameIdentity(target, verified, errors);
            VerifyUnpairedTargetTexturesUnchanged(
                target, verified, pairedTextureObjectIndices, errors);
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
        ImportedScene donor)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        var errors = new List<string>();
        var warnings = new List<string>();
        if (donor.Meshes.Count == 0)
            errors.Add("GLB не содержит meshes.");
        if (donor.Meshes.Any(mesh => mesh.Skinning is null))
            errors.Add("Все GLB primitives должны содержать JOINTS_0/WEIGHTS_0 и ссылаться на skin.");
        ImportedSkeleton? skeleton = donor.Meshes
            .Select(mesh => mesh.Skinning?.Skeleton)
            .FirstOrDefault(value => value is not null);
        if (skeleton is null)
        {
            errors.Add("GLB не содержит поддерживаемый skin.");
            return EmptyGlbPlan(donor, errors);
        }
        if (donor.Meshes.Any(mesh => mesh.Skinning is not null &&
                (!mesh.Skinning.Skeleton.JointNames.SequenceEqual(skeleton.JointNames) ||
                 mesh.Skinning.Skeleton.InverseBindMatrices.Count !=
                 skeleton.InverseBindMatrices.Count)))
            errors.Add("Все primitives должны использовать один и тот же skeleton.");

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
        var targetBind = targetBindByIndex
            .Where(item => target.Objects[item.Key].TypeHash == SmoClassIds.Node)
            .ToDictionary(item => target.Objects[item.Key].Name, item => item.Value,
                StringComparer.Ordinal);
        var targetInverse = new Dictionary<string, Matrix4x4>(StringComparer.Ordinal);
        foreach ((string name, Matrix4x4 bind) in targetBind)
            if (Matrix4x4.Invert(bind, out Matrix4x4 inverse))
                targetInverse[name] = inverse;

        var donorBind = new Dictionary<string, Matrix4x4>(StringComparer.Ordinal);
        for (int index = 0; index < skeleton.JointNames.Count; index++)
        {
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
            string? sharedTextureName = GetImportedVisualGroupTextureName(
                donor, group.First());
            if (sharedTextureName is not null && materialIndices.Length > 1)
            {
                warnings.Add(
                    $"Material states {string.Join(", ", materialIndices)} use the shared atlas " +
                    $"\"{sharedTextureName}\" and are merged into one primary visual group; " +
                    "the target primary material state is preserved.");
            }
        }
        ImportedGroup[] donorGroups = groupedDonorMeshes
            .Select(group => new ImportedGroup(
                group.Min(mesh => mesh.MaterialIndex),
                group.ToArray(),
                group.Sum(mesh => mesh.TriangleIndices.Length / 3)))
            .OrderByDescending(group => group.TriangleCount)
            .ThenBy(group => group.MaterialIndex)
            .ToArray();
        if (donorGroups.Length > targetGroups.Length)
            errors.Add(
                $"GLB material groups ({donorGroups.Length}) не помещаются в target texture groups ({targetGroups.Length}).");
        ImportedGroupPair[] groupPairs = [];
        if (targetGroups.Length > 0 && donorGroups.Length > 0 &&
            donorGroups.Length <= targetGroups.Length)
        {
            VisualGroup[] orderedTargets = targetGroups
                .OrderByDescending(group => group.Meshes.Sum(mesh => mesh.Mesh.TriangleCount))
                .ThenBy(group => group.TextureObjectIndex)
                .ToArray();
            VisualGroup primaryTarget = orderedTargets
                .FirstOrDefault(group => !IsRigidTargetGroup(target, group)) ??
                orderedTargets[0];
            var primaryMeshes = donorGroups[0].Meshes.ToList();
            var pairs = new List<ImportedGroupPair>();
            var unusedTargets = new List<VisualGroup>(
                orderedTargets.Where(group => group.TextureObjectIndex !=
                    primaryTarget.TextureObjectIndex));
            foreach (ImportedGroup donorGroup in donorGroups.Skip(1))
            {
                int compatibleIndex = unusedTargets.FindIndex(candidate =>
                    !IsRigidTargetGroup(target, candidate) ||
                    ImportedGroupFitsPreservedPalettes(
                        target, candidate, donorGroup, remap));
                if (compatibleIndex >= 0)
                {
                    VisualGroup selected = unusedTargets[compatibleIndex];
                    unusedTargets.RemoveAt(compatibleIndex);
                    pairs.Add(new ImportedGroupPair(
                        selected.TextureObjectIndex, donorGroup));
                }
                else
                {
                    primaryMeshes.AddRange(donorGroup.Meshes);
                    warnings.Add(
                        $"Material group {donorGroup.MaterialIndex} нельзя помещать во " +
                        "вложенную однокостную target-ветку. Geometry объединена с основным " +
                        "body group; отдельные material/alpha flags этого primitive не сохраняются.");
                }
            }
            pairs.Add(new ImportedGroupPair(
                primaryTarget.TextureObjectIndex,
                new ImportedGroup(
                    donorGroups[0].MaterialIndex,
                    primaryMeshes,
                    primaryMeshes.Sum(mesh => mesh.TriangleIndices.Length / 3))));
            groupPairs = pairs.ToArray();
        }

        if (errors.Count == 0)
        {
            try
            {
                ImportedTransferMesh[] dryMeshes = BuildImportedTransferMeshes(
                    donor,
                    new GlbPlanContext(
                        new GlbSkinTransferPlan(
                            SmoSkeletonCompatibility.Exact, donor.Meshes.Count,
                            donorGroups.Length, skeleton.JointNames.Count, usedJoints.Count,
                            differentBindPose, matched, remapped, [], [], []),
                        skeleton, remap, targetBind, targetInverse, groupPairs),
                    ReplacementTransform.Identity,
                    SkinnedGeometryTransferMode.PreservePreparedGeometry);
                byte[] scratch = target.Data.ToArray();
                foreach (ImportedGroupPair pair in groupPairs)
                {
                    VisualGroup targetGroup = targetGroups.Single(group =>
                        group.TextureObjectIndex == pair.TargetTextureObjectIndex);
                    ImportedTransferMesh[] groupMeshes = pair.Donor.Meshes.Select(mesh =>
                        dryMeshes.Single(value => value.Key ==
                            GetImportedMeshIndex(donor.Meshes, mesh))).ToArray();
                    PatchImportedPalettes(
                        scratch, target, targetGroup, groupMeshes, remap, targetInverse);
                }
            }
            catch (Exception exception) when (exception is InvalidDataException or
                                              InvalidOperationException or
                                              OverflowException)
            {
                errors.Add("План palettes не построен: " + exception.Message);
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
            publicPlan, skeleton, remap, targetBind, targetInverse, groupPairs);
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
            emptySkeleton,
            new Dictionary<string, string>(),
            new Dictionary<string, Matrix4x4>(),
            new Dictionary<string, Matrix4x4>(),
            []);
    }

    private static string GetImportedVisualGroupKey(
        ImportedScene donor,
        ImportedMesh mesh)
    {
        string? textureName = GetImportedVisualGroupTextureName(donor, mesh);
        return textureName is null
            ? "material:" + mesh.MaterialIndex.ToString(
                System.Globalization.CultureInfo.InvariantCulture)
            : "texture:" + textureName;
    }

    private static string? GetImportedVisualGroupTextureName(
        ImportedScene donor,
        ImportedMesh mesh)
    {
        if ((uint)mesh.MaterialIndex >= (uint)donor.Materials.Count)
            return null;
        string? sourceName = donor.Materials[mesh.MaterialIndex].BaseColorTextureName;
        if (string.IsNullOrWhiteSpace(sourceName))
            return null;
        string fileName = Path.GetFileName(sourceName.Trim());
        return string.IsNullOrWhiteSpace(fileName) ? sourceName.Trim() : fileName;
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

    private static void VerifyTargetNodeInvariants(
        SmoDocument target,
        SmoDocument output,
        ICollection<string> errors)
    {
        string[] targetLinks = SmoNodeHierarchy.Decode(target).Links
            .Select(link =>
                $"{link.ParentObjectIndex}:{link.ChildObjectIndex}:{link.ChildObjectId}")
            .Order(StringComparer.Ordinal)
            .ToArray();
        string[] outputLinks = SmoNodeHierarchy.Decode(output).Links
            .Select(link =>
                $"{link.ParentObjectIndex}:{link.ChildObjectIndex}:{link.ChildObjectId}")
            .Order(StringComparer.Ordinal)
            .ToArray();
        if (!targetLinks.SequenceEqual(outputLinks, StringComparer.Ordinal))
            errors.Add("target node hierarchy links changed");

        foreach (SmoObjectEntry targetEntry in target.Objects.Where(entry =>
                     entry.TypeHash is SmoClassIds.Node or
                         SmoClassIds.RenderNode or SmoClassIds.Model))
        {
            SmoObjectEntry outputEntry = output.Objects[targetEntry.Index];
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
            SmoObjectEntry outputEntry = output.Objects[targetEntry.Index];
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

    private static byte[] ReplacePairedTextureRgb(
        byte[] output,
        string? sourcePath,
        IReadOnlySet<int> pairedTextureObjectIndices,
        ReadOnlySpan<byte> imageData)
    {
        if (pairedTextureObjectIndices.Count == 0)
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

        foreach (int objectIndex in pairedTextureObjectIndices.Order())
        {
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
            output = FixedSizeTextureWriter.ReplaceRgb(
                output, textureInfo.Index, imageData);
        }
        return output;
    }

    private static void VerifyUnpairedTargetTexturesUnchanged(
        SmoDocument target,
        SmoDocument output,
        IReadOnlySet<int> pairedTextureObjectIndices,
        ICollection<string> errors)
    {
        foreach (SmoObjectEntry targetEntry in target.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.TextureData &&
                     !pairedTextureObjectIndices.Contains(entry.Index)))
        {
            SmoObjectEntry outputEntry = output.Objects[targetEntry.Index];
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
                    $"unpaired target texture [{targetEntry.Index}] {targetEntry.Name} changed");
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
        var canonicalInverse = new Dictionary<int, Matrix4x4>();
        foreach ((int nodeIndex, Matrix4x4 bindWorld) in targetBindWorld)
        {
            if (Matrix4x4.Invert(bindWorld, out Matrix4x4 inverseBind) &&
                IsFinite(inverseBind))
            {
                canonicalInverse[nodeIndex] = inverseBind;
            }
        }

        Dictionary<int, Matrix4x4[]> originalMatrices = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .Select(entry => SmoSkinDecoder.TryDecode(
                target, entry, out SmoSkin? skin, out _) ? skin : null)
            .Where(skin => skin is not null)
            .SelectMany(skin => skin!.Bones)
            .GroupBy(bone => bone.NodeObjectIndex)
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
                bool canonical = canonicalInverse.TryGetValue(
                        bone.NodeObjectIndex, out Matrix4x4 expected) &&
                    ApproximatelyEqual(bone.InverseBindMatrix, expected, 0.0001f);
                bool preservedOriginal = originalMatrices.TryGetValue(
                        bone.NodeObjectIndex, out Matrix4x4[]? candidates) &&
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
        IReadOnlyDictionary<int, Matrix4x4> targetBindWorld =
            SmoSkinBindingResolver.ResolveBindWorldMatrices(target);
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
                    if (!targetBindWorld.TryGetValue(
                            bone.NodeObjectIndex, out Matrix4x4 bindWorld))
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
                string second = ImportedCorner(
                    mesh, skinning, checked((int)mesh.TriangleIndices[index + 1]), boneRemap);
                string third = ImportedCorner(
                    mesh, skinning, checked((int)mesh.TriangleIndices[index + 2]), boneRemap);
                triangles.Add(CanonicalTriangle(first, second, third));
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
        return GeometryCorner(mesh.Positions[vertex], uv, influences);

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
        return GeometryCorner(mesh.Positions[vertex], uv, influences);

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
        IReadOnlyDictionary<string, float> influences)
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
                float normalized = item.Value / total;
                int quantized = checked((int)MathF.Round(normalized * 1_000_000f));
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

    private static bool ImportedGroupFitsPreservedPalettes(
        SmoDocument target,
        VisualGroup targetGroup,
        ImportedGroup donorGroup,
        IReadOnlyDictionary<string, string> boneRemap)
    {
        HashSet<string>[] palettes = targetGroup.Meshes.Select(mesh =>
            mesh.Skin.Bones.Select(bone =>
                    target.Objects[bone.NodeObjectIndex].Name)
                .ToHashSet(StringComparer.Ordinal)).ToArray();
        foreach (ImportedMesh mesh in donorGroup.Meshes)
        {
            ImportedSkinning skinning = mesh.Skinning ??
                throw new InvalidOperationException($"Mesh {mesh.Name} has no skinning data.");
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                var bones = new HashSet<string>(StringComparer.Ordinal);
                AddVertex(checked((int)mesh.TriangleIndices[index]));
                AddVertex(checked((int)mesh.TriangleIndices[index + 1]));
                AddVertex(checked((int)mesh.TriangleIndices[index + 2]));
                if (!palettes.Any(palette => bones.IsSubsetOf(palette)))
                    return false;

                void AddVertex(int vertex)
                {
                    Vector4 weights = skinning.Weights[vertex];
                    ImportedJointIndices joints = skinning.JointIndices[vertex];
                    Add(weights.X, joints.X);
                    Add(weights.Y, joints.Y);
                    Add(weights.Z, joints.Z);
                    Add(weights.W, joints.W);
                }

                void Add(float weight, ushort joint)
                {
                    if (weight <= WeightEpsilon)
                        return;
                    string donorName = skinning.Skeleton.JointNames[joint];
                    bones.Add(boneRemap[donorName]);
                }
            }
        }
        return true;
    }

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
        IReadOnlyDictionary<string, Matrix4x4> targetInverseBind)
    {
        var slots = targetGroup.Meshes.Select(mesh =>
        {
            bool fixedPalette = mesh.Skin.Bones.Any(
                    bone => bone.InlineSerializedSize != 0) ||
                mesh.Skin.Bones.Select(bone =>
                        target.Objects[bone.NodeObjectIndex].Name)
                    .Distinct(StringComparer.Ordinal).Count() == 1;
            return new PaletteSlotPlan(
            mesh,
            fixedPalette,
            fixedPalette
                ? mesh.Skin.Bones.Select(bone =>
                    target.Objects[bone.NodeObjectIndex].Name).ToHashSet(StringComparer.Ordinal)
                : new HashSet<string>(StringComparer.Ordinal));
        })
            .ToArray();
        foreach (ImportedTransferMesh mesh in donorMeshes)
        {
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                int[] vertices =
                [
                    checked((int)mesh.TriangleIndices[index]),
                    checked((int)mesh.TriangleIndices[index + 1]),
                    checked((int)mesh.TriangleIndices[index + 2])
                ];
                HashSet<string> bones = GetImportedTriangleBones(mesh, vertices, boneRemap);
                PaletteSlotPlan? selected = slots
                    .Where(slot => slot.Fixed
                        ? bones.All(slot.Bones.Contains)
                        : slot.Bones.Union(bones).Distinct(StringComparer.Ordinal).Count() <= 16)
                    .OrderByDescending(slot => slot.Fixed)
                    .ThenBy(slot => bones.Count(name => !slot.Bones.Contains(name)))
                    .ThenBy(slot => slot.TriangleCount)
                    .FirstOrDefault();
                if (selected is null)
                    throw new InvalidOperationException(
                        $"Material group needs more 16-bone palettes; triangle uses: " +
                        string.Join(", ", bones.Order(StringComparer.Ordinal)) + ".");
                if (!selected.Fixed)
                    selected.Bones.UnionWith(bones);
                selected.TriangleCount++;
            }
        }

        string fallback = boneRemap.Values.First();
        var patchedSkins = new HashSet<int>();
        foreach (PaletteSlotPlan slot in slots.Where(slot => !slot.Fixed))
        {
            if (!patchedSkins.Add(slot.Source.SkinEntry.Index))
                continue;
            List<string> names = slot.Bones.Order(StringComparer.Ordinal).ToList();
            if (names.Count == 0)
                names.Add(fallback);
            while (names.Count < slot.Source.Skin.Bones.Count)
                names.Add(names[0]);
            if (names.Count != slot.Source.Skin.Bones.Count)
                throw new InvalidOperationException(
                    $"Target skin [{slot.Source.SkinEntry.Index}] palette capacity exceeded.");
            PatchImportedPalette(
                output, target, slot.Source.SkinEntry, names, targetInverseBind);
        }
    }

    private sealed record PaletteSlotPlan(
        MeshSource Source,
        bool Fixed,
        HashSet<string> Bones)
    {
        public int TriangleCount { get; set; }
    }

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
