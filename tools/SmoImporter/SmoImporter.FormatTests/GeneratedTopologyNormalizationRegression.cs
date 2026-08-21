using System.Numerics;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class GeneratedTopologyNormalizationRegression
{
    public static void Run(string targetPath)
    {
        targetPath = Path.GetFullPath(targetPath);
        byte[] targetBefore = File.ReadAllBytes(targetPath);
        SmoDocument target = SmoDocument.Load(targetPath);
        SmoExportScene targetScene = SmoSceneBuilder.Build(
            target,
            new SmoExportOptions(
                ApplyWorldTransforms: true,
                AnimationPaths: null,
                Resources: SmoExportResourceTypes.Meshes |
                           SmoExportResourceTypes.Skeleton));
        ImportedScene clean = BuildUnskinnedTargetSurface(targetScene);
        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
        var alignment = new ReplacementTransform(1, Vector3.Zero, Vector3.Zero);
        TargetRigFittingPoseSnapshot neutral = TargetRigBodyPoseMapper.CreateSnapshot(
            rig, TargetRigBodyPoseParameters.Neutral);

        TargetRigBodySelection cleanBody = TargetRigAutomaticPoseFitter.SelectBody(
            rig, clean, alignment);
        GeneratedSkinningPreparationResult cleanPrepared =
            GeneratedSkinningPreparer.Prepare(
                target, clean, neutral, alignment, cleanBody);

        int augmentedMeshIndex = clean.Meshes
            .Select((mesh, index) => (mesh, index))
            .Where(item => item.mesh.Positions.Length > 0)
            .Select(item => item.index)
            .First();
        ImportedScene augmented = AddSyntheticNonSurfaceVertices(
            clean, augmentedMeshIndex);
        TargetRigBodySelection augmentedBody = TargetRigAutomaticPoseFitter.SelectBody(
            rig, augmented, alignment);
        AssertEquivalentBodySelection(cleanBody, augmentedBody);
        GeneratedSkinningPreparationResult augmentedPrepared =
            GeneratedSkinningPreparer.Prepare(
                target, augmented, neutral, alignment, augmentedBody);
        AssertEquivalentVisiblePreparation(
            cleanPrepared,
            augmentedPrepared,
            augmentedMeshIndex,
            clean.Meshes[augmentedMeshIndex].Positions.Length);

        AssertRejectedByBothPipelines(
            target,
            rig,
            neutral,
            alignment,
            WithOutOfRangeIndex(clean, augmentedMeshIndex),
            "out-of-range index");
        AssertRejectedByBothPipelines(
            target,
            rig,
            neutral,
            alignment,
            WithIncompleteTriangle(clean, augmentedMeshIndex),
            "incomplete triangle index list");
        AssertRejectedByBothPipelines(
            target,
            rig,
            neutral,
            alignment,
            WithNonFinitePosition(clean, augmentedMeshIndex),
            "non-finite position");

        if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBefore))
            throw new InvalidOperationException(
                "Synthetic topology normalization regression modified the target SMO.");
        Console.WriteLine(
            "GENERATED TOPOLOGY NORMALIZATION PASS: " +
            "one unused vertex and one three-vertex degenerate face were ignored; " +
            "visible membership/weights stayed identical; malformed topology was rejected.");
    }

    private static ImportedScene BuildUnskinnedTargetSurface(SmoExportScene scene)
    {
        ImportedMesh[] meshes = scene.Meshes
            .Where(mesh => mesh.SkinObjectIndex is not null &&
                           mesh.Positions.Length > 0 &&
                           mesh.TriangleIndices.Length > 0)
            .Select(mesh => new ImportedMesh(
                mesh.Name,
                mesh.Positions.ToArray(),
                mesh.Normals.Length == mesh.Positions.Length
                    ? mesh.Normals.ToArray()
                    : [],
                mesh.TextureCoordinates0.Length == mesh.Positions.Length
                    ? mesh.TextureCoordinates0.ToArray()
                    : [],
                mesh.TriangleIndices.ToArray()))
            .ToArray();
        if (meshes.Length == 0)
            throw new InvalidOperationException(
                "Synthetic topology regression target has no skinned surface meshes.");
        return new ImportedScene(Array.AsReadOnly(meshes));
    }

    private static ImportedScene AddSyntheticNonSurfaceVertices(
        ImportedScene source,
        int meshIndex)
    {
        ImportedMesh[] meshes = source.Meshes.Select(CloneMesh).ToArray();
        ImportedMesh mesh = meshes[meshIndex];
        int firstAdded = mesh.Positions.Length;
        Vector3 anchor = mesh.Positions[0];
        Vector3[] positions = mesh.Positions
            .Concat(new[]
            {
                anchor + new Vector3(17, 23, 31), // deliberately unindexed
                anchor + new Vector3(41, 43, 47),
                anchor + new Vector3(42, 43, 47),
                anchor + new Vector3(43, 43, 47) // three collinear vertices
            })
            .ToArray();
        uint[] indices = mesh.TriangleIndices
            .Concat(new[]
            {
                checked((uint)(firstAdded + 1)),
                checked((uint)(firstAdded + 2)),
                checked((uint)(firstAdded + 3))
            })
            .ToArray();
        meshes[meshIndex] = mesh with
        {
            Positions = positions,
            Normals = AppendRepeated(mesh.Normals, 4),
            TextureCoordinates = AppendRepeated(mesh.TextureCoordinates, 4),
            TriangleIndices = indices,
            DiffuseColorsArgb = mesh.DiffuseColors.Length == 0
                ? null
                : AppendRepeated(mesh.DiffuseColors, 4)
        };
        return new ImportedScene(
            Array.AsReadOnly(meshes),
            source.Textures,
            source.Materials);
    }

    private static ImportedScene WithOutOfRangeIndex(ImportedScene source, int meshIndex) =>
        ReplaceMesh(source, meshIndex, mesh => mesh with
        {
            TriangleIndices = mesh.TriangleIndices
                .Concat(new[]
                {
                    checked((uint)mesh.Positions.Length), 0u, 1u
                })
                .ToArray()
        });

    private static ImportedScene WithIncompleteTriangle(ImportedScene source, int meshIndex) =>
        ReplaceMesh(source, meshIndex, mesh => mesh with
        {
            TriangleIndices = mesh.TriangleIndices.Append(0u).ToArray()
        });

    private static ImportedScene WithNonFinitePosition(ImportedScene source, int meshIndex) =>
        ReplaceMesh(source, meshIndex, mesh =>
        {
            Vector3[] positions = mesh.Positions.ToArray();
            positions[0] = new Vector3(float.NaN, positions[0].Y, positions[0].Z);
            return mesh with { Positions = positions };
        });

    private static ImportedScene ReplaceMesh(
        ImportedScene source,
        int meshIndex,
        Func<ImportedMesh, ImportedMesh> replace)
    {
        ImportedMesh[] meshes = source.Meshes.Select(CloneMesh).ToArray();
        meshes[meshIndex] = replace(meshes[meshIndex]);
        return new ImportedScene(Array.AsReadOnly(meshes), source.Textures, source.Materials);
    }

    private static ImportedMesh CloneMesh(ImportedMesh mesh) => mesh with
    {
        Positions = mesh.Positions.ToArray(),
        Normals = mesh.Normals.ToArray(),
        TextureCoordinates = mesh.TextureCoordinates.ToArray(),
        TriangleIndices = mesh.TriangleIndices.ToArray(),
        DiffuseColorsArgb = mesh.DiffuseColorsArgb?.ToArray()
    };

    private static T[] AppendRepeated<T>(T[] values, int count)
    {
        if (values.Length == 0)
            return [];
        return values.Concat(Enumerable.Repeat(values[0], count)).ToArray();
    }

    private static void AssertEquivalentBodySelection(
        TargetRigBodySelection clean,
        TargetRigBodySelection augmented)
    {
        if (clean.TotalComponentCount != augmented.TotalComponentCount ||
            clean.ExcludedComponentCount != augmented.ExcludedComponentCount ||
            clean.Components.Count != augmented.Components.Count)
        {
            throw new InvalidOperationException(
                "Synthetic non-surface vertices changed body component counts.");
        }
        for (int componentIndex = 0; componentIndex < clean.Components.Count; componentIndex++)
        {
            TargetRigSelectedBodyComponent expected = clean.Components[componentIndex];
            TargetRigSelectedBodyComponent actual = augmented.Components[componentIndex];
            if (expected.ComponentIndex != actual.ComponentIndex ||
                expected.Role != actual.Role ||
                expected.UniquePositionCount != actual.UniquePositionCount ||
                expected.TriangleCount != actual.TriangleCount ||
                expected.SurfaceArea != actual.SurfaceArea ||
                expected.AlignedMinimum != actual.AlignedMinimum ||
                expected.AlignedMaximum != actual.AlignedMaximum ||
                !SameMembership(expected.VerticesByMesh, actual.VerticesByMesh))
            {
                throw new InvalidOperationException(
                    "Synthetic non-surface vertices changed visible body membership.");
            }
        }
    }

    private static bool SameMembership(
        IReadOnlyList<TargetRigBodyVertexMembership> first,
        IReadOnlyList<TargetRigBodyVertexMembership> second) =>
        first.Count == second.Count && first.Zip(second).All(pair =>
            pair.First.MeshIndex == pair.Second.MeshIndex &&
            pair.First.MeshName == pair.Second.MeshName &&
            pair.First.VertexIndices.SequenceEqual(pair.Second.VertexIndices));

    private static void AssertEquivalentVisiblePreparation(
        GeneratedSkinningPreparationResult clean,
        GeneratedSkinningPreparationResult augmented,
        int augmentedMeshIndex,
        int originalVertexCount)
    {
        if (clean.Analysis.DonorComponentCount != augmented.Analysis.DonorComponentCount ||
            clean.Analysis.DonorMainComponentVertexCount !=
                augmented.Analysis.DonorMainComponentVertexCount ||
            clean.Analysis.DonorMainComponentTriangleCount !=
                augmented.Analysis.DonorMainComponentTriangleCount ||
            augmented.Analysis.PreparedVertexCount !=
                clean.Analysis.PreparedVertexCount + 4)
        {
            throw new InvalidOperationException(
                "Synthetic non-surface vertices changed surface analysis.");
        }
        AssertScene(clean.PreparedScene, augmented.PreparedScene);
        AssertScene(clean.FittingPreviewScene, augmented.FittingPreviewScene);

        void AssertScene(ImportedScene expected, ImportedScene actual)
        {
            if (expected.Meshes.Count != actual.Meshes.Count)
                throw new InvalidOperationException("Synthetic preparation changed mesh count.");
            for (int meshIndex = 0; meshIndex < expected.Meshes.Count; meshIndex++)
            {
                ImportedMesh before = expected.Meshes[meshIndex];
                ImportedMesh after = actual.Meshes[meshIndex];
                int compareVertices = meshIndex == augmentedMeshIndex
                    ? originalVertexCount
                    : before.Positions.Length;
                if (!before.TriangleIndices.SequenceEqual(after.TriangleIndices) ||
                    !before.Positions.SequenceEqual(after.Positions.Take(compareVertices)) ||
                    before.Skinning is null || after.Skinning is null ||
                    !before.Skinning.JointIndices.SequenceEqual(
                        after.Skinning.JointIndices.Take(compareVertices)) ||
                    !before.Skinning.Weights.SequenceEqual(
                        after.Skinning.Weights.Take(compareVertices)))
                {
                    throw new InvalidOperationException(
                        "Synthetic topology normalization changed visible geometry or weights.");
                }
            }
            ImportedSkinning extraSkin = actual.Meshes[augmentedMeshIndex].Skinning!;
            ImportedJointIndices[] extraJoints = extraSkin.JointIndices
                .Skip(originalVertexCount)
                .ToArray();
            Vector4[] extraWeights = extraSkin.Weights.Skip(originalVertexCount).ToArray();
            if (extraJoints.Length != 4 || extraWeights.Length != 4 ||
                extraJoints.Distinct().Count() != 1 ||
                extraWeights.Any(weight => weight != Vector4.UnitX))
            {
                throw new InvalidOperationException(
                    "Non-surface vertices did not receive one deterministic inert influence.");
            }
        }
    }

    private static void AssertRejectedByBothPipelines(
        SmoDocument target,
        TargetRigDefinition rig,
        TargetRigFittingPoseSnapshot pose,
        ReplacementTransform alignment,
        ImportedScene invalid,
        string label)
    {
        bool selectionRejected = false;
        try
        {
            _ = TargetRigAutomaticPoseFitter.SelectBody(rig, invalid, alignment);
        }
        catch (InvalidDataException)
        {
            selectionRejected = true;
        }
        bool preparationRejected = false;
        try
        {
            _ = GeneratedSkinningPreparer.Prepare(target, invalid, pose, alignment);
        }
        catch (InvalidDataException)
        {
            preparationRejected = true;
        }
        if (!selectionRejected || !preparationRejected)
        {
            throw new InvalidOperationException(
                $"Synthetic {label} was not rejected by both topology pipelines.");
        }
    }
}
