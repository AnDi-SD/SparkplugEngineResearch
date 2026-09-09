using System.Collections.ObjectModel;
using System.Numerics;
using SmoViewer.Core;
using SmoViewer.Scene;

namespace SmoLVLcreator.Core;

/// <summary>
/// Editor-facing projection of one SMO level. Binary knowledge stays in
/// SmoViewer.Core; this type only organizes decoded data for editor workflows.
/// </summary>
public sealed class SmoLevelWorkspace
{
    private SmoLevelWorkspace(
        SmoPreparedScene preparedScene,
        IReadOnlyList<SmoLevelAsset> assets,
        IReadOnlyList<SmoLevelTexture> textures,
        IReadOnlyList<SmoCollisionMesh> collisions,
        IReadOnlyList<string> decodeIssues)
    {
        PreparedScene = preparedScene;
        Assets = assets;
        Textures = textures;
        Collisions = collisions;
        DecodeIssues = decodeIssues;
    }

    public SmoPreparedScene PreparedScene { get; }
    public SmoDocument Document => PreparedScene.Document;
    public string SourcePath => Document.SourcePath ?? string.Empty;
    public string DisplayName => Path.GetFileName(SourcePath);
    public IReadOnlyList<SmoLevelAsset> Assets { get; }
    public IReadOnlyList<SmoLevelTexture> Textures { get; }
    public IReadOnlyList<SmoCollisionMesh> Collisions { get; }
    public IReadOnlyList<string> DecodeIssues { get; }
    public int ObjectCount => Document.Objects.Count;
    public int PlacementCount => Assets.Sum(asset => asset.Placements.Count);
    public int TextureCount => Document.Objects.Count(entry =>
        entry.TypeHash == SmoClassIds.TextureData);
    public int DiagnosticCount => Document.Diagnostics.Count + DecodeIssues.Count +
        PreparedScene.TextureIssues.Count;

    public static SmoLevelWorkspace Load(string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);

