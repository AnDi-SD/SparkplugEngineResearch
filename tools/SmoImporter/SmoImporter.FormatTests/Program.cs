using SmoImporter.Core;

if (args is ["--reserved-header-authoring", string reservedHeaderOutput])
{
    try { return ReservedHeaderWriterRegression.Run(reservedHeaderOutput); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args is ["--skin-palette-authoring", string skinPaletteSource, string skinPaletteOutput])
{
    try { return SkinPaletteWriterRegression.Run(skinPaletteSource, skinPaletteOutput); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args is ["--renderable-scalar-authoring", string renderableOutput])
{
    try { return RenderableScalarWriterRegression.Run(renderableOutput); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args is ["--reference-range-benchmark", string rangeSource, string rangeOutput])
{
    try { return ReferenceRangeBatchBenchmark.Run(rangeSource, rangeOutput); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args is ["--material-scalar-authoring", string materialTemplate, string materialOutput])
{
    try { return MaterialScalarWriterRegression.Run(materialTemplate, materialOutput); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args is ["--forest-reference-remap", string remapTemplate, string remapOutput])
{
    try { return ForestReferenceRemapRegression.Run(remapTemplate, remapOutput); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args is ["--model-graph-links", string graphTemplate, string graphOutput])
{
    try { return ModelGraphLinkRegression.Run(graphTemplate, graphOutput); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args is ["--smo-occurrences", string occurrenceSource, string layeredSource, string occurrenceOutput])
    return SmoOccurrenceImportRegression.Run(occurrenceSource, layeredSource, occurrenceOutput);

if (args.Length >= 3 && args[0] == "--collision-writer-regression")
{
    try { CollisionWriterRegression.Run(args[1], args.Skip(2).ToArray()); return 0; }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length >= 3 && args[0] == "--mesh-writer-regression")
{
    try { MeshWriterRegression.Run(args[1], args.Skip(2).ToArray()); return 0; }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length == 3 && args[0] == "--generated-cancellation-regression")
{
    try { GeneratedResourceSafetyRegression.RunCancellation(args[1], args[2]); return 0; }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length == 4 && args[0] == "--grouped-preparation-control")
{
    try { return GroupedPreparationControl.Run(args[1], args[2], args[3]); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length == 2 && args[0] == "--palette-selection-trace")
{
    try { return PaletteSelectionTrace.Run(args[1]); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length == 1 && args[0] == "--texture-pixel-comparison-regression")
{
    try { return TexturePixelComparisonRegression.Run(); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length >= 3 && args[0] == "--bulk-visual-removal-regression")
{
    try { return BulkVisualRemovalRegression.Run(args.Skip(1).ToArray()); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length >= 4 && args[0] == "--clean-skinned-pose-input")
{
    try { return CleanSkinnedPoseRegression.Prepare(args.Skip(1).ToArray()); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length >= 3 && args[0] == "--clean-skinned-target-regression")
{
    try { return CleanSkinnedTargetRegression.Run(args.Skip(1).ToArray()); }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length is 3 or 4 or 5 && args[0] == "--texture-static-integration")
    return TextureStaticIntegrationRegression.Run(args[1],args[2],args.Length>=4?int.Parse(args[3]):1,args.Length==5 && args[4]=="independent");

if (args.Length == 2 && args[0] == "--texture-forward-reference-guard")
    return TextureStaticIntegrationRegression.VerifyRejectedForwardReference(args[1]);

if (args.Length >= 3 && args[0] == "--texture-template-regression")
{
    try { TextureTemplateRegression.Run(args.Skip(1).ToArray()); return 0; }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length >= 3 && args[0] == "--texture-writer-regression")
{
    TextureWriterRegression.Run(args.Skip(1).ToArray());
    return 0;
}

if (args.Length == 1 && args[0] == "--glb-normal-repair-regression")
{
    GlbNormalRepairRegression.Run();
    Console.WriteLine("GLB NORMAL REPAIR REGRESSION PASS");
    return 0;
}

if (args.Length == 1 && args[0] == "--glb-resource-safety-regression")
{
    GlbResourceSafetyRegression.Run();
    Console.WriteLine("GLB RESOURCE SAFETY REGRESSION PASS");
    return 0;
}

if (args.Length == 1 && args[0] == "--alpha-component-regression")
{
    AlphaBranchRegression.RunConnectedComponentClassification();
    Console.WriteLine("ALPHA COMPONENT REGRESSION PASS");
    return 0;
}

if (args.Length == 1 && args[0] == "--rigid-level-preparation-regression")
{
    RigidLevelImportPreparationRegression.Run();
    Console.WriteLine("LEVEL IMPORT PREPARATION REGRESSION PASS");
    return 0;
}

if (args.Length == 2 && args[0] == "--static-model-replacement-regression")
{
    StaticModelReplacementRegression.Run(args[1]);
    Console.WriteLine("STATIC MODEL REPLACEMENT REGRESSION PASS");
    return 0;
}

if (args.Length == 2 && args[0] == "--static-multi-template-regression")
{
    StaticModelReplacementRegression.RunSharedTextureTarget(args[1]);
    Console.WriteLine("STATIC MULTI-TEMPLATE REGRESSION PASS");
    return 0;
}

if (args.Length == 4 && args[0] == "--porting-mode-pair")
{
    PortingModePairRegression.Run(args[1], args[2], args[3]);
    return 0;
}

if (args.Length == 5 && args[0] == "--porting-mode-pair")
{
    PortingModePairRegression.Run(args[1], args[2], args[3], args[4]);
    return 0;
}

if (args.Length == 3 && args[0] == "--generated-weight-audit")
{
    GeneratedSkinningWeightAudit.Run(args[1], args[2]);
    return 0;
}

if (args.Length == 3 && args[0] == "--component-exception-audit")
{
    GeneratedSkinningWeightAudit.Run(
        args[1],
        args[2],
        verifyManualException: true);
    return 0;
}

if (args.Length == 1 && args[0] == "--generated-quality-guard-regression")
{
    GeneratedSkinningQualityGuardRegression.Run();
    return 0;
}

if (args.Length == 4 && args[0] == "--generated-weight-audit-sequence")
{
    GeneratedSkinningWeightAudit.Run(args[1], args[2]);
    GeneratedSkinningWeightAudit.Run(args[1], args[3]);
    return 0;
}

if (args.Length == 5 && args[0] == "--porting-mode-write")
{
    PortingModePairRegression.Run(args[1], args[2], args[3], args[4]);
    return 0;
}

if (args.Length == 2 && args[0] == "--model-import-smoke")
{
    ImportedScene scene = ImportedModelReader.Read(args[1]);
    Console.WriteLine(
        $"MODEL IMPORT PASS: meshes={scene.Meshes.Count}; " +
        $"vertices={scene.Meshes.Sum(mesh => mesh.Positions.Length)}; " +
        $"triangles={scene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3)}; " +
        $"materials={scene.Materials.Count}; textures={scene.Textures.Count}; " +
        $"skinned={scene.HasSkinning}");
    return 0;
}

if (args.Length == 3 && args[0] == "--native-smo-visual-replacement")
{
    SmoNativeVisualReplacementRegression.Run(args[1], args[2]);
    Console.WriteLine("NATIVE SMO VISUAL REPLACEMENT PASS");
    return 0;
}

if (args.Length == 4 && args[0] == "--native-smo-visual-write")
{
    SmoNativeVisualGraphReplaceResult result =
        SmoNativeVisualGraphReplacer.Replace(
            SmoViewer.Core.SmoDocument.Load(Path.GetFullPath(args[1])),
            SmoViewer.Core.SmoDocument.Load(Path.GetFullPath(args[2])),
            Path.GetFullPath(args[3]));
    Console.WriteLine(
        $"NATIVE SMO VISUAL WRITE PASS: {result.OutputPath}; " +
        $"meshes={result.MeshCount}; textures={result.TextureCount}; " +
        $"sha256={result.Sha256}");
    return 0;
}

if (args.Length == 2 && args[0] == "--model-mesh-bounds")
{
    ImportedScene scene = ImportedModelReader.Read(args[1]);
    foreach ((ImportedMesh mesh, int index) in scene.Meshes.Select((mesh, index) => (mesh, index)))
    {
        if (mesh.Positions.Length == 0)
            continue;
        var minimum = new System.Numerics.Vector3(
            mesh.Positions.Min(value => value.X),
            mesh.Positions.Min(value => value.Y),
            mesh.Positions.Min(value => value.Z));
        var maximum = new System.Numerics.Vector3(
            mesh.Positions.Max(value => value.X),
            mesh.Positions.Max(value => value.Y),
            mesh.Positions.Max(value => value.Z));
        Console.WriteLine(
            $"[{index}] {mesh.Name}: vertices={mesh.Positions.Length}; " +
            $"triangles={mesh.TriangleIndices.Length / 3}; min={minimum}; max={maximum}");
    }
    return 0;
}

if (args.Length == 2 && args[0] == "--native-fbx-roundtrip")
{
    NativeFbxRoundTripRegression.Run(args[1]);
    return 0;
}

if (args.Length == 2 && args[0] == "--auto-alpha-fbx-regression")
{
    AutoAlphaTextureRegression.Run(args[1]);
    return 0;
}

if (args.Length == 3 && args[0] == "--generated-skinning-degenerate-fbx-regression")
{
    DegenerateFbxGeneratedSkinningRegression.Run(args[1], args[2]);
    return 0;
}

if (args.Length == 2 && args[0] == "--generated-topology-normalization-regression")
{
    GeneratedTopologyNormalizationRegression.Run(args[1]);
    return 0;
}

if (args.Length == 2 && args[0] == "--target-fitting-preview-regression")
{
    TargetRigFittingPreviewRegression.Run(args[1]);
    return 0;
}

if (args.Length == 2 && args[0] == "--semantic-region-regression")
{
    GeneratedSkinningSemanticRegionRegression.RunSynthetic(args[1]);
    return 0;
}

if (args.Length == 2 && args[0] == "--semantic-head-topology-regression")
{
    try { GeneratedSkinningSemanticRegionRegression.RunBoundedHeadTopology(args[1]); return 0; }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length == 2 && args[0] == "--semantic-hand-topology-regression")
{
    try { GeneratedSkinningSemanticHandRegression.Run(args[1]); return 0; }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length == 3 && args[0] == "--daphne-semantic-coverage-audit")
{
    try { DaphneSemanticCoverageAudit.Run(args[1], args[2]); return 0; }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length == 8 && args[0] == "--semantic-head-donor-audit")
{
    GeneratedSkinningSemanticRegionRegression.RunHeadDonorAudit(
        args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
    return 0;
}

if (args.Length is 4 or 5 && args[0] == "--daphne-semantic-region-regression")
{
    GeneratedSkinningSemanticRegionRegression.RunDaphne(
        args[1], args[2], args[3], args.Length == 5 ? args[4] : null);
    return 0;
}

if (args.Length == 2 && args[0] == "--generated-resource-safety-regression")
{
    try { GeneratedResourceSafetyRegression.Run(args[1]); return 0; }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length == 4 && args[0] == "--daphne-mode3-regression")
{
    try { DaphneMode3Regression.Run(args[1], args[2], args[3]); return 0; }
    catch (Exception error) { Console.Error.WriteLine(error); return 1; }
}

if (args.Length == 4 && args[0] == "--material-group-native-integration")
{
    MaterialGroupMatchingRegression.RunNativeBranchIntegration(
        args[1], args[2], args[3]);
    Console.WriteLine("MATERIAL GROUP NATIVE BRANCH INTEGRATION PASS");
    return 0;
}

Console.Error.WriteLine(
    "Unknown or incomplete regression command.");
return 2;
