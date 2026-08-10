using System.Buffers.Binary;
using System.Numerics;
using System.Text;
using System.Text.Json;

namespace SmoExporter.Core;

public static class GlbExporter
{
    public static void Export(SmoExportScene scene, string outputPath)
    {
        ArgumentNullException.ThrowIfNull(scene);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outputPath))!);

        using var binary = new MemoryStream();
        var views = new List<object>();
        var accessors = new List<object>();
        var images = new List<object>();
        var textures = new List<object>();
        var materials = new List<object>();
        var gltfMeshes = new List<object>();
        var nodes = new List<object>();
        var textureIndices = new Dictionary<int, int>();

        foreach (SmoExportMesh mesh in scene.Meshes)
        {
            int positionAccessor = AddVector3Accessor(
                binary, views, accessors, mesh.Positions, includeBounds: true, target: 34962);
            var attributes = new Dictionary<string, int> { ["POSITION"] = positionAccessor };
            if (mesh.Normals.Length == mesh.Positions.Length)
                attributes["NORMAL"] = AddVector3Accessor(
                    binary, views, accessors, mesh.Normals, false, 34962);
            if (mesh.TextureCoordinates0.Length == mesh.Positions.Length)
                attributes["TEXCOORD_0"] = AddVector2Accessor(
                    binary, views, accessors, mesh.TextureCoordinates0, 34962);
            if (mesh.TextureCoordinates1.Length == mesh.Positions.Length)
                attributes["TEXCOORD_1"] = AddVector2Accessor(
                    binary, views, accessors, mesh.TextureCoordinates1, 34962);
            if (mesh.Colors.Length == mesh.Positions.Length)
                attributes["COLOR_0"] = AddVector4Accessor(
                    binary, views, accessors, mesh.Colors, 34962);

            int indexAccessor = AddIndicesAccessor(
                binary, views, accessors, mesh.TriangleIndices);
            int? textureIndex = null;
            if (mesh.Texture is not null)
            {
                if (!textureIndices.TryGetValue(mesh.Texture.ObjectIndex, out int existing))
                {
                    int imageView = AddBytes(binary, views, mesh.Texture.PngBytes, null);
                    int imageIndex = images.Count;
                    images.Add(new
                    {
                        name = CleanName(mesh.Texture.Name, $"texture_{mesh.Texture.ObjectIndex}"),
                        mimeType = "image/png",
                        bufferView = imageView
                    });
                    existing = textures.Count;
                    textures.Add(new { source = imageIndex });
                    textureIndices[mesh.Texture.ObjectIndex] = existing;
                }
                textureIndex = existing;
            }

            int materialIndex = materials.Count;
            var pbr = new Dictionary<string, object>
            {
                ["baseColorFactor"] = ToArray(mesh.MaterialColor),
                ["metallicFactor"] = 0f,
                ["roughnessFactor"] = 1f
            };
            if (textureIndex.HasValue)
                pbr["baseColorTexture"] = new { index = textureIndex.Value };
            materials.Add(new
            {
                name = CleanName(mesh.Name, $"material_{mesh.ObjectIndex}"),
                pbrMetallicRoughness = pbr,
                doubleSided = true
            });

            var primitive = new Dictionary<string, object>
            {
                ["attributes"] = attributes,
                ["indices"] = indexAccessor,
                ["material"] = materialIndex,
                ["mode"] = 4
            };
            gltfMeshes.Add(new
            {
                name = CleanName(mesh.Name, $"mesh_{mesh.ObjectIndex}"),
                primitives = new[] { primitive },
                extras = new
                {
                    sparkplugObjectIndex = mesh.ObjectIndex,
                    sparkplugObjectId = mesh.ObjectId,
                    sparkplugMarker = $"0x{mesh.Marker:X2}",
                    sparkplugPrimitiveType = mesh.PrimitiveType,
                    sparkplugVertexFormat = $"0x{mesh.VertexFormat:X4}",
                    sparkplugSerializedStride = mesh.SerializedStride,
                    sparkplugRuntimeStride = mesh.RuntimeStride
                }
            });
            nodes.Add(new
            {
                name = CleanName(mesh.Name, $"node_{mesh.ObjectIndex}"),
                mesh = gltfMeshes.Count - 1
            });
        }

        int binaryLength = checked((int)binary.Length);
        var root = new
        {
            asset = new { version = "2.0", generator = "Sparkplug SmoExporter" },
            scene = 0,
            scenes = new[] { new { name = Path.GetFileNameWithoutExtension(scene.SourcePath), nodes = Enumerable.Range(0, nodes.Count).ToArray() } },
            nodes,
            meshes = gltfMeshes,
            materials,
            textures,
            images,
            accessors,
            bufferViews = views,
            buffers = new[] { new { byteLength = binaryLength } },
            extras = new
            {
                sparkplugSource = Path.GetFileName(scene.SourcePath),
                sparkplugSourceSha256 = scene.SourceSha256,
                sparkplugPlatformFlags = scene.PlatformFlags,
                warnings = scene.Warnings
            }
        };
        byte[] json = JsonSerializer.SerializeToUtf8Bytes(root, new JsonSerializerOptions
        {
            WriteIndented = false,
            DefaultIgnoreCondition = System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull
        });
        json = Pad(json, 0x20);
        byte[] bin = Pad(binary.ToArray(), 0x00);

        using FileStream output = File.Create(outputPath);
        Span<byte> header = stackalloc byte[12];
        BinaryPrimitives.WriteUInt32LittleEndian(header, 0x46546C67);
        BinaryPrimitives.WriteUInt32LittleEndian(header[4..], 2);
        BinaryPrimitives.WriteUInt32LittleEndian(
            header[8..], checked((uint)(12 + 8 + json.Length + 8 + bin.Length)));
        output.Write(header);
        WriteChunk(output, json, 0x4E4F534A);
        WriteChunk(output, bin, 0x004E4942);
    }

    private static int AddVector3Accessor(
        MemoryStream binary, List<object> views, List<object> accessors,
        Vector3[] values, bool includeBounds, int target)
    {
        byte[] bytes = new byte[checked(values.Length * 12)];
        for (int i = 0; i < values.Length; i++)
        {
            WriteSingle(bytes, i * 12, values[i].X);
            WriteSingle(bytes, i * 12 + 4, values[i].Y);
            WriteSingle(bytes, i * 12 + 8, values[i].Z);
        }
        int view = AddBytes(binary, views, bytes, target);
        int result = accessors.Count;
        if (includeBounds)
        {
            Vector3 min = values.Aggregate(Vector3.Min);
            Vector3 max = values.Aggregate(Vector3.Max);
            accessors.Add(new { bufferView = view, componentType = 5126, count = values.Length, type = "VEC3", min = ToArray(min), max = ToArray(max) });
        }
        else
            accessors.Add(new { bufferView = view, componentType = 5126, count = values.Length, type = "VEC3" });
        return result;
    }

    private static int AddVector2Accessor(MemoryStream binary, List<object> views, List<object> accessors, Vector2[] values, int target)
    {
        byte[] bytes = new byte[checked(values.Length * 8)];
        for (int i = 0; i < values.Length; i++)
        {
            WriteSingle(bytes, i * 8, values[i].X);
            WriteSingle(bytes, i * 8 + 4, values[i].Y);
        }
        int view = AddBytes(binary, views, bytes, target);
        int result = accessors.Count;
        accessors.Add(new { bufferView = view, componentType = 5126, count = values.Length, type = "VEC2" });
        return result;
    }

    private static int AddVector4Accessor(MemoryStream binary, List<object> views, List<object> accessors, Vector4[] values, int target)
    {
        byte[] bytes = new byte[checked(values.Length * 16)];
        for (int i = 0; i < values.Length; i++)
        {
            WriteSingle(bytes, i * 16, values[i].X);
            WriteSingle(bytes, i * 16 + 4, values[i].Y);
            WriteSingle(bytes, i * 16 + 8, values[i].Z);
            WriteSingle(bytes, i * 16 + 12, values[i].W);
        }
        int view = AddBytes(binary, views, bytes, target);
        int result = accessors.Count;
        accessors.Add(new { bufferView = view, componentType = 5126, count = values.Length, type = "VEC4" });
        return result;
    }

    private static int AddIndicesAccessor(MemoryStream binary, List<object> views, List<object> accessors, uint[] values)
    {
        byte[] bytes = new byte[checked(values.Length * 4)];
        for (int i = 0; i < values.Length; i++)
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(i * 4), values[i]);
        int view = AddBytes(binary, views, bytes, 34963);
        int result = accessors.Count;
        accessors.Add(new { bufferView = view, componentType = 5125, count = values.Length, type = "SCALAR" });
        return result;
    }

    private static int AddBytes(MemoryStream binary, List<object> views, byte[] bytes, int? target)
    {
        Align(binary);
        int offset = checked((int)binary.Position);
        binary.Write(bytes);
        int result = views.Count;
        views.Add(target.HasValue
            ? new { buffer = 0, byteOffset = offset, byteLength = bytes.Length, target = target.Value }
            : (object)new { buffer = 0, byteOffset = offset, byteLength = bytes.Length });
        return result;
    }

    private static void Align(MemoryStream stream)
    {
        while ((stream.Position & 3) != 0)
            stream.WriteByte(0);
    }

    private static void WriteSingle(Span<byte> bytes, int offset, float value) =>
        BinaryPrimitives.WriteInt32LittleEndian(bytes[offset..], BitConverter.SingleToInt32Bits(value));

    private static float[] ToArray(Vector2 value) => [value.X, value.Y];
    private static float[] ToArray(Vector3 value) => [value.X, value.Y, value.Z];
    private static float[] ToArray(Vector4 value) => [value.X, value.Y, value.Z, value.W];

    private static string CleanName(string value, string fallback) =>
        string.IsNullOrWhiteSpace(value) ? fallback : value.Replace('\0', '_');

    private static byte[] Pad(byte[] value, byte padding)
    {
        int length = (value.Length + 3) & ~3;
        if (length == value.Length)
            return value;
        byte[] padded = new byte[length];
        value.CopyTo(padded, 0);
        padded.AsSpan(value.Length).Fill(padding);
        return padded;
    }

    private static void WriteChunk(Stream output, byte[] bytes, uint type)
    {
        Span<byte> header = stackalloc byte[8];
        BinaryPrimitives.WriteUInt32LittleEndian(header, (uint)bytes.Length);
        BinaryPrimitives.WriteUInt32LittleEndian(header[4..], type);
        output.Write(header);
        output.Write(bytes);
    }
}
