using System.Numerics;

namespace SmoViewer.Core;

/// <summary>
/// Structural evidence about one confirmed princess-style alpha draw unit.
/// This deliberately reports geometry/run granularity only; it does not try
/// to reproduce the native renderer's ordering policy.
/// </summary>
public sealed record SmoAlphaRunGranularityInfo(
    bool IsApplicable,
    int GeometryComponentCount,
    int SignificantComponentCount,
    float BoundsDiagonal,
    float MaximumComponentGap,
    float MaximumComponentGapFraction,
    float MaximumGapToMedianComponentSize,
    int ActiveDeformTargetCount,
    bool IsGeneratedImporterMesh,
    bool HasSuspiciousCombinedRun,
    string? Diagnostic);

/// <summary>
/// Conservatively detects a likely aggregation of spatially unrelated alpha
/// overlays inside one skinned material/mesh run. A warning is emitted only
/// for the exactly confirmed princess transparent-surface state.
/// </summary>
public static class SmoAlphaRunGranularityAnalyzer
{
    private const int MinimumSignificantTriangleCount = 2;
    private const float GeneratedMinimumGapFraction = 0.20f;
    private const float NativeMinimumGapFraction = 0.35f;
    private const float MinimumGapToMedianComponentSize = 2.0f;

    public static SmoAlphaRunGranularityInfo Analyze(
        SmoMesh mesh,
        SmoMaterialRenderStateInfo renderState,
        SmoSkin? consumingSkin = null)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        ArgumentNullException.ThrowIfNull(renderState);

        bool applicable =
            renderState.HasConfirmedPrincessTransparentSurfaceState &&
            mesh.HasSkinningData;
        if (!applicable)
            return Empty(isApplicable: false, mesh.Name);

        ComponentBounds[] components = FindComponents(mesh);
        ComponentBounds[] significant = components
            .Where(component =>
                component.TriangleCount >= MinimumSignificantTriangleCount)
            .ToArray();
        if (significant.Length == 0)
            significant = components;

        Bounds3 meshBounds = Bounds3.FromPositions(mesh.Positions);
        float boundsDiagonal = meshBounds.Diagonal;
        float maximumGap = 0.0f;
        for (int left = 0; left < significant.Length; left++)
        {
            for (int right = left + 1; right < significant.Length; right++)
            {
                maximumGap = MathF.Max(
                    maximumGap,
                    significant[left].Bounds.DistanceTo(
                        significant[right].Bounds));
            }
        }

        float[] componentSizes = significant
            .Select(component => component.Bounds.Diagonal)
            .Where(size => float.IsFinite(size) && size > 0.000001f)
            .Order()
            .ToArray();
        float medianSize = componentSizes.Length == 0
            ? 0.0f
            : componentSizes[componentSizes.Length / 2];
        float gapFraction = boundsDiagonal > 0.000001f
            ? maximumGap / boundsDiagonal
            : 0.0f;
        float gapToMedianSize = medianSize > 0.000001f
            ? maximumGap / medianSize
            : 0.0f;
        int activeDeformTargetCount = CountActiveDeformTargets(
            mesh, consumingSkin);
        bool generated = IsGeneratedImporterMesh(mesh.Name);

        // Two disconnected pieces are common for symmetric authored details
        // such as eyes or wings. Requiring at least three significant islands,
        // plus a large empty spatial gap, keeps this a conservative aggregation
        // warning rather than a generic disconnected-mesh warning.
        float minimumGapFraction = generated
            ? GeneratedMinimumGapFraction
            : NativeMinimumGapFraction;
        int minimumComponentCount = generated ? 3 : 4;
        bool hasIndependentBindingEvidence = activeDeformTargetCount >= 2;
        bool hasExceptionalFragmentation = significant.Length >= 8;
        bool suspicious =
            significant.Length >= minimumComponentCount &&
            gapFraction >= minimumGapFraction &&
            gapToMedianSize >= MinimumGapToMedianComponentSize &&
            (hasIndependentBindingEvidence || hasExceptionalFragmentation);

