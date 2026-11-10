using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;
using SmoExporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>Writes skinned body geometry and independent animated rigid wing branches as one verified output.</summary>
internal static class SmoWingSkinnedModelWriter
{
    private sealed record BodySelection(ImportedScene Scene, SkinnedRenderableMaterialProfile Materials);

    internal static GlbSkinTransferPlan Analyze(SmoDocument target, ImportedScene donor,
        SkinnedRenderableMaterialProfile? materialProfile, SmoWingAttachmentDraft wings, CancellationToken cancellationToken)
    {
        BodySelection body = SelectBody(target, donor, materialProfile, wings, cancellationToken);
        GlbSkinTransferPlan plan = SmoSkinnedVisualGraphPipeline.AnalyzeSkinnedModel(target, body.Scene, body.Materials, cancellationToken);
        int wingMeshes = wings.Left.LocalScene.Meshes.Count + wings.Right.LocalScene.Meshes.Count;
        return plan with
        {
            MeshCount = body.Scene.Meshes.Count + wingMeshes,
            MaterialGroupCount = plan.MaterialGroupCount + wings.Left.LocalScene.Meshes.Select(mesh => mesh.MaterialIndex).Distinct().Count() +
                wings.Right.LocalScene.Meshes.Select(mesh => mesh.MaterialIndex).Distinct().Count(),
            Messages = plan.Messages.Append("Selected wing surfaces will be written as separate rigid visuals on animated WingL/WingR nodes.").ToArray()
        };
    }

