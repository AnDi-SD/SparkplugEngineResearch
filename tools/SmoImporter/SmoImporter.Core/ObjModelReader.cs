using System.Globalization;
using System.Numerics;

namespace SmoImporter.Core;

public static class ObjModelReader
{
    public static ImportedScene Read(string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        string fullPath = Path.GetFullPath(path);
        if (!File.Exists(fullPath))
            throw new FileNotFoundException("OBJ model was not found.", fullPath);

        string[] lines = File.ReadAllLines(fullPath);
        var materials = ReadMaterials(fullPath, lines);
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

        foreach (string raw in lines)
        {
            string line = raw.Trim();
            if (line.Length == 0 || line[0] == '#')
                continue;
            string[] parts = line.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            switch (parts[0])
            {
                case "v" when parts.Length >= 4:
                    positions.Add(new Vector3(Parse(parts[1]), Parse(parts[2]), Parse(parts[3])));
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
                    break;
                case "usemtl" when parts.Length >= 2:
                {
                    string materialName = string.Join(' ', parts.Skip(1));
                    if (!materialIndices.TryGetValue(materialName, out materialIndex))
                    {
                        materialIndex = materials.Count;
                        materials.Add(new ImportedMaterial(materialName));
                        materialIndices.Add(materialName, materialIndex);
                    }
                    current = SwitchBuilder(
                        current, builders, objectName, materialIndex, materials);
                    break;
                }
                case "f" when parts.Length >= 4:
                {
                    int[] polygon = parts.Skip(1)
                        .Select(token => current.GetVertex(token, positions, uvs, normals))
                        .ToArray();
                    for (int i = 1; i < polygon.Length - 1; i++)
                    {
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
        return new ImportedScene(meshes, SourceMaterials: materials.AsReadOnly());
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
        IEnumerable<string> lines)
    {
        string directory = Path.GetDirectoryName(objPath) ?? Directory.GetCurrentDirectory();
        var result = new List<ImportedMaterial>();
        var indices = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        foreach (string raw in lines)
        {
            string line = raw.Trim();
            if (!line.StartsWith("mtllib ", StringComparison.OrdinalIgnoreCase))
                continue;
            string reference = Unquote(line[7..].Trim());
            string materialPath = Path.GetFullPath(Path.Combine(directory, reference));
            if (!File.Exists(materialPath))
                continue;
            ReadMaterialLibrary(materialPath, result, indices);
        }
        return result;
    }

    private static void ReadMaterialLibrary(
        string path,
        List<ImportedMaterial> materials,
        Dictionary<string, int> indices)
    {
        int current = -1;
        foreach (string raw in File.ReadLines(path))
        {
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
                    indices.Add(name, current);
                }
                continue;
            }
            if (current >= 0 && line.StartsWith("map_Kd ", StringComparison.OrdinalIgnoreCase))
            {
                string textureName = Unquote(line[7..].Trim());
                materials[current] = materials[current] with
                {
                    BaseColorTextureName = Path.GetFileName(textureName)
                };
            }
        }
    }

    private static string Unquote(string value) =>
        value.Length >= 2 && value[0] == '"' && value[^1] == '"'
            ? value[1..^1]
            : value;

    private static float Parse(string value) =>
        float.Parse(value, NumberStyles.Float, CultureInfo.InvariantCulture);

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
