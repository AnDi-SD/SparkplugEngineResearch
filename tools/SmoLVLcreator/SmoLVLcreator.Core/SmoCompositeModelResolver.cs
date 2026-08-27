using System.Collections.ObjectModel;
using System.Numerics;
using SmoViewer.Core;
using SmoViewer.Scene;

namespace SmoLVLcreator.Core;

/// <summary>
/// Reconstructs authoring models which SMO stores as several render entities.
/// The resolver uses graph structure, authored names and world-space geometry;
/// it deliberately contains no vocabulary of object kinds.
/// </summary>
internal static class SmoCompositeModelResolver
{
    public static IReadOnlyList<SmoCompositeModel> Resolve(
        SmoLevelWorkspace workspace,
        IEnumerable<SmoLevelEntity> sourceEntities)
    {
        ArgumentNullException.ThrowIfNull(workspace);
        ArgumentNullException.ThrowIfNull(sourceEntities);

        SmoLevelEntity[] entities = sourceEntities
            .Where(entity => entity.Kind == SmoLevelEntityKind.Visual)
            .OrderBy(entity => entity.Id.SceneObjectIndex)
            .ToArray();
        if (entities.Length < 2)
            return Array.Empty<SmoCompositeModel>();

        Dictionary<(int Mesh, int Scene), SmoSceneMesh> sceneMeshes = workspace
            .PreparedScene.Meshes
            .GroupBy(mesh => (mesh.Mesh.ObjectIndex, mesh.SceneObjectIndex))
            .ToDictionary(group => group.Key, group => group.First());
        ModelAtom[] atoms = entities.Select((entity, index) =>
            CreateAtom(workspace, sceneMeshes, entity, index)).ToArray();
        var sets = new DisjointSets(atoms.Length);

        SeedNumberedStaticObjects(workspace, atoms, sets);
        SeedNumberedPartitionModels(atoms, sets);
        InferAssemblies(atoms, sets);

        SmoCompositeModel[] result = atoms
            .Select((atom, index) => (Atom: atom, Root: sets.Find(index)))
            .GroupBy(item => item.Root, item => item.Atom)
            .Where(group => group.Count() > 1)
            .Select(group => CreateComposite(workspace, group.ToArray()))
            .OrderBy(model => model.Entities.Min(entity => entity.Id.SceneObjectIndex))
            .ToArray();
        return new ReadOnlyCollection<SmoCompositeModel>(result);
    }

    private static ModelAtom CreateAtom(
        SmoLevelWorkspace workspace,
        IReadOnlyDictionary<(int Mesh, int Scene), SmoSceneMesh> sceneMeshes,
        SmoLevelEntity entity,
        int index)
    {
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var renderableIndices = new HashSet<int>();
        var sourceIndices = new HashSet<int> { entity.Id.SceneObjectIndex };
        var bounds = ModelBounds.Empty;

        foreach (SmoEditablePlacement part in entity.Parts)
        {
            sourceIndices.Add(part.Source.SceneObjectIndex);
            if (part.Source.ModelObjectIndex is int modelIndex &&
                (uint)modelIndex < (uint)workspace.Document.Objects.Count)
            {
                SmoObjectEntry model = workspace.Document.Objects[modelIndex];
                string authoredName = StripNumberedPartSuffix(
                    model.Name.TrimEnd('\0'), out _);
                if (!string.IsNullOrWhiteSpace(authoredName))
                    names.Add(authoredName);
                int? renderable = FindAncestorOfType(
                    workspace.Document.Objects,
                    model,
                    SmoClassIds.PartitionRenderable);
                if (renderable is int renderableIndex)
                    renderableIndices.Add(renderableIndex);
            }
            else if (!string.IsNullOrWhiteSpace(part.Asset.DisplayName))
            {
                names.Add(StripNumberedPartSuffix(part.Asset.DisplayName, out _));
            }

            if (sceneMeshes.TryGetValue(
                    (part.Asset.ObjectIndex, part.Source.SceneObjectIndex),
                    out SmoSceneMesh? sceneMesh))
            {
                bounds = bounds.Include(sceneMesh);
            }
        }

        if (names.Count == 0 && !string.IsNullOrWhiteSpace(entity.Name))
            names.Add(StripNumberedPartSuffix(entity.Name, out _));
        return new ModelAtom(
            index,
            entity,
            names.ToArray(),
            renderableIndices.ToArray(),
            sourceIndices.ToArray(),
            bounds);
    }

