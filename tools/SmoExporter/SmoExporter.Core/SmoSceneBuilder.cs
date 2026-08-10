using System.Numerics;
using System.Security.Cryptography;
using SmoViewer.Core;

namespace SmoExporter.Core;

public static class SmoSceneBuilder
{
    public static SmoExportScene Build(
        SmoDocument document,
        SmoExportOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(document);
        options ??= new SmoExportOptions();
        var warnings = new List<string>();
        var meshes = new List<SmoExportMesh>();
        IReadOnlyDictionary<int, SmoTextureBinding> textures =
            SmoTextureBindingResolver.ResolveAll(document);
        IReadOnlyDictionary<int, uint> materialColors =
            SmoMaterialColorResolver.ResolveAll(document);

        foreach (SmoObjectEntry entry in document.Objects.Where(
                     item => item.TypeHash == SmoClassIds.MeshData))
        {
            if (!SmoMeshDecoder.TryDecode(document, entry, out SmoMesh? mesh, out string error) ||
                mesh is null)
            {
                warnings.Add(error);
                continue;
            }

            Matrix4x4 world = options.ApplyWorldTransforms
                ? SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, entry)
                : Matrix4x4.Identity;
            Matrix4x4 normalMatrix = world;
            if (Matrix4x4.Invert(world, out Matrix4x4 inverse))
                normalMatrix = Matrix4x4.Transpose(inverse);

            Vector3[] positions = mesh.Positions.Select(value =>
            {
                Vector3 transformed = Vector3.Transform(value, world);
                return new Vector3(transformed.X, transformed.Y, -transformed.Z);
            }).ToArray();
            Vector3[] normals = mesh.HasNormals
                ? mesh.Normals.Select(value =>
                {
                    Vector3 transformed = Vector3.TransformNormal(value, normalMatrix);
                    transformed.Z = -transformed.Z;
                    return transformed.LengthSquared() > 0.000001f
                        ? Vector3.Normalize(transformed)
                        : Vector3.UnitY;
                }).ToArray()
                : [];
            uint[] triangles = mesh.TriangleIndices.ToArray();
            for (int index = 0; index < triangles.Length; index += 3)
                (triangles[index + 1], triangles[index + 2]) =
                    (triangles[index + 2], triangles[index + 1]);

            SmoExportTexture? texture = null;
            if (textures.TryGetValue(entry.Index, out SmoTextureBinding? binding))
            {
                if (binding.Issue is not null)
                    warnings.Add(binding.Issue);
                else if (binding.Texture is not null)
                {
                    SmoTexture source = binding.BaseTexture ?? binding.Texture;
                    texture = new SmoExportTexture(
                        source.ObjectIndex,
                        source.Name,
                        source.Width,
                        source.Height,
                        PngEncoder.EncodeBgra32(
                            source.Width, source.Height, source.Bgra32Pixels.Span));
                }
            }

            Vector4 materialColor = materialColors.TryGetValue(entry.Index, out uint argb)
                ? DecodeArgb(argb)
                : Vector4.One;
            // Some skinned assets serialize an all-zero diffuse channel as a
            // placeholder. glTF COLOR_0 multiplies baseColorTexture, so exporting
            // that placeholder would turn a valid textured model completely black.
            // Keep COLOR_0 only when it carries actual RGB information.
            bool hasRenderableDiffuse = mesh.HasDiffuseColors &&
                mesh.DiffuseColorsArgb.Any(color => (color & 0x00FFFFFF) != 0);
            Vector4[] colors = hasRenderableDiffuse
                ? mesh.DiffuseColorsArgb.Select(DecodeArgb).ToArray()
                : [];
            meshes.Add(new SmoExportMesh(
                entry.Index,
                entry.Id,
                mesh.Name,
                mesh.Marker,
                mesh.PrimitiveType,
                mesh.VertexFormat,
                mesh.Stride,
                mesh.RuntimeStride,
                positions,
                normals,
                mesh.TextureCoordinates.ToArray(),
                mesh.TextureCoordinates1.ToArray(),
                colors,
                triangles,
                texture,
                materialColor));
        }

        string sourcePath = document.SourcePath ?? "memory.smo";
        string hash = Convert.ToHexString(SHA256.HashData(document.Data.Span));
        return new SmoExportScene(
            sourcePath, hash, document.Header.Version, meshes, warnings);
    }

    private static Vector4 DecodeArgb(uint argb) => new(
        ((argb >> 16) & 0xFF) / 255f,
        ((argb >> 8) & 0xFF) / 255f,
        (argb & 0xFF) / 255f,
        (argb >> 24) / 255f);
}