        string? diagnostic = suspicious
            ? "NATIVE_ALPHA_RUN_GRANULARITY_UNCONFIRMED: this confirmed " +
              "princess-style skinned alpha draw unit contains " +
              $"{significant.Length} disconnected significant geometry " +
              $"components (maximum empty gap {maximumGap:G4}, " +
              $"{gapFraction:P0} of run bounds; " +
              $"{activeDeformTargetCount} active deform targets). The isolated " +
              "OpenGL preview does not " +
              "confirm native alpha sorting. If unrelated decals or overlays " +
              "were combined into this one material/mesh run, the game may " +
              "draw some of them behind opaque geometry; preserve ordered " +
              "source run boundaries and validate the result in game. This " +
              "is a structural warning, not proof of a rendering cause."
            : null;

        return new SmoAlphaRunGranularityInfo(
            true,
            components.Length,
            significant.Length,
            boundsDiagonal,
            maximumGap,
            gapFraction,
            gapToMedianSize,
            activeDeformTargetCount,
            generated,
            suspicious,
            diagnostic);
    }

    private static SmoAlphaRunGranularityInfo Empty(
        bool isApplicable,
        string meshName) =>
        new(
            isApplicable,
            0,
            0,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            0,
            IsGeneratedImporterMesh(meshName),
            false,
            null);

    private static bool IsGeneratedImporterMesh(string name) =>
        name.StartsWith("imp_a_x_", StringComparison.OrdinalIgnoreCase);

    private static int CountActiveDeformTargets(
        SmoMesh mesh,
        SmoSkin? consumingSkin)
    {
        var activePaletteIndices = new HashSet<int>();
        for (int index = 0; index < mesh.VertexCount; index++)
        {
            Vector4 weights = mesh.BlendWeights[index];
            SmoBlendIndices indices = mesh.BlendIndices[index];
            if (weights.X > 0.000001f)
                activePaletteIndices.Add(indices.X);
            if (weights.Y > 0.000001f)
                activePaletteIndices.Add(indices.Y);
            if (weights.Z > 0.000001f)
                activePaletteIndices.Add(indices.Z);
            if (weights.W > 0.000001f)
                activePaletteIndices.Add(indices.W);
        }

        if (consumingSkin is null)
            return activePaletteIndices.Count;

        Dictionary<int, int> targetsByPalette = consumingSkin.Bones
            .GroupBy(bone => bone.PaletteIndex)
            .ToDictionary(
                group => group.Key,
                group => group.First().NodeObjectIndex);
        return activePaletteIndices
            .Where(targetsByPalette.ContainsKey)
            .Select(paletteIndex => targetsByPalette[paletteIndex])
            .Distinct()
            .Count();
    }

    private static ComponentBounds[] FindComponents(SmoMesh mesh)
    {
        int vertexCount = mesh.VertexCount;
        if (vertexCount == 0 || mesh.TriangleIndices.Length < 3)
            return [];

        var union = new UnionFind(vertexCount);
        bool[] used = new bool[vertexCount];
        for (int offset = 0; offset + 2 < mesh.TriangleIndices.Length; offset += 3)
        {
            int a = checked((int)mesh.TriangleIndices[offset]);
            int b = checked((int)mesh.TriangleIndices[offset + 1]);
            int c = checked((int)mesh.TriangleIndices[offset + 2]);
            if ((uint)a >= (uint)vertexCount ||
                (uint)b >= (uint)vertexCount ||
                (uint)c >= (uint)vertexCount)
            {
                continue;
            }

            used[a] = true;
            used[b] = true;
            used[c] = true;
            union.Join(a, b);
            union.Join(a, c);
        }

        // UV seams sometimes duplicate a position without changing the actual
        // surface connectivity. Join exact serialized positions before
        // counting islands so those seams do not create false components.
        var firstAtPosition = new Dictionary<Vector3, int>();
        for (int index = 0; index < vertexCount; index++)
        {
            if (!used[index] || !IsFinite(mesh.Positions[index]))
                continue;
            if (firstAtPosition.TryGetValue(mesh.Positions[index], out int first))
                union.Join(first, index);
            else
                firstAtPosition.Add(mesh.Positions[index], index);
        }

        var byRoot = new Dictionary<int, ComponentAccumulator>();
        for (int index = 0; index < vertexCount; index++)
        {
            if (!used[index] || !IsFinite(mesh.Positions[index]))
                continue;
            int root = union.Find(index);
            if (!byRoot.TryGetValue(root, out ComponentAccumulator? component))
            {
                component = new ComponentAccumulator();
                byRoot.Add(root, component);
            }
            component.AddVertex(mesh.Positions[index]);
        }

        for (int offset = 0; offset + 2 < mesh.TriangleIndices.Length; offset += 3)
        {
            int first = checked((int)mesh.TriangleIndices[offset]);
            if ((uint)first >= (uint)vertexCount || !used[first])
                continue;
            int root = union.Find(first);
            if (byRoot.TryGetValue(root, out ComponentAccumulator? component))
                component.TriangleCount++;
        }

        return byRoot.Values
            .Where(component => component.VertexCount > 0)
            .Select(component => new ComponentBounds(
                component.TriangleCount,
                component.Bounds))
            .ToArray();
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) &&
        float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);

    private sealed class ComponentAccumulator
    {
        public int TriangleCount { get; set; }
        public int VertexCount { get; private set; }
        public Bounds3 Bounds { get; private set; } = Bounds3.Empty;

        public void AddVertex(Vector3 position)
        {
            Bounds = Bounds.Include(position);
            VertexCount++;
        }
    }

    private sealed class UnionFind
    {
        private readonly int[] _parent;
        private readonly byte[] _rank;

        public UnionFind(int count)
        {
            _parent = Enumerable.Range(0, count).ToArray();
            _rank = new byte[count];
        }

        public int Find(int item)
        {
            int root = item;
            while (_parent[root] != root)
                root = _parent[root];
            while (_parent[item] != item)
            {
                int next = _parent[item];
                _parent[item] = root;
                item = next;
            }
            return root;
        }

        public void Join(int left, int right)
        {
            int leftRoot = Find(left);
            int rightRoot = Find(right);
            if (leftRoot == rightRoot)
                return;
            if (_rank[leftRoot] < _rank[rightRoot])
                _parent[leftRoot] = rightRoot;
            else if (_rank[leftRoot] > _rank[rightRoot])
                _parent[rightRoot] = leftRoot;
            else
            {
                _parent[rightRoot] = leftRoot;
                _rank[leftRoot]++;
            }
        }
    }

    private readonly record struct ComponentBounds(
        int TriangleCount,
        Bounds3 Bounds);

    private readonly record struct Bounds3(Vector3 Minimum, Vector3 Maximum)
    {
        public static Bounds3 Empty => new(
            new Vector3(float.PositiveInfinity),
            new Vector3(float.NegativeInfinity));

        public float Diagonal => IsValid
            ? Vector3.Distance(Minimum, Maximum)
            : 0.0f;

        private bool IsValid =>
            IsFinite(Minimum) &&
            IsFinite(Maximum) &&
            Minimum.X <= Maximum.X &&
            Minimum.Y <= Maximum.Y &&
            Minimum.Z <= Maximum.Z;

        public Bounds3 Include(Vector3 position) => !IsValid
            ? new Bounds3(position, position)
            : new Bounds3(
                Vector3.Min(Minimum, position),
                Vector3.Max(Maximum, position));

        public float DistanceTo(Bounds3 other)
        {
            if (!IsValid || !other.IsValid)
                return 0.0f;
            Vector3 gap = new(
                AxisGap(Minimum.X, Maximum.X, other.Minimum.X, other.Maximum.X),
                AxisGap(Minimum.Y, Maximum.Y, other.Minimum.Y, other.Maximum.Y),
                AxisGap(Minimum.Z, Maximum.Z, other.Minimum.Z, other.Maximum.Z));
            return gap.Length();
        }

        public static Bounds3 FromPositions(IEnumerable<Vector3> positions)
        {
            Bounds3 bounds = Empty;
            foreach (Vector3 position in positions)
            {
                if (IsFinite(position))
                    bounds = bounds.Include(position);
            }
            return bounds;
        }

        private static float AxisGap(
            float firstMinimum,
            float firstMaximum,
            float secondMinimum,
            float secondMaximum)
        {
            if (firstMaximum < secondMinimum)
                return secondMinimum - firstMaximum;
            if (secondMaximum < firstMinimum)
                return firstMinimum - secondMaximum;
            return 0.0f;
        }
    }
}
