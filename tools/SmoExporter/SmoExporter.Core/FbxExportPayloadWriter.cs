using System.Numerics;
using System.Text;
using SmoViewer.Core;

namespace SmoExporter.Core;

internal static class FbxExportPayloadWriter
{
    private static readonly byte[] Magic = "SMOFBXE1"u8.ToArray();
    // v3 textures with material alpha < 1 contain the combined opacity already.
    // v4 adds transport mesh ordinals and actual reference-slot provenance.
    private const uint ProtocolVersion = 4;

    public static void Write(SmoExportScene scene, string path)
    {
        using var stream = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None);
        using var writer = new BinaryWriter(stream, new UTF8Encoding(false, true));
        writer.Write(Magic);
        writer.Write(ProtocolVersion);
        writer.Write((uint)scene.Resources);
        writer.Write((uint)scene.SceneMode);
        WriteString(writer, scene.SourcePath);
        var variants = new Dictionary<(SmoExportTexture Texture, float Alpha), SmoExportTexture>();
        var meshOrdinals = scene.Meshes.Select((mesh, ordinal) => (mesh.VariantKey, ordinal))
            .ToDictionary(value => value.VariantKey, value => value.ordinal);
        WriteItems(writer, scene.Meshes, (output, mesh) =>
        {
            SmoExportTexture? texture = mesh.Texture;
            if (FbxExporter.RequiresTextureAlphaBake(scene, mesh) && texture is not null)
            {
                var key = (texture, mesh.MaterialColor.W);
                if (!variants.TryGetValue(key, out SmoExportTexture? variant))
                {
                    byte[] opacity = PngEncoder.EncodeMultipliedOpacity(
                        texture.Width, texture.Height, texture.Bgra32Pixels.Span,
                        mesh.MaterialColor.W);
                    variant = texture with
                    {
                        PngBytes = texture.OpaqueRgbPngBytes ?? PngEncoder.EncodeBgr24(
                            texture.Width, texture.Height, texture.Bgra32Pixels.Span),
                        OpacityMaskPngBytes = opacity
                    };
                    variants.Add(key, variant);
                }
                mesh = mesh with { Texture = variant };
            }
            output.Write(meshOrdinals[mesh.VariantKey]); // host transport index, never a file object ID
            WriteMesh(output, mesh);
        });
        WriteItems(writer, scene.MeshPlacements, (output, placement) =>
        {
            output.Write(meshOrdinals[placement.EffectiveMeshKey]);
            output.Write(placement.OccurrenceKey?.ContainerObjectIndex ?? -1);
            output.Write(placement.OccurrenceKey?.MemberSlot ?? -1);
            WritePlacement(output, placement);
        });
        WriteItems(writer, scene.Nodes, WriteNode);
        WriteItems(writer, scene.Skins, WriteSkin);
        WriteItems(writer, scene.Animations, WriteAnimation);
    }

    private static void WritePlacement(
        BinaryWriter writer, SmoExportMeshPlacement placement)
    {
        writer.Write(placement.SceneObjectIndex);
        WriteString(writer, placement.Name);
        writer.Write(placement.MeshObjectIndex);
        writer.Write(placement.IsSharedInstance);
        writer.Write(placement.StaticObjectIndex ?? -1);
        writer.Write(placement.MaterialObjectIndex ?? -1);
        writer.Write(placement.ParentNodeObjectIndex ?? -1);
        WriteMatrix(writer, placement.WorldMatrix);
        WriteMatrix(writer, placement.LocalMatrix);
    }

    private static void WriteMesh(BinaryWriter writer, SmoExportMesh mesh)
    {
        writer.Write(mesh.ObjectIndex);
        writer.Write(mesh.ObjectId);
        WriteString(writer, mesh.Name);
        WriteArray(writer, mesh.Positions, WriteVector3);
        WriteArray(writer, mesh.Normals, WriteVector3);
        WriteArray(writer, mesh.TextureCoordinates0, WriteVector2);
        WriteArray(writer, mesh.TextureCoordinates1, WriteVector2);
        WriteArray(writer, mesh.Colors, WriteVector4);
        WriteArray(writer, mesh.BlendWeights, WriteVector4);
        WriteArray(writer, mesh.JointIndices, WriteVector4);
        WriteArray(writer, mesh.TriangleIndices, static (output, value) => output.Write(value));
        WriteTexture(writer, mesh.Texture);
        WriteTexture(writer, mesh.EffectTexture);
        WriteVector4(writer, mesh.MaterialColor);
        writer.Write(mesh.UsesAlphaBlend);
        writer.Write(mesh.SkinObjectIndex ?? -1);
        writer.Write(mesh.ParentNodeObjectIndex ?? -1);
        WriteMatrix(writer, mesh.BindWorldMatrix);
        WriteMatrix(writer, mesh.BindLocalMatrix);
    }

    private static void WriteTexture(BinaryWriter writer, SmoExportTexture? texture)
    {
        writer.Write(texture is not null);
        if (texture is null) return;
        writer.Write(texture.ObjectIndex);
        WriteString(writer, texture.Name);
        writer.Write(texture.Width);
        writer.Write(texture.Height);
        WriteBytes(writer, texture.PngBytes);
        WriteBytes(writer, texture.OpacityMaskPngBytes ?? []);
        WriteBytes(writer, texture.OpaqueRgbPngBytes ?? []);
    }

    private static void WriteNode(BinaryWriter writer, SmoExportNode node)
    {
        writer.Write(node.ObjectIndex);
        WriteString(writer, node.Name);
        writer.Write(node.ParentObjectIndex ?? -1);
        WriteMatrix(writer, node.BindWorldMatrix);
        WriteMatrix(writer, node.BindLocalMatrix);
    }

    private static void WriteSkin(BinaryWriter writer, SmoExportSkin skin)
    {
        writer.Write(skin.ObjectIndex);
        WriteString(writer, skin.Name);
        WriteArray(writer, skin.JointObjectIndices, static (output, value) => output.Write(value));
        WriteArray(writer, skin.InverseBindMatrices, WriteMatrix);
    }

    private static void WriteAnimation(BinaryWriter writer, SmoExportAnimation animation)
    {
        WriteString(writer, animation.Name);
        writer.Write(animation.Duration);
        WriteItems(writer, animation.Tracks, WriteTrack);
    }

    private static void WriteTrack(BinaryWriter writer, SmoExportAnimationTrack track)
    {
        writer.Write(track.NodeObjectIndex);
        WriteString(writer, track.NodeName);
        WriteArray(writer, track.Positions, static (output, key) =>
        {
            output.Write(key.Time);
            WriteVector3(output, key.Value);
        });
        WriteArray(writer, track.Rotations, static (output, key) =>
        {
            output.Write(key.Time);
            WriteVector4(output, new Vector4(key.Value.X, key.Value.Y, key.Value.Z, key.Value.W));
        });
        WriteArray(writer, track.Scales, static (output, key) =>
        {
            output.Write(key.Time);
            WriteVector3(output, key.Value);
        });
    }

    private static void WriteItems<T>(
        BinaryWriter writer,
        IReadOnlyList<T> values,
        Action<BinaryWriter, T> write) => WriteArray(writer, values, write);

    private static void WriteArray<T>(
        BinaryWriter writer,
        IReadOnlyList<T> values,
        Action<BinaryWriter, T> write)
    {
        writer.Write(checked((uint)values.Count));
        foreach (T value in values) write(writer, value);
    }

    private static void WriteBytes(BinaryWriter writer, byte[] values)
    {
        writer.Write(checked((uint)values.Length));
        writer.Write(values);
    }

    private static void WriteString(BinaryWriter writer, string value)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(value ?? string.Empty);
        writer.Write(checked((uint)bytes.Length));
        writer.Write(bytes);
    }

    private static void WriteVector2(BinaryWriter writer, Vector2 value)
    {
        writer.Write(value.X);
        writer.Write(value.Y);
    }

    private static void WriteVector3(BinaryWriter writer, Vector3 value)
    {
        writer.Write(value.X);
        writer.Write(value.Y);
        writer.Write(value.Z);
    }

    private static void WriteVector4(BinaryWriter writer, Vector4 value)
    {
        writer.Write(value.X);
        writer.Write(value.Y);
        writer.Write(value.Z);
        writer.Write(value.W);
    }

    private static void WriteMatrix(BinaryWriter writer, Matrix4x4 value)
    {
        writer.Write(value.M11); writer.Write(value.M12); writer.Write(value.M13); writer.Write(value.M14);
        writer.Write(value.M21); writer.Write(value.M22); writer.Write(value.M23); writer.Write(value.M24);
        writer.Write(value.M31); writer.Write(value.M32); writer.Write(value.M33); writer.Write(value.M34);
        writer.Write(value.M41); writer.Write(value.M42); writer.Write(value.M43); writer.Write(value.M44);
    }
}
