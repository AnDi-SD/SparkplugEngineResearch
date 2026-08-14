using System.Security.Cryptography;
using System.Numerics;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;

if (args.Length is 2 or 3 && args[0] == "--rigid-texture-bundle")
{
 RigidGlbTextureBundle bundle = RigidGlbTextureBundleReader.ReadModel(
  args[1], args.Length == 3 ? args[2] : null);
 int vertexCount = bundle.Scene.Meshes.Sum(mesh => mesh.Positions.Length);
 int triangleCount = bundle.Scene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
 int frameCount = bundle.MaterialGroups.Sum(group => group.Frames.Count);
 if (bundle.MaterialGroups.SelectMany(group => group.Meshes).Count() !=
     bundle.Scene.Meshes.Count)
  throw new InvalidOperationException("Rigid bundle lost or duplicated a source mesh.");
 if (bundle.MaterialGroups.SelectMany(group => group.Frames).Any(frame =>
      frame.Texture.Width < frame.SourceWidth ||
      frame.Texture.Height < frame.SourceHeight))
  throw new InvalidOperationException("Rigid bundle downscaled a texture.");
 Console.WriteLine(
  $"RIGID BUNDLE: groups={bundle.MaterialGroups.Count}; frames={frameCount}; " +
 $"meshes={bundle.Scene.Meshes.Count}; vertices={vertexCount}; triangles={triangleCount}");
 if (bundle.IgnoredMeshes.Count > 0)
  Console.WriteLine("  ignored model meshes: " + string.Join(", ", bundle.IgnoredMeshes));
 if (bundle.IgnoredTextureFiles.Count > 0)
  Console.WriteLine("  ignored PNG files: " + string.Join(", ", bundle.IgnoredTextureFiles));
 foreach (RigidMaterialGroup group in bundle.MaterialGroups)
  Console.WriteLine(
   $"  {group.Name}: meshes={group.Meshes.Count}; frames={group.Frames.Count}; " +
   string.Join(", ", group.Frames.Select(frame =>
    $"{Path.GetFileName(frame.SourcePath)}={frame.Texture.Width}x{frame.Texture.Height}" +
    (frame.WasUpscaled ? " (upscaled)" : " (exact)"))));
 return 0;
}

if (args.Length == 2 && args[0] == "--rigid-texture-resize-regression")
{
 string modelPath = Path.GetFullPath(args[1]);
 RigidGlbTextureBundle sourceBundle = RigidGlbTextureBundleReader.ReadModel(modelPath);
 RigidMaterialGroup firstGroup = sourceBundle.MaterialGroups[0];
 string temporary = Path.Combine(
  Path.GetTempPath(), "smo-rigid-texture-resize-" + Guid.NewGuid().ToString("N"));
 Directory.CreateDirectory(temporary);
 try
 {
  foreach (RigidTextureFrame frame in sourceBundle.MaterialGroups.SelectMany(group => group.Frames))
   File.Copy(frame.SourcePath, Path.Combine(temporary, Path.GetFileName(frame.SourcePath)));

  string resizedSource = Path.Combine(
   temporary, Path.GetFileName(firstGroup.BaseFrame.SourcePath));
  using (var image = new Image<Rgba32>(3, 5))
  {
   image[0, 0] = new Rgba32(17, 91, 203, 0);
   image[2, 4] = new Rgba32(211, 37, 9, 255);
   image.SaveAsPng(resizedSource);
  }

  RigidGlbTextureBundle resized = RigidGlbTextureBundleReader.ReadModel(
   modelPath, temporary);
  RigidTextureFrame normalized = resized.MaterialGroups
   .Single(group => group.MaterialNumber == firstGroup.MaterialNumber).BaseFrame;
  if (!normalized.WasUpscaled || normalized.SourceWidth != 3 ||
      normalized.SourceHeight != 5 || normalized.Texture.Width != 4 ||
      normalized.Texture.Height != 8)
   throw new InvalidOperationException("Non-POT texture was not enlarged from 3x5 to 4x8.");
  using Image<Rgba32> decoded = Image.Load<Rgba32>(normalized.Texture.Data);
  if (decoded[0, 0] != new Rgba32(17, 91, 203, 0) ||
      decoded[3, 7] != new Rgba32(211, 37, 9, 255))
   throw new InvalidOperationException(
    "Alpha-aware POT enlargement did not preserve clamped corner pixels.");

  bool refusedDownscale = false;
  try
  {
   _ = RigidGlbTextureBundleReader.ReadModel(
    modelPath, temporary, maximumTextureDimension: 4);
  }
  catch (InvalidDataException exception) when (
   exception.Message.Contains("Downscaling is forbidden", StringComparison.Ordinal))
  {
   refusedDownscale = true;
  }
  if (!refusedDownscale)
   throw new InvalidOperationException("Texture reader did not reject a required downscale.");
  Console.WriteLine(
   "RIGID TEXTURE RESIZE PASS: 3x5 -> 4x8; hidden RGB/alpha corners preserved; downscale rejected.");
 }
 finally
 {
  if (Directory.Exists(temporary))
   Directory.Delete(temporary, recursive: true);
 }
 return 0;
}

