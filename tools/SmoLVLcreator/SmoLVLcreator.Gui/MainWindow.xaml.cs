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
    private string _editorTool = "Select";

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
        DocumentNameText.Text = LevelText.Get("Main.Workspace.Closed");
        ViewportEmptyTitleText.Text = _gpuViewportFailureReason is null
            ? LevelText.Get("Main.Workspace.Closed")
            : LevelText.Get("Main.Viewport.Unavailable");
        ViewportStateText.Text = _gpuViewportFailureReason is null
            ? LevelText.Get("Main.Viewport.OpenHint")
            : LevelText.Get("Main.Viewport.GpuUnavailable", _gpuViewportFailureReason);
        ViewportSelectionText.Text = LevelText.Get("Main.Viewport.SelectionEmpty");
        ClearInspector();
        SetWorkspaceUiState(hasDocument: false);
        UpdateCommandAvailability();
        ApplyAssetFilter();
        BuildSceneTree();
        UpdateStatistics();
        StatusText.Text = LevelText.Get("Main.Workspace.OpenHint");
    }

    private readonly List<(TreeViewItem Node, string Key, object?[] Arguments)> _outlinerText = [];

    private void SetOutlinerText(TreeViewItem node, string key, params object?[] arguments)
    {
        node.Header = LevelText.Get(key, arguments);
        _outlinerText.Add((node, key, arguments));
    }

    private void RefreshMainLanguage()
    {
        WorkspaceTitleText.Text = App.WorkspaceTitle;
        UpdateViewportModeButtons();
        UpdateCameraCaption();
        UpdateCommandAvailability();
        UpdateStatistics();
        foreach ((TreeViewItem node, string key, object?[] arguments) in _outlinerText)
            node.Header = LevelText.Get(key, arguments);

        if (_workspace is null)
            DocumentNameText.Text = LevelText.Get("Main.Workspace.Closed");
        else
            UpdateDocumentCaption();

        if (_gpuViewportFailureReason is string reason)
        {
            ViewportEmptyTitleText.Text = LevelText.Get("Main.Viewport.Unavailable");
            ViewportStateText.Text = LevelText.Get("Main.Viewport.GpuUnavailable", reason);
        }
        else if (_workspace is null)
        {
            ViewportEmptyTitleText.Text = LevelText.Get("Main.Workspace.Closed");
            ViewportStateText.Text = LevelText.Get("Main.Viewport.OpenHint");
        }
        else if (_workspace.PreparedScene.Meshes.Count == 0)
        {
            ViewportEmptyTitleText.Text = LevelText.Get("Main.Viewport.NoGeometry");
            ViewportStateText.Text = LevelText.Get("Main.Viewport.NoGeometryDetails");
        }

        if (_selectedPendingPlacement is not null ||
            TryResolveActiveCollision(out _) || TryResolveActiveSelection(out _, out _, out _))
        {
            RefreshActiveInspector();
        }
        else
        {
            ClearInspector(_workspace?.DisplayName);
            ViewportSelectionText.Text = LevelText.Get("Main.Viewport.SelectionEmpty");
        }
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
        SelectedNameText.Text = LevelText.Get("Main.Inspector.None");
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
            ? LevelText.Get("Main.Inspector.OpenHint")
            : LevelText.Get("Main.Inspector.SelectHint");
        DiagnosticsText.Foreground = _workspace is null ? NoticeBrush : HealthyBrush;
        RawDataText.Text = _workspace is null
            ? LevelText.Get("Main.Inspector.RawEmpty")
            : LevelText.Get("Main.Inspector.DirectFields");
    }

    private async void OpenLevel_Click(object sender, RoutedEventArgs e)
    {
        if (_isBusy || !ConfirmDiscardProjectChanges())
            return;
        var dialog = new OpenFileDialog
        {
            Title = LevelText.Get("Main.Message.OpenASmoLVLcreatorLevelOrProject"),
            Filter = LevelText.Get("Main.Message.LevelOrProjectSmoSmolvlprojSmo") +
                     LevelText.Get("Main.Message.SparkplugModelOrLevelSmoSmo") +
                     LevelText.Get("Main.Message.SmoLVLcreatorProjectSmolvlprojSmolvlproj") +
                     LevelText.Get("Main.Message.AllFiles"),
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
            SetBusy(true, LevelText.Get("Main.Message.CreatingAProjectFrom", Path.GetFileName(fullPath)));
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
                LevelText.Get("Main.Message.OpenedAsAProjectObjectsSource", Path.GetFileName(fullPath), loaded.Project.Objects.Count));
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Main.Message.CouldNotOpenTheLevel"));
            MessageBox.Show(
                this,
                exception.Message,
                LevelText.Get("Main.Message.SmoLVLcreatorOpenError"),
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
    }

    private void AttachWorkspace(
        SmoLevelWorkspace workspace,
        string? savePath,
        bool preserveEditorState = false,
        SmoLevelDocument? preparedDocument = null)
    {
        SmoLevelDocument document = preparedDocument ?? new SmoLevelDocument(workspace);
        SmoLevelWorkspace? previousWorkspace = _workspace;
        SmoLevelDocument? previousDocument = _document;
        if (previousDocument is not null)
            previousDocument.Changed -= LevelDocument_Changed;
        _workspace = workspace;
        _document = document;
        _document.Changed += LevelDocument_Changed;
        if (preserveEditorState)
        {
            SmoLevelEntityId? active = RemapProjectEditorEntities(
                previousWorkspace, previousDocument, workspace, _document);
            _selectedPendingPlacement = null;
            _pendingPlacementByScene.Clear();
            _collisionRegionByInfo.Clear();
            ActivateFirstSelectedEntity(active);
            _activePlacementIndex = 0;
        }
        _savePath = savePath;
        _allAssets.Clear();
        _allAssets.AddRange(workspace.Assets.Select(EditorAssetItem.FromAsset));
        if (preserveEditorState && TryResolveActiveSelection(out _, out _, out int placementIndex))
            _activePlacementIndex = placementIndex;
        SetWorkspaceUiState(hasDocument: true);
        ClearInspector(workspace.DisplayName);
        ViewportEmptyTitleText.Text = _gpuViewportFailureReason is not null
            ? LevelText.Get("Main.Viewport.Unavailable")
            : workspace.PreparedScene.Meshes.Count > 0
                ? LevelText.Get("Main.Viewport.LoadingLevel")
                : LevelText.Get("Main.Viewport.NoGeometry");
        LoadRenderScene(
            workspace.PreparedScene,
            preserveEditorSelection: preserveEditorState);

        UpdateDocumentCaption();
        if (_gpuRendererAvailable && workspace.PreparedScene.Meshes.Count > 0)
        {
            ViewportStateText.Text =
                LevelText.Get("Main.Viewport.LoadedCatalog", workspace.Assets.Count, workspace.PlacementCount, workspace.Collisions.Count);
        }
        AssetFilterBox.Clear();
        OutlinerFilterBox.Clear();
        ApplyAssetFilter();
        BuildSceneTree();
        if (!preserveEditorState && AssetList.Items.Count > 0)
            AssetList.SelectedIndex = 0;
        else
            ViewportSelectionText.Text = LevelText.Get("Main.Viewport.SelectionEmpty");
        UpdateStatistics();
    }

    private SmoLevelEntityId? RemapProjectEditorEntities(
        SmoLevelWorkspace? previousWorkspace,
        SmoLevelDocument? previousDocument,
        SmoLevelWorkspace workspace,
        SmoLevelDocument document)
    {
        if (_project is null || previousWorkspace is null || previousDocument is null)
        {
            _selectedEntities.Clear();
            _hiddenEntities.Clear();
            return null;
        }
        if (_selectedEntities.Count == 0 && _hiddenEntities.Count == 0)
            return null;

        // Preview rebuilds can shift dense indices. Only IDs from the original
        // catalog are stable across removal and reuse of generated object IDs.
        HashSet<uint> originalIds = _project.Objects.Select(entry => entry.Id).ToHashSet();
        IReadOnlyList<SmoObjectEntry> previousObjects = previousWorkspace.Document.Objects;
        ILookup<uint, SmoObjectEntry> previousById = previousObjects.ToLookup(entry => entry.Id);
        ILookup<uint, SmoObjectEntry> currentById = workspace.Document.Objects.ToLookup(entry => entry.Id);

        int? activeIndex = _activeCollisionEntityIndex ?? _activeSelectionKey?.SceneObjectIndex;
        SmoLevelEntityId? active = activeIndex is int index
            ? RemapEntity(new SmoLevelEntityId(index))
            : null;
        RemapSet(_selectedEntities);
        RemapSet(_hiddenEntities);
        return active;

        void RemapSet(HashSet<SmoLevelEntityId> entities)
        {
            var remapped = new List<SmoLevelEntityId>(entities.Count);
            foreach (SmoLevelEntityId entityId in entities)
            {
                if (RemapEntity(entityId) is SmoLevelEntityId currentId)
                    remapped.Add(currentId);
            }
            entities.Clear();
            entities.UnionWith(remapped);
        }

        SmoLevelEntityId? RemapEntity(SmoLevelEntityId entityId)
        {
            if ((uint)entityId.SceneObjectIndex >= (uint)previousObjects.Count ||
                !previousDocument.TryGetEntity(entityId, out SmoLevelEntity? previousEntity))
                return null;

            SmoObjectEntry previous = previousObjects[entityId.SceneObjectIndex];
            if (previous.Id == 0 || !originalIds.Contains(previous.Id) ||
                previousById[previous.Id].Count() != 1 || currentById[previous.Id].Count() != 1)
                return null;

            SmoObjectEntry current = currentById[previous.Id].Single();
            var currentId = new SmoLevelEntityId(current.Index);
            if (current.TypeHash != previous.TypeHash ||
                !document.TryGetEntity(currentId, out SmoLevelEntity? currentEntity) ||
                currentEntity!.Kind != previousEntity!.Kind)
                return null;

            return currentId;
        }
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
                ? LevelText.Get("Main.Tool.RegenerateCollision")
                : LevelText.Get("Main.Tool.CreateCollision");
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
            ? LevelText.Get("Main.Message.ExportSelected", selectedCount)
            : LevelText.Get("Main.Menu.Export");
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
            Title = LevelText.Get("Main.Message.SaveTheEditedSMOLevel"),
            Filter = LevelText.Get("Main.Message.SparkplugModelOrLevelSmoSmo2"),
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
            Title = LevelText.Get("Main.Message.ExportSelectedObjects", selection.Length),
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
                LevelText.Get("Main.Message.ExportingObjectsTo", selection.Length, format.ToString().ToUpperInvariant()));
            SmoLevelExportResult result = await Task.Run(() =>
                SmoLevelExportService.Export(
                    document,
                    selection,
                    outputPath,
                    format));
            SetBusy(
                false,
                LevelText.Get("Main.Message.ExportedObjectsParts", result.EntityCount, result.PlacementCount, Path.GetFileName(result.OutputPath)) +
                (result.Warnings.Count > 0
                    ? LevelText.Get("Main.Message.Warnings", result.Warnings.Count)
                    : string.Empty));
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Main.Message.CouldNotExportTheSelectedObjects"));
            MessageBox.Show(
                this,
                exception.Message,
                LevelText.Get("Main.Message.SmoLVLcreatorExportError"),
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
        StatusText.Text = LevelText.Get("Main.Message.SelectOneOrMoreVisualObjects");
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
                LevelText.Get("Main.Message.SettingsSavedQuickExport") +
                $"{_settings.ExportFormat.ToString().ToUpperInvariant()} · " +
                _settings.ExportDirectory;
        }
        catch (Exception exception)
        {
            MessageBox.Show(
                this,
                exception.Message,
                LevelText.Get("Main.Message.SmoLVLcreatorCouldNotSaveSettings"),
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
                StatusText.Text = LevelText.Get("Main.Message.NoChangesToSave");
                return;
            }
            StatusText.Text = LevelText.Get("Main.Message.RestoringCollisionRegistry");
        }

        SmoLevelDocument document = _document;
        string diagnosticLogPath = SmoLevelSaveService.GetLogPath(fullPath);
        SmoLevelSaveMemoryAssessment memory =
            SmoLevelIsolatedSaveClient.AssessMemory(document);
        if (memory.ShouldWarn)
        {
            MessageBoxResult choice = MessageBox.Show(
                this,
                LevelText.Get("Main.Message.SavingMayRequireAroundMiBOf", memory.EstimatedPeakMiB, memory.AvailablePhysicalMiB, memory.RecommendedAvailableMiB, memory.WorkerLimitMiB),
                LevelText.Get("Main.Message.SmoLVLcreatorLowAvailableMemory"),
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning,
                MessageBoxResult.No);
            if (choice != MessageBoxResult.Yes)
            {
                StatusText.Text =
                    LevelText.Get("Main.Message.SaveCanceledAfterMemoryWarning");
                return;
            }
        }
        _isSaving = true;
        using var saveCancellation = new CancellationTokenSource();
        _saveCancellation = saveCancellation;
        try
        {
            SetBusy(true, LevelText.Get("Main.Message.Saving", System.IO.Path.GetFileName(path)));
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
                    LevelText.Get("Main.Message.MiB", update.CompletedSteps, update.TotalSteps, update.CurrentContainerBytes / (1024d * 1024d));
                StatusText.Text =
                    LevelText.Get("Main.Message.SaveLevelAsync", update.Message, update.CompletedSteps, update.TotalSteps);
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
                : LevelText.Get("Main.Message.Backup", System.IO.Path.GetFileName(result.BackupPath));
            SetBusy(
                false,
                LevelText.Get("Main.Message.SavedObjectsTransformsAddedPlacementsResources", result.EditedEntityCount, result.PatchedTransformCount, result.AddedPlacementCount, result.ResourceEditCount, backup, System.IO.Path.GetFileName(result.LogPath)));
        }
        catch (OperationCanceledException)
        {
            SetBusy(false, LevelText.Get("Main.Message.SaveCanceledSourceAndTargetFiles"));
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Main.Message.CouldNotSaveTheLevel"));
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
                LevelText.Get("Main.Message.DetailedLogYouCanSendThe", logPath),
                LevelText.Get("Main.Message.SmoLVLcreatorSaveError"),
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
        StatusText.Text = LevelText.Get("Main.Message.CancelingAfterTheCurrentStage");
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
                LevelText.Get("Main.Workspace.NewProjectName"));
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
            ? LevelText.Get("Main.Message.ObjectsShown", restored)
            : LevelText.Get("Main.Message.AllObjectsAreAlreadyVisible");
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
            LevelText.Get("Main.Message.ObjectsIsolated", Math.Max(visibleSet.Count, 1));
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
        ViewportSelectionText.Text = LevelText.Get("Main.Viewport.SelectionEmpty");
        StatusText.Text = LevelText.Get("Main.Message.ObjectsHidden", hidden.Length);
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
                    LevelText.Get("Main.Message.LinkedAndCtrlZToUndo", visual.Name, collision.Name);
            }
        }
        catch (Exception exception)
        {
            StatusText.Text = LevelText.Get("Main.Message.CouldNotCreateLink", exception.Message);
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
                    LevelText.Get("Main.Message.CouldNotRemoveLink", exception.Message);
                return;
            }
            ApplyPlacementHighlights();
            UpdateBindingsText(visual);
            StatusText.Text =
                LevelText.Get("Main.Message.LinkRemovedCtrlZToUndo", visual.Name, collision.Name);
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
        _outlinerText.Clear();
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
                                asset.Placements[index].SceneObjectIndex, asset.Placements[index].OccurrenceKey),
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
            IsExpanded = filter.Length > 0
        };
        SetOutlinerText(models, "Main.Outliner.ModelCount", visibleModels.Sum(entry => entry.PlacementIndices.Length));
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
                Header = $"◇  {item.Name}   ×{visibleModel.PlacementIndices.Length}",
                Tag = new SceneTreeSelection(item, firstPlacementIndex),
                Opacity = hiddenPlacementCount == visibleModel.PlacementIndices.Length
                    ? 0.48
                    : 1
            };
            if (hiddenPlacementCount > 0)
                SetOutlinerText(assetNode, "Main.Outliner.HiddenCount", assetNode.Header, hiddenPlacementCount);

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
            IsExpanded = filter.Length > 0
        };
        SetOutlinerText(collisions, "Main.Outliner.CollisionCount", visibleCollisions.Length);
        foreach (SmoLevelEntity collision in visibleCollisions)
        {
            bool hidden = _hiddenEntities.Contains(collision.Id);
            var collisionNode = new TreeViewItem
            {
                Tag = new CollisionTreeSelection(collision.Id.SceneObjectIndex),
                Opacity = hidden ? 0.48 : 1
            };
            SetOutlinerText(collisionNode, "Main.Outliner.CollisionShapeCount",
                hidden ? "○  " : string.Empty, collision.Name, collision.Collisions.Count);
            collisions.Items.Add(collisionNode);
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
                       placement.SceneObjectIndex, placement.OccurrenceKey),
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
        StatusText.Text = LevelText.Get("Main.Message.CollisionSelected", collision.Entity.Name);
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
            ? LevelText.Get("Main.Inspector.GeneratedCollisionAddedAsSpCollisionInfoSpMeshBV")
            : LevelText.Get("Main.Inspector.NodeSpCollisionInfo", collision.Source.NodeObjectIndex) +
              $"[{collision.Source.CollisionInfoObjectIndex}] / spMeshBV " +
              $"[{collision.Source.MeshBoundingVolumeObjectIndex}]";
        ObjectIdentityText.Text = generated
            ? LevelText.Get("Main.Inspector.SpCollisionInfoNew")
            : $"spCollisionInfo · [{collision.Source.CollisionInfoObjectIndex}]";
        ObjectIdText.Text = objectId is uint id
            ? $"0x{id:X8}"
            : LevelText.Get("Main.Inspector.AssignedOnSave");
        SetPositionEditor(translation, enabled: writable);
        SetRotationScaleEditor(rotation, scale, enabled: writable);
        PlacementCountText.Text = LevelText.Get("Main.Inspector.CollisionShapes", entity.Collisions.Count);
        GeometryCountText.Text = $"{vertexCount:N0} / {triangleCount:N0}";
        VertexLayoutText.Text = LevelText.Get("Main.Inspector.SpMeshBVVIndexedUInt");
        ChannelsText.Text = LevelText.Get("Main.Inspector.CollisionTriangles");
        TextureNameText.Text = "—";
        MaterialText.Text = LevelText.Get("Main.Inspector.CollisionWireframeOverlay");
        UpdateBindingsText(entity);
        DiagnosticsText.Text =
            (generated
                ? LevelText.Get("Main.Inspector.ConvexCollisionMeshGeneratedBySmoLVLcreator")
                : LevelText.Get("Main.Inspector.GeometryDecodedBySharedSmoViewerCore")) +
            (writable
                ? generated
                    ? LevelText.Get("Main.Inspector.CANSAVENewBranchWillBe")
                    : LevelText.Get("Main.Inspector.CANSAVETranslationWillBeWritten")
                : LevelText.Get("Main.Inspector.SAVEBLOCKEDTheSpMeshBVLayoutFor"));
        DiagnosticsText.Foreground = writable ? HealthyBrush : NoticeBrush;
        if (generated)
        {
            UpdateRawData();
        }
        else
        {
            UpdateRawData(
                (LevelText.Get("Main.Inspector.CollisionNode"), collision.Source.NodeObjectIndex),
                (LevelText.Get("Main.Inspector.CollisionInfo"), collision.Source.CollisionInfoObjectIndex),
                (LevelText.Get("Main.Inspector.CollisionMesh"), collision.Source.MeshBoundingVolumeObjectIndex));
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
            LevelText.Get("Main.Inspector.SELECTEDPlacement", item.Name) +
            $"{Math.Min(placementIndex + 1, item.PlacementCount)} / {item.PlacementCount}";

        if (item.Asset is not SmoLevelAsset asset)
        {
            ClearInspector(_workspace?.DisplayName);
            ViewportSelectionText.Text = LevelText.Get("Main.Viewport.SelectionEmpty");
            return;
        }

        placementIndex = Math.Clamp(placementIndex, 0, asset.Placements.Count - 1);
        SmoLevelPlacement placement = asset.Placements[placementIndex];
        var placementId = new SmoPlacementId(
            asset.ObjectIndex,
            placement.SceneObjectIndex, placement.OccurrenceKey);
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
            (placement.IsSharedInstance ? LevelText.Get("Main.Inspector.SharedInstance") : LevelText.Get("Main.Inspector.EmbeddedMesh"));
        GeometryCountText.Text = $"{asset.VertexCount:N0} / {asset.TriangleCount:N0}";
        VertexLayoutText.Text =
            LevelText.Get("Main.Inspector.XStride", asset.VertexFormat, asset.VertexStride);
        ChannelsText.Text = string.Join(" · ", new[]
        {
            asset.HasNormals ? LevelText.Get("Main.Inspector.Normals") : null,
            asset.HasUv0 ? "UV0" : null,
            asset.HasUv1 ? "UV1" : null,
            asset.HasVertexDiffuse ? LevelText.Get("Main.Inspector.VertexDiffuse") : null,
            asset.HasSkinning ? LevelText.Get("Main.Inspector.Skinning") : null
        }.Where(value => value is not null));
        TextureNameText.Text = asset.Texture is SmoLevelTexture texture
            ? $"{texture.Name.TrimEnd('\0')} · {texture.Width} × {texture.Height} · " +
              $"0x{texture.FormatCode:X4} {texture.SourceLayout}"
            : LevelText.Get("Main.Inspector.NoConfirmedTextureBinding");
        MaterialText.Text = asset.MaterialSummary ??
            (asset.UsesAlphaBlend ? LevelText.Get("Main.Inspector.AlphaBlend") : LevelText.Get("Main.Inspector.RenderStateUndefined"));
        if (editable is not null)
            UpdateBindingsText(editable.Entity);
        else
            BindingsText.Text =
                "spStaticRenderObject → spModel → spMeshData → spMaterialData → spTextureData";
        string diagnostic = asset.Issue ??
            LevelText.Get("Main.Inspector.DecodedBySharedSmoViewerCoreThe");
        DiagnosticsText.Text = diagnostic + "\n" +
            (placementWritable
                ? LevelText.Get("Main.Inspector.CANSAVEConfirmedTransformWriter")
                : LevelText.Get("Main.Inspector.SAVEBLOCKEDThePlacementMatrixFor"));
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
            (LevelText.Get("Main.Inspector.Placement2"), placement.SceneObjectIndex),
            (LevelText.Get("Main.Inspector.Model"), placement.ModelObjectIndex),
            (LevelText.Get("Main.Inspector.MeshResource"), asset.ObjectIndex));
    }

    private void UpdateBindingsText(SmoLevelEntity entity)
    {
        if (_document is null)
            return;
        string compositeText = _document.TryGetCompositeModel(
                entity.Id,
                out SmoCompositeModel? composite)
            ? LevelText.Get("Main.Inspector.CompositeModelMeshesObjectsLMBSelects", composite!.Name, composite.Parts.Count, composite.Entities.Count)
            : string.Empty;
        IReadOnlyList<SmoLevelCollisionLink> links =
            _document.GetCollisionLinks(entity.Id);
        if (links.Count == 0)
        {
            BindingsText.Text = compositeText + (entity.Kind == SmoLevelEntityKind.Collision
                ? LevelText.Get("Main.Inspector.SpPartitionNodeSpCollisionInfoSpMeshBVLinkedVisualObject")
                : LevelText.Get("Main.Inspector.SpStaticRenderObjectSpModelSpMeshDataLinkedCollisionIs"));
            return;
        }

        SmoLevelEntity[] peers = links
            .Select(link => entity.Kind == SmoLevelEntityKind.Collision
                ? _document.GetEntity(link.VisualEntityId)
                : _document.GetEntity(link.CollisionEntityId))
            .DistinctBy(peer => peer.Id)
            .ToArray();
        string peerKind = entity.Kind == SmoLevelEntityKind.Collision
            ? LevelText.Get("Main.Inspector.VisualObject")
            : LevelText.Get("Main.Inspector.Collision");
        string peerList = string.Join(
            "\n",
            peers.Select(peer =>
            {
                SmoLevelCollisionLink link = links.First(candidate =>
                    candidate.VisualEntityId == peer.Id ||
                    candidate.CollisionEntityId == peer.Id);
                return $"{peerKind}: {peer.Name} [{peer.Id.SceneObjectIndex}] · " +
                       LevelText.Get("Main.Inspector.Match", link.Confidence);
            }));
        BindingsText.Text = compositeText + peerList +
            LevelText.Get("Main.Inspector.MOVELINKEDMovesBothTurnIt");
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
            string quaternionText = string.Format(CultureInfo.InvariantCulture,
                LevelText.Get("Main.Message.SMOQuaternionXYZW"), stored.X, stored.Y, stored.Z, stored.W);
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
            StatusText.Text = LevelText.Get("Main.Message.PositionEnterValidXYAnd");
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
            StatusText.Text = LevelText.Get("Main.Message.RotationEnterValidXYAnd");
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
            StatusText.Text = LevelText.Get("Main.Message.ScaleXYAndZMust");
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
            StatusText.Text = LevelText.Get("Main.Tool.Active", EditorToolCaption(tool));
    }

    private static string EditorToolCaption(string tool) => LevelText.Get(tool switch
    {
        "Rotate" => "Main.Tool.Rotate",
        "Scale" => "Main.Tool.Scale",
        "Move" => "Main.Tool.Move",
        _ => "Main.Tool.Select"
    });

    private static string TransformSpaceCaption(string space) => LevelText.Get(space == "LOCAL"
        ? "Main.Tool.Local"
        : "Main.Tool.World");

    private void TransformSpace_Checked(object sender, RoutedEventArgs e)
    {
        if (sender is not RadioButton { Tag: string space })
            return;
        CancelGizmoDrag();
        if (StatusText is not null)
            StatusText.Text = LevelText.Get("Main.Tool.CoordinateSystem", TransformSpaceCaption(space));
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
            StatsText.Text = LevelText.Get("Main.Workspace.Closed");
            return;
        }

        StatsText.Text =
            LevelText.Get("Main.Workspace.Stats", _workspace.ObjectCount +
                (_document?.GeneratedCollisions.Count ?? 0) * 2, _workspace.Assets.Count, _workspace.PlacementCount, (_document?.Collisions.Count(collision =>
                !_document.RemovedEntityIds.Contains(collision.Entity.Id)) ??
                _workspace.Collisions.Count), _workspace.TextureCount, _workspace.DiagnosticCount);
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
            _status = status;
            HasIssue = hasIssue;
        }

        public SmoLevelAsset? Asset { get; }
        public string Name { get; }
        public int PlacementCount { get; }
        public int VertexCount { get; }
        public int TriangleCount { get; }
        private readonly string _status;
        public string Status => LevelText.Display(_status);
        public bool HasIssue { get; }
        public string PlacementBadge => $"×{PlacementCount}";
        public string Kind => Asset?.HasSkinning == true ? LevelText.Get("Main.Inspector.Skinned") : LevelText.Get("Main.Inspector.Rigid");
        public string Details => LevelText.Get("Main.Inspector.MeshDetails", VertexCount, TriangleCount);
        public Brush StatusColor => HasIssue ? ErrorBrush :
            Asset?.Texture is null ? NoticeBrush : HealthyBrush;

        public static EditorAssetItem FromAsset(SmoLevelAsset asset)
        {
            string status = asset.Issue ??
                (asset.Texture is SmoLevelTexture texture
                    ? texture.Name.TrimEnd('\0')
                    : LevelText.Label("Main.Inspector.NoTexture"));
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
