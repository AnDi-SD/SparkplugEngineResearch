using System.Security.Cryptography;
using System.Numerics;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

if (args.Length == 2 && args[0] == "--scan-object-references")
{
 SmoDocument document = SmoDocument.Load(args[1]);
 Dictionary<uint, SmoObjectEntry> byId = document.Objects
  .GroupBy(entry => entry.Id).Where(group => group.Count() == 1)
  .ToDictionary(group => group.Key, group => group.Single());
 foreach (SmoObjectEntry owner in document.Objects)
 {
  ReadOnlySpan<byte> bytes = document.Data.Span.Slice(
   checked((int)owner.PhysicalOffset), checked((int)owner.SerializedSize));
  for (int offset = 8; offset <= bytes.Length - 8; offset++)
  {
   uint id = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(bytes[offset..]);
   uint inlineSize = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(bytes[(offset + 4)..]);
   if (!byId.TryGetValue(id, out SmoObjectEntry? referenced) ||
       inlineSize != 0 && inlineSize != referenced.SerializedSize)
    continue;
   bool physicalPrefix = owner.PhysicalOffset + offset + 8 == referenced.PhysicalOffset;
   Console.WriteLine(
    $"[{owner.Index}] {owner.Name} +0x{offset:X}: id={id} -> " +
    $"[{referenced.Index}] {referenced.Name}; inline=0x{inlineSize:X}; " +
    $"prefix={physicalPrefix}");
   offset += 7;
  }
 }
 return 0;
}

if (args.Length == 2 && args[0] == "--dump-tree")
{
 SmoDocument document = SmoDocument.Load(args[1]);
 foreach (SmoObjectEntry entry in document.Objects)
  Console.WriteLine(
   $"[{entry.Index,3}] depth={entry.NestingDepth} parent={entry.ParentIndex?.ToString() ?? "-",3} " +
   $"id={entry.Id,3} off=0x{entry.LogicalOffset:X6} size=0x{entry.SerializedSize:X6} " +
   $"{entry.ClassName ?? $"0x{entry.TypeHash:X8}"} {entry.Name}");
 foreach (SmoObjectEntry entry in document.Objects.Where(item => item.TypeHash == SmoClassIds.Skin))
  if (SmoSkinDecoder.TryDecode(document, entry, out SmoSkin? skin, out _) && skin is not null)
  {
   Console.WriteLine($"SKIN [{entry.Index}] " + string.Join(", ", skin.Bones.Select(bone =>
    $"{bone.PaletteIndex}:{document.Objects[bone.NodeObjectIndex].Name}/inline=0x{bone.InlineSerializedSize:X}")));
   SmoObjectEntry? meshEntry = document.Objects.FirstOrDefault(item =>
    item.ParentIndex == entry.Index && item.TypeHash == SmoClassIds.MeshData);
   if (meshEntry is not null)
   {
    SmoMesh mesh = SmoMeshDecoder.Decode(document, meshEntry);
    int[] used = Enumerable.Range(0, mesh.VertexCount).SelectMany(index =>
    {
     System.Numerics.Vector4 weights = mesh.BlendWeights[index];
     SmoBlendIndices indices = mesh.BlendIndices[index];
     return new[] { (weights.X, (int)indices.X), (weights.Y, (int)indices.Y),
      (weights.Z, (int)indices.Z), (weights.W, (int)indices.W) };
    }).Where(item => item.Item1 > 0.000001f).Select(item => item.Item2).Distinct().Order().ToArray();
    Console.WriteLine($"USED [{entry.Index}] " + string.Join(", ", used.Select(slot =>
     $"{slot}:{document.Objects[skin.Bones[slot].NodeObjectIndex].Name}")));
   }
  }
 return 0;
}