    private static void SeedNumberedStaticObjects(
        SmoLevelWorkspace workspace,
        IReadOnlyList<ModelAtom> atoms,
        DisjointSets sets)
    {
        var candidates = new List<StaticCandidate>();
        foreach (ModelAtom atom in atoms.Where(atom => atom.Entity.Parts.Count == 1))
        {
            int ownerIndex = atom.Entity.Id.SceneObjectIndex;
            if ((uint)ownerIndex >= (uint)workspace.Document.Objects.Count)
                continue;
            SmoObjectEntry owner = workspace.Document.Objects[ownerIndex];
            string ownerName = owner.Name.TrimEnd('\0');
            if (owner.TypeHash != SmoClassIds.StaticRenderObject ||
                owner.ParentIndex is not int parentIndex ||
                string.IsNullOrWhiteSpace(ownerName) ||
                atom.Entity.Parts[0].Source.ModelObjectIndex is not int modelIndex ||
                (uint)modelIndex >= (uint)workspace.Document.Objects.Count)
            {
                continue;
            }

            string modelName = workspace.Document.Objects[modelIndex].Name.TrimEnd('\0');
            if (!TryReadOwnedComponent(ownerName, modelName, out int componentIndex))
                continue;
            candidates.Add(new StaticCandidate(
                atom.Index,
                ownerName,
                parentIndex,
                atom.Entity.WorldTransform,
                componentIndex));
        }

        foreach (IGrouping<StaticKey, StaticCandidate> group in candidates.GroupBy(
                     candidate => new StaticKey(
                         candidate.ParentIndex,
                         candidate.OwnerName.ToUpperInvariant(),
                         candidate.WorldTransform)))
        {
            StaticCandidate[] parts = group.ToArray();
            if (parts.Length < 2 ||
                parts.Select(part => part.ComponentIndex).Distinct().Count() != parts.Length)
            {
                continue;
            }
            UnionAll(sets, parts.Select(part => part.AtomIndex));
        }
    }

    private static void SeedNumberedPartitionModels(
        IReadOnlyList<ModelAtom> atoms,
        DisjointSets sets)
    {
        // ModelAtom already exposes suffix-free authored names. Grouping them
        // under one partition renderable is the strongest baked-model signal.
        foreach (IGrouping<(int Renderable, string Name), PartitionCandidate> group in
                 atoms.Where(atom => atom.PartitionRenderableIndices.Length == 1)
                     .SelectMany(atom => atom.Names.Select(name => new PartitionCandidate(
                         atom.Index,
                         atom.PartitionRenderableIndices[0],
                         name)))
                     .GroupBy(candidate => (
                         candidate.RenderableIndex,
                         candidate.AuthoredName.ToUpperInvariant())))
        {
            int[] indices = group.Select(candidate => candidate.AtomIndex)
                .Distinct().ToArray();
            if (indices.Length > 1)
                UnionAll(sets, indices);
        }
    }