    internal static GlbSkinTransferResult Replace(SmoDocument target, ImportedScene donor,
        ReplacementTransform transform, string outputPath, SkinnedGeometryTransferMode transferMode,
        SkinnedRenderableMaterialProfile? materialProfile, SmoWingAttachmentDraft wings, CancellationToken cancellationToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        if (transform != ReplacementTransform.Identity)
            throw new InvalidOperationException("The wing attachment draft already includes placement. Mixed replacement requires the identity transform.");
        BodySelection body = SelectBody(target, donor, materialProfile, wings, cancellationToken);
        wings = ResolveWingMaterialModes(wings, materialProfile ?? SkinnedRenderableMaterialProfile.Default);
        string output = Path.GetFullPath(outputPath);
        if (target.SourcePath is { } source && string.Equals(Path.GetFullPath(source), output, StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("The output path must differ from the target input path.");
        string directory = Path.GetDirectoryName(output) ?? throw new InvalidOperationException("The output directory is missing.");
        Directory.CreateDirectory(directory);
        string bodyPath = Path.Combine(directory, $".{Path.GetFileName(output)}.{Guid.NewGuid():N}.body.smo");
        try
        {
            GlbSkinTransferResult bodyResult = SmoSkinnedVisualGraphPipeline.RebuildSkinnedModel(target, body.Scene,
                transform, bodyPath, transferMode, body.Materials, cancellationToken);
            cancellationToken.ThrowIfCancellationRequested();
            SmoDocument bodyDocument = SmoDocument.Load(bodyPath);
            SmoWingVisualGraphAppendResult appended = SmoWingVisualGraphAppender.Append(target, bodyDocument, wings, cancellationToken);
            int triangleCount = bodyResult.TriangleCount + CountTriangles(wings.Left.LocalScene) + CountTriangles(wings.Right.LocalScene);
            SmoDocument? verified = null;
            SmoVerifiedOutputInstallResult installed = SmoVerifiedOutputInstaller.Install(output, appended.Data, temporary =>
            {
                cancellationToken.ThrowIfCancellationRequested();
                verified = SmoDocument.Load(temporary);
                VerifyBodyUnchanged(bodyDocument, verified, appended.RelocatedBodyTextureIds);
                SmoWingVisualGraphAppender.Verify(verified, wings, appended);
                var scene = SmoSceneBuilder.Build(verified);
                int actualTriangles = scene.MeshPlacements.Sum(placement =>
                    scene.Meshes.Single(mesh => mesh.VariantKey == placement.EffectiveMeshKey).TriangleIndices
                        .Chunk(3).Count(triangle => triangle.Length == 3 && triangle[0] != triangle[1] && triangle[1] != triangle[2] && triangle[0] != triangle[2]));
                if (actualTriangles != triangleCount)
                    throw new InvalidDataException($"The mixed character has {actualTriangles} triangles, expected {triangleCount}.");
                cancellationToken.ThrowIfCancellationRequested();
            }, cancellationToken, target.SourcePath);
            return new GlbSkinTransferResult(installed.OutputPath,
                verified!.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData),
                bodyResult.VertexCount + wings.Left.LocalScene.Meshes.Sum(mesh => mesh.Positions.Length) +
                    wings.Right.LocalScene.Meshes.Sum(mesh => mesh.Positions.Length),
                triangleCount, bodyResult.PaletteCount, verified.Data.Length, installed.Sha256);
        }
        finally
        {
            if (File.Exists(bodyPath))
                File.Delete(bodyPath);
        }
    }

    private static BodySelection SelectBody(SmoDocument target, ImportedScene donor,
        SkinnedRenderableMaterialProfile? materialProfile, SmoWingAttachmentDraft wings, CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        ArgumentNullException.ThrowIfNull(wings);
        materialProfile ??= SkinnedRenderableMaterialProfile.Default;
        materialProfile.ValidateSource(donor);
        if ((uint)wings.SpineObjectIndex >= (uint)target.Objects.Count ||
            target.Objects[wings.SpineObjectIndex].Name != "Spine_03" ||
            wings.SourceTargetRigFingerprint != TargetRigDefinition.ComputeSourceFingerprint(target) ||
            wings.SourceTopologyFingerprint != ComputeTopologyFingerprint(donor))
            throw new InvalidDataException("The wing attachment draft belongs to another target or donor topology.");
        if (wings.Left.Reference.NodeName != "WingL" || wings.Right.Reference.NodeName != "WingR")
            throw new InvalidDataException("The wing attachment draft has invalid native node names.");

        var membership = new Dictionary<int, HashSet<int>>();
        foreach (SmoWingRigidAttachment side in new[] { wings.Left, wings.Right })
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (side.SourceMemberships.Count == 0 || side.LocalScene.Meshes.Count == 0 || side.LocalScene.HasSkinning)
                throw new InvalidDataException("A wing attachment must contain explicit rigid donor surfaces.");
            foreach (TargetRigBodyVertexMembership member in side.SourceMemberships)
            {
                if ((uint)member.MeshIndex >= (uint)donor.Meshes.Count || donor.Meshes[member.MeshIndex].Name != member.MeshName)
                    throw new InvalidDataException("A wing attachment refers to another donor mesh.");
                if (!membership.TryGetValue(member.MeshIndex, out HashSet<int>? vertices))
                    membership.Add(member.MeshIndex, vertices = []);
                foreach (int vertex in member.VertexIndices)
                    if ((uint)vertex >= (uint)donor.Meshes[member.MeshIndex].Positions.Length || !vertices.Add(vertex))
                        throw new InvalidDataException("Wing attachments contain missing or overlapping donor vertices.");
            }
        }

        var bodyMeshes = new List<ImportedMesh>();
        var bodyMaterialModes = new List<SkinnedRenderableMaterialOverride>();
        int removedTriangles = 0;
        for (int index = 0; index < donor.Meshes.Count; index++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            ImportedMesh mesh = donor.Meshes[index];
            ImportedMesh? bodyMesh = mesh;
            if (membership.TryGetValue(index, out HashSet<int>? wingVertices))
            {
                var triangles = new List<uint>();
                for (int offset = 0; offset < mesh.TriangleIndices.Length; offset += 3)
                {
                    uint a = mesh.TriangleIndices[offset], b = mesh.TriangleIndices[offset + 1], c = mesh.TriangleIndices[offset + 2];
                    bool first = wingVertices.Contains(checked((int)a));
                    bool second = wingVertices.Contains(checked((int)b));
                    bool third = wingVertices.Contains(checked((int)c));
                    if (first || second || third)
                    {
                        if (!first || !second || !third)
                            throw new InvalidDataException("A donor triangle crosses a wing attachment boundary.");
                        removedTriangles++;
                    }
                    else
                        triangles.AddRange([a, b, c]);
                }
                bodyMesh = triangles.Count == 0 ? null : CompactMesh(mesh, triangles);
            }
            if (bodyMesh is null)
                continue;
            SkinnedRenderableMaterialMode mode = materialProfile.GetMode(index);
            if (mode != SkinnedRenderableMaterialMode.Auto)
                bodyMaterialModes.Add(new SkinnedRenderableMaterialOverride(bodyMeshes.Count, mode));
            bodyMeshes.Add(bodyMesh);
        }
        int expectedWingTriangles = CountTriangles(wings.Left.LocalScene) + CountTriangles(wings.Right.LocalScene);
        if (removedTriangles != expectedWingTriangles || bodyMeshes.Count == 0)
            throw new InvalidDataException("The body and wing surfaces do not form the complete donor geometry.");
        ImportedScene scene = donor with { Meshes = bodyMeshes.AsReadOnly() };
        return new BodySelection(scene, bodyMaterialModes.Count == 0 ? SkinnedRenderableMaterialProfile.Default :
            new SkinnedRenderableMaterialProfile(scene, bodyMaterialModes));
    }