if (args.Length == 4 && args[0] == "--smo-to-smo")
{
 if (!File.Exists(args[1]) || !File.Exists(args[2]))
  throw new FileNotFoundException("Target or donor SMO was not found.");
 string targetHash = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[1])));
 string donorHash = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[2])));
 SmoDocument target = SmoDocument.Load(args[1]);
 SmoDocument donor = SmoDocument.Load(args[2]);
 SmoToSmoReplacementPlan plan = SmoToSmoReplacer.Analyze(target, donor);
 if (!plan.CanReplace)
  throw new InvalidOperationException(
   "SMO skeletons are incompatible: " + string.Join(" | ", plan.Messages));
 SmoToSmoReplacementResult result = SmoToSmoReplacer.Replace(target, donor, args[3]);
 if (Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[1]))) != targetHash)
  throw new InvalidOperationException("Target SMO was modified.");
 if (Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[2]))) != donorHash)
  throw new InvalidOperationException("Donor SMO was modified.");
 SmoDocument verified = SmoDocument.Load(args[3]);
 if (verified.HasErrors)
  throw new InvalidOperationException("SMO replacement failed strict verification.");
 if (verified.Objects.Count != target.Objects.Count)
  throw new InvalidOperationException(
   $"Target object graph size changed: {verified.Objects.Count} != {target.Objects.Count}.");
 for (int index = 0; index < target.Objects.Count; index++)
 {
  SmoObjectEntry before = target.Objects[index];
  SmoObjectEntry after = verified.Objects[index];
  if (after.Id != before.Id || after.Name != before.Name ||
      after.TypeHash != before.TypeHash || after.ParentIndex != before.ParentIndex)
   throw new InvalidOperationException(
    $"Target object identity [{index}] changed during visual transplant.");
 }
 foreach (string serviceName in new[]
          { "model_root_master", "collision_volume_root", "SubMaster" })
  if (!verified.Objects.Any(entry => entry.Name.Equals(
       serviceName, StringComparison.OrdinalIgnoreCase)))
   throw new InvalidOperationException(
    $"Target service node {serviceName} is absent from the hybrid graph.");

 string outputHash = Convert.ToHexString(SHA256.HashData(verified.Data.Span));
 if (outputHash == targetHash || outputHash == donorHash)
  throw new InvalidOperationException("Output is a copy of an input instead of a visual transplant.");

 int NonDegenerateTriangles(SmoMesh mesh) => Enumerable
  .Range(0, mesh.TriangleIndices.Length / 3)
  .Count(triangle =>
  {
   uint first = mesh.TriangleIndices[triangle * 3];
   uint second = mesh.TriangleIndices[triangle * 3 + 1];
   uint third = mesh.TriangleIndices[triangle * 3 + 2];
   return first != second && second != third && first != third;
  });
 int DonorTriangles(SmoDocument document) => document.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
  .Sum(entry => NonDegenerateTriangles(SmoMeshDecoder.Decode(document, entry)));
 if (DonorTriangles(verified) != DonorTriangles(donor))
  throw new InvalidOperationException("Output triangle payload does not match the donor.");

 string FloatBits(float value) =>
  BitConverter.SingleToInt32Bits(value).ToString("X8");
 string[] GeometryFingerprints(SmoDocument document, bool includeUv) => document.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
  .SelectMany(entry =>
  {
   SmoMesh mesh = SmoMeshDecoder.Decode(document, entry);
   return Enumerable.Range(0, mesh.TriangleIndices.Length / 3)
    .Where(triangle =>
    {
     uint first = mesh.TriangleIndices[triangle * 3];
     uint second = mesh.TriangleIndices[triangle * 3 + 1];
     uint third = mesh.TriangleIndices[triangle * 3 + 2];
     return first != second && second != third && first != third;
    })
    .Select(triangle => string.Join("|", Enumerable.Range(0, 3)
     .Select(corner => checked((int)mesh.TriangleIndices[triangle * 3 + corner]))
     .Select(vertex =>
     {
      System.Numerics.Vector3 position = mesh.Positions[vertex];
      System.Numerics.Vector2 uv = mesh.HasTextureCoordinates
       ? mesh.TextureCoordinates[vertex]
       : System.Numerics.Vector2.Zero;
      uint color = mesh.HasDiffuseColors ? mesh.DiffuseColorsArgb[vertex] : 0xFFFFFFFF;
      string geometry =
       $"{FloatBits(position.X)}:{FloatBits(position.Y)}:{FloatBits(position.Z)}:{color:X8}";
      return includeUv
       ? $"{geometry}:{FloatBits(uv.X)}:{FloatBits(uv.Y)}"
       : geometry;
     })
     .Order(StringComparer.Ordinal)));
  })
  .Order(StringComparer.Ordinal)
  .ToArray();
 if (!GeometryFingerprints(verified, true)
      .SequenceEqual(GeometryFingerprints(donor, true)))
  throw new InvalidOperationException(
   "Output triangle positions, UV0 or diffuse colors do not match the donor.");

 string TextureFingerprint(SmoTexture texture) =>
  $"{texture.Width}x{texture.Height}:0x{texture.FormatCode:X4}:" +
  Convert.ToHexString(SHA256.HashData(texture.Bgra32Pixels.Span));
 SmoTexture[] DecodedTextures(SmoDocument document) => document.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
  .Select(entry => SmoTextureDecoder.TryDecode(document, entry, out SmoTexture? texture, out string error)
   ? texture
   : throw new InvalidDataException(error))
  .ToArray();
 SmoTexture[] donorTextures = DecodedTextures(donor);
 SmoTexture[] outputTextures = DecodedTextures(verified);
 string[] donorTextureFingerprints = donorTextures
  .Select(TextureFingerprint).Order(StringComparer.Ordinal).ToArray();
 string[] outputTextureFingerprints = outputTextures
  .Select(TextureFingerprint).Order(StringComparer.Ordinal).ToArray();
 if (donorTextureFingerprints.Except(outputTextureFingerprints).Any())
  throw new InvalidOperationException(
   "At least one donor texture is absent from the target visual slots.");

 IReadOnlyDictionary<int, SmoTextureBinding> outputBindings =
 SmoTextureBindingResolver.ResolveAll(verified);
 foreach (SmoObjectEntry visibleMeshEntry in verified.Objects.Where(entry =>
           entry.TypeHash == SmoClassIds.MeshData &&
           NonDegenerateTriangles(SmoMeshDecoder.Decode(verified, entry)) > 0))
 {
  if (!outputBindings.TryGetValue(
      visibleMeshEntry.Index, out SmoTextureBinding? binding) ||
      binding.Texture is null || binding.Issue is not null)
   throw new InvalidOperationException(
    $"Visible output mesh [{visibleMeshEntry.Index}] is not bound to a donor texture.");
 }

 foreach (SmoObjectEntry meshEntry in verified.Objects.Where(entry =>
           entry.TypeHash == SmoClassIds.MeshData))
 {
  SmoObjectEntry? owner = meshEntry;
  while (owner?.ParentIndex is int parentIndex && owner.TypeHash != SmoClassIds.Skin)
   owner = verified.Objects[parentIndex];
  if (owner?.TypeHash != SmoClassIds.Skin)
   throw new InvalidDataException(
    $"Mesh [{meshEntry.Index}] has no target skin palette owner.");
  if (!SmoSkinDecoder.TryDecode(
       verified, owner, out SmoSkin? skin, out string skinError) || skin is null)
   throw new InvalidDataException(
    $"Mesh [{meshEntry.Index}] has no valid target skin palette: {skinError}");
  SmoMesh mesh = SmoMeshDecoder.Decode(verified, meshEntry);
  for (int vertex = 0; vertex < mesh.VertexCount; vertex++)
  {
   System.Numerics.Vector4 weights = mesh.BlendWeights[vertex];
   SmoBlendIndices indices = mesh.BlendIndices[vertex];
   (float Weight, byte Index)[] influences =
   [
    (weights.X, indices.X), (weights.Y, indices.Y),
    (weights.Z, indices.Z), (weights.W, indices.W)
   ];
   if (influences.Any(influence =>
        influence.Weight > 0.000001f && influence.Index >= skin.Bones.Count))
    throw new InvalidDataException(
     $"Mesh [{meshEntry.Index}] has an active bone index outside its target palette.");
  }
 }
 Console.WriteLine(
  $"SMO→SMO PASS: compatibility={plan.Compatibility}; " +
  $"donor-meshes={result.DonorMeshCount}; output-meshes={result.MeshCount}; " +
  $"donor-textures={result.DonorTextureCount}; output-textures={result.TextureCount}; " +
  $"bones={result.BoneCount}; target-objects={target.Objects.Count}; " +
  $"donor-objects={donor.Objects.Count}; triangles={DonorTriangles(verified)}; " +
  $"matched-bones={plan.MatchedBoneNames.Count}; ignored-donor-bones=" +
  $"{plan.IgnoredDonorBones.Count}; unbound-target-bones={plan.UnboundTargetBones.Count}; " +
  $"helper-path-adaptations={plan.HierarchyAdaptations.Count}; " +
  $"SHA-256={result.Sha256}; output={result.OutputPath}");
 foreach (string message in plan.Messages)
  Console.WriteLine("  " + message);
 return 0;
}

