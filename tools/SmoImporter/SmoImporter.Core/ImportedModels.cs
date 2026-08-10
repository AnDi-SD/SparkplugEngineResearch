using System.Numerics;

namespace SmoImporter.Core;

public sealed record ImportedMesh(
    string Name,
    Vector3[] Positions,
    Vector3[] Normals,
    Vector2[] TextureCoordinates,
    uint[] TriangleIndices);

public sealed record ImportedTexture(
    string Name, string MimeType, int Width, int Height, byte[] Data);

public sealed record ImportedScene(
    IReadOnlyList<ImportedMesh> Meshes,
    IReadOnlyList<ImportedTexture>? EmbeddedTextures = null)
{
    public IReadOnlyList<ImportedTexture> Textures => EmbeddedTextures ?? [];
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
