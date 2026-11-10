using System.Numerics;

namespace SmoImporter.Core;

/// <summary>Rigid donor geometry in its native wing node's local frame, with its material assignments.</summary>
public sealed record SmoWingRigidAttachment(
    SmoWingReference Reference,
    IReadOnlyList<int> SourceComponentIndices,
    ImportedScene LocalScene)
{
    public IReadOnlyList<TargetRigBodyVertexMembership> SourceMemberships { get; init; } = [];
}

/// <summary>The two fitted rigid branches and their exact target Spine_03 owner.</summary>
public sealed record SmoWingAttachmentDraft(int SpineObjectIndex, SmoWingRigidAttachment Left, SmoWingRigidAttachment Right)
{
    public string SourceTopologyFingerprint { get; init; } = string.Empty;
    public string SourceTargetRigFingerprint { get; init; } = string.Empty;

    public static SmoWingAttachmentDraft Create(SmoWingFitResult fit, TargetRigFittingPoseSnapshot pose,
        IReadOnlyList<GeneratedSkinningComponentInfo> selectedComponents, CancellationToken cancellationToken = default)
    {
        int spine = pose.Definition.GetJointIndex("Spine_03");
        var components = selectedComponents.ToDictionary(component => component.ComponentIndex);
        return new SmoWingAttachmentDraft(pose.Definition.Joints[spine].ObjectIndex,
            CreateSide(fit.Scene, fit.Left, pose.WorldMatrices[spine], components, cancellationToken),
            CreateSide(fit.Scene, fit.Right, pose.WorldMatrices[spine], components, cancellationToken))
        {
            SourceTopologyFingerprint = SmoWingSkinnedModelWriter.ComputeTopologyFingerprint(fit.Scene),
            SourceTargetRigFingerprint = pose.TargetRigFingerprint
        };
    }

    private static SmoWingRigidAttachment CreateSide(ImportedScene scene, SmoWingSideFit side, Matrix4x4 spineWorld,
        IReadOnlyDictionary<int, GeneratedSkinningComponentInfo> components, CancellationToken cancellationToken)
    {
        Matrix4x4 nodeWorld = side.Reference.NodeLocalToSpine * spineWorld;
        if (!Matrix4x4.Invert(nodeWorld, out Matrix4x4 inverseNodeWorld) || nodeWorld.GetDeterminant() <= 0)
            throw new InvalidDataException($"The fitted wing node {side.Reference.NodeName} has an invalid attachment transform.");

        var meshes = new List<ImportedMesh>();
        var membersByMesh = side.Components.SelectMany(index => components[index].VerticesByMesh)
            .GroupBy(member => member.MeshIndex).OrderBy(group => group.Key);
        foreach (var group in membersByMesh)
        {
            cancellationToken.ThrowIfCancellationRequested();
            HashSet<int> membership = group.SelectMany(member => member.VertexIndices).ToHashSet();
            meshes.Add(ExtractMesh(scene.Meshes[group.Key], membership, inverseNodeWorld, Matrix4x4.Transpose(nodeWorld),
                cancellationToken));
        }

        return new SmoWingRigidAttachment(side.Reference, side.Components, scene with { Meshes = meshes.AsReadOnly() })
        {
            SourceMemberships = side.Components.SelectMany(index => components[index].VerticesByMesh)
                .Select(member => member with { VertexIndices = member.VertexIndices.ToArray() }).ToArray()
        };
    }

    private static ImportedMesh ExtractMesh(ImportedMesh source, IReadOnlySet<int> membership, Matrix4x4 positionTransform,
        Matrix4x4 normalTransform, CancellationToken cancellationToken)
    {
        var sourceToLocal = new Dictionary<int, uint>();
        var sourceVertices = new List<int>();
        var triangles = new List<uint>();
        for (int i = 0; i < source.TriangleIndices.Length; i += 3)
        {
            if ((i & 4095) == 0)
                cancellationToken.ThrowIfCancellationRequested();
            int a = checked((int)source.TriangleIndices[i]);
            int b = checked((int)source.TriangleIndices[i + 1]);
            int c = checked((int)source.TriangleIndices[i + 2]);
            bool hasA = membership.Contains(a);
            bool hasB = membership.Contains(b);
            bool hasC = membership.Contains(c);
            if (!hasA && !hasB && !hasC)
                continue;
            if (!hasA || !hasB || !hasC)
                throw new InvalidDataException($"A triangle of mesh '{source.Name}' crosses a fitted wing boundary.");

            triangles.Add(Remap(a));
            triangles.Add(Remap(b));
            triangles.Add(Remap(c));
        }
        if (triangles.Count == 0)
            throw new InvalidDataException($"Mesh '{source.Name}' has no wing triangles.");

        Vector3[] positions = sourceVertices.Select(index => Vector3.Transform(source.Positions[index], positionTransform)).ToArray();
        Vector3[] normals = CopyAttribute(source.Normals, source.Positions.Length, sourceVertices);
        for (int i = 0; i < normals.Length; i++)
        {
            Vector3 normal = Vector3.TransformNormal(normals[i], normalTransform);
            normals[i] = normal.LengthSquared() > 0 ? Vector3.Normalize(normal) : Vector3.Zero;
        }
        return source with
        {
            Positions = positions,
            Normals = normals,
            TextureCoordinates = CopyAttribute(source.TextureCoordinates, source.Positions.Length, sourceVertices),
            SecondaryTextureCoordinates = CopyAttribute(source.SecondaryTextureCoordinates, source.Positions.Length, sourceVertices),
            DiffuseColorsArgb = CopyAttribute(source.DiffuseColors, source.Positions.Length, sourceVertices),
            TriangleIndices = triangles.ToArray(),
            Skinning = null
        };

        uint Remap(int vertex)
        {
            if (!sourceToLocal.TryGetValue(vertex, out uint local))
            {
                local = checked((uint)sourceVertices.Count);
                sourceVertices.Add(vertex);
                sourceToLocal.Add(vertex, local);
            }
            return local;
        }
    }

    private static T[] CopyAttribute<T>(T[] source, int originalCount, IReadOnlyList<int> indices) =>
        source.Length == originalCount ? indices.Select(index => source[index]).ToArray() : [];
}
