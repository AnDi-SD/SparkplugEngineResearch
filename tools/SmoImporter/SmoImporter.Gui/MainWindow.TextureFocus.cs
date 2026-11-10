using System.IO;
using System.Windows;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;
using SmoImporter.Core;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private sealed record TextureFocusKey(int? NativeIndex = null, int? SlotKey = null,
        ImportedTexture? UnboundImage = null);

    private TextureFocusKey? _textureFocus;
    private TextureFocusKey? _shownTextureFocus;
    private bool _syncingTextureSelection;
    private Model3DGroup? _fullDonorPreview;
    private Model3DGroup? _textureFocusBase;
    private GeometryModel3D?[] _donorMeshVisuals = [];

    private static TextureFocusKey? GetTextureFocusKey(TextureResourceItem? item) => item switch
    {
        { Slot: { } slot } => new TextureFocusKey(SlotKey: slot.Key),
        { Image: { } image } => new TextureFocusKey(UnboundImage: image),
        _ => null
    };

    private void FocusExternalTexture()
    {
        if (_preparationStep && !_syncingTextureSelection && !ReplacementIsSmo)
        {
            _textureFocus = GetTextureFocusKey(TextureList.SelectedItem as TextureResourceItem);
            RefreshTextureFocusPreview();
        }
    }

    private void FocusNativeTexture()
    {
        if (_preparationStep && !_syncingTextureSelection && ReplacementIsSmo)
        {
            _textureFocus = NativeDonorTextureList.SelectedItem is NativeDonorTextureItem item
                ? new TextureFocusKey(NativeIndex: item.Slot.ObjectIndex) : null;
            RefreshTextureFocusPreview();
        }
    }

    private void ReturnToDonorModel_Click(object sender, RoutedEventArgs e) => ClearTextureFocus();

    private void ClearTextureFocus()
    {
        _textureFocus = null;
        _syncingTextureSelection = true;
        try
        {
            if (TextureList is not null)
                TextureList.SelectedIndex = -1;
            if (NativeDonorTextureList is not null)
                NativeDonorTextureList.SelectedIndex = -1;
        }
        finally
        {
            _syncingTextureSelection = false;
        }
        RefreshTextureFocusPreview();
    }

    private void RefreshTextureFocusPreview()
    {
        if (TextureFocusPanel is null || ReturnToDonorModelButton is null)
            return;
        var desired = _preparationStep && _fullDonorPreview is not null ? _textureFocus : null;
        if (Equals(_shownTextureFocus, desired) && ReferenceEquals(_textureFocusBase, _fullDonorPreview))
            return;
        _shownTextureFocus = desired;
        _textureFocusBase = _fullDonorPreview;
        if (desired is null)
        {
            DonorSelectionVisual.Content = _fullDonorPreview;
            TextureFocusPanel.Visibility = ReturnToDonorModelButton.Visibility = Visibility.Collapsed;
            TextureFocusImage.Source = null;
            return;
        }

        BitmapSource? image = null;
        string title = "Текстура";
        var group = new Model3DGroup();
        try
        {
            if (desired.NativeIndex is int nativeIndex)
            {
                var nativeMaterials = _editedNativeDonorMaterials ?? _nativeDonorPreviewMaterials;
                SmoTexture? texture = null;
                for (int meshIndex = 0; meshIndex < nativeMaterials.Count; meshIndex++)
                {
                    var matches = GetNativeTextureReferences(nativeMaterials[meshIndex])
                        .Where(reference => reference.ObjectIndex == nativeIndex).ToArray();
                    if (matches.Length == 0)
                        continue;
                    AddFocusedMesh(group, meshIndex);
                    texture ??= matches.Select(reference => reference.Texture).OfType<SmoTexture>().FirstOrDefault();
                }
                SmoDocument document = CurrentNativeDonorDocument;
                SmoObjectEntry entry = document.Objects[nativeIndex];
                title = entry.Name;
                if (texture is null)
                    SmoTextureDecoder.TryDecode(document, entry, out texture, out _);
                if (texture is not null)
                    image = SmoThumbnailRenderer.CreateTextureThumbnail(texture.Width, texture.Height,
                        texture.Bgra32Pixels, 256, 256);
            }
            else if (TextureList.Items.OfType<TextureResourceItem>()
                .FirstOrDefault(item => GetTextureFocusKey(item) == desired) is { } row)
            {
                title = row.Display;
                foreach (int meshIndex in row.SourceMeshKeys ?? [])
                    AddFocusedMesh(group, meshIndex);
                if (row.Image is { } texture)
                    image = ReadPreviewBitmap(texture);
                else if (row.Slot is { } slot && _replacementScene is { } scene)
                {
                    string? uri = scene.Materials[slot.Key].PreviewTextureUri;
                    if (uri is not null)
                        image = TryReadLinkedInputImage(_replacementPath, uri);
                }
            }
        }
        catch (Exception exception) when (exception is IOException or InvalidDataException or
            UnauthorizedAccessException or ArgumentException or InvalidOperationException or OverflowException)
        {
            SessionLog.Error("TEXTURE_FOCUS", exception);
        }

        group.Freeze();
        DonorSelectionVisual.Content = group;
        TextureFocusImage.Source = image;
        TextureFocusTitleText.Text = image is not null && group.Children.Count == 0
            ? $"{title} · без привязки" : title;
        TextureFocusPanel.Visibility = ReturnToDonorModelButton.Visibility = Visibility.Visible;
    }

    private void AddFocusedMesh(Model3DGroup group, int meshIndex)
    {
        if ((uint)meshIndex < (uint)_donorMeshVisuals.Length && _donorMeshVisuals[meshIndex] is { } mesh)
            group.Children.Add(mesh);
    }

    private static IEnumerable<SmoLoadedTexture> GetNativeTextureReferences(SmoModelReader.NativePreviewMaterial material)
    {
        foreach (var pass in material.Material?.Passes ?? [])
        foreach (var layer in pass.Layers)
        {
            if (layer.Texture is { } texture)
                yield return texture;
            foreach (var key in layer.Animation?.Keys ?? [])
                if (key.Texture is { } frame)
                    yield return frame;
        }
        foreach (var pass in material.Draw?.Passes ?? [])
        foreach (var stage in pass.Stages)
            if (stage.Texture is { } texture)
                yield return texture;
    }
}
