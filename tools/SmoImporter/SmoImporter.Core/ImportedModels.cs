using System.Numerics;

namespace SmoImporter.Core;

public sealed record ImportedMesh(
    string Name,
    Vector3[] Positions,
    Vector3[] Normals,
    Vector2[] TextureCoordinates,
    uint[] TriangleIndices,
    uint[]? DiffuseColorsArgb = null,
    int MaterialIndex = -1,
    ImportedSkinning? Skinning = null)
{
    public uint[] DiffuseColors => DiffuseColorsArgb ?? [];
}

public readonly record struct ImportedJointIndices(
    ushort X,
    ushort Y,
    ushort Z,
    ushort W);

public sealed record ImportedSkeleton(
    string Name,
    IReadOnlyList<string> JointNames,
    IReadOnlyList<Matrix4x4> InverseBindMatrices);

public sealed record ImportedSkinning(
    ImportedSkeleton Skeleton,
    ImportedJointIndices[] JointIndices,
    Vector4[] Weights);

public sealed record ImportedTexture(
    string Name, string MimeType, int Width, int Height, byte[] Data);

public sealed record ImportedMaterial(
    string Name,
    string? BaseColorTextureName = null);

public sealed record ImportedScene(
    IReadOnlyList<ImportedMesh> Meshes,
    IReadOnlyList<ImportedTexture>? EmbeddedTextures = null,
    IReadOnlyList<ImportedMaterial>? SourceMaterials = null)
{
    public IReadOnlyList<ImportedTexture> Textures => EmbeddedTextures ?? [];
    public IReadOnlyList<ImportedMaterial> Materials => SourceMaterials ?? [];
    public bool HasSkinning => Meshes.Any(mesh => mesh.Skinning is not null);
}

public sealed record ReplacementTransform(
    float Scale,
    Vector3 RotationDegrees,
    Vector3 Translation)
{
    public static ReplacementTransform Identity { get; } =
        new(1f, Vector3.Zero, Vector3.Zero);

    public Matrix4x4 Matrix =>
        Matrix4x4.CreateScale(Scale) *
        Matrix4x4.CreateFromYawPitchRoll(
            Degrees(RotationDegrees.Y),
            Degrees(RotationDegrees.X),
            Degrees(RotationDegrees.Z)) *
        Matrix4x4.CreateTranslation(Translation);

    private static float Degrees(float value) => value * MathF.PI / 180f;
}

public sealed record SmoBoneSlot(int Slot, uint ObjectId, string Name);

public sealed record ReplacementResult(
    string OutputPath,
    int VertexCount,
    int TriangleCount,
    int BoneSlot,
    string BoneName);
