using System.Numerics;
using System.Runtime.InteropServices;
using System.Text.Json;
using SmoImporter.Core;

namespace SmoLVLcreator.Core;

/// <summary>
/// Private worker transport for the rigid scene accepted in the editor.
/// Geometry and image bytes are streamed without JSON numeric arrays or base64.
/// </summary>
internal static class SmoRigidImportSnapshot
{
    private const int Version = 1;

    public static void Write(string path, ImportedScene scene)
    {
        if (scene.HasSkinning)
            throw new InvalidDataException("An isolated level import requires a rigid scene.");

        var header = new Header(
            scene.Meshes.Select(mesh => new MeshHeader(mesh.Name, mesh.MaterialIndex)).ToArray(),
            scene.Textures.Select(texture => texture with { Data = [] }).ToArray(),
            scene.Materials,
            scene.ImportWarnings);

        using var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write);
        using var writer = new BinaryWriter(stream);
        writer.Write(Version);
        WriteArray(writer, JsonSerializer.SerializeToUtf8Bytes(header));
        foreach (ImportedMesh mesh in scene.Meshes)
        {
            WriteArray(writer, mesh.Positions);
            WriteArray(writer, mesh.Normals);
            WriteArray(writer, mesh.TextureCoordinates);
            WriteArray(writer, mesh.SecondaryTextureCoordinates);
            WriteArray(writer, mesh.TriangleIndices);
            writer.Write(mesh.DiffuseColorsArgb is not null);
            if (mesh.DiffuseColorsArgb is { } colors)
                WriteArray(writer, colors);
        }
        foreach (ImportedTexture texture in scene.Textures)
            WriteArray(writer, texture.Data);
    }

    public static ImportedScene Read(string path)
    {
        using var stream = File.OpenRead(path);
        using var reader = new BinaryReader(stream);
        if (reader.ReadInt32() != Version)
            throw new InvalidDataException("Unsupported rigid import snapshot version.");
        Header header = JsonSerializer.Deserialize<Header>(ReadArray<byte>(reader))
            ?? throw new InvalidDataException("The rigid import snapshot has no header.");

        var meshes = new ImportedMesh[header.Meshes.Length];
        for (int index = 0; index < meshes.Length; index++)
        {
            MeshHeader mesh = header.Meshes[index];
            Vector3[] positions = ReadArray<Vector3>(reader);
            Vector3[] normals = ReadArray<Vector3>(reader);
            Vector2[] uv0 = ReadArray<Vector2>(reader);
            Vector2[] uv1 = ReadArray<Vector2>(reader);
            uint[] indices = ReadArray<uint>(reader);
            uint[]? colors = reader.ReadBoolean() ? ReadArray<uint>(reader) : null;
            meshes[index] = new ImportedMesh(mesh.Name, positions, normals, uv0, indices, colors, mesh.MaterialIndex)
            {
                SecondaryTextureCoordinates = uv1
            };
        }

        ImportedTexture[] textures = header.Textures;
        for (int index = 0; index < textures.Length; index++)
            textures[index] = textures[index] with { Data = ReadArray<byte>(reader) };
        if (stream.Position != stream.Length)
            throw new InvalidDataException("Unexpected trailing bytes in the rigid import snapshot.");
        return new ImportedScene(meshes, textures, header.Materials) { ImportWarnings = header.Warnings };
    }

    private static void WriteArray<T>(BinaryWriter writer, T[] values) where T : unmanaged
    {
        ReadOnlySpan<byte> bytes = MemoryMarshal.AsBytes(values.AsSpan());
        writer.Write(bytes.Length);
        writer.Write(bytes);
    }

    private static T[] ReadArray<T>(BinaryReader reader) where T : unmanaged
    {
        int length = reader.ReadInt32();
        int stride = Marshal.SizeOf<T>();
        Stream stream = reader.BaseStream;
        if (length < 0 || length % stride != 0 || length > stream.Length - stream.Position)
            throw new InvalidDataException("Invalid array length in the rigid import snapshot.");
        var values = new T[length / stride];
        stream.ReadExactly(MemoryMarshal.AsBytes(values.AsSpan()));
        return values;
    }

    private sealed record MeshHeader(string Name, int MaterialIndex);

    private sealed record Header(
        MeshHeader[] Meshes,
        ImportedTexture[] Textures,
        IReadOnlyList<ImportedMaterial> Materials,
        IReadOnlyList<string> Warnings);
}
