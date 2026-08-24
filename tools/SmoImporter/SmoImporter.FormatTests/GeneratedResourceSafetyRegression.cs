using System.Diagnostics;
using System.Numerics;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class GeneratedResourceSafetyRegression
{
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
