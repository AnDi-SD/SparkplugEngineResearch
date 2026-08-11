using System.Numerics;
using SmoViewer.Core;

namespace SmoExporter.Core;

public sealed record SmoExportOptions(
    bool ApplyWorldTransforms = true,
    IReadOnlyList<string>? AnimationPaths = null);

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
    Vector4[] BlendWeights,
    Vector4[] JointIndices,
    uint[] TriangleIndices,
    SmoExportTexture? Texture,
    Vector4 MaterialColor,
    int? SkinObjectIndex);

public sealed record SmoExportNode(
    int ObjectIndex,
    string Name,
    int? ParentObjectIndex,
    Matrix4x4 BindWorldMatrix,
    Matrix4x4 BindLocalMatrix);

public sealed record SmoExportSkin(
    int ObjectIndex,
    string Name,
    IReadOnlyList<int> JointObjectIndices,
    IReadOnlyList<Matrix4x4> InverseBindMatrices);

public sealed record SmoExportAnimationTrack(
    int NodeObjectIndex,
    string NodeName,
    IReadOnlyList<SmoAnimationKey<Vector3>> Positions,
    IReadOnlyList<SmoAnimationKey<Quaternion>> Rotations,
    IReadOnlyList<SmoAnimationKey<Vector3>> Scales);

public sealed record SmoExportAnimation(
    string Name,
    float Duration,
    IReadOnlyList<SmoExportAnimationTrack> Tracks);

public sealed record SmoExportScene(
    string SourcePath,
    string SourceSha256,
    uint PlatformFlags,
    IReadOnlyList<SmoExportMesh> Meshes,
    IReadOnlyList<SmoExportNode> Nodes,
    IReadOnlyList<SmoExportSkin> Skins,
    IReadOnlyList<SmoExportAnimation> Animations,
    IReadOnlyList<string> Warnings);
