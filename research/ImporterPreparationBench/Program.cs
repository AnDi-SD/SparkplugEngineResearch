using System.Diagnostics;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

if (args.Length != 6)
    throw new ArgumentException("TARGET_SMO DONOR_MODEL NEW_OUTPUT_DIR ITERATIONS_1_TO_7 gui|back daphne|coarse");
string targetPath = Path.GetFullPath(args[0]), donorPath = Path.GetFullPath(args[1]);
string directory = Path.GetFullPath(args[2]);
int iterations = int.Parse(args[3]);
if (iterations is < 1 or > 7 || args[4] is not ("gui" or "back") || args[5] is not ("daphne" or "coarse"))
    throw new ArgumentException("Invalid bounded run settings");
if (Directory.Exists(directory)) throw new InvalidOperationException("Use a new output directory");
Directory.CreateDirectory(directory);
var options = new JsonSerializerOptions { WriteIndented = true, IncludeFields = true };
try
{
    string sourceHash = Hash(donorPath), targetHash = Hash(targetPath);
    var target = SmoDocument.Load(targetPath);
    var rawDonor = ImportedModelReader.ReadGeometryOnly(donorPath);
    var textures = rawDonor.Materials.Select(m => m.BaseColorTextureName)
        .Where(n => !string.IsNullOrWhiteSpace(n))
        .Select(n => Path.Combine(Path.GetDirectoryName(donorPath)!, Path.GetFileName(n!)))
        .Distinct(StringComparer.OrdinalIgnoreCase).Where(File.Exists)
        .Select(ImportedTextureFileReader.Read).ToArray();
    var donor = ImportedTextureCatalog.ResolveExternalOverrides(rawDonor, textures).EffectiveScene;
    var rig = TargetRigDefinition.FromSmoDocument(target);
    var scene = SmoSceneBuilder.Build(target, new SmoExportOptions(
        ApplyWorldTransforms: true, AnimationPaths: null,
        Resources: SmoExportResourceTypes.Meshes | SmoExportResourceTypes.Skeleton));
    var alignment = args[5] == "daphne"
        ? new ReplacementTransform(84.5f, Vector3.Zero, new Vector3(0, 12, -3))
        : ReplacementTransformFitter.FitByHeightAndCenter(
            scene.Meshes.SelectMany(mesh => mesh.Positions),
            donor.Meshes.SelectMany(mesh => mesh.Positions));
    var fitWatch = Stopwatch.StartNew();
    TargetRigFittingPoseSnapshot pose;
    TargetRigBodySelection body;
    if (args[5] == "daphne")
    {
        var fit = TargetRigAutomaticPoseFitter.Fit(rig, scene, donor, alignment);
        pose = fit.Pose;
        body = fit.BodySelection;
    }
    else
    {
        pose = rig.CreateFittingPose().Capture();
        body = TargetRigAutomaticPoseFitter.SelectBody(rig, donor, alignment);
    }
    double fitMs = fitWatch.Elapsed.TotalMilliseconds;
    var rows = new List<object>();
    var stageRows = new List<object>();
    using var process = Process.GetCurrentProcess();
    string? expected = null;
    GeneratedSkinningPreparationResult? prepared = null;
    for (int round = -1; round < iterations; round++)
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(45));
        long allocated = GC.GetTotalAllocatedBytes(true);
        TimeSpan cpu = process.TotalProcessorTime;
        var watch = Stopwatch.StartNew();
        var progress = new SynchronousProgress(value => stageRows.Add(new
        {
            round, milliseconds = watch.Elapsed.TotalMilliseconds,
            allocatedBytes = GC.GetTotalAllocatedBytes(false) - allocated,
            value.Fraction, value.Stage
        }));
        prepared = GeneratedSkinningPreparer.PrepareCancellable(target, donor, pose,
            alignment, body, componentOverrides: null, regionOverrides: null,
            timeout.Token, progress, enableAutomaticBackExtraction: args[4] == "back");
        double prepareMs = watch.Elapsed.TotalMilliseconds;
        double prepareCpuMs = (process.TotalProcessorTime - cpu).TotalMilliseconds;
        long prepareBytes = GC.GetTotalAllocatedBytes(true) - allocated;
        string fingerprint = Convert.ToHexString(SHA256.HashData(JsonSerializer.SerializeToUtf8Bytes(prepared, options)));
        expected ??= fingerprint;
        if (fingerprint != expected) throw new InvalidDataException("Preparation changed between identical inputs");
        rows.Add(new { round, warmup = round < 0, prepareMs, prepareCpuMs, prepareBytes, fingerprint,
            prepared.Analysis.MaximumInfluencesPerVertex, prepared.Analysis.PreparedVertexCount,
            attachments = prepared.Analysis.Attachments.Count });
        Console.WriteLine($"{round}: prepare {prepareMs:F2} ms, CPU {prepareCpuMs:F2} ms, allocated {prepareBytes}");
    }
    var plan = SmoSkinnedGlbReplacer.Analyze(target, prepared!.PreparedScene);
    if (!plan.CanReplace) throw new InvalidDataException(string.Join(" | ", plan.Messages));
    var output = SmoSkinnedGlbReplacer.Replace(target, prepared.PreparedScene,
        ReplacementTransform.Identity, Path.Combine(directory, "prepared.smo"),
        SkinnedGeometryTransferMode.PreservePreparedGeometry);
    if (Hash(targetPath) != targetHash || Hash(donorPath) != sourceHash)
        throw new InvalidDataException("Input file changed");
    File.WriteAllText(Path.Combine(directory, "report.json"), JsonSerializer.Serialize(new
    {
        kind = "importer-preparation-benchmark", status = "passed", mode = args[4],
        targetPath, targetSha256 = targetHash, donorPath, donorSha256 = sourceHash,
        alignmentMode = args[5], alignment, poseSource = args[5] == "daphne" ? "automatic-fit" : "identity-pose",
        iterations, warmups = 1, fitMs, rows, stageRows,
        output = output.OutputPath, outputSha256 = output.Sha256,
        analysis = prepared.Analysis, peakWorkingSet = process.PeakWorkingSet64,
        coreAssemblySha256 = Hash(typeof(GeneratedSkinningPreparer).Assembly.Location),
        benchmarkAssemblySha256 = Hash(typeof(Program).Assembly.Location),
        scope = "Actual automatic weighting with an explicitly recorded fixed pose/alignment; gui mode disables automatic back extraction like the GUI consumer. Not an artistic deformation-quality or native-game-render claim."
    }, options));
    return 0;
}
catch (Exception error)
{
    File.WriteAllText(Path.Combine(directory, "failure.txt"), error.ToString());
    Console.Error.WriteLine(error);
    return 1;
}

static string Hash(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));

sealed class SynchronousProgress(Action<GeneratedSkinningProgress> action) : IProgress<GeneratedSkinningProgress>
{
    public void Report(GeneratedSkinningProgress value) => action(value);
}
