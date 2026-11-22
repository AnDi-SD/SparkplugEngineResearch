using Microsoft.Win32;
using SmoImporter.Core;
using SmoLVLcreator.Core;
using System.IO;
using System.Windows;

namespace SmoLVLcreator.Gui;

public partial class ModelImportWindow : Window
{
    private readonly string _modelPath;
    private readonly bool _isObj;
    private readonly CancellationTokenSource _lifetime = new();
    private TextureDraft _draft;
    private ImportedTextureCatalogResult _catalog;
    private bool _busy;
    private bool _closed;

    public ModelImportWindow(string modelPath, ImportedScene source)
    {
        _modelPath = modelPath;
        _isObj = Path.GetExtension(modelPath).Equals(".obj", StringComparison.OrdinalIgnoreCase);
        _draft = new TextureDraft(source, [], new Dictionary<int, ImportedTexture>(), null);
        _catalog = ImportedTextureCatalog.ResolveExternalOverrides(source, []);
        InitializeComponent();
        ModelNameText.Text = Path.GetFileName(modelPath);
        ModelNameText.ToolTip = modelPath;
        MaterialLibraryPanel.Visibility = _isObj ? Visibility.Visible : Visibility.Collapsed;
        RefreshOptions();
    }

    internal ImportedScene EffectiveScene => _catalog.EffectiveScene;

    protected override void OnClosed(EventArgs e)
    {
        _closed = true;
        _lifetime.Cancel();
        _lifetime.Dispose();
        base.OnClosed(e);
    }

