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
        Matrix4x4[] meshTransforms = ResolveMeshTransforms(root, meshes.GetArrayLength());
        var result = new List<ImportedMesh>();

        for (int meshIndex = 0; meshIndex < meshes.GetArrayLength(); meshIndex++)
        {
            JsonElement mesh = meshes[meshIndex];
            string baseName = mesh.TryGetProperty("name", out JsonElement name)
                ? name.GetString() ?? $"mesh_{meshIndex}" : $"mesh_{meshIndex}";
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
                uint[] indices = primitive.TryGetProperty("indices", out JsonElement index)
                    ? ReadIndices(root, binary, index.GetInt32())
                    : Enumerable.Range(0, positions.Length).Select(value => (uint)value).ToArray();
                Matrix4x4 transform = meshTransforms[meshIndex];
                positions = positions.Select(value => Vector3.Transform(value, transform)).ToArray();
                if (normals.Length == positions.Length)
                {
                    Matrix4x4 normalTransform = transform;
                    if (Matrix4x4.Invert(transform, out Matrix4x4 inverse))
                        normalTransform = Matrix4x4.Transpose(inverse);
                    normals = normals.Select(value => Vector3.Normalize(
                        Vector3.TransformNormal(value, normalTransform))).ToArray();
                }
                result.Add(new ImportedMesh(
                    primitiveIndex == 0 ? baseName : $"{baseName}_{primitiveIndex}",
                    positions, normals, uvs, indices));
                primitiveIndex++;
            }
        }
        return new ImportedScene(result, ReadEmbeddedBaseColorTextures(root, binary));
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
            if (!material.TryGetProperty("pbrMetallicRoughness", out JsonElement pbr) ||
                !pbr.TryGetProperty("baseColorTexture", out JsonElement baseColor) ||
                !baseColor.TryGetProperty("index", out JsonElement textureIndex))
                continue;
            int texture = textureIndex.GetInt32();
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
    public static ImportedScene Read(string path) => Path.GetExtension(path).ToLowerInvariant() switch
    {
        ".glb" => GlbModelReader.Read(path),
        ".obj" => ObjModelReader.Read(path),
        _ => throw new NotSupportedException("Supported replacement formats: .glb and .obj.")
    };
}
