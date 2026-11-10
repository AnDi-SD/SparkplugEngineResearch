using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using Microsoft.Win32;
using SmoImporter.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private const string TextureFileFilter =
        "Изображения (*.png;*.jpg;*.jpeg;*.bmp;*.tga)|*.png;*.jpg;*.jpeg;*.bmp;*.tga";

    private static readonly IReadOnlyDictionary<int, ImportedTexture> NoTextureAssignments =
        new Dictionary<int, ImportedTexture>();

    private IReadOnlyDictionary<int, ImportedTexture> _manualTextureAssignments = NoTextureAssignments;
    private string? _objMaterialOverridePath;

    private bool ReplacementIsObj => string.Equals(Path.GetExtension(_replacementPath),
        ".obj", StringComparison.OrdinalIgnoreCase);

    private bool CanEditTextureInput => _preparationStep && !WizardBusy && !_isClosing && HasInputDonor;

    private bool CanReplaceTextureRow(object? item) => item switch
    {
        TextureResourceItem { CanReplace: true } external => !ReplacementIsSmo && TextureList.Items.Contains(external),
        NativeDonorTextureItem { CanReplace: true } native => ReplacementIsSmo && NativeDonorTextureList.Items.Contains(native),
        _ => false
    };

    private async void ReplaceTextureRow_Click(object sender, RoutedEventArgs e)
    {
        object? item = (sender as FrameworkElement)?.DataContext;
        if (!CanEditTextureInput || !CanReplaceTextureRow(item))
            return;
        var dialog = new OpenFileDialog { Filter = TextureFileFilter, CheckFileExists = true };
        if (dialog.ShowDialog(this) == true)
            await ReplaceTextureRowAsync(item!, dialog.FileName);
    }

    private async Task ReplaceTextureRowAsync(object item, string imagePath)
    {
        if (!CanEditTextureInput || !CanReplaceTextureRow(item))
            return;
        if (item is NativeDonorTextureItem native)
        {
            NativeDonorTextureList.SelectedItem = native;
            await ChangeNativeDonorTextureAsync(native.Slot.ObjectIndex, imagePath);
        }
        else if (item is TextureResourceItem { Slot: { } slot } external)
        {
            TextureList.SelectedItem = external;
            await RunTextureEditAsync(async () =>
            {
                ImportedTexture image = await Task.Run(() => ImportedTextureFileReader.Read(imagePath));
                if (_isClosing)
                    return;
                var assignments = new Dictionary<int, ImportedTexture>(_manualTextureAssignments)
                {
                    [slot.Key] = image
                };
                ApplyTextureAssignments(_externalTextureHistory, assignments);
            }, "Не удалось заменить текстуру. Проверьте изображение.");
        }
    }

    private string[]? GetTextureDropFiles(Border surface, DragEventArgs e)
    {
        if (!CanEditTextureInput || (e.AllowedEffects & DragDropEffects.Copy) == 0 ||
            !e.Data.GetDataPresent(DataFormats.FileDrop, autoConvert: false) ||
            e.Data.GetData(DataFormats.FileDrop, autoConvert: false) is not string[] { Length: > 0 } paths ||
            paths.Any(path => string.IsNullOrWhiteSpace(path) || !Path.IsPathFullyQualified(path) ||
                !IsSupportedExternalTexturePath(path)))
            return null;

        bool automatic = surface == AutomaticTextureDropSurface;
        return automatic || (paths.Length == 1 && CanReplaceTextureRow(surface.DataContext)) ? paths : null;
    }

    private void TextureDrop_DragOver(object sender, DragEventArgs e)
    {
        var surface = (Border)sender;
        bool accepted = GetTextureDropFiles(surface, e) is not null;
        e.Effects = accepted ? DragDropEffects.Copy : DragDropEffects.None;
        if (accepted)
            surface.BorderBrush = Brushes.NavajoWhite;
        else
            surface.ClearValue(Border.BorderBrushProperty);
        e.Handled = true;
    }

    private void TextureDrop_DragLeave(object sender, DragEventArgs e) =>
        ((Border)sender).ClearValue(Border.BorderBrushProperty);

    private async void TextureDrop_Drop(object sender, DragEventArgs e)
    {
        var surface = (Border)sender;
        surface.ClearValue(Border.BorderBrushProperty);
        string[]? files = GetTextureDropFiles(surface, e);
        e.Handled = true;
        e.Effects = files is null ? DragDropEffects.None : DragDropEffects.Copy;
        if (files is null)
            return;

        if (surface == AutomaticTextureDropSurface)
            await AddAutomaticTextureFilesAsync(files);
        else
            await ReplaceTextureRowAsync(surface.DataContext, files[0]);
    }

    private Task AddAutomaticTextureFilesAsync(string[] paths) => RunTextureEditAsync(async () =>
    {
        if (ReplacementIsSmo)
        {
            var slotsByName = _nativeDonorTextureSlots.Where(slot => slot.CanReplace)
                .ToLookup(slot => IsSupportedExternalTexturePath(slot.Name)
                    ? Path.GetFileNameWithoutExtension(slot.Name)
                    : Path.GetFileName(slot.Name), StringComparer.OrdinalIgnoreCase);
            var edits = _nativeDonorTextureEdits.ToDictionary(edit => edit.ObjectIndex);
            var matches = paths.Distinct(StringComparer.OrdinalIgnoreCase)
                .Select(path => (Path: path, Slots: slotsByName[Path.GetFileNameWithoutExtension(path)].ToArray()))
                .Where(candidate => candidate.Slots.Length == 1)
                .GroupBy(candidate => candidate.Slots[0].ObjectIndex)
                .Where(group => group.Count() == 1 && (!edits.TryGetValue(group.Key, out var edit) || edit.Automatic))
                .Select(group => (ObjectIndex: group.Key, group.Single().Path)).ToArray();
            if (matches.Length == 0)
            {
                _inputError = "Нет однозначных совпадений по имени. Можно бросить файл на нужную строку.";
                return;
            }

            var imported = await Task.Run(() => matches.Select(match => new NativeDonorTextureEdit(
                match.ObjectIndex, ImportedTextureFileReader.Read(match.Path), Automatic: true)).ToArray());
            if (_isClosing)
                return;
            foreach (var edit in imported)
                edits[edit.ObjectIndex] = edit;
            await RestoreNativeDonorTexturesAsync(edits.Values.OrderBy(edit => edit.ObjectIndex).ToArray());
            if (matches.Length < paths.Length)
                _inputError = "Часть файлов осталась без назначения. Можно выбрать текстуру вручную.";
        }
        else
        {
            var (textures, _) = await Task.Run(() => MergeExternalTextureFiles(paths));
            if (!_isClosing)
                ApplyTextureOverrides(textures);
        }
    }, "Не удалось распределить текстуры. Проверьте файлы.");

    private void RefreshObjMaterialControls()
    {
        if (ObjMaterialPanel is null)
            return;
        ObjMaterialPanel.Visibility = ReplacementIsObj ? Visibility.Visible : Visibility.Collapsed;
        if (!ReplacementIsObj)
            return;
        var libraries = _baseReplacementScene?.ObjMaterialLibraryPaths ?? [];
        ObjMaterialPathText.Text = libraries.Count == 0 ? "MTL не найден" :
            string.Join(", ", libraries.Select(Path.GetFileName));
        ObjMaterialPathText.ToolTip = libraries.Count == 0 ? null : string.Join(Environment.NewLine, libraries);
        ChooseObjMaterialButton.IsEnabled = CanEditTextureInput;
        ResetObjMaterialButton.Visibility = _objMaterialOverridePath is null ? Visibility.Collapsed : Visibility.Visible;
        ResetObjMaterialButton.IsEnabled = CanEditTextureInput;
    }

    private async void ChooseObjMaterial_Click(object sender, RoutedEventArgs e)
    {
        if (!CanEditTextureInput || !ReplacementIsObj)
            return;
        var dialog = new OpenFileDialog
        {
            Filter = "Материалы OBJ (*.mtl)|*.mtl",
            CheckFileExists = true,
            InitialDirectory = Path.GetDirectoryName(_objMaterialOverridePath ?? _replacementPath)
        };
        if (dialog.ShowDialog(this) == true)
            await ChangeObjMaterialAsync(dialog.FileName);
    }

    private async void ResetObjMaterial_Click(object sender, RoutedEventArgs e)
    {
        if (CanEditTextureInput && ReplacementIsObj && _objMaterialOverridePath is not null)
            await ChangeObjMaterialAsync(null);
    }

    private Task ChangeObjMaterialAsync(string? path) =>
        RunTextureEditAsync(() => RestoreObjMaterialAsync(path, _externalTextureHistory, NoTextureAssignments),
            "Не удалось применить MTL. Проверьте файл и имена материалов.");

    private async Task RestoreObjMaterialAsync(string? materialPath, ImportedTexture[] externalTextures,
        IReadOnlyDictionary<int, ImportedTexture> assignments)
    {
        if (!ReplacementIsObj || _replacementPath is not { } donorPath)
            throw new InvalidOperationException("Выберите модель OBJ.");
        var result = await Task.Run(() =>
        {
            ImportedScene source = materialPath is null
                ? ObjModelReader.Read(donorPath)
                : ObjModelReader.ReadWithMaterialLibrary(donorPath, materialPath);
            var catalog = ImportedTextureCatalog.ResolveExternalOverrides(source, externalTextures, assignments);
            return (Source: source, Catalog: catalog);
        });
        if (_isClosing)
            return;

        ClearTextureFocus();
        if (ReferenceEquals(_shownInputDonor, _baseReplacementScene))
            _shownInputDonor = result.Source;
        _objMaterialOverridePath = materialPath;
        _externalTextures.Clear();
        _externalTextures.AddRange(externalTextures);
        _externalTextureHistory = externalTextures;
        _manualTextureAssignments = assignments;
        ApplyExternalReplacementState(donorPath, result.Source, result.Catalog.EffectiveScene,
            result.Catalog, resetTransform: false);
    }

    private async Task RunTextureEditAsync(Func<Task> apply, string failureMessage)
    {
        if (!CanEditTextureInput)
            return;
        InputStepState previous = CaptureInputStep();
        _inputActionRunning = true;
        _inputError = null;
        ControlsScrollViewer.IsEnabled = false;
        RefreshWizardState();
        try
        {
            await apply();
            if (!_isClosing)
                RecordInputStep();
        }
        catch (Exception exception) when (!_isClosing)
        {
            if (CaptureInputStep() != previous)
                await RecoverInputFilesAsync(previous);
            if (!_isClosing)
                ReportInputError(exception, failureMessage);
        }
        catch (Exception) when (_isClosing) { }
        finally
        {
            _inputActionRunning = false;
            if (!_isClosing)
            {
                ControlsScrollViewer.IsEnabled = !WizardBusy;
                RefreshState();
            }
        }
    }
}