    private async void ChooseMtl_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Filter = LevelText.Get("Dialog.Import.MtlFilter"),
            CheckFileExists = true,
            InitialDirectory = Path.GetDirectoryName(_draft.MaterialLibraryPath ?? _modelPath)
        };
        if (dialog.ShowDialog(this) == true)
            await ChangeMaterialLibraryAsync(dialog.FileName);
    }

    private async void ResetMtl_Click(object sender, RoutedEventArgs e) =>
        await ChangeMaterialLibraryAsync(null);

    internal Task ChangeMaterialLibraryAsync(string? path)
    {
        if (!_isObj)
            throw new InvalidOperationException(LevelText.Get("Dialog.Import.MtlOnlyObj"));

        return EditAsync((draft, token) =>
        {
            ImportedScene source = path is null
                ? ObjModelReader.Read(_modelPath, token)
                : ObjModelReader.ReadWithMaterialLibrary(_modelPath, path, token);
            // Slot keys refer to the previous MTL's material indices.
            return new TextureDraft(source, draft.Automatic, new Dictionary<int, ImportedTexture>(), path);
        }, "Dialog.Import.MtlEditError");
    }

    private async void AutoAssign_Click(object sender, RoutedEventArgs e)
    {
        var dialog = ImageDialog(multiselect: true);
        if (dialog.ShowDialog(this) == true)
            await ApplyImagesAsync(dialog.FileNames);
    }

    private async void ResetAuto_Click(object sender, RoutedEventArgs e) =>
        await ApplyImagesAsync([]);

    private async void AssignImage_Click(object sender, RoutedEventArgs e)
    {
        if ((sender as FrameworkElement)?.DataContext is not TextureRow row)
            return;
        var dialog = ImageDialog(multiselect: false);
        if (dialog.ShowDialog(this) == true)
            await ApplyImagesAsync(dialog.FileNames, row.Key);
    }

    private async void ClearImage_Click(object sender, RoutedEventArgs e)
    {
        if ((sender as FrameworkElement)?.DataContext is TextureRow row)
        {
            await EditAsync((draft, _) =>
            {
                var manual = new Dictionary<int, ImportedTexture>(draft.Manual);
                manual.Remove(row.Key);
                return draft with { Manual = manual };
            }, "Dialog.Import.TextureEditError");
        }
    }

    internal Task ApplyImagesAsync(IReadOnlyList<string> paths, int? slotKey = null) =>
        EditAsync((draft, token) =>
        {
            if (slotKey is int key)
            {
                var manual = new Dictionary<int, ImportedTexture>(draft.Manual)
                {
                    [key] = ImportedTextureFileReader.Read(paths.Single())
                };
                return draft with { Manual = manual };
            }

            var automatic = new List<ImportedTexture>();
            foreach (string path in paths.Select(Path.GetFullPath).Distinct(StringComparer.OrdinalIgnoreCase))
            {
                token.ThrowIfCancellationRequested();
                automatic.Add(ImportedTextureFileReader.Read(path));
            }
            return draft with { Automatic = automatic };
        }, "Dialog.Import.TextureEditError");

    private OpenFileDialog ImageDialog(bool multiselect) => new()
    {
        Filter = LevelText.Get("Dialog.Import.ImageFilter"),
        CheckFileExists = true,
        Multiselect = multiselect,
        InitialDirectory = Path.GetDirectoryName(_modelPath)
    };

    private async Task EditAsync(Func<TextureDraft, CancellationToken, TextureDraft> edit, string errorKey)
    {
        if (_busy || _closed)
            return;
        _busy = true;
        TextureEditPanel.IsEnabled = ContinueButton.IsEnabled = false;
        EditStatusText.Text = LevelText.Get("Dialog.Import.Reading");
        CancellationToken cancellationToken = _lifetime.Token;
        TextureDraft original = _draft;
        try
        {
            (TextureDraft updated, ImportedTextureCatalogResult resolved) = await Task.Run(() =>
            {
                TextureDraft draft = edit(original, cancellationToken);
                cancellationToken.ThrowIfCancellationRequested();
                return (draft, ImportedTextureCatalog.ResolveExternalOverrides(draft.Source, draft.Automatic, draft.Manual));
            }, cancellationToken);
            if (_closed)
                return;
            _draft = updated;
            _catalog = resolved;
            RefreshOptions();
            EditStatusText.Text = string.Empty;
        }
        catch (Exception exception)
        {
            if (!_closed)
            {
                EditStatusText.Text = LevelText.Get(errorKey);
                EditStatusText.ToolTip = LevelText.Error(exception.Message);
            }
        }
        finally
        {
            _busy = false;
            if (!_closed)
                TextureEditPanel.IsEnabled = ContinueButton.IsEnabled = true;
        }
    }

    private void RefreshOptions()
    {
        string[] libraries = _draft.Source.ObjMaterialLibraryPaths.ToArray();
        MaterialLibraryText.Text = libraries.Length == 0
            ? LevelText.Get("Dialog.Import.NoMaterialLibrary")
            : string.Join(", ", libraries.Select(Path.GetFileName));
        MaterialLibraryText.ToolTip = libraries.Length == 0 ? null : string.Join(Environment.NewLine, libraries);
        ResetMtlButton.Visibility = _draft.MaterialLibraryPath is null ? Visibility.Collapsed : Visibility.Visible;

        ImportedScene scene = EffectiveScene;
        SlotList.ItemsSource = _catalog.Slots.Select(slot =>
        {
            ImportedTexture? image = slot.EffectiveTextureIndex < 0 ? null : scene.Textures[slot.EffectiveTextureIndex];
            string? imageTitle = image is null ? null : Path.GetFileName(image.SourcePath) ?? image.Name;
            return new TextureRow(
                slot.Key,
                slot.Name,
                string.Join(", ", slot.MaterialIndices.Select(index => scene.Materials[index].Name)),
                imageTitle is null ? LevelText.Get("Dialog.Import.NoImage") : slot.IsExplicit
                    ? LevelText.Get("Dialog.Import.ManualImage", imageTitle) : imageTitle,
                image?.SourcePath,
                slot.IsExplicit);
        }).ToArray();
        bool hasSlots = _catalog.Slots.Count != 0;
        AutoAssignButton.IsEnabled = hasSlots;
        ResetAutoButton.Visibility = _draft.Automatic.Count == 0 ? Visibility.Collapsed : Visibility.Visible;
        SlotList.Visibility = hasSlots ? Visibility.Visible : Visibility.Collapsed;
        EmptyTexturesText.Visibility = hasSlots ? Visibility.Collapsed : Visibility.Visible;
        string[] unused = _catalog.UnusedExternalTextures
            .Select(texture => Path.GetFileName(texture.SourcePath) ?? texture.Name)
            .ToArray();
        UnusedImagesText.Text = LevelText.Get("Dialog.Import.UnusedImages", string.Join(", ", unused));
        UnusedImagesText.ToolTip = UnusedImagesText.Text;
        UnusedImagesText.Visibility = unused.Length == 0 ? Visibility.Collapsed : Visibility.Visible;
        EditStatusText.ToolTip = null;
    }

    private void Continue_Click(object sender, RoutedEventArgs e) => DialogResult = true;

    private void Cancel_Click(object sender, RoutedEventArgs e) => DialogResult = false;

    private sealed record TextureDraft(
        ImportedScene Source,
        IReadOnlyList<ImportedTexture> Automatic,
        IReadOnlyDictionary<int, ImportedTexture> Manual,
        string? MaterialLibraryPath);

    private sealed record TextureRow(
        int Key,
        string Name,
        string Materials,
        string Image,
        string? ImagePath,
        bool IsManual);
}
