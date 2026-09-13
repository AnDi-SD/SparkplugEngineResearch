using System.Collections.ObjectModel;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Authored visual branches commonly stored below a GUI control node.</summary>
public enum SmoGuiVisualState
{
    Unclassified = 0,
    Normal,
    Highlighted,
    Pushed,
    Disabled,
    Shadow
}

/// <summary>The structural 2D representation carried by an SMO resource.</summary>
public enum SmoGuiContentKind
{
    None = 0,
    PlanarMeshScene,
    RuntimeNodeLayout
}

/// <summary>Axis-aligned bounds after the complete SMO node transform chain.</summary>
public sealed record SmoGuiBounds(Vector3 Minimum, Vector3 Maximum)
{
    public Vector3 Size => Maximum - Minimum;
    public Vector3 Center => (Minimum + Maximum) * 0.5f;
}

/// <summary>GUI semantics resolved for one decoded mesh.</summary>
public sealed record SmoGuiMeshInfo(
    int ObjectIndex,
    int? RootGroupObjectIndex,
    string RootGroupName,
    string ElementName,
    SmoGuiVisualState VisualState,
    bool IsCollision,
    uint VertexFormat,
    SmoGuiBounds WorldBounds);

/// <summary>A direct child of the authored GUI scene root.</summary>
public sealed record SmoGuiRootGroupInfo(
    int ObjectIndex,
    string Name,
    int ObjectCount,
    int NodeCount,
    int MeshCount);

/// <summary>GUI context inherited by any object below a root screen branch.</summary>
public sealed record SmoGuiObjectContext(
    int ObjectIndex,
    int RootGroupObjectIndex,
    string RootGroupName,
    SmoGuiVisualState VisualState,
    bool IsCollision);

/// <summary>
/// A leaf slot in a node-only GUI layout. The position is inherited from its
/// authored node ancestors; the actual text or widget is supplied at runtime.
/// </summary>
public sealed record SmoGuiLayoutAnchorInfo(
    int ObjectIndex,
    int RootGroupObjectIndex,
    string RootGroupName,
    string Name,
    SmoGuiVisualState VisualState,
    Vector3 WorldPosition);

/// <summary>
/// Evidence-backed description of Sparkplug 2D content. Detection is structural:
/// regular GUI scenes combine planar transformed meshes, absence of skinning and
/// authored state/collision branches; node-only runtime layouts combine text-slot
/// leaves, authored states and transforms. Neither path matches a file name.
/// </summary>
public sealed class SmoGuiSceneInfo
{
    internal SmoGuiSceneInfo(
        SmoGuiContentKind contentKind,
        int totalMeshCount,
        int decodedMeshCount,
        int planarMeshCount,
        int collisionMeshCount,
        int stateMeshCount,
        int namedControlNodeCount,
        int textClassObjectCount,
        SmoGuiBounds? worldBounds,
        IReadOnlyList<SmoGuiRootGroupInfo> rootGroups,
        IReadOnlyList<SmoGuiMeshInfo> meshes,
        IReadOnlyDictionary<int, SmoGuiObjectContext> objectContexts,
        IReadOnlyList<SmoGuiLayoutAnchorInfo> layoutAnchors)
    {
        ContentKind = contentKind;
        IsGuiContent = contentKind != SmoGuiContentKind.None;
        TotalMeshCount = totalMeshCount;
        DecodedMeshCount = decodedMeshCount;
        PlanarMeshCount = planarMeshCount;
        CollisionMeshCount = collisionMeshCount;
        StateMeshCount = stateMeshCount;
        NamedControlNodeCount = namedControlNodeCount;
        TextClassObjectCount = textClassObjectCount;
        WorldBounds = worldBounds;
        RootGroups = rootGroups;
        Meshes = meshes;
        ObjectContextsByObjectIndex = objectContexts;
        LayoutAnchors = layoutAnchors;
        MeshesByObjectIndex = new ReadOnlyDictionary<int, SmoGuiMeshInfo>(
            meshes.ToDictionary(mesh => mesh.ObjectIndex));
    }

