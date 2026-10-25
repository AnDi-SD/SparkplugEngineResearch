using System.Numerics;
using System.Text;
using SixLabors.ImageSharp;
using SmoExporter.Core;

namespace SmoImporter.Core;

/// <summary>
/// Reads FBX directly through the bundled Autodesk FBX SDK bridge. No GLB
/// conversion, Blender installation or Python runtime participates in this path.
/// </summary>
public static class FbxModelReader
{
    private static readonly byte[] Magic = "SMOFBXI1"u8.ToArray();
    private const uint ProtocolVersion = 1;
    private const int MaximumStringBytes = 16 * 1024 * 1024;

    public static ImportedScene Read(
        string path,
        string? nativeBridgePath = null,
        CancellationToken cancellationToken = default) =>
        ReadCore(path, nativeBridgePath, includeAllRigidMeshes: false,
            ignoreSkinning: false, cancellationToken);

    public static ImportedScene ReadRigid(
        string path,
        string? nativeBridgePath = null,
        CancellationToken cancellationToken = default) =>
        ReadCore(path, nativeBridgePath, includeAllRigidMeshes: true,
            ignoreSkinning: false, cancellationToken);

    public static ImportedScene ReadGeometryOnly(
        string path,
        string? nativeBridgePath = null,
        CancellationToken cancellationToken = default) =>
        ReadCore(path, nativeBridgePath, includeAllRigidMeshes: true,
            ignoreSkinning: true, cancellationToken);

