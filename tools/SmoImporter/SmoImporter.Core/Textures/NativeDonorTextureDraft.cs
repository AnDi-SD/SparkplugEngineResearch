using SmoViewer.Core;
using TextureToolDocument = SMOTextureTool.Core.SmoDocument;
using TextureToolLayout = SMOTextureTool.Core.TextureLayout;

namespace SmoImporter.Core;

/// <summary>Inspects and edits texture resources in an in-memory native donor.</summary>
public static class NativeDonorTextureDraft
{
    public sealed record TextureSlot(int ObjectIndex, uint ObjectId, string Name,
        int Width, int Height, string? ReplacementIssue)
    {
        public bool CanReplace => ReplacementIssue is null;
    }

    public static IReadOnlyList<TextureSlot> Inspect(SmoDocument donor,
        IReadOnlyList<SmoModelReader.NativePreviewMaterial> materials)
    {
        ArgumentNullException.ThrowIfNull(donor);
        ArgumentNullException.ThrowIfNull(materials);

        var references = new Dictionary<int, SmoLoadedTexture>();
        void AddReference(SmoLoadedTexture? reference)
        {
            if (reference is not null)
                references.TryAdd(reference.ObjectIndex, reference);
        }

        foreach (var material in materials.Select(item => item.Material).OfType<SmoLoadedMaterial>()
                     .DistinctBy(item => item.ObjectIndex))
        foreach (var pass in material.Passes)
        foreach (var layer in pass.Layers)
        {
            AddReference(layer.Texture);
            if (layer.Animation is { } animation)
            {
                foreach (var key in animation.Keys)
                    AddReference(key.Texture);
            }
        }

        return references.Values.OrderBy(item => item.ObjectIndex)
            .Select(item => Describe(donor, item)).ToArray();
    }

    public static SmoDocument Apply(SmoDocument donor, int textureObjectIndex, ImportedTexture replacement)
    {
        ArgumentNullException.ThrowIfNull(donor);
        ArgumentNullException.ThrowIfNull(replacement);
        if ((uint)textureObjectIndex >= (uint)donor.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(textureObjectIndex));

        SmoObjectEntry original = donor.Objects[textureObjectIndex];
        if (donor.HasErrors || original.TypeHash != SmoClassIds.TextureData)
            throw new InvalidDataException("The selected donor resource is not a valid texture object.");

        // TextureTool's list Index is not the container ObjectIndex. Resolve the
        // exact object first and pass the ordinal expected by its writer.
        var catalog = TextureToolDocument.Parse(donor.Data.Span);
        var texture = catalog.Textures.SingleOrDefault(item => item.ObjectIndex == textureObjectIndex);
        if (texture is not { CanReplace: true, Layout: TextureToolLayout.Bgra })
            throw new NotSupportedException(texture?.ReplacementIssue ?? "This embedded texture cannot be replaced.");

        byte[] editedBytes = FixedSizeTextureWriter.ReplaceRgba(donor.Data.ToArray(), texture.Index, replacement.Data);
        var edited = SmoDocument.ParseOwned(editedBytes, donor.SourcePath);
        if (edited.HasErrors || edited.Objects.Count != donor.Objects.Count)
            throw new InvalidDataException("The edited donor failed structural verification.");
        var editedObject = edited.Objects[textureObjectIndex];
        if (editedObject.Id != original.Id || editedObject.TypeHash != original.TypeHash ||
            !SmoTextureDataDecoder.TryDecode(edited, editedObject, out var decoded, out _) ||
            decoded.SelectedRepresentation is not { MipLevels.Count: 1 } representation ||
            representation.MipLevels[0].Width != texture.Width || representation.MipLevels[0].Height != texture.Height)
            throw new InvalidDataException("The edited donor texture did not retain its identity or dimensions.");

        return edited;
    }

    private static TextureSlot Describe(SmoDocument donor, SmoLoadedTexture reference)
    {
        int index = reference.ObjectIndex;
        if ((uint)index >= (uint)donor.Objects.Count)
            throw new InvalidDataException("A donor material references an unknown texture object.");
        SmoObjectEntry entry = donor.Objects[index];
        if (entry.Id != reference.ObjectId || entry.TypeHash != SmoClassIds.TextureData)
            throw new InvalidDataException("A donor material's texture reference did not match the donor catalog.");

        int width = reference.Texture?.Width ?? 0;
        int height = reference.Texture?.Height ?? 0;
        string? issue;
        if (SmoTextureDataDecoder.TryDecode(donor, entry, out var decoded, out string error))
        {
            if (decoded.SelectedRepresentation is { } representation)
            {
                width = representation.Width;
                height = representation.Height;
            }
            issue = SmoTextureDataWriter.CanReplace(decoded, out string reason) ? null : reason;
        }
        else
        {
            issue = error;
        }

        string name = string.IsNullOrWhiteSpace(entry.Name) ? $"Текстура {index}" : entry.Name;
        return new TextureSlot(index, entry.Id, name, width, height, issue);
    }
}
