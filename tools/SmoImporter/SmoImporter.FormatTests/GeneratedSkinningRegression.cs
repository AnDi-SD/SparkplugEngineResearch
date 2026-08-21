using System.Numerics;
using SmoImporter.Core;

internal readonly record struct GeneratedAnatomyRegressionMetrics(
    int ChestVertexCount,
    float MeanChestCentralMass,
    float ChestCentralDominantRatio,
    int HeadVertexCount,
    float MeanHeadMass,
    float HeadDominantRatio,
    int LimbVertexCount,
    float LimbCentralDominantRatio);

internal static class GeneratedSkinningRegression
{
    private static readonly HashSet<string> CentralBodyBones = new(
        ["Pelvis", "Spine_01", "Spine_02", "Spine_03", "Neck"],
        StringComparer.Ordinal);

    public static GeneratedAnatomyRegressionMetrics VerifyAnatomicalVolumes(
        GeneratedSkinningPreparationResult preparation,
        TargetRigDefinition rig,
        TargetRigFittingPoseSnapshot pose,
        TargetRigBodySelection bodySelection)
    {
        ArgumentNullException.ThrowIfNull(preparation);
        ArgumentNullException.ThrowIfNull(rig);
        ArgumentNullException.ThrowIfNull(pose);
        ArgumentNullException.ThrowIfNull(bodySelection);

        var selected = bodySelection.Components
            .SelectMany(component => component.VerticesByMesh)
            .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                (membership.MeshIndex, VertexIndex: vertex)))
            .ToHashSet();
        int smoothVertexCount = selected.Count;
        if (smoothVertexCount == 0 ||
            preparation.Analysis.AnatomicalVolumeAffectedVertexCount <= 0 ||
            preparation.Analysis.AnatomicalVolumeLegacyVertexCount <= 0 ||
            preparation.Analysis.AnatomicalVolumeAffectedVertexCount +
                preparation.Analysis.AnatomicalVolumeLegacyVertexCount !=
                smoothVertexCount)
        {
            throw new InvalidOperationException(
                "Anatomical-field coverage does not partition the exact selected " +
                "smooth-body vertices into affected and bit-identical legacy paths.");
        }

        Vector3[] jointPositions = rig.Joints
            .Where(joint => joint.IsDeformJoint)
            .Select(joint => Translation(pose.WorldMatrices[joint.JointIndex]))
            .ToArray();
        float height = jointPositions.Max(position => position.Y) -
                       jointPositions.Min(position => position.Y);
        if (!float.IsFinite(height) || height <= 0.000001f)
            throw new InvalidOperationException("Target fitting pose has no finite height.");

        Vector3 spine02 = At(rig, pose, "Spine_02");
        Vector3 neck = At(rig, pose, "Neck");
        Vector3 head = At(rig, pose, "Head");
        Vector3 pelvis = At(rig, pose, "Pelvis");
        Vector3 leftBicep = At(rig, pose, "L_Bicep");
        Vector3 rightBicep = At(rig, pose, "R_Bicep");
        float centerX = (spine02.X + neck.X) * 0.5f;

        var chest = new List<(ImportedMesh Mesh, int Vertex)>();
        var headRegion = new List<(ImportedMesh Mesh, int Vertex)>();
        var limbRegion = new List<(ImportedMesh Mesh, int Vertex)>();
        float chestLowerY = spine02.Y;
        float chestUpperY = neck.Y - (neck.Y - spine02.Y) * 0.08f;
        float chestHalfWidth = height * 0.09f;
        float outerArmX = MathF.Max(
            MathF.Abs(leftBicep.X - centerX),
            MathF.Abs(rightBicep.X - centerX)) + height * 0.015f;
        foreach ((int meshIndex, int vertexIndex) in selected)
        {
            ImportedMesh mesh = preparation.FittingPreviewScene.Meshes[meshIndex];
            Vector3 position = mesh.Positions[vertexIndex];
            float spineAmount = Math.Clamp(
                (position.Y - spine02.Y) /
                MathF.Max(0.000001f, neck.Y - spine02.Y),
                0,
                1);
            float spineZ = spine02.Z + (neck.Z - spine02.Z) * spineAmount;
            if (position.Y >= chestLowerY && position.Y <= chestUpperY &&
                MathF.Abs(position.X - centerX) <= chestHalfWidth &&
                position.Z >= spineZ + height * 0.005f)
            {
                chest.Add((mesh, vertexIndex));
            }

            if (position.Y >= head.Y + height * 0.005f &&
                position.Y <= head.Y + height * 0.18f &&
                MathF.Abs(position.X - head.X) <= height * 0.10f &&
                MathF.Abs(position.Z - head.Z) <= height * 0.14f)
            {
                headRegion.Add((mesh, vertexIndex));
            }

            bool outerArm = MathF.Abs(position.X - centerX) >= outerArmX &&
                            position.Y >= MathF.Min(leftBicep.Y, rightBicep.Y) -
                                height * 0.08f;
            bool lowerLeg = position.Y <= pelvis.Y - height * 0.05f &&
                            MathF.Abs(position.X - centerX) >= height * 0.0125f;
            if (outerArm || lowerLeg)
                limbRegion.Add((mesh, vertexIndex));
        }

        if (chest.Count < 80 || headRegion.Count < 20 || limbRegion.Count < 200)
        {
            throw new InvalidOperationException(
                $"Anatomical regression regions are under-sampled: chest={chest.Count}, " +
                $"head={headRegion.Count}, limbs={limbRegion.Count}.");
        }

        float meanChestCentralMass = chest.Average(sample =>
            SemanticMass(sample.Mesh, sample.Vertex, CentralBodyBones));
        float chestCentralDominantRatio = chest.Count(sample =>
            CentralBodyBones.Contains(DominantBone(sample.Mesh, sample.Vertex))) /
            (float)chest.Count;
        float meanHeadMass = headRegion.Average(sample =>
            SemanticMass(sample.Mesh, sample.Vertex, new HashSet<string>(["Head"],
                StringComparer.Ordinal)));
        float headDominantRatio = headRegion.Count(sample =>
            string.Equals(
                DominantBone(sample.Mesh, sample.Vertex),
                "Head",
                StringComparison.Ordinal)) / (float)headRegion.Count;
        float limbCentralDominantRatio = limbRegion.Count(sample =>
            CentralBodyBones.Contains(DominantBone(sample.Mesh, sample.Vertex)) ||
            string.Equals(
                DominantBone(sample.Mesh, sample.Vertex),
                "Head",
                StringComparison.Ordinal)) / (float)limbRegion.Count;

        Console.WriteLine(
            "  Chest dominant breakdown: " +
            string.Join(", ", chest
                .GroupBy(sample => DominantBone(sample.Mesh, sample.Vertex))
                .OrderByDescending(group => group.Count())
                .Select(group => $"{group.Key}={group.Count()}")));
        string[] chestBones = chest[0].Mesh.Skinning!.Skeleton.JointNames.ToArray();
        Console.WriteLine(
            "  Chest mean-mass breakdown: " +
            string.Join(", ", chestBones
                .Select(name => (Name: name, Mass: chest.Average(sample =>
                    SemanticMass(
                        sample.Mesh,
                        sample.Vertex,
                        new HashSet<string>([name], StringComparer.Ordinal)))))
                .Where(value => value.Mass >= 0.005f)
                .OrderByDescending(value => value.Mass)
                .Select(value => $"{value.Name}={value.Mass:G6}")));

        if (meanChestCentralMass < 0.85f ||
            chestCentralDominantRatio < 0.85f)
        {
            throw new InvalidOperationException(
                $"Shifted torso volumes do not keep the front chest on central " +
                $"bones: mean mass={meanChestCentralMass:G9}, " +
                $"dominant={chestCentralDominantRatio:P3}, vertices={chest.Count}.");
        }
        if (meanHeadMass < 0.80f || headDominantRatio < 0.90f)
        {
            throw new InvalidOperationException(
                $"Shifted Head ellipsoid does not keep the smooth upper head on Head: " +
                $"mean mass={meanHeadMass:G9}, dominant={headDominantRatio:P3}, " +
                $"vertices={headRegion.Count}.");
        }
        if (limbCentralDominantRatio != 0)
        {
            throw new InvalidOperationException(
                $"Finite torso/head fields leaked into the protected outer arms or " +
                $"lower legs: central dominant={limbCentralDominantRatio:P3}, " +
                $"vertices={limbRegion.Count}.");
        }

        return new GeneratedAnatomyRegressionMetrics(
            chest.Count,
            meanChestCentralMass,
            chestCentralDominantRatio,
            headRegion.Count,
            meanHeadMass,
            headDominantRatio,
            limbRegion.Count,
            limbCentralDominantRatio);
    }

    public static GeneratedSkinningComponentOverrides CreateLaylaOverrides(
        GeneratedSkinningPreparationResult preparation,
        int headAndHairMeshIndex,
        int wingMeshIndex)
    {
        GeneratedSkinningAttachment[] headAndHair = preparation.Analysis.Attachments
            .Where(attachment => attachment.MeshIndices.Contains(headAndHairMeshIndex))
            .OrderBy(attachment => attachment.ComponentIndex)
            .ToArray();
        GeneratedSkinningAttachment[] wings = preparation.Analysis.Attachments
            .Where(attachment => attachment.MeshIndices.Contains(wingMeshIndex))
            .OrderBy(attachment => attachment.ComponentIndex)
            .ToArray();
        if (headAndHair.Length == 0 || wings.Length != 8 ||
            headAndHair.Select(attachment => attachment.ComponentIndex)
                .Intersect(wings.Select(attachment => attachment.ComponentIndex)).Any())
        {
            throw new InvalidOperationException(
                $"Layla attachment fixture changed: head/hair={headAndHair.Length}, " +
                $"wings={wings.Length}.");
        }

        GeneratedSkinningComponentOverride[] components = headAndHair
            .Select(attachment => new GeneratedSkinningComponentOverride(
                attachment.ComponentIndex,
                GeneratedSkinningComponentAttachmentTarget.Head,
                attachment.VerticesByMesh))
            .Concat(wings.Select(attachment => new GeneratedSkinningComponentOverride(
                attachment.ComponentIndex,
                GeneratedSkinningComponentAttachmentTarget.UpperBack,
                attachment.VerticesByMesh)))
            .OrderBy(component => component.ComponentIndex)
            .ToArray();
        return new GeneratedSkinningComponentOverrides(
            Array.AsReadOnly(components),
            preparation.Analysis.DonorComponentCount,
            preparation.Analysis.TargetRigFingerprint,
            preparation.Analysis.DonorGeometryFingerprint);
    }

    public static void VerifyManualAssignments(
        GeneratedSkinningPreparationResult baseline,
        GeneratedSkinningPreparationResult overridden,
        GeneratedSkinningComponentOverrides overrides,
        string context)
    {
        Dictionary<int, GeneratedSkinningComponentOverride> expected = overrides.Components
            .ToDictionary(component => component.ComponentIndex);
        Dictionary<int, GeneratedSkinningAttachment> baselineByComponent = baseline
            .Analysis.Attachments.ToDictionary(attachment => attachment.ComponentIndex);
        Dictionary<int, GeneratedSkinningAttachment> actualByComponent = overridden
            .Analysis.Attachments.ToDictionary(attachment => attachment.ComponentIndex);
        if (!baselineByComponent.Keys.ToHashSet().SetEquals(actualByComponent.Keys))
            throw new InvalidOperationException($"{context}: attachment topology changed.");

        foreach ((int componentIndex, GeneratedSkinningAttachment attachment) in
                 actualByComponent)
        {
            if (!expected.TryGetValue(componentIndex, out GeneratedSkinningComponentOverride? item))
            {
                GeneratedSkinningAttachment before = baselineByComponent[componentIndex];
                if (attachment.ManualAssignment is not null ||
                    !string.Equals(
                        attachment.TargetBoneName,
                        before.TargetBoneName,
                        StringComparison.Ordinal))
                {
                    throw new InvalidOperationException(
                        $"{context}: an unrelated automatic attachment changed.");
                }
                continue;
            }

            string expectedBone = item.Target switch
            {
                GeneratedSkinningComponentAttachmentTarget.UpperBack => "Spine_03",
                GeneratedSkinningComponentAttachmentTarget.Head => "Head",
                _ => throw new InvalidOperationException(
                    $"{context}: unsupported expected assignment {item.Target}.")
            };
            if (attachment.ManualAssignment != item.Target ||
                !string.Equals(
                    attachment.TargetBoneName,
                    expectedBone,
                    StringComparison.Ordinal) ||
                !MembershipExactlyEqual(attachment.VerticesByMesh, item.VerticesByMesh))
            {
                throw new InvalidOperationException(
                    $"{context}: component #{componentIndex} did not retain its exact " +
                    $"manual {item.Target} identity.");
            }
            foreach (TargetRigBodyVertexMembership membership in item.VerticesByMesh)
            {
                foreach ((string sceneName, ImportedScene scene) in new[]
                         {
                          ("fitting", overridden.FittingPreviewScene),
                          ("canonical", overridden.PreparedScene)
                         })
                {
                    ImportedMesh mesh = scene.Meshes[membership.MeshIndex];
                    ImportedSkinning skin = mesh.Skinning ??
                        throw new InvalidOperationException(
                            $"{context}: overridden {sceneName} mesh has no skinning.");
                    int expectedJoint = skin.Skeleton.JointNames
                        .Select((name, index) => (name, index))
                        .Single(pair => string.Equals(
                            pair.name,
                            expectedBone,
                            StringComparison.Ordinal)).index;
                    foreach (int vertex in membership.VertexIndices)
                    {
                        ImportedJointIndices joints = skin.JointIndices[vertex];
                        if (skin.Weights[vertex] != Vector4.UnitX ||
                            joints.X != expectedJoint ||
                            joints.Y != 0 || joints.Z != 0 || joints.W != 0)
                        {
                            throw new InvalidOperationException(
                                $"{context}: component #{componentIndex} vertex " +
                                $"[{membership.MeshIndex}:{vertex}] is not exact " +
                                $"one-hot {expectedBone} in the {sceneName} scene.");
                        }
                    }
                }
            }
        }
    }

    private static float SemanticMass(
        ImportedMesh mesh,
        int vertex,
        IReadOnlySet<string> boneNames)
    {
        ImportedSkinning skin = mesh.Skinning ?? throw new InvalidOperationException(
            $"Mesh '{mesh.Name}' has no generated skinning.");
        ImportedJointIndices joints = skin.JointIndices[vertex];
        Vector4 weights = skin.Weights[vertex];
        ushort[] indices = [joints.X, joints.Y, joints.Z, joints.W];
        float[] values = [weights.X, weights.Y, weights.Z, weights.W];
        float result = 0;
        for (int influence = 0; influence < 4; influence++)
        {
            if (values[influence] > 0 &&
                boneNames.Contains(skin.Skeleton.JointNames[indices[influence]]))
                result += values[influence];
        }
        return result;
    }

    private static string DominantBone(ImportedMesh mesh, int vertex)
    {
        ImportedSkinning skin = mesh.Skinning ?? throw new InvalidOperationException(
            $"Mesh '{mesh.Name}' has no generated skinning.");
        ImportedJointIndices joints = skin.JointIndices[vertex];
        Vector4 weights = skin.Weights[vertex];
        (ushort Joint, float Weight)[] influences =
        [
            (joints.X, weights.X),
            (joints.Y, weights.Y),
            (joints.Z, weights.Z),
            (joints.W, weights.W)
        ];
        ushort dominant = influences
            .OrderByDescending(influence => influence.Weight)
            .ThenBy(influence => influence.Joint)
            .First().Joint;
        return skin.Skeleton.JointNames[dominant];
    }

    private static bool MembershipExactlyEqual(
        IReadOnlyList<TargetRigBodyVertexMembership> left,
        IReadOnlyList<TargetRigBodyVertexMembership> right)
    {
        if (left.Count != right.Count)
            return false;
        return left.OrderBy(value => value.MeshIndex)
            .Zip(right.OrderBy(value => value.MeshIndex))
            .All(pair => pair.First.MeshIndex == pair.Second.MeshIndex &&
                         string.Equals(
                             pair.First.MeshName,
                             pair.Second.MeshName,
                             StringComparison.Ordinal) &&
                         pair.First.VertexIndices.SequenceEqual(
                             pair.Second.VertexIndices));
    }

    private static Vector3 At(
        TargetRigDefinition rig,
        TargetRigFittingPoseSnapshot pose,
        string name) => Translation(pose.WorldMatrices[rig.GetJointIndex(name)]);

    private static Vector3 Translation(Matrix4x4 matrix) =>
        new(matrix.M41, matrix.M42, matrix.M43);
}
