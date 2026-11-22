using System.Globalization;
using System.Numerics;

namespace SmoImporter.Core;

public static class ObjModelReader
{
    public static ImportedScene Read(
        string path,
        CancellationToken cancellationToken = default) =>
        ReadCore(path, null, cancellationToken);

    public static ImportedScene ReadWithMaterialLibrary(
        string path,
        string materialLibraryPath,
        CancellationToken cancellationToken = default)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(materialLibraryPath);
        string fullMaterialLibraryPath = Path.GetFullPath(materialLibraryPath);
        if (!Path.GetExtension(fullMaterialLibraryPath).Equals(".mtl", StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Выберите библиотеку материалов в формате MTL.");
        return ReadCore(path, fullMaterialLibraryPath, cancellationToken);
    }

    private static ImportedScene ReadCore(
        string path,
        string? materialLibraryPath,
        CancellationToken cancellationToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        string fullPath = Path.GetFullPath(path);
        ImportedModelResourceLimits.ValidateInputFile(fullPath, "OBJ");
        cancellationToken.ThrowIfCancellationRequested();

        var warnings = new List<string>();
        var diagnostics = new List<ImportedDiagnostic>();
        string[] libraryPaths = materialLibraryPath is null
            ? ResolveMaterialLibraries(fullPath, File.ReadLines(fullPath), warnings, diagnostics, cancellationToken)
            : [materialLibraryPath];
        var materials = new List<ImportedMaterial>();
        var materialIndices = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        var authoredMaps = new Dictionary<int, (string LibraryPath, string Reference)>();
        foreach (string libraryPath in libraryPaths)
        {
            ImportedModelResourceLimits.ValidateInputFile(libraryPath, "MTL");
            ReadMaterialLibrary(libraryPath, materials, materialIndices, authoredMaps, cancellationToken);
        }
        var positions = new List<Vector3>();
        var normals = new List<Vector3>();
        var uvs = new List<Vector2>();
        var builders = new List<Builder>();
        string objectName = Path.GetFileNameWithoutExtension(fullPath);
        int materialIndex = -1;
        Builder current = NewBuilder(objectName, materialIndex, materials);
        builders.Add(current);

        long generatedIndexCount = 0;
        int lineNumber = 0;
        foreach (string raw in File.ReadLines(fullPath))
        {
            lineNumber++;
            if ((lineNumber & 0x3FFF) == 0)
                cancellationToken.ThrowIfCancellationRequested();
            string line = raw.Trim();
            if (line.Length == 0 || line[0] == '#')
                continue;
            string[] parts = line.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            switch (parts[0])
            {
                case "v" when parts.Length >= 4:
                    positions.Add(new Vector3(Parse(parts[1]), Parse(parts[2]), Parse(parts[3])));
                    ImportedModelResourceLimits.ValidateCount(
                        positions.Count,
                        ImportedModelResourceLimits.MaximumTotalVertices,
                        "OBJ source vertex");
                    break;
                case "vn" when parts.Length >= 4:
                    normals.Add(new Vector3(Parse(parts[1]), Parse(parts[2]), Parse(parts[3])));
                    break;
                case "vt" when parts.Length >= 3:
                    uvs.Add(new Vector2(Parse(parts[1]), 1f - Parse(parts[2])));
                    break;
                case "o" or "g" when parts.Length >= 2:
                    objectName = string.Join('_', parts.Skip(1));
                    current = SwitchBuilder(
                        current, builders, objectName, materialIndex, materials);
                    ImportedModelResourceLimits.ValidateCount(
                        builders.Count,
                        ImportedModelResourceLimits.MaximumMeshes,
                        "OBJ mesh group");
                    break;
                case "usemtl" when parts.Length >= 2:
                {
                    string materialName = string.Join(' ', Tokenize(line[6..]));
                    if (materialName.Length == 0)
                        throw new InvalidDataException("OBJ содержит usemtl без имени материала.");
                    if (!materialIndices.TryGetValue(materialName, out materialIndex))
                    {
                        materialIndex = materials.Count;
                        materials.Add(new ImportedMaterial(materialName));
                        ImportedModelResourceLimits.ValidateCount(
                            materials.Count,
                            ImportedModelResourceLimits.MaximumMaterials,
                            "OBJ material");
                        materialIndices.Add(materialName, materialIndex);
                    }
                    current = SwitchBuilder(
                        current, builders, objectName, materialIndex, materials);
                    ImportedModelResourceLimits.ValidateCount(
                        builders.Count,
                        ImportedModelResourceLimits.MaximumMeshes,
                        "OBJ mesh group");
                    break;
                }
                case "f" when parts.Length >= 4:
                {
                    int[] polygon = parts.Skip(1)
                        .Select(token => current.GetVertex(token, positions, uvs, normals))
                        .ToArray();
                    for (int i = 1; i < polygon.Length - 1; i++)
                    {
                        generatedIndexCount = checked(generatedIndexCount + 3);
                        ImportedModelResourceLimits.ValidateCount(
                            generatedIndexCount,
                            ImportedModelResourceLimits.MaximumTotalIndices,
                            "OBJ generated index");
                        current.Indices.Add((uint)polygon[0]);
                        current.Indices.Add((uint)polygon[i]);
                        current.Indices.Add((uint)polygon[i + 1]);
                    }
                    break;
                }
            }
        }

        ImportedMesh[] meshes = builders
            .Where(item => item.Indices.Count > 0)
            .Select(item => item.Build())
            .ToArray();
        if (meshes.Length == 0)
            throw new InvalidDataException("OBJ contains no faces.");
        ImportedModelResourceLimits.ValidateCount(
            meshes.Sum(mesh => (long)mesh.Positions.Length),
            ImportedModelResourceLimits.MaximumTotalVertices,
            "OBJ expanded vertex");
        AdjacentTextureResolution resolved = ResolveAdjacentTextures(
            meshes,
            materials,
            authoredMaps,
            cancellationToken);
        var scene = new ImportedScene(
            resolved.Meshes,
            resolved.Textures,
            resolved.Materials)
        {
            ImportWarnings = warnings.AsReadOnly(),
            ImportDiagnostics = diagnostics.AsReadOnly(),
            ObjMaterialLibraryPaths = Array.AsReadOnly(libraryPaths)
        };
        return ImportedTextureRecovery.Recover(scene, "OBJ", cancellationToken, resolved.UnavailableReasons, inferTextureAlpha: true);
    }

    private static Builder SwitchBuilder(
        Builder current,
        ICollection<Builder> builders,
        string objectName,
        int materialIndex,
        IReadOnlyList<ImportedMaterial> materials)
    {
        string name = BuildName(objectName, materialIndex, materials);
        if (current.Indices.Count == 0)
        {
            current.Name = name;
            current.MaterialIndex = materialIndex;
            return current;
        }
        var next = new Builder(name, materialIndex);
        builders.Add(next);
        return next;
    }

    private static Builder NewBuilder(
        string objectName,
        int materialIndex,
        IReadOnlyList<ImportedMaterial> materials) =>
        new(BuildName(objectName, materialIndex, materials), materialIndex);

    private static string BuildName(
        string objectName,
        int materialIndex,
        IReadOnlyList<ImportedMaterial> materials) =>
        materialIndex >= 0 && materialIndex < materials.Count
            ? $"{objectName}_{materials[materialIndex].Name}"
            : objectName;

    private static string[] ResolveMaterialLibraries(
        string objPath,
        IEnumerable<string> lines,
        ICollection<string> warnings,
        ICollection<ImportedDiagnostic> diagnostics,
        CancellationToken cancellationToken)
    {
        string directory = Path.GetDirectoryName(objPath) ?? Directory.GetCurrentDirectory();
        var adjacentFiles = Directory.EnumerateFiles(directory)
            .ToDictionary(
                file => Path.GetFileName(file),
                file => file,
                StringComparer.OrdinalIgnoreCase);
        var libraryPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var warnedReferences = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (string raw in lines)
        {
            cancellationToken.ThrowIfCancellationRequested();
            string line = raw.Trim();
            if (!line.StartsWith("mtllib ", StringComparison.OrdinalIgnoreCase))
                continue;
            string references = line[7..].Trim();
            string? completeReference = FindAdjacentFile(
                adjacentFiles,
                Unquote(references));
            if (completeReference is not null)
            {
                libraryPaths.Add(completeReference);
                continue;
            }

            bool missingReference = false;
            foreach (string reference in Tokenize(references))
            {
                string? materialPath = FindAdjacentFile(adjacentFiles, reference);
                if (materialPath is not null)
                    libraryPaths.Add(materialPath);
                else
                    missingReference = true;
            }
            if (missingReference && warnedReferences.Count < 16 && warnedReferences.Add(references))
            {
                string message = $"OBJ: не найдена библиотека материалов из mtllib {references}.";
                warnings.Add(message);
                diagnostics.Add(new ImportedDiagnostic("MissingMtl", message, references));
            }
        }
        if (libraryPaths.Count == 0)
        {
            string fallbackName = Path.GetFileNameWithoutExtension(objPath) + ".mtl";
            if (adjacentFiles.TryGetValue(fallbackName, out string? fallbackPath))
                libraryPaths.Add(fallbackPath);
        }
        return libraryPaths.OrderBy(path => path, StringComparer.OrdinalIgnoreCase).ToArray();
    }

    private static void ReadMaterialLibrary(
        string path,
        List<ImportedMaterial> materials,
        Dictionary<string, int> indices,
        Dictionary<int, (string LibraryPath, string Reference)> authoredMaps,
        CancellationToken cancellationToken)
    {
        int current = -1;
        foreach (string raw in File.ReadLines(path))
        {
            cancellationToken.ThrowIfCancellationRequested();
            string line = raw.Trim();
            if (line.Length == 0 || line[0] == '#')
                continue;
            if (line.StartsWith("newmtl ", StringComparison.OrdinalIgnoreCase))
            {
                string name = string.Join(' ', Tokenize(line[7..]));
                if (name.Length == 0)
                    throw new InvalidDataException($"MTL '{path}' contains an empty material name.");
                if (!indices.TryGetValue(name, out current))
                {
                    current = materials.Count;
                    materials.Add(new ImportedMaterial(name));
                    ImportedModelResourceLimits.ValidateCount(
                        materials.Count,
                        ImportedModelResourceLimits.MaximumMaterials,
                        "MTL material");
                    indices.Add(name, current);
                }
                continue;
            }
            if (current >= 0 && line.StartsWith("map_Kd ", StringComparison.OrdinalIgnoreCase))
            {
                string? textureName = ParseMapReference(line[7..]);
                if (!string.IsNullOrWhiteSpace(textureName))
                {
                    authoredMaps[current] = (path, textureName);
                    materials[current] = materials[current] with
                    {
                        BaseColorTextureName = SafeFileName(textureName)
                    };
                }
                continue;
            }
            if (current >= 0 && line.StartsWith("map_d ", StringComparison.OrdinalIgnoreCase))
            {
                materials[current] = materials[current] with
                {
                    AlphaMode = ImportedMaterialAlphaMode.Blend
                };
                continue;
            }
            if (current >= 0 && TryReadDissolve(line, out bool usesAlpha) && usesAlpha)
            {
                materials[current] = materials[current] with
                {
                    AlphaMode = ImportedMaterialAlphaMode.Blend
                };
            }
        }
    }

    private static AdjacentTextureResolution ResolveAdjacentTextures(
        IReadOnlyList<ImportedMesh> sourceMeshes,
        IReadOnlyList<ImportedMaterial> sourceMaterials,
        IReadOnlyDictionary<int, (string LibraryPath, string Reference)> authoredMaps,
        CancellationToken cancellationToken)
    {
        var materials = sourceMaterials.ToArray();
        var textures = new List<ImportedTexture>();
        var indices = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        var failures = new Dictionary<int, string>();
        foreach (int materialIndex in sourceMeshes.Select(mesh => mesh.MaterialIndex)
                     .Where(index => index >= 0 && index < materials.Length).Distinct().OrderBy(index => index))
        {
            cancellationToken.ThrowIfCancellationRequested();
            ImportedMaterial material = materials[materialIndex];
            string? reference = material.BaseColorTextureName;
            string? texturePath = null;
            if (authoredMaps.TryGetValue(materialIndex, out var authoredMap))
            {
                reference = authoredMap.Reference;
                texturePath = ResolveMaterialTexturePath(authoredMap.LibraryPath, reference);
            }
            // An absent authored binding stays absent. Nearby images, matching
            // mesh names, or a single unused image are not evidence of intent.
            if (string.IsNullOrWhiteSpace(reference)) continue;
            if (texturePath is null)
            {
                failures[materialIndex] = $"не найдена указанная текстура «{reference}»";
                continue;
            }
            if (!indices.TryGetValue(texturePath, out int textureIndex))
            {
                if (!ImportedTextureRecovery.TryReadReferencedFile(texturePath, out ImportedTexture? texture, out string? issue))
                {
                    failures[materialIndex] = $"текстура «{reference}» недоступна или повреждена: {issue}";
                    continue;
                }
                textureIndex = textures.Count;
                indices.Add(texturePath, textureIndex);
                textures.Add(texture!);
            }
            materials[materialIndex] = material with { BaseColorTextureIndex = textureIndex };
        }
        return new AdjacentTextureResolution(
            sourceMeshes, textures.AsReadOnly(), Array.AsReadOnly(materials), failures);
    }
    private static string? ResolveMaterialTexturePath(string materialLibraryPath, string reference)
    {
        string relative = reference.Replace('\\', Path.DirectorySeparatorChar)
            .Replace('/', Path.DirectorySeparatorChar);
        if (Path.IsPathRooted(relative) || relative.Contains(':'))
            return null;

        string directory = Path.GetDirectoryName(materialLibraryPath)!;
        string prefix = Path.TrimEndingDirectorySeparator(directory);
        if (!Path.EndsInDirectorySeparator(prefix))
            prefix += Path.DirectorySeparatorChar;
        string candidate = Path.GetFullPath(relative, directory);
        return candidate.StartsWith(prefix, StringComparison.OrdinalIgnoreCase) &&
            IsSupportedTextureFile(candidate) && File.Exists(candidate) ? candidate : null;
    }

    private static bool IsSupportedTextureFile(string path) =>
        Path.GetExtension(path).ToLowerInvariant() is
            ".png" or ".jpg" or ".jpeg" or ".bmp" or ".tga";

    private static string? FindAdjacentFile(
        IReadOnlyDictionary<string, string> adjacentFiles,
        string reference)
    {
        string fileName = SafeFileName(reference);
        return adjacentFiles.TryGetValue(fileName, out string? path)
            ? path
            : null;
    }

    private static string SafeFileName(string value)
    {
        string normalized = Unquote(value.Trim()).Replace('\\', Path.DirectorySeparatorChar)
            .Replace('/', Path.DirectorySeparatorChar);
        return Path.GetFileName(normalized);
    }

    private static string? ParseMapReference(string value)
    {
        string[] tokens = Tokenize(value).ToArray();
        int index = 0;
        while (index < tokens.Length && tokens[index].StartsWith('-'))
        {
            string option = tokens[index++].ToLowerInvariant();
            if (option is "-o" or "-s" or "-t")
            {
                int numericCount = 0;
                while (index < tokens.Length && numericCount < 3 &&
                       float.TryParse(
                           tokens[index],
                           NumberStyles.Float,
                           CultureInfo.InvariantCulture,
                           out _))
                {
                    index++;
                    numericCount++;
                }
                continue;
            }
            int argumentCount = option switch
            {
                "-mm" => 2,
                "-blendu" or "-blendv" or "-boost" or "-bm" or "-cc" or
                "-clamp" or "-imfchan" or "-texres" or "-type" => 1,
                _ => 0
            };
            index = Math.Min(tokens.Length, index + argumentCount);
        }
        return index < tokens.Length
            ? string.Join(' ', tokens.Skip(index))
            : null;
    }

    private static bool TryReadDissolve(string line, out bool usesAlpha)
    {
        usesAlpha = false;
        string[] tokens = Tokenize(line).ToArray();
        if (tokens.Length < 2)
            return false;
        bool isDissolve = tokens[0].Equals("d", StringComparison.OrdinalIgnoreCase);
        bool isTransparency = tokens[0].Equals("Tr", StringComparison.OrdinalIgnoreCase);
        if (!isDissolve && !isTransparency)
            return false;
        string? numeric = tokens.Skip(1).LastOrDefault(token => float.TryParse(
            token,
            NumberStyles.Float,
            CultureInfo.InvariantCulture,
            out _));
        if (numeric is null || !float.TryParse(
                numeric,
                NumberStyles.Float,
                CultureInfo.InvariantCulture,
                out float value))
            return false;
        usesAlpha = isDissolve ? value < 0.999f : value > 0.001f;
        return true;
    }

    private static IReadOnlyList<string> Tokenize(string value)
    {
        var result = new List<string>();
        var current = new System.Text.StringBuilder();
        char quote = '\0';
        foreach (char character in value)
        {
            if (quote == '\0' && character == '#')
                break;
            if (character is '\'' or '"')
            {
                if (quote == '\0')
                {
                    quote = character;
                    continue;
                }
                if (quote == character)
                {
                    quote = '\0';
                    continue;
                }
            }
            if (quote == '\0' && char.IsWhiteSpace(character))
            {
                if (current.Length > 0)
                {
                    result.Add(current.ToString());
                    current.Clear();
                }
                continue;
            }
            current.Append(character);
        }
        if (current.Length > 0)
            result.Add(current.ToString());
        return result;
    }

    private static string Unquote(string value) =>
        value.Length >= 2 && value[0] == '"' && value[^1] == '"'
            ? value[1..^1]
            : value;

    private static float Parse(string value)
    {
        if (!float.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out float result) ||
            !float.IsFinite(result))
            throw new InvalidDataException($"OBJ: недопустимое числовое значение «{value}»; исправьте координаты, нормали или UV модели.");
        return result;
    }

