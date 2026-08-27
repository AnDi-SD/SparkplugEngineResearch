using Microsoft.Win32;
using SmoLVLcreator.Core;
using SmoViewer.Core;
using System.Globalization;
using System.IO;
using System.Numerics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace SmoLVLcreator.Gui;

public partial class MainWindow : Window
{
    private static readonly Brush HealthyBrush =
        new SolidColorBrush(Color.FromRgb(113, 190, 132));
    private static readonly Brush NoticeBrush =
        new SolidColorBrush(Color.FromRgb(230, 154, 70));
    private static readonly Brush ErrorBrush =
        new SolidColorBrush(Color.FromRgb(229, 107, 107));

    private readonly List<EditorAssetItem> _allAssets = [];
    private SmoLevelWorkspace? _workspace;
    private SmoLevelDocument? _document;
    private string? _savePath;
    private GridLength _inspectorWidth = new(330);
    private GridLength _outlinerWidth = new(292);
    private GridLength _assetHeight = new(205);
    private bool _syncingSelection;
    private bool _updatingPositionEditor;
    private bool _isSaving;
    private bool _isExporting;
    private bool _isBusy;
    private CancellationTokenSource? _saveCancellation;
    private SmoLVLcreatorSettings _settings = SmoLVLcreatorSettingsStore.Load();
    private string _editorTool = "Выбор";

    public MainWindow()
    {
        InitializeComponent();
        InitializeGpuViewport();
        InitializeEmptyWorkspace();
        Loaded += MainWindow_Loaded;
    }

    private void InitializeEmptyWorkspace()
    {
        ResetRenderScene();
        _workspace = null;
        _document = null;
        _savePath = null;
        _allAssets.Clear();
        DocumentNameText.Text = "Уровень не открыт";
        ViewportEmptyTitleText.Text = _gpuViewportFailureReason is null
            ? "Уровень не открыт"
            : "Viewport недоступен";
        ViewportStateText.Text = _gpuViewportFailureReason is null
            ? "Откройте SMO-файл, чтобы начать работу с уровнем."
            : $"GPU viewport недоступен.\n{_gpuViewportFailureReason}";
        ViewportSelectionText.Text = "NO SELECTION";
        ClearInspector();
        SetWorkspaceUiState(hasDocument: false);
        UpdateCommandAvailability();
        ApplyAssetFilter();
        BuildSceneTree();
        UpdateStatistics();
        StatusText.Text = "Откройте SMO-файл для начала работы";
    }

    private void SetWorkspaceUiState(bool hasDocument)
    {
        EditorToolsPanel.IsEnabled = hasDocument;
        ViewportModePanel.IsEnabled = hasDocument;
        WorldTransformSpaceButton.IsEnabled = hasDocument;
        LocalTransformSpaceButton.IsEnabled = hasDocument;
        CatalogImportModelButton.IsEnabled = hasDocument;
        if (AssetList.ContextMenu is not null)
            AssetList.ContextMenu.IsEnabled = hasDocument;
        AssetFilterBox.IsEnabled = hasDocument;
        OutlinerFilterBox.IsEnabled = hasDocument;
        InspectorConfirmationBadge.Visibility = hasDocument
            ? Visibility.Visible
            : Visibility.Collapsed;
        ViewportSelectionBadge.Visibility = hasDocument
            ? Visibility.Visible
            : Visibility.Collapsed;
        ViewportAxisLegend.Visibility = hasDocument
            ? Visibility.Visible
            : Visibility.Collapsed;
        ViewportControlsHint.Visibility = hasDocument
            ? Visibility.Visible
            : Visibility.Collapsed;
    }

    private void ClearInspector(string? context = null)
    {
        SelectedNameText.Text = "Ничего не выбрано";
        SelectedPathText.Text = context ?? "—";
        ObjectIdentityText.Text = "—";
        ObjectIdText.Text = "—";
        SetPositionEditor(null);
        SetRotationScaleEditor(null, null);
        PlacementCountText.Text = "—";
        GeometryCountText.Text = "—";
        VertexLayoutText.Text = "—";
        ChannelsText.Text = "—";
        TextureNameText.Text = "—";
        MaterialText.Text = "—";
        BindingsText.Text = "—";
        DiagnosticsText.Text = _workspace is null
            ? "Откройте SMO-файл для начала работы."
            : "Выберите объект в сцене, структуре уровня или каталоге.";
        DiagnosticsText.Foreground = _workspace is null ? NoticeBrush : HealthyBrush;
        RawDataText.Text = _workspace is null
            ? "Нет данных: откройте SMO и выберите объект."
            : "Выберите объект, чтобы увидеть его прямые SBOO-поля.";
    }

    private async void OpenLevel_Click(object sender, RoutedEventArgs e)
    {
        if (_isBusy || !ConfirmDiscardProjectChanges())
            return;
        var dialog = new OpenFileDialog
        {
            Title = "Открыть уровень или проект SmoLVLcreator",
            Filter = "Уровень или проект (*.smo;*.smolvlproj)|*.smo;*.smolvlproj|" +
                     "Sparkplug model or level (*.smo)|*.smo|" +
                     "SmoLVLcreator project (*.smolvlproj)|*.smolvlproj|" +
                     "Все файлы (*.*)|*.*",
            CheckFileExists = true,
            Multiselect = false
        };
        if (dialog.ShowDialog(this) != true)
            return;

        if (Path.GetExtension(dialog.FileName).Equals(
                ".smolvlproj",
                StringComparison.OrdinalIgnoreCase))
            await LoadProjectAsync(dialog.FileName);
        else
            await LoadLevelAsync(dialog.FileName);
    }

    private async void MainWindow_Loaded(object sender, RoutedEventArgs e)
    {
        Loaded -= MainWindow_Loaded;
        string? startupPath = Environment.GetCommandLineArgs()
            .Skip(1)
            .FirstOrDefault(path =>
                System.IO.Path.GetExtension(path) is string extension &&
                (extension.Equals(".smo", StringComparison.OrdinalIgnoreCase) ||
                 extension.Equals(".smolvlproj", StringComparison.OrdinalIgnoreCase)));
        if (startupPath is not null && System.IO.File.Exists(startupPath))
        {
            if (System.IO.Path.GetExtension(startupPath).Equals(
                    ".smolvlproj",
                    StringComparison.OrdinalIgnoreCase))
                await LoadProjectAsync(startupPath);
            else
                await LoadLevelAsync(startupPath);
        }
    }

