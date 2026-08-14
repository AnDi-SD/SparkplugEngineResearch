using System.Buffers.Binary;
using System.Numerics;
using System.Text.Json;
using SixLabors.ImageSharp;

namespace SmoImporter.Core;

public static class GlbModelReader
{
    public static ImportedScene Read(string path)
    {
        byte[] file = File.ReadAllBytes(path);
        if (file.Length < 20 || BinaryPrimitives.ReadUInt32LittleEndian(file) != 0x46546C67 ||
            BinaryPrimitives.ReadUInt32LittleEndian(file.AsSpan(4)) != 2)
            throw new InvalidDataException("Only binary glTF 2.0 (.glb) is supported.");
        int jsonLength = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(file.AsSpan(12)));
        using JsonDocument document = JsonDocument.Parse(file.AsMemory(20, jsonLength));
        int binaryHeader = 20 + jsonLength;
        if (binaryHeader + 8 > file.Length ||
            BinaryPrimitives.ReadUInt32LittleEndian(file.AsSpan(binaryHeader + 4)) != 0x004E4942)
            throw new InvalidDataException("GLB has no binary buffer chunk.");
        int binaryLength = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(file.AsSpan(binaryHeader)));
        ReadOnlyMemory<byte> binary = file.AsMemory(binaryHeader + 8, binaryLength);
        JsonElement root = document.RootElement;
        JsonElement meshes = root.GetProperty("meshes");
        string?[] meshNodeNames = ResolveMeshNodeNames(root, meshes.GetArrayLength());
        Matrix4x4[] meshTransforms = ResolveMeshTransforms(root, meshes.GetArrayLength());
        int?[] meshSkins = ResolveMeshSkins(root, meshes.GetArrayLength());
        GlbSkinLayout[] skins = ReadSkins(root, binary);
        var result = new List<ImportedMesh>();

        for (int meshIndex = 0; meshIndex < meshes.GetArrayLength(); meshIndex++)
        {
            JsonElement mesh = meshes[meshIndex];
            string baseName = meshNodeNames[meshIndex] ??
                (mesh.TryGetProperty("name", out JsonElement name)
                    ? name.GetString() ?? $"mesh_{meshIndex}"
                    : $"mesh_{meshIndex}");
            int primitiveIndex = 0;
            foreach (JsonElement primitive in mesh.GetProperty("primitives").EnumerateArray())
            {
                int mode = primitive.TryGetProperty("mode", out JsonElement modeElement)
                    ? modeElement.GetInt32() : 4;
                if (mode != 4) throw new InvalidDataException("Only glTF TRIANGLES primitives are supported.");
                JsonElement attributes = primitive.GetProperty("attributes");
                Vector3[] positions = ReadVector3(root, binary, attributes.GetProperty("POSITION").GetInt32());
                Vector3[] normals = attributes.TryGetProperty("NORMAL", out JsonElement normal)
                    ? ReadVector3(root, binary, normal.GetInt32()) : [];
                Vector2[] uvs = attributes.TryGetProperty("TEXCOORD_0", out JsonElement uv)
                    ? ReadVector2(root, binary, uv.GetInt32()) : [];
                uint[] colors = attributes.TryGetProperty("COLOR_0", out JsonElement color)
                    ? ReadColorsArgb(root, binary, color.GetInt32()) : [];
                uint[] indices = primitive.TryGetProperty("indices", out JsonElement index)
                    ? ReadIndices(root, binary, index.GetInt32())
                    : Enumerable.Range(0, positions.Length).Select(value => (uint)value).ToArray();
                Matrix4x4 transform = meshTransforms[meshIndex];
                ImportedSkinning? skinning = null;
                bool hasJoints = attributes.TryGetProperty("JOINTS_0", out JsonElement joints);
                bool hasWeights = attributes.TryGetProperty("WEIGHTS_0", out JsonElement weights);
                if (hasJoints != hasWeights)
                    throw new InvalidDataException(
                        $"Mesh {meshIndex} primitive {primitiveIndex} must contain both JOINTS_0 and WEIGHTS_0.");
                if (hasJoints)
                {
                    int skinIndex = meshSkins[meshIndex] ?? throw new InvalidDataException(
                        $"Mesh {meshIndex} contains skin attributes but its node has no skin.");
                    if ((uint)skinIndex >= (uint)skins.Length)
                        throw new InvalidDataException($"Mesh {meshIndex} references invalid skin {skinIndex}.");
                    GlbSkinLayout skin = skins[skinIndex];
                    ImportedJointIndices[] jointValues = RemapJointIndices(
                        ReadJointIndices(root, binary, joints.GetInt32()),
                        skin.SourceToCanonicalJoint,
                        skinIndex);
                    Vector4[] weightValues = ReadVector4(
                        root, binary, weights.GetInt32());
                    if (jointValues.Length != positions.Length || weightValues.Length != positions.Length)
                        throw new InvalidDataException("Skin attribute count differs from POSITION count.");
                    skinning = new ImportedSkinning(skin.Skeleton, jointValues, weightValues);
                }
                if (skinning is null)
                {
                    positions = positions.Select(value => Vector3.Transform(value, transform)).ToArray();
                    if (normals.Length == positions.Length)
                    {
                        Matrix4x4 normalTransform = transform;
                        if (Matrix4x4.Invert(transform, out Matrix4x4 inverse))
                            normalTransform = Matrix4x4.Transpose(inverse);
                        normals = normals.Select(value => Vector3.Normalize(
                            Vector3.TransformNormal(value, normalTransform))).ToArray();
                    }
                }
                result.Add(new ImportedMesh(
                    primitiveIndex == 0 ? baseName : $"{baseName}_{primitiveIndex}",
                    positions, normals, uvs, indices, colors,
                    primitive.TryGetProperty("material", out JsonElement material)
                        ? material.GetInt32() : -1,
                    skinning));
                primitiveIndex++;
            }
        }
        return new ImportedScene(
            result,
            ReadEmbeddedBaseColorTextures(root, binary),
            ReadMaterials(root));
    }

    private static IReadOnlyList<ImportedMaterial> ReadMaterials(JsonElement root)
    {
        if (!root.TryGetProperty("materials", out JsonElement materials))
            return [];
        bool hasTextures = root.TryGetProperty("textures", out JsonElement textures);
        bool hasImages = root.TryGetProperty("images", out JsonElement images);
        var result = new ImportedMaterial[materials.GetArrayLength()];
        for (int materialIndex = 0; materialIndex < result.Length; materialIndex++)
        {
            JsonElement material = materials[materialIndex];
            string name = material.TryGetProperty("name", out JsonElement materialName) &&
                          !string.IsNullOrWhiteSpace(materialName.GetString())
                ? materialName.GetString()!
                : $"material_{materialIndex}";
            string? textureName = null;
            if (hasTextures && hasImages &&
                TryGetColorTextureIndex(material, out int texture))
            {
                if ((uint)texture < (uint)textures.GetArrayLength() &&
                    textures[texture].TryGetProperty("source", out JsonElement source))
                {
                    int image = source.GetInt32();
                    if ((uint)image < (uint)images.GetArrayLength())
                    {
                        JsonElement imageElement = images[image];
                        textureName = imageElement.TryGetProperty("uri", out JsonElement uri) &&
                                      !string.IsNullOrWhiteSpace(uri.GetString())
                            ? Path.GetFileName(uri.GetString())
                            : imageElement.TryGetProperty("name", out JsonElement imageName) &&
                              !string.IsNullOrWhiteSpace(imageName.GetString())
                                ? imageName.GetString()
                                : null;
                    }
                }
            }
            result[materialIndex] = new ImportedMaterial(name, textureName);
        }
        return result;
    }

    private static GlbSkinLayout[] ReadSkins(
        JsonElement root,
        ReadOnlyMemory<byte> binary)
    {
        if (!root.TryGetProperty("skins", out JsonElement skins))
            return [];
        if (!root.TryGetProperty("nodes", out JsonElement nodes))
            throw new InvalidDataException("GLB skins require nodes.");
        var result = new GlbSkinLayout[skins.GetArrayLength()];
        for (int skinIndex = 0; skinIndex < result.Length; skinIndex++)
        {
            JsonElement skin = skins[skinIndex];
            if (!skin.TryGetProperty("joints", out JsonElement joints) ||
                !skin.TryGetProperty("inverseBindMatrices", out JsonElement inverseBind))
                throw new InvalidDataException(
                    $"Skin {skinIndex} must contain joints and inverseBindMatrices.");
            int[] nodeIndices = joints.EnumerateArray().Select(value =>
            {
                int nodeIndex = value.GetInt32();
                if ((uint)nodeIndex >= (uint)nodes.GetArrayLength())
                    throw new InvalidDataException($"Skin {skinIndex} references invalid node {nodeIndex}.");
                return nodeIndex;
            }).ToArray();
            string[] names = nodeIndices.Select(nodeIndex =>
            {
                string? name = nodes[nodeIndex].TryGetProperty("name", out JsonElement nodeName)
                    ? nodeName.GetString() : null;
                return string.IsNullOrWhiteSpace(name)
                    ? throw new InvalidDataException(
                        $"Skin {skinIndex} joint node {nodeIndex} has no name.")
                    : name;
            }).ToArray();
            Matrix4x4[] matrices = ReadMatrix4(root, binary, inverseBind.GetInt32());
            if (matrices.Length != names.Length)
                throw new InvalidDataException(
                    $"Skin {skinIndex} has {names.Length} joints but {matrices.Length} inverse bind matrices.");
            string name = skin.TryGetProperty("name", out JsonElement skinName)
                ? skinName.GetString() ?? $"skin_{skinIndex}"
                : $"skin_{skinIndex}";
            result[skinIndex] = CanonicalizeSkin(
                skinIndex, name, nodeIndices, names, matrices);
        }
        return result;
    }

    private static GlbSkinLayout CanonicalizeSkin(
        int skinIndex,
        string name,
        IReadOnlyList<int> sourceNodes,
        IReadOnlyList<string> sourceNames,
        IReadOnlyList<Matrix4x4> sourceMatrices)
    {
        var canonicalSlotByNode = new Dictionary<int, int>();
        var canonicalNames = new List<string>();
        var canonicalMatrices = new List<Matrix4x4>();
        var sourceToCanonical = new int[sourceNodes.Count];
        for (int sourceSlot = 0; sourceSlot < sourceNodes.Count; sourceSlot++)
        {
            int node = sourceNodes[sourceSlot];
            Matrix4x4 matrix = sourceMatrices[sourceSlot];
            if (canonicalSlotByNode.TryGetValue(node, out int canonicalSlot))
            {
                if (!canonicalMatrices[canonicalSlot].Equals(matrix))
                {
                    throw new InvalidDataException(
                        $"Skin {skinIndex} repeats joint node {node} with different " +
                        "inverse bind matrices.");
                }
                sourceToCanonical[sourceSlot] = canonicalSlot;
                continue;
            }

            canonicalSlot = canonicalNames.Count;
            canonicalSlotByNode.Add(node, canonicalSlot);
            sourceToCanonical[sourceSlot] = canonicalSlot;
            canonicalNames.Add(sourceNames[sourceSlot]);
            canonicalMatrices.Add(matrix);
        }

        if (canonicalNames.Distinct(StringComparer.Ordinal).Count() != canonicalNames.Count)
        {
            throw new InvalidDataException(
                $"Skin {skinIndex} contains duplicate joint names on distinct nodes.");
        }
        return new GlbSkinLayout(
            new ImportedSkeleton(name, canonicalNames, canonicalMatrices),
            sourceToCanonical);
    }

    private static ImportedJointIndices[] RemapJointIndices(
        IReadOnlyList<ImportedJointIndices> source,
        IReadOnlyList<int> sourceToCanonical,
        int skinIndex)
    {
        ushort Remap(ushort sourceSlot)
        {
            if (sourceSlot >= sourceToCanonical.Count)
            {
                throw new InvalidDataException(
                    $"Skin {skinIndex} vertex references joint slot {sourceSlot}, " +
                    $"but the skin contains only {sourceToCanonical.Count} joints.");
            }
            return checked((ushort)sourceToCanonical[sourceSlot]);
        }

        return source.Select(value => new ImportedJointIndices(
            Remap(value.X), Remap(value.Y), Remap(value.Z), Remap(value.W))).ToArray();
    }

    private sealed record GlbSkinLayout(
        ImportedSkeleton Skeleton,
        IReadOnlyList<int> SourceToCanonicalJoint);

    private static int?[] ResolveMeshSkins(JsonElement root, int meshCount)
    {
        var result = new int?[meshCount];
        if (!root.TryGetProperty("nodes", out JsonElement nodes))
            return result;
        foreach (JsonElement node in nodes.EnumerateArray())
        {
            if (!node.TryGetProperty("mesh", out JsonElement mesh) ||
                !node.TryGetProperty("skin", out JsonElement skin))
                continue;
            int meshIndex = mesh.GetInt32();
            if ((uint)meshIndex >= (uint)meshCount)
                throw new InvalidDataException($"Node references invalid mesh {meshIndex}.");
            int skinIndex = skin.GetInt32();
            if (result[meshIndex] is int existing && existing != skinIndex)
                throw new InvalidDataException(
                    $"Mesh {meshIndex} is instanced with different skins ({existing}, {skinIndex}).");
            result[meshIndex] = skinIndex;
        }
        return result;
    }

    private static string?[] ResolveMeshNodeNames(JsonElement root, int meshCount)
    {
        var result = new string?[meshCount];
        var owners = new int?[meshCount];
        if (!root.TryGetProperty("nodes", out JsonElement nodes))
            return result;

        int nodeIndex = 0;
        foreach (JsonElement node in nodes.EnumerateArray())
        {
            if (!node.TryGetProperty("mesh", out JsonElement mesh))
            {
                nodeIndex++;
                continue;
            }

            int meshIndex = mesh.GetInt32();
            if ((uint)meshIndex >= (uint)meshCount)
                throw new InvalidDataException(
                    $"Node {nodeIndex} references invalid mesh {meshIndex}.");
            if (owners[meshIndex] is int existingOwner)
                throw new InvalidDataException(
                    $"Mesh {meshIndex} is instanced by nodes {existingOwner} and {nodeIndex}; " +
                    "a single ImportedMesh cannot preserve both node names and transforms.");

            owners[meshIndex] = nodeIndex;
            if (node.TryGetProperty("name", out JsonElement name) &&
                !string.IsNullOrWhiteSpace(name.GetString()))
                result[meshIndex] = name.GetString();
            nodeIndex++;
        }

        return result;
    }

    private static IReadOnlyList<ImportedTexture> ReadEmbeddedBaseColorTextures(
        JsonElement root, ReadOnlyMemory<byte> binary)
    {
        if (!root.TryGetProperty("materials", out JsonElement materials) ||
            !root.TryGetProperty("textures", out JsonElement textures) ||
            !root.TryGetProperty("images", out JsonElement images) ||
            !root.TryGetProperty("bufferViews", out JsonElement views))
            return [];

        var imageIndices = new List<int>();
        foreach (JsonElement material in materials.EnumerateArray())
        {
            if (!TryGetColorTextureIndex(material, out int texture))
                continue;
            if ((uint)texture >= (uint)textures.GetArrayLength() ||
                !textures[texture].TryGetProperty("source", out JsonElement source))
                continue;
            int image = source.GetInt32();
            if ((uint)image < (uint)images.GetArrayLength() && !imageIndices.Contains(image))
                imageIndices.Add(image);
        }

        var result = new List<ImportedTexture>(imageIndices.Count);
        foreach (int imageIndex in imageIndices)
        {
            JsonElement image = images[imageIndex];
            if (!image.TryGetProperty("bufferView", out JsonElement viewIndex))
                continue;
            int viewNumber = viewIndex.GetInt32();
            if ((uint)viewNumber >= (uint)views.GetArrayLength())
                throw new InvalidDataException($"GLB image {imageIndex} references an invalid bufferView.");
            JsonElement view = views[viewNumber];
            int offset = view.TryGetProperty("byteOffset", out JsonElement byteOffset)
                ? byteOffset.GetInt32() : 0;
            int length = view.GetProperty("byteLength").GetInt32();
            if (offset < 0 || length <= 0 || offset > binary.Length - length)
                throw new InvalidDataException($"GLB image {imageIndex} crosses the binary buffer boundary.");
            string name = image.TryGetProperty("name", out JsonElement imageName)
                ? imageName.GetString() ?? $"image_{imageIndex}" : $"image_{imageIndex}";
            string mime = image.TryGetProperty("mimeType", out JsonElement mimeType)
                ? mimeType.GetString() ?? "application/octet-stream" : "application/octet-stream";
            byte[] imageBytes = binary.Slice(offset, length).ToArray();
            ImageInfo info = Image.Identify(imageBytes) ?? throw new InvalidDataException(
                $"GLB image {imageIndex} has an unsupported or invalid image payload.");
            result.Add(new ImportedTexture(
                name, mime, info.Width, info.Height, imageBytes));
        }
        return result;
    }

    private static bool TryGetColorTextureIndex(
        JsonElement material,
        out int textureIndex)
    {
        // Character rips frequently store the visible atlas in emissiveTexture
        // instead of the glTF PBR base-color slot.  Prefer the proper PBR slot,
        // then accept emissive as the deterministic fallback.
        if (material.TryGetProperty("pbrMetallicRoughness", out JsonElement pbr) &&
            pbr.TryGetProperty("baseColorTexture", out JsonElement baseColor) &&
            baseColor.TryGetProperty("index", out JsonElement baseColorIndex))
        {
            textureIndex = baseColorIndex.GetInt32();
            return true;
        }
        if (material.TryGetProperty("emissiveTexture", out JsonElement emissive) &&
            emissive.TryGetProperty("index", out JsonElement emissiveIndex))
        {
            textureIndex = emissiveIndex.GetInt32();
            return true;
        }
        textureIndex = -1;
        return false;
    }

    private static Matrix4x4[] ResolveMeshTransforms(JsonElement root, int meshCount)
    {
        Matrix4x4[] result = Enumerable.Repeat(Matrix4x4.Identity, meshCount).ToArray();
        if (!root.TryGetProperty("nodes", out JsonElement nodes)) return result;
        Matrix4x4[] local = nodes.EnumerateArray().Select(ReadNodeMatrix).ToArray();
        int?[] parents = new int?[local.Length];
        for (int parent = 0; parent < local.Length; parent++)
        {
            if (!nodes[parent].TryGetProperty("children", out JsonElement children)) continue;
            foreach (JsonElement child in children.EnumerateArray()) parents[child.GetInt32()] = parent;
        }
        for (int nodeIndex = 0; nodeIndex < local.Length; nodeIndex++)
        {
            if (!nodes[nodeIndex].TryGetProperty("mesh", out JsonElement mesh)) continue;
            Matrix4x4 world = local[nodeIndex];
            int? cursor = parents[nodeIndex];
            while (cursor.HasValue) { world *= local[cursor.Value]; cursor = parents[cursor.Value]; }
            int meshIndex = mesh.GetInt32();
            if ((uint)meshIndex < (uint)result.Length) result[meshIndex] = world;
        }
        return result;
    }

    private static Matrix4x4 ReadNodeMatrix(JsonElement node)
    {
        if (node.TryGetProperty("matrix", out JsonElement matrix))
        {
            float[] v = matrix.EnumerateArray().Select(item => item.GetSingle()).ToArray();
            return new Matrix4x4(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13],v[14],v[15]);
        }
        Vector3 scale = ReadVector(node, "scale", Vector3.One);
        Vector3 translation = ReadVector(node, "translation", Vector3.Zero);
        Quaternion rotation = Quaternion.Identity;
        if (node.TryGetProperty("rotation", out JsonElement r))
        {
            float[] q = r.EnumerateArray().Select(item => item.GetSingle()).ToArray();
            rotation = new Quaternion(q[0], q[1], q[2], q[3]);
        }
        return Matrix4x4.CreateScale(scale) * Matrix4x4.CreateFromQuaternion(rotation) * Matrix4x4.CreateTranslation(translation);
    }

    private static Vector3 ReadVector(JsonElement node, string name, Vector3 fallback)
    {
        if (!node.TryGetProperty(name, out JsonElement element)) return fallback;
        float[] v = element.EnumerateArray().Select(item => item.GetSingle()).ToArray();
        return new Vector3(v[0], v[1], v[2]);
    }

    private static Vector3[] ReadVector3(JsonElement root, ReadOnlyMemory<byte> binary, int accessor) =>
        ReadFloats(root, binary, accessor, 3).Select(v => new Vector3(v[0], v[1], v[2])).ToArray();

    private static Vector2[] ReadVector2(JsonElement root, ReadOnlyMemory<byte> binary, int accessor) =>
        ReadFloats(root, binary, accessor, 2).Select(v => new Vector2(v[0], v[1])).ToArray();

    private static Vector4[] ReadVector4(JsonElement root, ReadOnlyMemory<byte> binary, int accessor) =>
        ReadFloats(root, binary, accessor, 4)
            .Select(v => new Vector4(v[0], v[1], v[2], v[3])).ToArray();

    private static Matrix4x4[] ReadMatrix4(
        JsonElement root,
        ReadOnlyMemory<byte> binary,
        int accessor) =>
        ReadFloats(root, binary, accessor, 16).Select(v => new Matrix4x4(
            v[0], v[1], v[2], v[3],
            v[4], v[5], v[6], v[7],
            v[8], v[9], v[10], v[11],
            v[12], v[13], v[14], v[15])).ToArray();

    private static ImportedJointIndices[] ReadJointIndices(
        JsonElement root,
        ReadOnlyMemory<byte> binary,
        int accessorIndex)
    {
        JsonElement accessor = root.GetProperty("accessors")[accessorIndex];
        if (accessor.GetProperty("type").GetString() != "VEC4")
            throw new InvalidDataException("JOINTS_0 must be VEC4.");
        JsonElement view = root.GetProperty("bufferViews")[accessor.GetProperty("bufferView").GetInt32()];
        int componentType = accessor.GetProperty("componentType").GetInt32();
        int componentSize = componentType switch
        {
            5121 => 1,
            5123 => 2,
            _ => throw new InvalidDataException("JOINTS_0 must use UNSIGNED_BYTE or UNSIGNED_SHORT.")
        };
        int count = accessor.GetProperty("count").GetInt32();
        int offset = (view.TryGetProperty("byteOffset", out JsonElement vo) ? vo.GetInt32() : 0) +
            (accessor.TryGetProperty("byteOffset", out JsonElement ao) ? ao.GetInt32() : 0);
        int stride = view.TryGetProperty("byteStride", out JsonElement strideElement)
            ? strideElement.GetInt32() : componentSize * 4;
        ReadOnlySpan<byte> span = binary.Span;
        var result = new ImportedJointIndices[count];
        for (int index = 0; index < count; index++)
        {
            int start = offset + index * stride;
            ushort x = ReadJointComponent(span, start, 0, componentType, componentSize);
            ushort y = ReadJointComponent(span, start, 1, componentType, componentSize);
            ushort z = ReadJointComponent(span, start, 2, componentType, componentSize);
            ushort w = ReadJointComponent(span, start, 3, componentType, componentSize);
            result[index] = new ImportedJointIndices(x, y, z, w);
        }
        return result;
    }

    private static ushort ReadJointComponent(
        ReadOnlySpan<byte> data,
        int start,
        int component,
        int componentType,
        int componentSize) => componentType == 5121
            ? data[start + component]
            : BinaryPrimitives.ReadUInt16LittleEndian(
                data.Slice(start + component * componentSize, componentSize));

    private static uint[] ReadColorsArgb(
        JsonElement root,
        ReadOnlyMemory<byte> binary,
        int accessorIndex)
    {
        JsonElement accessor = root.GetProperty("accessors")[accessorIndex];
        string type = accessor.GetProperty("type").GetString() ?? string.Empty;
        int width = type switch
        {
            "VEC3" => 3,
            "VEC4" => 4,
            _ => throw new InvalidDataException("COLOR_0 must be VEC3 or VEC4.")
        };
        int componentType = accessor.GetProperty("componentType").GetInt32();
        int componentSize = componentType switch
        {
            5121 => 1,
            5123 => 2,
            5126 => 4,
            _ => throw new InvalidDataException("Unsupported COLOR_0 component type.")
        };
        if (componentType != 5126 &&
            (!accessor.TryGetProperty("normalized", out JsonElement normalized) ||
             !normalized.GetBoolean()))
            throw new InvalidDataException("Integer COLOR_0 must be normalized.");
        JsonElement view = root.GetProperty("bufferViews")[accessor.GetProperty("bufferView").GetInt32()];
        int count = accessor.GetProperty("count").GetInt32();
        int offset = (view.TryGetProperty("byteOffset", out JsonElement vo) ? vo.GetInt32() : 0) +
            (accessor.TryGetProperty("byteOffset", out JsonElement ao) ? ao.GetInt32() : 0);
        int stride = view.TryGetProperty("byteStride", out JsonElement strideElement)
            ? strideElement.GetInt32() : componentSize * width;
        ReadOnlySpan<byte> data = binary.Span;
        var result = new uint[count];
        for (int index = 0; index < count; index++)
        {
            int start = offset + index * stride;
            byte r = ToColorByte(ReadColorComponent(
                data, start, 0, componentType, componentSize));
            byte g = ToColorByte(ReadColorComponent(
                data, start, 1, componentType, componentSize));
            byte b = ToColorByte(ReadColorComponent(
                data, start, 2, componentType, componentSize));
            byte a = width == 4
                ? ToColorByte(ReadColorComponent(
                    data, start, 3, componentType, componentSize))
                : byte.MaxValue;
            result[index] = (uint)(a << 24 | r << 16 | g << 8 | b);
        }
        return result;
    }

    private static float ReadColorComponent(
        ReadOnlySpan<byte> data,
        int start,
        int component,
        int componentType,
        int componentSize) => componentType switch
        {
            5121 => data[start + component] / 255f,
            5123 => BinaryPrimitives.ReadUInt16LittleEndian(
                data.Slice(start + component * componentSize, componentSize)) / 65535f,
            _ => BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(
                data.Slice(start + component * componentSize, componentSize)))
        };

    private static byte ToColorByte(float value) => checked((byte)Math.Clamp(
        (int)MathF.Round(value * byte.MaxValue), 0, byte.MaxValue));

    private static float[][] ReadFloats(JsonElement root, ReadOnlyMemory<byte> binary, int accessorIndex, int width)
    {
        JsonElement accessor = root.GetProperty("accessors")[accessorIndex];
        if (accessor.GetProperty("componentType").GetInt32() != 5126)
            throw new InvalidDataException("Only FLOAT vertex attributes are supported.");
        JsonElement view = root.GetProperty("bufferViews")[accessor.GetProperty("bufferView").GetInt32()];
        int count = accessor.GetProperty("count").GetInt32();
        int offset = (view.TryGetProperty("byteOffset", out JsonElement vo) ? vo.GetInt32() : 0) +
            (accessor.TryGetProperty("byteOffset", out JsonElement ao) ? ao.GetInt32() : 0);
        int stride = view.TryGetProperty("byteStride", out JsonElement strideElement)
            ? strideElement.GetInt32() : width * 4;
        var result = new float[count][];
        ReadOnlySpan<byte> span = binary.Span;
        for (int i = 0; i < count; i++)
        {
            result[i] = new float[width];
            for (int c = 0; c < width; c++)
                result[i][c] = BitConverter.Int32BitsToSingle(
                    BinaryPrimitives.ReadInt32LittleEndian(span.Slice(offset + i * stride + c * 4, 4)));
        }
        return result;
    }

    private static uint[] ReadIndices(JsonElement root, ReadOnlyMemory<byte> binary, int accessorIndex)
    {
        JsonElement accessor = root.GetProperty("accessors")[accessorIndex];
        JsonElement view = root.GetProperty("bufferViews")[accessor.GetProperty("bufferView").GetInt32()];
        int type = accessor.GetProperty("componentType").GetInt32();
        int size = type switch { 5121 => 1, 5123 => 2, 5125 => 4, _ => throw new InvalidDataException("Unsupported glTF index type.") };
        int count = accessor.GetProperty("count").GetInt32();
        int offset = (view.TryGetProperty("byteOffset", out JsonElement vo) ? vo.GetInt32() : 0) +
            (accessor.TryGetProperty("byteOffset", out JsonElement ao) ? ao.GetInt32() : 0);
        ReadOnlySpan<byte> span = binary.Span;
        var result = new uint[count];
        for (int i = 0; i < count; i++) result[i] = type switch
        {
            5121 => span[offset + i],
            5123 => BinaryPrimitives.ReadUInt16LittleEndian(span.Slice(offset + i * size, size)),
            _ => BinaryPrimitives.ReadUInt32LittleEndian(span.Slice(offset + i * size, size))
        };
        return result;
    }

}

public static class ImportedModelReader
{
    public static ImportedScene Read(
        string path,
        string? blenderPath = null) => Path.GetExtension(path).ToLowerInvariant() switch
    {
        ".glb" => GlbModelReader.Read(path),
        ".fbx" => FbxModelReader.Read(path, blenderPath),
        ".obj" => ObjModelReader.Read(path),
        _ => throw new NotSupportedException(
            "Supported replacement formats: .fbx, .glb and .obj.")
    };
}
