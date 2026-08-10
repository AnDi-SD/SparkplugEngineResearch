using System.Security.Cryptography;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

if (args.Length == 5 && args[0] == "--fixed-texture-probe")
{
 byte[] source = File.ReadAllBytes(args[1]);
 ImportedScene scene = GlbModelReader.Read(args[2]);
 int imageIndex = int.Parse(args[3], System.Globalization.CultureInfo.InvariantCulture);
 if ((uint)imageIndex >= (uint)scene.Textures.Count)
  throw new ArgumentOutOfRangeException(nameof(imageIndex), $"GLB has {scene.Textures.Count} embedded base-color textures.");
 byte[] output = FixedSizeTextureWriter.ReplaceRgb(source, 1, scene.Textures[imageIndex].Data);
 File.WriteAllBytes(args[4], output);
 SMOTextureTool.Core.SmoDocument before = SMOTextureTool.Core.SmoDocument.Parse(source);
 SMOTextureTool.Core.SmoDocument after = SMOTextureTool.Core.SmoDocument.Parse(output);
 SMOTextureTool.Core.TextureInfo target = before.Textures[0];
 int outside = source.Select((value, index) => (value, index))
  .Count(item => item.index < target.PixelDataOffset || item.index >= target.PixelDataOffset + target.PixelDataSize
   ? item.value != output[item.index] : false);
 if (source.Length != output.Length || outside != 0)
  throw new InvalidOperationException("Fixed texture probe changed file structure or bytes outside pixel data.");
 if (after.Textures[0].Material is null)
  throw new InvalidOperationException("Fixed texture probe lost the material owner.");
 Console.WriteLine($"FIXED TEXTURE PROBE: {target.Width}x{target.Height}, same length, 0 changes outside pixels, alpha preserved; output={Path.GetFullPath(args[4])}");
 return 0;
}

if (args.Length == 4 && args[0] == "--raw-texture-probe")
{
 byte[] probe = File.ReadAllBytes(args[1]);
 SMOTextureTool.Core.SmoDocument probeDocument = SMOTextureTool.Core.SmoDocument.Parse(probe);
 if (probeDocument.Textures.Count == 0) throw new InvalidOperationException("Texture probe source has no textures.");
 SMOTextureTool.Core.TextureInfo texture = probeDocument.Textures[0];
 int relativeOffset = args[2] switch
 {
  "first-blue" => 1,
  "center-blue" => checked(((texture.Height / 2) * texture.Width + texture.Width / 2) * 4 + 1),
  _ => throw new ArgumentException("Probe channel must be first-blue or center-blue.")
 };
 int changedOffset = checked(texture.PixelDataOffset + relativeOffset);
 probe[changedOffset] ^= 1;
 File.WriteAllBytes(args[3], probe);
 byte[] verified = File.ReadAllBytes(args[3]);
 int differences = verified.Zip(File.ReadAllBytes(args[1])).Count(pair => pair.First != pair.Second);
 if (differences != 1) throw new InvalidOperationException($"Raw probe changed {differences} bytes instead of one.");
 _ = SmoDocument.Load(args[3]);
 _ = SMOTextureTool.Core.SmoDocument.Load(args[3]);
 Console.WriteLine($"RAW TEXTURE PROBE: changed exactly one RGB byte at 0x{changedOffset:X}; output={Path.GetFullPath(args[3])}");
 return 0;
}