    private static void InferAssemblies(
        IReadOnlyList<ModelAtom> atoms,
        DisjointSets sets)
    {
        // Two passes are normally sufficient (base + accessory); four keep
        // deeper authoring assemblies possible without turning level loading
        // into a repeated all-pairs operation.
        for (int pass = 0; pass < 4; pass++)
        {
            ModelCluster[] clusters = BuildClusters(atoms, sets);
            var candidates = new List<AssemblyCandidate>();
            for (int leftIndex = 0; leftIndex < clusters.Length; leftIndex++)
            {
                for (int rightIndex = leftIndex + 1;
                     rightIndex < clusters.Length;
                     rightIndex++)
                {
                    AssemblyCandidate? candidate = ScoreAssembly(
                        clusters[leftIndex],
                        clusters[rightIndex]);
                    if (candidate is not null)
                        candidates.Add(candidate);
                }
            }
            if (candidates.Count == 0)
                return;

            Dictionary<int, AssemblyCandidate> best = candidates
                .SelectMany(candidate => new[]
                {
                    (Root: candidate.Left.Root, Candidate: candidate),
                    (Root: candidate.Right.Root, Candidate: candidate)
                })
                .GroupBy(item => item.Root)
                .ToDictionary(
                    group => group.Key,
                    group => group.Select(item => item.Candidate)
                        .OrderByDescending(candidate => candidate.Score)
                        .ThenByDescending(candidate => candidate.GeometryAffinity)
                        .First());

            AssemblyCandidate[] mutualBest = candidates
                .Where(candidate =>
                    ReferenceEquals(best[candidate.Left.Root], candidate) &&
                    ReferenceEquals(best[candidate.Right.Root], candidate))
                .OrderByDescending(candidate => candidate.Score)
                .ThenByDescending(candidate => candidate.GeometryAffinity)
                .ToArray();
            if (mutualBest.Length == 0)
                return;
            foreach (AssemblyCandidate candidate in mutualBest)
                sets.Union(candidate.Left.Root, candidate.Right.Root);
        }
    }

    private static AssemblyCandidate? ScoreAssembly(
        ModelCluster left,
        ModelCluster right)
    {
        bool sameRenderable = left.PartitionRenderableIndices
            .Intersect(right.PartitionRenderableIndices).Any();
        bool crossRepresentation =
            (left.HasBakedGeometry && right.HasPlacedGeometry) ||
            (left.HasPlacedGeometry && right.HasBakedGeometry);
        bool separateBakedRepresentations =
            left.HasBakedGeometry && !left.HasPlacedGeometry &&
            right.HasBakedGeometry && !right.HasPlacedGeometry;
        bool strictSpatialPair =
            !sameRenderable && !crossRepresentation && !separateBakedRepresentations;

        float nameAffinity = left.Names.SelectMany(leftName =>
                right.Names.Select(rightName =>
                    AnchoredNameAffinity(leftName, rightName)))
            .DefaultIfEmpty()
            .Max();
        float requiredName = strictSpatialPair
            ? 0.8f
            : separateBakedRepresentations && !sameRenderable
                ? 0.75f
                : 0.6f;
        if (nameAffinity < requiredName)
            return null;
        float geometryAffinity = GeometryAffinity(left.Bounds, right.Bounds);
        float requiredGeometry = sameRenderable
            ? 0.3f
            : strictSpatialPair
                ? 0.97f
                : 0.94f;
        if (geometryAffinity < requiredGeometry)
            return null;

        float score = sameRenderable
            ? nameAffinity * MathF.Sqrt(geometryAffinity)
            : nameAffinity * geometryAffinity;
        return new AssemblyCandidate(left, right, score, geometryAffinity);
    }

    private static ModelCluster[] BuildClusters(
        IReadOnlyList<ModelAtom> atoms,
        DisjointSets sets) => atoms
        .GroupBy(atom => sets.Find(atom.Index))
        .Select(group =>
        {
            ModelAtom[] grouped = group.ToArray();
            return new ModelCluster(
                group.Key,
                grouped,
                grouped.SelectMany(atom => atom.Names)
                    .Distinct(StringComparer.OrdinalIgnoreCase).ToArray(),
                grouped.SelectMany(atom => atom.PartitionRenderableIndices)
                    .Distinct().ToArray(),
                grouped.Any(atom => atom.PartitionRenderableIndices.Length > 0),
                grouped.Any(atom => atom.PartitionRenderableIndices.Length == 0),
                ModelBounds.Union(grouped.Select(atom => atom.Bounds)));
        })
        .ToArray();

