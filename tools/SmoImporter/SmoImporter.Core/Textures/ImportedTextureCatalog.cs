namespace SmoImporter.Core;

/// <summary>A logical texture shared by the listed materials in the base scene.</summary>
/// <remarks>The key is the first material index in this slot. It belongs to the base scene.</remarks>
public sealed record ImportedTextureSlot(
    int Key,
    string Name,
    IReadOnlyList<int> MaterialIndices,
    int EffectiveTextureIndex,
    bool IsExplicit);

public sealed record ImportedTextureCatalogResult(
    ImportedScene EffectiveScene,
    IReadOnlyList<ImportedTexture> UnusedExternalTextures,
    IReadOnlyList<string> Messages)
{
    public IReadOnlyList<ImportedTextureSlot> Slots { get; init; } = [];
}

/// <summary>
/// Resolves external image files against the logical texture groups declared by
/// an imported scene. Matching is deterministic: an exact file name wins over
/// a bare-name match, while ambiguous names are rejected instead of guessed.
/// </summary>
public static class ImportedTextureCatalog
{
    public static ImportedTextureCatalogResult ResolveExternalOverrides(
        ImportedScene source,
        IReadOnlyList<ImportedTexture> external)
        => ResolveExternalOverrides(source, external, null);

    /// <summary>
    /// Applies selected-slot assignments before automatic matching. The keys come from
    /// <see cref="ImportedTextureCatalogResult.Slots"/> for this base scene.
    /// </summary>
    public static ImportedTextureCatalogResult ResolveExternalOverrides(
        ImportedScene source,
        IReadOnlyList<ImportedTexture> external,
        IReadOnlyDictionary<int, ImportedTexture>? explicitAssignments)
    {
        ArgumentNullException.ThrowIfNull(source);
        ArgumentNullException.ThrowIfNull(external);

        ImportedTexture[] sourceTextures = source.Textures.ToArray();
        ImportedMaterial[] sourceMaterials = source.Materials.ToArray();
        TextureGroup[] groups = BuildGroups(sourceTextures, sourceMaterials);
        var selectedTextures = new Dictionary<TextureGroup, ImportedTexture>();
        if (explicitAssignments is { Count: > 0 })
        {
            foreach (TextureGroup group in groups)
            {
                if (explicitAssignments.TryGetValue(group.MaterialIndices[0], out ImportedTexture? texture))
                {
                    selectedTextures.Add(group, texture ?? throw new ArgumentException(
                        $"Texture for slot [{group.MaterialIndices[0]}] is null.", nameof(explicitAssignments)));
                }
            }
            if (selectedTextures.Count != explicitAssignments.Count)
                throw new InvalidDataException("A selected texture slot is absent from the base scene.");
        }

        ExternalEntry[] externalEntries = external
            .Select((texture, index) => new ExternalEntry(
                index,
                texture ?? throw new ArgumentException(
                    $"External texture at index {index} is null.", nameof(external)),
                TextureAliases.FromTexture(texture)))
            .ToArray();

        RejectDuplicateExternalFileNames(externalEntries);

        var proposals = new List<MatchProposal>();
        foreach (ExternalEntry entry in externalEntries)
        {
            (TextureGroup Group, int Score)[] candidates = groups
                .Select(group => (Group: group, Score: MatchScore(group.Aliases, entry.Aliases)))
                .Where(candidate => candidate.Score > 0)
                .ToArray();
            if (candidates.Length == 0)
                continue;

            int bestScore = candidates.Max(candidate => candidate.Score);
            TextureGroup[] bestGroups = candidates
                .Where(candidate => candidate.Score == bestScore &&
                    !selectedTextures.ContainsKey(candidate.Group))
                .Select(candidate => candidate.Group)
                .ToArray();
            // An explicit override occupies its slot without sending its
            // automatic candidate to a different, less specific match.
            if (bestGroups.Length == 0)
                continue;
            if (bestGroups.Length != 1)
            {
                throw new InvalidDataException(
                    $"External texture '{DisplayName(entry.Texture)}' matches multiple " +
                    "source texture groups: " +
                    string.Join(", ", bestGroups.Select(group => group.DisplayName)) + ".");
            }
            proposals.Add(new MatchProposal(entry, bestGroups[0], bestScore));
        }

        var assignments = new Dictionary<TextureGroup, ExternalEntry>();
        foreach (IGrouping<TextureGroup, MatchProposal> groupProposals in
                 proposals.GroupBy(proposal => proposal.Group))
        {
            int bestScore = groupProposals.Max(proposal => proposal.Score);
            MatchProposal[] best = groupProposals
                .Where(proposal => proposal.Score == bestScore)
                .ToArray();
            if (best.Length != 1)
            {
                throw new InvalidDataException(
                    $"Source texture group {groupProposals.Key.DisplayName} matches multiple " +
                    "external files: " + string.Join(", ", best.Select(proposal =>
                        $"'{DisplayName(proposal.External.Texture)}'")) + ".");
            }
            assignments.Add(groupProposals.Key, best[0].External);
        }

        HashSet<int> assignedExternalIndices = assignments.Values
            .Select(entry => entry.Index)
            .ToHashSet();
        var effectiveTextures = sourceTextures.ToList();
        ImportedMaterial[] effectiveMaterials = sourceMaterials.ToArray();
        var messages = new List<string>();
        var effectiveAssignments = assignments
            .Select(pair => new KeyValuePair<TextureGroup, ImportedTexture>(pair.Key, pair.Value.Texture))
            .Concat(selectedTextures)
            .OrderBy(pair => pair.Key.SortKey);
        foreach ((TextureGroup group, ImportedTexture texture) in effectiveAssignments)
        {
            int effectiveTextureIndex = group.TextureIndex;
            if (effectiveTextureIndex >= 0)
                effectiveTextures[effectiveTextureIndex] = texture;
            else
            {
                effectiveTextureIndex = effectiveTextures.Count;
                effectiveTextures.Add(texture);
            }

            messages.Add(selectedTextures.ContainsKey(group)
                ? $"External texture '{DisplayName(texture)}' is assigned to {group.MaterialDescription}."
                : group.TextureIndex >= 0
                    ? $"External texture '{DisplayName(texture)}' overrides source " +
                      $"texture [{effectiveTextureIndex}] for {group.MaterialDescription}."
                    : $"External texture '{DisplayName(texture)}' resolves " +
                      $"{group.MaterialDescription} by an explicit filename match.");

            foreach (int materialIndex in group.MaterialIndices)
            {
                effectiveMaterials[materialIndex] = effectiveMaterials[materialIndex] with
                {
                    BaseColorTextureIndex = effectiveTextureIndex
                };
            }
        }

        ImportedTexture[] unused = externalEntries
            .Where(entry => !assignedExternalIndices.Contains(entry.Index))
            .Select(entry => entry.Texture)
            .ToArray();
        foreach (ImportedTexture texture in unused)
        {
            messages.Add(
                $"External texture '{DisplayName(texture)}' is not referenced by any " +
                "source material and remains unused.");
        }

        var effectiveScene = source with
        {
            EmbeddedTextures = effectiveTextures.AsReadOnly(),
            SourceMaterials = Array.AsReadOnly(effectiveMaterials)
        };
        ImportedModelResourceLimits.ValidateTextures(
            effectiveScene.Textures,
            "Imported scene");
        ImportedTextureSlot[] slots = groups.Select(group =>
        {
            int key = group.MaterialIndices[0];
            return new ImportedTextureSlot(
                key,
                SlotName(group, sourceTextures, sourceMaterials[key]),
                group.MaterialIndices.AsReadOnly(),
                effectiveMaterials[key].BaseColorTextureIndex,
                selectedTextures.ContainsKey(group));
        }).ToArray();
        return new ImportedTextureCatalogResult(
            effectiveScene,
            Array.AsReadOnly(unused),
            messages.AsReadOnly())
        {
            Slots = Array.AsReadOnly(slots)
        };
    }

