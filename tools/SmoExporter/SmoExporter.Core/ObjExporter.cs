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
        if (!Includes(scene, SmoExportResourceTypes.Meshes) || scene.Meshes.Count == 0)
            throw new InvalidOperationException(
                "OBJ export requires at least one selected mesh.");

        bool includeMaterials = Includes(scene, SmoExportResourceTypes.Materials);
        bool includeTextures = includeMaterials &&
            Includes(scene, SmoExportResourceTypes.Textures);
        if (includeMaterials)
        {
            foreach (SmoExportMesh mesh in scene.Meshes)
            {
                ValidateAlpha(mesh.MaterialColor.W, "material factor", mesh);
                _ = GetUniformVertexAlpha(mesh);
            }
        }
        string fullPath = Path.GetFullPath(outputPath);
        string directory = Path.GetDirectoryName(fullPath)!;
        Directory.CreateDirectory(directory);
        string stem = Path.GetFileNameWithoutExtension(fullPath);
        string materialFile = stem + ".mtl";
        var obj = new StringBuilder();
        var mtl = new StringBuilder();
        obj.AppendLine("# Sparkplug SmoExporter OBJ compatibility export");
        obj.AppendLine($"# source-sha256 {scene.SourceSha256}");
        if (includeMaterials)
            obj.AppendLine($"mtllib {materialFile}");
        var writtenTextures = new Dictionary<int, string>();
        var writtenOpacityMasks = new Dictionary<int, string>();
        int vertexBase = 1;
        int uvBase = 1;
        int normalBase = 1;

        foreach (SmoExportMesh mesh in scene.Meshes)
        {
            string name = SafeName(mesh.Name, $"mesh_{mesh.ObjectIndex}");
            string material = $"material_{mesh.ObjectIndex}";
            obj.AppendLine($"o {name}");
            obj.AppendLine($"g {name}");
            if (includeMaterials)
                obj.AppendLine($"usemtl {material}");
            bool hasUv = mesh.TextureCoordinates0.Length == mesh.Positions.Length;
            bool hasNormals = mesh.Normals.Length == mesh.Positions.Length;
            foreach (Vector3 source in mesh.Positions)
            {
                Vector3 value = Vector3.Transform(source, mesh.BindWorldMatrix);
                obj.AppendLine(FormattableString.Invariant($"v {value.X:R} {value.Y:R} {value.Z:R}"));
            }
            if (hasUv)
            {
                foreach (Vector2 value in mesh.TextureCoordinates0)
                    obj.AppendLine(FormattableString.Invariant(
                        $"vt {value.X:R} {1f - value.Y:R}"));
            }
            Matrix4x4 normalMatrix = mesh.BindWorldMatrix;
            if (Matrix4x4.Invert(mesh.BindWorldMatrix, out Matrix4x4 inverseWorld))
                normalMatrix = Matrix4x4.Transpose(inverseWorld);
            if (hasNormals)
            {
                foreach (Vector3 source in mesh.Normals)
                {
                    Vector3 value = Vector3.TransformNormal(source, normalMatrix);
                    if (value.LengthSquared() > 0.000001f)
                        value = Vector3.Normalize(value);
                    obj.AppendLine(FormattableString.Invariant(
                        $"vn {value.X:R} {value.Y:R} {value.Z:R}"));
                }
            }

            bool reversesWinding = mesh.BindWorldMatrix.GetDeterminant() < 0;
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                string[] corners = new string[3];
                for (int corner = 0; corner < 3; corner++)
                {
                    int sourceCorner = reversesWinding && corner > 0 ? 3 - corner : corner;
                    int local = checked((int)mesh.TriangleIndices[index + sourceCorner]);
                    int position = vertexBase + local;
                    corners[corner] = hasUv && hasNormals
                        ? $"{position}/{uvBase + local}/{normalBase + local}"
                        : hasUv ? $"{position}/{uvBase + local}"
                        : hasNormals ? $"{position}//{normalBase + local}"
                        : position.ToString(CultureInfo.InvariantCulture);
                }
                obj.AppendLine($"f {corners[0]} {corners[1]} {corners[2]}");
            }

            if (includeMaterials)
            {
                mtl.AppendLine($"newmtl {material}");
                mtl.AppendLine(FormattableString.Invariant(
                    $"Kd {mesh.MaterialColor.X:R} {mesh.MaterialColor.Y:R} {mesh.MaterialColor.Z:R}"));
                float opacity = mesh.UsesAlphaBlend
                    ? mesh.MaterialColor.W * GetUniformVertexAlpha(mesh)
                    : 1f;
                mtl.AppendLine(FormattableString.Invariant($"d {opacity:R}"));
                mtl.AppendLine("illum 2");
                if (includeTextures && mesh.Texture is not null)
                {
                    if (!writtenTextures.TryGetValue(
                            mesh.Texture.ObjectIndex, out string? fileName))
                    {
                        fileName = $"{stem}_texture_{mesh.Texture.ObjectIndex}.png";
                        File.WriteAllBytes(
                            Path.Combine(directory, fileName),
                            mesh.Texture.OpaqueRgbPngBytes ?? mesh.Texture.PngBytes);
                        writtenTextures[mesh.Texture.ObjectIndex] = fileName;
                    }
                    mtl.AppendLine($"map_Kd {fileName}");
                    if (mesh.UsesAlphaBlend &&
                        mesh.Texture.OpacityMaskPngBytes is byte[] opacityMask)
                    {
                        if (!writtenOpacityMasks.TryGetValue(
                                mesh.Texture.ObjectIndex, out string? opacityFileName))
                        {
                            opacityFileName =
                                $"{stem}_texture_{mesh.Texture.ObjectIndex}_opacity.png";
                            File.WriteAllBytes(
                                Path.Combine(directory, opacityFileName), opacityMask);
                            writtenOpacityMasks[mesh.Texture.ObjectIndex] = opacityFileName;
                        }
                        mtl.AppendLine($"map_d {opacityFileName}");
                    }
                }
                mtl.AppendLine();
            }
            vertexBase += mesh.Positions.Length;
            if (hasUv) uvBase += mesh.TextureCoordinates0.Length;
            if (hasNormals) normalBase += mesh.Normals.Length;
        }

        File.WriteAllText(fullPath, obj.ToString(), new UTF8Encoding(false));
        if (includeMaterials)
            File.WriteAllText(
                Path.Combine(directory, materialFile), mtl.ToString(), new UTF8Encoding(false));
    }

    private static string SafeName(string value, string fallback)
    {
        if (string.IsNullOrWhiteSpace(value)) return fallback;
        var result = new string(value.Select(character =>
            char.IsWhiteSpace(character) || character is '#' or '/' or '\\' ? '_' : character).ToArray());
        return string.IsNullOrWhiteSpace(result) ? fallback : result;
    }

    private static bool Includes(
        SmoExportScene scene, SmoExportResourceTypes resource) =>
        (scene.Resources & resource) != 0;

    private static float GetUniformVertexAlpha(SmoExportMesh mesh)
    {
        if (mesh.Colors.Length == 0 || mesh.Colors.Length != mesh.Positions.Length)
            return 1f;

        float alpha = mesh.Colors[0].W;
        ValidateAlpha(alpha, "COLOR_0", mesh);
        for (int index = 1; index < mesh.Colors.Length; index++)
        {
            float value = mesh.Colors[index].W;
            ValidateAlpha(value, "COLOR_0", mesh);
            if (value != alpha)
            {
                throw new InvalidDataException(
                    $"OBJ cannot represent varying COLOR_0 alpha on mesh " +
                    $"[{mesh.ObjectIndex}] {mesh.Name} without changing its transparency.");
            }
        }
        return alpha;
    }

    private static void ValidateAlpha(
        float alpha, string source, SmoExportMesh mesh)
    {
        if (!float.IsFinite(alpha) || alpha is < 0f or > 1f)
        {
            throw new InvalidDataException(
                $"OBJ mesh [{mesh.ObjectIndex}] {mesh.Name} has an invalid " +
                $"{source} alpha value ({alpha}).");
        }
    }
}