    private static SmoCompositeModel CreateComposite(
        SmoLevelWorkspace workspace,
        IReadOnlyList<ModelAtom> atoms)
    {
        SmoLevelEntity[] entities = atoms.Select(atom => atom.Entity)
            .DistinctBy(entity => entity.Id)
            .OrderBy(entity => entity.Id.SceneObjectIndex)
            .ToArray();
        string[] names = atoms.SelectMany(atom => atom.Names)
            .Where(name => !string.IsNullOrWhiteSpace(name))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToArray();
        string[] placedNames = atoms
            .Where(atom => atom.PartitionRenderableIndices.Length == 0)
            .SelectMany(atom => atom.Names)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToArray();
        string name = names.Length == 1
            ? names[0]
            : placedNames.Length > 0
                ? placedNames.OrderBy(candidate => candidate.Length)
                    .ThenBy(candidate => candidate, StringComparer.OrdinalIgnoreCase)
                    .First()
                : names.Select(StripTrailingVariant)
                    .OrderBy(candidate => candidate.Length)
                    .ThenBy(candidate => candidate, StringComparer.OrdinalIgnoreCase)
                    .FirstOrDefault() ?? entities[0].Name;
        int[] renderables = atoms.SelectMany(atom => atom.PartitionRenderableIndices)
            .Distinct().ToArray();
        int parentIndex = renderables.Length == 1
            ? renderables[0]
            : FindCommonParent(
                workspace.Document.Objects,
                atoms.SelectMany(atom => atom.SourceObjectIndices).Distinct());
        return new SmoCompositeModel(
            name,
            parentIndex,
            entities[0].WorldTransform,
            new ReadOnlyCollection<SmoLevelEntity>(entities));
    }

    private static int FindCommonParent(
        IReadOnlyList<SmoObjectEntry> objects,
        IEnumerable<int> sourceIndices)
    {
        int[][] paths = sourceIndices
            .Where(index => (uint)index < (uint)objects.Count)
            .Select(index => AncestorPath(objects, index))
            .ToArray();
        if (paths.Length == 0)
            return 0;
        int common = paths[0][0];
        for (int depth = 0; depth < paths.Min(path => path.Length); depth++)
        {
            int candidate = paths[0][depth];
            if (paths.All(path => path[depth] == candidate))
                common = candidate;
            else
                break;
        }
        return common;
    }

    private static int[] AncestorPath(
        IReadOnlyList<SmoObjectEntry> objects,
        int sourceIndex)
    {
        var path = new List<int>();
        var visited = new HashSet<int>();
        int? cursor = sourceIndex;
        while (cursor is int index &&
               (uint)index < (uint)objects.Count &&
               visited.Add(index))
        {
            path.Add(index);
            cursor = objects[index].ParentIndex;
        }
        path.Reverse();
        return path.ToArray();
    }

    private static float AnchoredNameAffinity(string left, string right)
    {
        string a = NormalizeAnchoredName(left);
        string b = NormalizeAnchoredName(right);
        if (a.Length == 0 || b.Length == 0)
            return 0;
        if (a.Equals(b, StringComparison.Ordinal))
            return 1;

        int prefix = 0;
        while (prefix < Math.Min(a.Length, b.Length) && a[prefix] == b[prefix])
            prefix++;
        int prefixLetters = a[..prefix].Count(char.IsLetter);
        // A shared namespace token such as "Muza_" is authorship context,
        // not model identity. Require the prefix to enter the next token.
        bool endsAtTokenBoundary = prefix > 0 && a[prefix - 1] == '_';
        float prefixAffinity = prefixLetters >= 4 && !endsAtTokenBoundary
            ? (float)prefix / Math.Min(a.Length, b.Length)
            : 0;

        string[] tokensA = a.Split('_', StringSplitOptions.RemoveEmptyEntries);
        string[] tokensB = b.Split('_', StringSplitOptions.RemoveEmptyEntries);
        float tokenAffinity = 0;
        if (tokensA.Length > 1 && tokensB.Length > 1 &&
            tokensA[0].Length >= 3 &&
            tokensA[0].Equals(tokensB[0], StringComparison.Ordinal))
        {
            int secondaryMatch = tokensA.Skip(1)
                .SelectMany(leftToken => tokensB.Skip(1).Select(rightToken =>
                    LongestCommonSubstringLength(leftToken, rightToken)))
                .DefaultIfEmpty()
                .Max();
            if (secondaryMatch >= 3)
            {
                tokenAffinity = (float)(tokensA[0].Length + secondaryMatch) /
                    Math.Max(1, Math.Min(
                        a.Count(char.IsLetterOrDigit),
                        b.Count(char.IsLetterOrDigit)));
            }
        }
        return MathF.Max(prefixAffinity, tokenAffinity);
    }