    private static string SlotName(TextureGroup group, IReadOnlyList<ImportedTexture> textures, ImportedMaterial material)
    {
        if (group.TextureIndex >= 0 && !string.IsNullOrWhiteSpace(textures[group.TextureIndex].Name))
            return textures[group.TextureIndex].Name;

        return string.IsNullOrWhiteSpace(material.BaseColorTextureName)
            ? material.Name
            : NormalizeGroupReference(material.BaseColorTextureName);
    }

    private static TextureGroup[] BuildGroups(
        IReadOnlyList<ImportedTexture> textures,
        IReadOnlyList<ImportedMaterial> materials)
    {
        var groups = new Dictionary<string, TextureGroup>(StringComparer.OrdinalIgnoreCase);
        for (int materialIndex = 0; materialIndex < materials.Count; materialIndex++)
        {
            ImportedMaterial material = materials[materialIndex];
            if (material.BaseColorTextureIndex < -1 ||
                material.BaseColorTextureIndex >= textures.Count)
            {
                throw new InvalidDataException(
                    $"Material [{materialIndex}] '{material.Name}' references texture " +
                    $"index {material.BaseColorTextureIndex}, but the scene contains " +
                    $"{textures.Count} textures.");
            }

            bool resolved = material.BaseColorTextureIndex >= 0;
            bool unbound = !resolved && string.IsNullOrWhiteSpace(material.BaseColorTextureName);
            string key = resolved
                ? $"texture:{material.BaseColorTextureIndex}"
                : unbound ? $"unbound:{materialIndex}"
                    : "unresolved:" + NormalizeGroupReference(material.BaseColorTextureName!);
            if (!groups.TryGetValue(key, out TextureGroup? group))
            {
                group = new TextureGroup(
                    key,
                    resolved ? material.BaseColorTextureIndex : -1,
                    resolved
                        ? $"source texture [{material.BaseColorTextureIndex}]"
                        : unbound ? $"untextured material [{materialIndex}] '{material.Name}'"
                            : $"unresolved texture '{material.BaseColorTextureName}'");
                groups.Add(key, group);
            }
            group.MaterialIndices.Add(materialIndex);
            group.MaterialNames.Add(material.Name);
            group.Aliases.Add(material.BaseColorTextureName);
        }

        foreach (TextureGroup group in groups.Values.Where(group => group.TextureIndex >= 0))
            group.Aliases.Add(textures[group.TextureIndex]);
        return groups.Values
            .OrderBy(group => group.SortKey, StringComparer.OrdinalIgnoreCase)
            .ToArray();
    }

