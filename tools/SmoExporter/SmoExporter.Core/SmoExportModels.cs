using System.Numerics;
using SmoViewer.Core;

namespace SmoExporter.Core;

public sealed record SmoExportOptions(bool ApplyWorldTransforms = true);

public sealed record SmoExportTexture(
    int ObjectIndex,
    string Name,
    int Width,
    int Height,
    byte[] PngBytes);

public sealed record SmoExportMesh(
    int ObjectIndex,
    uint ObjectId,
    string Name,
    byte Marker,
    uint PrimitiveType,
    uint VertexFormat,
    int SerializedStride,
    int RuntimeStride,
    Vector3[] Positions,
    Vector3[] Normals,
    Vector2[] TextureCoordinates0,
    Vector2[] TextureCoordinates1,
    Vector4[] Colors,
    uint[] TriangleIndices,
    SmoExportTexture? Texture,
    Vector4 MaterialColor);

public sealed record SmoExportScene(
    string SourcePath,
    string SourceSha256,
    uint PlatformFlags,
    IReadOnlyList<SmoExportMesh> Meshes,
    IReadOnlyList<string> Warnings);
