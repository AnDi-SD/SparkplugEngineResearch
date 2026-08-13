using System.Globalization;
using System.Numerics;
using System.Text;

namespace SmoExporter.Core;

public static class ObjExporter
{
    public static void Export(SmoExportScene scene, string outputPath)
    {
        ArgumentNullException.ThrowIfNull(scene);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        string fullPath = Path.GetFullPath(outputPath);
        string directory = Path.GetDirectoryName(fullPath)!;
        Directory.CreateDirectory(directory);
        string stem = Path.GetFileNameWithoutExtension(fullPath);
        string materialFile = stem + ".mtl";
        var obj = new StringBuilder();
        var mtl = new StringBuilder();
        obj.AppendLine("# Sparkplug SmoExporter OBJ compatibility export");
        obj.AppendLine($"# source-sha256 {scene.SourceSha256}");
        obj.AppendLine($"mtllib {materialFile}");
        var writtenTextures = new Dictionary<int, string>();
        int vertexBase = 1;
        int uvBase = 1;
        int normalBase = 1;

        foreach (SmoExportMesh mesh in scene.Meshes)
        {
            string name = SafeName(mesh.Name, $"mesh_{mesh.ObjectIndex}");
            string material = $"material_{mesh.ObjectIndex}";
            obj.AppendLine($"o {name}");
            obj.AppendLine($"g {name}");
            obj.AppendLine($"usemtl {material}");
            foreach (Vector3 source in mesh.Positions)
            {
                Vector3 value = Vector3.Transform(source, mesh.BindWorldMatrix);
                obj.AppendLine(FormattableString.Invariant($"v {value.X:R} {value.Y:R} {value.Z:R}"));
            }
            foreach (Vector2 value in mesh.TextureCoordinates0)
                obj.AppendLine(FormattableString.Invariant($"vt {value.X:R} {1f - value.Y:R}"));
            Matrix4x4 normalMatrix = mesh.BindWorldMatrix;
            if (Matrix4x4.Invert(mesh.BindWorldMatrix, out Matrix4x4 inverseWorld))
                normalMatrix = Matrix4x4.Transpose(inverseWorld);
            foreach (Vector3 source in mesh.Normals)
            {
                Vector3 value = Vector3.TransformNormal(source, normalMatrix);
                if (value.LengthSquared() > 0.000001f)
                    value = Vector3.Normalize(value);
                obj.AppendLine(FormattableString.Invariant(
                    $"vn {value.X:R} {value.Y:R} {value.Z:R}"));
            }

            bool hasUv = mesh.TextureCoordinates0.Length == mesh.Positions.Length;
            bool hasNormals = mesh.Normals.Length == mesh.Positions.Length;
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                string[] corners = new string[3];
                for (int corner = 0; corner < 3; corner++)
                {
                    int local = checked((int)mesh.TriangleIndices[index + corner]);
                    int position = vertexBase + local;
                    corners[corner] = hasUv && hasNormals
                        ? $"{position}/{uvBase + local}/{normalBase + local}"
                        : hasUv ? $"{position}/{uvBase + local}"
                        : hasNormals ? $"{position}//{normalBase + local}"
                        : position.ToString(CultureInfo.InvariantCulture);
                }
                obj.AppendLine($"f {corners[0]} {corners[1]} {corners[2]}");
            }

            mtl.AppendLine($"newmtl {material}");
            mtl.AppendLine(FormattableString.Invariant(
                $"Kd {mesh.MaterialColor.X:R} {mesh.MaterialColor.Y:R} {mesh.MaterialColor.Z:R}"));
            mtl.AppendLine(FormattableString.Invariant($"d {mesh.MaterialColor.W:R}"));
            mtl.AppendLine("illum 2");
            if (mesh.Texture is not null)
            {
                if (!writtenTextures.TryGetValue(mesh.Texture.ObjectIndex, out string? fileName))
                {
                    fileName = $"{stem}_texture_{mesh.Texture.ObjectIndex}.png";
                    File.WriteAllBytes(Path.Combine(directory, fileName), mesh.Texture.PngBytes);
                    writtenTextures[mesh.Texture.ObjectIndex] = fileName;
                }
                mtl.AppendLine($"map_Kd {fileName}");
                if (mesh.UsesAlphaBlend)
                    mtl.AppendLine($"map_d {fileName}");
            }
            mtl.AppendLine();
            vertexBase += mesh.Positions.Length;
            if (hasUv) uvBase += mesh.TextureCoordinates0.Length;
            if (hasNormals) normalBase += mesh.Normals.Length;
        }

        File.WriteAllText(fullPath, obj.ToString(), new UTF8Encoding(false));
        File.WriteAllText(Path.Combine(directory, materialFile), mtl.ToString(), new UTF8Encoding(false));
    }

    private static string SafeName(string value, string fallback)
    {
        if (string.IsNullOrWhiteSpace(value)) return fallback;
        var result = new string(value.Select(character =>
            char.IsWhiteSpace(character) || character is '#' or '/' or '\\' ? '_' : character).ToArray());
        return string.IsNullOrWhiteSpace(result) ? fallback : result;
    }
}