if (args.Length == 4 && args[0] == "--skinned-glb-diagnostics")
{
 SmoDocument target = SmoDocument.Load(args[1]);
 ImportedScene donor = GlbModelReader.Read(args[2]);
 ImportedTexture texture = donor.Textures.FirstOrDefault() ??
  throw new InvalidDataException("Diagnostic GLB has no embedded base-color texture.");
 string outputDirectory = Path.GetFullPath(args[3]);
 Directory.CreateDirectory(outputDirectory);

 WriteTextureVariant("01_texture_rgb_only.smo", replaceAlpha: false);
 WriteTextureVariant("02_texture_rgba.smo", replaceAlpha: true);

 ImportedMesh[] bodyMeshes = donor.Meshes
  .GroupBy(mesh => mesh.MaterialIndex)
  .OrderByDescending(group => group.Sum(mesh => mesh.TriangleIndices.Length / 3))
  .First().ToArray();
 var bodyScene = new ImportedScene(bodyMeshes, donor.Textures);
 WriteSkinnedVariant(
  "04_body_skinned_original_texture.smo", bodyScene,
  rebaseToTargetBindPose: true);

 ImportedSkeleton skeleton = bodyMeshes[0].Skinning?.Skeleton ??
  throw new InvalidDataException("Diagnostic body primitive has no skin.");
 int pelvis = skeleton.JointNames
  .Select((name, index) => (name, index))
  .Single(item => item.name == "Pelvis").index;
 if (pelvis > ushort.MaxValue)
  throw new InvalidDataException("Pelvis joint index exceeds UInt16.");
 ImportedMesh[] rigidMeshes = bodyMeshes.Select(mesh => mesh with
 {
  Skinning = new ImportedSkinning(
   skeleton,
   Enumerable.Repeat(
    new ImportedJointIndices((ushort)pelvis, 0, 0, 0), mesh.Positions.Length).ToArray(),
   Enumerable.Repeat(Vector4.UnitX, mesh.Positions.Length).ToArray())
 }).ToArray();
 WriteSkinnedVariant(
  "03_body_rigid_pelvis_original_texture.smo",
  new ImportedScene(rigidMeshes, donor.Textures),
  rebaseToTargetBindPose: false);
 return 0;

 void WriteTextureVariant(string fileName, bool replaceAlpha)
 {
  byte[] output = target.Data.ToArray();
  int[] textureIndices = SMOTextureTool.Core.SmoDocument.Parse(output).Textures
   .Select(item => item.Index).ToArray();
  foreach (int textureIndex in textureIndices)
   output = replaceAlpha
    ? FixedSizeTextureWriter.ReplaceRgbaDiagnosticUnsafe(
       output, textureIndex, texture.Data)
    : FixedSizeTextureWriter.ReplaceRgb(output, textureIndex, texture.Data);
  string path = Path.Combine(outputDirectory, fileName);
  File.WriteAllBytes(path, output);
  SmoDocument verified = SmoDocument.Load(path);
  if (verified.HasErrors || verified.Objects.Count != target.Objects.Count)
   throw new InvalidDataException($"Diagnostic texture variant {fileName} is invalid.");
  Console.WriteLine(
   $"{fileName}: texture-only; alpha={(replaceAlpha ? "donor" : "target")}; " +
   $"SHA-256={Convert.ToHexString(SHA256.HashData(output))}");
 }

 void WriteSkinnedVariant(
  string fileName,
  ImportedScene scene,
  bool rebaseToTargetBindPose)
 {
  string path = Path.Combine(outputDirectory, fileName);
  GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
   target, scene, ReplacementTransform.Identity, path,
   rebaseToTargetBindPose, texture: null);
  Console.WriteLine(
   $"{fileName}: triangles={result.TriangleCount}; palettes={result.PaletteCount}; " +
   $"SHA-256={result.Sha256}");
 }
}