        SmoDocument document = SmoDocument.Load(path);
        return Create(document);
    }

    public static SmoLevelWorkspace Create(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        if (document.HasErrors)
            throw new InvalidDataException(
                "A level workspace requires a structurally valid SMO document.");
        SmoPreparedScene preparedScene = SmoSceneBuilder.Build(document);

        var textures = new List<SmoLevelTexture>();
        foreach (SmoObjectEntry entry in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.TextureData))
        {
            if (!SmoTextureDecoder.TryDecode(
                    document,
                    entry,
                    out SmoTexture? texture,
                    out _))
            {
                continue;
            }
            textures.Add(new SmoLevelTexture(
                texture.ObjectIndex,
                texture.Name,
                texture.Width,
                texture.Height,
                texture.FormatCode,
                texture.SourceLayout.ToString(),
                texture.Bgra32Pixels));
        }

        var assets = new List<SmoLevelAsset>();
        foreach (IGrouping<int, SmoSceneMesh> group in preparedScene.Meshes
                     .GroupBy(item => item.Mesh.ObjectIndex))
        {
            SmoSceneMesh source = group.FirstOrDefault(item =>
                item.SharedInstance is null) ?? group.First();
            SmoMesh mesh = source.Mesh;
            SmoObjectEntry entry = document.Objects[mesh.ObjectIndex];
            SmoObjectEntry? modelAncestor = FindAncestor(
                document.Objects,
                entry,
                SmoClassIds.Model);
            string effectiveName = entry.Name.TrimEnd('\0');
            if (string.IsNullOrWhiteSpace(effectiveName))
            {
                effectiveName = source.SharedInstance?.ModelObjectName?.TrimEnd('\0') ??
                    modelAncestor?.Name.TrimEnd('\0') ??
                    string.Empty;
            }
            SmoLevelPlacement[] placements = group.Select(item =>
                new SmoLevelPlacement(
                    item.SceneObjectIndex,
                    item.RenderableObjectIndex,
                    item.RenderableObjectIndex is int renderableIndex
                        ? document.Objects[renderableIndex].Name : effectiveName,
                    item.WorldTransform,
                    item.SharedInstance is not null)
                { OccurrenceKey = item.OccurrenceKey }).ToArray();
            string? issue = preparedScene.TextureIssues.FirstOrDefault(candidate =>
                candidate.Contains(
                    $"[{entry.Index}]", StringComparison.OrdinalIgnoreCase) ||
                candidate.Contains(
                    $"\"{entry.Name}\"", StringComparison.OrdinalIgnoreCase));

            assets.Add(new SmoLevelAsset(
                entry.Index,
                entry.Id,
                effectiveName,
                BuildObjectPath(document.Objects, entry),
                mesh.VertexCount,
                mesh.TriangleCount,
                mesh.VertexFormat,
                mesh.Stride,
                mesh.HasNormals,
                mesh.HasTextureCoordinates,
                mesh.HasTextureCoordinates1,
                mesh.HasDiffuseColors,
                mesh.HasSkinningData,
                source.UsesAlphaBlend,
                source.MaterialRenderState?.Summary,
                issue,
                source.Texture is SmoTexture texture
                    ? new SmoLevelTexture(
                        texture.ObjectIndex,
                        texture.Name,
                        texture.Width,
                        texture.Height,
                        texture.FormatCode,
                        texture.SourceLayout.ToString(),
                        texture.Bgra32Pixels)
                    : null,
                new ReadOnlyCollection<SmoLevelPlacement>(placements)));
        }

        return new SmoLevelWorkspace(
            preparedScene,
            new ReadOnlyCollection<SmoLevelAsset>(assets),
            new ReadOnlyCollection<SmoLevelTexture>(textures),
            SmoCollisionMeshDecoder.DecodeAll(document),
            preparedScene.DecodeErrors);
    }

    private static SmoObjectEntry? FindAncestor(
        IReadOnlyList<SmoObjectEntry> objects,
        SmoObjectEntry entry,
        uint classId)
    {
        int? cursor = entry.ParentIndex;
        var visited = new HashSet<int>();
        while (cursor is int index &&
               (uint)index < (uint)objects.Count &&
               visited.Add(index))
        {
            SmoObjectEntry candidate = objects[index];
            if (candidate.TypeHash == classId)
                return candidate;
            cursor = candidate.ParentIndex;
        }

        return null;
    }

    private static string BuildObjectPath(
        IReadOnlyList<SmoObjectEntry> objects,
        SmoObjectEntry entry)
    {
        var path = new Stack<string>();
        SmoObjectEntry? cursor = entry;
        var visited = new HashSet<int>();
        while (cursor is not null && visited.Add(cursor.Index))
        {
            path.Push(string.IsNullOrWhiteSpace(cursor.Name)
                ? $"[{cursor.Index}] {SmoClassRegistry.GetDisplayName(cursor.TypeHash)}"
                : cursor.Name.TrimEnd('\0'));
            cursor = cursor.ParentIndex is int parent &&
                     (uint)parent < (uint)objects.Count
                ? objects[parent]
                : null;
        }

        return string.Join(" / ", path);
    }
}

public sealed record SmoLevelAsset(
    int ObjectIndex,
    uint ObjectId,
    string Name,
    string FullPath,
    int VertexCount,
    int TriangleCount,
    uint VertexFormat,
    int VertexStride,
    bool HasNormals,
    bool HasUv0,
    bool HasUv1,
    bool HasVertexDiffuse,
    bool HasSkinning,
    bool UsesAlphaBlend,
    string? MaterialSummary,
    string? Issue,
    SmoLevelTexture? Texture,
    IReadOnlyList<SmoLevelPlacement> Placements)
{
    public string DisplayName => string.IsNullOrWhiteSpace(Name)
        ? $"Mesh_{ObjectIndex}"
        : Name.TrimEnd('\0');

    public bool HasIssue => !string.IsNullOrWhiteSpace(Issue);
}

public sealed record SmoLevelPlacement(
    int SceneObjectIndex,
    int? ModelObjectIndex,
    string Name,
    Matrix4x4 WorldTransform,
    bool IsSharedInstance)
{
    public SmoRenderOccurrenceKey? OccurrenceKey { get; init; }
}

public sealed record SmoLevelTexture(
    int ObjectIndex,
    string Name,
    int Width,
    int Height,
    ushort FormatCode,
    string SourceLayout,
    ReadOnlyMemory<byte> Bgra32Pixels);