if (args.Length == 2 && args[0] == "--scan-texture-metadata")
{
 string[] files = Directory.Exists(args[1])
  ? Directory.EnumerateFiles(args[1], "*.smo", SearchOption.AllDirectories).ToArray()
  : new[] { args[1] };
 foreach (string file in files)
 {
  SmoDocument document = SmoDocument.Load(file);
  foreach (SmoObjectEntry entry in document.Objects.Where(item =>
             item.TypeHash == SmoClassIds.TextureData))
  {
   if (!SmoTextureDecoder.TryDecode(document, entry, out SmoTexture? texture, out _) ||
       texture is null || texture.FormatCode is not (0x32E3 or 0x43E3) ||
       texture.Width == texture.Height)
    continue;
   byte[] bytes = document.Data.Span.Slice(
    checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize)).ToArray();
   uint U32(int offset) => System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
    bytes.AsSpan(offset));
   Console.WriteLine(
    $"{Path.GetFileName(file)} [{entry.Index}] {entry.Name} " +
    $"{texture.Width}x{texture.Height}: " +
    $"2C={U32(0x2C):X8} 30={U32(0x30):X8} 34={U32(0x34):X8} 38={U32(0x38):X8}");
  }
 }
 return 0;
}

if (args.Length == 2 && args[0] == "--dump-texture-sequences")
{
 const uint sequenceClass = 0x16FB0E47;
 SmoDocument document = SmoDocument.Load(args[1]);
 Dictionary<uint, SmoObjectEntry> byId = document.Objects
  .GroupBy(entry => entry.Id).Where(group => group.Count() == 1)
  .ToDictionary(group => group.Key, group => group.Single());
 foreach (SmoObjectEntry entry in document.Objects.Where(item =>
             item.TypeHash == sequenceClass))
 {
  byte[] bytes = document.Data.Span.Slice(
   checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize)).ToArray();
  if (!SmoDataBlockReader.TryReadHeader(bytes, 8, out SmoDataBlockHeader field) ||
      field.FieldType != 0 || field.PayloadSize < 4)
   throw new InvalidDataException($"Sequence [{entry.Index}] has no field0 payload.");
  uint count = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
   bytes.AsSpan(field.PayloadOffset));
  int cursor = checked(field.PayloadOffset + 4 + (int)count * 4);
  Console.WriteLine($"SEQUENCE [{entry.Index}] {entry.Name}: keys={count}");
  for (int index = 0; index < count; index++)
  {
   float time = BitConverter.Int32BitsToSingle(
    System.Buffers.Binary.BinaryPrimitives.ReadInt32LittleEndian(
     bytes.AsSpan(field.PayloadOffset + 4 + index * 4)));
   uint id = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(cursor));
   uint size = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(cursor + 4));
   Console.WriteLine($"  {index:D2}: t={time:G9} id={id} " +
    $"name={(byId.TryGetValue(id, out SmoObjectEntry? texture) ? texture.Name : "?")} inline=0x{size:X}");
   cursor = checked(cursor + 8 + (int)size);
  }
  if (cursor != field.PayloadEnd)
   throw new InvalidDataException(
    $"Sequence [{entry.Index}] leaves {field.PayloadEnd - cursor} payload bytes.");
 }
 return 0;
}

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

