using System.Numerics;
using SmoViewer.Core;

namespace SmoExporter.Core;

[Flags]
public enum SmoExportResourceTypes
{
    None = 0,
    Meshes = 1,
    Skeleton = 2,
    Materials = 4,
    Textures = 8,
    Animations = 16,
    ServiceNodes = 32,
    All = 63
}

public sealed record SmoExportOptions(
    bool ApplyWorldTransforms = true,
    IReadOnlyList<string>? AnimationPaths = null,
    SmoExportResourceTypes Resources = SmoExportResourceTypes.All);

public sealed record SmoExportTexture(
    int ObjectIndex,
    string Name,
    int Width,
    int Height,
    byte[] PngBytes,
    byte[]? OpacityMaskPngBytes = null,
    byte[]? OpaqueRgbPngBytes = null);

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
    SmoExportTexture? EffectTexture,
    Vector4 MaterialColor,
    bool UsesAlphaBlend,
    int? SkinObjectIndex,
    int? ParentNodeObjectIndex,
    Matrix4x4 BindWorldMatrix,
    Matrix4x4 BindLocalMatrix);

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
    SmoExportResourceTypes Resources,
    IReadOnlyList<SmoExportMesh> Meshes,
    IReadOnlyList<SmoExportNode> Nodes,
    IReadOnlyList<SmoExportSkin> Skins,
    IReadOnlyList<SmoExportAnimation> Animations,
    IReadOnlyList<string> Warnings);