    private static ImportedMesh CompactMesh(ImportedMesh source, IReadOnlyList<uint> triangles)
    {
        int[] vertices = triangles.Distinct().Select(vertex => checked((int)vertex)).Order().ToArray();
        var mapping = vertices.Select((vertex, index) => (vertex, index)).ToDictionary(item => item.vertex, item => checked((uint)item.index));
        T[] SelectAttribute<T>(T[] values) => values.Length == source.Positions.Length ? vertices.Select(vertex => values[vertex]).ToArray() : [];
        return source with
        {
            Positions = SelectAttribute(source.Positions),
            Normals = SelectAttribute(source.Normals),
            TextureCoordinates = SelectAttribute(source.TextureCoordinates),
            SecondaryTextureCoordinates = SelectAttribute(source.SecondaryTextureCoordinates),
            DiffuseColorsArgb = SelectAttribute(source.DiffuseColors),
            TriangleIndices = triangles.Select(vertex => mapping[checked((int)vertex)]).ToArray(),
            Skinning = source.Skinning is { } skin ? skin with
            {
                JointIndices = SelectAttribute(skin.JointIndices),
                Weights = SelectAttribute(skin.Weights)
            } : null
        };
    }

    private static SmoWingAttachmentDraft ResolveWingMaterialModes(SmoWingAttachmentDraft wings, SkinnedRenderableMaterialProfile profile) =>
        wings with
        {
            Left = ResolveSideMaterialModes(wings.Left, profile),
            Right = ResolveSideMaterialModes(wings.Right, profile)
        };

    private static SmoWingRigidAttachment ResolveSideMaterialModes(SmoWingRigidAttachment side, SkinnedRenderableMaterialProfile profile)
    {
        int[] sourceMeshes = side.SourceMemberships.Select(member => member.MeshIndex).Distinct().Order().ToArray();
        if (sourceMeshes.Length != side.LocalScene.Meshes.Count)
            throw new InvalidDataException("Wing material assignments no longer match the source mesh membership.");
        if (!sourceMeshes.Any(index => profile.GetMode(index) != SkinnedRenderableMaterialMode.Auto))
            return side;
        var meshes = side.LocalScene.Meshes.ToArray();
        var materials = side.LocalScene.Materials.ToList();
        for (int index = 0; index < meshes.Length; index++)
        {
            SkinnedRenderableMaterialMode mode = profile.GetMode(sourceMeshes[index]);
            if (mode == SkinnedRenderableMaterialMode.Auto)
                continue;
            ImportedMesh mesh = meshes[index];
            if ((uint)mesh.MaterialIndex >= (uint)materials.Count)
                throw new InvalidDataException("An explicit wing material mode has no source material.");
            materials.Add(materials[mesh.MaterialIndex] with
            {
                AlphaMode = mode == SkinnedRenderableMaterialMode.TransparentSurface
                    ? ImportedMaterialAlphaMode.Blend : ImportedMaterialAlphaMode.Opaque
            });
            meshes[index] = mesh with { MaterialIndex = materials.Count - 1 };
        }
        return side with { LocalScene = side.LocalScene with { Meshes = meshes, SourceMaterials = materials.AsReadOnly() } };
    }

    internal static string ComputeTopologyFingerprint(ImportedScene donor)
    {
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        Span<byte> integer = stackalloc byte[4];
        BinaryPrimitives.WriteInt32LittleEndian(integer, donor.Meshes.Count);
        hash.AppendData(integer);
        foreach (ImportedMesh mesh in donor.Meshes)
        {
            byte[] name = Encoding.UTF8.GetBytes(mesh.Name);
            BinaryPrimitives.WriteInt32LittleEndian(integer, name.Length);
            hash.AppendData(integer);
            hash.AppendData(name);
            BinaryPrimitives.WriteInt32LittleEndian(integer, mesh.Positions.Length);
            hash.AppendData(integer);
            BinaryPrimitives.WriteInt32LittleEndian(integer, mesh.TriangleIndices.Length);
            hash.AppendData(integer);
            foreach (uint vertex in mesh.TriangleIndices)
            {
                BinaryPrimitives.WriteUInt32LittleEndian(integer, vertex);
                hash.AppendData(integer);
            }
        }
        return Convert.ToHexString(hash.GetHashAndReset());
    }