    private static int LongestCommonSubstringLength(string left, string right)
    {
        int[] previous = new int[right.Length + 1];
        int[] current = new int[right.Length + 1];
        int longest = 0;
        for (int leftIndex = 1; leftIndex <= left.Length; leftIndex++)
        {
            for (int rightIndex = 1; rightIndex <= right.Length; rightIndex++)
            {
                if (left[leftIndex - 1] != right[rightIndex - 1])
                    continue;
                current[rightIndex] = previous[rightIndex - 1] + 1;
                longest = Math.Max(longest, current[rightIndex]);
            }
            (previous, current) = (current, previous);
            Array.Clear(current);
        }
        return longest;
    }

    private static string NormalizeAnchoredName(string value)
    {
        value = StripTrailingVariant(value).ToLowerInvariant();
        return new string(value.Where(character =>
                char.IsLetterOrDigit(character) || character == '_')
            .ToArray());
    }

    private static string StripTrailingVariant(string value)
    {
        value = value.Trim().TrimEnd('\0');
        int end = value.Length;
        while (end > 0 && char.IsDigit(value[end - 1]))
            end--;
        while (end > 0 && value[end - 1] is '_' or '-' or ' ')
            end--;
        return end == 0 ? value : value[..end];
    }

    private static float GeometryAffinity(ModelBounds left, ModelBounds right)
    {
        if (left.IsEmpty || right.IsEmpty)
            return 0;
        Vector3 overlap = Vector3.Max(
            Vector3.Zero,
            Vector3.Min(left.Maximum, right.Maximum) -
            Vector3.Max(left.Minimum, right.Minimum));
        Vector3 leftSize = left.Size;
        Vector3 rightSize = right.Size;
        float xy = ProjectionContainment(
            overlap.X, overlap.Y, leftSize.X, leftSize.Y, rightSize.X, rightSize.Y);
        float xz = ProjectionContainment(
            overlap.X, overlap.Z, leftSize.X, leftSize.Z, rightSize.X, rightSize.Z);
        float yz = ProjectionContainment(
            overlap.Y, overlap.Z, leftSize.Y, leftSize.Z, rightSize.Y, rightSize.Z);
        float projection = MathF.Max(xy, MathF.Max(xz, yz));
        Vector3 separation = Vector3.Max(
            Vector3.Zero,
            Vector3.Max(left.Minimum - right.Maximum, right.Minimum - left.Maximum));
        float scale = MathF.Max(
            MathF.Min(MaxExtent(leftSize), MaxExtent(rightSize)),
            1e-3f);
        float proximity = MathF.Exp(-separation.Length() / (scale * 0.75f));
        return projection * proximity;
    }

    private static float ProjectionContainment(
        float overlapA,
        float overlapB,
        float leftA,
        float leftB,
        float rightA,
        float rightB) => overlapA * overlapB / MathF.Max(
        MathF.Min(leftA * leftB, rightA * rightB),
        1e-6f);

    private static float MaxExtent(Vector3 value) =>
        MathF.Max(value.X, MathF.Max(value.Y, value.Z));

    private static string StripNumberedPartSuffix(
        string modelName,
        out int componentIndex)
    {
        componentIndex = -1;
        modelName = modelName.TrimEnd('\0');
        if (modelName.Length < 5 || modelName[^4] != '-' ||
            !int.TryParse(modelName.AsSpan(modelName.Length - 3), out componentIndex))
        {
            return modelName;
        }
        return modelName[..^4];
    }

