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
        var gltfSkins = new List<object>();
        var gltfAnimations = new List<object>();
        var textureIndices = new Dictionary<int, int>();
        var nodeObjects = new Dictionary<int, Dictionary<string, object>>();
        var nodeChildren = new Dictionary<int, List<int>>();
        Dictionary<int, int> nodeIndices = scene.Nodes
            .Select((node, index) => (node.ObjectIndex, index))
            .ToDictionary(item => item.ObjectIndex, item => item.index);
        Dictionary<int, int[]> children = scene.Nodes
            .Where(node => node.ParentObjectIndex is not null)
            .GroupBy(node => node.ParentObjectIndex!.Value)
            .ToDictionary(group => group.Key, group => group
                .Select(node => node.ObjectIndex).ToArray());
        foreach (SmoExportNode node in scene.Nodes)
        {
            if (!Matrix4x4.Decompose(node.BindLocalMatrix,
                    out Vector3 scale, out Quaternion rotation, out Vector3 translation))
                throw new InvalidDataException(
                    $"Node {node.ObjectIndex} ({node.Name}) has a non-decomposable bind transform.");
            var value = new Dictionary<string, object>
            {
                ["name"] = CleanName(node.Name, $"bone_{node.ObjectIndex}"),
                ["translation"] = new[] { translation.X, translation.Y, translation.Z },
                ["rotation"] = new[] { rotation.X, rotation.Y, rotation.Z, rotation.W },
                ["scale"] = new[] { scale.X, scale.Y, scale.Z },
                ["extras"] = new { sparkplugObjectIndex = node.ObjectIndex }
            };
            List<int> gltfChildren = children.TryGetValue(
                    node.ObjectIndex, out int[]? sourceChildren)
                ? sourceChildren.Where(nodeIndices.ContainsKey)
                    .Select(index => nodeIndices[index]).ToList()
                : [];
            if (gltfChildren.Count > 0)
                value["children"] = gltfChildren;
            nodeObjects[node.ObjectIndex] = value;
            nodeChildren[node.ObjectIndex] = gltfChildren;
            nodes.Add(value);
        }
        Dictionary<int, int> skinIndices = new();
        foreach (SmoExportSkin skin in scene.Skins)
        {
            int inverseBindAccessor = AddMatrix4Accessor(
                binary, views, accessors, skin.InverseBindMatrices.ToArray());
            int[] joints = skin.JointObjectIndices.Where(nodeIndices.ContainsKey)
                .Select(index => nodeIndices[index]).ToArray();
            skinIndices[skin.ObjectIndex] = gltfSkins.Count;
            gltfSkins.Add(new
            {
                name = CleanName(skin.Name, $"skin_{skin.ObjectIndex}"),
                joints,
                inverseBindMatrices = inverseBindAccessor,
                skeleton = joints.FirstOrDefault()
            });
        }

        int? AddTexture(SmoExportTexture? texture)
        {
            if (texture is null)
                return null;
            if (textureIndices.TryGetValue(texture.ObjectIndex, out int existing))
                return existing;

            int imageView = AddBytes(binary, views, texture.PngBytes, null);
            int imageIndex = images.Count;
            images.Add(new
            {
                name = CleanName(texture.Name, $"texture_{texture.ObjectIndex}"),
                mimeType = "image/png",
                bufferView = imageView
            });
            existing = textures.Count;
            textures.Add(new { source = imageIndex });
            textureIndices[texture.ObjectIndex] = existing;
            return existing;
        }

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
            if (mesh.BlendWeights.Length == mesh.Positions.Length &&
                mesh.JointIndices.Length == mesh.Positions.Length)
            {
                attributes["WEIGHTS_0"] = AddVector4Accessor(
                    binary, views, accessors, mesh.BlendWeights, 34962);
                attributes["JOINTS_0"] = AddJointAccessor(
                    binary, views, accessors, mesh.JointIndices, 34962);
            }

            int indexAccessor = AddIndicesAccessor(
                binary, views, accessors, mesh.TriangleIndices);
            int? textureIndex = AddTexture(mesh.Texture);
            int? effectTextureIndex = AddTexture(mesh.EffectTexture);

            int materialIndex = materials.Count;
            var pbr = new Dictionary<string, object>
            {
                ["baseColorFactor"] = ToArray(mesh.MaterialColor),
                ["metallicFactor"] = 0f,
                ["roughnessFactor"] = 1f
            };
            if (textureIndex.HasValue)
                pbr["baseColorTexture"] = new { index = textureIndex.Value };
            var gltfMaterial = new Dictionary<string, object>
            {
                ["name"] = CleanName(mesh.Name, $"material_{mesh.ObjectIndex}"),
                ["pbrMetallicRoughness"] = pbr,
                ["doubleSided"] = true
            };
            if (mesh.UsesAlphaBlend)
                gltfMaterial["alphaMode"] = "BLEND";
            if (effectTextureIndex.HasValue)
            {
                gltfMaterial["emissiveTexture"] = new
                {
                    index = effectTextureIndex.Value,
                    texCoord = 1
                };
                gltfMaterial["emissiveFactor"] = new[] { 1f, 1f, 1f };
            }
            materials.Add(gltfMaterial);

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
            var meshNode = new Dictionary<string, object>
            {
                ["name"] = CleanName(mesh.Name, $"node_{mesh.ObjectIndex}"),
                ["mesh"] = gltfMeshes.Count - 1
            };
            if (!Matrix4x4.Decompose(mesh.BindLocalMatrix,
                    out Vector3 meshScale,
                    out Quaternion meshRotation,
                    out Vector3 meshTranslation))
            {
                throw new InvalidDataException(
                    $"Mesh node {mesh.ObjectIndex} ({mesh.Name}) has a " +
                    "non-decomposable bind transform.");
            }
            meshNode["translation"] = new[]
                { meshTranslation.X, meshTranslation.Y, meshTranslation.Z };
            meshNode["rotation"] = new[]
                { meshRotation.X, meshRotation.Y, meshRotation.Z, meshRotation.W };
            meshNode["scale"] = new[] { meshScale.X, meshScale.Y, meshScale.Z };
            if (mesh.SkinObjectIndex is int skinObjectIndex &&
                skinIndices.TryGetValue(skinObjectIndex, out int gltfSkinIndex))
                meshNode["skin"] = gltfSkinIndex;
            int meshNodeIndex = nodes.Count;
            nodes.Add(meshNode);
            if (mesh.ParentNodeObjectIndex is int parentObjectIndex &&
                nodeObjects.TryGetValue(parentObjectIndex, out Dictionary<string, object>? parentNode))
            {
                List<int> parentChildren = nodeChildren[parentObjectIndex];
                parentChildren.Add(meshNodeIndex);
                parentNode["children"] = parentChildren;
            }
        }

        foreach (SmoExportAnimation animation in scene.Animations)
            gltfAnimations.Add(BuildAnimation(
                animation, nodeIndices, binary, views, accessors));

        int binaryLength = checked((int)binary.Length);
        var root = new
        {
            asset = new { version = "2.0", generator = "Sparkplug SmoExporter" },
            scene = 0,
            scenes = new[] { new { name = Path.GetFileNameWithoutExtension(scene.SourcePath), nodes = GetSceneRoots(scene, nodeIndices).ToArray() } },
            nodes,
            meshes = gltfMeshes,
            skins = gltfSkins,
            animations = gltfAnimations,
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

    private static IEnumerable<int> GetSceneRoots(
        SmoExportScene scene, IReadOnlyDictionary<int, int> nodeIndices)
    {
        foreach (SmoExportNode node in scene.Nodes)
            if (node.ParentObjectIndex is not int parent || !nodeIndices.ContainsKey(parent))
                yield return nodeIndices[node.ObjectIndex];
        for (int meshIndex = 0; meshIndex < scene.Meshes.Count; meshIndex++)
        {
            int? parent = scene.Meshes[meshIndex].ParentNodeObjectIndex;
            if (parent is null || !nodeIndices.ContainsKey(parent.Value))
                yield return scene.Nodes.Count + meshIndex;
        }
    }

    private static object BuildAnimation(
        SmoExportAnimation animation, IReadOnlyDictionary<int, int> nodeIndices,
        MemoryStream binary, List<object> views, List<object> accessors)
    {
        var samplers = new List<object>();
        var channels = new List<object>();
        foreach (SmoExportAnimationTrack track in animation.Tracks)
        {
            if (!nodeIndices.TryGetValue(track.NodeObjectIndex, out int node)) continue;
            Add(track.Positions.Select(key => key.Time).ToArray(),
                track.Positions.Select(key => key.Value).ToArray(), "translation");
            Add4(track.Rotations.Select(key => key.Time).ToArray(),
                track.Rotations.Select(key => new Vector4(
                    key.Value.X, key.Value.Y, key.Value.Z, key.Value.W)).ToArray(), "rotation");
            Add(track.Scales.Select(key => key.Time).ToArray(),
                track.Scales.Select(key => key.Value).ToArray(), "scale");

            void Add(float[] times, Vector3[] values, string path)
            {
                if (times.Length == 0 || times.Length != values.Length) return;
                int input = AddScalarAccessor(binary, views, accessors, times);
                int output = AddVector3Accessor(binary, views, accessors, values, false, 34962);
                int sampler = samplers.Count;
                samplers.Add(new { input, output, interpolation = "LINEAR" });
                channels.Add(new { sampler, target = new { node, path } });
            }
            void Add4(float[] times, Vector4[] values, string path)
            {
                if (times.Length == 0 || times.Length != values.Length) return;
                int input = AddScalarAccessor(binary, views, accessors, times);
                int output = AddVector4Accessor(binary, views, accessors, values, 34962);
                int sampler = samplers.Count;
                samplers.Add(new { input, output, interpolation = "LINEAR" });
                channels.Add(new { sampler, target = new { node, path } });
            }
        }
        return new { name = CleanName(animation.Name, "animation"), samplers, channels };
    }

    private static int AddScalarAccessor(
        MemoryStream binary, List<object> views, List<object> accessors, float[] values)
    {
        byte[] bytes = new byte[checked(values.Length * 4)];
        for (int i = 0; i < values.Length; i++) WriteSingle(bytes, i * 4, values[i]);
        int view = AddBytes(binary, views, bytes, null);
        int result = accessors.Count;
        accessors.Add(new
        {
            bufferView = view, componentType = 5126, count = values.Length, type = "SCALAR",
            min = new[] { values.Min() }, max = new[] { values.Max() }
        });
        return result;
    }

    private static int AddMatrix4Accessor(
        MemoryStream binary, List<object> views, List<object> accessors, Matrix4x4[] values)
    {
        byte[] bytes = new byte[checked(values.Length * 64)];
        for (int i = 0; i < values.Length; i++)
        {
            float[] matrix = ToArray(values[i]);
            for (int component = 0; component < 16; component++)
                WriteSingle(bytes, i * 64 + component * 4, matrix[component]);
        }
        int view = AddBytes(binary, views, bytes, null);
        int result = accessors.Count;
        accessors.Add(new { bufferView = view, componentType = 5126, count = values.Length, type = "MAT4" });
        return result;
    }

    private static int AddJointAccessor(
        MemoryStream binary, List<object> views, List<object> accessors,
        Vector4[] values, int target)
    {
        byte[] bytes = new byte[checked(values.Length * 8)];
        for (int i = 0; i < values.Length; i++)
        {
            BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(i * 8), checked((ushort)values[i].X));
            BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(i * 8 + 2), checked((ushort)values[i].Y));
            BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(i * 8 + 4), checked((ushort)values[i].Z));
            BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(i * 8 + 6), checked((ushort)values[i].W));
        }
        int view = AddBytes(binary, views, bytes, target);
        int result = accessors.Count;
        accessors.Add(new { bufferView = view, componentType = 5123, count = values.Length, type = "VEC4" });
        return result;
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
    private static float[] ToArray(Matrix4x4 value) =>
    [
        value.M11, value.M12, value.M13, value.M14,
        value.M21, value.M22, value.M23, value.M24,
        value.M31, value.M32, value.M33, value.M34,
        value.M41, value.M42, value.M43, value.M44
    ];

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
