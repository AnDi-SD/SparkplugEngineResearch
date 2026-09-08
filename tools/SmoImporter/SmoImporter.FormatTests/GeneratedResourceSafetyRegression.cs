using System.Diagnostics;
using System.Numerics;
using System.Security.Cryptography;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class GeneratedResourceSafetyRegression
{
 public static void RunCancellation(string targetPath, string donorPath)
 {
  var target = SmoDocument.Load(targetPath);
  var donor = ImportedModelReader.ReadGeometryOnly(donorPath);
  var rig = TargetRigDefinition.FromSmoDocument(target);
  var pose = rig.CreateFittingPose().Capture();
  var targetScene = SmoSceneBuilder.Build(target, new SmoExportOptions(
   ApplyWorldTransforms: true, AnimationPaths: null,
   Resources: SmoExportResourceTypes.Meshes | SmoExportResourceTypes.Skeleton));
  var alignment = ReplacementTransformFitter.FitByHeightAndCenter(
   targetScene.Meshes.SelectMany(m => m.Positions), donor.Meshes.SelectMany(m => m.Positions));
  var body = TargetRigAutomaticPoseFitter.SelectBody(rig, donor, alignment);
  byte[] targetBefore = SHA256.HashData(File.ReadAllBytes(targetPath));
  byte[] donorBefore = SHA256.HashData(File.ReadAllBytes(donorPath));
  foreach (double threshold in new[] {-1d, 0.01d, 0.88d})
  {
   using var cancellation = new CancellationTokenSource();
   if (threshold < 0) cancellation.Cancel();
   var progress = new CancellationProgress(cancellation, threshold);
   var watch = Stopwatch.StartNew();
   bool rejected = false;
   try
   {
    _ = GeneratedSkinningPreparer.PrepareCancellable(target, donor, pose, alignment, body,
     componentOverrides: null, regionOverrides: null, cancellation.Token, progress,
     enableAutomaticBackExtraction: false);
   }
   catch (OperationCanceledException) when (cancellation.IsCancellationRequested) { rejected = true; }
   if (!rejected || watch.Elapsed > TimeSpan.FromSeconds(3))
    throw new InvalidOperationException($"Cancellation threshold {threshold} did not stop promptly.");
  }
  if (!SHA256.HashData(File.ReadAllBytes(targetPath)).AsSpan().SequenceEqual(targetBefore) ||
      !SHA256.HashData(File.ReadAllBytes(donorPath)).AsSpan().SequenceEqual(donorBefore))
   throw new InvalidOperationException("Cancellation changed an input file.");
  Console.WriteLine("GENERATED CANCELLATION PASS: before start, initial progress, before final analysis; inputs unchanged.");
 }

 private sealed class CancellationProgress(CancellationTokenSource source, double threshold)
  : IProgress<GeneratedSkinningProgress>
 {
  public void Report(GeneratedSkinningProgress value)
  {
   if (value.Fraction >= threshold) source.Cancel();
  }
 }

 public static void Run(string targetPath)
 {
  SmoDocument target = SmoDocument.Load(Path.GetFullPath(targetPath));
  const int vertexCount = 200_001;
  var donor = new ImportedScene(
   new[]
   {
    new ImportedMesh(
     "over-budget",
     new Vector3[vertexCount],
     new Vector3[vertexCount],
     new Vector2[vertexCount],
     Array.Empty<uint>())
   });

  Stopwatch stopwatch = Stopwatch.StartNew();
  try
  {
   _ = GeneratedSkinningPreparer.Prepare(target, donor);
   throw new InvalidOperationException(
    "Over-budget generated skinning was not blocked.");
  }
  catch (InvalidDataException exception) when (
   exception.Message.Contains("safety budget", StringComparison.OrdinalIgnoreCase))
  {
  }

  if (stopwatch.Elapsed > TimeSpan.FromSeconds(2))
   throw new InvalidOperationException(
    $"Generated-skinning budget was checked too late: {stopwatch.Elapsed}.");
  Console.WriteLine(
   "GENERATED RESOURCE SAFETY REGRESSION PASS: oversized donor was " +
   "blocked before topology and weight calculation.");
 }
}
