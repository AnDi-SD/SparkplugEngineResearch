using System.Numerics;

namespace SmoImporter.Core;

public sealed record MeshSplitPolicy(
    int MaxVerticesPerChunk = 65_535,
    int MaxIndicesPerChunk = int.MaxValue,
    int MaxTrianglesPerChunk = int.MaxValue / 3,
    int MaxBonesPerChunk = 16)
{
    public void Validate()
    {
        if (MaxVerticesPerChunk is < 3 or > 65_535)
            throw new ArgumentOutOfRangeException(nameof(MaxVerticesPerChunk), "Current PC writer uses UInt16 index values.");
        if (MaxIndicesPerChunk < 3 || MaxTrianglesPerChunk < 1 || MaxBonesPerChunk < 1)
            throw new ArgumentOutOfRangeException(nameof(MeshSplitPolicy), "All split limits must be positive.");
    }
}

public sealed record MeshChunk(
    int Index,
    int SourceTriangleStart,
    int SourceTriangleCount,
    ImportedMesh Mesh);

public sealed record MeshSplitPlan(
    MeshSplitPolicy Policy,
    ImportedMesh CombinedSource,
    IReadOnlyList<MeshChunk> Chunks)
{
    public int SourceVertexCount => CombinedSource.Positions.Length;
    public int SourceTriangleCount => CombinedSource.TriangleIndices.Length / 3;
}

public static class ImportedMeshCombiner
{
    public static ImportedMesh Combine(ImportedScene scene, string name = "combined_replacement")
    {
        ArgumentNullException.ThrowIfNull(scene);
        if (scene.Meshes.Count == 0) throw new InvalidOperationException("Imported scene contains no meshes.");
        bool hasNormals = scene.Meshes.All(mesh => mesh.Normals.Length == mesh.Positions.Length);
        bool hasUvs = scene.Meshes.All(mesh => mesh.TextureCoordinates.Length == mesh.Positions.Length);
        var positions = new List<Vector3>();
        var normals = new List<Vector3>();
        var uvs = new List<Vector2>();
        var indices = new List<uint>();
        foreach (ImportedMesh mesh in scene.Meshes)
        {
            uint vertexBase = checked((uint)positions.Count);
            positions.AddRange(mesh.Positions);
            if (hasNormals) normals.AddRange(mesh.Normals);
            if (hasUvs) uvs.AddRange(mesh.TextureCoordinates);
            foreach (uint index in mesh.TriangleIndices)
            {
                if (index >= mesh.Positions.Length)
                    throw new InvalidDataException($"Mesh {mesh.Name} contains an out-of-range index {index}.");
                indices.Add(checked(vertexBase + index));
            }
        }
        return new ImportedMesh(name, positions.ToArray(),
            hasNormals ? normals.ToArray() : [], hasUvs ? uvs.ToArray() : [], indices.ToArray());
    }
}

public static class MeshSplitter
{
    public static MeshSplitPlan Split(ImportedScene scene, MeshSplitPolicy? policy = null)
    {
        policy ??= new MeshSplitPolicy();
        policy.Validate();
        ImportedMesh source = ImportedMeshCombiner.Combine(scene);
        if (source.TriangleIndices.Length % 3 != 0)
            throw new InvalidDataException("Triangle index count is not divisible by three.");
        var chunks = new List<MeshChunk>();
        var builder = new ChunkBuilder(source);
        int chunkStart = 0;
        int totalTriangles = source.TriangleIndices.Length / 3;
        for (int triangle = 0; triangle < totalTriangles; triangle++)
        {
            ReadOnlySpan<uint> corners = source.TriangleIndices.AsSpan(triangle * 3, 3);
            int newVertices = 0;
            foreach (uint index in corners)
            {
                if (!builder.Contains(index)) newVertices++;
            }
            bool exceeds = builder.TriangleCount > 0 &&
                (builder.VertexCount + newVertices > policy.MaxVerticesPerChunk ||
                 builder.IndexCount + 3 > policy.MaxIndicesPerChunk ||
                 builder.TriangleCount + 1 > policy.MaxTrianglesPerChunk);
            if (exceeds)
            {
                chunks.Add(builder.Build(chunks.Count, chunkStart));
                builder = new ChunkBuilder(source);
                chunkStart = triangle;
            }
            if (newVertices > policy.MaxVerticesPerChunk)
                throw new InvalidOperationException("A single triangle exceeds the configured vertex limit.");
            builder.AddTriangle(corners);
        }
        if (builder.TriangleCount > 0) chunks.Add(builder.Build(chunks.Count, chunkStart));
        return new MeshSplitPlan(policy, source, chunks);
    }

    private sealed class ChunkBuilder(ImportedMesh source)
    {
        private readonly Dictionary<uint, uint> _remap = [];
        private readonly List<Vector3> _positions = [];
        private readonly List<Vector3> _normals = [];
        private readonly List<Vector2> _uvs = [];
        private readonly List<uint> _indices = [];
        private readonly bool _hasNormals = source.Normals.Length == source.Positions.Length;
        private readonly bool _hasUvs = source.TextureCoordinates.Length == source.Positions.Length;
        public int VertexCount => _positions.Count;
        public int IndexCount => _indices.Count;
        public int TriangleCount => _indices.Count / 3;
        public bool Contains(uint index) => _remap.ContainsKey(index);

        public void AddTriangle(ReadOnlySpan<uint> corners)
        {
            foreach (uint sourceIndex in corners)
            {
                if (sourceIndex >= source.Positions.Length)
                    throw new InvalidDataException($"Source index {sourceIndex} is outside the combined vertex array.");
                if (!_remap.TryGetValue(sourceIndex, out uint localIndex))
                {
                    localIndex = checked((uint)_positions.Count);
                    _remap[sourceIndex] = localIndex;
                    _positions.Add(source.Positions[(int)sourceIndex]);
                    if (_hasNormals) _normals.Add(source.Normals[(int)sourceIndex]);
                    if (_hasUvs) _uvs.Add(source.TextureCoordinates[(int)sourceIndex]);
                }
                _indices.Add(localIndex);
            }
        }

        public MeshChunk Build(int index, int triangleStart) => new(
            index, triangleStart, TriangleCount,
            new ImportedMesh($"replacement_chunk_{index:D3}", _positions.ToArray(),
                _hasNormals ? _normals.ToArray() : [], _hasUvs ? _uvs.ToArray() : [], _indices.ToArray()));
    }
}
