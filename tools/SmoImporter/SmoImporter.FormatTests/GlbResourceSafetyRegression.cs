using System.Diagnostics;
using System.Text;
using System.Text.Json;
using SmoImporter.Core;

internal static class GlbResourceSafetyRegression
{
 private const uint JsonChunkType = 0x4E4F534A;
 private const uint BinaryChunkType = 0x004E4942;

 public static void Run()
 {
  string directory = Path.Combine(
   Path.GetTempPath(),
   $"SmoImporter-GlbSafety-{Guid.NewGuid():N}");
  Directory.CreateDirectory(directory);

  try
  {
   string cyclicPath = Path.Combine(directory, "cyclic-nodes.glb");
   WriteGlb(cyclicPath, CreateCyclicNodeDocument(), CreateTriangleBinary());
   ExpectFastRejection(cyclicPath, "cycle");

   string excessivePath = Path.Combine(directory, "excessive-accessor.glb");
   WriteGlb(excessivePath, CreateExcessiveAccessorDocument(), new byte[4]);
   ExpectFastRejection(excessivePath, "accessor");

   using var cancelled = new CancellationTokenSource();
   cancelled.Cancel();
   try
   {
    _ = GlbModelReader.ReadGeometryOnly(cyclicPath, cancelled.Token);
    throw new InvalidOperationException(
     "A pre-cancelled GLB read was not cancelled.");
   }
   catch (OperationCanceledException)
   {
   }

   VerifyCompressedTextureCatalogBudget();

   Console.WriteLine(
    "GLB RESOURCE SAFETY REGRESSION PASS: cyclic hierarchy and " +
    "oversized accessor were rejected without a hang; cancellation works; " +
    "large compressed texture catalogs remain bounded without a false rejection.");
  }
  finally
  {
   Directory.Delete(directory, recursive: true);
  }
 }

 private static void VerifyCompressedTextureCatalogBudget()
 {
  ImportedTexture[] mikuScaleCatalog = Enumerable.Range(0, 13)
   .Select(index => new ImportedTexture(
    $"texture-{index}",
    "image/png",
    index == 11 ? 1024 : 4096,
    index == 11 ? 1024 : 4096,
    [0]))
   .ToArray();
  ImportedModelResourceLimits.ValidateTextures(
   mikuScaleCatalog,
   "compressed-catalog regression");
  ImportedTextureMemoryEstimate estimate =
   ImportedTextureMemoryEstimator.Estimate(
    mikuScaleCatalog,
    3_972_844_749);
  const long expectedBaseRgbaBytes =
   12L * 4096 * 4096 * 4 + 1024L * 1024 * 4;
  if (estimate.TextureCount != 13 ||
      estimate.DecodedBaseRgbaBytes != expectedBaseRgbaBytes ||
      estimate.DecodedMipmappedRgbaBytes != 1_079_334_212 ||
      estimate.ResolutionGroups.Count != 2 ||
      estimate.ResolutionGroups[0] !=
       new ImportedTextureResolutionGroup(4096, 4096, 12) ||
      estimate.MipmappedBudgetFraction is < 0.27 or > 0.28)
  {
   throw new InvalidOperationException(
    "Large texture-catalog memory estimate is inaccurate.");
  }

  ImportedTexture[] excessiveCatalog = Enumerable.Range(0, 5)
   .Select(index => new ImportedTexture(
    $"oversized-{index}",
    "image/png",
    8192,
    8192,
    [0]))
   .ToArray();
  try
  {
   ImportedModelResourceLimits.ValidateTextures(
    excessiveCatalog,
    "excessive compressed-catalog regression");
   throw new InvalidOperationException(
    "A texture catalog above the sequential-processing budget was accepted.");
  }
  catch (InvalidDataException)
  {
  }
 }

