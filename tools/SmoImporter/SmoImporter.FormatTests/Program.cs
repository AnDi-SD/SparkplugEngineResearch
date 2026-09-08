using SmoImporter.Core;

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
    GeneratedSkinningSemanticRegionRegression.RunBoundedHeadTopology(args[1]);
    return 0;
}

if (args.Length == 2 && args[0] == "--semantic-hand-topology-regression")
{
    GeneratedSkinningSemanticHandRegression.Run(args[1]);
    return 0;
}

if (args.Length == 3 && args[0] == "--daphne-semantic-coverage-audit")
{
    DaphneSemanticCoverageAudit.Run(args[1], args[2]);
    return 0;
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
    GeneratedResourceSafetyRegression.Run(args[1]);
    return 0;
}

if (args.Length == 4 && args[0] == "--daphne-mode3-regression")
{
    DaphneMode3Regression.Run(args[1], args[2], args[3]);
    return 0;
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