    private static int CountTriangles(ImportedScene scene) => scene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);

    private static void VerifyBodyUnchanged(SmoDocument before, SmoDocument after, IReadOnlySet<uint> relocatedTextureIds)
    {
        if (after.HasErrors)
            throw new InvalidDataException("The mixed character failed strict SMO parsing.");
        // The sole permitted body-material byte change is demoting a relocated
        // inline texture to the same ID/zero-size reference. Build that exact
        // expected field stream; its temporary dangling references are never
        // used to load materials or decode body geometry.
        SmoDocument expectedMaterials = before;
        foreach (uint textureId in relocatedTextureIds.Order())
        {
            SmoObjectEntry texture = expectedMaterials.Objects.Single(entry => entry.Id == textureId &&
                entry.TypeHash == SmoClassIds.TextureData);
            SmoObjectEntry owner = expectedMaterials.Objects[texture.ParentIndex ??
                throw new InvalidDataException("The relocated body texture has no inline owner.")];
            if (owner.TypeHash != SmoClassIds.MaterialData)
                throw new InvalidDataException("The relocated body texture has an unsupported owner.");
            expectedMaterials = SmoDocument.ParseOwned(SmoVisualForestInjector.DemoteInlineLeafToReference(
                expectedMaterials, owner.Id, 10, textureId), before.SourcePath);
            if (expectedMaterials.HasErrors)
                throw new InvalidDataException("The expected body texture reference failed structural verification.");
        }
        var existingIds = after.Objects.ToDictionary(entry => entry.Id);
        foreach (SmoObjectEntry entry in before.Objects)
        {
            if (!existingIds.TryGetValue(entry.Id, out SmoObjectEntry? actual) || entry.TypeHash != actual.TypeHash || entry.Name != actual.Name)
                throw new InvalidDataException($"Body object {entry.Id} changed while attaching wings.");
            if (entry.TypeHash == SmoClassIds.Node)
            {
                if (!SmoNodeDecoder.TryDecode(before, entry, out SmoNodeData? originalNode) || originalNode is null ||
                    !SmoNodeDecoder.TryDecode(after, actual, out SmoNodeData? currentNode) || currentNode is null ||
                    originalNode.Position != currentNode.Position || originalNode.Rotation != currentNode.Rotation ||
                    originalNode.Scale != currentNode.Scale || originalNode.IsBone != currentNode.IsBone ||
                    originalNode.IsAnimated != currentNode.IsAnimated || originalNode.IsStatic != currentNode.IsStatic ||
                    originalNode.BillboardAxis != currentNode.BillboardAxis)
                    throw new InvalidDataException($"Body node {entry.Id} changed while attaching wings.");
            }
            if (entry.TypeHash is SmoClassIds.MaterialData or SmoClassIds.TextureData)
            {
                SmoDocument expectedDocument = entry.TypeHash == SmoClassIds.MaterialData ? expectedMaterials : before;
                SmoObjectEntry expectedEntry = expectedDocument.Objects.Single(candidate => candidate.Id == entry.Id);
                ReadOnlySpan<byte> originalBytes = expectedDocument.Data.Span.Slice(
                    checked((int)expectedEntry.PhysicalOffset), checked((int)expectedEntry.SerializedSize));
                ReadOnlySpan<byte> appendedBytes = after.Data.Span.Slice(checked((int)actual.PhysicalOffset), checked((int)actual.SerializedSize));
                if (!originalBytes.SequenceEqual(appendedBytes))
                    throw new InvalidDataException($"Body material or texture {entry.Id} changed while attaching wings.");
            }
            if (entry.TypeHash != SmoClassIds.MeshData)
                continue;
            SmoMesh expected = SmoMeshDecoder.Decode(before, entry);
            SmoMesh current = SmoMeshDecoder.Decode(after, actual);
            if (!expected.Positions.SequenceEqual(current.Positions) || !expected.Normals.SequenceEqual(current.Normals) ||
                !expected.TriangleIndices.SequenceEqual(current.TriangleIndices) ||
                !expected.TextureCoordinates.SequenceEqual(current.TextureCoordinates) ||
                !expected.TextureCoordinates1.SequenceEqual(current.TextureCoordinates1) ||
                !expected.BlendWeights.SequenceEqual(current.BlendWeights) ||
                !expected.BlendIndices.SequenceEqual(current.BlendIndices))
                throw new InvalidDataException($"Body geometry {entry.Id} changed while attaching wings.");
        }
    }
}
