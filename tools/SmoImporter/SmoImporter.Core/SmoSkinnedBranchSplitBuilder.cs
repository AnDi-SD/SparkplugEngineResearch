using System.Buffers.Binary;
using System.Numerics;
using System.Text;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoViewer.Core;

namespace SmoImporter.Core;

internal sealed record SmoSkinnedBranchSourceMesh(
    int Key,
    string Name,
    Vector3[] Positions,
    Vector3[] Normals,
    Vector2[] TextureCoordinates,
    uint[] DiffuseColorsArgb,
    uint[] TriangleIndices,
    ImportedSkinning Skinning);

internal enum SmoSkinnedRenderableMaterialFamily
{
    OpaqueBody = 0,
    OpaqueOverlay = 1,
    AlphaBlend = 2
}

/// <summary>
/// Records the material family selected for each source renderable. UV/texel
/// coverage is measured per triangle. Automatic classification promotes a whole
/// geometrically connected component when any of its triangles can sample alpha;
/// explicit material overrides remain a mesh-level final decision.
/// </summary>
internal sealed class SmoSkinnedRenderableOpacityPlan
{
    private readonly IReadOnlyDictionary<int, SmoSkinnedRenderableMaterialFamily[]>
        _familyByMesh;

    public SmoSkinnedRenderableOpacityPlan(
        IReadOnlyDictionary<int, SmoSkinnedRenderableMaterialFamily[]> familyByMesh,
        int opaqueBodyTriangleCount,
        int opaqueOverlayTriangleCount,
        int alphaTriangleCount)
    {
        _familyByMesh = familyByMesh;
        OpaqueBodyTriangleCount = opaqueBodyTriangleCount;
        OpaqueOverlayTriangleCount = opaqueOverlayTriangleCount;
        AlphaTriangleCount = alphaTriangleCount;
    }

    public int OpaqueBodyTriangleCount { get; }
    public int OpaqueOverlayTriangleCount { get; }
    public int AlphaTriangleCount { get; }
    public int SeparateBranchTriangleCount =>
        OpaqueOverlayTriangleCount + AlphaTriangleCount;

    public SmoSkinnedRenderableMaterialFamily GetFamily(
        int meshKey,
        int triangleOrdinal)
    {
        if (!_familyByMesh.TryGetValue(
                meshKey, out SmoSkinnedRenderableMaterialFamily[]? values) ||
            (uint)triangleOrdinal >= (uint)values.Length)
        {
            throw new InvalidOperationException(
                $"Renderable material was not classified for mesh {meshKey}, " +
                $"triangle {triangleOrdinal}.");
        }
        return values[triangleOrdinal];
    }

    public bool IsAlpha(int meshKey, int triangleOrdinal) =>
        GetFamily(meshKey, triangleOrdinal) ==
        SmoSkinnedRenderableMaterialFamily.AlphaBlend;

    public bool IsOpaqueOverlay(int meshKey, int triangleOrdinal) =>
        GetFamily(meshKey, triangleOrdinal) ==
        SmoSkinnedRenderableMaterialFamily.OpaqueOverlay;

    public bool IsSeparateBranch(int meshKey, int triangleOrdinal) =>
        GetFamily(meshKey, triangleOrdinal) !=
        SmoSkinnedRenderableMaterialFamily.OpaqueBody;
}

internal sealed record SmoSkinnedBranchSplitAnalysis(
    int BranchCount,
    int VertexCount,
    int TriangleCount);

internal sealed record SmoSkinnedBranchSplitResult(
    byte[] Data,
    int BranchCount,
    int VertexCount,
    int TriangleCount,
    IReadOnlySet<uint> AddedObjectIds,
    IReadOnlySet<uint> AddedSkinIds,
    IReadOnlySet<uint> AddedMeshIds,
    IReadOnlyDictionary<uint, SmoSkinnedRenderableMaterialFamily> AddedObjectFamilies);

internal sealed record SmoSurfaceOverlayNormalConformResult(
    SmoSkinnedBranchSourceMesh[] Meshes,
    int ConformedComponentCount,
    int ConformedVertexCount);

/// <summary>
/// Builds independent post-body native Bloom material runs for opaque overlays
/// and true-alpha surfaces. Each source renderable starts its own material/spSkin
/// draw unit and may add palette/vertex continuations. Continuation skins inherit
/// their run-start material exactly like shipped split spSkin sequences.
/// </summary>
internal static class SmoSkinnedBranchSplitBuilder
{
    private const int ObjectSignatureSize = 8;
    private const int ObjectReferenceSize = 8;
    private const int PaletteCapacity = 16;
    private const float WeightEpsilon = 0.000001f;
    private const uint SharedFogClassId = SmoClassIds.Fog;
    private const uint OpaqueOverlayFinalBlendOperation = 0;
    private const uint SkinnedTransparentSurfaceFinalBlendOperation = 2;
    private const uint OpaqueOverlayAlphaSortEnable = 0;
    private const uint SkinnedTransparentSurfaceAlphaSortEnable = 1;
    private const uint GeneratedPriority = 1;
    private const uint OpaqueOverlayVertexDiffuse = 0xFFFFFFFF;
    private const uint SkinnedTransparentSurfaceVertexDiffuse = 0xFF000000;

    private static readonly uint[] SkinnedTransparentSurfaceMaterialRenderStates =
        [0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6];

    private static readonly uint[] OpaqueOverlayMaterialRenderStates =
        [0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6];

    private static readonly uint[] SkinnedTransparentSurfaceLightingTextureStates =
        [0, 3, 3, 0, 0, 0xFF000000, 2, 0, 0];

    private static readonly uint[] OpaqueOverlayLightingTextureStates =
        [0, 3, 3, 0, 0, 0xFF000000, 2, 0, 0];

    /// <summary>
    /// Makes a close alpha/opaque-overlay sheet shade continuously with the
    /// larger opaque surface underneath it. This is deliberately geometric:
    /// names and material IDs are not trusted, every vertex of a connected
    /// overlay component must be very close to an opaque triangle in another
    /// source mesh, and the authored normals must already face substantially
    /// the same way. Wings, jewelry, clothing volumes and other detached parts
    /// therefore retain their authored normals.
    /// </summary>
    public static SmoSurfaceOverlayNormalConformResult ConformCloseSurfaceNormals(
        IReadOnlyList<SmoSkinnedBranchSourceMesh> meshes,
        SmoSkinnedRenderableOpacityPlan opacity)
    {
        ArgumentNullException.ThrowIfNull(meshes);
        ArgumentNullException.ThrowIfNull(opacity);
        if (meshes.Count == 0)
            return new SmoSurfaceOverlayNormalConformResult([], 0, 0);

        Vector3 globalMinimum = new(float.PositiveInfinity);
        Vector3 globalMaximum = new(float.NegativeInfinity);
        var referenceTriangles = new List<NormalReferenceTriangle>();
        foreach (SmoSkinnedBranchSourceMesh mesh in meshes)
        {
            if (mesh.Normals.Length != mesh.Positions.Length)
                continue;
            foreach (Vector3 position in mesh.Positions)
            {
                globalMinimum = Vector3.Min(globalMinimum, position);
                globalMaximum = Vector3.Max(globalMaximum, position);
            }
            for (int triangle = 0; triangle < mesh.TriangleIndices.Length / 3; triangle++)
            {
                if (opacity.GetFamily(mesh.Key, triangle) !=
                    SmoSkinnedRenderableMaterialFamily.OpaqueBody)
                {
                    continue;
                }
                int index = triangle * 3;
                int first = CheckedVertex(mesh, mesh.TriangleIndices[index]);
                int second = CheckedVertex(mesh, mesh.TriangleIndices[index + 1]);
                int third = CheckedVertex(mesh, mesh.TriangleIndices[index + 2]);
                referenceTriangles.Add(new NormalReferenceTriangle(
                    mesh.Key,
                    mesh.Positions[first],
                    mesh.Positions[second],
                    mesh.Positions[third],
                    mesh.Normals[first],
                    mesh.Normals[second],
                    mesh.Normals[third]));
            }
        }
        float globalDiagonal = Vector3.Distance(globalMinimum, globalMaximum);
        if (referenceTriangles.Count == 0 || !float.IsFinite(globalDiagonal) ||
            globalDiagonal <= 0.000001f)
        {
            return new SmoSurfaceOverlayNormalConformResult(
                meshes.ToArray(), 0, 0);
        }

        var search = new NormalReferenceSearch(referenceTriangles);
        var output = meshes.ToArray();
        int conformedComponents = 0;
        int conformedVertices = 0;
        for (int meshIndex = 0; meshIndex < meshes.Count; meshIndex++)
        {
            SmoSkinnedBranchSourceMesh mesh = meshes[meshIndex];
            if (mesh.Positions.Length == 0 ||
                mesh.Normals.Length != mesh.Positions.Length)
            {
                continue;
            }
            var connectivity = new UnionFind(mesh.Positions.Length);
            var candidateVertices = new HashSet<int>();
            for (int triangle = 0; triangle < mesh.TriangleIndices.Length / 3; triangle++)
            {
                if (!opacity.IsSeparateBranch(mesh.Key, triangle))
                    continue;
                int index = triangle * 3;
                int first = CheckedVertex(mesh, mesh.TriangleIndices[index]);
                int second = CheckedVertex(mesh, mesh.TriangleIndices[index + 1]);
                int third = CheckedVertex(mesh, mesh.TriangleIndices[index + 2]);
                candidateVertices.Add(first);
                candidateVertices.Add(second);
                candidateVertices.Add(third);
                connectivity.Union(first, second);
                connectivity.Union(first, third);
            }
            if (candidateVertices.Count == 0)
                continue;

            // Attribute seams often duplicate all three triangle vertices.
            // Rejoin raw index islands only when they share an entire geometric
            // edge (two exact positions); a single coincident point can be an
            // intentional contact between independent overlay pieces.
            var verticesByPosition = new Dictionary<Vector3, List<int>>();
            foreach (int vertex in candidateVertices)
            {
                Vector3 position = mesh.Positions[vertex];
                if (!verticesByPosition.TryGetValue(position, out List<int>? equal))
                {
                    equal = [];
                    verticesByPosition.Add(position, equal);
                }
                equal.Add(vertex);
            }
            var sharedPositions = new Dictionary<(int First, int Second), int>();
            foreach (List<int> equal in verticesByPosition.Values)
            {
                int[] roots = equal.Select(connectivity.Find)
                    .Distinct().Order().ToArray();
                if (roots.Length > 32)
                {
                    throw new InvalidDataException(
                        $"Mesh {mesh.Name} has more than 32 overlapping overlay " +
                        "components at one position; normal conformity is ambiguous.");
                }
                for (int first = 0; first < roots.Length; first++)
                {
                    for (int second = first + 1; second < roots.Length; second++)
                    {
                        var pair = (roots[first], roots[second]);
                        sharedPositions[pair] =
                            sharedPositions.GetValueOrDefault(pair) + 1;
                    }
                }
            }
            foreach (KeyValuePair<(int First, int Second), int> pair in sharedPositions)
            {
                if (pair.Value >= 2)
                    connectivity.Union(pair.Key.First, pair.Key.Second);
            }

            Vector3[]? replacement = null;
            foreach (int[] component in candidateVertices
                         .GroupBy(connectivity.Find)
                         .Select(group => group.Order().ToArray()))
            {
                Vector3 minimum = new(float.PositiveInfinity);
                Vector3 maximum = new(float.NegativeInfinity);
                foreach (int vertex in component)
                {
                    minimum = Vector3.Min(minimum, mesh.Positions[vertex]);
                    maximum = Vector3.Max(maximum, mesh.Positions[vertex]);
                }
                float componentDiagonal = Vector3.Distance(minimum, maximum);
                float maximumDistance = MathF.Min(
                    globalDiagonal * 0.0015f,
                    MathF.Max(
                        componentDiagonal * 0.04f,
                        globalDiagonal * 0.00075f));
                if (!float.IsFinite(maximumDistance) ||
                    maximumDistance <= globalDiagonal * 0.000001f)
                {
                    continue;
                }

                var inherited = new Vector3[component.Length];
                float dotTotal = 0;
                bool accepted = true;
                for (int ordinal = 0; ordinal < component.Length; ordinal++)
                {
                    int vertex = component[ordinal];
                    Vector3 sourceNormal = NormalizeOrZero(mesh.Normals[vertex]);
                    if (sourceNormal == Vector3.Zero)
                    {
                        accepted = false;
                        break;
                    }
                    if (!search.TryFind(
                            mesh.Positions[vertex],
                            sourceNormal,
                            mesh.Key,
                            maximumDistance,
                            out Vector3 inheritedNormal))
                    {
                        accepted = false;
                        break;
                    }
                    float dot = Vector3.Dot(sourceNormal, inheritedNormal);
                    if (dot < 0.45f)
                    {
                        accepted = false;
                        break;
                    }
                    dotTotal += dot;
                    inherited[ordinal] = inheritedNormal;
                }
                if (!accepted || dotTotal / component.Length < 0.90f)
                    continue;

                replacement ??= mesh.Normals.ToArray();
                for (int ordinal = 0; ordinal < component.Length; ordinal++)
                    replacement[component[ordinal]] = inherited[ordinal];
                conformedComponents++;
                conformedVertices += component.Length;
            }
            if (replacement is not null)
                output[meshIndex] = mesh with { Normals = replacement };
        }
        return new SmoSurfaceOverlayNormalConformResult(
            output, conformedComponents, conformedVertices);
    }

