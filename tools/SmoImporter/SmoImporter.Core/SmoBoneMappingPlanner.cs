using SmoViewer.Core;
using System.Numerics;

namespace SmoImporter.Core;

public sealed record SmoIgnoredDonorBone(
    string DonorBoneName,
    string TargetBoneName);

internal sealed record SmoBoneRemapPlan(
    IReadOnlyDictionary<string, string> DonorToTarget,
    IReadOnlyList<string> MatchedBones,
    IReadOnlyList<SmoIgnoredDonorBone> IgnoredDonorBones,
    IReadOnlyList<string> UnboundTargetBones,
    IReadOnlyList<string> Errors);

/// <summary>
/// Builds the exact donor-palette to target-node mapping used by the writer.
/// Extra donor bones are collapsed to their nearest weighted common ancestor;
/// target bones without donor weights remain in the graph but are reported.
/// </summary>
internal static class SmoBoneMappingPlanner
{
    public static SmoBoneRemapPlan Build(SmoDocument target, SmoDocument donor)
    {
        HashSet<string> targetWeighted = GetWeightedBoneNames(target);
        HashSet<string> donorWeighted = GetWeightedBoneNames(donor);
        Dictionary<string, SmoObjectEntry> targetNodes = GetUniqueNodes(target);
        Dictionary<string, SmoObjectEntry> donorNodes = GetUniqueNodes(donor);
        SmoNodeHierarchy donorHierarchy = SmoNodeHierarchy.Decode(donor);
        IReadOnlyDictionary<int, SmoObjectEntry> donorEntries = donor.Objects
            .ToDictionary(entry => entry.Index);
        IReadOnlyDictionary<string, Vector3> donorBindPositions =
            GetBoneBindPositions(donor);

        var remap = new Dictionary<string, string>(StringComparer.Ordinal);
        var matched = new List<string>();
        var ignored = new List<SmoIgnoredDonorBone>();
        var errors = new List<string>();

        foreach (string donorBone in donorWeighted.Order(StringComparer.Ordinal))
        {
            if (targetNodes.ContainsKey(donorBone))
            {
                remap.Add(donorBone, donorBone);
                matched.Add(donorBone);
                continue;
            }

            if (!donorNodes.TryGetValue(donorBone, out SmoObjectEntry? donorNode))
            {
                errors.Add($"Donor bone \"{donorBone}\" не имеет уникального spNode.");
                continue;
            }

            string? fallback = FindNearestCommonWeightedAncestor(
                donorNode.Index,
                donorHierarchy,
                donorEntries,
                targetNodes.Keys,
                donorWeighted);
            fallback ??= FindClosestSharedBone(
                donorBone, donorBindPositions, targetNodes.Keys, donorWeighted);
            if (fallback is null)
            {
                errors.Add(
                    $"Для дополнительной donor bone \"{donorBone}\" не найден " +
                    "общий weighted ancestor или ближайшая shared bone в target skeleton.");
                continue;
            }

            remap.Add(donorBone, fallback);
            ignored.Add(new SmoIgnoredDonorBone(donorBone, fallback));
        }

        string[] unboundTarget = targetWeighted
            .Except(donorWeighted, StringComparer.Ordinal)
            .Order(StringComparer.Ordinal)
            .ToArray();
        return new SmoBoneRemapPlan(
            remap,
            matched.Order(StringComparer.Ordinal).ToArray(),
            ignored.OrderBy(item => item.DonorBoneName, StringComparer.Ordinal).ToArray(),
            unboundTarget,
            errors);
    }