if (args.Length == 4 && args[0] == "--rigid-multitexture")
{
 string targetPath = Path.GetFullPath(args[1]);
 string modelPath = Path.GetFullPath(args[2]);
 string outputPath = Path.GetFullPath(args[3]);
 if (string.Equals(targetPath, outputPath, StringComparison.OrdinalIgnoreCase) ||
     string.Equals(modelPath, outputPath, StringComparison.OrdinalIgnoreCase))
  throw new InvalidOperationException("Rigid multi-texture output must be a new file.");
 byte[] targetBefore = File.ReadAllBytes(targetPath);
 byte[] modelBefore = File.ReadAllBytes(modelPath);
 SmoDocument target = SmoDocument.Load(targetPath);
 RigidGlbTextureBundle bundle = RigidGlbTextureBundleReader.ReadModel(modelPath);
 SmoRigidMultiMaterialPackAnalysis analysis =
  SmoRigidMultiMaterialPacker.Analyze(target, bundle);
 if (!analysis.CanPack)
  throw new InvalidOperationException(
   "Rigid multi-texture analysis failed: " + string.Join(" | ", analysis.Messages));
 if (analysis.MaterialGroupCount != bundle.MaterialGroups.Count ||
     analysis.MeshCount != bundle.MaterialGroups.Sum(group => group.Meshes.Count) ||
     analysis.TextureCount != bundle.MaterialGroups.Sum(group => group.Frames.Count) ||
     analysis.RigidBoneSlot != 8 ||
     !analysis.RigidBoneName.Equals("Head", StringComparison.OrdinalIgnoreCase))
  throw new InvalidOperationException("Rigid multi-texture analysis summary is inconsistent.");
 SmoExportScene targetScene = SmoSceneBuilder.Build(target);
 ReplacementTransform fit = ReplacementTransformFitter.FitByHeightAndCenter(
  targetScene.Meshes.SelectMany(mesh => mesh.Positions),
  bundle.MaterialGroups.SelectMany(group => group.Meshes).SelectMany(mesh => mesh.Positions));
 SmoRigidMultiMaterialPackResult result = SmoRigidMultiMaterialPacker.Pack(
  target, bundle, fit, outputPath);
 if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBefore))
  throw new InvalidOperationException("Rigid multi-texture packing modified the target SMO.");
 if (!File.ReadAllBytes(modelPath).SequenceEqual(modelBefore))
  throw new InvalidOperationException("Rigid multi-texture packing modified the source model.");
 SmoDocument verified = SmoDocument.Load(outputPath);
 if (verified.HasErrors)
  throw new InvalidOperationException("Rigid multi-texture output failed strict parsing.");
 HashSet<uint> targetIds = target.Objects.Select(entry => entry.Id).ToHashSet();
 Dictionary<uint, SmoObjectEntry> verifiedById = verified.Objects.ToDictionary(entry => entry.Id);
 foreach (SmoRigidPackedTexture packed in result.Textures)
 {
  SmoObjectEntry textureEntry = verifiedById[packed.ObjectId];
  if (!SmoTextureDecoder.TryDecode(
       verified, textureEntry, out SmoTexture? decoded, out string textureError) ||
      decoded is null)
   throw new InvalidOperationException(textureError);
  RigidTextureFrame sourceFrame = bundle.MaterialGroups
   .Single(group => group.MaterialNumber == packed.MaterialNumber).Frames
   .Single(frame => frame.FrameNumber == packed.FrameNumber);
  using Image<Rgba32> expectedImage = Image.Load<Rgba32>(sourceFrame.Texture.Data);
  byte[] expectedBgra = new byte[checked(expectedImage.Width * expectedImage.Height * 4)];
  int pixelOffset = 0;
  expectedImage.ProcessPixelRows(accessor =>
  {
   for (int y = 0; y < accessor.Height; y++)
    foreach (Rgba32 pixel in accessor.GetRowSpan(y))
    {
     expectedBgra[pixelOffset++] = pixel.B;
     expectedBgra[pixelOffset++] = pixel.G;
     expectedBgra[pixelOffset++] = pixel.R;
     expectedBgra[pixelOffset++] = pixel.A;
    }
  });
  if (decoded.Width != sourceFrame.Texture.Width ||
      decoded.Height != sourceFrame.Texture.Height ||
      !decoded.Bgra32Pixels.Span.SequenceEqual(expectedBgra))
   throw new InvalidOperationException(
    $"Packed texture {packed.ObjectName} differs from its normalized PNG pixels.");
 }
 SmoObjectEntry[] addedMeshes = verified.Objects.Where(entry =>
  !targetIds.Contains(entry.Id) && entry.TypeHash == SmoClassIds.MeshData).ToArray();
 SmoObjectEntry[] addedSkins = verified.Objects.Where(entry =>
  !targetIds.Contains(entry.Id) && entry.TypeHash == SmoClassIds.Skin).ToArray();
 if (addedMeshes.Length != bundle.MaterialGroups.Count ||
     addedSkins.Length != bundle.MaterialGroups.Count)
  throw new InvalidOperationException("Rigid multi-texture branch count mismatch.");
 foreach (SmoObjectEntry skinEntry in addedSkins)
 {
  if (!SmoSkinDecoder.TryDecode(
       verified, skinEntry, out SmoSkin? skin, out string skinError) || skin is null)
   throw new InvalidOperationException(skinError);
  if (skin.Bones.Count != 16 || skin.Bones.Any(bone => bone.InlineSerializedSize != 0) ||
      !verified.Objects[skin.Bones[8].NodeObjectIndex].Name.Equals(
       "Head", StringComparison.OrdinalIgnoreCase))
   throw new InvalidOperationException(
    $"Generated skin {skinEntry.Name} does not use a reference-only Head palette.");
 }
 IReadOnlyDictionary<int, SmoTextureBinding> bindings =
  SmoTextureBindingResolver.ResolveAll(verified);
 foreach (RigidMaterialGroup group in bundle.MaterialGroups)
 {
  SmoObjectEntry mesh = addedMeshes.Single(entry =>
   entry.Name == $"layla_mat{group.MaterialNumber}_mesh");
 if (!bindings.TryGetValue(mesh.Index, out SmoTextureBinding? binding) ||
      binding.Texture is null || binding.Issue is not null ||
      (binding.AnimationFrames?.Count ?? 1) != group.Frames.Count)
   throw new InvalidOperationException(
   $"Generated {group.Name} texture/sequence binding is incomplete.");
  SmoMesh decodedMesh = SmoMeshDecoder.Decode(verified, mesh);
  if (decodedMesh.Normals.Length != decodedMesh.VertexCount ||
      decodedMesh.Normals.Any(normal =>
       !float.IsFinite(normal.X) || !float.IsFinite(normal.Y) || !float.IsFinite(normal.Z) ||
       normal.LengthSquared() < 0.5f))
   throw new InvalidOperationException(
    $"Generated {group.Name} has missing, invalid or non-unit vertex normals.");
 }
 const uint textureSequenceClassId = 0x16FB0E47;
 foreach (RigidMaterialGroup group in bundle.MaterialGroups.Where(item => item.Frames.Count > 1))
 {
  SmoObjectEntry sequence = verified.Objects.Single(entry =>
   !targetIds.Contains(entry.Id) && entry.TypeHash == textureSequenceClassId &&
   entry.Name == $"layla_mat{group.MaterialNumber}_sequence");
  ReadOnlySpan<byte> sequenceBytes = verified.Data.Span.Slice(
   checked((int)sequence.PhysicalOffset), checked((int)sequence.SerializedSize));
  if (!SmoDataBlockReader.TryReadHeader(
       sequenceBytes, 8, out SmoDataBlockHeader sequenceField) ||
      sequenceField.FieldType != 0 || sequenceField.PayloadSize < 4)
   throw new InvalidOperationException($"Generated {group.Name} sequence is malformed.");

  var expectedSchedule = new List<int>(checked(4 * group.Frames.Count - 2));
  expectedSchedule.AddRange(Enumerable.Repeat(0, 3));
  for (int frame = 1; frame < group.Frames.Count - 1; frame++)
   expectedSchedule.AddRange(Enumerable.Repeat(frame, 2));
  expectedSchedule.AddRange(Enumerable.Repeat(group.Frames.Count - 1, 3));
  for (int frame = group.Frames.Count - 2; frame >= 1; frame--)
   expectedSchedule.AddRange(Enumerable.Repeat(frame, 2));
  uint keyCount = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
   sequenceBytes[sequenceField.PayloadOffset..]);
  if (keyCount != expectedSchedule.Count)
   throw new InvalidOperationException($"Generated {group.Name} sequence key count is wrong.");

  float keyTime = 0;
  float keyStep = BitConverter.Int32BitsToSingle(0x3D088889);
  for (int key = 0; key < expectedSchedule.Count; key++)
  {
   keyTime += keyStep;
   int actualBits = System.Buffers.Binary.BinaryPrimitives.ReadInt32LittleEndian(
    sequenceBytes[(sequenceField.PayloadOffset + 4 + key * 4)..]);
   if (actualBits != BitConverter.SingleToInt32Bits(keyTime))
    throw new InvalidOperationException($"Generated {group.Name} sequence time {key} is wrong.");
  }

  uint[] textureIds = result.Textures
   .Where(texture => texture.MaterialNumber == group.MaterialNumber)
   .OrderBy(texture => texture.FrameNumber)
   .Select(texture => texture.ObjectId)
   .ToArray();
  var inlineDefinitions = new HashSet<int> { 0 };
  int referenceOffset = checked(
   sequenceField.PayloadOffset + 4 + expectedSchedule.Count * sizeof(float));
  for (int key = 0; key < expectedSchedule.Count; key++)
  {
   int frame = expectedSchedule[key];
   uint actualId = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
    sequenceBytes[referenceOffset..]);
   uint actualSize = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
    sequenceBytes[(referenceOffset + 4)..]);
   uint expectedSize = inlineDefinitions.Add(frame)
    ? verifiedById[textureIds[frame]].SerializedSize
    : 0;
   if (actualId != textureIds[frame] || actualSize != expectedSize)
    throw new InvalidOperationException(
     $"Generated {group.Name} sequence reference {key} is wrong or forward-linked.");
   referenceOffset = checked(referenceOffset + 8 + (int)actualSize);
  }
  if (referenceOffset != sequenceField.PayloadEnd)
   throw new InvalidOperationException($"Generated {group.Name} sequence leaves unread bytes.");
 }
 Console.WriteLine(
  $"RIGID MULTITEXTURE PASS: groups={result.MaterialGroupCount}; " +
  $"meshes={result.AddedMeshCount}; textures={result.AddedTextureCount}; " +
  $"sequences={result.AddedSequenceCount}; vertices={result.VertexCount}; " +
  $"triangles={result.TriangleCount}; bone=[{result.RigidBoneSlot}] {result.RigidBoneName}; " +
  $"scale={fit.Scale:G9}; move={fit.Translation}; bytes={result.FileSize}; " +
  $"sha256={result.Sha256}; file={result.OutputPath}");
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
 if (verified.Objects.Count <= target.Objects.Count)
  throw new InvalidOperationException("No donor visual objects were added to the target graph.");
 Dictionary<uint, SmoObjectEntry> verifiedById = verified.Objects.ToDictionary(entry => entry.Id);
 HashSet<uint> targetIds = target.Objects.Select(entry => entry.Id).ToHashSet();
 for (int index = 0; index < target.Objects.Count; index++)
 {
  SmoObjectEntry before = target.Objects[index];
  if (!verifiedById.TryGetValue(before.Id, out SmoObjectEntry? after))
   throw new InvalidOperationException($"Target object ID {before.Id} disappeared.");
  if (after.Id != before.Id || after.Name != before.Name ||
      after.TypeHash != before.TypeHash)
   throw new InvalidOperationException(
    $"Target object identity [{index}] changed during visual transplant.");
  uint? beforeParentId = before.ParentIndex is int beforeParent
   ? target.Objects[beforeParent].Id
   : null;
  uint? afterParentId = after.ParentIndex is int afterParent
   ? verified.Objects[afterParent].Id
   : null;
  if (beforeParentId != afterParentId)
   throw new InvalidOperationException(
    $"Target object ID {before.Id} changed its catalog parent.");
 }
 SmoObjectEntry[] packedMeshes = verified.Objects.Where(entry =>
  entry.TypeHash == SmoClassIds.MeshData && !targetIds.Contains(entry.Id)).ToArray();
 SmoObjectEntry[] packedTextures = verified.Objects.Where(entry =>
  entry.TypeHash == SmoClassIds.TextureData && !targetIds.Contains(entry.Id)).ToArray();
 if (packedMeshes.Length != donor.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) ||
     packedTextures.Length != donor.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData))
  throw new InvalidOperationException(
   $"Packed branch counts differ: meshes={packedMeshes.Length}, textures={packedTextures.Length}.");
 if (verified.Objects.Any(entry =>
      entry.TypeHash == SmoClassIds.Node && !targetIds.Contains(entry.Id)))
  throw new InvalidOperationException("Donor node objects leaked into the packed visual graph.");
 bool isBloomFaragondaRegression =
  Path.GetFileName(args[1]).Equals("bloom_jeans.smo", StringComparison.OrdinalIgnoreCase) &&
  Path.GetFileName(args[2]).Equals("Faragonda.smo", StringComparison.OrdinalIgnoreCase);
 if (isBloomFaragondaRegression &&
     (verified.Objects.Count != 142 ||
      verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) != 13 ||
      verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.Skin) != 13 ||
      verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MaterialData) != 5 ||
      verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData) != 5 ||
      verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.UvController) != 1 ||
      result.PackedMeshCount != 7 || result.PackedTextureCount != 3 ||
      result.NonDegenerateTriangleCount != 1440))
  throw new InvalidOperationException(
    "Faragonda→Bloom packed graph does not match the 142-object/7-mesh/3-texture regression profile.");

  bool isStellaToBloomRegression =
   Path.GetFileName(args[1]).Equals("bloom_jeans.smo", StringComparison.OrdinalIgnoreCase) &&
   Path.GetFileName(args[2]).Equals("StellaX.smo", StringComparison.OrdinalIgnoreCase);
  bool isBloomToStellaRegression =
   Path.GetFileName(args[1]).Equals("StellaX.smo", StringComparison.OrdinalIgnoreCase) &&
   Path.GetFileName(args[2]).Equals("bloom_jeans.smo", StringComparison.OrdinalIgnoreCase);
  if (isStellaToBloomRegression &&
      (verified.Objects.Count != 158 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) != 14 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.Skin) != 12 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MaterialData) != 7 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData) != 14 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.RenderNode) != 5 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.Model) != 2 ||
       result.PackedMeshCount != 8 || result.PackedTextureCount != 12 ||
       result.NonDegenerateTriangleCount != 1357))
   throw new InvalidOperationException(
    "StellaX→Bloom packed forest does not match the 158-object/8-mesh/12-texture profile.");
  if (isBloomToStellaRegression &&
      (verified.Objects.Count != 141 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) != 14 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.Skin) != 12 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MaterialData) != 7 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData) != 14 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.RenderNode) != 5 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.Model) != 2 ||
       result.PackedMeshCount != 6 || result.PackedTextureCount != 2 ||
       result.NonDegenerateTriangleCount != 1880))
   throw new InvalidOperationException(
    "Bloom→StellaX packed forest does not match the 141-object/6-mesh/2-texture profile.");

  if (isStellaToBloomRegression || isBloomToStellaRegression)
  {
   const uint sharedHelperClassId = 0x7AC95AEC;
   if (verified.Objects.Count(entry => entry.TypeHash == sharedHelperClassId) !=
       target.Objects.Count(entry => entry.TypeHash == sharedHelperClassId))
    throw new InvalidOperationException(
     "The byte-identical shared visual helper was copied instead of reusing target identity.");

   (string Root, string Parent)[] expectedForestParents = isStellaToBloomRegression
    ? [("stella_head", "Scene Root"), ("WingR", "Spine_03"), ("WingL", "Spine_03")]
    : [("bloom_eyes", "Head")];
   foreach ((string rootName, string parentName) in expectedForestParents)
   {
    SmoObjectEntry root = verified.Objects.Single(entry =>
     entry.TypeHash == SmoClassIds.RenderNode &&
     !targetIds.Contains(entry.Id) &&
     entry.Name.Equals(rootName, StringComparison.Ordinal));
    if (root.ParentIndex is not int parentIndex ||
        !targetIds.Contains(verified.Objects[parentIndex].Id) ||
        !verified.Objects[parentIndex].Name.Equals(parentName, StringComparison.Ordinal))
     throw new InvalidOperationException(
      $"Packed render {rootName} is not attached to target node {parentName}.");
   }
   if (isStellaToBloomRegression)
   {
    SmoObjectEntry relocatedAtlas = verified.Objects.Single(entry =>
     entry.TypeHash == SmoClassIds.TextureData &&
     !targetIds.Contains(entry.Id) &&
     entry.Name.Equals("stella_x", StringComparison.Ordinal));
    SmoObjectEntry? ancestor = relocatedAtlas;
    bool belongsToWingR = false;
    while (ancestor?.ParentIndex is int parentIndex)
    {
     ancestor = verified.Objects[parentIndex];
     if (ancestor.TypeHash == SmoClassIds.RenderNode &&
         ancestor.Name.Equals("WingR", StringComparison.Ordinal))
     {
      belongsToWingR = true;
      break;
     }
    }
    if (!belongsToWingR)
     throw new InvalidOperationException(
      "The shared stella_x atlas was not relocated inline to the earliest WingR consumer.");
   }
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
 int SerializedTriangles(SmoDocument document) => document.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
  .Sum(entry => SmoMeshDecoder.Decode(document, entry).TriangleCount);
 int Vertices(SmoDocument document) => document.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
  .Sum(entry => SmoMeshDecoder.Decode(document, entry).VertexCount);
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
 IReadOnlyDictionary<int, SmoTextureBinding> donorBindings =
  SmoTextureBindingResolver.ResolveAll(donor);
 foreach (SmoObjectEntry visibleMeshEntry in verified.Objects.Where(entry =>
           entry.TypeHash == SmoClassIds.MeshData &&
           NonDegenerateTriangles(SmoMeshDecoder.Decode(verified, entry)) > 0))
 {
  if (outputBindings.TryGetValue(
       visibleMeshEntry.Index, out SmoTextureBinding? binding) &&
      binding.Texture is not null && binding.Issue is null)
   continue;
  byte[] visibleBytes = verified.Data.Span.Slice(
   checked((int)visibleMeshEntry.PhysicalOffset),
   checked((int)visibleMeshEntry.SerializedSize)).ToArray();
  SmoObjectEntry? donorMesh = donor.Objects.SingleOrDefault(entry =>
   entry.TypeHash == SmoClassIds.MeshData &&
   entry.Name == visibleMeshEntry.Name &&
   entry.SerializedSize == visibleMeshEntry.SerializedSize &&
   donor.Data.Span.Slice(
    checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize))
    .SequenceEqual(visibleBytes));
  bool donorAlsoHasNoResolvedTexture = donorMesh is not null &&
   (!donorBindings.TryGetValue(donorMesh.Index, out SmoTextureBinding? donorBinding) ||
    donorBinding.Texture is null || donorBinding.Issue is not null);
  if (!donorAlsoHasNoResolvedTexture)
    throw new InvalidOperationException(
     $"Visible output mesh [{visibleMeshEntry.Index}] is not bound to a donor texture.");
 }

 foreach (SmoObjectEntry legacyMeshEntry in verified.Objects.Where(entry =>
           entry.TypeHash == SmoClassIds.MeshData && targetIds.Contains(entry.Id)))
 {
  SmoMesh legacy = SmoMeshDecoder.Decode(verified, legacyMeshEntry);
  if (legacy.StripIndices.Length < 3 ||
      legacy.StripIndices.Any(index => index != legacy.StripIndices[0]) ||
      NonDegenerateTriangles(legacy) != 0)
   throw new InvalidOperationException(
    $"Legacy target mesh ID {legacyMeshEntry.Id} is not strictly degenerate.");
 }

 foreach (SmoObjectEntry meshEntry in verified.Objects.Where(entry =>
           entry.TypeHash == SmoClassIds.MeshData))
 {
  SmoMesh mesh = SmoMeshDecoder.Decode(verified, meshEntry);
  SmoObjectEntry? owner = meshEntry;
  while (owner?.ParentIndex is int parentIndex && owner.TypeHash != SmoClassIds.Skin)
   owner = verified.Objects[parentIndex];
  if (owner?.TypeHash != SmoClassIds.Skin)
  {
   if (mesh.HasSkinningData)
    throw new InvalidDataException(
     $"Skinned mesh [{meshEntry.Index}] has no target skin palette owner.");
   continue;
  }
  if (!SmoSkinDecoder.TryDecode(
       verified, owner, out SmoSkin? skin, out string skinError) || skin is null)
   throw new InvalidDataException(
    $"Mesh [{meshEntry.Index}] has no valid target skin palette: {skinError}");
  if (!mesh.HasSkinningData)
   continue;
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
  $"donor-meshes={result.DonorMeshCount}; packed-meshes={result.PackedMeshCount}; " +
  $"donor-textures={result.DonorTextureCount}; packed-textures={result.PackedTextureCount}; " +
  $"bones={result.BoneCount}; target-objects={target.Objects.Count}; " +
  $"donor-objects={donor.Objects.Count}; triangles={DonorTriangles(verified)}; " +
  $"donor-serialized-triangles={SerializedTriangles(donor)}; " +
  $"target-serialized-triangles={SerializedTriangles(target)}; " +
  $"donor-vertices={Vertices(donor)}; target-vertices={Vertices(target)}; " +
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

if (args.Length is 4 or 5 && args[0] is "--skinned-glb" or "--skinned-fbx")
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
  SkinnedGeometryTransferMode.RetargetToGameBindPose,
  texture: args.Length == 5
   ? ImportedTextureFileReader.Read(args[4])
   : donor.Textures.FirstOrDefault());
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

