using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoExporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Core;

internal sealed record SmoWingVisualMesh(uint MeshId, uint NodeId, uint MaterialId, uint TextureId,
    ImportedMesh Source, byte[] Bgra, int Width, int Height, bool Alpha);

internal sealed record SmoWingVisualGraphAppendResult(byte[] Data, IReadOnlyList<SmoWingVisualMesh> Meshes,
    IReadOnlySet<uint> RelocatedBodyTextureIds);

/// <summary>Serializes selected rigid wing surfaces as children of the animated native wing nodes.</summary>
internal static class SmoWingVisualGraphAppender
{
    private sealed record Built(uint Id, byte[] Data, IReadOnlyList<SmoVisualForestEntry> Entries);
    private sealed record WingTexture(Built Object, byte[] Bgra, int Width, int Height);
    private static readonly Matrix4x4 Reflection = Matrix4x4.CreateScale(1, 1, -1);
    private static readonly uint[] AlphaStates = [0, 0, 1, 0, 1, 0, 3, 0, 2, 0, 6];
    private static readonly uint[] TextureStates = [0, 3, 3, 0, 0, 0xFF000000, 2, 0, 0];
    private const float Epsilon = 0.0002f;

    internal static SmoWingVisualGraphAppendResult Append(SmoDocument originalTarget, SmoDocument bodyOutput,
        SmoWingAttachmentDraft draft, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        SmoProductionPlatformGuard.EnsurePcWritable(bodyOutput);
        if (bodyOutput.HasErrors ||
            (uint)draft.SpineObjectIndex >= originalTarget.Objects.Count)
            throw new InvalidDataException("The wing target is not a supported PC character.");
        SmoObjectEntry originalSpine = originalTarget.Objects[draft.SpineObjectIndex];
        if (originalSpine.Name != "Spine_03" || originalSpine.TypeHash != SmoClassIds.Node)
            throw new InvalidDataException("The wing attachment has no exact native spine owner.");
        SmoObjectEntry spine = bodyOutput.Objects.Single(entry => entry.Id == originalSpine.Id && entry.TypeHash == SmoClassIds.Node);
        var template = SelectMaterialTemplate(originalTarget);
        uint nextId = Math.Max(originalTarget.Objects.Max(entry => entry.Id), bodyOutput.Objects.Max(entry => entry.Id));
        var resourceNames = originalTarget.Objects.Concat(bodyOutput.Objects).Select(entry => entry.Name)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        var current = bodyOutput;
        var expected = new List<SmoWingVisualMesh>();
        var textures = new Dictionary<string, WingTexture>(StringComparer.Ordinal);
        var textureIdsByPixels = new Dictionary<string, uint>(StringComparer.Ordinal);
        var bodyTextureIds = bodyOutput.Objects.Where(entry => entry.TypeHash == SmoClassIds.TextureData)
            .Select(entry => entry.Id).ToHashSet();
        var relocatedBodyTextureIds = new HashSet<uint>();
        foreach (SmoObjectEntry entry in bodyOutput.Objects.Where(entry => entry.TypeHash == SmoClassIds.TextureData))
        {
            if (!SmoTextureDecoder.TryDecode(bodyOutput, entry, out SmoTexture? existing, out string issue) || existing is null)
                throw new InvalidDataException($"The body texture {entry.Id} cannot be reused: {issue}");
            textureIdsByPixels.TryAdd(TexturePixelKey(existing.Width, existing.Height, existing.Bgra32Pixels.Span), entry.Id);
        }
        foreach (SmoWingRigidAttachment side in new[] { draft.Left, draft.Right })
        {
            cancellationToken.ThrowIfCancellationRequested();
            SmoObjectEntry[] matches = current.Objects.Where(entry => IsWingNode(entry) &&
                string.Equals(entry.Name, side.Reference.NodeName, StringComparison.Ordinal)).ToArray();
            uint nodeId;
            if (matches.Length == 0)
            {
                Built node;
                if (side.Reference.NodeObjectIndex is int originalIndex)
                {
                    SmoObjectEntry originalNode = originalTarget.Objects[originalIndex];
                    nodeId = originalNode.Id;
                    if (current.Objects.Any(entry => entry.Id == nodeId))
                        throw new InvalidDataException("The original wing node ID was reused during body serialization.");
                    RequireParent(originalTarget, nodeId, originalSpine.Id);
                    node = CopyNodeScalars(originalTarget, originalNode);
                }
                else
                {
                    nodeId = checked(++nextId);
                    node = BuildNode(nodeId, side.Reference.NodeName, side.Reference.NodeLocalToSpine);
                }
                current = InsertNodeChild(current, spine.Id, node);
            }
            else
            {
                if (matches is not [var node] || side.Reference.NodeObjectIndex is not int originalNode ||
                    originalTarget.Objects[originalNode].Id != node.Id ||
                    originalTarget.Objects[originalNode].TypeHash != node.TypeHash)
                    throw new InvalidDataException("The wing node identity is missing or ambiguous.");
                nodeId = node.Id;
                RequireParent(current, nodeId, spine.Id);
            }

            bool nativeRenderNode = current.Objects.Single(entry => entry.Id == nodeId).TypeHash == SmoClassIds.RenderNode;
            ImportedScene prepared = SmoLevelRigidImportPreparer.Prepare(side.LocalScene);
            foreach (ImportedMesh mesh in prepared.Meshes)
            {
                cancellationToken.ThrowIfCancellationRequested();
                uint modelId = checked(++nextId), materialId = checked(++nextId);
                uint meshId = checked(++nextId);
                ImportedTexture? texture = ResolveTexture(prepared, mesh);
                string? textureKey = texture is null ? null : $"{texture.Width}x{texture.Height}:" + Convert.ToHexString(SHA256.HashData(texture.Data));
                bool defineTexture = false;
                WingTexture? serializedTexture = null;
                if (texture is not null && !textures.TryGetValue(textureKey!, out serializedTexture))
                {
                    using Image<Rgba32> image = Image.Load<Rgba32>(texture.Data);
                    if (image.Width != texture.Width || image.Height != texture.Height)
                        throw new InvalidDataException("The wing texture dimensions differ from its image data.");
                    byte[] pixels = ToBgra(image);
                    string pixelKey = TexturePixelKey(image.Width, image.Height, pixels);
                    Built textureObject;
                    if (textureIdsByPixels.TryGetValue(pixelKey, out uint sharedTextureId))
                        textureObject = new Built(sharedTextureId, [], []);
                    else
                    {
                        uint textureId = checked(++nextId);
                        textureObject = Leaf(textureId, AllocateResourceName("imp_t", textureId, resourceNames), SmoClassIds.TextureData,
                            SmoTextureDataWriter.CreateBgraObject(image.Width, image.Height, pixels));
                        textureIdsByPixels.Add(pixelKey, textureId);
                        defineTexture = true;
                    }
                    serializedTexture = new WingTexture(textureObject, pixels, image.Width, image.Height);
                    textures.Add(textureKey!, serializedTexture);
                }
                if (serializedTexture is not null && !defineTexture)
                {
                    SmoObjectEntry existing = current.Objects.Single(entry => entry.Id == serializedTexture!.Object.Id);
                    SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == nodeId);
                    if (existing.LogicalOffset >= owner.LogicalEnd)
                    {
                        // Native references must encounter their definition first.
                        // Move the identical leaf into this earlier wing branch;
                        // its old material retains an ordinary ID/zero-size reference.
                        SmoObjectEntry oldMaterial = current.Objects[existing.ParentIndex ??
                            throw new InvalidDataException("A shared wing texture has no inline material owner.")];
                        if (oldMaterial.TypeHash != SmoClassIds.MaterialData)
                            throw new InvalidDataException("A shared wing texture has an unsupported definition owner.");
                        byte[] bytes = current.Data.Span.Slice(checked((int)existing.PhysicalOffset),
                            checked((int)existing.SerializedSize)).ToArray();
                        Built relocated = new(existing.Id, bytes, [new SmoVisualForestEntry(existing.Id,
                            existing.RawName.ToArray(), existing.TypeHash, 0, checked((uint)bytes.Length))]);
                        current = SmoDocument.ParseOwned(SmoVisualForestInjector.DemoteInlineLeafToReference(
                            current, oldMaterial.Id, 10, existing.Id), current.SourcePath);
                        serializedTexture = serializedTexture with { Object = relocated };
                        textures[textureKey!] = serializedTexture;
                        defineTexture = true;
                        if (bodyTextureIds.Contains(existing.Id))
                            relocatedBodyTextureIds.Add(existing.Id);
                    }
                }
                bool alpha = SmoLevelModelGraphReplacer.RequiresTextureAlpha(prepared, mesh) ||
                    texture is null && mesh.DiffuseColors.Any(color => color >> 24 < 255);
                Built material = BuildMaterial(originalTarget, template, materialId,
                    AllocateResourceName("imp_m", materialId, resourceNames), serializedTexture?.Object, alpha, defineTexture);
                Built geometry = BuildMesh(meshId, AllocateResourceName("imp_g", meshId, resourceNames), mesh);
                Built model = BuildModel(modelId, AllocateResourceName("imp_o", modelId, resourceNames), material, geometry, alpha);
                if (nativeRenderNode)
                    current = InsertRenderable(current, nodeId, model);
                else
                {
                    uint renderId = checked(++nextId);
                    Built render = BuildRenderNode(renderId, AllocateResourceName("imp_n", renderId, resourceNames), model);
                    current = InsertNodeChild(current, nodeId, render);
                }
                expected.Add(new SmoWingVisualMesh(meshId, nodeId, materialId, serializedTexture?.Object.Id ?? 0, mesh,
                    serializedTexture?.Bgra ?? [], serializedTexture?.Width ?? 0, serializedTexture?.Height ?? 0, alpha));
            }
        }
        var result = new SmoWingVisualGraphAppendResult(current.Data.ToArray(), expected.AsReadOnly(), relocatedBodyTextureIds);
        Verify(current, draft, result);
        return result;
    }

    internal static void Verify(SmoDocument output, SmoWingAttachmentDraft draft, SmoWingVisualGraphAppendResult result)
    {
        if (output.HasErrors || result.Meshes.Count == 0)
            throw new InvalidDataException("The rigid wing graph is empty or invalid.");
        SmoExportScene scene = SmoSceneBuilder.Build(output);
        SmoObjectEntry spine = output.Objects.Single(entry => entry.TypeHash == SmoClassIds.Node && entry.Name == "Spine_03");
        foreach (SmoWingRigidAttachment side in new[] { draft.Left, draft.Right })
        {
            SmoObjectEntry node = output.Objects.Single(entry => IsWingNode(entry) && entry.Name == side.Reference.NodeName);
            RequireParent(output, node.Id, spine.Id);
            if (!SmoNodeTransformDecoder.TryDecode(output, node, out var transform) || transform is null)
                throw new InvalidDataException("The serialized wing node transform cannot be read.");
            Matrix4x4 actual = SmoExportCoordinateSystem.ToExportMatrix(transform.LocalMatrix);
            Matrix4x4 expected = side.Reference.NodeLocalToSpine;
            foreach (Vector3 point in new[] { Vector3.Zero, Vector3.UnitX, Vector3.UnitY, Vector3.UnitZ })
                if (Vector3.Distance(Vector3.Transform(point, actual), Vector3.Transform(point, expected)) > Epsilon)
                    throw new InvalidDataException("The native wing attachment transform changed during serialization.");
        }
        foreach (SmoWingVisualMesh expected in result.Meshes)
        {
            SmoObjectEntry entry = output.Objects.Single(candidate => candidate.Id == expected.MeshId);
            var placements = scene.MeshPlacements.Where(placement => placement.MeshObjectIndex == entry.Index).ToArray();
            if (placements is not [var placement] || placement.ParentNodeObjectIndex is not int parent)
                throw new InvalidDataException("A rigid wing has no unique native scene placement.");
            SmoObjectEntry parentEntry = output.Objects[parent];
            SmoObjectEntry modelEntry = output.Objects[entry.ParentIndex ?? throw new InvalidDataException("A wing mesh has no model owner.")];
            SmoObjectEntry renderEntry = output.Objects[modelEntry.ParentIndex ?? throw new InvalidDataException("A wing model has no render node owner.")];
            if (modelEntry.TypeHash != SmoClassIds.Model || renderEntry.TypeHash != SmoClassIds.RenderNode ||
                (parentEntry.Id != expected.NodeId && parentEntry.Id != renderEntry.Id))
                throw new InvalidDataException("The wing mesh has an unexpected native placement owner.");
            if (renderEntry.Id != expected.NodeId)
                RequireParent(output, renderEntry.Id, expected.NodeId);
            var mesh = scene.Meshes.Single(candidate => candidate.VariantKey == placement.EffectiveMeshKey);
            if (mesh.SkinObjectIndex is not null || mesh.Positions.Length != expected.Source.Positions.Length ||
                !mesh.TriangleIndices.SequenceEqual(expected.Source.TriangleIndices))
                throw new InvalidDataException("The selected wing was not preserved as a rigid triangle mesh.");
            for (int index = 0; index < mesh.Positions.Length; index++)
            {
                if (Vector3.Distance(mesh.Positions[index], expected.Source.Positions[index]) > Epsilon)
                    throw new InvalidDataException("The rigid wing local positions changed during serialization.");
                if (expected.Source.TextureCoordinates.Length == mesh.Positions.Length &&
                    (mesh.TextureCoordinates0.Length != mesh.Positions.Length ||
                     Vector2.Distance(mesh.TextureCoordinates0[index], expected.Source.TextureCoordinates[index]) > Epsilon))
                    throw new InvalidDataException("The rigid wing UV coordinates changed during serialization.");
                if (expected.Source.Normals.Length == mesh.Positions.Length &&
                    (mesh.Normals.Length != mesh.Positions.Length ||
                     Vector3.Distance(mesh.Normals[index], expected.Source.Normals[index]) > Epsilon))
                    throw new InvalidDataException("The rigid wing normals changed during serialization.");
                if (expected.Source.SecondaryTextureCoordinates.Length == mesh.Positions.Length &&
                    (mesh.TextureCoordinates1.Length != mesh.Positions.Length ||
                     Vector2.Distance(mesh.TextureCoordinates1[index], expected.Source.SecondaryTextureCoordinates[index]) > Epsilon))
                    throw new InvalidDataException("The rigid wing secondary UV coordinates changed during serialization.");
            }
            SmoMesh decodedMesh = SmoMeshDecoder.Decode(output, entry);
            if (expected.Source.DiffuseColors.Length != 0 && !decodedMesh.DiffuseColorsArgb.SequenceEqual(expected.Source.DiffuseColors))
                throw new InvalidDataException("The rigid wing vertex colors changed during serialization.");
            var loaded = mesh.LoadedMaterial;
            var texture = loaded?.Passes.SingleOrDefault()?.Layers.SingleOrDefault()?.Texture;
            if (loaded is null || loaded.ObjectId != expected.MaterialId)
                throw new InvalidDataException($"Wing mesh {expected.MeshId}: loaded material ID {loaded?.ObjectId.ToString() ?? "missing"}, expected {expected.MaterialId}.");
            if (expected.TextureId == 0)
            {
                if (texture is not null || loaded.VertexAlpha != 1)
                    throw new InvalidDataException($"Wing mesh {expected.MeshId}: an untextured source acquired a texture or lost vertex alpha.");
            }
            else
            {
                if (texture is null || texture.ObjectId != expected.TextureId)
                    throw new InvalidDataException($"Wing mesh {expected.MeshId}: loaded texture ID {texture?.ObjectId.ToString() ?? "missing"}, expected {expected.TextureId}.");
                if (texture.Texture is not { } decoded)
                    throw new InvalidDataException($"Wing mesh {expected.MeshId}: texture {expected.TextureId} has no decoded pixels: {texture.Issue ?? "no decoder issue reported"}.");
                if (decoded.Width != expected.Width || decoded.Height != expected.Height)
                    throw new InvalidDataException($"Wing mesh {expected.MeshId}: texture {expected.TextureId} dimensions {decoded.Width}x{decoded.Height}, expected {expected.Width}x{expected.Height}.");
                if (!decoded.Bgra32Pixels.Span.SequenceEqual(expected.Bgra))
                {
                    ReadOnlySpan<byte> actualPixels = decoded.Bgra32Pixels.Span;
                    int commonLength = Math.Min(actualPixels.Length, expected.Bgra.Length);
                    int firstMismatch = 0;
                    while (firstMismatch < commonLength && actualPixels[firstMismatch] == expected.Bgra[firstMismatch])
                        firstMismatch++;
                    string actualByte = firstMismatch < actualPixels.Length ? actualPixels[firstMismatch].ToString() : "end";
                    string expectedByte = firstMismatch < expected.Bgra.Length ? expected.Bgra[firstMismatch].ToString() : "end";
                    throw new InvalidDataException($"Wing mesh {expected.MeshId}: texture {expected.TextureId} BGRA mismatch at byte {firstMismatch}, actual {actualByte}, expected {expectedByte}; lengths {actualPixels.Length}/{expected.Bgra.Length}.");
                }
            }
            if (placement.MaterialObjectIndex != loaded.ObjectIndex)
                throw new InvalidDataException($"Wing mesh {expected.MeshId}: placement material index {placement.MaterialObjectIndex}, expected loaded material index {loaded.ObjectIndex}.");
            if (expected.Alpha && (loaded.Passes[0].Blend != 2 || !loaded.RenderStates.SequenceEqual(AlphaStates)))
                throw new InvalidDataException("The rigid wing transparency state failed native readback.");
        }
    }

    private static SmoObjectEntry SelectMaterialTemplate(SmoDocument target)
    {
        var loaded = SmoLoadedResources.Get(target);
        foreach (var material in loaded.Models.Values.Select(model => model.Material).Where(material => material is not null).Distinct())
        {
            if (material!.ColorControllerId != 0 || material.Passes.Count != 1 ||
                material.Passes[0].Layers is not [var layer] || layer.ClassId != SmoClassIds.StandardLayer ||
                layer.Animation is not null || layer.UvControllerId != 0 || layer.Texture is null)
                continue;
            SmoObjectEntry entry = target.Objects[material.ObjectIndex];
            if (target.Objects.Any(child => child.ParentIndex == entry.Index && child.TypeHash != SmoClassIds.TextureData))
                continue;
            return entry;
        }
        throw new NotSupportedException("The character has no supported single-standard-layer material template for rigid wings.");
    }

    private static Built BuildMaterial(SmoDocument source, SmoObjectEntry template, uint id, string name, Built? texture,
        bool alpha, bool defineTexture)
    {
        SmoLoadedMaterial original = SmoLoadedResources.Get(source).Models.Values.Select(model => model.Material)
            .First(material => material?.ObjectIndex == template.Index)!;
        uint[] states = alpha ? AlphaStates : original.RenderStates.ToArray();
        byte[] data = SmoMaterialScalarWriter.PatchSingleStandardLayer(source, template, states, alpha ? 2u : 0u, TextureStates);
        using var stream = new MemoryStream();
        stream.Write(data.AsSpan(0, 8));
        if (texture is null) stream.Write(SmoDataBlockWriter.BuildField(1, new byte[] { 1 }));
        var entries = new List<SmoVisualForestEntry>();
        int offset = 8, textures = 0;
        while (offset < data.Length && SmoDataBlockReader.TryReadHeader(data, offset, out var field))
        {
            if (field.RawHeader != 0 && field.FieldType == 10)
            {
                textures++;
                if (texture is not null && defineTexture)
                    WriteInline(stream, 10, texture, entries);
                else if (texture is not null)
                {
                    byte[] reference = new byte[8];
                    BinaryPrimitives.WriteUInt32LittleEndian(reference, texture.Id);
                    stream.Write(SmoDataBlockWriter.BuildField(10, reference));
                }
            }
            else if (texture is not null || field.FieldType != 1)
                stream.Write(data.AsSpan(field.Offset, checked((int)field.PayloadEnd - field.Offset)));
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != data.Length || textures != 1)
            throw new InvalidDataException("The native wing material template has ambiguous texture fields.");
        return Finish(id, name, SmoClassIds.MaterialData, stream, entries);
    }

    private static Built BuildMesh(uint id, string name, ImportedMesh mesh)
    {
        Vector3[] positions = mesh.Positions.Select(point => new Vector3(point.X, point.Y, -point.Z)).ToArray();
        Vector3[] normals = mesh.Normals.Length == positions.Length
            ? mesh.Normals.Select(normal => new Vector3(normal.X, normal.Y, -normal.Z)).ToArray()
            : Enumerable.Repeat(Vector3.UnitY, positions.Length).ToArray();
        Vector2[] uv = mesh.TextureCoordinates.Length == positions.Length ? mesh.TextureCoordinates : new Vector2[positions.Length];
        uint[] colors = mesh.DiffuseColors.Length == positions.Length ? mesh.DiffuseColors : Enumerable.Repeat(0xFFFFFFFFu, positions.Length).ToArray();
        uint[] triangles = mesh.TriangleIndices.ToArray();
        for (int index = 0; index < triangles.Length; index += 3)
            (triangles[index + 1], triangles[index + 2]) = (triangles[index + 2], triangles[index + 1]);
        SmoMesh rigid = SmoMesh.CreateRigidTransient(mesh.Name, positions, normals, uv, mesh.SecondaryTextureCoordinates, colors, triangles);
        return Leaf(id, name, SmoClassIds.MeshData, SmoMeshDataWriter.CreateTriangleList(rigid));
    }

    private static Built BuildModel(uint id, string name, Built material, Built mesh, bool alpha)
    {
        using var stream = BeginObject(SmoClassIds.Model);
        var entries = new List<SmoVisualForestEntry>();
        WriteInline(stream, 0, material, entries);
        stream.Write(SmoDataBlockWriter.BuildField(2, UInt32(alpha ? 1u : 0u)));
        stream.Write(SmoDataBlockWriter.BuildField(3, UInt32(1)));
        stream.WriteByte(0);
        WriteInline(stream, 0, mesh, entries);
        stream.WriteByte(0);
        return Finish(id, name, SmoClassIds.Model, stream, entries);
    }

    private static Built BuildRenderNode(uint id, string name, Built model)
    {
        using var stream = BeginObject(SmoClassIds.RenderNode);
        var entries = new List<SmoVisualForestEntry>();
        stream.WriteByte(0);
        WriteInline(stream, 0, model, entries);
        stream.WriteByte(0);
        return Finish(id, name, SmoClassIds.RenderNode, stream, entries);
    }

    private static Built BuildNode(uint id, string name, Matrix4x4 externalLocal)
    {
        Matrix4x4 local = Reflection * externalLocal * Reflection;
        if (!Matrix4x4.Decompose(local, out var scale, out var rotation, out var translation) ||
            scale.X <= 0 || scale.Y <= 0 || scale.Z <= 0)
            throw new InvalidDataException("The native wing attachment matrix cannot be serialized as a node transform.");
        using var stream = BeginObject(SmoClassIds.Node);
        stream.Write(SmoDataBlockWriter.BuildField(0, Floats(translation.X, translation.Y, translation.Z)));
        stream.Write(SmoDataBlockWriter.BuildField(1, Floats(rotation.X, rotation.Y, rotation.Z, rotation.W)));
        stream.Write(SmoDataBlockWriter.BuildField(2, Floats(scale.X, scale.Y, scale.Z)));
        stream.Write(SmoDataBlockWriter.BuildField(8, new byte[] { 1 }));
        stream.WriteByte(0);
        return Finish(id, name, SmoClassIds.Node, stream, []);
    }

    private static Built CopyNodeScalars(SmoDocument original, SmoObjectEntry node)
    {
        if (!IsWingNode(node))
            throw new InvalidDataException($"Original wing node [{node.Index}] '{node.Name}' has class 0x{node.TypeHash:X8}.");
        if (!SmoObjectFieldReader.TryRead(original, node, out var fields, out string error))
            throw new InvalidDataException($"Original wing node [{node.Index}] '{node.Name}' field stream: {error}");
        ReadOnlySpan<byte> data = original.Data.Span.Slice(checked((int)node.PhysicalOffset), checked((int)node.SerializedSize));
        using var stream = new MemoryStream();
        stream.Write(data[..8]);
        for (int index = 0; index < fields.Count; index++)
        {
            SmoObjectField field = fields[index];
            SmoSerializedFieldRegistry.TryDescribeField(node.TypeHash, fields, index, out var descriptor);
            if (descriptor?.Key is "node.child" or "render_node.renderable")
                continue;
            if (descriptor?.Key == "node.collision")
                throw new NotSupportedException("Original wing collision branches cannot be transferred to a replacement surface.");
            stream.Write(original.Data.Span.Slice(field.AbsoluteHeaderOffset, field.EncodedSize));
        }
        byte[] copied = stream.ToArray();
        return new Built(node.Id, copied, [new SmoVisualForestEntry(node.Id, node.RawName.ToArray(),
            node.TypeHash, 0, checked((uint)copied.Length))]);
    }

    private static bool IsWingNode(SmoObjectEntry entry) => entry.TypeHash is SmoClassIds.Node or SmoClassIds.RenderNode;

    private static string AllocateResourceName(string prefix, uint id, ISet<string> names)
    {
        // The native texture manager caches resources by name. Distinct donor
        // surfaces may share a source name while carrying different image data.
        string name = $"{prefix}_{id:X8}";
        if (names.Add(name))
            return name;
        for (int suffix = 1; ; suffix = checked(suffix + 1))
        {
            string candidate = $"{name}_{suffix}";
            if (names.Add(candidate))
                return candidate;
        }
    }

    private static SmoDocument InsertNodeChild(SmoDocument current, uint ownerId, Built child)
    {
        using var stream = new MemoryStream();
        var entries = new List<SmoVisualForestEntry>();
        WriteInline(stream, 5, child, entries);
        var attachment = new SmoVisualForestAttachment(ownerId, stream.ToArray(), entries);
        SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == ownerId);
        if (owner.TypeHash != SmoClassIds.Node ||
            !SmoObjectFieldReader.TryRead(current, owner, out var fields, out _))
            throw new InvalidDataException("The native wing node field stream is invalid.");
        bool hasChildren = fields.Any(field => field.RawHeader != 0 && field.FieldType == 5);
        byte[] data = hasChildren
            ? SmoVisualForestInjector.InjectAfterLastFieldType(current, ownerId, 5, [attachment])
            : SmoVisualForestInjector.Inject(current, ownerId, [attachment]);
        var result = SmoDocument.ParseOwned(data, current.SourcePath);
        if (result.HasErrors)
            throw new InvalidDataException("The native wing branch failed structural readback.");
        return result;
    }

    private static SmoDocument InsertRenderable(SmoDocument current, uint ownerId, Built model)
    {
        SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == ownerId);
        if (!SmoRenderNodeDecoder.TryDecode(current, owner, out var renderNode) || renderNode is null)
            throw new InvalidDataException("The native wing render-node field stream is invalid.");
        using var stream = new MemoryStream();
        var entries = new List<SmoVisualForestEntry>();
        WriteInline(stream, 0, model, entries);
        var attachment = new SmoVisualForestAttachment(ownerId, stream.ToArray(), entries);
        // A RenderNode's final section owns renderable field 0. Its inherited
        // node-child field 5 belongs before the earlier, base-node terminator.
        byte[] data = SmoVisualForestInjector.Inject(current, ownerId, [attachment]);
        var result = SmoDocument.ParseOwned(data, current.SourcePath);
        if (result.HasErrors)
            throw new InvalidDataException("The native wing renderable failed structural readback.");
        return result;
    }

    private static void RequireParent(SmoDocument document, uint childId, uint parentId)
    {
        var hierarchy = SmoNodeHierarchy.Decode(document);
        int child = document.Objects.Single(entry => entry.Id == childId).Index;
        int parent = document.Objects.Single(entry => entry.Id == parentId).Index;
        if (!hierarchy.ParentsByChild.TryGetValue(child, out var parents) || parents is not [var actual] || actual != parent)
            throw new InvalidDataException("The rigid wing has a different native node owner.");
    }

    private static ImportedTexture? ResolveTexture(ImportedScene scene, ImportedMesh mesh)
    {
        int texture = (uint)mesh.MaterialIndex < scene.Materials.Count ? scene.Materials[mesh.MaterialIndex].BaseColorTextureIndex : -1;
        if (texture >= 0)
            return (uint)texture < scene.Textures.Count ? scene.Textures[texture] :
                throw new InvalidDataException("The wing material references a missing donor texture.");
        return null;
    }

    private static byte[] ToBgra(Image<Rgba32> image)
    {
        var pixels = new Rgba32[checked(image.Width * image.Height)];
        image.CopyPixelDataTo(pixels);
        var result = new byte[checked(pixels.Length * 4)];
        for (int index = 0; index < pixels.Length; index++)
        {
            result[index * 4] = pixels[index].B;
            result[index * 4 + 1] = pixels[index].G;
            result[index * 4 + 2] = pixels[index].R;
            result[index * 4 + 3] = pixels[index].A;
        }
        return result;
    }

    private static string TexturePixelKey(int width, int height, ReadOnlySpan<byte> pixels) =>
        $"{width}x{height}:" + Convert.ToHexString(SHA256.HashData(pixels));

    private static MemoryStream BeginObject(uint type)
    {
        var stream = new MemoryStream();
        stream.Write(UInt32(type));
        stream.Write("SBOO"u8);
        return stream;
    }

    private static Built Leaf(uint id, string name, uint type, byte[] data) =>
        new(id, data, [new SmoVisualForestEntry(id, RawName(name), type, 0, checked((uint)data.Length))]);

    private static Built Finish(uint id, string name, uint type, MemoryStream stream, List<SmoVisualForestEntry> children)
    {
        byte[] data = stream.ToArray();
        children.Insert(0, new SmoVisualForestEntry(id, RawName(name), type, 0, checked((uint)data.Length)));
        return new Built(id, data, children.AsReadOnly());
    }

    private static void WriteInline(MemoryStream stream, int field, Built child, List<SmoVisualForestEntry> entries)
    {
        // A small newly inserted wing shell will later receive complete models
        // and textures. The common injector preserves ancestor header widths.
        stream.Write(SmoDataBlockWriter.BuildReservedHeader(field, checked((uint)(8 + child.Data.Length)),
            SmoDataBlockSizeCode.UInt32));
        stream.Write(UInt32(child.Id));
        stream.Write(UInt32(checked((uint)child.Data.Length)));
        int shift = checked((int)stream.Position);
        stream.Write(child.Data);
        entries.AddRange(child.Entries.Select(entry => entry with { RelativeOffset = checked(entry.RelativeOffset + shift) }));
    }

    private static byte[] RawName(string name) => Encoding.UTF8.GetBytes(name + "\0");
    private static byte[] UInt32(uint value)
    {
        byte[] result = new byte[4];
        BinaryPrimitives.WriteUInt32LittleEndian(result, value);
        return result;
    }
    private static byte[] Floats(params float[] values)
    {
        byte[] result = new byte[checked(values.Length * 4)];
        for (int index = 0; index < values.Length; index++)
            BinaryPrimitives.WriteSingleLittleEndian(result.AsSpan(index * 4), values[index]);
        return result;
    }
}