if (args.Length is not 1 and not 3 and not 4 || !File.Exists(args[0]) ||
    (args.Length >= 3 && !File.Exists(args[1])) || (args.Length == 4 && !File.Exists(args[3])))
{
 Console.Error.WriteLine("Usage: SmoImporter.FormatTests <sample.smo> [replacement.glb output.smo [texture.png]]");
 return 2;
}
int checks = 0;
void Check(bool value, string message) { checks++; if (!value) throw new InvalidOperationException("FAIL: " + message); }
string temp = Path.Combine(Path.GetTempPath(), "smo-import-tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temp);
try
{
 byte[] originalBytes = File.ReadAllBytes(args[0]);
 string originalHash = Convert.ToHexString(SHA256.HashData(originalBytes));
 SmoDocument sourceDocument = SmoDocument.Load(args[0]);
 SMOTextureTool.Core.SmoDocument sourceTextures = SMOTextureTool.Core.SmoDocument.Load(args[0]);
 Console.WriteLine("SOURCE TEXTURES: " + string.Join("; ", sourceTextures.Textures.Select(texture =>
  $"#{texture.Index} {texture.Width}x{texture.Height} fmt=0x{texture.FormatCode:X4} layout={texture.Layout} " +
  $"owner={(texture.Material is null ? "none" : $"material#{texture.Material.Index}/pass{texture.Material.PassIndex}/layer{texture.Material.LayerIndex}")}")));
 SmoExportScene exported = SmoSceneBuilder.Build(sourceDocument);
 Check(exported.Meshes.Count > 0, "exported mesh exists");
 string glb = Path.Combine(temp, "source.glb");
 GlbExporter.Export(exported, glb);
 ImportedScene imported = GlbModelReader.Read(glb);
 Check(imported.Meshes.Count == exported.Meshes.Count, "GLB mesh count round-trip");
 MeshSplitPolicy splitPolicy = new(1000, 1500, 500, 1);
 MeshSplitPlan split = MeshSplitter.Split(imported, splitPolicy);
 Check(split.Chunks.Count > 1, "whole scene is split into multiple chunks");
 Check(split.Chunks.All(chunk => chunk.Mesh.Positions.Length <= splitPolicy.MaxVerticesPerChunk &&
  chunk.Mesh.TriangleIndices.Length <= splitPolicy.MaxIndicesPerChunk &&
  chunk.SourceTriangleCount <= splitPolicy.MaxTrianglesPerChunk), "every chunk respects limits");
 Check(split.Chunks.Sum(chunk => chunk.SourceTriangleCount) == split.SourceTriangleCount, "all triangles are preserved");
 MeshSplitPlan repeatedSplit = MeshSplitter.Split(imported, splitPolicy);
 Check(split.Chunks.SelectMany(chunk => chunk.Mesh.TriangleIndices).SequenceEqual(
  repeatedSplit.Chunks.SelectMany(chunk => chunk.Mesh.TriangleIndices)), "split is deterministic");
 SmoExportMesh exportMesh = exported.Meshes[0];
 SmoObjectEntry entry = sourceDocument.Objects[exportMesh.ObjectIndex];
 SmoMesh before = SmoMeshDecoder.Decode(sourceDocument, entry);
 IReadOnlyList<SmoBoneSlot> slots = SmoMeshReplacer.GetBoneSlots(sourceDocument, entry);
 string output = Path.Combine(temp, "replaced.smo");
 ReplacementResult result = SmoMeshReplacer.Replace(
  sourceDocument, entry, imported.Meshes[0], ReplacementTransform.Identity,
  slots.Count > 0 ? slots[0].Slot : 0, output);
 Check(new FileInfo(output).Length == originalBytes.Length, "in-place output length");
 Check(Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[0]))) == originalHash, "source remains unchanged");
 SmoDocument afterDocument = SmoDocument.Load(output);
 SmoMesh after = SmoMeshDecoder.Decode(afterDocument, afterDocument.Objects[entry.Index]);
 Check(after.VertexCount == before.VertexCount, "vertex count preserved");
 Check(after.StripIndices.SequenceEqual(before.StripIndices), "original strip preserved");
 Check(after.Positions.Zip(before.Positions).All(pair => System.Numerics.Vector3.Distance(pair.First, pair.Second) < 0.0001f), "identity geometry round-trip");
 if (after.HasSkinningData)
  Check(after.BlendWeights.All(weight => weight.X == 1f && weight.Y == 0f && weight.Z == 0f && weight.W == 0f), "rigid weights written");
 if (args.Length >= 3)
 {
  ImportedScene wholeScene = ImportedModelReader.Read(args[1]);
  Check(wholeScene.Textures.Count > 0, "GLB embedded base-color texture is discovered");
  IReadOnlyList<SmoBoneSlot> rigidChoices = SmoWholeModelReplacer.GetRigidBoneChoices(sourceDocument);
  Check(rigidChoices.Count > 0, "whole replacement exposes rigid bone choices");
  int chosenRigidSlot = rigidChoices[0].Slot;
  WholeModelReplacementResult whole = SmoWholeModelReplacer.Replace(
   sourceDocument, wholeScene, ReplacementTransform.Identity, args[2], chosenRigidSlot,
   args.Length == 4 ? args[3] : null,
   args.Length == 3 ? wholeScene.Textures[0] : null);
  SmoDocument wholeDocument = SmoDocument.Load(args[2]);
  SmoMesh[] wholeMeshes = wholeDocument.Objects.Where(item => item.TypeHash == SmoClassIds.MeshData)
   .Select(item => SmoMeshDecoder.Decode(wholeDocument, item)).ToArray();
  int importedTriangles = wholeScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
  Check(wholeMeshes.Length == exported.Meshes.Count, "whole replacement keeps mesh slot count");
  Check(wholeMeshes.Max(mesh => mesh.TriangleCount) == importedTriangles, "whole replacement preserves every visible triangle in one slot");
  Check(wholeMeshes.All(mesh => mesh.PrimitiveType == SmoMeshDecoder.TriangleListPrimitive), "whole replacement uses triangle lists");
  Check(wholeMeshes.Where(mesh => mesh.TriangleCount > 0).All(mesh =>
   mesh.HasSkinningData && mesh.BlendWeights.All(weight => weight.X == 1f)), "visible whole replacement is rigid-skinned");
  Check(wholeMeshes.Single(mesh => mesh.TriangleCount == importedTriangles).BlendIndices.All(
   indices => indices.X == chosenRigidSlot), "selected rigid bone slot is written to every visible vertex");
  Check(wholeMeshes.Count(mesh => mesh.TriangleCount == importedTriangles) == 1, "whole replacement uses one visible rigid mesh slot");
  Check(wholeMeshes.Where(mesh => mesh.TriangleCount != importedTriangles).All(mesh =>
   mesh.Stride == 56 && mesh.VertexCount == 1 && mesh.TriangleIndices.SequenceEqual(new uint[] { 0, 0, 0 })),
   "disabled slots keep a valid degenerate primitive");
  Check(!wholeDocument.HasErrors, "whole replacement passes strict container validation");
  if (args.Length >= 3)
  {
   SMOTextureTool.Core.SmoDocument textured = SMOTextureTool.Core.SmoDocument.Load(args[2]);
   Check(textured.Textures.Count > 0, "textured replacement keeps texture catalog");
   Check(textured.Textures[0].Material is not null, "primary texture keeps its material owner");
   Console.WriteLine("TEXTURES: " + string.Join("; ", textured.Textures.Select(texture =>
    $"#{texture.Index} {texture.Width}x{texture.Height} fmt=0x{texture.FormatCode:X4} layout={texture.Layout} " +
    $"owner={(texture.Material is null ? "none" : $"material#{texture.Material.Index}/pass{texture.Material.PassIndex}/layer{texture.Material.LayerIndex}")}")));
  }
  Console.WriteLine($"WHOLE: meshes={whole.MeshCount}; vertices={whole.VertexCount}; triangles={whole.TriangleCount}; bytes={whole.FileSize}; file={whole.OutputPath}");
 }
 Console.WriteLine($"PASS: {checks} assertions; mesh={entry.Name}; output={result.VertexCount} vertices");
 return 0;
}
finally { Directory.Delete(temp, true); }