    private static HashSet<string> GetWeightedBoneNames(SmoDocument document)
    {
        var result = new HashSet<string>(StringComparer.Ordinal);
        foreach (SmoObjectEntry meshEntry in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MeshData))
        {
            SmoObjectEntry? cursor = meshEntry;
            while (cursor?.ParentIndex is int parentIndex &&
                   cursor.TypeHash != SmoClassIds.Skin)
                cursor = document.Objects[parentIndex];
            if (cursor?.TypeHash != SmoClassIds.Skin)
                continue;
            if (!SmoSkinDecoder.TryDecode(
                    document, cursor, out SmoSkin? skin, out _) || skin is null ||
                !SmoMeshDecoder.TryDecode(
                    document, meshEntry, out SmoMesh? mesh, out _) || mesh is null ||
                !mesh.HasSkinningData)
                continue;
            for (int vertex = 0; vertex < mesh.VertexCount; vertex++)
            {
                Vector4 weights = mesh.BlendWeights[vertex];
                SmoBlendIndices indices = mesh.BlendIndices[vertex];
                Add(weights.X, indices.X);
                Add(weights.Y, indices.Y);
                Add(weights.Z, indices.Z);
                Add(weights.W, indices.W);
            }

            void Add(float weight, byte paletteIndex)
            {
                if (weight <= 0.000001f || paletteIndex >= skin.Bones.Count)
                    return;
                result.Add(document.Objects[
                    skin.Bones[paletteIndex].NodeObjectIndex].Name);
            }
        }
        return result;
    }

    private static Dictionary<string, SmoObjectEntry> GetUniqueNodes(SmoDocument document) =>
        document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Node &&
                !string.IsNullOrWhiteSpace(entry.Name))
            .GroupBy(entry => entry.Name, StringComparer.Ordinal)
            .Where(group => group.Count() == 1)
            .ToDictionary(
                group => group.Key,
                group => group.Single(),
                StringComparer.Ordinal);

    private static IReadOnlyDictionary<string, Vector3> GetBoneBindPositions(
        SmoDocument document)
    {
        var positions = new Dictionary<string, Vector3>(StringComparer.Ordinal);
        foreach (SmoObjectEntry skinEntry in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            if (!SmoSkinDecoder.TryDecode(
                    document, skinEntry, out SmoSkin? skin, out _) || skin is null)
                continue;
            foreach (SmoSkinBone bone in skin.Bones)
            {
                string name = document.Objects[bone.NodeObjectIndex].Name;
                if (!positions.ContainsKey(name) &&
                    Matrix4x4.Invert(bone.InverseBindMatrix, out Matrix4x4 bindWorld))
                    positions.Add(name, bindWorld.Translation);
            }
        }
        return positions;
    }

    private static string? FindClosestSharedBone(
        string donorBone,
        IReadOnlyDictionary<string, Vector3> donorBindPositions,
        IEnumerable<string> targetNodeNames,
        IReadOnlySet<string> donorWeighted)
    {
        if (!donorBindPositions.TryGetValue(donorBone, out Vector3 source))
            return null;
        HashSet<string> targets = targetNodeNames.ToHashSet(StringComparer.Ordinal);
        return donorBindPositions
            .Where(pair => targets.Contains(pair.Key) && donorWeighted.Contains(pair.Key))
            .OrderBy(pair => Vector3.DistanceSquared(source, pair.Value))
            .ThenBy(pair => pair.Key, StringComparer.Ordinal)
            .Select(pair => pair.Key)
            .FirstOrDefault();
    }

    private static string? FindNearestCommonWeightedAncestor(
        int donorNodeIndex,
        SmoNodeHierarchy hierarchy,
        IReadOnlyDictionary<int, SmoObjectEntry> entries,
        IEnumerable<string> targetNodeNames,
        IReadOnlySet<string> donorWeighted)
    {
        HashSet<string> targetNames = targetNodeNames.ToHashSet(StringComparer.Ordinal);
        var visited = new HashSet<int>();
        int cursor = donorNodeIndex;
        while (visited.Add(cursor) &&
               hierarchy.ParentsByChild.TryGetValue(
                   cursor, out IReadOnlyList<int>? parents) &&
               parents.Count == 1 &&
               entries.TryGetValue(parents[0], out SmoObjectEntry? parent))
        {
            if (targetNames.Contains(parent.Name) && donorWeighted.Contains(parent.Name))
                return parent.Name;
            cursor = parent.Index;
        }
        return null;
    }
}