    /// <summary>
    /// Conservatively classifies source meshes from their actually reachable
    /// base-level alpha texels, including bilinear support. Automatic state is
    /// uniform within each geometrically connected component so one continuous
    /// surface cannot cross opaque and alpha draw-call contracts. Explicit state
    /// remains uniform for the entire source mesh.
    /// </summary>
    public static SmoSkinnedRenderableOpacityPlan ClassifyRenderables(
        IReadOnlyList<SmoSkinnedBranchSourceMesh> meshes,
        ImportedTexture texture,
        SkinnedRenderableMaterialProfile? materialProfile = null)
    {
        ArgumentNullException.ThrowIfNull(meshes);
        ArgumentNullException.ThrowIfNull(texture);
        materialProfile ??= SkinnedRenderableMaterialProfile.Default;
        using Image<Rgba32> image = Image.Load<Rgba32>(texture.Data);
        if (image.Width != texture.Width || image.Height != texture.Height)
        {
            throw new InvalidDataException(
                $"Texture {texture.Name} declares {texture.Width}x{texture.Height}, " +
                $"but its image is {image.Width}x{image.Height}.");
        }

        bool[,] alpha = new bool[image.Height, image.Width];
        int[,] prefix = new int[image.Height + 1, image.Width + 1];
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < accessor.Height; y++)
            {
                ReadOnlySpan<Rgba32> row = accessor.GetRowSpan(y);
                int running = 0;
                for (int x = 0; x < row.Length; x++)
                {
                    alpha[y, x] = row[x].A < byte.MaxValue;
                    if (alpha[y, x])
                        running++;
                    prefix[y + 1, x + 1] = prefix[y, x + 1] + running;
                }
            }
        });

        var classifications = new Dictionary<
            int, SmoSkinnedRenderableMaterialFamily[]>();
        int opaqueBodyCount = 0;
        int opaqueOverlayCount = 0;
        int alphaCount = 0;
        foreach (SmoSkinnedBranchSourceMesh mesh in meshes.OrderBy(item => item.Key))
        {
            if (mesh.TextureCoordinates.Length != mesh.Positions.Length)
                throw new InvalidDataException(
                    $"Mesh {mesh.Name} has no complete TEXCOORD_0 stream.");
            if (mesh.TriangleIndices.Length % 3 != 0)
                throw new InvalidDataException(
                    $"Mesh {mesh.Name} has an incomplete triangle index stream.");
            int triangleCount = mesh.TriangleIndices.Length / 3;
            var triangleUsesAlpha = new bool[triangleCount];
            var triangleVertices = new int[mesh.TriangleIndices.Length];
            for (int triangle = 0; triangle < triangleCount; triangle++)
            {
                int index = triangle * 3;
                int first = CheckedVertex(mesh, mesh.TriangleIndices[index]);
                int second = CheckedVertex(mesh, mesh.TriangleIndices[index + 1]);
                int third = CheckedVertex(mesh, mesh.TriangleIndices[index + 2]);
                triangleVertices[index] = first;
                triangleVertices[index + 1] = second;
                triangleVertices[index + 2] = third;
                triangleUsesAlpha[triangle] = TriangleCanSampleAlpha(
                    mesh.TextureCoordinates[first],
                    mesh.TextureCoordinates[second],
                    mesh.TextureCoordinates[third],
                    image.Width,
                    image.Height,
                    alpha,
                    prefix,
                    mesh.Name,
                    triangle);
            }
            SmoSkinnedRenderableMaterialFamily[] values =
                materialProfile.GetMode(mesh.Key) switch
                {
                    SkinnedRenderableMaterialMode.OpaqueOverlay =>
                        Enumerable.Repeat(
                            SmoSkinnedRenderableMaterialFamily.OpaqueOverlay,
                            triangleCount).ToArray(),
                    SkinnedRenderableMaterialMode.TransparentSurface =>
                        Enumerable.Repeat(
                            SmoSkinnedRenderableMaterialFamily.AlphaBlend,
                            triangleCount).ToArray(),
                    _ => ClassifyAutomaticComponents(
                        mesh, triangleVertices, triangleUsesAlpha)
                };
            foreach (SmoSkinnedRenderableMaterialFamily family in values)
            {
                switch (family)
                {
                    case SmoSkinnedRenderableMaterialFamily.OpaqueBody:
                        opaqueBodyCount++;
                        break;
                    case SmoSkinnedRenderableMaterialFamily.OpaqueOverlay:
                        opaqueOverlayCount++;
                        break;
                    case SmoSkinnedRenderableMaterialFamily.AlphaBlend:
                        alphaCount++;
                        break;
                    default:
                        throw new InvalidOperationException(
                            $"Unsupported renderable material family {family}.");
                }
            }
            if (!classifications.TryAdd(mesh.Key, values))
                throw new InvalidDataException($"Duplicate imported mesh key {mesh.Key}.");
        }
        return new SmoSkinnedRenderableOpacityPlan(
            classifications, opaqueBodyCount, opaqueOverlayCount, alphaCount);
    }

    private static SmoSkinnedRenderableMaterialFamily[] ClassifyAutomaticComponents(
        SmoSkinnedBranchSourceMesh mesh,
        IReadOnlyList<int> triangleVertices,
        IReadOnlyList<bool> triangleUsesAlpha)
    {
        int triangleCount = mesh.TriangleIndices.Length / 3;
        if (triangleVertices.Count != mesh.TriangleIndices.Length ||
            triangleUsesAlpha.Count != triangleCount)
        {
            throw new InvalidOperationException(
                $"Mesh {mesh.Name} alpha classification inputs are inconsistent.");
        }
        if (triangleCount == 0)
            return [];

        var connectivity = new UnionFind(mesh.Positions.Length);
        var referenced = new bool[mesh.Positions.Length];
        for (int triangle = 0; triangle < triangleCount; triangle++)
        {
            int index = triangle * 3;
            int first = triangleVertices[index];
            int second = triangleVertices[index + 1];
            int third = triangleVertices[index + 2];
            referenced[first] = referenced[second] = referenced[third] = true;
            connectivity.Union(first, second);
            connectivity.Union(first, third);
        }

        // OBJ/glTF attribute seams commonly duplicate the vertices along one
        // geometric edge. Join raw index components only when they share at
        // least two exactly equal positions. One coincident point can merely be
        // contact between otherwise separate clothing or accessory surfaces;
        // an epsilon could likewise join surfaces which are only very close.
        var verticesByPosition = new Dictionary<Vector3, List<int>>();
        for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
        {
            if (!referenced[vertex])
                continue;
            Vector3 position = mesh.Positions[vertex];
            if (!verticesByPosition.TryGetValue(position, out List<int>? equal))
            {
                equal = [];
                verticesByPosition.Add(position, equal);
            }
            equal.Add(vertex);
        }
        var sharedPositionCount = new Dictionary<(int First, int Second), int>();
        foreach (List<int> equal in verticesByPosition.Values)
        {
            int[] roots = equal.Select(connectivity.Find)
                .Distinct()
                .Order()
                .ToArray();
            if (roots.Length > 32)
            {
                throw new InvalidDataException(
                    $"Mesh {mesh.Name} has more than 32 overlapping raw components " +
                    "at one position; alpha-component connectivity is ambiguous.");
            }
            for (int first = 0; first < roots.Length; first++)
            {
                for (int second = first + 1; second < roots.Length; second++)
                {
                    var pair = (roots[first], roots[second]);
                    sharedPositionCount[pair] =
                        sharedPositionCount.GetValueOrDefault(pair) + 1;
                }
            }
        }
        foreach (KeyValuePair<(int First, int Second), int> pair in sharedPositionCount)
        {
            if (pair.Value >= 2)
                connectivity.Union(pair.Key.First, pair.Key.Second);
        }

        var alphaByComponent = new Dictionary<int, bool>();
        for (int triangle = 0; triangle < triangleCount; triangle++)
        {
            int root = connectivity.Find(triangleVertices[triangle * 3]);
            alphaByComponent[root] =
                alphaByComponent.GetValueOrDefault(root) || triangleUsesAlpha[triangle];
        }
        var result = new SmoSkinnedRenderableMaterialFamily[triangleCount];
        for (int triangle = 0; triangle < triangleCount; triangle++)
        {
            int root = connectivity.Find(triangleVertices[triangle * 3]);
            result[triangle] = alphaByComponent[root]
                ? SmoSkinnedRenderableMaterialFamily.AlphaBlend
                : SmoSkinnedRenderableMaterialFamily.OpaqueBody;
        }
        return result;
    }

    public static SmoSkinnedBranchSplitAnalysis Analyze(
        SmoDocument target,
        uint textureObjectId,
        IReadOnlyList<SmoSkinnedBranchSourceMesh> meshes,
        SmoSkinnedRenderableOpacityPlan opacity,
        IReadOnlyDictionary<string, string> boneRemap,
        IReadOnlyDictionary<string, Matrix4x4> targetInverseBind)
    {
        BuildContext context = BuildContext.Create(target, textureObjectId);
        PalettePlan[] plans = BuildPalettePlans(
            meshes, opacity, boneRemap, targetInverseBind);
        int vertices = plans.Sum(plan => CountUniqueVertices(plan.Triangles));
        return new SmoSkinnedBranchSplitAnalysis(
            plans.Length,
            vertices,
            plans.Sum(plan => plan.Triangles.Count));
    }

    public static SmoSkinnedBranchSplitResult Inject(
        SmoDocument target,
        uint textureObjectId,
        IReadOnlyList<SmoSkinnedBranchSourceMesh> meshes,
        SmoSkinnedRenderableOpacityPlan opacity,
        IReadOnlyDictionary<string, string> boneRemap,
        IReadOnlyDictionary<string, Matrix4x4> targetInverseBind)
    {
        ArgumentNullException.ThrowIfNull(target);
        BuildContext context = BuildContext.Create(target, textureObjectId);
        PalettePlan[] plans = BuildPalettePlans(
            meshes, opacity, boneRemap, targetInverseBind);
        if (plans.Length == 0)
        {
            return new SmoSkinnedBranchSplitResult(
                target.Data.ToArray(), 0, 0, 0,
                new HashSet<uint>(), new HashSet<uint>(), new HashSet<uint>(),
                new Dictionary<uint, SmoSkinnedRenderableMaterialFamily>());
        }

        var allocator = new ObjectIdAllocator(target.Objects.Select(entry => entry.Id));
        var attachments = new List<SmoVisualForestAttachment>(plans.Length);
        var addedIds = new HashSet<uint>();
        var addedSkinIds = new HashSet<uint>();
        var addedMeshIds = new HashSet<uint>();
        var addedObjectFamilies =
            new Dictionary<uint, SmoSkinnedRenderableMaterialFamily>();
        int vertexCount = 0;
        int triangleCount = 0;
        int materialRunIndex = 0;

        for (int branchIndex = 0; branchIndex < plans.Length; branchIndex++)
        {
            PalettePlan plan = plans[branchIndex];
            bool expectedRunStart = branchIndex == 0 ||
                plans[branchIndex - 1].SourceMeshKey != plan.SourceMeshKey;
            if (plan.StartsRenderable != expectedRunStart)
            {
                throw new InvalidDataException(
                    $"Alpha branch {branchIndex} changed source-renderable run boundaries.");
            }
            uint skinId = allocator.Take();
            uint meshId = allocator.Take();
            string prefix = plan.MaterialFamily ==
                SmoSkinnedRenderableMaterialFamily.OpaqueOverlay
                    ? "imp_o"
                    : "imp_a";
            BuiltObject? material = null;
            if (plan.StartsRenderable)
            {
                uint materialId = allocator.Take();
                material = plan.MaterialFamily ==
                    SmoSkinnedRenderableMaterialFamily.OpaqueOverlay
                        ? BuildOpaqueOverlayMaterial(
                            target,
                            context,
                            materialId,
                            $"{prefix}_m_{textureObjectId:X8}_{materialRunIndex:D2}")
                        : BuildSkinnedTransparentSurfaceMaterial(
                            target,
                            context,
                            materialId,
                            $"{prefix}_m_{textureObjectId:X8}_{materialRunIndex:D2}");
                addedIds.Add(materialId);
                addedObjectFamilies.Add(materialId, plan.MaterialFamily);
                materialRunIndex++;
            }

            BuiltObject mesh = BuildMesh(
                target,
                context.MeshTemplate,
                meshId,
                $"{prefix}_x_{textureObjectId:X8}_{branchIndex:D2}",
                plan,
                boneRemap);
            SmoObjectEntry skinTemplate = plan.StartsRenderable
                ? context.PrimarySkin
                : context.ContinuationSkin;
            BuiltObject skin = BuildSkin(
                target,
                context,
                skinTemplate,
                skinId,
                $"{prefix}_s_{textureObjectId:X8}_{branchIndex:D2}",
                plan.MaterialFamily,
                material,
                mesh,
                plan.PaletteNames,
                targetInverseBind);
            attachments.Add(WrapSkinAttachment(
                target, context.Render, skinTemplate, skin));

            addedIds.Add(skinId);
            addedIds.Add(meshId);
            addedSkinIds.Add(skinId);
            addedMeshIds.Add(meshId);
            addedObjectFamilies.Add(skinId, plan.MaterialFamily);
            addedObjectFamilies.Add(meshId, plan.MaterialFamily);
            vertexCount += mesh.VertexCount;
            triangleCount += mesh.TriangleCount;
        }

        byte[] output = SmoVisualForestInjector.Inject(
            target, context.Render.Id, attachments);
        var result = new SmoSkinnedBranchSplitResult(
            output,
            plans.Length,
            vertexCount,
            triangleCount,
            addedIds,
            addedSkinIds,
            addedMeshIds,
            addedObjectFamilies);
        Verify(
            target,
            result,
            textureObjectId,
            targetInverseBind,
            materialRunIndex);
        return result;
    }

    private static PalettePlan[] BuildPalettePlans(
        IReadOnlyList<SmoSkinnedBranchSourceMesh> meshes,
        SmoSkinnedRenderableOpacityPlan opacity,
        IReadOnlyDictionary<string, string> boneRemap,
        IReadOnlyDictionary<string, Matrix4x4> targetInverseBind)
    {
        ArgumentNullException.ThrowIfNull(meshes);
        ArgumentNullException.ThrowIfNull(opacity);
        ArgumentNullException.ThrowIfNull(boneRemap);
        ArgumentNullException.ThrowIfNull(targetInverseBind);

        var split = new List<PalettePlan>();
        int classifiedTriangleCount = 0;
        foreach (SmoSkinnedBranchSourceMesh mesh in meshes.OrderBy(item => item.Key))
        {
            TrianglePlan[] branchTriangles = Enumerable.Range(
                    0, mesh.TriangleIndices.Length / 3)
                .Where(triangle => opacity.IsSeparateBranch(mesh.Key, triangle))
                .Select(triangle => new TrianglePlan(
                    mesh,
                    triangle,
                    opacity.GetFamily(mesh.Key, triangle),
                    GetTriangleBones(mesh, triangle, boneRemap, targetInverseBind)))
                .ToArray();
            classifiedTriangleCount += branchTriangles.Length;

            // A native spSkin is a sortable draw unit. Never bin-pack triangles
            // from different imported renderables into one unit: face cards,
            // jewelry, and wings need independent bounds/order even when their
            // bone sets happen to fit in the same 16-slot palette.
            var bins = new List<PalettePlan>();
            foreach (TrianglePlan triangle in branchTriangles)
            {
                PalettePlan? selected = bins
                    .Where(bin => bin.MaterialFamily == triangle.MaterialFamily &&
                                  bin.Bones.Union(
                                      triangle.Bones,
                                      StringComparer.Ordinal).Count() <= PaletteCapacity)
                    .OrderBy(bin => triangle.Bones.Count(
                        name => !bin.Bones.Contains(name)))
                    .ThenBy(bin => bin.Triangles.Count)
                    .ThenBy(bin => bin.Ordinal)
                    .FirstOrDefault();
                if (selected is null)
                {
                    selected = new PalettePlan(
                        bins.Count,
                        mesh.Key,
                        false,
                        triangle.MaterialFamily,
                        triangle.Bones);
                    bins.Add(selected);
                }
                else
                {
                    selected.Bones.UnionWith(triangle.Bones);
                }
                selected.Triangles.Add(triangle);
            }

            int firstPlanForRenderable = split.Count;
            foreach (PalettePlan source in bins)
            {
                PalettePlan current = new(
                    split.Count,
                    mesh.Key,
                    split.Count == firstPlanForRenderable,
                    source.MaterialFamily,
                    source.Bones);
                var vertices = new HashSet<(int Mesh, int Vertex)>();
                foreach (TrianglePlan triangle in source.Triangles)
                {
                    (int Mesh, int Vertex)[] triangleVertices = TriangleVertices(triangle);
                    int added = triangleVertices.Count(vertex => !vertices.Contains(vertex));
                    if (current.Triangles.Count > 0 &&
                        vertices.Count + added > ushort.MaxValue)
                    {
                        split.Add(current);
                        current = new PalettePlan(
                            split.Count,
                            mesh.Key,
                            false,
                            source.MaterialFamily,
                            source.Bones);
                        vertices.Clear();
                    }
                    current.Triangles.Add(triangle);
                    vertices.UnionWith(triangleVertices);
                }
                if (current.Triangles.Count > 0)
                    split.Add(current);
            }
        }
        if (classifiedTriangleCount != opacity.SeparateBranchTriangleCount)
            throw new InvalidDataException(
                "Separate renderable classification count changed before branch planning.");
        return split.ToArray();
    }

    private static HashSet<string> GetTriangleBones(
        SmoSkinnedBranchSourceMesh mesh,
        int triangleOrdinal,
        IReadOnlyDictionary<string, string> boneRemap,
        IReadOnlyDictionary<string, Matrix4x4> targetInverseBind)
    {
        var result = new HashSet<string>(StringComparer.Ordinal);
        int index = triangleOrdinal * 3;
        AddVertex(CheckedVertex(mesh, mesh.TriangleIndices[index]));
        AddVertex(CheckedVertex(mesh, mesh.TriangleIndices[index + 1]));
        AddVertex(CheckedVertex(mesh, mesh.TriangleIndices[index + 2]));
        if (result.Count is 0 or > PaletteCapacity)
            throw new InvalidDataException(
                $"Mesh {mesh.Name} triangle {triangleOrdinal} needs {result.Count} bones; " +
                $"a native palette supports 1..{PaletteCapacity}.");
        return result;

        void AddVertex(int vertex)
        {
            Vector4 weights = mesh.Skinning.Weights[vertex];
            ImportedJointIndices joints = mesh.Skinning.JointIndices[vertex];
            Add(weights.X, joints.X);
            Add(weights.Y, joints.Y);
            Add(weights.Z, joints.Z);
            Add(weights.W, joints.W);
        }

        void Add(float weight, ushort joint)
        {
            if (weight <= WeightEpsilon)
                return;
            if (!float.IsFinite(weight) || weight < 0 ||
                joint >= mesh.Skinning.Skeleton.JointNames.Count)
            {
                throw new InvalidDataException(
                    $"Mesh {mesh.Name} triangle {triangleOrdinal} has an invalid skin influence.");
            }
            string donorName = mesh.Skinning.Skeleton.JointNames[joint];
            if (!boneRemap.TryGetValue(donorName, out string? targetName) ||
                !targetInverseBind.ContainsKey(targetName))
            {
                throw new InvalidOperationException(
                    $"Bone {donorName} has no complete target bind mapping.");
            }
            result.Add(targetName);
        }
    }

    private static BuiltObject BuildSkinnedTransparentSurfaceMaterial(
        SmoDocument document,
        BuildContext context,
        uint id,
        string name) => BuildMaterial(
        document,
        context,
        id,
        name,
        SkinnedTransparentSurfaceFinalBlendOperation,
        SkinnedTransparentSurfaceMaterialRenderStates,
        SkinnedTransparentSurfaceLightingTextureStates);

    private static BuiltObject BuildOpaqueOverlayMaterial(
        SmoDocument document,
        BuildContext context,
        uint id,
        string name) => BuildMaterial(
        document,
        context,
        id,
        name,
        OpaqueOverlayFinalBlendOperation,
        OpaqueOverlayMaterialRenderStates,
        OpaqueOverlayLightingTextureStates);

    private static BuiltObject BuildMaterial(
        SmoDocument document,
        BuildContext context,
        uint id,
        string name,
        uint finalBlendOperation,
        IReadOnlyList<uint> materialRenderStates,
        IReadOnlyList<uint> lightingTextureStates)
    {
        ReadOnlySpan<byte> source = ObjectBytes(document, context.OpaqueMaterial);
        SmoDataBlockHeader textureField = FindInlineChildField(
            document, context.OpaqueMaterial, context.Texture);
        using var stream = new MemoryStream();
        stream.Write(source[..ObjectSignatureSize]);
        int offset = ObjectSignatureSize;
        while (offset < source.Length &&
               SmoDataBlockReader.TryReadHeader(source, offset, out SmoDataBlockHeader field))
        {
            if (field.Offset == textureField.Offset)
            {
                WriteReferenceOnlyField(stream, source, field, context.Texture.Id);
            }
            else if (field.FieldType == 0 &&
                     field.PayloadSize == materialRenderStates.Count * sizeof(uint))
            {
                stream.Write(source.Slice(field.Offset, field.HeaderSize));
                foreach (uint value in materialRenderStates)
                    WriteUInt32(stream, value);
            }
            else if (field.FieldType == 3 && field.PayloadSize == sizeof(uint))
            {
                stream.Write(source.Slice(field.Offset, field.HeaderSize));
                WriteUInt32(stream, finalBlendOperation);
            }
            else if (field.FieldType == 17 &&
                     field.PayloadSize == lightingTextureStates.Count * sizeof(uint))
            {
                stream.Write(source.Slice(field.Offset, field.HeaderSize));
                foreach (uint value in lightingTextureStates)
                    WriteUInt32(stream, value);
            }
            else
            {
                stream.Write(source.Slice(
                    field.Offset, checked((int)field.PayloadEnd - field.Offset)));
            }
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != source.Length)
            throw new InvalidDataException(
                $"Material template [{context.OpaqueMaterial.Index}] has an invalid field stream.");
        byte[] data = stream.ToArray();
        return BuiltObject.CreateRoot(
            id, RawName(name), SmoClassIds.MaterialData, data);
    }

    private static BuiltObject BuildMesh(
        SmoDocument document,
        SmoObjectEntry templateEntry,
        uint id,
        string name,
        PalettePlan plan,
        IReadOnlyDictionary<string, string> boneRemap)
    {
        SmoMesh template = SmoMeshDecoder.Decode(document, templateEntry);
        if (template.Marker != SmoMeshDecoder.E1Marker ||
            !SmoVertexLayoutRegistry.TryGet(template.VertexFormat, out SmoVertexLayout? layout) ||
            layout is null || layout.SerializedStride != template.Stride ||
            layout.BlendWeightsOffset is null || layout.BlendIndicesOffset is null)
        {
            throw new InvalidOperationException(
                $"Mesh template [{templateEntry.Index}] is not a writable skinned E1 layout.");
        }

        string[] names = plan.PaletteNames;
        var palette = names.Select((bone, index) => (bone, index))
            .ToDictionary(item => item.bone, item => checked((byte)item.index),
                StringComparer.Ordinal);
        var vertexMap = new Dictionary<(int Mesh, int Vertex), ushort>();
        var records = new List<byte[]>();
        var indices = new List<ushort>(checked(plan.Triangles.Count * 3));
        foreach (TrianglePlan triangle in plan.Triangles)
        {
            int triangleOffset = triangle.TriangleOrdinal * 3;
            AddVertex(CheckedVertex(triangle.Mesh,
                triangle.Mesh.TriangleIndices[triangleOffset]));
            AddVertex(CheckedVertex(triangle.Mesh,
                triangle.Mesh.TriangleIndices[triangleOffset + 1]));
            AddVertex(CheckedVertex(triangle.Mesh,
                triangle.Mesh.TriangleIndices[triangleOffset + 2]));

            void AddVertex(int sourceVertex)
            {
                var key = (triangle.Mesh.Key, sourceVertex);
                if (!vertexMap.TryGetValue(key, out ushort targetVertex))
                {
                    if (records.Count >= ushort.MaxValue)
                        throw new InvalidOperationException(
                            $"Generated alpha mesh {name} exceeds 65,535 vertices.");
                    byte[] record = BuildVertexRecord(
                        triangle.Mesh,
                        sourceVertex,
                        layout,
                        plan.MaterialFamily,
                        palette,
                        boneRemap);
                    targetVertex = checked((ushort)records.Count);
                    records.Add(record);
                    vertexMap.Add(key, targetVertex);
                }
                indices.Add(targetVertex);
            }
        }

        int indexBytes = checked(indices.Count * sizeof(ushort));
        int vertexBytes = checked(records.Count * template.Stride);
        const int preambleSize = 17;
        const int primitiveHeaderSize = 12;
        const int vertexHeaderSize = 12;
        int payloadSize = checked(
            preambleSize + primitiveHeaderSize + indexBytes + vertexHeaderSize + vertexBytes);
        byte[] result = new byte[checked(ObjectSignatureSize + 5 + payloadSize + 1)];
        WriteUInt32(result, 0, SmoClassIds.MeshData);
        "SBOO"u8.CopyTo(result.AsSpan(4));
        result[8] = SmoMeshDecoder.E1Marker;
        WriteUInt32(result, 9, checked((uint)payloadSize));
        int payload = 13;
        WriteUInt32(result, payload, template.VertexFormat);
        WriteUInt32(result, payload + 4, checked((uint)records.Count));
        WriteUInt32(result, payload + 8,
            checked((uint)(records.Count * template.RuntimeStride)));
        WriteUInt32(result, payload + 12, checked((uint)indexBytes));
        result[payload + 16] = 0;
        int primitive = payload + preambleSize;
        WriteUInt32(result, primitive, SmoMeshDecoder.TriangleListPrimitive);
        WriteUInt32(result, primitive + 4, checked((uint)plan.Triangles.Count));
        WriteUInt32(result, primitive + 8, 0);
        int indexOffset = primitive + primitiveHeaderSize;
        for (int index = 0; index < indices.Count; index++)
            WriteUInt16(result, indexOffset + index * sizeof(ushort), indices[index]);
        int vertexHeader = indexOffset + indexBytes;
        WriteUInt32(result, vertexHeader, template.VertexFormat);
        WriteUInt32(result, vertexHeader + 4, checked((uint)records.Count));
        WriteUInt32(result, vertexHeader + 8, 0);
        int vertexOffset = vertexHeader + vertexHeaderSize;
        foreach (byte[] record in records)
        {
            record.CopyTo(result.AsSpan(vertexOffset));
            vertexOffset += record.Length;
        }
        return new BuiltObject(
            result,
            [new ObjectPlacement(
                id, RawName(name), SmoClassIds.MeshData, 0, checked((uint)result.Length))],
            records.Count,
            plan.Triangles.Count);
    }

    private static byte[] BuildVertexRecord(
        SmoSkinnedBranchSourceMesh mesh,
        int vertex,
        SmoVertexLayout layout,
        SmoSkinnedRenderableMaterialFamily materialFamily,
        IReadOnlyDictionary<string, byte> palette,
        IReadOnlyDictionary<string, string> boneRemap)
    {
        byte[] record = new byte[layout.SerializedStride];
        WriteVector3(record, 0, mesh.Positions[vertex]);
        if (layout.NormalOffset is int normalOffset)
            WriteVector3(record, normalOffset, mesh.Normals[vertex]);
        if (layout.DiffuseArgbOffset is int diffuseOffset)
        {
            uint diffuse = materialFamily switch
            {
                SmoSkinnedRenderableMaterialFamily.OpaqueOverlay =>
                    OpaqueOverlayVertexDiffuse,
                SmoSkinnedRenderableMaterialFamily.AlphaBlend =>
                    SkinnedTransparentSurfaceVertexDiffuse,
                _ => throw new InvalidOperationException(
                    $"Unsupported generated material family {materialFamily}.")
            };
            WriteUInt32(record, diffuseOffset, diffuse);
        }
        Vector2 uv = mesh.TextureCoordinates[vertex];
        if (layout.TextureCoordinate0Offset is int uv0Offset)
            WriteVector2(record, uv0Offset, uv);
        if (layout.TextureCoordinate1Offset is int uv1Offset)
            WriteVector2(record, uv1Offset, uv);

        var influences = new Dictionary<byte, float>();
        Vector4 sourceWeights = mesh.Skinning.Weights[vertex];
        ImportedJointIndices sourceIndices = mesh.Skinning.JointIndices[vertex];
        Add(sourceWeights.X, sourceIndices.X);
        Add(sourceWeights.Y, sourceIndices.Y);
        Add(sourceWeights.Z, sourceIndices.Z);
        Add(sourceWeights.W, sourceIndices.W);
        (byte Slot, float Weight)[] ordered = influences
            .OrderByDescending(item => item.Value)
            .ThenBy(item => item.Key)
            .Take(4)
            .Select(item => (item.Key, item.Value))
            .ToArray();
        float total = ordered.Sum(item => item.Weight);
        if (!float.IsFinite(total) || total <= WeightEpsilon)
            throw new InvalidDataException(
                $"Mesh {mesh.Name} vertex {vertex} has no positive skin weights.");
        float[] weights = new float[4];
        byte[] indices = new byte[4];
        for (int index = 0; index < ordered.Length; index++)
        {
            indices[index] = ordered[index].Slot;
            weights[index] = ordered[index].Weight / total;
        }
        WriteVector4(record, layout.BlendWeightsOffset!.Value,
            new Vector4(weights[0], weights[1], weights[2], weights[3]));
        indices.CopyTo(record, layout.BlendIndicesOffset!.Value);
        return record;

        void Add(float weight, ushort joint)
        {
            if (weight <= WeightEpsilon)
                return;
            string donorName = mesh.Skinning.Skeleton.JointNames[joint];
            if (!boneRemap.TryGetValue(donorName, out string? targetName) ||
                !palette.TryGetValue(targetName, out byte slot))
            {
                throw new InvalidOperationException(
                    $"Bone {donorName} is absent from the generated alpha palette.");
            }
            influences[slot] = influences.GetValueOrDefault(slot) + weight;
        }
    }

    private static BuiltObject BuildSkin(
        SmoDocument document,
        BuildContext context,
        SmoObjectEntry templateEntry,
        uint id,
        string name,
        SmoSkinnedRenderableMaterialFamily materialFamily,
        BuiltObject? material,
        BuiltObject mesh,
        IReadOnlyList<string> paletteNames,
        IReadOnlyDictionary<string, Matrix4x4> targetInverseBind)
    {
        ReadOnlySpan<byte> source = ObjectBytes(document, templateEntry);
        SmoObjectEntry templateMesh = document.Objects.Single(entry =>
            entry.ParentIndex == templateEntry.Index &&
            entry.TypeHash == SmoClassIds.MeshData);
        SmoDataBlockHeader meshField = FindInlineChildField(
            document, templateEntry, templateMesh);
        SmoDataBlockHeader paletteField = FindPaletteField(source, PaletteCapacity);
        SmoDataBlockHeader helperField = FindObjectReferenceField(
            source, context.Helper.Id);
        SmoDataBlockHeader? materialField = null;
        if (material is not null)
        {
            materialField = FindInlineChildField(
                document, templateEntry, context.OpaqueMaterial);
        }
        byte[] palette = BuildReferencePaletteField(
            source.Slice(paletteField.Offset, paletteField.HeaderSize),
            paletteField,
            paletteNames,
            context.TargetNodes,
            targetInverseBind);

        using var stream = new MemoryStream();
        stream.Write(source[..ObjectSignatureSize]);
        var placements = new List<ObjectPlacement>
        {
            new(id, RawName(name), SmoClassIds.Skin, 0, 0)
        };
        int offset = ObjectSignatureSize;
        while (offset < source.Length &&
               SmoDataBlockReader.TryReadHeader(source, offset, out SmoDataBlockHeader field))
        {
            if (materialField.HasValue && field.Offset == materialField.Value.Offset)
            {
                WriteResizedInlineField(stream, source, field, material!, placements);
            }
            else if (field.Offset == helperField.Offset)
            {
                WriteReferenceOnlyField(stream, source, field, context.Helper.Id);
            }
            else if (field.Offset == meshField.Offset)
            {
                WriteResizedInlineField(stream, source, field, mesh, placements);
            }
            else if (field.Offset == paletteField.Offset)
            {
                stream.Write(palette);
            }
            else if (field.FieldType == 2 && field.PayloadSize == sizeof(uint))
            {
                stream.Write(source.Slice(field.Offset, field.HeaderSize));
                uint alphaSortEnable = materialFamily switch
                {
                    SmoSkinnedRenderableMaterialFamily.OpaqueOverlay =>
                        OpaqueOverlayAlphaSortEnable,
                    SmoSkinnedRenderableMaterialFamily.AlphaBlend =>
                        SkinnedTransparentSurfaceAlphaSortEnable,
                    _ => throw new InvalidOperationException(
                        $"Unsupported generated material family {materialFamily}.")
                };
                WriteUInt32(stream, alphaSortEnable);
            }
            else if (field.FieldType == 3 && field.PayloadSize == sizeof(uint))
            {
                stream.Write(source.Slice(field.Offset, field.HeaderSize));
                WriteUInt32(stream, GeneratedPriority);
            }
            else
            {
                stream.Write(source.Slice(
                    field.Offset, checked((int)field.PayloadEnd - field.Offset)));
            }
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != source.Length)
            throw new InvalidDataException(
                $"Skin template [{templateEntry.Index}] has an invalid field stream.");
        byte[] data = stream.ToArray();
        placements[0] = placements[0] with { SerializedSize = checked((uint)data.Length) };
        return new BuiltObject(data, placements, mesh.VertexCount, mesh.TriangleCount);
    }

    private static byte[] BuildReferencePaletteField(
        ReadOnlySpan<byte> originalHeader,
        SmoDataBlockHeader paletteField,
        IReadOnlyList<string> names,
        IReadOnlyDictionary<string, SmoObjectEntry> targetNodes,
        IReadOnlyDictionary<string, Matrix4x4> targetInverseBind)
    {
        if (names.Count is 0 or > PaletteCapacity)
            throw new InvalidDataException(
                $"Generated palette has {names.Count} unique bones.");
        if (paletteField.SizeKind != SmoDataBlockSizeCode.UInt32)
            throw new InvalidDataException("Skin palette does not use a writable UInt32 size.");
        string[] padded = Enumerable.Range(0, PaletteCapacity)
            .Select(index => index < names.Count ? names[index] : names[0])
            .ToArray();
        int payloadSize = checked(
            8 + padded.Length * (ObjectReferenceSize + 16 * sizeof(float)));
        byte[] result = new byte[checked(originalHeader.Length + payloadSize)];
        originalHeader.CopyTo(result);
        WriteUInt32(result, originalHeader.Length - sizeof(uint), checked((uint)payloadSize));
        WriteUInt32(result, originalHeader.Length, 0);
        WriteUInt32(result, originalHeader.Length + sizeof(uint), checked((uint)padded.Length));
        int cursor = originalHeader.Length + 8;
        foreach (string name in padded)
        {
            if (!targetNodes.TryGetValue(name, out SmoObjectEntry? node) ||
                !targetInverseBind.TryGetValue(name, out Matrix4x4 inverseBind))
            {
                throw new InvalidOperationException(
                    $"Generated palette bone {name} has no unique target node/bind matrix.");
            }
            WriteUInt32(result, cursor, node.Id);
            WriteUInt32(result, cursor + sizeof(uint), 0);
            WriteMatrix(result.AsSpan(cursor + ObjectReferenceSize), inverseBind);
            cursor += ObjectReferenceSize + 16 * sizeof(float);
        }
        return result;
    }

    private static SmoVisualForestAttachment WrapSkinAttachment(
        SmoDocument document,
        SmoObjectEntry render,
        SmoObjectEntry templateSkin,
        BuiltObject skin)
    {
        SmoDataBlockHeader wrapper = FindInlineChildField(
            document, render, templateSkin);
        ReadOnlySpan<byte> renderBytes = ObjectBytes(document, render);
        byte[] header = renderBytes.Slice(wrapper.Offset, wrapper.HeaderSize).ToArray();
        PatchPayloadSize(
            header, 0, wrapper, checked((uint)(ObjectReferenceSize + skin.Data.Length)));
        byte[] fieldData = new byte[checked(
            header.Length + ObjectReferenceSize + skin.Data.Length)];
        header.CopyTo(fieldData, 0);
        WriteUInt32(fieldData, header.Length, skin.Root.Id);
        WriteUInt32(fieldData, header.Length + sizeof(uint),
            checked((uint)skin.Data.Length));
        int rootOffset = header.Length + ObjectReferenceSize;
        skin.Data.CopyTo(fieldData, rootOffset);
        SmoVisualForestEntry[] entries = skin.Placements.Select(placement =>
            new SmoVisualForestEntry(
                placement.Id,
                placement.RawName,
                placement.TypeHash,
                checked(rootOffset + placement.RelativeOffset),
                placement.SerializedSize)).ToArray();
        return new SmoVisualForestAttachment(render.Id, fieldData, entries);
    }

    private static void Verify(
        SmoDocument target,
        SmoSkinnedBranchSplitResult result,
        uint textureObjectId,
        IReadOnlyDictionary<string, Matrix4x4> targetInverseBind,
        int expectedMaterialRunCount)
    {
        SmoDocument output = SmoDocument.Parse(result.Data, target.SourcePath);
        var errors = new List<string>();
        if (output.HasErrors)
            errors.Add("strict parser reported errors");
        if (output.Objects.Select(entry => entry.Id).Distinct().Count() != output.Objects.Count)
            errors.Add("object IDs are not unique");
        Dictionary<uint, SmoObjectEntry> outputById = output.Objects
            .ToDictionary(entry => entry.Id);
        HashSet<uint> actualAddedIds = output.Objects
            .Select(entry => entry.Id)
            .Except(target.Objects.Select(entry => entry.Id))
            .ToHashSet();
        if (!actualAddedIds.SetEquals(result.AddedObjectIds) ||
            output.Objects.Count != target.Objects.Count + result.AddedObjectIds.Count)
        {
            errors.Add("material branch injection produced unplanned object identities");
        }
        foreach (SmoObjectEntry source in target.Objects)
        {
            if (!outputById.TryGetValue(source.Id, out SmoObjectEntry? retained) ||
                retained.TypeHash != source.TypeHash || retained.Name != source.Name)
            {
                errors.Add($"target object ID {source.Id} identity changed");
                continue;
            }
            uint? sourceParent = source.ParentIndex is int sourceParentIndex
                ? target.Objects[sourceParentIndex].Id
                : null;
            uint? retainedParent = retained.ParentIndex is int retainedParentIndex
                ? output.Objects[retainedParentIndex].Id
                : null;
            if (sourceParent != retainedParent)
                errors.Add($"target object ID {source.Id} parent changed");
            if (source.TypeHash is SmoClassIds.MaterialData or
                    SmoClassIds.Skin or SmoClassIds.MeshData &&
                !ObjectBytes(target, source).SequenceEqual(
                    ObjectBytes(output, retained)))
            {
                errors.Add(
                    $"existing visual object ID {source.Id} changed during material branch injection");
            }
        }

        SmoObjectEntry[] materials = output.Objects.Where(entry =>
            result.AddedObjectIds.Contains(entry.Id) &&
            entry.TypeHash == SmoClassIds.MaterialData).ToArray();
        if (materials.Length != expectedMaterialRunCount)
        {
            errors.Add(
                $"generated material runs {materials.Length} != planned " +
                expectedMaterialRunCount);
        }
        if (materials.Select(material => material.ParentIndex).Distinct().Count() !=
            materials.Length)
        {
            errors.Add("generated material starts do not have distinct spSkin owners");
        }
        foreach (SmoObjectEntry material in materials)
        {
            if (!result.AddedObjectFamilies.TryGetValue(
                    material.Id, out SmoSkinnedRenderableMaterialFamily family))
            {
                errors.Add($"generated material ID {material.Id} has no planned family");
                continue;
            }
            uint expectedBlend = family ==
                SmoSkinnedRenderableMaterialFamily.OpaqueOverlay
                    ? OpaqueOverlayFinalBlendOperation
                    : SkinnedTransparentSurfaceFinalBlendOperation;
            IReadOnlyList<uint> expectedRenderStates = family ==
                SmoSkinnedRenderableMaterialFamily.OpaqueOverlay
                    ? OpaqueOverlayMaterialRenderStates
                    : SkinnedTransparentSurfaceMaterialRenderStates;
            IReadOnlyList<uint> expectedLightingStates = family ==
                SmoSkinnedRenderableMaterialFamily.OpaqueOverlay
                    ? OpaqueOverlayLightingTextureStates
                    : SkinnedTransparentSurfaceLightingTextureStates;
            if (!SmoMaterialRenderState.TryDecode(
                    output, material, out SmoMaterialRenderStateInfo? materialState) ||
                materialState is null ||
                materialState.FinalBlendOperation != expectedBlend ||
                !materialState.MaterialRenderStates.SequenceEqual(
                    expectedRenderStates) ||
                !HasExactUInt32Field(
                    output, material, 17, expectedLightingStates))
            {
                errors.Add(
                    $"generated material ID {material.Id} does not match its " +
                    $"planned {family} native state");
            }
            if (material.ParentIndex is not int materialParentIndex ||
                !result.AddedSkinIds.Contains(output.Objects[materialParentIndex].Id))
            {
                errors.Add(
                    $"generated material ID {material.Id} is outside the " +
                    "generated spSkin run");
            }
        }

        SmoObjectEntry sourceTexture = target.Objects.Single(entry =>
            entry.Id == textureObjectId &&
            entry.TypeHash == SmoClassIds.TextureData);
        SmoObjectEntry sourceMaterial = target.Objects[sourceTexture.ParentIndex!.Value];
        SmoObjectEntry sourceSkin = target.Objects[sourceMaterial.ParentIndex!.Value];
        uint sourceRenderId = target.Objects[sourceSkin.ParentIndex!.Value].Id;

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(output);
        int triangles = 0;
        foreach (uint skinId in result.AddedSkinIds)
        {
            if (!result.AddedObjectFamilies.TryGetValue(
                    skinId, out SmoSkinnedRenderableMaterialFamily family))
            {
                errors.Add($"generated skin ID {skinId} has no planned family");
                continue;
            }
            string skinError = "object is absent";
            SmoSkin? skin = null;
            if (!outputById.TryGetValue(skinId, out SmoObjectEntry? skinEntry) ||
                !SmoSkinDecoder.TryDecode(
                    output, skinEntry, out skin, out skinError) || skin is null)
            {
                errors.Add($"generated skin ID {skinId} is invalid: {skinError}");
                continue;
            }
            uint expectedAlphaSortEnable = family ==
                SmoSkinnedRenderableMaterialFamily.OpaqueOverlay
                    ? OpaqueOverlayAlphaSortEnable
                    : SkinnedTransparentSurfaceAlphaSortEnable;
            if (skin.AlphaSortEnable != expectedAlphaSortEnable ||
                skin.Priority != GeneratedPriority ||
                skin.Bones.Count > PaletteCapacity)
            {
                errors.Add(
                    $"generated skin ID {skinId} changed {family} " +
                    "sort/priority/palette state");
            }
            if (skinEntry.ParentIndex is not int skinParentIndex ||
                output.Objects[skinParentIndex].Id != sourceRenderId)
            {
                errors.Add($"generated skin ID {skinId} is outside the source render branch");
            }
            foreach (SmoSkinBone bone in skin.Bones)
            {
                SmoObjectEntry node = output.Objects[bone.NodeObjectIndex];
                if (bone.InlineSerializedSize != 0 ||
                    !targetInverseBind.TryGetValue(node.Name, out Matrix4x4 expected) ||
                    !ApproximatelyEqual(bone.InverseBindMatrix, expected, 0.0001f))
                {
                    errors.Add($"generated skin ID {skinId} has an invalid target palette reference");
                    break;
                }
            }
        }
        foreach (uint meshId in result.AddedMeshIds)
        {
            if (!result.AddedObjectFamilies.TryGetValue(
                    meshId, out SmoSkinnedRenderableMaterialFamily family))
            {
                errors.Add($"generated mesh ID {meshId} has no planned family");
                continue;
            }
            if (!outputById.TryGetValue(meshId, out SmoObjectEntry? meshEntry))
            {
                errors.Add($"generated mesh ID {meshId} is absent");
                continue;
            }
            SmoMesh mesh = SmoMeshDecoder.Decode(output, meshEntry);
            triangles += mesh.TriangleCount;
            uint expectedDiffuse = family ==
                SmoSkinnedRenderableMaterialFamily.OpaqueOverlay
                    ? OpaqueOverlayVertexDiffuse
                    : SkinnedTransparentSurfaceVertexDiffuse;
            if (!mesh.HasDiffuseColors ||
                mesh.DiffuseColorsArgb.Any(color => color != expectedDiffuse))
                errors.Add(
                    $"generated mesh ID {meshId} changed {family} " +
                    $"vertex diffuse {expectedDiffuse:X8}");
            uint expectedBlend = family ==
                SmoSkinnedRenderableMaterialFamily.OpaqueOverlay
                    ? OpaqueOverlayFinalBlendOperation
                    : SkinnedTransparentSurfaceFinalBlendOperation;
            IReadOnlyList<uint> expectedRenderStates = family ==
                SmoSkinnedRenderableMaterialFamily.OpaqueOverlay
                    ? OpaqueOverlayMaterialRenderStates
                    : SkinnedTransparentSurfaceMaterialRenderStates;
            if (!bindings.TryGetValue(meshEntry.Index, out SmoTextureBinding? binding) ||
                binding.Issue is not null || binding.Texture is null ||
                output.Objects[binding.Texture.ObjectIndex].Id != textureObjectId ||
                binding.MaterialRenderState?.FinalBlendOperation !=
                    expectedBlend ||
                !binding.MaterialRenderState.MaterialRenderStates
                    .SequenceEqual(expectedRenderStates))
            {
                errors.Add(
                    $"generated mesh ID {meshId} has no exact {family} " +
                    "texture/material binding");
            }
            if (meshEntry.ParentIndex is not int parentIndex ||
                !result.AddedSkinIds.Contains(output.Objects[parentIndex].Id))
                errors.Add($"generated mesh ID {meshId} is outside the generated skin run");
        }
        if (triangles != result.TriangleCount)
            errors.Add(
                $"generated separate-branch triangles {triangles} != " +
                $"planned {result.TriangleCount}");
        if (errors.Count > 0)
        {
            throw new InvalidDataException(
                "Native Bloom material branch verification failed: " +
                string.Join("; ", errors.Distinct()) + ".");
        }
    }

    private static bool TriangleCanSampleAlpha(
        Vector2 first,
        Vector2 second,
        Vector2 third,
        int width,
        int height,
        bool[,] alpha,
        int[,] prefix,
        string meshName,
        int triangleOrdinal)
    {
        Vector2[] uv = [first, second, third];
        if (uv.Any(value => !float.IsFinite(value.X) || !float.IsFinite(value.Y) ||
                            value.X < 0 || value.X > 1 || value.Y < 0 || value.Y > 1))
        {
            throw new InvalidDataException(
                $"Mesh {meshName} triangle {triangleOrdinal} has UV outside the " +
                "verified atlas domain [0, 1].");
        }
        Vector2 a = new(first.X * width - 0.5f, first.Y * height - 0.5f);
        Vector2 b = new(second.X * width - 0.5f, second.Y * height - 0.5f);
        Vector2 c = new(third.X * width - 0.5f, third.Y * height - 0.5f);
        int minX = Math.Clamp(
            (int)MathF.Floor(MathF.Min(a.X, MathF.Min(b.X, c.X)) - 1.001f),
            0, width - 1);
        int maxX = Math.Clamp(
            (int)MathF.Ceiling(MathF.Max(a.X, MathF.Max(b.X, c.X)) + 1.001f),
            0, width - 1);
        int minY = Math.Clamp(
            (int)MathF.Floor(MathF.Min(a.Y, MathF.Min(b.Y, c.Y)) - 1.001f),
            0, height - 1);
        int maxY = Math.Clamp(
            (int)MathF.Ceiling(MathF.Max(a.Y, MathF.Max(b.Y, c.Y)) + 1.001f),
            0, height - 1);
        if (RectangleSum(prefix, minX, minY, maxX, maxY) == 0)
            return false;
        for (int y = minY; y <= maxY; y++)
        {
            for (int x = minX; x <= maxX; x++)
            {
                if (alpha[y, x] && TriangleIntersectsRectangle(
                        a, b, c,
                        x - 1.001f, y - 1.001f,
                        x + 1.001f, y + 1.001f))
                    return true;
            }
        }
        return false;
    }

    private static int RectangleSum(
        int[,] prefix,
        int minX,
        int minY,
        int maxX,
        int maxY) =>
        prefix[maxY + 1, maxX + 1] - prefix[minY, maxX + 1] -
        prefix[maxY + 1, minX] + prefix[minY, minX];

    private static bool TriangleIntersectsRectangle(
        Vector2 a,
        Vector2 b,
        Vector2 c,
        float left,
        float top,
        float right,
        float bottom)
    {
        if (PointInRectangle(a, left, top, right, bottom) ||
            PointInRectangle(b, left, top, right, bottom) ||
            PointInRectangle(c, left, top, right, bottom))
            return true;
        Vector2[] corners =
        [
            new(left, top), new(right, top),
            new(right, bottom), new(left, bottom)
        ];
        if (corners.Any(point => PointInTriangle(point, a, b, c)))
            return true;
        for (int edge = 0; edge < 3; edge++)
        {
            Vector2 t0 = edge switch { 0 => a, 1 => b, _ => c };
            Vector2 t1 = edge switch { 0 => b, 1 => c, _ => a };
            for (int side = 0; side < 4; side++)
                if (SegmentsIntersect(t0, t1, corners[side], corners[(side + 1) % 4]))
                    return true;
        }
        return false;
    }

    private static bool PointInRectangle(
        Vector2 point,
        float left,
        float top,
        float right,
        float bottom) =>
        point.X >= left && point.X <= right &&
        point.Y >= top && point.Y <= bottom;

    private static bool PointInTriangle(
        Vector2 point,
        Vector2 a,
        Vector2 b,
        Vector2 c)
    {
        float d1 = Cross(point - a, b - a);
        float d2 = Cross(point - b, c - b);
        float d3 = Cross(point - c, a - c);
        bool negative = d1 < -0.0001f || d2 < -0.0001f || d3 < -0.0001f;
        bool positive = d1 > 0.0001f || d2 > 0.0001f || d3 > 0.0001f;
        return !(negative && positive);
    }

    private static bool SegmentsIntersect(
        Vector2 a,
        Vector2 b,
        Vector2 c,
        Vector2 d)
    {
        float o1 = Cross(b - a, c - a);
        float o2 = Cross(b - a, d - a);
        float o3 = Cross(d - c, a - c);
        float o4 = Cross(d - c, b - c);
        if ((o1 > 0 && o2 < 0 || o1 < 0 && o2 > 0) &&
            (o3 > 0 && o4 < 0 || o3 < 0 && o4 > 0))
            return true;
        const float epsilon = 0.0001f;
        return MathF.Abs(o1) <= epsilon && PointOnSegment(c, a, b) ||
               MathF.Abs(o2) <= epsilon && PointOnSegment(d, a, b) ||
               MathF.Abs(o3) <= epsilon && PointOnSegment(a, c, d) ||
               MathF.Abs(o4) <= epsilon && PointOnSegment(b, c, d);
    }

    private static bool PointOnSegment(Vector2 point, Vector2 first, Vector2 second) =>
        point.X >= MathF.Min(first.X, second.X) - 0.0001f &&
        point.X <= MathF.Max(first.X, second.X) + 0.0001f &&
        point.Y >= MathF.Min(first.Y, second.Y) - 0.0001f &&
        point.Y <= MathF.Max(first.Y, second.Y) + 0.0001f;

    private static float Cross(Vector2 left, Vector2 right) =>
        left.X * right.Y - left.Y * right.X;

    private static int CheckedVertex(SmoSkinnedBranchSourceMesh mesh, uint index)
    {
        if (index >= mesh.Positions.Length)
            throw new InvalidDataException(
                $"Mesh {mesh.Name} index {index} is outside its vertex array.");
        return checked((int)index);
    }

    private static int CountUniqueVertices(IReadOnlyList<TrianglePlan> triangles) =>
        triangles.SelectMany(TriangleVertices).Distinct().Count();

    private static (int Mesh, int Vertex)[] TriangleVertices(TrianglePlan triangle)
    {
        int index = triangle.TriangleOrdinal * 3;
        return
        [
            (triangle.Mesh.Key, CheckedVertex(
                triangle.Mesh, triangle.Mesh.TriangleIndices[index])),
            (triangle.Mesh.Key, CheckedVertex(
                triangle.Mesh, triangle.Mesh.TriangleIndices[index + 1])),
            (triangle.Mesh.Key, CheckedVertex(
                triangle.Mesh, triangle.Mesh.TriangleIndices[index + 2]))
        ];
    }

    private static SmoDataBlockHeader FindInlineChildField(
        SmoDocument document,
        SmoObjectEntry owner,
        SmoObjectEntry child)
    {
        ReadOnlySpan<byte> source = ObjectBytes(document, owner);
        int offset = ObjectSignatureSize;
        while (offset < source.Length &&
               SmoDataBlockReader.TryReadHeader(source, offset, out SmoDataBlockHeader field))
        {
            if (field.PayloadSize == child.SerializedSize + ObjectReferenceSize &&
                BinaryPrimitives.ReadUInt32LittleEndian(source[field.PayloadOffset..]) == child.Id &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    source[(field.PayloadOffset + sizeof(uint))..]) == child.SerializedSize &&
                owner.PhysicalOffset + field.PayloadOffset + ObjectReferenceSize ==
                    child.PhysicalOffset)
                return field;
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidDataException(
            $"Object [{child.Index}] is not an inline field of [{owner.Index}].");
    }

    private static SmoDataBlockHeader FindObjectReferenceField(
        ReadOnlySpan<byte> source,
        uint objectId)
    {
        int offset = ObjectSignatureSize;
        while (offset < source.Length &&
               SmoDataBlockReader.TryReadHeader(source, offset, out SmoDataBlockHeader field))
        {
            if (field.PayloadSize >= ObjectReferenceSize &&
                BinaryPrimitives.ReadUInt32LittleEndian(source[field.PayloadOffset..]) == objectId)
                return field;
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidDataException(
            $"Object reference ID {objectId} was not found in the skin template.");
    }

    private static SmoDataBlockHeader FindPaletteField(
        ReadOnlySpan<byte> source,
        int expectedBoneCount)
    {
        int offset = ObjectSignatureSize;
        while (offset < source.Length &&
               SmoDataBlockReader.TryReadHeader(source, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == 0 && field.PayloadSize >= 8 &&
                BinaryPrimitives.ReadUInt32LittleEndian(source[field.PayloadOffset..]) == 0 &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    source[(field.PayloadOffset + sizeof(uint))..]) == expectedBoneCount)
                return field;
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidDataException("The skin palette field was not found.");
    }

    private static void WriteResizedInlineField(
        Stream stream,
        ReadOnlySpan<byte> source,
        SmoDataBlockHeader template,
        BuiltObject child,
        ICollection<ObjectPlacement> placements)
    {
        byte[] header = source.Slice(template.Offset, template.HeaderSize).ToArray();
        PatchPayloadSize(
            header, 0, template, checked((uint)(ObjectReferenceSize + child.Data.Length)));
        stream.Write(header);
        WriteUInt32(stream, child.Root.Id);
        WriteUInt32(stream, checked((uint)child.Data.Length));
        int childOffset = checked((int)stream.Position);
        stream.Write(child.Data);
        foreach (ObjectPlacement placement in child.Placements)
            placements.Add(placement with
            {
                RelativeOffset = checked(childOffset + placement.RelativeOffset)
            });
    }

    private static void WriteReferenceOnlyField(
        Stream stream,
        ReadOnlySpan<byte> source,
        SmoDataBlockHeader template,
        uint objectId)
    {
        byte[] header = source.Slice(template.Offset, template.HeaderSize).ToArray();
        PatchPayloadSize(header, 0, template, ObjectReferenceSize);
        stream.Write(header);
        WriteUInt32(stream, objectId);
        WriteUInt32(stream, 0);
    }

    private static void PatchPayloadSize(
        Span<byte> header,
        int headerOffset,
        SmoDataBlockHeader template,
        uint payloadSize)
    {
        int sizeOffset = headerOffset + template.HeaderSize;
        switch (template.SizeKind)
        {
            case SmoDataBlockSizeCode.UInt8:
                header[sizeOffset - 1] = checked((byte)payloadSize);
                break;
            case SmoDataBlockSizeCode.UInt16:
                BinaryPrimitives.WriteUInt16LittleEndian(
                    header[(sizeOffset - sizeof(ushort))..], checked((ushort)payloadSize));
                break;
            case SmoDataBlockSizeCode.UInt32:
                BinaryPrimitives.WriteUInt32LittleEndian(
                    header[(sizeOffset - sizeof(uint))..], payloadSize);
                break;
            default:
                throw new InvalidDataException(
                    $"Field type {template.FieldType} has no writable variable-size header.");
        }
    }

    private static ReadOnlySpan<byte> ObjectBytes(
        SmoDocument document,
        SmoObjectEntry entry) =>
        document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));

    private static bool HasExactUInt32Field(
        SmoDocument document,
        SmoObjectEntry entry,
        byte fieldType,
        IReadOnlyList<uint> expected)
    {
        ReadOnlySpan<byte> source = ObjectBytes(document, entry);
        int offset = ObjectSignatureSize;
        while (offset < source.Length &&
               SmoDataBlockReader.TryReadHeader(
                   source, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == fieldType &&
                field.PayloadSize == expected.Count * sizeof(uint))
            {
                for (int index = 0; index < expected.Count; index++)
                {
                    uint actual = BinaryPrimitives.ReadUInt32LittleEndian(
                        source[(field.PayloadOffset + index * sizeof(uint))..]);
                    if (actual != expected[index])
                        return false;
                }
                return true;
            }
            offset = checked((int)field.PayloadEnd);
        }
        return false;
    }

    private static byte[] RawName(string value)
    {
        byte[] encoded = Encoding.UTF8.GetBytes(value);
        if (encoded.Length > 31)
        {
            throw new InvalidOperationException(
                $"Generated SMO name \"{value}\" exceeds the 31-byte safe catalog limit.");
        }
        byte[] result = new byte[encoded.Length + 1];
        encoded.CopyTo(result, 0);
        return result;
    }

    private static void WriteMatrix(Span<byte> data, Matrix4x4 matrix)
    {
        float[] values =
        [
            matrix.M11, matrix.M12, matrix.M13, matrix.M14,
            matrix.M21, matrix.M22, matrix.M23, matrix.M24,
            matrix.M31, matrix.M32, matrix.M33, matrix.M34,
            matrix.M41, matrix.M42, matrix.M43, matrix.M44
        ];
        for (int index = 0; index < values.Length; index++)
            WriteSingle(data, index * sizeof(float), values[index]);
    }

    private static void WriteVector2(Span<byte> data, int offset, Vector2 value)
    {
        WriteSingle(data, offset, value.X);
        WriteSingle(data, offset + sizeof(float), value.Y);
    }

    private static void WriteVector3(Span<byte> data, int offset, Vector3 value)
    {
        WriteSingle(data, offset, value.X);
        WriteSingle(data, offset + sizeof(float), value.Y);
        WriteSingle(data, offset + 2 * sizeof(float), value.Z);
    }

    private static void WriteVector4(Span<byte> data, int offset, Vector4 value)
    {
        WriteSingle(data, offset, value.X);
        WriteSingle(data, offset + sizeof(float), value.Y);
        WriteSingle(data, offset + 2 * sizeof(float), value.Z);
        WriteSingle(data, offset + 3 * sizeof(float), value.W);
    }

    private static void WriteSingle(Span<byte> data, int offset, float value) =>
        BinaryPrimitives.WriteInt32LittleEndian(
            data[offset..], BitConverter.SingleToInt32Bits(value));

    private static void WriteUInt16(Span<byte> data, int offset, ushort value) =>
        BinaryPrimitives.WriteUInt16LittleEndian(data[offset..], value);

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);

    private static void WriteUInt32(Stream stream, uint value)
    {
        Span<byte> data = stackalloc byte[sizeof(uint)];
        BinaryPrimitives.WriteUInt32LittleEndian(data, value);
        stream.Write(data);
    }

    private static bool ApproximatelyEqual(
        Matrix4x4 left,
        Matrix4x4 right,
        float epsilon)
    {
        float[] differences =
        [
            left.M11 - right.M11, left.M12 - right.M12,
            left.M13 - right.M13, left.M14 - right.M14,
            left.M21 - right.M21, left.M22 - right.M22,
            left.M23 - right.M23, left.M24 - right.M24,
            left.M31 - right.M31, left.M32 - right.M32,
            left.M33 - right.M33, left.M34 - right.M34,
            left.M41 - right.M41, left.M42 - right.M42,
            left.M43 - right.M43, left.M44 - right.M44
        ];
        return differences.All(value => MathF.Abs(value) <= epsilon);
    }

    private sealed record TrianglePlan(
        SmoSkinnedBranchSourceMesh Mesh,
        int TriangleOrdinal,
        SmoSkinnedRenderableMaterialFamily MaterialFamily,
        HashSet<string> Bones);

    private sealed class PalettePlan
    {
        public PalettePlan(
            int ordinal,
            int sourceMeshKey,
            bool startsRenderable,
            SmoSkinnedRenderableMaterialFamily materialFamily,
            IEnumerable<string> bones)
        {
            Ordinal = ordinal;
            SourceMeshKey = sourceMeshKey;
            StartsRenderable = startsRenderable;
            MaterialFamily = materialFamily;
            Bones = new HashSet<string>(bones, StringComparer.Ordinal);
        }

        public int Ordinal { get; }
        public int SourceMeshKey { get; }
        public bool StartsRenderable { get; }
        public SmoSkinnedRenderableMaterialFamily MaterialFamily { get; }
        public HashSet<string> Bones { get; }
        public List<TrianglePlan> Triangles { get; } = [];
        public string[] PaletteNames => Bones.Order(StringComparer.Ordinal).ToArray();
    }

    private sealed record ObjectPlacement(
        uint Id,
        byte[] RawName,
        uint TypeHash,
        int RelativeOffset,
        uint SerializedSize);

    private sealed record BuiltObject(
        byte[] Data,
        IReadOnlyList<ObjectPlacement> Placements,
        int VertexCount,
        int TriangleCount)
    {
        public ObjectPlacement Root => Placements[0];

        public static BuiltObject CreateRoot(
            uint id,
            byte[] rawName,
            uint typeHash,
            byte[] data) => new(
                data,
                [new ObjectPlacement(
                    id, rawName, typeHash, 0, checked((uint)data.Length))],
                0,
                0);
    }

    private readonly record struct NormalReferenceTriangle(
        int MeshKey,
        Vector3 First,
        Vector3 Second,
        Vector3 Third,
        Vector3 FirstNormal,
        Vector3 SecondNormal,
        Vector3 ThirdNormal)
    {
        public Vector3 Minimum => Vector3.Min(First, Vector3.Min(Second, Third));
        public Vector3 Maximum => Vector3.Max(First, Vector3.Max(Second, Third));
        public Vector3 Centroid => (First + Second + Third) / 3f;
    }

    private sealed class NormalReferenceSearch
    {
        private const int LeafSize = 8;
        private readonly NormalReferenceTriangle[] _triangles;
        private readonly Node _root;

        public NormalReferenceSearch(IEnumerable<NormalReferenceTriangle> triangles)
        {
            _triangles = triangles.ToArray();
            _root = Build(0, _triangles.Length);
        }

        public bool TryFind(
            Vector3 point,
            Vector3 sourceNormal,
            int excludedMeshKey,
            float maximumDistance,
            out Vector3 normal)
        {
            float bestDistanceSquared = maximumDistance * maximumDistance;
            float bestDot = float.NegativeInfinity;
            Vector3 bestNormal = Vector3.Zero;
            Find(
                _root,
                point,
                sourceNormal,
                excludedMeshKey,
                ref bestDistanceSquared,
                ref bestDot,
                ref bestNormal);
            normal = NormalizeOrZero(bestNormal);
            return normal != Vector3.Zero;
        }

        private Node Build(int start, int count)
        {
            Vector3 minimum = new(float.PositiveInfinity);
            Vector3 maximum = new(float.NegativeInfinity);
            Vector3 centroidMinimum = new(float.PositiveInfinity);
            Vector3 centroidMaximum = new(float.NegativeInfinity);
            for (int index = start; index < start + count; index++)
            {
                NormalReferenceTriangle triangle = _triangles[index];
                minimum = Vector3.Min(minimum, triangle.Minimum);
                maximum = Vector3.Max(maximum, triangle.Maximum);
                centroidMinimum = Vector3.Min(centroidMinimum, triangle.Centroid);
                centroidMaximum = Vector3.Max(centroidMaximum, triangle.Centroid);
            }
            if (count <= LeafSize)
                return new Node(minimum, maximum, start, count, null, null);

            Vector3 extent = centroidMaximum - centroidMinimum;
            int axis = extent.X >= extent.Y && extent.X >= extent.Z
                ? 0
                : extent.Y >= extent.Z ? 1 : 2;
            Array.Sort(
                _triangles,
                start,
                count,
                Comparer<NormalReferenceTriangle>.Create((left, right) =>
                    Axis(left.Centroid, axis).CompareTo(Axis(right.Centroid, axis))));
            int leftCount = count / 2;
            Node left = Build(start, leftCount);
            Node right = Build(start + leftCount, count - leftCount);
            return new Node(minimum, maximum, start, count, left, right);
        }

        private void Find(
            Node node,
            Vector3 point,
            Vector3 sourceNormal,
            int excludedMeshKey,
            ref float bestDistanceSquared,
            ref float bestDot,
            ref Vector3 bestNormal)
        {
            if (DistanceSquaredToBounds(point, node.Minimum, node.Maximum) >
                bestDistanceSquared)
            {
                return;
            }
            if (node.Left is null || node.Right is null)
            {
                for (int index = node.Start; index < node.Start + node.Count; index++)
                {
                    NormalReferenceTriangle triangle = _triangles[index];
                    if (triangle.MeshKey == excludedMeshKey)
                        continue;
                    ClosestPointBarycentric(
                        point,
                        triangle.First,
                        triangle.Second,
                        triangle.Third,
                        out Vector3 closest,
                        out Vector3 barycentric);
                    float distanceSquared = Vector3.DistanceSquared(point, closest);
                    if (distanceSquared > bestDistanceSquared)
                        continue;
                    Vector3 candidate =
                        triangle.FirstNormal * barycentric.X +
                        triangle.SecondNormal * barycentric.Y +
                        triangle.ThirdNormal * barycentric.Z;
                    candidate = NormalizeOrZero(candidate);
                    if (candidate == Vector3.Zero)
                        continue;
                    float dot = Vector3.Dot(sourceNormal, candidate);
                    if (dot < 0.45f)
                        continue;
                    float tieTolerance = MathF.Max(
                        0.000000000001f,
                        bestDistanceSquared * 0.00001f);
                    if (distanceSquared > bestDistanceSquared - tieTolerance &&
                        dot <= bestDot)
                    {
                        continue;
                    }
                    bestDistanceSquared = distanceSquared;
                    bestDot = dot;
                    bestNormal = candidate;
                }
                return;
            }

            float leftDistance = DistanceSquaredToBounds(
                point, node.Left.Minimum, node.Left.Maximum);
            float rightDistance = DistanceSquaredToBounds(
                point, node.Right.Minimum, node.Right.Maximum);
            if (leftDistance <= rightDistance)
            {
                Find(node.Left, point, sourceNormal, excludedMeshKey,
                    ref bestDistanceSquared, ref bestDot, ref bestNormal);
                Find(node.Right, point, sourceNormal, excludedMeshKey,
                    ref bestDistanceSquared, ref bestDot, ref bestNormal);
            }
            else
            {
                Find(node.Right, point, sourceNormal, excludedMeshKey,
                    ref bestDistanceSquared, ref bestDot, ref bestNormal);
                Find(node.Left, point, sourceNormal, excludedMeshKey,
                    ref bestDistanceSquared, ref bestDot, ref bestNormal);
            }
        }

        private static float Axis(Vector3 value, int axis) => axis switch
        {
            0 => value.X,
            1 => value.Y,
            _ => value.Z
        };

        private sealed record Node(
            Vector3 Minimum,
            Vector3 Maximum,
            int Start,
            int Count,
            Node? Left,
            Node? Right);
    }

    private static Vector3 NormalizeOrZero(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z) && value.LengthSquared() > 0.000000000001f
            ? Vector3.Normalize(value)
            : Vector3.Zero;

    private static float DistanceSquaredToBounds(
        Vector3 point,
        Vector3 minimum,
        Vector3 maximum)
    {
        Vector3 delta = Vector3.Max(
            Vector3.Zero,
            Vector3.Max(minimum - point, point - maximum));
        return delta.LengthSquared();
    }

    // Closest-point regions from Real-Time Collision Detection, with the
    // barycentric coordinates retained for interpolating the reference normal.
    private static void ClosestPointBarycentric(
        Vector3 point,
        Vector3 first,
        Vector3 second,
        Vector3 third,
        out Vector3 closest,
        out Vector3 barycentric)
    {
        Vector3 firstSecond = second - first;
        Vector3 firstThird = third - first;
        Vector3 firstPoint = point - first;
        float d1 = Vector3.Dot(firstSecond, firstPoint);
        float d2 = Vector3.Dot(firstThird, firstPoint);
        if (d1 <= 0 && d2 <= 0)
        {
            closest = first;
            barycentric = new Vector3(1, 0, 0);
            return;
        }

        Vector3 secondPoint = point - second;
        float d3 = Vector3.Dot(firstSecond, secondPoint);
        float d4 = Vector3.Dot(firstThird, secondPoint);
        if (d3 >= 0 && d4 <= d3)
        {
            closest = second;
            barycentric = new Vector3(0, 1, 0);
            return;
        }

        float vc = d1 * d4 - d3 * d2;
        if (vc <= 0 && d1 >= 0 && d3 <= 0)
        {
            float v = d1 / (d1 - d3);
            closest = first + firstSecond * v;
            barycentric = new Vector3(1 - v, v, 0);
            return;
        }

        Vector3 thirdPoint = point - third;
        float d5 = Vector3.Dot(firstSecond, thirdPoint);
        float d6 = Vector3.Dot(firstThird, thirdPoint);
        if (d6 >= 0 && d5 <= d6)
        {
            closest = third;
            barycentric = new Vector3(0, 0, 1);
            return;
        }

        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0 && d2 >= 0 && d6 <= 0)
        {
            float w = d2 / (d2 - d6);
            closest = first + firstThird * w;
            barycentric = new Vector3(1 - w, 0, w);
            return;
        }

        float va = d3 * d6 - d5 * d4;
        if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0)
        {
            float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            closest = second + (third - second) * w;
            barycentric = new Vector3(0, 1 - w, w);
            return;
        }

        float denominator = va + vb + vc;
        if (!float.IsFinite(denominator) || MathF.Abs(denominator) <= 0.000000000001f)
        {
            closest = first;
            barycentric = new Vector3(1, 0, 0);
            return;
        }
        float inverse = 1 / denominator;
        float insideV = vb * inverse;
        float insideW = vc * inverse;
        closest = first + firstSecond * insideV + firstThird * insideW;
        barycentric = new Vector3(1 - insideV - insideW, insideV, insideW);
    }

    private sealed class UnionFind
    {
        private readonly int[] _parents;
        private readonly byte[] _ranks;

        public UnionFind(int count)
        {
            _parents = Enumerable.Range(0, count).ToArray();
            _ranks = new byte[count];
        }

        public int Find(int value)
        {
            int root = value;
            while (_parents[root] != root)
                root = _parents[root];
            while (_parents[value] != value)
            {
                int parent = _parents[value];
                _parents[value] = root;
                value = parent;
            }
            return root;
        }

        public void Union(int left, int right)
        {
            int leftRoot = Find(left);
            int rightRoot = Find(right);
            if (leftRoot == rightRoot)
                return;
            if (_ranks[leftRoot] < _ranks[rightRoot])
            {
                _parents[leftRoot] = rightRoot;
            }
            else if (_ranks[leftRoot] > _ranks[rightRoot])
            {
                _parents[rightRoot] = leftRoot;
            }
            else
            {
                _parents[rightRoot] = leftRoot;
                _ranks[leftRoot]++;
            }
        }
    }

    private sealed class ObjectIdAllocator
    {
        private uint _next;

        public ObjectIdAllocator(IEnumerable<uint> ids) =>
            _next = checked(ids.DefaultIfEmpty().Max() + 1);

        public uint Take()
        {
            uint result = _next;
            _next = checked(_next + 1);
            return result;
        }
    }

    private sealed record BuildContext(
        SmoObjectEntry Render,
        SmoObjectEntry PrimarySkin,
        SmoObjectEntry ContinuationSkin,
        SmoObjectEntry OpaqueMaterial,
        SmoObjectEntry Texture,
        SmoObjectEntry Helper,
        SmoObjectEntry MeshTemplate,
        IReadOnlyDictionary<string, SmoObjectEntry> TargetNodes)
    {
        public static BuildContext Create(SmoDocument target, uint textureObjectId)
        {
            ArgumentNullException.ThrowIfNull(target);
            SmoObjectEntry texture = target.Objects.SingleOrDefault(entry =>
                entry.Id == textureObjectId && entry.TypeHash == SmoClassIds.TextureData) ??
                throw new NotSupportedException(
                    $"Target texture ID {textureObjectId} is absent or ambiguous.");
            if (texture.ParentIndex is not int materialIndex ||
                target.Objects[materialIndex].TypeHash != SmoClassIds.MaterialData)
                throw new NotSupportedException(
                    $"Target texture [{texture.Index}] is not inline under a material.");
            SmoObjectEntry material = target.Objects[materialIndex];
            if (material.ParentIndex is not int skinIndex ||
                target.Objects[skinIndex].TypeHash != SmoClassIds.Skin)
                throw new NotSupportedException(
                    $"Target material [{material.Index}] is not inline under an spSkin.");
            SmoObjectEntry primarySkin = target.Objects[skinIndex];
            if (primarySkin.ParentIndex is not int renderIndex ||
                target.Objects[renderIndex].TypeHash != SmoClassIds.RenderNode)
                throw new NotSupportedException(
                    $"Target skin [{primarySkin.Index}] has no direct spRenderNode parent.");
            SmoObjectEntry render = target.Objects[renderIndex];
            SmoObjectEntry helper = target.Objects.SingleOrDefault(entry =>
                entry.ParentIndex == primarySkin.Index &&
                entry.TypeHash == SharedFogClassId) ??
                throw new NotSupportedException(
                    "Primary Bloom skin has no native shared visual helper template.");
            SmoObjectEntry mesh = target.Objects.SingleOrDefault(entry =>
                entry.ParentIndex == primarySkin.Index &&
                entry.TypeHash == SmoClassIds.MeshData) ??
                throw new NotSupportedException(
                    "Primary Bloom skin has no unique mesh template.");
            SmoObjectEntry continuation = target.Objects
                .Where(entry => entry.ParentIndex == render.Index &&
                                entry.TypeHash == SmoClassIds.Skin &&
                                entry.Index != primarySkin.Index)
                .Where(entry => target.Objects.Any(child =>
                    child.ParentIndex == entry.Index &&
                    child.TypeHash == SmoClassIds.MeshData))
                .Where(entry => !target.Objects.Any(child =>
                    child.ParentIndex == entry.Index &&
                    child.TypeHash == SmoClassIds.MaterialData))
                .OrderBy(entry => entry.LogicalOffset)
                .FirstOrDefault() ?? throw new NotSupportedException(
                    "Target has no native material-less Bloom continuation spSkin template.");

            if (!SmoSkinDecoder.TryDecode(
                    target, primarySkin, out SmoSkin? primaryPalette, out string primaryError) ||
                primaryPalette is null || primaryPalette.Bones.Count != PaletteCapacity)
                throw new NotSupportedException(
                    $"Primary Bloom skin is not a 16-bone native template: {primaryError}");
            if (!SmoSkinDecoder.TryDecode(
                    target, continuation, out SmoSkin? continuationPalette,
                    out string continuationError) ||
                continuationPalette is null ||
                continuationPalette.Bones.Count != PaletteCapacity)
                throw new NotSupportedException(
                    $"Bloom continuation skin is not a 16-bone native template: " +
                    continuationError);

            ValidateMaterialTemplate(target, material, texture);
            ValidateSkinTemplate(target, primarySkin, material, helper, mesh);
            SmoObjectEntry continuationMesh = target.Objects.Single(entry =>
                entry.ParentIndex == continuation.Index &&
                entry.TypeHash == SmoClassIds.MeshData);
            ValidateSkinTemplate(
                target, continuation, null, helper, continuationMesh);

            Dictionary<string, SmoObjectEntry> nodes = target.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.Node &&
                                !string.IsNullOrWhiteSpace(entry.Name))
                .GroupBy(entry => entry.Name, StringComparer.Ordinal)
                .Where(group => group.Count() == 1)
                .ToDictionary(
                    group => group.Key, group => group.Single(), StringComparer.Ordinal);
            return new BuildContext(
                render,
                primarySkin,
                continuation,
                material,
                texture,
                helper,
                mesh,
                nodes);
        }

        private static void ValidateMaterialTemplate(
            SmoDocument target,
            SmoObjectEntry material,
            SmoObjectEntry texture)
        {
            ReadOnlySpan<byte> source = ObjectBytes(target, material);
            _ = FindInlineChildField(target, material, texture);
            int stateFieldCount = 0;
            int operationFieldCount = 0;
            int lightingTextureStateFieldCount = 0;
            int offset = ObjectSignatureSize;
            while (offset < source.Length &&
                   SmoDataBlockReader.TryReadHeader(
                       source, offset, out SmoDataBlockHeader field))
            {
                if (field.FieldType == 0 &&
                    field.PayloadSize ==
                    SkinnedTransparentSurfaceMaterialRenderStates.Length * sizeof(uint))
                {
                    stateFieldCount++;
                }
                if (field.FieldType == 3 && field.PayloadSize == sizeof(uint))
                    operationFieldCount++;
                if (field.FieldType == 17 &&
                    field.PayloadSize ==
                    SkinnedTransparentSurfaceLightingTextureStates.Length * sizeof(uint))
                {
                    lightingTextureStateFieldCount++;
                }
                offset = checked((int)field.PayloadEnd);
            }
            if (offset != source.Length || stateFieldCount != 1 ||
                operationFieldCount != 1 || lightingTextureStateFieldCount != 1)
                throw new NotSupportedException(
                    "Target material does not expose exactly one native writable " +
                    "Bloom MRS, FinalBlendOp, and LTS field.");
        }

        private static void ValidateSkinTemplate(
            SmoDocument target,
            SmoObjectEntry skin,
            SmoObjectEntry? material,
            SmoObjectEntry helper,
            SmoObjectEntry mesh)
        {
            ReadOnlySpan<byte> source = ObjectBytes(target, skin);
            if (material is not null)
                _ = FindInlineChildField(target, skin, material);
            _ = FindInlineChildField(target, skin, mesh);
            _ = FindObjectReferenceField(source, helper.Id);
            _ = FindPaletteField(source, PaletteCapacity);
            int sortFieldCount = 0;
            int priorityFieldCount = 0;
            int offset = ObjectSignatureSize;
            while (offset < source.Length &&
                   SmoDataBlockReader.TryReadHeader(
                       source, offset, out SmoDataBlockHeader field))
            {
                if (field.FieldType == 2 && field.PayloadSize == sizeof(uint))
                    sortFieldCount++;
                if (field.FieldType == 3 && field.PayloadSize == sizeof(uint))
                    priorityFieldCount++;
                offset = checked((int)field.PayloadEnd);
            }
            if (offset != source.Length || sortFieldCount != 1 ||
                priorityFieldCount != 1)
                throw new NotSupportedException(
                    $"Skin template [{skin.Index}] does not expose exactly one " +
                    "writable sort and priority field.");
        }
    }
}