if (args.Length == 4 && args[0] is "--skinned-glb" or "--skinned-fbx")
{
 SmoDocument target = SmoDocument.Load(args[1]);
 ImportedScene donor = args[0] == "--skinned-fbx"
  ? FbxModelReader.Read(args[2])
  : GlbModelReader.Read(args[2]);
 GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(target, donor);
 Console.WriteLine(
  $"Skinned model plan: {plan.Compatibility}; joints={plan.JointCount}; " +
  $"active={plan.ActiveJointCount}; exact={plan.MatchedBoneNames.Count}; " +
  $"remapped={plan.RemappedBones.Count}; bind-pose-diff={plan.DifferentBindPoseJointCount}.");
 foreach (GlbBoneRemap mapping in plan.RemappedBones)
  Console.WriteLine($"  {mapping.DonorBoneName} -> {mapping.TargetBoneName}: {mapping.Reason}");
 foreach (string message in plan.Messages)
  Console.WriteLine("  " + message);
 if (!plan.CanReplace)
  throw new InvalidOperationException("Skinned GLB plan is blocked.");
 GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
  target, donor, ReplacementTransform.Identity, args[3],
  rebaseToTargetBindPose: true,
  texture: donor.Textures.FirstOrDefault());
 SmoDocument verified = SmoDocument.Load(args[3]);
 if (verified.HasErrors || verified.Objects.Count != target.Objects.Count)
  throw new InvalidDataException("Skinned GLB result did not preserve target graph.");
 for (int index = 0; index < target.Objects.Count; index++)
 {
  SmoObjectEntry before = target.Objects[index];
  SmoObjectEntry after = verified.Objects[index];
  if (before.Id != after.Id || before.Name != after.Name ||
      before.TypeHash != after.TypeHash || before.ParentIndex != after.ParentIndex)
   throw new InvalidDataException($"Target object identity [{index}] changed.");
 }
 foreach (SmoObjectEntry targetSkinEntry in target.Objects.Where(entry =>
              entry.TypeHash == SmoClassIds.Skin))
 {
  if (!SmoSkinDecoder.TryDecode(
       target, targetSkinEntry, out SmoSkin? targetSkin, out _) || targetSkin is null)
   continue;
  string[] targetNames = targetSkin.Bones.Select(bone =>
   target.Objects[bone.NodeObjectIndex].Name).ToArray();
  if (targetNames.Distinct(StringComparer.Ordinal).Count() != 1)
   continue;
  SmoObjectEntry outputSkinEntry = verified.Objects[targetSkinEntry.Index];
  if (!SmoSkinDecoder.TryDecode(
       verified, outputSkinEntry, out SmoSkin? outputSkin, out string outputError) ||
      outputSkin is null)
   throw new InvalidDataException(
    $"Rigid target skin [{targetSkinEntry.Index}] is invalid: {outputError}");
  string[] outputNames = outputSkin.Bones.Select(bone =>
   verified.Objects[bone.NodeObjectIndex].Name).ToArray();
  if (!targetNames.SequenceEqual(outputNames, StringComparer.Ordinal))
   throw new InvalidDataException(
    $"Rigid target skin [{targetSkinEntry.Index}] palette changed.");
 }
 Console.WriteLine(
  $"SKINNED MODEL PASS: output-meshes={result.MeshSlotCount}; " +
  $"vertices={result.VertexCount}; triangles={result.TriangleCount}; " +
  $"palettes={result.PaletteCount}; SHA-256={result.Sha256}; output={result.OutputPath}");
 return 0;
}

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
 Console.Error.WriteLine("       SmoImporter.FormatTests --smo-to-smo <target.smo> <donor.smo> <output.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --skinned-glb <target.smo> <donor.glb> <output.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --skinned-fbx <target.smo> <donor.fbx> <output.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --skinned-glb-diagnostics <target.smo> <donor.glb> <output-dir>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --dump-tree <sample.smo>");
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
