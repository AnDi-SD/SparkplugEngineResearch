using System.Numerics;
using System.IO;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class TargetRigFittingPreviewRegression
{
    private const float WeightEpsilon = 0.000001f;

    public static void Run(string sourcePath,
        Func<SmoDocument, SmoExportScene, TargetRigFittingPoseSnapshot, TargetRigFittingPreviewResult>? build = null)
    {
        if (build is null)
        {
            RunProjection(sourcePath);
            return;
        }
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
            build(target, targetScene, identitySnapshot);
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
            if (before.Positions.Length != after.Positions.Length ||
                before.Positions.Zip(after.Positions).Any(pair => Vector3.Distance(pair.First, pair.Second) > .0025f) ||
                after.Normals.Length != 0 || ReferenceEquals(before.Positions, after.Positions))
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
            build(target, targetScene, translatedPose.Capture());
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
            FindWorkingLocalRotation(target, targetScene, rig, build);
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
            $"TARGET FITTING PREVIEW GPU PASS: identity error <= 0.0025; " +
            $"translated={translatedVertices} vertices, max-error=" +
            $"{maximumTranslationError:G9}; local-joint={localMovement.JointName}, " +
            $"max-movement={localMovement.MaximumMovement:G9}; " +
            "target graph, hierarchy, lengths and inverse binds unchanged.");
    }

    // Portable test of the provider/DTO boundary; this does not evaluate skinning.
    // Numerical target-pose assertions above run only with the actual GPU host.
    private static void RunProjection(string sourcePath)
    {
        byte[] bytes = File.ReadAllBytes(sourcePath);
        SmoDocument document = SmoDocument.Parse(bytes, sourcePath);
        SmoExportScene scene = SmoSceneBuilder.Build(document);
        TargetRigFittingPoseSnapshot pose = TargetRigDefinition.FromSmoDocument(document).CreateFittingPose().Capture();
        int calls = 0;
        var result = TargetRigFittingPreviewBuilder.Build(scene, pose, request =>
        {
            ++calls;
            if (!scene.Meshes.Any(mesh => ReferenceEquals(mesh, request.Mesh)) ||
                request.PaletteTransforms.Count == 0)
                throw new InvalidOperationException("Provider did not receive the stable original mesh and palette.");
            return request.Mesh.Positions;
        });
        if (calls == 0 || calls != scene.Meshes.Count(mesh => mesh.SkinObjectIndex is not null) ||
            result.Scene.Meshes.Where((mesh, index) => ReferenceEquals(mesh.Positions, scene.Meshes[index].Positions)).Any() ||
            result.Scene.Meshes.Any(mesh => mesh.Normals.Length != 0) ||
            !File.ReadAllBytes(sourcePath).SequenceEqual(bytes))
            throw new InvalidOperationException("Identity provider calls, snapshot ownership or input immutability failed.");
        bool failed = false;
        try { TargetRigFittingPreviewBuilder.Build(scene, pose, _ => []); }
        catch (InvalidDataException) { failed = true; }
        if (!failed) throw new InvalidOperationException("A missing backend vertex snapshot was accepted.");
        failed = false;
        try { TargetRigFittingPreviewBuilder.Build(scene, pose, request =>
            Enumerable.Repeat(new Vector3(float.NaN), request.Mesh.Positions.Length).ToArray()); }
        catch (InvalidDataException) { failed = true; }
        if (!failed) throw new InvalidOperationException("A non-finite backend vertex snapshot was accepted.");
        Console.WriteLine($"TARGET FITTING PROVIDER PASS: {calls} original meshes; identity uses provider; owned positions-only snapshots; invalid output rejected. GPU geometry is checked separately.");
    }

    private static (string JointName, float MaximumMovement) FindWorkingLocalRotation(
        SmoDocument target,
        SmoExportScene targetScene,
        TargetRigDefinition rig,
        Func<SmoDocument, SmoExportScene, TargetRigFittingPoseSnapshot, TargetRigFittingPreviewResult> build)
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
                    build(target, targetScene, snapshot);
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