    private static ImportedScene ReadCore(
        string path,
        string? nativeBridgePath,
        bool includeAllRigidMeshes,
        bool ignoreSkinning,
        CancellationToken cancellationToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        string fullInput = Path.GetFullPath(path);
        ImportedModelResourceLimits.ValidateInputFile(fullInput, "FBX");
        cancellationToken.ThrowIfCancellationRequested();

        string temporaryDirectory = Path.Combine(
            Path.GetTempPath(), "smo-import-fbx-native-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temporaryDirectory);
        try
        {
            string payload = Path.Combine(temporaryDirectory, "scene.bin");
            var arguments = new List<string> { fullInput, payload };
            if (includeAllRigidMeshes) arguments.Add("--all-meshes");
            if (ignoreSkinning) arguments.Add("--geometry-only");
            NativeFbxBridge.Run(
                "import",
                arguments,
                nativeBridgePath,
                cancellationToken: cancellationToken);
            if (!File.Exists(payload))
                throw new InvalidDataException("Нативный модуль FBX не создал данные сцены.");
            long payloadLength = new FileInfo(payload).Length;
            if (payloadLength > ImportedModelResourceLimits.MaximumNativePayloadBytes)
            {
                throw new InvalidDataException(
                    $"Native FBX payload is {payloadLength / (1024d * 1024d):N1} MiB, " +
                    "above the safe import limit. Reduce or split the FBX model.");
            }
            cancellationToken.ThrowIfCancellationRequested();
            return ReadPayload(payload);
        }
        finally
        {
            try { Directory.Delete(temporaryDirectory, recursive: true); }
            catch { }
        }
    }

    internal static ImportedScene ReadPayload(string path)
    {
        using var stream = new FileStream(
            path, FileMode.Open, FileAccess.Read, FileShare.Read,
            bufferSize: 64 * 1024, FileOptions.SequentialScan);
        using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
        if (!reader.ReadBytes(Magic.Length).AsSpan().SequenceEqual(Magic))
            throw new InvalidDataException("Invalid native FBX import payload signature.");
        uint version = reader.ReadUInt32();
        if (version != ProtocolVersion)
            throw new InvalidDataException(
                $"Unsupported native FBX import payload version {version}.");

        ImportedTexture[] textures = ReadItems(
            reader, "texture", ImportedModelResourceLimits.MaximumTextures, ReadTexture);
        ImportedMaterial[] materials = ReadItems(
            reader, "material", ImportedModelResourceLimits.MaximumMaterials, ReadMaterial);
        ImportedMesh[] meshes = ReadItems(
            reader, "mesh", ImportedModelResourceLimits.MaximumMeshes, ReadMesh);
        if (meshes.Length == 0)
            throw new InvalidDataException("FBX scene contains no mesh primitives.");
        if (stream.Position != stream.Length)
            throw new InvalidDataException("Native FBX import payload contains trailing data.");
        long totalVertices = meshes.Sum(mesh => (long)mesh.Positions.Length);
        long totalIndices = meshes.Sum(mesh => (long)mesh.TriangleIndices.Length);
        ImportedModelResourceLimits.ValidateCount(
            totalVertices,
            ImportedModelResourceLimits.MaximumTotalVertices,
            "Native FBX total vertex");
        ImportedModelResourceLimits.ValidateCount(
            totalIndices,
            ImportedModelResourceLimits.MaximumTotalIndices,
            "Native FBX total index");
        long decodedTexturePixels = 0;
        foreach (ImportedTexture texture in textures)
        {
            decodedTexturePixels = ImportedModelResourceLimits.AddTexturePixels(
                decodedTexturePixels,
                texture.Width,
                texture.Height,
                $"Native FBX texture '{texture.Name}'");
        }
        ValidateMaterialReferences(meshes, materials, textures);
        bool[] textureHasTransparency = textures
            .Select(TextureContainsTransparencyIfDecodable)
            .ToArray();
        materials = materials.Select(material =>
        {
            int textureIndex = material.BaseColorTextureIndex;
            return textureIndex >= 0 &&
                   textureHasTransparency[textureIndex] &&
                   material.AlphaMode == ImportedMaterialAlphaMode.Opaque
                ? material with { AlphaMode = ImportedMaterialAlphaMode.Blend }
                : material;
        }).ToArray();
        return new ImportedScene(
            Array.AsReadOnly(meshes),
            Array.AsReadOnly(textures),
            Array.AsReadOnly(materials));
    }

    private static bool TextureContainsTransparencyIfDecodable(
        ImportedTexture texture)
    {
        try
        {
            return ImportedTextureImageTools.TextureContainsTransparency(texture);
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          UnknownImageFormatException or
                                          InvalidImageContentException or
                                          NotSupportedException)
        {
            // The native protocol deliberately preserves unsupported image
            // payloads for a later external override. Such a payload carries
            // no trustworthy alpha evidence and therefore stays opaque.
            return false;
        }
    }

    private static ImportedTexture ReadTexture(BinaryReader reader)
    {
        string name = ReadString(reader);
        string mime = ReadString(reader);
        int width = reader.ReadInt32();
        int height = reader.ReadInt32();
        byte[] data = ReadBytes(
            reader,
            "texture data",
            ImportedModelResourceLimits.MaximumEncodedTextureBytes);
        string sourcePath = ReadString(reader);
        try
        {
            ImageInfo? information = Image.Identify(data);
            if (information is not null)
            {
                width = information.Width;
                height = information.Height;
            }
        }
        catch (Exception exception) when (exception is UnknownImageFormatException or
                                          InvalidImageContentException or
                                          NotSupportedException)
        {
            // Keep unsupported data and its logical name for external overrides.
        }
        return new ImportedTexture(
            name, mime, width, height, data,
            string.IsNullOrWhiteSpace(sourcePath) ? null : sourcePath);
    }

    private static ImportedMaterial ReadMaterial(BinaryReader reader) =>
        new(ReadString(reader), NullIfEmpty(ReadString(reader)), reader.ReadInt32());

    private static ImportedMesh ReadMesh(BinaryReader reader)
    {
        string name = ReadString(reader);
        int materialIndex = reader.ReadInt32();
        Vector3[] positions = ReadVector3Array(reader, "positions");
        Vector3[] normals = ReadVector3Array(reader, "normals");
        Vector2[] uvs = ReadVector2Array(reader, "texture coordinates");
        uint[] colors = ReadUInt32Array(reader, "vertex colors");
        uint[] indices = ReadUInt32Array(reader, "triangle indices");
        ValidateAttributeCount(name, positions.Length, normals.Length, "normal");
        ValidateAttributeCount(name, positions.Length, uvs.Length, "UV");
        ValidateAttributeCount(name, positions.Length, colors.Length, "color");
        if (indices.Length % 3 != 0 || indices.Any(index => index >= positions.Length))
            throw new InvalidDataException(
                $"FBX mesh '{name}' contains invalid triangle indices.");

        byte hasSkinning = reader.ReadByte();
        ImportedSkinning? skinning = hasSkinning switch
        {
            0 => null,
            1 => ReadSkinning(reader, name, positions.Length),
            _ => throw new InvalidDataException(
                $"FBX mesh '{name}' has an invalid skinning marker {hasSkinning}.")
        };
        return new ImportedMesh(
            name, positions, normals, uvs, indices, colors,
            materialIndex, skinning);
    }

    private static ImportedSkinning ReadSkinning(
        BinaryReader reader,
        string meshName,
        int vertexCount)
    {
        string skeletonName = ReadString(reader);
        int jointCount = ReadCount(
            reader,
            "joint",
            ImportedModelResourceLimits.MaximumJointsPerSkin);
        var names = new string[jointCount];
        var parents = new int[jointCount];
        var inverseBind = new Matrix4x4[jointCount];
        var bindWorld = new Matrix4x4[jointCount];
        var bindLocal = new Matrix4x4[jointCount];
        for (int joint = 0; joint < jointCount; joint++)
        {
            names[joint] = ReadString(reader);
            parents[joint] = reader.ReadInt32();
            if (parents[joint] < -1 || parents[joint] >= jointCount || parents[joint] == joint)
                throw new InvalidDataException(
                    $"FBX skeleton '{skeletonName}' has invalid parent {parents[joint]} " +
                    $"for joint {joint}.");
            inverseBind[joint] = ReadMatrix(reader);
            bindWorld[joint] = ReadMatrix(reader);
            bindLocal[joint] = ReadMatrix(reader);
        }
        ImportedJointIndices[] joints = ReadJointArray(reader);
        Vector4[] weights = ReadVector4Array(
            reader,
            "skin weights",
            ImportedModelResourceLimits.MaximumVerticesPerPrimitive);
        if (joints.Length != vertexCount || weights.Length != vertexCount)
            throw new InvalidDataException(
                $"FBX mesh '{meshName}' skin attribute count differs from its vertex count.");
        for (int vertex = 0; vertex < vertexCount; vertex++)
        {
            ImportedJointIndices slots = joints[vertex];
            Vector4 amounts = weights[vertex];
            ValidateInfluence(slots.X, amounts.X, 0);
            ValidateInfluence(slots.Y, amounts.Y, 1);
            ValidateInfluence(slots.Z, amounts.Z, 2);
            ValidateInfluence(slots.W, amounts.W, 3);

            void ValidateInfluence(ushort slot, float amount, int component)
            {
                if (!float.IsFinite(amount) || amount < 0 ||
                    (amount > 0 && slot >= jointCount))
                    throw new InvalidDataException(
                        $"FBX mesh '{meshName}' has an invalid skin influence at " +
                        $"vertex {vertex}, component {component}.");
            }
        }
        var skeleton = new ImportedSkeleton(
            skeletonName,
            Array.AsReadOnly(names),
            Array.AsReadOnly(inverseBind))
        {
            ParentJointIndices = Array.AsReadOnly(parents),
            BindWorldMatrices = Array.AsReadOnly(bindWorld),
            BindLocalMatrices = Array.AsReadOnly(bindLocal)
        };
        return new ImportedSkinning(skeleton, joints, weights);
    }

    private static Matrix4x4 ReadMatrix(BinaryReader reader) => new(
        reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle(),
        reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle(),
        reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle(),
        reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle());

    private static ImportedJointIndices[] ReadJointArray(BinaryReader reader)
    {
        int count = ReadCount(
            reader,
            "joint attribute",
            ImportedModelResourceLimits.MaximumVerticesPerPrimitive);
        var result = new ImportedJointIndices[count];
        for (int index = 0; index < count; index++)
            result[index] = new ImportedJointIndices(
                reader.ReadUInt16(), reader.ReadUInt16(),
                reader.ReadUInt16(), reader.ReadUInt16());
        return result;
    }

    private static Vector2[] ReadVector2Array(
        BinaryReader reader,
        string owner,
        int maximum = ImportedModelResourceLimits.MaximumVerticesPerPrimitive)
    {
        int count = ReadCount(reader, owner, maximum);
        var result = new Vector2[count];
        for (int index = 0; index < count; index++)
            result[index] = new Vector2(reader.ReadSingle(), reader.ReadSingle());
        return result;
    }

    private static Vector3[] ReadVector3Array(
        BinaryReader reader,
        string owner,
        int maximum = ImportedModelResourceLimits.MaximumVerticesPerPrimitive)
    {
        int count = ReadCount(reader, owner, maximum);
        var result = new Vector3[count];
        for (int index = 0; index < count; index++)
            result[index] = new Vector3(
                reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle());
        return result;
    }

    private static Vector4[] ReadVector4Array(
        BinaryReader reader,
        string owner,
        int maximum = ImportedModelResourceLimits.MaximumVerticesPerPrimitive)
    {
        int count = ReadCount(reader, owner, maximum);
        var result = new Vector4[count];
        for (int index = 0; index < count; index++)
            result[index] = new Vector4(
                reader.ReadSingle(), reader.ReadSingle(),
                reader.ReadSingle(), reader.ReadSingle());
        return result;
    }

    private static uint[] ReadUInt32Array(BinaryReader reader, string owner)
    {
        int maximum = owner.Contains("triangle", StringComparison.OrdinalIgnoreCase)
            ? ImportedModelResourceLimits.MaximumIndicesPerPrimitive
            : ImportedModelResourceLimits.MaximumVerticesPerPrimitive;
        int count = ReadCount(reader, owner, maximum);
        var result = new uint[count];
        for (int index = 0; index < count; index++) result[index] = reader.ReadUInt32();
        return result;
    }

    private static byte[] ReadBytes(BinaryReader reader, string owner, int maximum)
    {
        int count = ReadCount(reader, owner, maximum);
        byte[] result = reader.ReadBytes(count);
        if (result.Length != count)
            throw new EndOfStreamException($"Unexpected end of native FBX {owner}.");
        return result;
    }

    private static T[] ReadItems<T>(
        BinaryReader reader,
        string owner,
        int maximum,
        Func<BinaryReader, T> read)
    {
        int count = ReadCount(reader, owner, maximum);
        var result = new T[count];
        for (int index = 0; index < count; index++) result[index] = read(reader);
        return result;
    }

    private static int ReadCount(BinaryReader reader, string owner, int maximum)
    {
        uint value = reader.ReadUInt32();
        if (value > maximum)
            throw new InvalidDataException(
                $"Native FBX {owner} count {value:N0} exceeds the safe limit " +
                $"{maximum:N0}.");
        return checked((int)value);
    }

    private static string ReadString(BinaryReader reader)
    {
        uint byteCount = reader.ReadUInt32();
        if (byteCount > MaximumStringBytes)
            throw new InvalidDataException(
                $"Native FBX string length {byteCount} exceeds the safety limit.");
        byte[] bytes = reader.ReadBytes(checked((int)byteCount));
        if (bytes.Length != byteCount)
            throw new EndOfStreamException("Unexpected end of native FBX string.");
        return new UTF8Encoding(false, true).GetString(bytes);
    }

    private static void ValidateAttributeCount(
        string mesh,
        int vertexCount,
        int attributeCount,
        string attribute)
    {
        if (attributeCount != 0 && attributeCount != vertexCount)
            throw new InvalidDataException(
                $"FBX mesh '{mesh}' has {attributeCount} {attribute} values for " +
                $"{vertexCount} vertices.");
    }

    private static void ValidateMaterialReferences(
        IReadOnlyList<ImportedMesh> meshes,
        IReadOnlyList<ImportedMaterial> materials,
        IReadOnlyList<ImportedTexture> textures)
    {
        foreach (ImportedMesh mesh in meshes)
        {
            if (mesh.MaterialIndex < -1 || mesh.MaterialIndex >= materials.Count)
                throw new InvalidDataException(
                    $"FBX mesh '{mesh.Name}' references invalid material {mesh.MaterialIndex}.");
        }
        foreach (ImportedMaterial material in materials)
        {
            if (material.BaseColorTextureIndex < -1 ||
                material.BaseColorTextureIndex >= textures.Count)
                throw new InvalidDataException(
                    $"FBX material '{material.Name}' references invalid texture " +
                    $"{material.BaseColorTextureIndex}.");
        }
    }

    private static string? NullIfEmpty(string value) =>
        string.IsNullOrWhiteSpace(value) ? null : value;
}