    public bool IsGuiContent { get; }
    public SmoGuiContentKind ContentKind { get; }
    public int TotalMeshCount { get; }
    public int DecodedMeshCount { get; }
    public int PlanarMeshCount { get; }
    public int CollisionMeshCount { get; }
    public int StateMeshCount { get; }
    public int NamedControlNodeCount { get; }
    public int TextClassObjectCount { get; }
    public SmoGuiBounds? WorldBounds { get; }
    public IReadOnlyList<SmoGuiRootGroupInfo> RootGroups { get; }
    public IReadOnlyList<SmoGuiMeshInfo> Meshes { get; }
    public IReadOnlyDictionary<int, SmoGuiMeshInfo> MeshesByObjectIndex { get; }
    public IReadOnlyDictionary<int, SmoGuiObjectContext> ObjectContextsByObjectIndex
    {
        get;
    }
    public IReadOnlyList<SmoGuiLayoutAnchorInfo> LayoutAnchors { get; }
}

public static class SmoGuiSceneAnalyzer
{
    private static readonly string[] ControlNameTokens =
    [
        "button", "scroll", "meter", "value", "label", "resolution",
        "quality", "language", "display", "cancel", "menu", "GUICollision"
    ];

    public static SmoGuiSceneInfo Analyze(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);

        SmoObjectEntry[] meshEntries = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .ToArray();
        var meshInfos = new List<SmoGuiMeshInfo>(meshEntries.Length);
        int planarMeshCount = 0;
        int collisionMeshCount = 0;
        int stateMeshCount = 0;
        Vector3 boundsMinimum = new(float.PositiveInfinity);
        Vector3 boundsMaximum = new(float.NegativeInfinity);

        foreach (SmoObjectEntry entry in meshEntries)
        {
            if (!SmoMeshDecoder.TryDecode(
                    document, entry, out SmoMesh? mesh, out _) || mesh is null)
            {
                continue;
            }

            Matrix4x4 world =
                SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, entry);
            if (!TryCalculateBounds(mesh.Positions, world, out SmoGuiBounds? meshBounds) ||
                meshBounds is null)
            {
                continue;
            }

            if (IsPlanar(meshBounds))
                planarMeshCount++;
            boundsMinimum = Vector3.Min(boundsMinimum, meshBounds.Minimum);
            boundsMaximum = Vector3.Max(boundsMaximum, meshBounds.Maximum);

            SmoObjectEntry[] ancestors = EnumerateAncestors(document, entry).ToArray();
            bool isCollision = ancestors.Prepend(entry).Any(candidate =>
                candidate.Name.StartsWith(
                    "GUICollision", StringComparison.OrdinalIgnoreCase));
            if (isCollision)
                collisionMeshCount++;

            SmoGuiVisualState visualState = ClassifyVisualState(
                ancestors.Prepend(entry).Select(candidate => candidate.Name));
            if (visualState is not
                (SmoGuiVisualState.Unclassified or SmoGuiVisualState.Shadow))
            {
                stateMeshCount++;
            }

