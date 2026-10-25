using System.Globalization;
using System.Numerics;

namespace SmoImporter.Core;

public static class ObjModelReader
{
    public static ImportedScene Read(
        string path,
        CancellationToken cancellationToken = default)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        string fullPath = Path.GetFullPath(path);
        ImportedModelResourceLimits.ValidateInputFile(fullPath, "OBJ");
        cancellationToken.ThrowIfCancellationRequested();

        var materials = ReadMaterials(
            fullPath, File.ReadLines(fullPath), cancellationToken);
        var materialIndices = materials
            .Select((material, index) => (material.Name, index))
            .ToDictionary(item => item.Name, item => item.index, StringComparer.OrdinalIgnoreCase);
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
                    string materialName = string.Join(' ', parts.Skip(1));
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
        var warnings = new List<string>();
        AdjacentTextureResolution resolved = ResolveAdjacentTextures(
            fullPath,
            meshes,
            materials,
            warnings,
            cancellationToken);
        var scene = new ImportedScene(
            resolved.Meshes,
            resolved.Textures,
            resolved.Materials)
        {
            ImportWarnings = warnings.AsReadOnly()
        };
        ImportedModelResourceLimits.ValidateTextures(scene.Textures, "OBJ scene");
        return scene;
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

    private static List<ImportedMaterial> ReadMaterials(
        string objPath,
        IEnumerable<string> lines,
        CancellationToken cancellationToken)
    {
        string directory = Path.GetDirectoryName(objPath) ?? Directory.GetCurrentDirectory();
        var result = new List<ImportedMaterial>();
        var indices = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        var adjacentFiles = Directory.EnumerateFiles(directory)
            .ToDictionary(
                file => Path.GetFileName(file),
                file => file,
                StringComparer.OrdinalIgnoreCase);
        var libraryPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
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
                libraryPaths.Add(completeReference);
            else
            {
                foreach (string reference in Tokenize(references))
                {
                    string? materialPath = FindAdjacentFile(
                        adjacentFiles,
                        reference);
                    if (materialPath is not null)
                        libraryPaths.Add(materialPath);
                }
            }
        }
        if (libraryPaths.Count == 0)
        {
            string fallbackName = Path.GetFileNameWithoutExtension(objPath) + ".mtl";
            if (adjacentFiles.TryGetValue(fallbackName, out string? fallbackPath))
                libraryPaths.Add(fallbackPath);
        }
        foreach (string materialPath in libraryPaths.OrderBy(
                     path => path,
                     StringComparer.OrdinalIgnoreCase))
        {
            ImportedModelResourceLimits.ValidateInputFile(materialPath, "MTL");
            ReadMaterialLibrary(
                materialPath, result, indices, cancellationToken);
        }
        return result;
    }

    private static void ReadMaterialLibrary(
        string path,
        List<ImportedMaterial> materials,
        Dictionary<string, int> indices,
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
                string name = Unquote(line[7..].Trim());
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
        string objPath,
        IReadOnlyList<ImportedMesh> sourceMeshes,
        IReadOnlyList<ImportedMaterial> sourceMaterials,
        ICollection<string> warnings,
        CancellationToken cancellationToken)
    {
        string directory = Path.GetDirectoryName(objPath) ?? Directory.GetCurrentDirectory();
        AdjacentTextureCandidate[] candidates = Directory.EnumerateFiles(directory)
            .Where(IsSupportedTextureFile)
            .Select(path => new AdjacentTextureCandidate(
                Path.GetFullPath(path),
                Path.GetFileName(path),
                Path.GetFileNameWithoutExtension(path),
                IsLikelyBaseColorTexture(path)))
            .OrderBy(candidate => candidate.FileName, StringComparer.OrdinalIgnoreCase)
            .ToArray();
        AdjacentTextureCandidate[] colorCandidates = candidates
            .Where(candidate => candidate.IsLikelyBaseColor)
            .ToArray();
        var meshes = sourceMeshes.ToArray();
        var materials = sourceMaterials.ToList();
        var textures = new List<ImportedTexture>();
        var textureIndices = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        var textureHasAlpha = new Dictionary<int, bool>();
        var usedCandidatePaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        int ResolveTexture(AdjacentTextureCandidate candidate)
        {
            cancellationToken.ThrowIfCancellationRequested();
            usedCandidatePaths.Add(candidate.FullPath);
            if (textureIndices.TryGetValue(candidate.FullPath, out int existing))
                return existing;
            ImportedTexture texture = ImportedTextureFileReader.Read(candidate.FullPath);
            int index = textures.Count;
            textures.Add(texture);
            textureIndices.Add(candidate.FullPath, index);
            textureHasAlpha.Add(
                index,
                ImportedTextureImageTools.TextureContainsTransparency(texture));
            return index;
        }

        void AssignMaterial(int materialIndex, AdjacentTextureCandidate candidate)
        {
            int textureIndex = ResolveTexture(candidate);
            ImportedMaterial material = materials[materialIndex];
            materials[materialIndex] = material with
            {
                BaseColorTextureName = candidate.FileName,
                BaseColorTextureIndex = textureIndex,
                AlphaMode = textureHasAlpha[textureIndex]
                    ? ImportedMaterialAlphaMode.Blend
                    : material.AlphaMode
            };
        }

        int[] usedMaterialIndices = meshes
            .Select(mesh => mesh.MaterialIndex)
            .Where(index => index >= 0 && index < materials.Count)
            .Distinct()
            .OrderBy(index => index)
            .ToArray();
        var unresolvedMaterials = new List<int>();
        foreach (int materialIndex in usedMaterialIndices)
        {
            ImportedMaterial material = materials[materialIndex];
            if (material.BaseColorTextureIndex >= 0)
                continue;
            CandidateMatch match = string.IsNullOrWhiteSpace(
                    material.BaseColorTextureName)
                ? new CandidateMatch(null, Ambiguous: false)
                : FindCandidate(candidates, material.BaseColorTextureName);
            if (match.Candidate is null && !match.Ambiguous)
                match = FindCandidate(colorCandidates, material.Name);
            if (match.Candidate is not null)
                AssignMaterial(materialIndex, match.Candidate);
            else
            {
                unresolvedMaterials.Add(materialIndex);
                if (match.Ambiguous)
                {
                    warnings.Add(
                        $"OBJ material '{material.Name}' matches multiple adjacent " +
                        "textures and was left untextured.");
                }
            }
        }

        var unresolvedMeshGroups = new List<UnresolvedMeshGroup>();
        foreach (IGrouping<string, (ImportedMesh Mesh, int Index)> group in meshes
                     .Select((mesh, index) => (Mesh: mesh, Index: index))
                     .Where(item => item.Mesh.MaterialIndex < 0)
                     .GroupBy(item => item.Mesh.Name, StringComparer.OrdinalIgnoreCase))
        {
            CandidateMatch match = FindCandidate(colorCandidates, group.Key);
            if (match.Candidate is null)
            {
                unresolvedMeshGroups.Add(new UnresolvedMeshGroup(
                    group.Key,
                    group.Select(item => item.Index).ToArray()));
                if (match.Ambiguous)
                {
                    warnings.Add(
                        $"OBJ mesh group '{group.Key}' matches multiple adjacent " +
                        "textures and was left untextured.");
                }
                continue;
            }
            AssignNewMaterialToMeshes(
                group.Key,
                group.Select(item => item.Index).ToArray(),
                match.Candidate,
                meshes,
                materials,
                AssignMaterial);
        }

        AdjacentTextureCandidate[] unusedCandidates = colorCandidates
            .Where(candidate => !usedCandidatePaths.Contains(candidate.FullPath))
            .ToArray();
        int unresolvedGroupCount = unresolvedMaterials.Count + unresolvedMeshGroups.Count;
        if (unresolvedGroupCount == 1 && unusedCandidates.Length == 1)
        {
            AdjacentTextureCandidate fallback = unusedCandidates[0];
            if (unresolvedMaterials.Count == 1)
                AssignMaterial(unresolvedMaterials[0], fallback);
            else
            {
                UnresolvedMeshGroup group = unresolvedMeshGroups[0];
                AssignNewMaterialToMeshes(
                    group.Name,
                    group.MeshIndices,
                    fallback,
                    meshes,
                    materials,
                    AssignMaterial);
            }
            warnings.Add(
                $"OBJ adjacent texture '{fallback.FileName}' was assigned by the " +
                "unique untextured-group fallback.");
        }
        else if (candidates.Length > 0 && textures.Count == 0 && unresolvedGroupCount > 0)
        {
            warnings.Add(
                $"OBJ found {candidates.Length} adjacent image file(s), but none could " +
                "be assigned unambiguously by material or mesh name.");
        }

        return new AdjacentTextureResolution(
            Array.AsReadOnly(meshes),
            textures.AsReadOnly(),
            materials.AsReadOnly());
    }

    private static void AssignNewMaterialToMeshes(
        string name,
        IReadOnlyList<int> meshIndices,
        AdjacentTextureCandidate candidate,
        ImportedMesh[] meshes,
        List<ImportedMaterial> materials,
        Action<int, AdjacentTextureCandidate> assignMaterial)
    {
        int materialIndex = materials.Count;
        materials.Add(new ImportedMaterial(
            string.IsNullOrWhiteSpace(name) ? "OBJ_Default" : name,
            candidate.FileName));
        assignMaterial(materialIndex, candidate);
        foreach (int meshIndex in meshIndices)
            meshes[meshIndex] = meshes[meshIndex] with { MaterialIndex = materialIndex };
    }

    private static CandidateMatch FindCandidate(
        IReadOnlyList<AdjacentTextureCandidate> candidates,
        params string?[] references)
    {
        foreach (string reference in references
                     .OfType<string>()
                     .Where(reference => !string.IsNullOrWhiteSpace(reference)))
        {
            string fileName = SafeFileName(reference);
            AdjacentTextureCandidate[] exact = candidates
                .Where(candidate => candidate.FileName.Equals(
                    fileName,
                    StringComparison.OrdinalIgnoreCase))
                .ToArray();
            if (exact.Length == 1)
                return new CandidateMatch(exact[0], Ambiguous: false);
            if (exact.Length > 1)
                return new CandidateMatch(null, Ambiguous: true);

            string baseName = Path.GetFileNameWithoutExtension(fileName);
            AdjacentTextureCandidate[] byBaseName = candidates
                .Where(candidate => candidate.BaseName.Equals(
                    baseName,
                    StringComparison.OrdinalIgnoreCase))
                .ToArray();
            if (byBaseName.Length == 1)
                return new CandidateMatch(byBaseName[0], Ambiguous: false);
            if (byBaseName.Length > 1)
                return new CandidateMatch(null, Ambiguous: true);
        }
        return new CandidateMatch(null, Ambiguous: false);
    }

    private static bool IsSupportedTextureFile(string path) =>
        Path.GetExtension(path).ToLowerInvariant() is
            ".png" or ".jpg" or ".jpeg" or ".bmp" or ".tga";

    private static bool IsLikelyBaseColorTexture(string path)
    {
        string name = Path.GetFileNameWithoutExtension(path).ToLowerInvariant();
        string[] tokens = name.Split(
            ['_', '-', '.', ' '],
            StringSplitOptions.RemoveEmptyEntries);
        string[] nonColorTokens =
        [
            "normal", "norm", "nomr", "rough", "roughness", "metal",
            "metallic", "spec", "specular", "ao", "occlusion", "orm",
            "bump", "height", "displacement"
        ];
        return !tokens.Any(token => nonColorTokens.Contains(
                   token,
                   StringComparer.OrdinalIgnoreCase)) &&
               !name.EndsWith("_n", StringComparison.OrdinalIgnoreCase);
    }

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

    private static float Parse(string value) =>
        float.Parse(value, NumberStyles.Float, CultureInfo.InvariantCulture);

    private sealed record AdjacentTextureCandidate(
        string FullPath,
        string FileName,
        string BaseName,
        bool IsLikelyBaseColor);

    private sealed record CandidateMatch(
        AdjacentTextureCandidate? Candidate,
        bool Ambiguous);

    private sealed record UnresolvedMeshGroup(
        string Name,
        IReadOnlyList<int> MeshIndices);

    private sealed record AdjacentTextureResolution(
        IReadOnlyList<ImportedMesh> Meshes,
        IReadOnlyList<ImportedTexture> Textures,
        IReadOnlyList<ImportedMaterial> Materials);

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