    private static void RejectDuplicateExternalFileNames(
        IReadOnlyList<ExternalEntry> external)
    {
        IGrouping<string, ExternalEntry>? duplicate = external
            .SelectMany(entry => entry.Aliases.FileNames.Select(fileName =>
                (FileName: fileName, Entry: entry)))
            .GroupBy(item => item.FileName, item => item.Entry,
                StringComparer.OrdinalIgnoreCase)
            .FirstOrDefault(group => group.Select(entry => entry.Index).Distinct().Count() > 1);
        if (duplicate is null)
            return;
        throw new InvalidDataException(
            $"External texture filename '{duplicate.Key}' is ambiguous: " +
            string.Join(", ", duplicate.Select(entry =>
                $"'{DisplayName(entry.Texture)}'")) + ".");
    }

    private static int MatchScore(TextureAliases source, TextureAliases external)
    {
        if (source.FileNames.Overlaps(external.FileNames))
            return 2;
        return source.BaseNames.Overlaps(external.BaseNames) ? 1 : 0;
    }

    private static string NormalizeGroupReference(string value)
    {
        string trimmed = value.Trim();
        string fileName = SafeFileName(trimmed);
        return string.IsNullOrWhiteSpace(fileName) ? trimmed : fileName;
    }

    private static string DisplayName(ImportedTexture texture)
    {
        string? sourceName = string.IsNullOrWhiteSpace(texture.SourcePath)
            ? null
            : SafeFileName(texture.SourcePath);
        return string.IsNullOrWhiteSpace(sourceName) ? texture.Name : sourceName;
    }

    private static string SafeFileName(string value)
    {
        try
        {
            return Path.GetFileName(value);
        }
        catch (ArgumentException)
        {
            return value;
        }
    }

    private sealed class TextureGroup(
        string sortKey,
        int textureIndex,
        string displayName)
    {
        public string SortKey { get; } = sortKey;
        public int TextureIndex { get; } = textureIndex;
        public string DisplayName { get; } = displayName;
        public List<int> MaterialIndices { get; } = [];
        public List<string> MaterialNames { get; } = [];
        public TextureAliases Aliases { get; } = new();
        public string MaterialDescription => MaterialNames.Count == 1
            ? $"material '{MaterialNames[0]}'"
            : "materials " + string.Join(", ", MaterialNames.Select(name => $"'{name}'"));
    }

    private sealed record ExternalEntry(
        int Index,
        ImportedTexture Texture,
        TextureAliases Aliases);

    private sealed record MatchProposal(
        ExternalEntry External,
        TextureGroup Group,
        int Score);

    private sealed class TextureAliases
    {
        public HashSet<string> FileNames { get; } = new(StringComparer.OrdinalIgnoreCase);
        public HashSet<string> BaseNames { get; } = new(StringComparer.OrdinalIgnoreCase);

        public static TextureAliases FromTexture(ImportedTexture texture)
        {
            var result = new TextureAliases();
            result.Add(texture);
            return result;
        }

        public void Add(ImportedTexture texture)
        {
            Add(texture.Name);
            Add(texture.SourcePath);
        }

        public void Add(string? reference)
        {
            if (string.IsNullOrWhiteSpace(reference))
                return;
            string fileName = SafeFileName(reference.Trim());
            if (string.IsNullOrWhiteSpace(fileName))
                return;
            string extension = Path.GetExtension(fileName);
            if (!string.IsNullOrWhiteSpace(extension))
                FileNames.Add(fileName);
            string baseName = Path.GetFileNameWithoutExtension(fileName);
            BaseNames.Add(string.IsNullOrWhiteSpace(baseName) ? fileName : baseName);
        }
    }
}
