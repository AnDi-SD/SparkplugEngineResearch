using System.Numerics;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class TargetRigFittingPreviewRegression
{
    private const float WeightEpsilon = 0.000001f;

    public static void Run(string sourcePath)
    {
        string fullPath = Path.GetFullPath(sourcePath);
        byte[] sourceBytes = File.ReadAllBytes(fullPath);
        SmoDocument target = SmoDocument.Load(fullPath);
        SmoExportScene targetScene = SmoSceneBuilder.Build(target);
        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
        if (targetScene.Meshes.All(mesh => mesh.SkinObjectIndex is null))
            throw new InvalidOperationException(
                "Target preview regression requires at least one skinned mesh.");

        Vector3[][] sourcePositions = targetScene.Meshes
            .Select(mesh => mesh.Positions.ToArray()).ToArray();
        Vector3[][] sourceNormals = targetScene.Meshes
            .Select(mesh => mesh.Normals.ToArray()).ToArray();
        (int Parent, float Length, Matrix4x4 BindWorld)[] rigBefore = rig.Joints
            .Select(joint => (
                joint.ParentJointIndex,
                joint.BindLengthFromParent,
                joint.BindWorldMatrix))
            .ToArray();
        Matrix4x4[][] inverseBindsBefore = targetScene.Skins
            .Select(skin => skin.InverseBindMatrices.ToArray()).ToArray();

        TargetRigFittingPoseSnapshot identitySnapshot =
            rig.CreateFittingPose().Capture();
        TargetRigFittingPreviewResult identity =
            TargetRigFittingPreviewBuilder.Build(targetScene, identitySnapshot);
        if (!identity.IsIdentityPose || identity.SkinnedMeshCount <= 0 ||
            identity.SkinnedVertexCount <= 0 ||
            identity.Scene.Meshes.Count != targetScene.Meshes.Count)
        {
            throw new InvalidOperationException(
                "Identity target preview summary is inconsistent.");
        }
        for (int meshIndex = 0; meshIndex < targetScene.Meshes.Count; meshIndex++)
        {
            SmoExportMesh before = targetScene.Meshes[meshIndex];
            SmoExportMesh after = identity.Scene.Meshes[meshIndex];
            if (!before.Positions.SequenceEqual(after.Positions) ||
                !before.Normals.SequenceEqual(after.Normals) ||
                ReferenceEquals(before.Positions, after.Positions) ||
                ReferenceEquals(before.Normals, after.Normals) &&
                before.Normals.Length != 0)
            {
                throw new InvalidOperationException(
                    $"Identity preview changed or aliased geometry of mesh " +
                    $"[{before.ObjectIndex}] '{before.Name}'.");
            }
        }
        VerifyGraphAndInputsUnchanged(
            targetScene,
            identity.Scene,
            sourcePositions,
            sourceNormals,
            rig,
            rigBefore,
            inverseBindsBefore,
            "identity");

        Vector3 translation = new(1.25f, -0.5f, 0.75f);
        TargetRigFittingPose translatedPose = rig.CreateFittingPose();
        translatedPose.SetRootTransform(Quaternion.Identity, translation);
        TargetRigFittingPreviewResult translated =
            TargetRigFittingPreviewBuilder.Build(
                targetScene, translatedPose.Capture());
        int translatedVertices = 0;
        float maximumTranslationError = 0;
        for (int meshIndex = 0; meshIndex < targetScene.Meshes.Count; meshIndex++)
        {
            SmoExportMesh before = targetScene.Meshes[meshIndex];
            SmoExportMesh after = translated.Scene.Meshes[meshIndex];
            if (before.SkinObjectIndex is null)
            {
                if (!before.Positions.SequenceEqual(after.Positions))
                    throw new InvalidOperationException(
                        "Target pose preview moved an unskinned mesh.");
                continue;
            }
            for (int vertex = 0; vertex < before.Positions.Length; vertex++)
            {
                float error = Vector3.Distance(
                    before.Positions[vertex] + translation,
                    after.Positions[vertex]);
                maximumTranslationError = MathF.Max(maximumTranslationError, error);
                translatedVertices++;
            }
        }
        if (translated.IsIdentityPose || translatedVertices == 0 ||
            maximumTranslationError > 0.0025f)
        {
            throw new InvalidOperationException(
                $"Root-translation preview is inconsistent; maximum vertex " +
                $"error={maximumTranslationError:G9}.");
        }
        VerifyGraphAndInputsUnchanged(
            targetScene,
            translated.Scene,
            sourcePositions,
            sourceNormals,
            rig,
            rigBefore,
            inverseBindsBefore,
            "translated");

        (string JointName, float MaximumMovement) localMovement =
            FindWorkingLocalRotation(targetScene, rig);
        VerifyGraphAndInputsUnchanged(
            targetScene,
            targetScene,
            sourcePositions,
            sourceNormals,
            rig,
            rigBefore,
            inverseBindsBefore,
            "local rotation");
        if (!File.ReadAllBytes(fullPath).SequenceEqual(sourceBytes))
            throw new InvalidOperationException(
                "Target fitting preview modified the source SMO file.");

        Console.WriteLine(
            $"TARGET FITTING PREVIEW PASS: identity exact; " +
            $"translated={translatedVertices} vertices, max-error=" +
            $"{maximumTranslationError:G9}; local-joint={localMovement.JointName}, " +
            $"max-movement={localMovement.MaximumMovement:G9}; " +
            "target graph, hierarchy, lengths and inverse binds unchanged.");
    }

    private static (string JointName, float MaximumMovement) FindWorkingLocalRotation(
        SmoExportScene targetScene,
        TargetRigDefinition rig)
    {
        Dictionary<int, SmoExportSkin> skins = targetScene.Skins
            .ToDictionary(skin => skin.ObjectIndex);
        var usedJointObjects = new HashSet<int>();
        foreach (SmoExportMesh mesh in targetScene.Meshes.Where(
                     item => item.SkinObjectIndex is not null))
        {
            SmoExportSkin skin = skins[mesh.SkinObjectIndex!.Value];
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
                Vector4 weights = mesh.BlendWeights[vertex];
                Vector4 joints = mesh.JointIndices[vertex];
                float[] weightValues = [weights.X, weights.Y, weights.Z, weights.W];
                float[] jointValues = [joints.X, joints.Y, joints.Z, joints.W];
                for (int influence = 0; influence < 4; influence++)
                {
                    if (weightValues[influence] <= WeightEpsilon)
                        continue;
                    int paletteIndex = checked((int)MathF.Round(jointValues[influence]));
                    usedJointObjects.Add(skin.JointObjectIndices[paletteIndex]);
                }
            }
        }

        TargetRigJoint[] candidates = rig.Joints
            .Where(joint => joint.IsDeformJoint &&
                            joint.ParentJointIndex >= 0 &&
                            usedJointObjects.Contains(joint.ObjectIndex))
            .OrderBy(joint => joint.JointIndex)
            .ToArray();
        Vector3[] axes = [Vector3.UnitX, Vector3.UnitY, Vector3.UnitZ];
        foreach (TargetRigJoint candidate in candidates)
        {
            foreach (Vector3 axis in axes)
            {
                TargetRigFittingPose pose = rig.CreateFittingPose();
                pose.SetLocalRotationDelta(
                    candidate.JointIndex,
                    Quaternion.CreateFromAxisAngle(axis, MathF.PI / 12f));
                TargetRigFittingPoseSnapshot snapshot = pose.Capture();
                if (snapshot.IsIdentityPose)
                    throw new InvalidOperationException(
                        "A local fitting edit was incorrectly captured as identity.");
                TargetRigFittingPreviewResult preview =
                    TargetRigFittingPreviewBuilder.Build(targetScene, snapshot);
                float maximumMovement = 0;
                for (int meshIndex = 0; meshIndex < targetScene.Meshes.Count; meshIndex++)
                {
                    if (targetScene.Meshes[meshIndex].SkinObjectIndex is null)
                        continue;
                    for (int vertex = 0;
                         vertex < targetScene.Meshes[meshIndex].Positions.Length;
                         vertex++)
                    {
                        maximumMovement = MathF.Max(
                            maximumMovement,
                            Vector3.Distance(
                                targetScene.Meshes[meshIndex].Positions[vertex],
                                preview.Scene.Meshes[meshIndex].Positions[vertex]));
                    }
                }
                if (maximumMovement > 0.0001f)
                    return (candidate.Name, maximumMovement);
            }
        }

        throw new InvalidOperationException(
            "No used local target joint produced visible skinned-mesh movement.");
    }

    private static void VerifyGraphAndInputsUnchanged(
        SmoExportScene source,
        SmoExportScene preview,
        IReadOnlyList<Vector3[]> sourcePositions,
        IReadOnlyList<Vector3[]> sourceNormals,
        TargetRigDefinition rig,
        IReadOnlyList<(int Parent, float Length, Matrix4x4 BindWorld)> rigBefore,
        IReadOnlyList<Matrix4x4[]> inverseBindsBefore,
        string context)
    {
        if (!ReferenceEquals(source.Nodes, preview.Nodes) ||
            !ReferenceEquals(source.Skins, preview.Skins) ||
            source.Meshes.Count != preview.Meshes.Count)
        {
            throw new InvalidOperationException(
                $"{context}: preview replaced the target hierarchy or skin catalog.");
        }
        for (int meshIndex = 0; meshIndex < source.Meshes.Count; meshIndex++)
        {
            SmoExportMesh mesh = source.Meshes[meshIndex];
            SmoExportMesh posedMesh = preview.Meshes[meshIndex];
            if (!mesh.Positions.SequenceEqual(sourcePositions[meshIndex]) ||
                !mesh.Normals.SequenceEqual(sourceNormals[meshIndex]) ||
                mesh.SkinObjectIndex != posedMesh.SkinObjectIndex ||
                mesh.ParentNodeObjectIndex != posedMesh.ParentNodeObjectIndex ||
                mesh.BindWorldMatrix != posedMesh.BindWorldMatrix ||
                mesh.BindLocalMatrix != posedMesh.BindLocalMatrix ||
                !ReferenceEquals(mesh.Texture, posedMesh.Texture) ||
                !ReferenceEquals(mesh.EffectTexture, posedMesh.EffectTexture) ||
                mesh.MaterialColor != posedMesh.MaterialColor ||
                mesh.UsesAlphaBlend != posedMesh.UsesAlphaBlend ||
                !ReferenceEquals(mesh.TextureCoordinates0, posedMesh.TextureCoordinates0) ||
                !ReferenceEquals(mesh.TextureCoordinates1, posedMesh.TextureCoordinates1) ||
                !ReferenceEquals(mesh.Colors, posedMesh.Colors) ||
                !ReferenceEquals(mesh.TriangleIndices, posedMesh.TriangleIndices))
            {
                throw new InvalidOperationException(
                    $"{context}: target mesh input or bind ownership changed.");
            }
        }
        for (int skinIndex = 0; skinIndex < source.Skins.Count; skinIndex++)
        {
            if (!source.Skins[skinIndex].InverseBindMatrices.SequenceEqual(
                    inverseBindsBefore[skinIndex]))
            {
                throw new InvalidOperationException(
                    $"{context}: target inverse-bind palette changed.");
            }
        }
        for (int jointIndex = 0; jointIndex < rig.Joints.Count; jointIndex++)
        {
            TargetRigJoint joint = rig.Joints[jointIndex];
            var before = rigBefore[jointIndex];
            if (joint.ParentJointIndex != before.Parent ||
                joint.BindLengthFromParent != before.Length ||
                joint.BindWorldMatrix != before.BindWorld)
            {
                throw new InvalidOperationException(
                    $"{context}: fitting rig hierarchy, length or bind changed.");
            }
        }
    }
}