    private static bool TryReadOwnedComponent(
        string ownerName,
        string modelName,
        out int componentIndex)
    {
        string authoredName = StripNumberedPartSuffix(modelName, out componentIndex);
        return componentIndex >= 0 &&
               authoredName.Equals(ownerName, StringComparison.OrdinalIgnoreCase);
    }

    private static int? FindAncestorOfType(
        IReadOnlyList<SmoObjectEntry> objects,
        SmoObjectEntry entry,
        uint typeHash)
    {
        var visited = new HashSet<int>();
        SmoObjectEntry? cursor = entry;
        while (cursor is not null && visited.Add(cursor.Index))
        {
            if (cursor.TypeHash == typeHash)
                return cursor.Index;
            cursor = cursor.ParentIndex is int parent && (uint)parent < (uint)objects.Count
                ? objects[parent]
                : null;
        }
        return null;
    }

    private static void UnionAll(DisjointSets sets, IEnumerable<int> source)
    {
        int[] indices = source.Distinct().ToArray();
        for (int index = 1; index < indices.Length; index++)
            sets.Union(indices[0], indices[index]);
    }

    private sealed record ModelAtom(
        int Index,
        SmoLevelEntity Entity,
        string[] Names,
        int[] PartitionRenderableIndices,
        int[] SourceObjectIndices,
        ModelBounds Bounds);

    private sealed record ModelCluster(
        int Root,
        ModelAtom[] Atoms,
        string[] Names,
        int[] PartitionRenderableIndices,
        bool HasBakedGeometry,
        bool HasPlacedGeometry,
        ModelBounds Bounds);

    private sealed record AssemblyCandidate(
        ModelCluster Left,
        ModelCluster Right,
        float Score,
        float GeometryAffinity);

    private sealed record StaticCandidate(
        int AtomIndex,
        string OwnerName,
        int ParentIndex,
        Matrix4x4 WorldTransform,
        int ComponentIndex);

    private sealed record PartitionCandidate(
        int AtomIndex,
        int RenderableIndex,
        string AuthoredName);

    private readonly record struct StaticKey(
        int ParentIndex,
        string OwnerName,
        Matrix4x4 WorldTransform);

    private readonly record struct ModelBounds(Vector3 Minimum, Vector3 Maximum)
    {
        public static ModelBounds Empty => new(
            new Vector3(float.PositiveInfinity),
            new Vector3(float.NegativeInfinity));
        public bool IsEmpty => float.IsPositiveInfinity(Minimum.X);
        public Vector3 Size => IsEmpty ? Vector3.Zero : Maximum - Minimum;

        public ModelBounds Include(SmoSceneMesh sceneMesh)
        {
            ModelBounds result = this;
            foreach (Vector3 position in sceneMesh.Mesh.Positions)
            {
                Vector3 world = Vector3.Transform(position, sceneMesh.WorldTransform);
                result = new ModelBounds(
                    Vector3.Min(result.Minimum, world),
                    Vector3.Max(result.Maximum, world));
            }
            return result;
        }

        public static ModelBounds Union(IEnumerable<ModelBounds> source)
        {
            ModelBounds result = Empty;
            foreach (ModelBounds bounds in source.Where(bounds => !bounds.IsEmpty))
            {
                result = new ModelBounds(
                    Vector3.Min(result.Minimum, bounds.Minimum),
                    Vector3.Max(result.Maximum, bounds.Maximum));
            }
            return result;
        }
    }

    private sealed class DisjointSets
    {
        private readonly int[] _parents;
        private readonly byte[] _ranks;

        public DisjointSets(int count)
        {
            _parents = Enumerable.Range(0, count).ToArray();
            _ranks = new byte[count];
        }

        public int Find(int value)
        {
            if (_parents[value] != value)
                _parents[value] = Find(_parents[value]);
            return _parents[value];
        }

        public void Union(int left, int right)
        {
            left = Find(left);
            right = Find(right);
            if (left == right)
                return;
            if (_ranks[left] < _ranks[right])
                (left, right) = (right, left);
            _parents[right] = left;
            if (_ranks[left] == _ranks[right])
                _ranks[left]++;
        }
    }
}
