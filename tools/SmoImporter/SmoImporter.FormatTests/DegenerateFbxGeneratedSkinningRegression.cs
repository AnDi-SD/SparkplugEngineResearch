using System.Numerics;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class DegenerateFbxGeneratedSkinningRegression
{
    private const float PositionEpsilon = 0.000001f;

    public static void Run(string targetPath, string donorPath)
    {
        targetPath = Path.GetFullPath(targetPath);
        donorPath = Path.GetFullPath(donorPath);
        byte[] targetFileBefore = File.ReadAllBytes(targetPath);
        byte[] donorFileBefore = File.ReadAllBytes(donorPath);

        SmoDocument target = SmoDocument.Load(targetPath);
        SmoExportScene targetScene = SmoSceneBuilder.Build(
            target,
            new SmoExportOptions(
                ApplyWorldTransforms: true,
                AnimationPaths: null,
                Resources: SmoExportResourceTypes.Meshes |
                           SmoExportResourceTypes.Skeleton));
        ImportedScene donor = ImportedModelReader.ReadGeometryOnly(donorPath);
        SceneSnapshot donorBefore = SceneSnapshot.Capture(donor);
        TopologyExpectation expected = InspectTopology(donor);
        if (expected.ValidTriangleCount == 0 || expected.DegenerateTriangleCount == 0 ||
            expected.NonSurfaceVertexCount == 0)
        {
            throw new InvalidOperationException(
                "The regression donor must contain both a usable surface and " +
                "vertices referenced only by degenerate triangles.");
        }

        ReplacementTransform alignment = ReplacementTransformFitter.FitByHeightAndCenter(
            targetScene.Meshes.SelectMany(mesh => mesh.Positions),
            donor.Meshes.SelectMany(mesh => mesh.Positions));
        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);

        // This is intentionally the complete manual workflow. A user-authored
        // pose must be preparable without invoking automatic pose fitting first.
        TargetRigBodySelection body = TargetRigAutomaticPoseFitter.SelectBody(
            rig, donor, alignment);
        AssertNonSurfaceVerticesExcludedFromBody(body, expected);
        GeneratedSkinningPreparationResult neutral = GeneratedSkinningPreparer.Prepare(
            target,
            donor,
            TargetRigBodyPoseMapper.CreateSnapshot(
                rig, TargetRigBodyPoseParameters.Neutral),
            alignment,
            body);
        var authoredParameters = new TargetRigBodyPoseParameters(
            ArmElevationDegrees: 46.7f,
            ArmForwardDegrees: 19.1f,
            ElbowBendDegrees: 0,
            LegSpreadDegrees: -8,
            KneeBendDegrees: 0,
            TorsoPitchDegrees: 8.4f,
            NeckForward: -36.6f);
        TargetRigFittingPoseSnapshot authoredPose =
            TargetRigBodyPoseMapper.CreateSnapshot(rig, authoredParameters);
        GeneratedSkinningPreparationResult authored = GeneratedSkinningPreparer.Prepare(
            target, donor, authoredPose, alignment, body);
        GeneratedSkinningPreparationResult authoredRepeated =
            GeneratedSkinningPreparer.Prepare(
                target, donor, authoredPose, alignment, body);

        ValidatePreparedScene(neutral, donor, expected, "neutral");
        ValidatePreparedScene(authored, donor, expected, "authored");
        ValidatePreparedScene(authoredRepeated, donor, expected, "authored repeated");
        AssertPreparedScenesEqual(
            authored.PreparedScene,
            authoredRepeated.PreparedScene,
            "Repeated authored preparation is not bitwise deterministic.");
        AssertPreparedScenesEqual(
            authored.FittingPreviewScene,
            authoredRepeated.FittingPreviewScene,
            "Repeated authored fitting preview is not bitwise deterministic.");
        RunSyntheticNonSurfaceContract(
            target,
            targetScene,
            rig,
            donor,
            authoredPose);
        donorBefore.AssertUnchanged(donor);
        if (!File.ReadAllBytes(targetPath).SequenceEqual(targetFileBefore) ||
            !File.ReadAllBytes(donorPath).SequenceEqual(donorFileBefore))
        {
            throw new InvalidOperationException(
                "Degenerate-FBX generated-skinning regression modified an input file.");
        }

        Console.WriteLine(
            $"DEGENERATE FBX GENERATED SKINNING PASS: " +
            $"meshes={donor.Meshes.Count}; vertices={expected.VertexCount}; " +
            $"triangles={expected.SourceTriangleCount}->{expected.ValidTriangleCount}; " +
            $"degenerate={expected.DegenerateTriangleCount}; " +
            $"nonSurfaceVertices={expected.NonSurfaceVertexCount}; " +
            $"components={authored.Analysis.DonorComponentCount}; " +
            $"alignmentScale={alignment.Scale:G9}.");
    }

    private static void RunSyntheticNonSurfaceContract(
        SmoDocument target,
        SmoExportScene targetScene,
        TargetRigDefinition rig,
        ImportedScene source,
        TargetRigFittingPoseSnapshot pose)
    {
        ImportedScene clean = CompactToRenderableSurface(source);
        TopologyExpectation cleanTopology = InspectTopology(clean);
        if (cleanTopology.DegenerateTriangleCount != 0 ||
            cleanTopology.NonSurfaceVertexCount != 0)
        {
            throw new InvalidOperationException(
                "Synthetic topology baseline is not a clean surface.");
        }
        (ImportedScene augmented, int augmentedMeshIndex, int originalVertexCount) =
            AddSyntheticNonSurfaceVertices(clean);
        TopologyExpectation augmentedTopology = InspectTopology(augmented);
        if (augmentedTopology.DegenerateTriangleCount != 1 ||
            augmentedTopology.NonSurfaceVertexCount != 4 ||
            augmentedTopology.ValidTriangleCount != cleanTopology.ValidTriangleCount)
        {
            throw new InvalidOperationException(
                "Synthetic donor does not contain exactly one unused vertex and " +
                "three vertices belonging only to one degenerate triangle.");
        }

        ReplacementTransform alignment = ReplacementTransformFitter.FitByHeightAndCenter(
            targetScene.Meshes.SelectMany(mesh => mesh.Positions),
            clean.Meshes.SelectMany(mesh => mesh.Positions));
        TargetRigBodySelection cleanBody = TargetRigAutomaticPoseFitter.SelectBody(
            rig, clean, alignment);
        TargetRigBodySelection augmentedBody = TargetRigAutomaticPoseFitter.SelectBody(
            rig, augmented, alignment);
        AssertEquivalentBodySurface(cleanBody, augmentedBody);
        AssertNonSurfaceVerticesExcludedFromBody(augmentedBody, augmentedTopology);

        GeneratedSkinningPreparationResult cleanPrepared =
            GeneratedSkinningPreparer.Prepare(
                target, clean, pose, alignment, cleanBody);
        GeneratedSkinningPreparationResult augmentedPrepared =
            GeneratedSkinningPreparer.Prepare(
                target, augmented, pose, alignment, augmentedBody);
        ValidatePreparedScene(
            cleanPrepared, clean, cleanTopology, "synthetic clean baseline");
        ValidatePreparedScene(
            augmentedPrepared,
            augmented,
            augmentedTopology,
            "synthetic non-surface augmentation");
        AssertOriginalPreparedVerticesEqual(
            cleanPrepared.PreparedScene,
            augmentedPrepared.PreparedScene,
            augmentedMeshIndex,
            originalVertexCount,
            "Synthetic non-surface vertices changed canonical geometry or weights.");
        AssertOriginalPreparedVerticesEqual(
            cleanPrepared.FittingPreviewScene,
            augmentedPrepared.FittingPreviewScene,
            augmentedMeshIndex,
            originalVertexCount,
            "Synthetic non-surface vertices changed fitting geometry or weights.");
        if (cleanPrepared.Analysis.DonorComponentCount !=
                augmentedPrepared.Analysis.DonorComponentCount ||
            cleanPrepared.Analysis.DonorMainComponentVertexCount !=
                augmentedPrepared.Analysis.DonorMainComponentVertexCount ||
            cleanPrepared.Analysis.Attachments.Sum(attachment => attachment.VertexCount) !=
                augmentedPrepared.Analysis.Attachments.Sum(
                    attachment => attachment.VertexCount))
        {
            throw new InvalidOperationException(
                "Synthetic non-surface vertices entered body or attachment component counts.");
        }
    }

    private static ImportedScene CompactToRenderableSurface(ImportedScene source)
    {
        TopologyExpectation topology = InspectTopology(source);
        ImportedMesh[] meshes = source.Meshes.Select((mesh, meshIndex) =>
        {
            uint[] surfaceIndices = topology.FilteredIndices[meshIndex];
            bool[] used = new bool[mesh.Positions.Length];
            foreach (uint index in surfaceIndices)
                used[index] = true;
            int[] remap = Enumerable.Repeat(-1, mesh.Positions.Length).ToArray();
            int next = 0;
            for (int vertex = 0; vertex < used.Length; vertex++)
            {
                if (used[vertex])
                    remap[vertex] = next++;
            }
            uint[] compactIndices = surfaceIndices
                .Select(index => checked((uint)remap[index]))
                .ToArray();
            return mesh with
            {
                Positions = CompactAttribute(mesh.Positions, used, meshIndex, "positions"),
                Normals = CompactAttribute(mesh.Normals, used, meshIndex, "normals"),
                TextureCoordinates = CompactAttribute(
                    mesh.TextureCoordinates, used, meshIndex, "texture coordinates"),
                TriangleIndices = compactIndices,
                DiffuseColorsArgb = mesh.DiffuseColorsArgb is null
                    ? null
                    : CompactAttribute(
                        mesh.DiffuseColorsArgb, used, meshIndex, "diffuse colors"),
                Skinning = null
            };
        }).ToArray();
        return new ImportedScene(
            Array.AsReadOnly(meshes), source.Textures, source.Materials);
    }

    private static (ImportedScene Scene, int MeshIndex, int OriginalVertexCount)
        AddSyntheticNonSurfaceVertices(ImportedScene source)
    {
        int meshIndex = Enumerable.Range(0, source.Meshes.Count)
            .First(index => source.Meshes[index].Positions.Length > 0);
        ImportedMesh original = source.Meshes[meshIndex];
        int originalVertexCount = original.Positions.Length;
        Vector3 anchor = original.Positions[0];
        ImportedMesh augmented = original with
        {
            Positions = AppendCopies(original.Positions, anchor, 4),
            Normals = original.Normals.Length == 0
                ? []
                : AppendCopies(original.Normals, original.Normals[0], 4),
            TextureCoordinates = original.TextureCoordinates.Length == 0
                ? []
                : AppendCopies(
                    original.TextureCoordinates,
                    original.TextureCoordinates[0],
                    4),
            TriangleIndices = original.TriangleIndices.Concat(new uint[]
            {
                checked((uint)originalVertexCount + 1),
                checked((uint)originalVertexCount + 2),
                checked((uint)originalVertexCount + 3)
            }).ToArray(),
            DiffuseColorsArgb = original.DiffuseColorsArgb is { Length: > 0 } colors
                ? AppendCopies(colors, colors[0], 4)
                : original.DiffuseColorsArgb,
            Skinning = null
        };
        ImportedMesh[] meshes = source.Meshes.ToArray();
        meshes[meshIndex] = augmented;
        return (
            new ImportedScene(
                Array.AsReadOnly(meshes), source.Textures, source.Materials),
            meshIndex,
            originalVertexCount);
    }

    private static T[] CompactAttribute<T>(
        T[] values,
        bool[] used,
        int meshIndex,
        string label)
    {
        if (values.Length == 0)
            return [];
        if (values.Length != used.Length)
        {
            throw new InvalidDataException(
                $"Mesh {meshIndex} {label} do not have one value per vertex.");
        }
        return values.Where((_, index) => used[index]).ToArray();
    }

    private static T[] AppendCopies<T>(T[] values, T value, int count) =>
        values.Concat(Enumerable.Repeat(value, count)).ToArray();

    private static void AssertEquivalentBodySurface(
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
        foreach ((TargetRigSelectedBodyComponent first,
                  TargetRigSelectedBodyComponent second) in
                 clean.Components.Zip(augmented.Components))
        {
            bool sameMembership = first.VerticesByMesh.Count ==
                                  second.VerticesByMesh.Count &&
                first.VerticesByMesh.Zip(second.VerticesByMesh).All(pair =>
                    pair.First.MeshIndex == pair.Second.MeshIndex &&
                    pair.First.MeshName == pair.Second.MeshName &&
                    pair.First.VertexIndices.SequenceEqual(
                        pair.Second.VertexIndices));
            if (first.ComponentIndex != second.ComponentIndex ||
                first.Role != second.Role ||
                first.UniquePositionCount != second.UniquePositionCount ||
                first.TriangleCount != second.TriangleCount ||
                first.SurfaceArea != second.SurfaceArea ||
                first.AlignedMinimum != second.AlignedMinimum ||
                first.AlignedMaximum != second.AlignedMaximum ||
                !sameMembership)
            {
                throw new InvalidOperationException(
                    "Synthetic non-surface vertices changed body surface identity.");
            }
        }
    }

    private static void AssertOriginalPreparedVerticesEqual(
        ImportedScene clean,
        ImportedScene augmented,
        int augmentedMeshIndex,
        int originalVertexCount,
        string message)
    {
        if (clean.Meshes.Count != augmented.Meshes.Count)
            throw new InvalidOperationException(message);
        for (int meshIndex = 0; meshIndex < clean.Meshes.Count; meshIndex++)
        {
            ImportedMesh first = clean.Meshes[meshIndex];
            ImportedMesh second = augmented.Meshes[meshIndex];
            int count = meshIndex == augmentedMeshIndex
                ? originalVertexCount
                : first.Positions.Length;
            if (second.Positions.Length < count ||
                !first.Positions.SequenceEqual(second.Positions.Take(count)) ||
                !first.Normals.SequenceEqual(second.Normals.Take(first.Normals.Length)) ||
                !first.TextureCoordinates.SequenceEqual(
                    second.TextureCoordinates.Take(first.TextureCoordinates.Length)) ||
                !first.TriangleIndices.SequenceEqual(second.TriangleIndices) ||
                !first.DiffuseColors.SequenceEqual(
                    second.DiffuseColors.Take(first.DiffuseColors.Length)) ||
                first.Skinning is null || second.Skinning is null ||
                !first.Skinning.JointIndices.SequenceEqual(
                    second.Skinning.JointIndices.Take(count)) ||
                !first.Skinning.Weights.SequenceEqual(
                    second.Skinning.Weights.Take(count)))
            {
                throw new InvalidOperationException(message);
            }
        }
    }

    private static TopologyExpectation InspectTopology(ImportedScene scene)
    {
        var filteredIndices = new uint[scene.Meshes.Count][];
        var nonSurfaceVertices = new bool[scene.Meshes.Count][];
        int vertexCount = 0;
        int sourceTriangleCount = 0;
        int validTriangleCount = 0;
        int degenerateTriangleCount = 0;
        int nonSurfaceVertexCount = 0;
        for (int meshIndex = 0; meshIndex < scene.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = scene.Meshes[meshIndex];
            if (mesh.Positions.Any(position => !IsFinite(position)))
                throw new InvalidDataException(
                    $"Regression donor mesh {meshIndex} has non-finite positions.");
            if (mesh.TriangleIndices.Length == 0 ||
                mesh.TriangleIndices.Length % 3 != 0)
            {
                throw new InvalidDataException(
                    $"Regression donor mesh {meshIndex} has incomplete topology.");
            }

            bool[] referencedBySurface = new bool[mesh.Positions.Length];
            var kept = new List<uint>(mesh.TriangleIndices.Length);
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                uint first = mesh.TriangleIndices[index];
                uint second = mesh.TriangleIndices[index + 1];
                uint third = mesh.TriangleIndices[index + 2];
                if (first >= mesh.Positions.Length ||
                    second >= mesh.Positions.Length ||
                    third >= mesh.Positions.Length)
                {
                    throw new InvalidDataException(
                        $"Regression donor mesh {meshIndex} has an out-of-range index.");
                }
                Vector3 a = mesh.Positions[(int)first];
                Vector3 b = mesh.Positions[(int)second];
                Vector3 c = mesh.Positions[(int)third];
                float area = Vector3.Cross(b - a, c - a).Length() * 0.5f;
                bool valid = first != second && second != third && first != third &&
                             float.IsFinite(area) &&
                             area > PositionEpsilon * PositionEpsilon;
                sourceTriangleCount++;
                if (!valid)
                {
                    degenerateTriangleCount++;
                    continue;
                }
                kept.Add(first);
                kept.Add(second);
                kept.Add(third);
                referencedBySurface[(int)first] = true;
                referencedBySurface[(int)second] = true;
                referencedBySurface[(int)third] = true;
                validTriangleCount++;
            }

            filteredIndices[meshIndex] = kept.ToArray();
            nonSurfaceVertices[meshIndex] = referencedBySurface
                .Select(referenced => !referenced)
                .ToArray();
            vertexCount += mesh.Positions.Length;
            nonSurfaceVertexCount += referencedBySurface.Count(referenced => !referenced);
        }
        return new TopologyExpectation(
            filteredIndices,
            nonSurfaceVertices,
            vertexCount,
            sourceTriangleCount,
            validTriangleCount,
            degenerateTriangleCount,
            nonSurfaceVertexCount);
    }

    private static void AssertNonSurfaceVerticesExcludedFromBody(
        TargetRigBodySelection body,
        TopologyExpectation expected)
    {
        foreach (TargetRigSelectedBodyComponent component in body.Components)
        {
            foreach (TargetRigBodyVertexMembership membership in component.VerticesByMesh)
            {
                foreach (int vertexIndex in membership.VertexIndices)
                {
                    if (expected.NonSurfaceVertices[membership.MeshIndex][vertexIndex])
                    {
                        throw new InvalidOperationException(
                            "A vertex outside every non-degenerate triangle entered " +
                            "the selected body membership.");
                    }
                }
            }
        }
    }

    private static void ValidatePreparedScene(
        GeneratedSkinningPreparationResult result,
        ImportedScene donor,
        TopologyExpectation expected,
        string label)
    {
        ValidatePreparedGeometryAndWeights(
            result.PreparedScene, donor, expected, label + " canonical scene");
        ValidatePreparedGeometryAndWeights(
            result.FittingPreviewScene, donor, expected, label + " fitting scene");
        int attachmentVertexCount = result.Analysis.Attachments.Sum(
            attachment => attachment.VertexCount);
        int bodyVertexCount = result.Analysis.DonorMainComponentVertexCount;
        if (bodyVertexCount + attachmentVertexCount + expected.NonSurfaceVertexCount !=
            expected.VertexCount)
        {
            throw new InvalidOperationException(
                $"{label}: non-surface vertices leaked into body/component counts: " +
                $"body={bodyVertexCount}, attachments={attachmentVertexCount}, " +
                $"nonSurface={expected.NonSurfaceVertexCount}, total={expected.VertexCount}.");
        }
    }

    private static void ValidatePreparedGeometryAndWeights(
        ImportedScene prepared,
        ImportedScene donor,
        TopologyExpectation expected,
        string label)
    {
        if (prepared.Meshes.Count != donor.Meshes.Count)
            throw new InvalidOperationException($"{label}: mesh count changed.");
        int preparedVertexCount = 0;
        int preparedTriangleCount = 0;
        for (int meshIndex = 0; meshIndex < prepared.Meshes.Count; meshIndex++)
        {
            ImportedMesh source = donor.Meshes[meshIndex];
            ImportedMesh mesh = prepared.Meshes[meshIndex];
            if (mesh.Name != source.Name ||
                mesh.MaterialIndex != source.MaterialIndex ||
                mesh.Positions.Length != source.Positions.Length ||
                !mesh.TriangleIndices.SequenceEqual(expected.FilteredIndices[meshIndex]))
            {
                throw new InvalidOperationException(
                    $"{label}: mesh {meshIndex} did not preserve vertex slots and " +
                    "the ordered non-degenerate triangle subset.");
            }
            if (mesh.Skinning is null ||
                mesh.Skinning.JointIndices.Length != mesh.Positions.Length ||
                mesh.Skinning.Weights.Length != mesh.Positions.Length)
            {
                throw new InvalidOperationException(
                    $"{label}: mesh {meshIndex} does not have one generated weight " +
                    "record per preserved vertex slot.");
            }
            int jointCount = mesh.Skinning.Skeleton.JointNames.Count;
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
                Vector4 weight = mesh.Skinning.Weights[vertex];
                ImportedJointIndices joints = mesh.Skinning.JointIndices[vertex];
                float sum = weight.X + weight.Y + weight.Z + weight.W;
                if (!IsFinite(weight) ||
                    weight.X < 0 || weight.Y < 0 || weight.Z < 0 || weight.W < 0 ||
                    MathF.Abs(sum - 1) > 0.00001f ||
                    joints.X >= jointCount || joints.Y >= jointCount ||
                    joints.Z >= jointCount || joints.W >= jointCount)
                {
                    throw new InvalidOperationException(
                        $"{label}: mesh {meshIndex} vertex {vertex} has invalid " +
                        "generated joints or weights.");
                }
            }
            preparedVertexCount += mesh.Positions.Length;
            preparedTriangleCount += mesh.TriangleIndices.Length / 3;
        }
        if (preparedVertexCount != expected.VertexCount ||
            preparedTriangleCount != expected.ValidTriangleCount)
        {
            throw new InvalidOperationException(
                $"{label}: prepared totals are {preparedVertexCount} vertices and " +
                $"{preparedTriangleCount} triangles; expected {expected.VertexCount} " +
                $"and {expected.ValidTriangleCount}.");
        }
    }

    private static void AssertPreparedScenesEqual(
        ImportedScene first,
        ImportedScene second,
        string message)
    {
        if (first.Meshes.Count != second.Meshes.Count)
            throw new InvalidOperationException(message);
        for (int meshIndex = 0; meshIndex < first.Meshes.Count; meshIndex++)
        {
            ImportedMesh a = first.Meshes[meshIndex];
            ImportedMesh b = second.Meshes[meshIndex];
            if (a.Name != b.Name ||
                a.MaterialIndex != b.MaterialIndex ||
                !a.Positions.SequenceEqual(b.Positions) ||
                !a.Normals.SequenceEqual(b.Normals) ||
                !a.TextureCoordinates.SequenceEqual(b.TextureCoordinates) ||
                !a.TriangleIndices.SequenceEqual(b.TriangleIndices) ||
                !a.DiffuseColors.SequenceEqual(b.DiffuseColors) ||
                a.Skinning is null || b.Skinning is null ||
                !a.Skinning.JointIndices.SequenceEqual(b.Skinning.JointIndices) ||
                !a.Skinning.Weights.SequenceEqual(b.Skinning.Weights))
            {
                throw new InvalidOperationException(message);
            }
        }
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);

    private static bool IsFinite(Vector4 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z) && float.IsFinite(value.W);

    private sealed record TopologyExpectation(
        uint[][] FilteredIndices,
        bool[][] NonSurfaceVertices,
        int VertexCount,
        int SourceTriangleCount,
        int ValidTriangleCount,
        int DegenerateTriangleCount,
        int NonSurfaceVertexCount);

    private sealed record MeshSnapshot(
        Vector3[] Positions,
        Vector3[] Normals,
        Vector2[] TextureCoordinates,
        uint[] TriangleIndices,
        uint[] DiffuseColors);

    private sealed class SceneSnapshot
    {
        private readonly MeshSnapshot[] _meshes;

        private SceneSnapshot(MeshSnapshot[] meshes) => _meshes = meshes;

        public static SceneSnapshot Capture(ImportedScene scene) =>
            new(scene.Meshes.Select(mesh => new MeshSnapshot(
                mesh.Positions.ToArray(),
                mesh.Normals.ToArray(),
                mesh.TextureCoordinates.ToArray(),
                mesh.TriangleIndices.ToArray(),
                mesh.DiffuseColors.ToArray())).ToArray());

        public void AssertUnchanged(ImportedScene scene)
        {
            if (scene.Meshes.Count != _meshes.Length)
                throw new InvalidOperationException("Preparation mutated the donor mesh list.");
            for (int meshIndex = 0; meshIndex < _meshes.Length; meshIndex++)
            {
                MeshSnapshot before = _meshes[meshIndex];
                ImportedMesh after = scene.Meshes[meshIndex];
                if (!before.Positions.SequenceEqual(after.Positions) ||
                    !before.Normals.SequenceEqual(after.Normals) ||
                    !before.TextureCoordinates.SequenceEqual(after.TextureCoordinates) ||
                    !before.TriangleIndices.SequenceEqual(after.TriangleIndices) ||
                    !before.DiffuseColors.SequenceEqual(after.DiffuseColors))
                {
                    throw new InvalidOperationException(
                        $"Preparation mutated donor mesh {meshIndex}.");
                }
            }
        }
    }
}