    private sealed record AdjacentTextureResolution(
        IReadOnlyList<ImportedMesh> Meshes,
        IReadOnlyList<ImportedTexture> Textures,
        IReadOnlyList<ImportedMaterial> Materials,
        IReadOnlyDictionary<int, string> UnavailableReasons);

    private sealed class Builder(string name, int materialIndex)
    {
        private readonly Dictionary<(int P, int T, int N), int> _vertices = [];
        public string Name { get; set; } = name;
        public int MaterialIndex { get; set; } = materialIndex;
        public List<Vector3> Positions { get; } = [];
        public List<Vector3> Normals { get; } = [];
        public List<Vector2> Uvs { get; } = [];
        public List<uint> Indices { get; } = [];
        private bool _allNormals = true;
        private bool _allUvs = true;

        public int GetVertex(
            string token,
            IReadOnlyList<Vector3> positions,
            IReadOnlyList<Vector2> uvs,
            IReadOnlyList<Vector3> normals)
        {
            string[] fields = token.Split('/');
            int p = Resolve(fields[0], positions.Count);
            int t = fields.Length > 1 && fields[1].Length > 0
                ? Resolve(fields[1], uvs.Count) : -1;
            int n = fields.Length > 2 && fields[2].Length > 0
                ? Resolve(fields[2], normals.Count) : -1;
            if (_vertices.TryGetValue((p, t, n), out int existing))
                return existing;
            int index = Positions.Count;
            _vertices[(p, t, n)] = index;
            Positions.Add(positions[p]);
            if (t >= 0)
                Uvs.Add(uvs[t]);
            else
                _allUvs = false;
            if (n >= 0)
                Normals.Add(normals[n]);
            else
                _allNormals = false;
            return index;
        }

        public ImportedMesh Build() => new(
            Name,
            Positions.ToArray(),
            _allNormals && Normals.Count == Positions.Count ? Normals.ToArray() : [],
            _allUvs && Uvs.Count == Positions.Count ? Uvs.ToArray() : [],
            Indices.ToArray(),
            MaterialIndex: MaterialIndex);

        private static int Resolve(string value, int count)
        {
            int parsed = int.Parse(value, CultureInfo.InvariantCulture);
            int index = parsed > 0 ? parsed - 1 : count + parsed;
            if ((uint)index >= (uint)count)
                throw new InvalidDataException("OBJ index is outside its source array.");
            return index;
        }
    }
}
