using System.Numerics;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class PortingModePairRegression
{
    public static void Run(
        string modeArgument,
        string targetArgument,
        string donorArgument,
        string? retainedOutputArgument = null)
    {
        string mode = modeArgument.Trim().ToLowerInvariant();
        string targetPath = RequireFile(targetArgument, ".smo", "target");
        string donorPath = RequireFile(donorArgument, null, "donor");
        byte[] targetBefore = File.ReadAllBytes(targetPath);
        byte[] donorBefore = File.ReadAllBytes(donorPath);
        bool retainOutput = retainedOutputArgument is not null;
        string? retainedOutputPath = retainedOutputArgument is null
            ? null
            : Path.GetFullPath(retainedOutputArgument);
        if (retainedOutputPath is not null)
        {
            Require(Path.GetExtension(retainedOutputPath).Equals(
                    ".smo", StringComparison.OrdinalIgnoreCase),
                "retained output must be a .smo file");
            Require(!string.Equals(retainedOutputPath, targetPath,
                    StringComparison.OrdinalIgnoreCase) &&
                    !string.Equals(retainedOutputPath, donorPath,
                    StringComparison.OrdinalIgnoreCase),
                "retained output must be separate from both inputs");
            Require(!File.Exists(retainedOutputPath),
                "retained output already exists");
        }
        string directory = retainedOutputPath is null
            ? Path.Combine(
                Path.GetTempPath(),
                $"SmoPortingPair-{mode}-{Guid.NewGuid():N}")
            : Path.GetDirectoryName(retainedOutputPath)!;
        Directory.CreateDirectory(directory);
        string outputPath = retainedOutputPath ??
            Path.Combine(directory, "output.smo");

        try
        {
            SmoDocument target = SmoDocument.Load(targetPath);
            Require(!target.HasErrors, "target has parser errors");
            PairResult pair = mode switch
            {
                "native" => RunNative(target, donorPath, outputPath),
                "prepared" => RunPrepared(target, donorPath, outputPath),
                "adapt" => RunAdapt(target, donorPath, outputPath),
                "generate" => RunGenerate(target, donorPath, outputPath),
                "static" => RunStatic(target, donorPath, outputPath),
                _ => throw new ArgumentException(
                    "Mode must be native, prepared, adapt, generate, or static.")
            };

            Require(File.Exists(outputPath), "writer did not install output");
            SmoDocument output = SmoDocument.Load(outputPath);
            Require(!output.HasErrors, "output has parser errors");
            Require(output.Objects.Count(entry =>
                    entry.TypeHash == SmoClassIds.MeshData) == pair.MeshCount,
                "output mesh count differs from writer result");
            if (mode == "static")
            {
                Require(!output.Objects.Any(entry =>
                        entry.TypeHash == SmoClassIds.Skin),
                    "static output contains spSkin");
                Require(output.Objects.Where(entry =>
                        entry.TypeHash == SmoClassIds.MeshData)
                    .All(entry => !SmoMeshDecoder.Decode(output, entry)
                        .HasSkinningData),
                    "static output contains a skinned vertex layout");
            }
            else
            {
                Require(output.Objects.Any(entry =>
                        entry.TypeHash == SmoClassIds.Skin),
                    $"{mode} output contains no spSkin");
            }
            Require(File.ReadAllBytes(targetPath).SequenceEqual(targetBefore),
                "target source changed");
            Require(File.ReadAllBytes(donorPath).SequenceEqual(donorBefore),
                "donor source changed");

            Console.WriteLine(
                $"PORTING MODE PAIR PASS: mode={mode}; " +
                $"target={Path.GetFileName(targetPath)}; " +
                $"donor={Path.GetFileName(donorPath)}; meshes={pair.MeshCount}; " +
                $"vertices={pair.VertexCount}; triangles={pair.TriangleCount}; " +
                $"details={pair.Details}");
            if (retainOutput)
                Console.WriteLine($"PORTING MODE OUTPUT: {outputPath}");
        }
        finally
        {
            if (!retainOutput && Directory.Exists(directory))
                Directory.Delete(directory, recursive: true);
        }
    }

    private static PairResult RunNative(
        SmoDocument target,
        string donorPath,
        string outputPath)
    {
        Require(Path.GetExtension(donorPath).Equals(
                ".smo", StringComparison.OrdinalIgnoreCase),
            "native mode requires an SMO donor");
        SmoDocument donor = SmoDocument.Load(donorPath);
        Require(!donor.HasErrors, "native donor has parser errors");
        SmoNativeVisualGraphPlan plan =
            SmoNativeVisualGraphReplacer.Analyze(target, donor);
        Require(plan.CanReplace,
            "native analysis failed: " + string.Join(" | ", plan.Errors));
        SmoNativeVisualGraphReplaceResult result =
            SmoNativeVisualGraphReplacer.Replace(target, donor, outputPath);
        SmoDocument output = SmoDocument.Load(outputPath);
        int vertices = DecodeMeshes(output).Sum(mesh => mesh.VertexCount);
        int triangles = DecodeMeshes(output).Sum(mesh => mesh.TriangleCount);
        return new PairResult(
            result.MeshCount,
            vertices,
            triangles,
            $"roots={result.VisualRootCount}; textures={result.TextureCount}");
    }

    private static PairResult RunPrepared(
        SmoDocument target,
        string donorPath,
        string outputPath)
    {
        ImportedScene donor = LoadDonor(donorPath, geometryOnly: false);
        Require(donor.Meshes.Count > 0 &&
                donor.Meshes.All(mesh => mesh.Skinning is not null),
            "prepared mode requires every donor mesh to have skinning");
        GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(target, donor);
        Require(plan.CanReplace,
            "prepared analysis failed: " + string.Join(" | ", plan.Messages));
        GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
            target,
            donor,
            ReplacementTransform.Identity,
            outputPath,
            SkinnedGeometryTransferMode.RetargetToGameBindPose);
        return new PairResult(
            result.MeshSlotCount,
            result.VertexCount,
            result.TriangleCount,
            $"activeJoints={plan.ActiveJointCount}; palettes={result.PaletteCount}");
    }

    private static PairResult RunAdapt(
        SmoDocument target,
        string donorPath,
        string outputPath)
    {
        ImportedScene donor = LoadDonor(donorPath, geometryOnly: false);
        SkinnedModelPortingAnalysis adaptation =
            SkinnedModelPortingPreparer.AnalyzeAdaptDonorWeights(target, donor);
        Require(adaptation.CanPrepare,
            "adapt analysis failed: " + string.Join(" | ", adaptation.Errors));
        SkinnedModelPortingPreparation preparation =
            SkinnedModelPortingPreparer.PrepareAdaptDonorWeights(target, donor);
        GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(
            target,
            preparation.PreparedScene);
        Require(plan.CanReplace,
            "adapted writer analysis failed: " +
            string.Join(" | ", plan.Messages));
        GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
            target,
            preparation.PreparedScene,
            ReplacementTransform.Identity,
            outputPath,
            SkinnedGeometryTransferMode.PreservePreparedGeometry);
        return new PairResult(
            result.MeshSlotCount,
            result.VertexCount,
            result.TriangleCount,
            $"mapped={adaptation.JointMappings.Count}; palettes={result.PaletteCount}");
    }

    private static PairResult RunGenerate(
        SmoDocument target,
        string donorPath,
        string outputPath)
    {
        ImportedScene donor = LoadDonor(donorPath, geometryOnly: true);
        Require(donor.Meshes.Count > 0 &&
                donor.Meshes.All(mesh => mesh.Skinning is null),
            "geometry-only donor is not fully unskinned");
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
        TargetRigFittingPoseSnapshot fittingPose;
        TargetRigBodySelection bodySelection;
        string poseMode;
        try
        {
            TargetRigAutomaticPoseFitResult fit = TargetRigAutomaticPoseFitter.Fit(
                rig,
                targetScene,
                donor,
                alignment);
            fittingPose = fit.Pose;
            bodySelection = fit.BodySelection;
            poseMode = "automatic";
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          ArgumentException)
        {
            fittingPose = rig.CreateFittingPose().Capture();
            bodySelection = TargetRigAutomaticPoseFitter.SelectBody(
                rig,
                donor,
                alignment);
            poseMode = "reset-fallback:" + exception.GetType().Name;
        }
        GeneratedSkinningPreparationResult preparation =
            GeneratedSkinningPreparer.Prepare(
                target,
                donor,
                fittingPose,
                alignment,
                bodySelection,
                enableAutomaticBackExtraction: false);
        Require(preparation.Analysis.Components.Count ==
                    preparation.Analysis.DonorComponentCount,
            "generated component UI contract did not expose every donor component");
        Require(!preparation.Analysis.Components.Any(component =>
                    component.AutomaticBinding ==
                        GeneratedSkinningAutomaticComponentBinding.UpperBack) &&
                !preparation.Analysis.Attachments.Any(attachment =>
                    attachment.PlaneAssignment ==
                        GeneratedSkinningComponentAttachmentTarget.UpperBack),
            "generated importer path unexpectedly enabled automatic Back extraction");
        GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(
            target,
            preparation.PreparedScene);
        plan = GeneratedSkinningQualityGuard.Apply(
            preparation.Analysis,
            plan);
        Require(plan.CanReplace,
            "generated writer analysis failed: " +
            string.Join(" | ", plan.Messages));
        GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
            target,
            preparation.PreparedScene,
            ReplacementTransform.Identity,
            outputPath,
            SkinnedGeometryTransferMode.PreservePreparedGeometry);
        return new PairResult(
            result.MeshSlotCount,
            result.VertexCount,
            result.TriangleCount,
            $"components={preparation.Analysis.DonorComponentCount}; " +
            $"allComponents={preparation.Analysis.Components.Count}; " +
            "autoBack=off; " +
            $"attachments={preparation.Analysis.Attachments.Count}; " +
            $"activeJoints={plan.ActiveJointCount}; " +
            $"headVertices=" +
            $"{preparation.Analysis.HeadProtectedComponentVertexCount}; " +
            $"palettes={result.PaletteCount}; pose={poseMode}");
    }

    private static PairResult RunStatic(
        SmoDocument target,
        string donorPath,
        string outputPath)
    {
        ImportedScene donor = LoadDonor(donorPath, geometryOnly: true);
        int selectedMeshIndex = target.Objects.First(entry =>
            entry.TypeHash == SmoClassIds.MeshData).Index;
        SmoLevelModelGraphReplacementAnalysis plan =
            SmoLevelModelGraphReplacer.Analyze(
                target,
                selectedMeshIndex,
                donor,
                SmoLevelModelGraphWriteOptions.StandaloneStatic);
        Require(plan.CanReplace,
            "static analysis failed: " + string.Join(" | ", plan.Messages));
        ReplacementTransform transform = ReplacementTransformFitter.FitByHeightAndCenter(
            SmoSceneBuilder.Build(target).Meshes.SelectMany(mesh => mesh.Positions),
            donor.Meshes.SelectMany(mesh => mesh.Positions));
        SmoLevelModelGraphFileResult result = SmoLevelModelGraphReplacer.ReplaceFile(
            target,
            selectedMeshIndex,
            donor,
            transform,
            outputPath,
            SmoLevelModelGraphWriteOptions.StandaloneStatic,
            protectedInputPaths: [donorPath]);
        return new PairResult(
            result.MeshCount,
            result.VertexCount,
            result.TriangleCount,
            $"textures={result.TextureCount}");
    }

    private static SmoMesh[] DecodeMeshes(SmoDocument document) =>
        document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .Select(entry => SmoMeshDecoder.Decode(document, entry))
            .ToArray();

    private static ImportedScene LoadDonor(
        string donorPath,
        bool geometryOnly)
    {
        ImportedScene source = geometryOnly
            ? ImportedModelReader.ReadGeometryOnly(donorPath)
            : ImportedModelReader.Read(donorPath);
        string? directory = Path.GetDirectoryName(donorPath);
        if (directory is null)
            return source;
        ImportedTexture[] adjacentTextures = Directory
            .EnumerateFiles(directory)
            .Where(path => Path.GetExtension(path).ToLowerInvariant() is
                ".png" or ".jpg" or ".jpeg" or ".bmp" or ".tga")
            .Order(StringComparer.OrdinalIgnoreCase)
            .Select(ImportedTextureFileReader.Read)
            .ToArray();
        return adjacentTextures.Length == 0
            ? source
            : ImportedTextureCatalog.ResolveExternalOverrides(
                source,
                adjacentTextures).EffectiveScene;
    }

    private static string RequireFile(
        string argument,
        string? extension,
        string role)
    {
        string path = Path.GetFullPath(argument);
        if (!File.Exists(path))
            throw new FileNotFoundException($"{role} file does not exist", path);
        if (extension is not null &&
            !Path.GetExtension(path).Equals(
                extension, StringComparison.OrdinalIgnoreCase))
        {
            throw new ArgumentException($"{role} must be a {extension} file.");
        }
        return path;
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }

    private sealed record PairResult(
        int MeshCount,
        int VertexCount,
        int TriangleCount,
        string Details);
}
