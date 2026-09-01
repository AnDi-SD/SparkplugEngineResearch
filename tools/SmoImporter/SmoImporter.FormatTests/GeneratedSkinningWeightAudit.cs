using System.Numerics;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class GeneratedSkinningWeightAudit
{
    public static void Run(
        string targetArgument,
        string donorArgument,
        bool verifyManualException = false)
    {
        string targetPath = Path.GetFullPath(targetArgument);
        string donorPath = Path.GetFullPath(donorArgument);
        byte[] targetBefore = File.ReadAllBytes(targetPath);
        byte[] donorBefore = File.ReadAllBytes(donorPath);

        SmoDocument target = SmoDocument.Load(targetPath);
        ImportedScene donor = ImportedModelReader.ReadGeometryOnly(donorPath);
        SmoExportScene targetScene = SmoSceneBuilder.Build(
            target,
            new SmoExportOptions(
                ApplyWorldTransforms: true,
                AnimationPaths: null,
                Resources: SmoExportResourceTypes.Meshes |
                           SmoExportResourceTypes.Skeleton));
        ReplacementTransform alignment = ReplacementTransformFitter.FitByHeightAndCenter(
            targetScene.Meshes.SelectMany(mesh => mesh.Positions),
            donor.Meshes.SelectMany(mesh => mesh.Positions));
        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
        TargetRigFittingPoseSnapshot pose;
        TargetRigBodySelection body;
        string poseMode;
        try
        {
            TargetRigAutomaticPoseFitResult fit = TargetRigAutomaticPoseFitter.Fit(
                rig,
                targetScene,
                donor,
                alignment);
            pose = fit.Pose;
            body = fit.BodySelection;
            poseMode = "automatic";
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          ArgumentException)
        {
            pose = rig.CreateFittingPose().Capture();
            body = TargetRigAutomaticPoseFitter.SelectBody(rig, donor, alignment);
            poseMode = "reset-fallback:" + exception.GetType().Name;
        }

        GeneratedSkinningPreparationResult preparation =
            GeneratedSkinningPreparer.PrepareCancellable(
                target,
                donor,
                pose,
                alignment,
                body,
                componentOverrides: null,
                regionOverrides: null,
                CancellationToken.None,
                progress: null,
                enableAutomaticBackExtraction: false);
        HashSet<string> activeJoints = CollectActiveJointNames(
            preparation.PreparedScene);
        double headFraction = preparation.Analysis.PreparedVertexCount == 0
            ? 0
            : (double)preparation.Analysis.HeadProtectedComponentVertexCount /
              preparation.Analysis.PreparedVertexCount;
        if (activeJoints.Count <
            GeneratedSkinningQualityGuard.MinimumHumanoidActiveJointCount)
        {
            throw new InvalidOperationException(
                $"Generated weight audit found only {activeJoints.Count} active joints.");
        }
        if (headFraction >
            GeneratedSkinningQualityGuard.MaximumRigidHeadVertexFraction)
        {
            throw new InvalidOperationException(
                $"Generated weight audit found pathological Head ownership " +
                $"{headFraction:P2}.");
        }
        if (preparation.Analysis.CapsuleOnlyVertexCount < 0)
            throw new InvalidOperationException(
                "Generated weight audit found negative capsule-only accounting.");
        if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBefore) ||
            !File.ReadAllBytes(donorPath).SequenceEqual(donorBefore))
        {
            throw new InvalidOperationException(
                "Generated weight audit changed an input file.");
        }
        if (preparation.Analysis.Components.Count !=
            preparation.Analysis.DonorComponentCount ||
            preparation.Analysis.Components.Select(component =>
                component.ComponentIndex).Distinct().Count() !=
            preparation.Analysis.DonorComponentCount)
        {
            throw new InvalidOperationException(
                "Generated component UI contract does not expose every component exactly once.");
        }
        if (preparation.Analysis.Components.Any(component =>
                component.AutomaticBinding ==
                    GeneratedSkinningAutomaticComponentBinding.UpperBack) ||
            preparation.Analysis.Attachments.Any(attachment =>
                attachment.PlaneAssignment ==
                    GeneratedSkinningComponentAttachmentTarget.UpperBack))
        {
            throw new InvalidOperationException(
                "Importer-mode audit found an automatic Back extraction.");
        }

        int? manualBackComponent = null;
        if (verifyManualException)
        {
            GeneratedSkinningComponentInfo selected = preparation.Analysis.Components
                .Where(component => component.ManualAssignment is null)
                .Where(component => component.AutomaticBinding ==
                    GeneratedSkinningAutomaticComponentBinding.GeneratedWeights)
                .OrderBy(component => component.VertexCount)
                .FirstOrDefault() ?? throw new InvalidOperationException(
                    "No automatic component is available for the manual exception audit.");
            var componentOverrides = new GeneratedSkinningComponentOverrides(
                [new GeneratedSkinningComponentOverride(
                    selected.ComponentIndex,
                    GeneratedSkinningComponentAttachmentTarget.UpperBack,
                    selected.VerticesByMesh)],
                preparation.Analysis.DonorComponentCount,
                preparation.Analysis.TargetRigFingerprint,
                preparation.Analysis.DonorGeometryFingerprint);
            GeneratedSkinningPreparationResult manual =
                GeneratedSkinningPreparer.PrepareCancellable(
                    target,
                    donor,
                    pose,
                    alignment,
                    body,
                    componentOverrides,
                    regionOverrides: null,
                    CancellationToken.None,
                    progress: null,
                    enableAutomaticBackExtraction: false);
            GeneratedSkinningComponentInfo rebound = manual.Analysis.Components
                .Single(component =>
                    component.ComponentIndex == selected.ComponentIndex);
            if (rebound.ManualAssignment !=
                    GeneratedSkinningComponentAttachmentTarget.UpperBack ||
                !manual.Analysis.Attachments.Any(attachment =>
                    attachment.ComponentIndex == selected.ComponentIndex &&
                    attachment.ManualAssignment ==
                        GeneratedSkinningComponentAttachmentTarget.UpperBack &&
                    attachment.TargetBoneName == "Spine_03") ||
                manual.Analysis.Components.Count !=
                    preparation.Analysis.Components.Count)
            {
                throw new InvalidOperationException(
                    "An automatic component did not become a manual Spine_03 exception " +
                    "while remaining in the complete component list.");
            }
            manualBackComponent = selected.ComponentIndex;
        }

        Console.WriteLine(
            $"GENERATED WEIGHT AUDIT PASS: target={Path.GetFileName(targetPath)}; " +
            $"donor={Path.GetFileName(donorPath)}; vertices=" +
            $"{preparation.Analysis.PreparedVertexCount}; components=" +
            $"{preparation.Analysis.DonorComponentCount}; activeJoints=" +
            $"{activeJoints.Count}; headVertices=" +
            $"{preparation.Analysis.HeadProtectedComponentVertexCount} " +
            $"({headFraction:P2}); capsuleOnly=" +
            $"{preparation.Analysis.CapsuleOnlyVertexCount}; allComponents=" +
            $"{preparation.Analysis.Components.Count}; autoBack=off; " +
            $"manualBackTest={manualBackComponent?.ToString() ?? "not-requested"}; " +
            $"pose={poseMode}; " +
            $"active=[{string.Join(",", activeJoints.Order())}]");
    }

    private static HashSet<string> CollectActiveJointNames(ImportedScene scene)
    {
        var result = new HashSet<string>(StringComparer.Ordinal);
        foreach (ImportedMesh mesh in scene.Meshes)
        {
            ImportedSkinning skin = mesh.Skinning ?? throw new InvalidDataException(
                $"Prepared mesh '{mesh.Name}' has no skinning.");
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
                ImportedJointIndices joints = skin.JointIndices[vertex];
                Vector4 weights = skin.Weights[vertex];
                ushort[] indices = [joints.X, joints.Y, joints.Z, joints.W];
                float[] values = [weights.X, weights.Y, weights.Z, weights.W];
                for (int influence = 0; influence < 4; influence++)
                {
                    if (values[influence] <= 0.000001f)
                        continue;
                    if (indices[influence] >= skin.Skeleton.JointNames.Count)
                        throw new InvalidDataException(
                            $"Prepared mesh '{mesh.Name}' has an invalid joint index.");
                    result.Add(skin.Skeleton.JointNames[indices[influence]]);
                }
            }
        }
        return result;
    }
}