    private async Task LoadLevelAsync(string path)
    {
        try
        {
            string fullPath = Path.GetFullPath(path);
            SetBusy(true, $"Создание проекта из {Path.GetFileName(fullPath)}…");
            (SmoProject Project, SmoLevelWorkspace Workspace) loaded =
                await Task.Run(() =>
                {
                    SmoProject project = SmoProject.Import(
                        SmoDocument.Load(fullPath));
                    return (project, BuildProjectPreviewWorkspace(project));
                });
            _project = loaded.Project;
            _projectSession =
                new SmoProjectSession(loaded.Project);
            _projectSession.MarkSaved();
            _projectPath = null;
            AttachWorkspace(loaded.Workspace, savePath: null);
            ApplyProjectUiState();
            SetBusy(false,
                $"{Path.GetFileName(fullPath)} открыт как проект · " +
                $"{loaded.Project.Objects.Count:N0} объектов · исходный SMO не изменяется");
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось открыть уровень");
            MessageBox.Show(
                this,
                exception.Message,
                "SmoLVLcreator — ошибка открытия",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
    }

    private void AttachWorkspace(
        SmoLevelWorkspace workspace,
        string? savePath,
        bool preserveEditorState = false)
    {
        if (_document is not null)
            _document.Changed -= LevelDocument_Changed;
        _workspace = workspace;
        _document = new SmoLevelDocument(workspace);
        _document.Changed += LevelDocument_Changed;
        if (preserveEditorState)
        {
            _selectedEntities.RemoveWhere(id => !_document.TryGetEntity(id, out _));
            _hiddenEntities.RemoveWhere(id => !_document.TryGetEntity(id, out _));
            _selectedPendingPlacement = null;
            _pendingPlacementByScene.Clear();
        }
        _savePath = savePath;
        _allAssets.Clear();
        _allAssets.AddRange(workspace.Assets.Select(EditorAssetItem.FromAsset));
        SetWorkspaceUiState(hasDocument: true);
        ClearInspector(workspace.DisplayName);
        ViewportEmptyTitleText.Text = _gpuViewportFailureReason is not null
            ? "Viewport недоступен"
            : workspace.PreparedScene.Meshes.Count > 0
                ? "Уровень загружается"
                : "Нет отображаемой геометрии";
        LoadRenderScene(
            workspace.PreparedScene,
            preserveEditorSelection: preserveEditorState);

        UpdateDocumentCaption();
        if (_gpuRendererAvailable && workspace.PreparedScene.Meshes.Count > 0)
        {
            ViewportStateText.Text =
                $"Каталог уровня загружен: {workspace.Assets.Count:N0} моделей, " +
                $"{workspace.PlacementCount:N0} размещений, " +
                $"{workspace.Collisions.Count:N0} collision shapes.";
        }
        AssetFilterBox.Clear();
        OutlinerFilterBox.Clear();
        ApplyAssetFilter();
        BuildSceneTree();
        if (!preserveEditorState && AssetList.Items.Count > 0)
            AssetList.SelectedIndex = 0;
        else
            ViewportSelectionText.Text = "NO SELECTION";
        UpdateStatistics();
    }

    private void SetBusy(bool busy, string message)
    {
        _isBusy = busy;
        Mouse.OverrideCursor = busy ? Cursors.Wait : null;
        UpdateCommandAvailability();
        StatusText.Text = message;
    }

    private void UpdateCommandAvailability()
    {
        if (SaveMenuItem is null)
            return;
        bool hasDocument = _document is not null;
        bool hasVisualSelection = hasDocument && _selectedEntities.Any(id =>
            !_hiddenEntities.Contains(id) &&
            _document!.TryGetEntity(id, out SmoLevelEntity? entity) &&
            entity!.Parts.Count > 0);
        bool hasGeneratedCollisionSelection = hasDocument &&
            _selectedEntities.Any(id =>
                _document!.GeneratedCollisions.ContainsKey(id));
        bool hasDeletableEntitySelection = hasDocument &&
            _selectedEntities.Any(id =>
                _document!.TryGetEntity(id, out _) &&
                !_document.RemovedEntityIds.Contains(id));
        if (CreateCollisionButton is not null)
        {
            CreateCollisionButton.IsEnabled = !_isBusy && hasDocument &&
                (_selectedPendingPlacement is not null || hasVisualSelection ||
                 hasGeneratedCollisionSelection);
            CreateCollisionButton.Content = hasGeneratedCollisionSelection
                ? "Регенерировать коллизию"
                : "Создать коллизию";
        }
        if (DeleteSelectedButton is not null)
        {
            DeleteSelectedButton.IsEnabled = !_isBusy && hasDocument &&
                (_selectedPendingPlacement is not null ||
                 hasDeletableEntitySelection);
        }
        if (ShowAllEntitiesButton is not null)
            ShowAllEntitiesButton.IsEnabled = !_isBusy && _hiddenEntities.Count > 0;
        if (CatalogImportModelButton is not null)
            CatalogImportModelButton.IsEnabled = !_isBusy && hasDocument;
        if (IsolateEntitiesButton is not null)
        {
            IsolateEntitiesButton.IsEnabled = !_isBusy && hasDocument &&
                (_selectedEntities.Count > 0 || _selectedPendingPlacement is not null);
        }
        if (HideEntitiesButton is not null)
        {
            HideEntitiesButton.IsEnabled = !_isBusy && hasDocument &&
                _selectedEntities.Any(id => !_hiddenEntities.Contains(id));
        }
        if (LinkCollisionButton is not null)
        {
            SmoLevelEntity[] selectedEntities = hasDocument
                ? _selectedEntities
                    .Where(id => _document!.TryGetEntity(id, out _))
                    .Select(_document!.GetEntity)
                    .ToArray()
                : [];
            SmoLevelEntity[] visuals = selectedEntities.Where(entity =>
                entity.Kind == SmoLevelEntityKind.Visual).ToArray();
            SmoLevelEntity[] collisions = selectedEntities.Where(entity =>
                entity.Kind == SmoLevelEntityKind.Collision).ToArray();
            bool exactPair = selectedEntities.Length == 2 &&
                             visuals.Length == 1 && collisions.Length == 1;
            bool linked = exactPair && _document!.GetCollisionLinks(visuals[0].Id)
                .Any(link => link.CollisionEntityId == collisions[0].Id);
            LinkCollisionButton.IsEnabled = !_isBusy && exactPair && !linked;
            UnlinkCollisionButton.IsEnabled = !_isBusy && exactPair && linked;
        }
        if (CopyPlacementMenuItem is not null)
        {
            bool hasCopyableSelection = hasDocument &&
                (_selectedPendingPlacement is not null ||
                 _selectedEntities.Any(id =>
                     _document!.TryGetEntity(id, out SmoLevelEntity? entity) &&
                     entity!.Kind == SmoLevelEntityKind.Visual &&
                     !_document.RemovedEntityIds.Contains(id)));
            CopyPlacementMenuItem.IsEnabled = !_isBusy && hasCopyableSelection;
            DuplicatePlacementMenuItem.IsEnabled =
                !_isBusy && hasCopyableSelection;
            PastePlacementMenuItem.IsEnabled = !_isBusy && hasDocument &&
                _placementClipboard?.SourceDocument == _document;
        }
        OpenMenuItem.IsEnabled = !_isBusy;
        SaveMenuItem.IsEnabled = !_isBusy && hasDocument;
        SaveAsMenuItem.IsEnabled = !_isBusy && hasDocument;
        ValidateInGameMenuItem.IsEnabled = !_isBusy && hasDocument;
        ExportMenuItem.IsEnabled = !_isBusy && hasVisualSelection;
        ExportAsMenuItem.IsEnabled = !_isBusy && hasVisualSelection;
        int selectedCount = hasDocument
            ? _selectedEntities.Count(id =>
                !_hiddenEntities.Contains(id) &&
                _document!.TryGetEntity(id, out SmoLevelEntity? entity) &&
                entity!.Parts.Count > 0)
            : 0;
        ExportMenuItem.Header = selectedCount > 0
            ? $"Экспорт · {selectedCount:N0} объект(а)"
            : "Экспорт";
        UpdateProjectCommandAvailability();
    }

    private async void SaveLevel_Click(object sender, RoutedEventArgs e)
    {
        if (HasProject)
        {
            if (_projectPath is null)
                await SaveProjectAsAsync();
            else
                await SaveProjectAsync(_projectPath);
            return;
        }
        if (_savePath is not null)
            await SaveLevelAsync(_savePath);
    }

    private async void SaveLevelAs_Click(object sender, RoutedEventArgs e)
    {
        if (HasProject)
        {
            await SaveProjectAsAsync();
            return;
        }
        if (_workspace is null)
            return;

        string sourcePath = _savePath ?? _workspace.SourcePath;
        string directory = System.IO.Path.GetDirectoryName(sourcePath) ?? string.Empty;
        string stem = System.IO.Path.GetFileNameWithoutExtension(sourcePath);
        var dialog = new SaveFileDialog
        {
            Title = "Сохранить изменённый SMO-уровень",
            Filter = "Sparkplug model or level (*.smo)|*.smo|Все файлы (*.*)|*.*",
            InitialDirectory = directory,
            FileName = stem + "_edited.smo",
            AddExtension = true,
            DefaultExt = ".smo",
            OverwritePrompt = true
        };
        if (dialog.ShowDialog(this) == true)
            await SaveLevelAsync(dialog.FileName);
    }

    private async void ExportSelection_Click(object sender, RoutedEventArgs e) =>
        await ExportSelectionQuickAsync();

    private async void ExportSelectionAs_Click(object sender, RoutedEventArgs e) =>
        await ExportSelectionAsAsync();

    private async Task ExportSelectionQuickAsync()
    {
        if (!TryGetVisualSelection(out SmoLevelEntityId[] selection))
            return;
        string directory = Path.GetFullPath(_settings.ExportDirectory);
        string fileName = BuildSelectionFileStem(selection) +
            SmoLevelExportService.GetExtension(_settings.ExportFormat);
        await ExportSelectionAsync(
            selection,
            Path.Combine(directory, fileName),
            _settings.ExportFormat);
    }

    private async Task ExportSelectionAsAsync()
    {
        if (!TryGetVisualSelection(out SmoLevelEntityId[] selection))
            return;
        DirectoryInfo? configuredDirectory = null;
        try
        {
            configuredDirectory = new DirectoryInfo(_settings.ExportDirectory);
        }
        catch
        {
        }
        string extension = SmoLevelExportService.GetExtension(
            _settings.ExportFormat);
        var dialog = new SaveFileDialog
        {
            Title = $"Экспортировать {selection.Length:N0} выбранных объект(а)",
            Filter = "glTF Binary (*.glb)|*.glb|Autodesk FBX (*.fbx)|*.fbx|Wavefront OBJ (*.obj)|*.obj",
            FilterIndex = _settings.ExportFormat switch
            {
                SmoLevelExportFormat.Fbx => 2,
                SmoLevelExportFormat.Obj => 3,
                _ => 1
            },
            InitialDirectory = configuredDirectory?.Exists == true
                ? configuredDirectory.FullName
                : Path.GetDirectoryName(_savePath ?? _workspace?.SourcePath),
            FileName = BuildSelectionFileStem(selection),
            AddExtension = true,
            DefaultExt = extension,
            OverwritePrompt = true
        };
        if (dialog.ShowDialog(this) != true)
            return;
        SmoLevelExportFormat format = dialog.FilterIndex switch
        {
            2 => SmoLevelExportFormat.Fbx,
            3 => SmoLevelExportFormat.Obj,
            _ => SmoLevelExportFormat.Glb
        };
        await ExportSelectionAsync(selection, dialog.FileName, format);
    }

    private async Task ExportSelectionAsync(
        SmoLevelEntityId[] selection,
        string outputPath,
        SmoLevelExportFormat format)
    {
        if (_document is null || _isExporting)
            return;
        SmoLevelDocument document = _document;
        _isExporting = true;
        try
        {
            SetBusy(
                true,
                $"Экспорт {selection.Length:N0} объект(а) в {format.ToString().ToUpperInvariant()}…");
            SmoLevelExportResult result = await Task.Run(() =>
                SmoLevelExportService.Export(
                    document,
                    selection,
                    outputPath,
                    format));
            SetBusy(
                false,
                $"Экспортировано · {result.EntityCount:N0} объект(а) · " +
                $"{result.PlacementCount:N0} частей · {Path.GetFileName(result.OutputPath)}" +
                (result.Warnings.Count > 0
                    ? $" · предупреждений: {result.Warnings.Count:N0}"
                    : string.Empty));
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось экспортировать выбранные объекты");
            MessageBox.Show(
                this,
                exception.Message,
                "SmoLVLcreator — ошибка экспорта",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
        finally
        {
            _isExporting = false;
            UpdateCommandAvailability();
        }
    }

    private bool TryGetVisualSelection(out SmoLevelEntityId[] selection)
    {
        selection = _document is null
            ? []
            : _selectedEntities
                .Where(id => !_hiddenEntities.Contains(id) &&
                    _document.GetEntity(id).Parts.Count > 0)
                .ToArray();
        if (selection.Length > 0)
            return true;
        StatusText.Text = "Для экспорта выберите один или несколько визуальных объектов";
        return false;
    }

    private string BuildSelectionFileStem(IReadOnlyList<SmoLevelEntityId> selection)
    {
        string level = Path.GetFileNameWithoutExtension(
            _savePath ?? _workspace?.SourcePath ?? "level");
        string suffix = selection.Count == 1 && _document is not null
            ? _document.GetEntity(selection[0]).Name
            : $"selection_{selection.Count}";
        string raw = $"{level}_{suffix}";
        char[] invalid = Path.GetInvalidFileNameChars();
        string safe = new(raw.Select(character =>
            invalid.Contains(character) ? '_' : character).ToArray());
        return string.IsNullOrWhiteSpace(safe) ? "selection" : safe.Trim();
    }

    private void Settings_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new SettingsWindow(_settings) { Owner = this };
        if (dialog.ShowDialog() != true || dialog.Result is null)
            return;
        try
        {
            SmoLVLcreatorSettingsStore.Save(dialog.Result);
            _settings = dialog.Result;
            StatusText.Text =
                $"Настройки сохранены · быстрый экспорт: " +
                $"{_settings.ExportFormat.ToString().ToUpperInvariant()} · " +
                _settings.ExportDirectory;
        }
        catch (Exception exception)
        {
            MessageBox.Show(
                this,
                exception.Message,
                "SmoLVLcreator — ошибка сохранения настроек",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
    }

    private void ValidateInGame_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null || _workspace is null || _isBusy)
            return;
        var window = new LevelNativeValidationWindow(
            _document,
            _workspace.SourcePath)
        {
            Owner = this
        };
        window.ShowDialog();
    }

    private void Exit_Click(object sender, RoutedEventArgs e) => Close();

    private void UndoMenu_Click(object sender, RoutedEventArgs e) => UndoLevelEdit();

    private void RedoMenu_Click(object sender, RoutedEventArgs e) => RedoLevelEdit();

    private async Task SaveLevelAsync(string path)
    {
        if (_document is null || _isSaving)
            return;
        string fullPath = System.IO.Path.GetFullPath(path);
        if (!_document.IsModified &&
            string.Equals(_savePath, fullPath, StringComparison.OrdinalIgnoreCase))
        {
            bool needsCollisionRepair = await Task.Run(() =>
                SmoLevelSaveService.NeedsCollisionRegistryRepair(_document) ||
                SmoLevelSaveService.NeedsCollisionMeshCompatibilityRepair(_document));
            if (!needsCollisionRepair)
            {
                StatusText.Text = "Нет изменений для сохранения";
                return;
            }
            StatusText.Text = "Восстановление реестра коллизий…";
        }

        SmoLevelDocument document = _document;
        string diagnosticLogPath = SmoLevelSaveService.GetLogPath(fullPath);
        SmoLevelSaveMemoryAssessment memory =
            SmoLevelIsolatedSaveClient.AssessMemory(document);
        if (memory.ShouldWarn)
        {
            MessageBoxResult choice = MessageBox.Show(
                this,
                $"По предварительной оценке сохранению может понадобиться около " +
                $"{memory.EstimatedPeakMiB:N0} МиБ памяти.\n\n" +
                $"Сейчас свободно: {memory.AvailablePhysicalMiB:N0} МиБ.\n" +
                $"Рекомендуемый объём с запасом для Windows: " +
                $"{memory.RecommendedAvailableMiB:N0} МиБ.\n\n" +
                "Расчёт приблизительный и не является запретом. Если продолжить, " +
                "Windows всё равно может сильно замедлиться или зависнуть, а " +
                "сохранение — завершиться ошибкой. Закройте тяжёлые программы, " +
                "если это возможно.\n\n" +
                "После подтверждения " +
                $"сохранение запустится в изолированной группе процессов с верхним пределом " +
                $"{memory.WorkerLimitMiB:N0} МиБ. Это верхняя граница, а не " +
                "объём, который обязательно будет занят. При достижении предела завершится " +
                "только операция сохранения, а исходный файл останется неизменным.\n\n" +
                "Продолжить сохранение?",
                "SmoLVLcreator — возможно недостаточно памяти",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning,
                MessageBoxResult.No);
            if (choice != MessageBoxResult.Yes)
            {
                StatusText.Text =
                    "Сохранение отменено после предупреждения о памяти";
                return;
            }
        }
        _isSaving = true;
        using var saveCancellation = new CancellationTokenSource();
        _saveCancellation = saveCancellation;
        try
        {
            SetBusy(true, $"Сохранение {System.IO.Path.GetFileName(path)}…");
            SaveProgressBar.Minimum = 0;
            SaveProgressBar.Maximum = 1;
            SaveProgressBar.Value = 0;
            SaveProgressBar.Visibility = Visibility.Visible;
            SaveCancelButton.IsEnabled = true;
            SaveCancelButton.Visibility = Visibility.Visible;
            var progress = new Progress<SmoLevelSaveProgress>(update =>
            {
                SaveProgressBar.Maximum = Math.Max(1, update.TotalSteps);
                SaveProgressBar.Value = Math.Clamp(
                    update.CompletedSteps,
                    0,
                    update.TotalSteps);
                SaveProgressBar.ToolTip =
                    $"{update.CompletedSteps:N0} / {update.TotalSteps:N0} · " +
                    $"{update.CurrentContainerBytes / (1024d * 1024d):N1} МиБ";
                StatusText.Text =
                    $"{update.Message} · {update.CompletedSteps:N0}/{update.TotalSteps:N0}";
            });
            SmoLevelSaveResult result = await SmoLevelIsolatedSaveClient.SaveAsync(
                document,
                fullPath,
                progress,
                saveCancellation.Token);
            diagnosticLogPath = result.LogPath;
            // The save service performs file work on a worker thread; WPF-bound
            // history notifications must be raised only after returning here.
            document.MarkSaved();
            SmoLevelSaveService.TryAppendDiagnostic(
                result.LogPath,
                "INFO",
                "UI_COMMIT",
                $"Editor history marked saved on UI thread " +
                $"{Environment.CurrentManagedThreadId}; revision={document.Revision}.");
            _savePath = result.OutputPath;
            UpdateDocumentCaption();

            string backup = result.BackupPath is null
                ? string.Empty
                : $" · backup: {System.IO.Path.GetFileName(result.BackupPath)}";
            SetBusy(
                false,
                $"Сохранено · {result.EditedEntityCount:N0} объектов · " +
                $"{result.PatchedTransformCount:N0} transforms · " +
                $"{result.AddedPlacementCount:N0} placements added · " +
                $"{result.ResourceEditCount:N0} resources{backup} · " +
                $"log: {System.IO.Path.GetFileName(result.LogPath)}");
        }
        catch (OperationCanceledException)
        {
            SetBusy(false, "Сохранение отменено · исходный и целевой файлы не изменены");
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось сохранить уровень");
            string logPath = exception is SmoLevelSaveException saveException
                ? saveException.LogPath
                : diagnosticLogPath;
            SmoLevelSaveService.TryAppendDiagnostic(
                logPath,
                "ERROR",
                "GUI_SAVE_ERROR",
                exception.ToString());
            MessageBox.Show(
                this,
                exception.Message +
                $"\n\nПодробный журнал:\n{logPath}\n\n" +
                "Его можно просто прислать целиком — там уже записаны этап " +
                "сохранения, объекты, матрицы и полный stack trace.",
                "SmoLVLcreator — ошибка сохранения",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
        finally
        {
            _saveCancellation = null;
            SaveProgressBar.Visibility = Visibility.Collapsed;
            SaveCancelButton.Visibility = Visibility.Collapsed;
            _isSaving = false;
            UpdateCommandAvailability();
        }
    }

    private void SaveCancel_Click(object sender, RoutedEventArgs e)
    {
        if (_saveCancellation is null || _saveCancellation.IsCancellationRequested)
            return;
        _saveCancellation.Cancel();
        SaveCancelButton.IsEnabled = false;
        StatusText.Text = "Отмена после завершения текущего этапа…";
    }

    private void UpdateDocumentCaption()
    {
        if (_workspace is null)
            return;
        if (HasProject)
        {
            string projectName = System.IO.Path.GetFileName(
                _projectPath ??
                _project!.Manifest.SourceFileName ??
                "Новый проект.smolvlproj");
            DocumentNameText.Text = _projectSession!.IsModified
                ? $"{projectName} *"
                : projectName;
            return;
        }
        string name = System.IO.Path.GetFileName(_savePath ?? _workspace.SourcePath);
        DocumentNameText.Text = _document?.IsModified == true
            ? $"{name} *"
            : name;
    }

    private void ApplyAssetFilter()
        => ApplyCatalogFilter();

    private void AssetFilter_Changed(object sender, TextChangedEventArgs e) =>
        ApplyAssetFilter();

    private void ShowAllEntities_Click(object sender, RoutedEventArgs e)
    {
        int restored = _hiddenEntities.Count;
        _hiddenEntities.Clear();
        RefreshVisibilityState();
        StatusText.Text = restored > 0
            ? $"Показано объектов: {restored:N0}"
            : "Все объекты уже видимы";
    }

    private void IsolateEntities_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null ||
            (_selectedEntities.Count == 0 && _selectedPendingPlacement is null))
        {
            return;
        }
        IReadOnlyList<SmoLevelEntityId> visible =
            LinkedCollisionMovementToggle?.IsChecked == true
                ? _document.ExpandLinkedEntities(_selectedEntities)
                : _selectedEntities.ToArray();
        HashSet<SmoLevelEntityId> visibleSet = visible.ToHashSet();
        _hiddenEntities.Clear();
        _hiddenEntities.UnionWith(_document.Entities
            .Where(entity =>
                !_document.RemovedEntityIds.Contains(entity.Id) &&
                !visibleSet.Contains(entity.Id))
            .Select(entity => entity.Id));
        RefreshVisibilityState();
        StatusText.Text =
            $"Изолировано объектов: {Math.Max(visibleSet.Count, 1):N0}";
    }

    private void HideEntities_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null || _selectedEntities.Count == 0)
            return;
        SmoLevelEntityId[] hidden = _selectedEntities.ToArray();
        _hiddenEntities.UnionWith(hidden);
        _selectedEntities.Clear();
        _activeSelectionKey = null;
        _activeCollisionEntityIndex = null;
        _selectedPendingPlacement = null;
        _syncingSelection = true;
        AssetList.SelectedItems.Clear();
        _syncingSelection = false;
        RefreshVisibilityState();
        ViewportSelectionText.Text = "NO SELECTION";
        StatusText.Text = $"Скрыто объектов: {hidden.Length:N0}";
    }

    private void RefreshVisibilityState()
    {
        ApplyPlacementHighlights();
        BuildSceneTree();
        if (_catalogSection == CatalogSection.Placements)
            ApplyCatalogFilter();
        UpdateCommandAvailability();
    }

    private void LinkCollision_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null ||
            !TryGetSelectedCollisionPair(
                out SmoLevelEntity? visual,
                out SmoLevelEntity? collision))
        {
            return;
        }
        try
        {
            if (_document.SetCollisionLink(visual!.Id, collision!.Id))
            {
                try
                {
                    if (HasProject)
                        RememberProjectCollisionLink(visual, collision, present: true);
                }
                catch
                {
                    _document.RemoveCollisionLink(visual.Id, collision.Id);
                    throw;
                }
                ApplyPlacementHighlights();
                UpdateBindingsText(visual);
                StatusText.Text =
                    $"Связаны {visual.Name} и {collision.Name} · Ctrl+Z — отменить";
            }
        }
        catch (Exception exception)
        {
            StatusText.Text = $"Не удалось создать связь: {exception.Message}";
        }
    }

    private void UnlinkCollision_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null ||
            !TryGetSelectedCollisionPair(
                out SmoLevelEntity? visual,
                out SmoLevelEntity? collision))
        {
            return;
        }
        if (_document.RemoveCollisionLink(visual!.Id, collision!.Id))
        {
            try
            {
                if (HasProject)
                    RememberProjectCollisionLink(visual, collision, present: false);
            }
            catch (Exception exception)
            {
                _document.SetCollisionLink(visual.Id, collision.Id);
                StatusText.Text =
                    $"РќРµ СѓРґР°Р»РѕСЃСЊ СѓРґР°Р»РёС‚СЊ СЃРІСЏР·СЊ: {exception.Message}";
                return;
            }
            ApplyPlacementHighlights();
            UpdateBindingsText(visual);
            StatusText.Text =
                $"Связь {visual.Name} ↔ {collision.Name} удалена · Ctrl+Z — отменить";
        }
    }

    private bool TryGetSelectedCollisionPair(
        out SmoLevelEntity? visual,
        out SmoLevelEntity? collision)
    {
        visual = null;
        collision = null;
        if (_document is null || _selectedEntities.Count != 2)
            return false;
        SmoLevelEntity[] entities = _selectedEntities
            .Select(_document.GetEntity)
            .ToArray();
        visual = entities.FirstOrDefault(entity =>
            entity.Kind == SmoLevelEntityKind.Visual);
        collision = entities.FirstOrDefault(entity =>
            entity.Kind == SmoLevelEntityKind.Collision);
        return visual is not null && collision is not null;
    }

    private void OutlinerFilter_Changed(object sender, TextChangedEventArgs e)
    {
        if (IsLoaded)
            BuildSceneTree();
    }

    private void BuildSceneTree()
    {
        if (SceneTree is null)
            return;

        SceneTree.Items.Clear();
        if (_workspace is null || _document is null)
        {
            OutlinerCountText.Text = "0";
            return;
        }

        string filter = OutlinerFilterBox?.Text.Trim() ?? string.Empty;
        EditorAssetItem[] visible = _allAssets
            .Where(item => filter.Length == 0 ||
                item.Name.Contains(filter, StringComparison.OrdinalIgnoreCase))
            .ToArray();
        var visibleModels = visible.Select(item => new
            {
                Item = item,
                PlacementIndices = Enumerable.Range(0, item.PlacementCount)
                    .Where(index => item.Asset is not SmoLevelAsset asset ||
                        !_document!.TryGetPlacement(
                            new SmoPlacementId(
                                asset.ObjectIndex,
                                asset.Placements[index].SceneObjectIndex),
                            out SmoEditablePlacement? placement) ||
                        !_document.RemovedEntityIds.Contains(placement!.Entity.Id))
                    .ToArray()
            })
            .Where(entry => entry.PlacementIndices.Length > 0)
            .ToArray();

        var root = new TreeViewItem
        {
            Header = _workspace.DisplayName,
            IsExpanded = true
        };
        var models = new TreeViewItem
        {
            Header = $"Размещённые модели  ({visibleModels.Sum(entry => entry.PlacementIndices.Length):N0})",
            IsExpanded = filter.Length > 0
        };
        root.Items.Add(models);

        foreach (var visibleModel in visibleModels)
        {
            EditorAssetItem item = visibleModel.Item;
            int firstPlacementIndex = visibleModel.PlacementIndices[0];
            int hiddenPlacementCount = item.Asset is SmoLevelAsset treeAsset
                ? visibleModel.PlacementIndices.Count(index =>
                    IsPlacementHidden(treeAsset, index))
                : 0;
            var assetNode = new TreeViewItem
            {
                Header = $"◇  {item.Name}   ×{visibleModel.PlacementIndices.Length}" +
                    (hiddenPlacementCount > 0
                        ? $"  · скрыто {hiddenPlacementCount:N0}"
                        : string.Empty),
                Tag = new SceneTreeSelection(item, firstPlacementIndex),
                Opacity = hiddenPlacementCount == visibleModel.PlacementIndices.Length
                    ? 0.48
                    : 1
            };

            if (item.Asset is { } asset && visibleModel.PlacementIndices.Length > 1)
            {
                foreach (int index in visibleModel.PlacementIndices)
                {
                    SmoLevelPlacement placement = asset.Placements[index];
                    bool hidden = IsPlacementHidden(asset, index);
                    assetNode.Items.Add(new TreeViewItem
                    {
                        Header = (hidden ? "○  " : string.Empty) +
                            $"#{index + 1:000}  {placement.Name.TrimEnd('\0')}",
                        Tag = new SceneTreeSelection(item, index),
                        Opacity = hidden ? 0.48 : 1
                    });
                }
            }

            models.Items.Add(assetNode);
        }

        SmoLevelEntity[] visibleCollisions = _document?.Entities
            .Where(entity => entity.Kind == SmoLevelEntityKind.Collision &&
                !_document.RemovedEntityIds.Contains(entity.Id) &&
                (filter.Length == 0 || entity.Name.Contains(
                    filter,
                    StringComparison.OrdinalIgnoreCase)))
            .OrderBy(entity => entity.Name, StringComparer.OrdinalIgnoreCase)
            .ToArray() ?? [];
        var collisions = new TreeViewItem
        {
            Header = $"Коллизии  ({visibleCollisions.Length:N0})",
            IsExpanded = filter.Length > 0
        };
        foreach (SmoLevelEntity collision in visibleCollisions)
        {
            bool hidden = _hiddenEntities.Contains(collision.Id);
            collisions.Items.Add(new TreeViewItem
            {
                Header = (hidden ? "○  " : string.Empty) +
                    $"△  {collision.Name}  · {collision.Collisions.Count:N0} shape",
                Tag = new CollisionTreeSelection(collision.Id.SceneObjectIndex),
                Opacity = hidden ? 0.48 : 1
            });
        }
        root.Items.Add(collisions);

        SceneTree.Items.Add(root);
        OutlinerCountText.Text = (visibleModels.Sum(entry => entry.PlacementIndices.Length) +
            visibleCollisions.Length)
            .ToString("N0", CultureInfo.CurrentCulture);
    }

    private bool IsPlacementHidden(SmoLevelAsset asset, int placementIndex)
    {
        if (_document is null ||
            (uint)placementIndex >= (uint)asset.Placements.Count)
        {
            return false;
        }
        SmoLevelPlacement placement = asset.Placements[placementIndex];
        return _document.TryGetPlacement(
                   new SmoPlacementId(
                       asset.ObjectIndex,
                       placement.SceneObjectIndex),
                   out SmoEditablePlacement? editable) &&
               _hiddenEntities.Contains(editable!.Entity.Id);
    }

    private void AssetList_SelectionChanged(
        object sender,
        SelectionChangedEventArgs e)
        => HandleCatalogSelectionChanged();

    private void SceneTree_SelectedItemChanged(
        object sender,
        RoutedPropertyChangedEventArgs<object> e)
    {
        if (_syncingSelection)
            return;
        if (e.NewValue is TreeViewItem { Tag: CollisionTreeSelection collisionSelection })
        {
            SelectCollisionFromTree(collisionSelection.CollisionInfoObjectIndex);
            return;
        }
        if (e.NewValue is not TreeViewItem { Tag: SceneTreeSelection selection })
            return;

        SelectCatalogItemForPlacement(selection.Item, selection.PlacementIndex);
        UpdateInspector(selection.Item, selection.PlacementIndex);
    }

    private void SelectCollisionFromTree(
        int collisionInfoObjectIndex,
        bool focus = true)
    {
        if (_document is null)
            return;
        SmoEditableCollision? collision = _document.Collisions.FirstOrDefault(candidate =>
            candidate.Entity.Id.SceneObjectIndex == collisionInfoObjectIndex);
        if (collision is null)
            return;
        _showCollisions = true;
        CollisionVisibilityToggle.IsChecked = true;
        _selectedEntities.Clear();
        _selectedEntities.Add(collision.Entity.Id);
        _activeSelectionKey = null;
        _activeCollisionEntityIndex = collisionInfoObjectIndex;
        _syncingSelection = true;
        AssetList.SelectedItem = null;
        _syncingSelection = false;
        ApplyPlacementHighlights();
        if (focus)
            FocusCollision(collision.Entity);
        UpdateCollisionInspector(collision);
        UpdateCollisionSelectionCaption(collision.Entity);
        UpdateCommandAvailability();
        StatusText.Text = $"Выбрана коллизия · {collision.Entity.Name}";
    }

    private void UpdateCollisionInspector(SmoEditableCollision collision)
    {
        SmoLevelEntity entity = collision.Entity;
        Matrix4x4.Decompose(
            collision.WorldTransform,
            out Vector3 scale,
            out Quaternion rotation,
            out Vector3 translation);
        int vertexCount = entity.Collisions.Sum(shape => shape.Source.Positions.Count);
        int triangleCount = entity.Collisions.Sum(shape =>
            shape.Source.TriangleIndices.Count / 3);
        bool generated = _document?.GeneratedCollisions.ContainsKey(entity.Id) == true;
        bool writable = HasProject
            ? TryCaptureProjectTransforms([entity.Id], out _, out _)
            : generated || _workspace is not null &&
                SmoPlacementTransformWriter.CanWriteCollisionTransform(
                    _workspace.Document,
                    entity.Id.SceneObjectIndex);
        uint? objectId = !generated && _workspace is not null
            ? _workspace.Document.Objects[entity.Id.SceneObjectIndex].Id
            : null;

        SelectedNameText.Text = entity.Name;
        SelectedPathText.Text = generated
            ? "Generated collision · будет добавлена как spCollisionInfo / spMeshBV при сохранении"
            : $"Node [{collision.Source.NodeObjectIndex}] / spCollisionInfo " +
              $"[{collision.Source.CollisionInfoObjectIndex}] / spMeshBV " +
              $"[{collision.Source.MeshBoundingVolumeObjectIndex}]";
        ObjectIdentityText.Text = generated
            ? "spCollisionInfo · новый"
            : $"spCollisionInfo · [{collision.Source.CollisionInfoObjectIndex}]";
        ObjectIdText.Text = objectId is uint id
            ? $"0x{id:X8}"
            : "до сохранения не назначен";
        SetPositionEditor(translation, enabled: writable);
        SetRotationScaleEditor(rotation, scale, enabled: writable);
        PlacementCountText.Text = $"{entity.Collisions.Count:N0} · collision shape";
        GeometryCountText.Text = $"{vertexCount:N0} / {triangleCount:N0}";
        VertexLayoutText.Text = "spMeshBV v2 · indexed UInt16";
        ChannelsText.Text = "Collision triangles";
        TextureNameText.Text = "—";
        MaterialText.Text = "Collision wireframe overlay";
        UpdateBindingsText(entity);
        DiagnosticsText.Text =
            (generated
                ? "Выпуклая collision-сетка создана генератором SmoLVLcreator.Core.\n"
                : "Геометрия декодирована общим SmoViewer.Core.\n") +
            (writable
                ? generated
                    ? "SAVE READY · новая ветка будет записана в ближайший сектор уровня."
                    : "SAVE READY · перенос будет записан в вершины spMeshBV."
                : "SAVE BLOCKED · структура spMeshBV этого объекта не подтверждена.");
        DiagnosticsText.Foreground = writable ? HealthyBrush : NoticeBrush;
        if (generated)
        {
            UpdateRawData();
        }
        else
        {
            UpdateRawData(
                ("collision node", collision.Source.NodeObjectIndex),
                ("collision info", collision.Source.CollisionInfoObjectIndex),
                ("collision mesh", collision.Source.MeshBoundingVolumeObjectIndex));
        }
    }

    private void RevealCollisionInTree(int collisionInfoObjectIndex)
    {
        TreeViewItem? item = FindCollisionTreeItem(
            SceneTree.Items.OfType<TreeViewItem>(),
            collisionInfoObjectIndex);
        if (item is null && !string.IsNullOrEmpty(OutlinerFilterBox.Text))
        {
            OutlinerFilterBox.Clear();
            item = FindCollisionTreeItem(
                SceneTree.Items.OfType<TreeViewItem>(),
                collisionInfoObjectIndex);
        }
        if (item is null)
            return;
        _syncingSelection = true;
        item.IsSelected = true;
        item.BringIntoView();
        _syncingSelection = false;
    }

    private static TreeViewItem? FindCollisionTreeItem(
        IEnumerable<TreeViewItem> nodes,
        int collisionInfoObjectIndex)
    {
        foreach (TreeViewItem node in nodes)
        {
            if (node.Tag is CollisionTreeSelection selection &&
                selection.CollisionInfoObjectIndex == collisionInfoObjectIndex)
            {
                return node;
            }
            TreeViewItem? descendant = FindCollisionTreeItem(
                node.Items.OfType<TreeViewItem>(),
                collisionInfoObjectIndex);
            if (descendant is not null)
            {
                node.IsExpanded = true;
                return descendant;
            }
        }
        return null;
    }

    private void UpdateInspector(
        EditorAssetItem item,
        int placementIndex,
        bool updateViewportSelection = true)
    {
        SelectedNameText.Text = item.Name;
        ViewportSelectionText.Text =
            $"SELECTED  ·  {item.Name}  ·  placement " +
            $"{Math.Min(placementIndex + 1, item.PlacementCount)} / {item.PlacementCount}";

        if (item.Asset is not SmoLevelAsset asset)
        {
            ClearInspector(_workspace?.DisplayName);
            ViewportSelectionText.Text = "NO SELECTION";
            return;
        }

        placementIndex = Math.Clamp(placementIndex, 0, asset.Placements.Count - 1);
        SmoLevelPlacement placement = asset.Placements[placementIndex];
        var placementId = new SmoPlacementId(
            asset.ObjectIndex,
            placement.SceneObjectIndex);
        SmoEditablePlacement? editable = null;
        Matrix4x4 worldTransform =
            _document?.TryGetPlacement(placementId, out editable) == true
                ? editable!.WorldTransform
                : placement.WorldTransform;
        Matrix4x4.Decompose(
            worldTransform,
            out Vector3 scale,
            out Quaternion rotation,
            out Vector3 translation);

        SelectedPathText.Text = asset.FullPath;
        ObjectIdentityText.Text = $"spMeshData · [{asset.ObjectIndex}]";
        ObjectIdText.Text = $"0x{asset.ObjectId:X8}";
        bool placementWritable = editable is not null &&
            (HasProject
                ? TryCaptureProjectTransforms(
                    [editable.Entity.Id],
                    out _,
                    out _)
                : _document?.CanPersistTransform(editable.Entity.Id) == true);
        SetPositionEditor(translation, enabled: placementWritable);
        SetRotationScaleEditor(rotation, scale, enabled: placementWritable);
        PlacementCountText.Text =
            $"{asset.Placements.Count:N0} · " +
            (placement.IsSharedInstance ? "shared instance" : "embedded mesh");
        GeometryCountText.Text = $"{asset.VertexCount:N0} / {asset.TriangleCount:N0}";
        VertexLayoutText.Text =
            $"0x{asset.VertexFormat:X8} · stride {asset.VertexStride}";
        ChannelsText.Text = string.Join(" · ", new[]
        {
            asset.HasNormals ? "Normals" : null,
            asset.HasUv0 ? "UV0" : null,
            asset.HasUv1 ? "UV1" : null,
            asset.HasVertexDiffuse ? "Vertex diffuse" : null,
            asset.HasSkinning ? "Skinning" : null
        }.Where(value => value is not null));
        TextureNameText.Text = asset.Texture is SmoLevelTexture texture
            ? $"{texture.Name.TrimEnd('\0')} · {texture.Width} × {texture.Height} · " +
              $"0x{texture.FormatCode:X4} {texture.SourceLayout}"
            : "Нет подтверждённой texture binding";
        MaterialText.Text = asset.MaterialSummary ??
            (asset.UsesAlphaBlend ? "Alpha blend" : "Render state не определён");
        if (editable is not null)
            UpdateBindingsText(editable.Entity);
        else
            BindingsText.Text =
                "spStaticRenderObject → spModel → spMeshData → spMaterialData → spTextureData";
        string diagnostic = asset.Issue ??
            "Декодировано общим SmoViewer.Core; связь не дублируется редактором.";
        DiagnosticsText.Text = diagnostic + "\n" +
            (placementWritable
                ? "SAVE READY · подтверждённый путь записи transform."
                : "SAVE BLOCKED · placement-матрица этого объекта пока не подтверждена.");
        DiagnosticsText.Foreground = asset.HasIssue
            ? ErrorBrush
            : placementWritable ? HealthyBrush : NoticeBrush;
        if (updateViewportSelection)
            ReplacePlacementSelection(
                asset,
                placementIndex,
                expandComposite: !Keyboard.Modifiers.HasFlag(ModifierKeys.Alt));
        if (editable is not null && _selectedEntities.Contains(editable.Entity.Id))
            UpdateSelectionCaption(item.Name, placementIndex);
        UpdateRawData(
            ("placement", placement.SceneObjectIndex),
            ("model", placement.ModelObjectIndex),
            ("mesh resource", asset.ObjectIndex));
    }

    private void UpdateBindingsText(SmoLevelEntity entity)
    {
        if (_document is null)
            return;
        string compositeText = _document.TryGetCompositeModel(
                entity.Id,
                out SmoCompositeModel? composite)
            ? $"Составная модель: {composite!.Name} · " +
              $"{composite.Parts.Count:N0} мешей / {composite.Entities.Count:N0} объектов\n" +
              "ЛКМ — целиком; Alt+ЛКМ — отдельный меш.\n"
            : string.Empty;
        IReadOnlyList<SmoLevelCollisionLink> links =
            _document.GetCollisionLinks(entity.Id);
        if (links.Count == 0)
        {
            BindingsText.Text = compositeText + (entity.Kind == SmoLevelEntityKind.Collision
                ? "spPartitionNode → spCollisionInfo → spMeshBV\nСвязанный визуальный объект не подтверждён."
                : "spStaticRenderObject → spModel → spMeshData\nСвязанная коллизия не подтверждена.");
            return;
        }

        SmoLevelEntity[] peers = links
            .Select(link => entity.Kind == SmoLevelEntityKind.Collision
                ? _document.GetEntity(link.VisualEntityId)
                : _document.GetEntity(link.CollisionEntityId))
            .DistinctBy(peer => peer.Id)
            .ToArray();
        string peerKind = entity.Kind == SmoLevelEntityKind.Collision
            ? "Визуальный объект"
            : "Коллизия";
        string peerList = string.Join(
            "\n",
            peers.Select(peer =>
            {
                SmoLevelCollisionLink link = links.First(candidate =>
                    candidate.VisualEntityId == peer.Id ||
                    candidate.CollisionEntityId == peer.Id);
                return $"{peerKind}: {peer.Name} [{peer.Id.SceneObjectIndex}] · " +
                       $"совпадение {link.Confidence:P0}";
            }));
        BindingsText.Text = compositeText + peerList +
            "\nMOVE LINKED — вместе; выключить для отдельного перемещения.";
    }

    private void SetPositionEditor(Vector3? value, bool enabled = false)
    {
        if (PositionXBox is null)
            return;
        _updatingPositionEditor = true;
        PositionXBox.IsEnabled = enabled;
        PositionYBox.IsEnabled = enabled;
        PositionZBox.IsEnabled = enabled;
        PositionXBox.Text = value?.X.ToString("0.###", CultureInfo.CurrentCulture) ?? string.Empty;
        PositionYBox.Text = value?.Y.ToString("0.###", CultureInfo.CurrentCulture) ?? string.Empty;
        PositionZBox.Text = value?.Z.ToString("0.###", CultureInfo.CurrentCulture) ?? string.Empty;
        _updatingPositionEditor = false;
    }

    private void SetRotationScaleEditor(
        Quaternion? rotation,
        Vector3? scale,
        bool enabled = false)
    {
        if (RotationXBox is null)
            return;
        _updatingPositionEditor = true;
        Vector3? degrees = rotation is Quaternion quaternion
            ? SmoEulerAngles.ToDegrees(quaternion)
            : null;
        foreach (TextBox box in new[]
                 {
                     RotationXBox, RotationYBox, RotationZBox,
                     ScaleXBox, ScaleYBox, ScaleZBox
                 })
        {
            box.IsEnabled = enabled;
        }
        RotationXBox.Text = FormatEditorNumber(degrees?.X);
        RotationYBox.Text = FormatEditorNumber(degrees?.Y);
        RotationZBox.Text = FormatEditorNumber(degrees?.Z);
        ScaleXBox.Text = FormatEditorNumber(scale?.X);
        ScaleYBox.Text = FormatEditorNumber(scale?.Y);
        ScaleZBox.Text = FormatEditorNumber(scale?.Z);
        if (rotation is Quaternion stored)
        {
            string quaternionText = FormattableString.Invariant(
                $"SMO quaternion: X {stored.X:0.######}, Y {stored.Y:0.######}, Z {stored.Z:0.######}, W {stored.W:0.######}");
            RotationXBox.ToolTip = quaternionText;
            RotationYBox.ToolTip = quaternionText;
            RotationZBox.ToolTip = quaternionText;
        }
        _updatingPositionEditor = false;
    }

    private static string FormatEditorNumber(float? value) =>
        value?.ToString("0.###", CultureInfo.CurrentCulture) ?? string.Empty;

    private void PositionBox_GotKeyboardFocus(object sender, KeyboardFocusChangedEventArgs e)
    {
        if (sender is TextBox textBox)
            textBox.SelectAll();
    }

    private void PositionBox_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter)
        {
            CommitPositionEditor();
            e.Handled = true;
        }
        else if (e.Key == Key.Escape)
        {
            RefreshActiveInspector();
            ViewportSurface.Focus();
            e.Handled = true;
        }
    }

    private void TransformBox_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter)
        {
            if (sender is FrameworkElement { Name: string name } &&
                name.StartsWith("Rotation", StringComparison.Ordinal))
            {
                CommitRotationEditor();
            }
            else
            {
                CommitScaleEditor();
            }
            e.Handled = true;
        }
        else if (e.Key == Key.Escape)
        {
            RefreshActiveInspector();
            ViewportSurface.Focus();
            e.Handled = true;
        }
    }

    private void RotationBox_LostKeyboardFocus(
        object sender,
        KeyboardFocusChangedEventArgs e)
    {
        if (!_updatingPositionEditor)
            CommitRotationEditor();
    }

    private void ScaleBox_LostKeyboardFocus(
        object sender,
        KeyboardFocusChangedEventArgs e)
    {
        if (!_updatingPositionEditor)
            CommitScaleEditor();
    }

    private void PositionBox_LostKeyboardFocus(
        object sender,
        KeyboardFocusChangedEventArgs e)
    {
        if (!_updatingPositionEditor)
            CommitPositionEditor();
    }

    private void CommitPositionEditor()
    {
        if (_updatingPositionEditor)
            return;
        if (!TryParsePosition(PositionXBox.Text, out float x) ||
            !TryParsePosition(PositionYBox.Text, out float y) ||
            !TryParsePosition(PositionZBox.Text, out float z))
        {
            StatusText.Text = "Position: требуется корректное число X/Y/Z";
            return;
        }
        ApplyPositionFromInspector(new Vector3(x, y, z));
    }

    private void CommitRotationEditor()
    {
        if (_updatingPositionEditor)
            return;
        if (!TryParsePosition(RotationXBox.Text, out float x) ||
            !TryParsePosition(RotationYBox.Text, out float y) ||
            !TryParsePosition(RotationZBox.Text, out float z))
        {
            StatusText.Text = "Rotation: требуется корректное число X/Y/Z";
            return;
        }
        ApplyRotationFromInspector(new Vector3(x, y, z));
    }

    private void CommitScaleEditor()
    {
        if (_updatingPositionEditor)
            return;
        if (!TryParsePosition(ScaleXBox.Text, out float x) ||
            !TryParsePosition(ScaleYBox.Text, out float y) ||
            !TryParsePosition(ScaleZBox.Text, out float z) ||
            x <= 0 || y <= 0 || z <= 0)
        {
            StatusText.Text = "Scale: X/Y/Z должны быть положительными числами";
            return;
        }
        ApplyScaleFromInspector(new Vector3(x, y, z));
    }

    private static bool TryParsePosition(string text, out float value) =>
        float.TryParse(
            text,
            NumberStyles.Float,
            CultureInfo.CurrentCulture,
            out value) ||
        float.TryParse(
            text,
            NumberStyles.Float,
            CultureInfo.InvariantCulture,
            out value);

    private void AssetList_MouseDoubleClick(object sender, MouseButtonEventArgs e)
    {
        if (_catalogSection is not (CatalogSection.Models or CatalogSection.Placements))
            return;
        CatalogItem[] selection = AssetList.SelectedItems
            .OfType<CatalogItem>()
            .ToArray();
        if (selection.Length > 0)
            ApplyCatalogVisualSelection(selection);
    }

    private void EditorTool_Checked(object sender, RoutedEventArgs e)
    {
        if (sender is not RadioButton { Tag: string tool })
            return;
        if (_editorTool != tool)
            CancelGizmoDrag();
        _editorTool = tool;
        if (StatusText is not null)
            StatusText.Text = $"Инструмент: {tool}";
    }

    private void TransformSpace_Checked(object sender, RoutedEventArgs e)
    {
        if (sender is not RadioButton { Tag: string space })
            return;
        CancelGizmoDrag();
        if (StatusText is not null)
            StatusText.Text = $"Система координат: {space}";
    }

    private void ToggleInspector_Click(object sender, RoutedEventArgs e)
    {
        if (InspectorColumn.Width.Value > 0)
        {
            _inspectorWidth = InspectorColumn.Width;
            InspectorColumn.MinWidth = 0;
            InspectorColumn.Width = new GridLength(0);
            InspectorSplitterColumn.Width = new GridLength(0);
        }
        else
        {
            InspectorColumn.MinWidth = 250;
            InspectorColumn.Width = _inspectorWidth;
            InspectorSplitterColumn.Width = new GridLength(5);
        }
    }

    private void ToggleOutliner_Click(object sender, RoutedEventArgs e)
    {
        if (OutlinerColumn.Width.Value > 0)
        {
            _outlinerWidth = OutlinerColumn.Width;
            OutlinerColumn.MinWidth = 0;
            OutlinerColumn.Width = new GridLength(0);
            OutlinerSplitterColumn.Width = new GridLength(0);
        }
        else
        {
            OutlinerColumn.MinWidth = 230;
            OutlinerColumn.Width = _outlinerWidth;
            OutlinerSplitterColumn.Width = new GridLength(5);
        }
    }

    private void ToggleAssets_Click(object sender, RoutedEventArgs e)
    {
        if (AssetRow.Height.Value > 0)
        {
            _assetHeight = AssetRow.Height;
            AssetRow.MinHeight = 0;
            AssetRow.Height = new GridLength(0);
        }
        else
        {
            AssetRow.MinHeight = 130;
            AssetRow.Height = _assetHeight;
        }
    }

    private void UpdateStatistics()
    {
        if (_workspace is null)
        {
            StatsText.Text = "Уровень не открыт";
            return;
        }

        StatsText.Text =
            $"{_workspace.ObjectCount +
                (_document?.GeneratedCollisions.Count ?? 0) * 2:N0} объектов · " +
            $"{_workspace.Assets.Count:N0} моделей · " +
            $"{_workspace.PlacementCount:N0} размещений · " +
            $"{(_document?.Collisions.Count(collision =>
                !_document.RemovedEntityIds.Contains(collision.Entity.Id)) ??
                _workspace.Collisions.Count):N0} collision shapes · " +
            $"{_workspace.TextureCount:N0} текстур · " +
            $"{_workspace.DiagnosticCount:N0} проблем";
    }

    private sealed record SceneTreeSelection(
        EditorAssetItem Item,
        int PlacementIndex);

    private sealed record CollisionTreeSelection(int CollisionInfoObjectIndex);

    private sealed class EditorAssetItem
    {
        private EditorAssetItem(
            SmoLevelAsset? asset,
            string name,
            int placementCount,
            int vertexCount,
            int triangleCount,
            string status,
            bool hasIssue)
        {
            Asset = asset;
            Name = name;
            PlacementCount = placementCount;
            VertexCount = vertexCount;
            TriangleCount = triangleCount;
            Status = status;
            HasIssue = hasIssue;
        }

        public SmoLevelAsset? Asset { get; }
        public string Name { get; }
        public int PlacementCount { get; }
        public int VertexCount { get; }
        public int TriangleCount { get; }
        public string Status { get; }
        public bool HasIssue { get; }
        public string PlacementBadge => $"×{PlacementCount}";
        public string Kind => Asset?.HasSkinning == true ? "SKIN" : "RIGID";
        public string Details => $"{VertexCount:N0} verts · {TriangleCount:N0} tris";
        public Brush StatusColor => HasIssue ? ErrorBrush :
            Asset?.Texture is null ? NoticeBrush : HealthyBrush;

        public static EditorAssetItem FromAsset(SmoLevelAsset asset)
        {
            string status = asset.Issue ??
                (asset.Texture is SmoLevelTexture texture
                    ? texture.Name.TrimEnd('\0')
                    : "без подтверждённой текстуры");
            return new EditorAssetItem(
                asset,
                asset.DisplayName,
                asset.Placements.Count,
                asset.VertexCount,
                asset.TriangleCount,
                status,
                asset.HasIssue);
        }

    }
}
