using System.Globalization;
using System.Numerics;

namespace SmoImporter.Core;

public static class ObjModelReader
{
    public static ImportedScene Read(string path)
    {
        var positions = new List<Vector3>();
        var normals = new List<Vector3>();
        var uvs = new List<Vector2>();
        var builders = new List<Builder>();
        Builder current = new(Path.GetFileNameWithoutExtension(path));
        builders.Add(current);

        foreach (string raw in File.ReadLines(path))
        {
            string line = raw.Trim();
            if (line.Length == 0 || line[0] == '#') continue;
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
                case "o" or "g" when parts.Length >= 2 && current.Indices.Count > 0:
                    current = new Builder(string.Join('_', parts.Skip(1)));
                    builders.Add(current);
                    break;
                case "o" or "g" when parts.Length >= 2:
                    current.Name = string.Join('_', parts.Skip(1));
                    break;
                case "f" when parts.Length >= 4:
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

        return new ImportedScene(builders.Where(item => item.Indices.Count > 0)
            .Select(item => item.Build()).ToArray());
    }

    private static float Parse(string value) =>
        float.Parse(value, NumberStyles.Float, CultureInfo.InvariantCulture);

    private sealed class Builder(string name)
    {
        private readonly Dictionary<(int P, int T, int N), int> _vertices = [];
        public string Name { get; set; } = name;
        public List<Vector3> Positions { get; } = [];
        public List<Vector3> Normals { get; } = [];
        public List<Vector2> Uvs { get; } = [];
        public List<uint> Indices { get; } = [];
        private bool _allNormals = true;
        private bool _allUvs = true;

        public int GetVertex(
            string token, IReadOnlyList<Vector3> positions,
            IReadOnlyList<Vector2> uvs, IReadOnlyList<Vector3> normals)
        {
            string[] fields = token.Split('/');
            int p = Resolve(fields[0], positions.Count);
            int t = fields.Length > 1 && fields[1].Length > 0 ? Resolve(fields[1], uvs.Count) : -1;
            int n = fields.Length > 2 && fields[2].Length > 0 ? Resolve(fields[2], normals.Count) : -1;
            if (_vertices.TryGetValue((p, t, n), out int existing)) return existing;
            int index = Positions.Count;
            _vertices[(p, t, n)] = index;
            Positions.Add(positions[p]);
            if (t >= 0) Uvs.Add(uvs[t]); else _allUvs = false;
            if (n >= 0) Normals.Add(normals[n]); else _allNormals = false;
            return index;
        }

        public ImportedMesh Build() => new(
            Name,
            Positions.ToArray(),
            _allNormals && Normals.Count == Positions.Count ? Normals.ToArray() : [],
            _allUvs && Uvs.Count == Positions.Count ? Uvs.ToArray() : [],
            Indices.ToArray());

        private static int Resolve(string value, int count)
        {
            int parsed = int.Parse(value, CultureInfo.InvariantCulture);
            int index = parsed > 0 ? parsed - 1 : count + parsed;
            if ((uint)index >= (uint)count) throw new InvalidDataException("OBJ index is outside its source array.");
            return index;
        }
    }
}
