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

public enum SmoExportSceneMode
{
    All = 0,
    LevelOnly = 1,
    LevelWithBakedObjects = 2,
    LevelWithInstances = 3,
    SeparateMeshes = 4
}

public sealed record SmoExportOptions(
    bool ApplyWorldTransforms = true,
    IReadOnlyList<string>? AnimationPaths = null,
    SmoExportResourceTypes Resources = SmoExportResourceTypes.All,
    SmoExportSceneMode SceneMode = SmoExportSceneMode.All,
    IReadOnlySet<int>? SelectedMeshObjectIndices = null);

public sealed record SmoExportTexture(
    int ObjectIndex,
    string Name,
    int Width,
    int Height,
    byte[] PngBytes,
    byte[]? OpacityMaskPngBytes = null,
    byte[]? OpaqueRgbPngBytes = null,
    ReadOnlyMemory<byte> Bgra32Pixels = default);

/// <summary>Host export variant identity; both components remain real file indices.</summary>
public readonly record struct SmoExportMeshKey(int MeshObjectIndex, int? RenderableObjectIndex);
public readonly record struct SmoExportPlacementKey(int SceneObjectIndex, SmoRenderOccurrenceKey? OccurrenceKey);

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
    Matrix4x4 BindLocalMatrix)
{
    public int? RenderableObjectIndex { get; init; }
    public SmoLoadedMaterial? LoadedMaterial { get; init; }
    public SmoExportMeshKey VariantKey => new(ObjectIndex, RenderableObjectIndex);
}

/// <summary>
/// A scene node which places one physical mesh. Multiple placements may point
/// at the same MeshObjectIndex without duplicating vertex or index buffers.
/// </summary>
public sealed record SmoExportMeshPlacement(
    int SceneObjectIndex,
    string Name,
    int MeshObjectIndex,
    bool IsSharedInstance,
    int? StaticObjectIndex,
    int? MaterialObjectIndex,
    int? ParentNodeObjectIndex,
    Matrix4x4 WorldMatrix,
    Matrix4x4 LocalMatrix)
{
    public SmoExportMeshKey? MeshVariantKey { get; init; }
    public SmoRenderOccurrenceKey? OccurrenceKey { get; init; }
    public SmoExportMeshKey EffectiveMeshKey => MeshVariantKey ?? new(MeshObjectIndex, null);
    public SmoExportPlacementKey PlacementKey => new(SceneObjectIndex, OccurrenceKey);
}

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
    SmoExportSceneMode SceneMode,
    IReadOnlyList<SmoExportMesh> Meshes,
    IReadOnlyList<SmoExportMeshPlacement> MeshPlacements,
    IReadOnlyList<SmoExportNode> Nodes,
    IReadOnlyList<SmoExportSkin> Skins,
    IReadOnlyList<SmoExportAnimation> Animations,
    IReadOnlyList<string> Warnings);