if (args.Length == 2 && args[0] == "--fixed-texture-writer-regression")
{
 byte[] source = File.ReadAllBytes(args[1]);
 SMOTextureTool.Core.SmoDocument textures =
  SMOTextureTool.Core.SmoDocument.Parse(source);
 int regressionChecks = 0;
 RunFixedSizeTextureWriterRegression(source, textures, (value, message) =>
 {
  regressionChecks++;
  if (!value)
   throw new InvalidOperationException("FAIL: " + message);
 });
 Console.WriteLine(
  $"FIXED TEXTURE WRITER PASS: {regressionChecks} assertions; " +
  $"source={Path.GetFullPath(args[1])}");
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
 Console.Error.WriteLine("       SmoImporter.FormatTests --skinned-glb <target.smo> <donor.glb> <output.smo> [texture.png]");
 Console.Error.WriteLine("       SmoImporter.FormatTests --skinned-fbx <target.smo> <donor.fbx> <output.smo> [texture.png]");
 Console.Error.WriteLine("       SmoImporter.FormatTests --skinned-glb-diagnostics <target.smo> <donor.glb> <output-dir>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --rigid-multitexture <target.smo> <donor.glb|obj|fbx> <output.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --rigid-texture-bundle <donor.glb|obj|fbx> [texture-folder]");
 Console.Error.WriteLine("       SmoImporter.FormatTests --rigid-texture-resize-regression <donor.glb|obj|fbx>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --scan-texture-metadata <media-directory>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --dump-texture-sequences <sample.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --fixed-texture-writer-regression <sample.smo>");
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
 Check(imported.Meshes.Where(mesh => mesh.Skinning is not null).All(mesh =>
  mesh.Skinning!.Skeleton.JointNames.Distinct(StringComparer.Ordinal).Count() ==
  mesh.Skinning.Skeleton.JointNames.Count),
  "repeated GLB palette padding is canonicalized to unique joints");
 Check(exported.Meshes.Zip(imported.Meshes).All(pair =>
 {
  if (pair.First.SkinObjectIndex is not int skinObjectIndex)
   return pair.Second.Skinning is null;
  SmoExportSkin sourceSkin = exported.Skins.Single(skin => skin.ObjectIndex == skinObjectIndex);
  return pair.Second.Skinning is not null &&
   pair.Second.Skinning.Skeleton.JointNames.Count ==
   sourceSkin.JointObjectIndices.Distinct().Count();
 }), "canonical GLB joint count matches unique source palette nodes");
 Check(imported.Meshes.Where(mesh => mesh.Skinning is not null).All(mesh =>
 {
  int jointCount = mesh.Skinning!.Skeleton.JointNames.Count;
  return mesh.Skinning.JointIndices.All(joints =>
   joints.X < jointCount && joints.Y < jointCount &&
   joints.Z < jointCount && joints.W < jointCount);
 }), "remapped GLB vertex joints stay inside the canonical palette");
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

static void RunFixedSizeTextureWriterRegression(
 byte[] source,
 SMOTextureTool.Core.SmoDocument document,
 Action<bool, string> check)
{
 SMOTextureTool.Core.TextureInfo? texture = document.Textures
  .Where(item => item.FormatCode is 0x32E3 or 0x43E3)
  .OrderBy(item => item.PixelDataSize)
  .FirstOrDefault();
 check(texture is not null, "a 0x32E3/0x43E3 texture exists for fixed-size writer tests");
 if (texture is null)
  return;

 int markerOffset = checked(texture.BlockOffset + 0x3C);
 check(texture.Layout == SMOTextureTool.Core.TextureLayout.Bgra,
  "0x32E3/0x43E3 fixed-size texture uses BGRA layout");
 check(texture.PixelDataOffset == texture.BlockOffset + 0x3D,
  "0x32E3/0x43E3 pixel payload starts at +0x3D");
 check(source[markerOffset] == 0, "serializer marker at +0x3C is 00");

 var first = new Rgba32(17, 34, 51, 68);
 var last = new Rgba32(85, 102, 119, 136);
 using var replacement = new Image<Rgba32>(texture.Width, texture.Height, first);
 replacement[texture.Width - 1, texture.Height - 1] = last;
 using var png = new MemoryStream();
 replacement.SaveAsPng(png);
 byte[] imageData = png.ToArray();

 byte[] rgb = FixedSizeTextureWriter.ReplaceRgb(source, texture.Index, imageData);
 check(rgb[markerOffset] == source[markerOffset],
  "RGB replacement keeps serializer marker unchanged");
 CheckRawBgra(rgb, texture.PixelDataOffset, first, preserveAlpha: true,
  source[texture.PixelDataOffset + 3], check, "RGB first pixel");
 int lastOffset = checked(texture.PixelDataOffset + texture.PixelDataSize - 4);
 CheckRawBgra(rgb, lastOffset, last, preserveAlpha: true,
  source[lastOffset + 3], check, "RGB last pixel");
 bool alphaPreserved = true;
 for (int offset = texture.PixelDataOffset;
      alphaPreserved && offset < texture.PixelDataOffset + texture.PixelDataSize;
      offset += 4)
  alphaPreserved = rgb[offset + 3] == source[offset + 3];
 check(alphaPreserved, "RGB replacement preserves every host Alpha byte");
 CheckOutsidePixelPayloadUnchanged(source, rgb, texture, check, "RGB replacement");

 byte[] rgba = FixedSizeTextureWriter.ReplaceRgbaDiagnosticUnsafe(
  source, texture.Index, imageData);
 check(rgba[markerOffset] == source[markerOffset],
  "full BGRA replacement keeps serializer marker unchanged");
 CheckRawBgra(rgba, texture.PixelDataOffset, first, preserveAlpha: false,
  0, check, "full BGRA first pixel");
 CheckRawBgra(rgba, lastOffset, last, preserveAlpha: false,
  0, check, "full BGRA last pixel");
 CheckOutsidePixelPayloadUnchanged(source, rgba, texture, check, "full BGRA replacement");
}

static void CheckRawBgra(
 byte[] data,
 int offset,
 Rgba32 expected,
 bool preserveAlpha,
 byte preservedAlpha,
 Action<bool, string> check,
 string context)
{
 check(data[offset] == expected.B &&
       data[offset + 1] == expected.G &&
       data[offset + 2] == expected.R,
  $"{context} stores B, G and R at +0, +1 and +2");
 check(data[offset + 3] == (preserveAlpha ? preservedAlpha : expected.A),
  $"{context} stores the expected Alpha at +3");
}

static void CheckOutsidePixelPayloadUnchanged(
 byte[] source,
 byte[] output,
 SMOTextureTool.Core.TextureInfo texture,
 Action<bool, string> check,
 string context)
{
 int payloadEnd = checked(texture.PixelDataOffset + texture.PixelDataSize);
 bool unchanged = source.Length == output.Length;
 for (int index = 0; unchanged && index < source.Length; index++)
  if (index < texture.PixelDataOffset || index >= payloadEnd)
   unchanged = source[index] == output[index];
 check(unchanged, $"{context} changes no bytes outside the pixel payload");
}
