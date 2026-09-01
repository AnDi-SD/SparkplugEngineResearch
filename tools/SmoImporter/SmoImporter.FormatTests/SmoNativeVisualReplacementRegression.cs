using SmoImporter.Core;
using SmoViewer.Core;

internal static class SmoNativeVisualReplacementRegression
{
    public static void Run(string targetPath, string donorPath)
    {
        targetPath = Path.GetFullPath(targetPath);
        donorPath = Path.GetFullPath(donorPath);
        byte[] targetBefore = File.ReadAllBytes(targetPath);
        byte[] donorBefore = File.ReadAllBytes(donorPath);
        SmoDocument target = SmoDocument.Load(targetPath);
        SmoDocument donor = SmoDocument.Load(donorPath);
        SmoNativeVisualGraphPlan plan =
            SmoNativeVisualGraphReplacer.Analyze(target, donor);
        Require(plan.CanReplace,
            "native plan failed: " + string.Join(" | ", plan.Errors));
        Require(plan.MeshCount == donor.Objects.Count(entry =>
                entry.TypeHash == SmoClassIds.MeshData),
            "plan must include every donor mesh");
        Require(plan.TextureCount == donor.Objects.Count(entry =>
                entry.TypeHash == SmoClassIds.TextureData),
            "plan must include every donor texture");

        HashSet<uint> oldVisualIds = target.Objects
            .Where(entry => entry.TypeHash is
                SmoClassIds.RenderNode or SmoClassIds.Skin or
                SmoClassIds.MeshData or SmoClassIds.MaterialData or
                SmoClassIds.TextureData)
            .Where(entry => target.Objects.Any(candidate =>
                candidate.TypeHash == SmoClassIds.MeshData &&
                candidate.PhysicalOffset >= entry.PhysicalOffset &&
                candidate.PhysicalEnd <= entry.PhysicalEnd))
            .Select(entry => entry.Id)
            .ToHashSet();
        string outputPath = Path.Combine(
            Path.GetTempPath(), $"native-smo-visual-{Guid.NewGuid():N}.smo");
        try
        {
            SmoNativeVisualGraphReplaceResult result =
                SmoNativeVisualGraphReplacer.Replace(
                    target, donor, outputPath);
            SmoDocument output = SmoDocument.Load(outputPath);
            Require(!output.HasErrors, "output must pass strict parsing");
            Require(result.MeshCount == plan.MeshCount &&
                    result.TextureCount == plan.TextureCount,
                "write counts must equal analyzed donor counts");
            Require(!output.Objects.Any(entry => oldVisualIds.Contains(entry.Id)),
                "old target visual IDs must not survive");
            HashSet<int> targetDeformIndices = target.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.Skin)
                .Select(entry =>
                {
                    Require(SmoSkinDecoder.TryDecode(
                            target, entry, out SmoSkin? skin, out string error) &&
                            skin is not null,
                        $"target skin {entry.Id} is invalid: {error}");
                    return skin!;
                })
                .SelectMany(skin => skin.Bones)
                .Select(bone => bone.NodeObjectIndex)
                .ToHashSet();
            HashSet<string> donorNodeKeys = donor.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.Node)
                .Select(NodeKey)
                .ToHashSet(StringComparer.Ordinal);
            HashSet<int> absentTargetDeform = targetDeformIndices
                .Where(index => !donorNodeKeys.Contains(NodeKey(target.Objects[index])))
                .ToHashSet();
            SmoNodeHierarchy targetHierarchy = SmoNodeHierarchy.Decode(target);
            foreach (SmoObjectEntry targetNode in target.Objects.Where(entry =>
                         entry.TypeHash == SmoClassIds.Node &&
                         !string.IsNullOrWhiteSpace(entry.Name)))
            {
                bool survived = output.Objects.Any(candidate =>
                        candidate.TypeHash == SmoClassIds.Node &&
                        candidate.RawName.Span.SequenceEqual(targetNode.RawName.Span));
                Require(survived ||
                        targetDeformIndices.Contains(targetNode.Index) ||
                        HasAncestor(
                            targetHierarchy,
                            targetNode.Index,
                            absentTargetDeform),
                    $"required target animation node {targetNode.Name} did not survive");
            }
            foreach (string ignoredBone in plan.IgnoredDonorBoneNames)
            {
                Require(!output.Objects.Any(entry =>
                        entry.TypeHash == SmoClassIds.Node &&
                        entry.Name.Equals(ignoredBone, StringComparison.Ordinal)),
                    $"ignored donor bone {ignoredBone} survived output");
            }
            HashSet<string> targetNodeNames = target.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.Node)
                .Select(entry => entry.Name)
                .ToHashSet(StringComparer.Ordinal);
            foreach (SmoObjectEntry skinEntry in output.Objects.Where(entry =>
                         entry.TypeHash == SmoClassIds.Skin))
            {
                Require(SmoSkinDecoder.TryDecode(
                        output, skinEntry, out SmoSkin? skin, out string skinError) &&
                        skin is not null,
                    $"output skin {skinEntry.Id} is invalid: {skinError}");
                foreach (SmoSkinBone bone in skin!.Bones)
                {
                    string boneName = output.Objects[bone.NodeObjectIndex].Name;
                    Require(targetNodeNames.Contains(boneName),
                        $"output skin {skinEntry.Id} retains donor-only bone {boneName}");
                }
            }
            VerifyExactLeafPayloads(donor, output, SmoClassIds.MeshData);
            VerifyExactLeafPayloads(donor, output, SmoClassIds.TextureData);
        }
        finally
        {
            if (File.Exists(outputPath))
                File.Delete(outputPath);
        }

        Require(File.ReadAllBytes(targetPath).SequenceEqual(targetBefore),
            "target source bytes changed");
        Require(File.ReadAllBytes(donorPath).SequenceEqual(donorBefore),
            "donor source bytes changed");
    }

    private static void VerifyExactLeafPayloads(
        SmoDocument donor,
        SmoDocument output,
        uint typeHash)
    {
        SmoObjectEntry[] donorEntries = donor.Objects
            .Where(entry => entry.TypeHash == typeHash)
            .ToArray();
        SmoObjectEntry[] outputEntries = output.Objects
            .Where(entry => entry.TypeHash == typeHash)
            .ToArray();
        Require(outputEntries.Length == donorEntries.Length,
            $"resource count mismatch for 0x{typeHash:X8}");
        foreach (SmoObjectEntry source in donorEntries)
        {
            SmoObjectEntry[] matches = outputEntries.Where(candidate =>
                    candidate.RawName.Span.SequenceEqual(source.RawName.Span) &&
                    candidate.SerializedSize == source.SerializedSize)
                .ToArray();
            Require(matches.Length == 1,
                $"resource {source.Name} has {matches.Length} output matches");
            Require(ObjectBytes(donor, source).SequenceEqual(
                    ObjectBytes(output, matches[0])),
                $"resource {source.Name} payload changed");
        }
    }

    private static ReadOnlySpan<byte> ObjectBytes(
        SmoDocument document,
        SmoObjectEntry entry) => document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset),
            checked((int)entry.SerializedSize));

    private static string NodeKey(SmoObjectEntry entry) =>
        $"{entry.TypeHash:X8}:{Convert.ToHexString(entry.RawName.Span)}";

    private static bool HasAncestor(
        SmoNodeHierarchy hierarchy,
        int objectIndex,
        IReadOnlySet<int> candidates)
    {
        var visited = new HashSet<int> { objectIndex };
        int cursor = objectIndex;
        while (hierarchy.ParentsByChild.TryGetValue(
                   cursor, out IReadOnlyList<int>? parents) &&
               parents.Count == 1)
        {
            cursor = parents[0];
            Require(visited.Add(cursor), "target hierarchy contains a cycle");
            if (candidates.Contains(cursor))
                return true;
        }
        return false;
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(
                "Native SMO visual replacement regression: " + message + ".");
    }
}