            (int? rootGroupObjectIndex, string rootGroupName) =
                ResolveRootGroup(document, entry, ancestors);
            meshInfos.Add(new SmoGuiMeshInfo(
                entry.Index,
                rootGroupObjectIndex,
                rootGroupName,
                ResolveElementName(entry, ancestors),
                visualState,
                isCollision,
                mesh.VertexFormat,
                meshBounds));
        }

        SmoGuiBounds? worldBounds = meshInfos.Count > 0
            ? new SmoGuiBounds(boundsMinimum, boundsMaximum)
            : null;
        int namedControlNodeCount = document.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.Node &&
            ControlNameTokens.Any(token => entry.Name.Contains(
                token, StringComparison.OrdinalIgnoreCase)));
        int textClassObjectCount = document.Objects.Count(entry =>
            entry.TypeHash is SmoClassIds.TextNode or
                SmoClassIds.TextRenderable or SmoClassIds.Font);
        int skinCount = document.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.Skin);
        bool mostlyPlanar = meshInfos.Count >= 3 &&
            planarMeshCount >= Math.Ceiling(meshInfos.Count * 0.8);
        bool hasGuiSemantics = collisionMeshCount > 0 || stateMeshCount >= 3;
        bool isPlanarMeshScene = meshInfos.Count == meshEntries.Length &&
            mostlyPlanar && skinCount == 0 && hasGuiSemantics;

        HashSet<int> parentObjectIndices = document.Objects
            .Where(entry => entry.ParentIndex.HasValue)
            .Select(entry => entry.ParentIndex!.Value)
            .ToHashSet();
        SmoObjectEntry[] runtimeTextSlots = document.Objects
            .Where(entry =>
                entry.TypeHash == SmoClassIds.Node &&
                !parentObjectIndices.Contains(entry.Index) &&
                entry.Name.StartsWith("text_", StringComparison.OrdinalIgnoreCase))
            .ToArray();
        bool hasAuthoredStateNode = document.Objects.Any(entry =>
            ClassifyVisualStateName(entry.Name) is
                SmoGuiVisualState.Normal or SmoGuiVisualState.Shadow);
        bool hasAuthoredTransform = document.Objects.Any(entry =>
            SmoNodeTransformDecoder.TryDecode(document, entry, out _));
        bool isRuntimeNodeLayout = meshEntries.Length == 0 &&
            document.Objects.Count >= 4 &&
            document.Objects.All(entry => entry.TypeHash == SmoClassIds.Node) &&
            runtimeTextSlots.Length > 0 &&
            hasAuthoredStateNode &&
            hasAuthoredTransform;
        SmoGuiContentKind contentKind = isPlanarMeshScene
            ? SmoGuiContentKind.PlanarMeshScene
            : isRuntimeNodeLayout
                ? SmoGuiContentKind.RuntimeNodeLayout
                : SmoGuiContentKind.None;

        SmoGuiRootGroupInfo[] rootGroups = ResolveRootGroups(document, meshInfos);
        IReadOnlyDictionary<int, SmoGuiObjectContext> objectContexts =
            contentKind != SmoGuiContentKind.None
                ? ResolveObjectContexts(document, rootGroups)
                : new ReadOnlyDictionary<int, SmoGuiObjectContext>(
                    new Dictionary<int, SmoGuiObjectContext>());
        IReadOnlyList<SmoGuiLayoutAnchorInfo> layoutAnchors =
            contentKind == SmoGuiContentKind.RuntimeNodeLayout
                ? ResolveLayoutAnchors(document, runtimeTextSlots, objectContexts)
                : Array.Empty<SmoGuiLayoutAnchorInfo>();
        if (worldBounds is null && layoutAnchors.Count > 0)
        {
            Vector3 minimum = layoutAnchors[0].WorldPosition;
            Vector3 maximum = minimum;
            foreach (SmoGuiLayoutAnchorInfo anchor in layoutAnchors.Skip(1))
            {
                minimum = Vector3.Min(minimum, anchor.WorldPosition);
                maximum = Vector3.Max(maximum, anchor.WorldPosition);
            }
            worldBounds = new SmoGuiBounds(minimum, maximum);
        }
        return new SmoGuiSceneInfo(
            contentKind,
            meshEntries.Length,
            meshInfos.Count,
            planarMeshCount,
            collisionMeshCount,
            stateMeshCount,
            namedControlNodeCount,
            textClassObjectCount,
            worldBounds,
            new ReadOnlyCollection<SmoGuiRootGroupInfo>(rootGroups),
            new ReadOnlyCollection<SmoGuiMeshInfo>(meshInfos),
            objectContexts,
            layoutAnchors);
    }

    /// <summary>Classifies an authored branch name without depending on numeric suffixes.</summary>
    public static SmoGuiVisualState ClassifyVisualStateName(string? name)
    {
        if (string.IsNullOrWhiteSpace(name))
            return SmoGuiVisualState.Unclassified;
        if (name.StartsWith("DISABLED", StringComparison.OrdinalIgnoreCase))
            return SmoGuiVisualState.Disabled;
        if (name.StartsWith("HIGHLIGHTED", StringComparison.OrdinalIgnoreCase))
            return SmoGuiVisualState.Highlighted;
        if (name.StartsWith("PUSHED", StringComparison.OrdinalIgnoreCase))
            return SmoGuiVisualState.Pushed;
        if (name.StartsWith("NORMAL", StringComparison.OrdinalIgnoreCase))
            return SmoGuiVisualState.Normal;
        if (name.StartsWith("shadow", StringComparison.OrdinalIgnoreCase))
            return SmoGuiVisualState.Shadow;
        return SmoGuiVisualState.Unclassified;
    }

    private static SmoGuiVisualState ClassifyVisualState(IEnumerable<string> names)
    {
        SmoGuiVisualState result = SmoGuiVisualState.Unclassified;
        foreach (string name in names)
        {
            SmoGuiVisualState candidate = ClassifyVisualStateName(name);
            if (candidate is SmoGuiVisualState.Disabled or
                SmoGuiVisualState.Highlighted or SmoGuiVisualState.Pushed or
                SmoGuiVisualState.Normal)
            {
                return candidate;
            }
            if (candidate == SmoGuiVisualState.Shadow)
                result = candidate;
        }
        return result;
    }

    private static bool TryCalculateBounds(
        IEnumerable<Vector3> positions,
        Matrix4x4 transform,
        out SmoGuiBounds? bounds)
    {
        Vector3 minimum = new(float.PositiveInfinity);
        Vector3 maximum = new(float.NegativeInfinity);
        int count = 0;
        foreach (Vector3 source in positions)
        {
            Vector3 point = Vector3.Transform(source, transform);
            if (!IsFinite(point))
                continue;
            minimum = Vector3.Min(minimum, point);
            maximum = Vector3.Max(maximum, point);
            count++;
        }
        bounds = count > 0 ? new SmoGuiBounds(minimum, maximum) : null;
        return bounds is not null;
    }

    private static bool IsPlanar(SmoGuiBounds bounds)
    {
        Vector3 size = Vector3.Abs(bounds.Size);
        float major = MathF.Max(size.X, MathF.Max(size.Y, size.Z));
        float minor = MathF.Min(size.X, MathF.Min(size.Y, size.Z));
        return major > 0 && minor <= major * 0.001f + 0.00001f;
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);

    private static IEnumerable<SmoObjectEntry> EnumerateAncestors(
        SmoDocument document,
        SmoObjectEntry entry)
    {
        SmoObjectEntry? cursor = entry;
        var visited = new HashSet<int> { entry.Index };
        while (cursor.ParentIndex is int parentIndex &&
               (uint)parentIndex < (uint)document.Objects.Count &&
               visited.Add(parentIndex))
        {
            cursor = document.Objects[parentIndex];
            yield return cursor;
        }
    }

    private static (int? ObjectIndex, string Name) ResolveRootGroup(
        SmoDocument document,
        SmoObjectEntry entry,
        IReadOnlyList<SmoObjectEntry> ancestors)
    {
        SmoObjectEntry childOfRoot = entry;
        foreach (SmoObjectEntry ancestor in ancestors)
        {
            if (ancestor.ParentIndex is null)
                break;
            childOfRoot = ancestor;
        }

        return childOfRoot.Index == entry.Index && entry.ParentIndex is null
            ? (null, "<root>")
            : (childOfRoot.Index,
                string.IsNullOrWhiteSpace(childOfRoot.Name)
                    ? $"object {childOfRoot.Index}"
                    : childOfRoot.Name);
    }

    private static string ResolveElementName(
        SmoObjectEntry entry,
        IReadOnlyList<SmoObjectEntry> ancestors)
    {
        foreach (SmoObjectEntry candidate in ancestors.Prepend(entry))
        {
            if (string.IsNullOrWhiteSpace(candidate.Name) ||
                candidate.Name.EndsWith("-000", StringComparison.OrdinalIgnoreCase) ||
                ClassifyVisualStateName(candidate.Name) != SmoGuiVisualState.Unclassified)
            {
                continue;
            }
            return candidate.Name;
        }
        return $"mesh {entry.Index}";
    }

    private static SmoGuiRootGroupInfo[] ResolveRootGroups(
        SmoDocument document,
        IReadOnlyList<SmoGuiMeshInfo> meshes) => document.Objects
        .Where(entry => entry.ParentIndex is int parentIndex &&
                        document.Objects[parentIndex].ParentIndex is null)
        .Select(entry =>
        {
            SmoObjectEntry[] descendants = document.Objects.Where(candidate =>
                candidate.LogicalOffset >= entry.LogicalOffset &&
                candidate.LogicalEnd <= entry.LogicalEnd).ToArray();
            return new SmoGuiRootGroupInfo(
                entry.Index,
                string.IsNullOrWhiteSpace(entry.Name)
                    ? $"object {entry.Index}"
                    : entry.Name,
                descendants.Length,
                descendants.Count(candidate => candidate.TypeHash == SmoClassIds.Node),
                meshes.Count(mesh => mesh.RootGroupObjectIndex == entry.Index));
        })
        .OrderBy(group => group.ObjectIndex)
        .ToArray();

    private static IReadOnlyDictionary<int, SmoGuiObjectContext> ResolveObjectContexts(
        SmoDocument document,
        IReadOnlyList<SmoGuiRootGroupInfo> rootGroups)
    {
        Dictionary<int, SmoGuiRootGroupInfo> groupsByIndex = rootGroups
            .ToDictionary(group => group.ObjectIndex);
        var contexts = new Dictionary<int, SmoGuiObjectContext>();
        foreach (SmoObjectEntry entry in document.Objects)
        {
            SmoObjectEntry[] path = EnumerateAncestors(document, entry)
                .Prepend(entry)
                .ToArray();
            SmoGuiRootGroupInfo? group = path
                .Select(item => groupsByIndex.GetValueOrDefault(item.Index))
                .FirstOrDefault(item => item is not null);
            if (group is null)
                continue;

            contexts.Add(entry.Index, new SmoGuiObjectContext(
                entry.Index,
                group.ObjectIndex,
                group.Name,
                ClassifyVisualState(path.Select(item => item.Name)),
                path.Any(item => item.Name.StartsWith(
                    "GUICollision", StringComparison.OrdinalIgnoreCase))));
        }
        return new ReadOnlyDictionary<int, SmoGuiObjectContext>(contexts);
    }

    private static IReadOnlyList<SmoGuiLayoutAnchorInfo> ResolveLayoutAnchors(
        SmoDocument document,
        IReadOnlyList<SmoObjectEntry> entries,
        IReadOnlyDictionary<int, SmoGuiObjectContext> objectContexts)
    {
        var anchors = new List<SmoGuiLayoutAnchorInfo>(entries.Count);
        foreach (SmoObjectEntry entry in entries)
        {
            if (!objectContexts.TryGetValue(
                    entry.Index, out SmoGuiObjectContext? context))
            {
                continue;
            }
            Matrix4x4 world = ResolveObjectWorldMatrix(document, entry);
            Vector3 position = new(world.M41, world.M42, world.M43);
            if (!IsFinite(position))
                continue;
            anchors.Add(new SmoGuiLayoutAnchorInfo(
                entry.Index,
                context.RootGroupObjectIndex,
                context.RootGroupName,
                entry.Name,
                context.VisualState,
                position));
        }
        return new ReadOnlyCollection<SmoGuiLayoutAnchorInfo>(anchors);
    }

    private static Matrix4x4 ResolveObjectWorldMatrix(
        SmoDocument document,
        SmoObjectEntry entry)
    {
        Matrix4x4 world = Matrix4x4.Identity;
        SmoObjectEntry? cursor = entry;
        var visited = new HashSet<int>();
        while (cursor is not null && visited.Add(cursor.Index))
        {
            if (SmoNodeTransformDecoder.TryDecode(
                    document, cursor, out SmoNodeTransform? transform) &&
                transform is not null)
            {
                world *= transform.LocalMatrix;
            }
            cursor = cursor.ParentIndex is int parentIndex &&
                     (uint)parentIndex < (uint)document.Objects.Count
                ? document.Objects[parentIndex]
                : null;
        }
        return world;
    }
}