 private static void ExpectFastRejection(string path, string diagnosticFragment)
 {
  Stopwatch stopwatch = Stopwatch.StartNew();
  Task<string?> read = Task.Run(() =>
  {
   try
   {
    _ = GlbModelReader.ReadGeometryOnly(path);
    return null;
   }
   catch (InvalidDataException exception)
   {
    return exception.Message;
   }
  });

  if (!read.Wait(TimeSpan.FromSeconds(3)))
   throw new InvalidOperationException(
    $"GLB safety validation hung for more than three seconds: {path}");

  string? message = read.Result;
  if (message is null)
   throw new InvalidOperationException(
    $"Unsafe GLB was accepted instead of being blocked: {path}");
  if (!message.Contains(diagnosticFragment, StringComparison.OrdinalIgnoreCase))
   throw new InvalidOperationException(
    $"GLB was rejected without the expected '{diagnosticFragment}' " +
    $"diagnostic. Actual message: {message}");
  if (stopwatch.Elapsed > TimeSpan.FromSeconds(3))
   throw new InvalidOperationException(
    $"GLB safety validation was unexpectedly slow: {stopwatch.Elapsed}.");
 }

 private static object CreateCyclicNodeDocument()
 {
  return new
  {
   asset = new { version = "2.0" },
   buffers = new[] { new { byteLength = 44 } },
   bufferViews = new object[]
   {
    new { buffer = 0, byteOffset = 0, byteLength = 36 },
    new { buffer = 0, byteOffset = 36, byteLength = 6 }
   },
   accessors = new object[]
   {
    new
    {
     bufferView = 0,
     componentType = 5126,
     count = 3,
     type = "VEC3",
     min = new[] { 0f, 0f, 0f },
     max = new[] { 1f, 1f, 0f }
    },
    new { bufferView = 1, componentType = 5123, count = 3, type = "SCALAR" }
   },
   meshes = new[]
   {
    new
    {
     primitives = new[]
     {
      new
      {
       attributes = new Dictionary<string, int> { ["POSITION"] = 0 },
       indices = 1
      }
     }
    }
   },
   nodes = new object[]
   {
    new { mesh = 0, children = new[] { 1 } },
    new { children = new[] { 0 } }
   },
   scenes = new[] { new { nodes = new[] { 0 } } },
   scene = 0
  };
 }

 private static object CreateExcessiveAccessorDocument()
 {
  return new
  {
   asset = new { version = "2.0" },
   buffers = new[] { new { byteLength = 4 } },
   bufferViews = new[] { new { buffer = 0, byteOffset = 0, byteLength = 4 } },
   accessors = new[]
   {
    new
    {
     bufferView = 0,
     componentType = 5126,
     count = 12_000_001,
     type = "SCALAR"
    }
   },
   meshes = new[]
   {
    new
    {
     primitives = new[]
     {
      new
      {
       attributes = new Dictionary<string, int> { ["POSITION"] = 0 }
      }
     }
    }
   },
   nodes = new[] { new { mesh = 0 } },
   scenes = new[] { new { nodes = new[] { 0 } } },
   scene = 0
  };
 }

 private static byte[] CreateTriangleBinary()
 {
  var binary = new byte[44];
  float[] positions = { 0f, 0f, 0f, 1f, 0f, 0f, 0f, 1f, 0f };
  Buffer.BlockCopy(positions, 0, binary, 0, 36);
  ushort[] indices = { 0, 1, 2 };
  Buffer.BlockCopy(indices, 0, binary, 36, 6);
  return binary;
 }

 private static void WriteGlb(string path, object document, byte[] binary)
 {
  byte[] json = JsonSerializer.SerializeToUtf8Bytes(document);
  int jsonLength = Align4(json.Length);
  int binaryLength = Align4(binary.Length);
  int totalLength = 12 + 8 + jsonLength + 8 + binaryLength;

  using var stream = File.Create(path);
  using var writer = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: false);
  writer.Write(0x46546C67u);
  writer.Write(2u);
  writer.Write((uint)totalLength);
  writer.Write((uint)jsonLength);
  writer.Write(JsonChunkType);
  writer.Write(json);
  for (int index = json.Length; index < jsonLength; index++)
   writer.Write((byte)' ');
  writer.Write((uint)binaryLength);
  writer.Write(BinaryChunkType);
  writer.Write(binary);
  for (int index = binary.Length; index < binaryLength; index++)
   writer.Write((byte)0);
 }

 private static int Align4(int value) => checked((value + 3) & ~3);
}
