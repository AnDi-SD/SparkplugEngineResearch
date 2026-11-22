using Microsoft.Win32;
using SmoImporter.Core;
using SmoLVLcreator.Core;
using SmoViewer.Core;
using System.IO;
using System.Numerics;
using System.Windows;
using System.Windows.Controls;

namespace SmoLVLcreator.Gui;

public partial class MainWindow
{
    private SmoProject? _project;
    private SmoProjectSession? _projectSession;
    private bool _projectDeletionInProgress;
    private string? _projectPath;
    private ProjectPlacementClipboard? _projectPlacementClipboard;

    private bool HasProject =>
        _project is not null && _projectSession is not null;

    private void ClearProjectState()
    {
        _project = null;
        _projectSession = null;
        _projectPath = null;
        _projectPlacementClipboard = null;
    }

    private async void CreateProject_Click(
        object sender,
        RoutedEventArgs e)
    {
        if (_workspace is null || _document is null || _isBusy)
            return;
        if (_document.IsModified)
        {
            MessageBox.Show(
                this,
                LevelText.Get("Project.CreateRequiresCleanSmo"),
                LevelText.Get("Project.ProjectTitle"),
                MessageBoxButton.OK,
                MessageBoxImage.Information);
            return;
        }

        string sourcePath = _workspace.SourcePath;
        var dialog = new SaveFileDialog
        {
            Title = LevelText.Get("Project.CreateTitle"),
            Filter = LevelText.Get("Project.ProjectFilter"),
            InitialDirectory = Path.GetDirectoryName(sourcePath),
            FileName = Path.GetFileNameWithoutExtension(sourcePath) + ".smolvlproj",
            AddExtension = true,
            DefaultExt = ".smolvlproj",
            OverwritePrompt = true
        };
        if (dialog.ShowDialog(this) != true)
            return;

        try
        {
            SetBusy(true, LevelText.Get("Project.CreatingProject", Path.GetFileName(dialog.FileName)));
            SmoDocument source = _workspace.Document;
            SmoProject project = await Task.Run(() =>
            {
                SmoProject imported = SmoProject.Import(source);
                SmoProjectArchive.Save(imported, dialog.FileName);
                return imported;
            });
            _project = project;
            _projectSession = new SmoProjectSession(project);
            _projectSession.MarkSaved();
            _projectPath = Path.GetFullPath(dialog.FileName);
            _savePath = null;
            ApplyProjectUiState();
            UpdateDocumentCaption();
            SetBusy(false,
                LevelText.Get("Project.ProjectCreated", project.Objects.Count));
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Project.CreateFailed"));
            ShowProjectError(LevelText.Get("Project.CreateError"), exception);
        }
    }

    private async void OpenProject_Click(
        object sender,
        RoutedEventArgs e)
    {
        if (_isBusy || !ConfirmDiscardProjectChanges())
            return;
        var dialog = new OpenFileDialog
        {
            Title = LevelText.Get("Project.OpenTitle"),
            Filter = LevelText.Get("Project.ProjectFilter"),
            CheckFileExists = true,
            Multiselect = false
        };
        if (dialog.ShowDialog(this) == true)
            await LoadProjectAsync(dialog.FileName);
    }

    private async Task LoadProjectAsync(string path)
    {
        try
        {
            string fullPath = Path.GetFullPath(path);
            SetBusy(true, LevelText.Get("Project.OpeningProject", Path.GetFileName(fullPath)));
            (SmoProject Project, SmoLevelWorkspace Workspace) loaded =
                await Task.Run(() =>
                {
                    SmoProject project =
                        SmoProjectArchive.Load(fullPath);
                    return (project, BuildProjectPreviewWorkspace(project));
                });
            _project = loaded.Project;
            _projectSession =
                new SmoProjectSession(loaded.Project);
            _projectSession.MarkSaved();
            _projectPath = fullPath;
            AttachWorkspace(loaded.Workspace, savePath: null);
            RestoreProjectCollisionLinks();
            ApplyProjectUiState();
            UpdateDocumentCaption();
            SetBusy(false,
                LevelText.Get("Project.ProjectOpened", loaded.Project.Objects.Count, loaded.Project.Manifest.ReferencePlacements.Count));
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Project.OpenFailed"));
            ShowProjectError(LevelText.Get("Project.OpenError"), exception);
        }
    }

    private async void SaveProject_Click(
        object sender,
        RoutedEventArgs e)
    {
        if (_projectPath is null)
            await SaveProjectAsAsync();
        else
            await SaveProjectAsync(_projectPath);
    }

    private async void SaveProjectAs_Click(
        object sender,
        RoutedEventArgs e) => await SaveProjectAsAsync();

    private async Task SaveProjectAsAsync()
    {
        if (!HasProject || _isBusy)
            return;
        string current = _projectPath ??
            _project!.Manifest.SourceFileName ?? "level.smo";
        var dialog = new SaveFileDialog
        {
            Title = LevelText.Get("Project.SaveTitle"),
            Filter = LevelText.Get("Project.ProjectFilter"),
            InitialDirectory = Path.GetDirectoryName(current),
            FileName = Path.GetFileNameWithoutExtension(current) + ".smolvlproj",
            AddExtension = true,
            DefaultExt = ".smolvlproj",
            OverwritePrompt = true
        };
        if (dialog.ShowDialog(this) == true)
            await SaveProjectAsync(dialog.FileName);
    }

    private async Task SaveProjectAsync(string path)
    {
        if (!HasProject || _isBusy)
            return;
        try
        {
            string fullPath = Path.GetFullPath(path);
            SetBusy(true, LevelText.Get("Project.SavingProject", Path.GetFileName(fullPath)));
            await Task.Run(() =>
                SmoProjectArchive.Save(_project!, fullPath));
            _projectPath = fullPath;
            _projectSession!.MarkSaved();
            UpdateDocumentCaption();
            SetBusy(false,
                LevelText.Get("Project.ProjectSaved", _project!.DataSection.Length));
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Project.SaveFailed"));
            ShowProjectError(LevelText.Get("Project.SaveError"), exception);
        }
    }

    private async void BuildProjectSmo_Click(
        object sender,
        RoutedEventArgs e)
    {
        if (!HasProject || _isBusy)
            return;
        string current = _projectPath ??
            _project!.Manifest.SourceFileName ?? "level.smo";
        var dialog = new SaveFileDialog
        {
            Title = LevelText.Get("Project.BuildTitle"),
            Filter = LevelText.Get("Project.SmoFilter"),
            InitialDirectory = Path.GetDirectoryName(current),
            FileName = Path.GetFileNameWithoutExtension(current) + "_built.smo",
            AddExtension = true,
            DefaultExt = ".smo",
            OverwritePrompt = true
        };
        if (dialog.ShowDialog(this) != true)
            return;
        string outputPath = Path.GetFullPath(dialog.FileName);
        string diagnosticLogPath = SmoLevelSaveService.GetLogPath(outputPath);
        SmoProjectBuildMemoryAssessment memory =
            SmoProjectIsolatedBuildClient.AssessMemory(_project!);
        if (memory.ShouldWarn)
        {
            MessageBoxResult choice = MessageBox.Show(
                this,
                LevelText.Get("Project.MemoryWarning", memory.EstimatedPeakMiB,
                    memory.AvailablePhysicalMiB, memory.RecommendedAvailableMiB, memory.WorkerLimitMiB),
                LevelText.Get("Project.MemoryTitle"),
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning,
                MessageBoxResult.No);
            if (choice != MessageBoxResult.Yes)
            {
                StatusText.Text = LevelText.Get("Project.MemoryCancelled");
                return;
            }
        }
        _isSaving = true;
        using var buildCancellation = new CancellationTokenSource();
        _saveCancellation = buildCancellation;
        try
        {
            SetBusy(true, LevelText.Get("Project.Building", Path.GetFileName(outputPath)));
            SaveProgressBar.Minimum = 0;
            SaveProgressBar.Maximum = 5;
            SaveProgressBar.Value = 0;
            SaveProgressBar.Visibility = Visibility.Visible;
            SaveCancelButton.IsEnabled = true;
            SaveCancelButton.Visibility = Visibility.Visible;
            var progress = new Progress<SmoProjectBuildProgress>(update =>
            {
                SaveProgressBar.Maximum = Math.Max(1, update.TotalSteps);
                SaveProgressBar.Value = Math.Clamp(
                    update.CompletedSteps,
                    0,
                    update.TotalSteps);
                SaveProgressBar.ToolTip =
                    LevelText.Get("Project.BuildProgress", update.CompletedSteps,
                        update.TotalSteps, update.CurrentContainerBytes / (1024d * 1024d));
                StatusText.Text = update.Message;
            });
            SmoProjectBuildResult result =
                await SmoProjectIsolatedBuildClient.BuildAsync(
                    _project!,
                    outputPath,
                    progress,
                    buildCancellation.Token);
            string backup = result.BackupPath is null
                ? string.Empty
                : LevelText.Get("Project.Backup", Path.GetFileName(result.BackupPath));
            SetBusy(false,
                LevelText.Get("Project.BuiltSmo", result.FileSize, result.ObjectCount, backup) +
                Path.GetFileName(diagnosticLogPath));
        }
        catch (OperationCanceledException)
        {
            SetBusy(false, LevelText.Get("Project.BuildCancelled"));
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Project.BuildFailed"));
            SmoLevelSaveService.TryAppendDiagnostic(
                diagnosticLogPath,
                "ERROR",
                "GUI_PROJECT_BUILD_ERROR",
                exception.ToString());
            MessageBox.Show(
                this,
                LevelText.Error(exception.Message) +
                LevelText.Get("Project.DiagnosticLog", diagnosticLogPath),
                LevelText.Get("Project.BuildError"),
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

    private async void ValidateProject_Click(
        object sender,
        RoutedEventArgs e)
    {
        if (!HasProject || _isBusy)
            return;
        string directory = Path.Combine(
            Path.GetTempPath(),
            "SmoLVLcreator",
            "ProjectNativeValidation",
            Guid.NewGuid().ToString("N"));
        string fileName = string.IsNullOrWhiteSpace(
            _project!.Manifest.SourceFileName)
                ? "level.smo"
                : Path.GetFileName(_project.Manifest.SourceFileName);
        string candidatePath = Path.Combine(directory, fileName);
        try
        {
            Directory.CreateDirectory(directory);
            SetBusy(true, LevelText.Get("Project.BuildingValidationCandidate"));
            SmoProjectBuildResult result =
                await SmoProjectIsolatedBuildClient.BuildAsync(
                    _project,
                    candidatePath,
                    progress: null,
                    CancellationToken.None);
            SetBusy(false,
                LevelText.Get("Project.ValidationCandidateBuilt", result.FileSize, result.ObjectCount));
            var window = new LevelNativeValidationWindow(
                candidatePath,
                _project.Manifest.SourceLogicalPath,
                _project.Manifest.SourcePathHint)
            {
                Owner = this
            };
            window.ShowDialog();
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Project.ValidationCandidateFailed"));
            ShowProjectError(LevelText.Get("Project.ValidationPreparationError"), exception);
        }
        finally
        {
            try
            {
                if (Directory.Exists(directory))
                    Directory.Delete(directory, recursive: true);
            }
            catch
            {
                // The validator already owns its copy; cleanup is best effort.
            }
        }
    }

    private async void AddProjectReferencePlacement_Click(
        object sender,
        RoutedEventArgs e)
    {
        if (!HasProject || _document is null || _workspace is null || _isBusy)
            return;
        if (!TryCaptureProjectPlacementClipboard(
                out ProjectPlacementClipboard? clipboard,
                out string reason))
        {
            MessageBox.Show(
                this,
                reason,
                LevelText.Get("Project.ReferencePlacementTitle"),
                MessageBoxButton.OK,
                MessageBoxImage.Information);
            return;
        }

        Vector3 target = ResolveDropPosition(new Point(
            Math.Max(ViewportSurface.ActualWidth, 1) * 0.5,
            Math.Max(ViewportSurface.ActualHeight, 1) * 0.5));
        await PasteProjectPlacementClipboardAsync(
            clipboard!,
            target,
            LevelText.Get("Project.ReferenceCopyPlaced"));
    }

    private async Task AddProjectCollisionAsync(
        SmoGeneratedCollisionMesh generated,
        string name,
        uint? replacedCollisionObjectId = null)
    {
        if (!HasProject || _isBusy)
            return;
        try
        {
            SetBusy(true, LevelText.Get("Project.PreparingCollision"));
            SmoProjectCollisionAddition? addition = null;
            bool changed = await Task.Run(() =>
                _projectSession!.Execute(
                    replacedCollisionObjectId is null
                        ? LevelText.Label("Project.CreateCollisionAction", name)
                        : LevelText.Label("Project.RegenerateCollisionAction", name),
                    project =>
                    {
                        if (replacedCollisionObjectId is uint oldId)
                            project.RemoveSceneBranches([oldId]);
                        addition = SmoProjectImporterBridge.AddCollision(
                            project,
                            generated.Positions,
                            generated.TriangleIndices,
                            name);
                    }));
            if (!changed || addition is null)
            {
                SetBusy(false, LevelText.Get("Project.CollisionUnchanged"));
                return;
            }
            await RefreshProjectPreviewAsync(
                LevelText.Get(
                    "Project.CollisionCreated",
                    LevelText.Get(replacedCollisionObjectId is null
                        ? "Project.CollisionCreatedVerb"
                        : "Project.CollisionRegeneratedVerb"),
                    generated.Positions.Count,
                    generated.TriangleCount),
                [addition.CollisionInfoObjectId]);
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Project.CollisionCreationFailed"));
            ShowProjectError(LevelText.Get("Project.CollisionError"), exception);
        }
    }

    private async Task AddProjectExternalModelAsync(
        ImportedScene imported,
        string sourcePath,
        string name)
    {
        if (!HasProject || _isBusy)
            return;
        Vector3 target = ResolveDropPosition(new Point(
            Math.Max(ViewportSurface.ActualWidth, 1) * 0.5,
            Math.Max(ViewportSurface.ActualHeight, 1) * 0.5));
        float scale = Path.GetExtension(sourcePath).Equals(
            ".glb",
            StringComparison.OrdinalIgnoreCase)
                ? 100f
                : 1f;
        Matrix4x4 transform = Matrix4x4.CreateScale(scale) *
            Matrix4x4.CreateTranslation(target);
        SmoWorkerHostInfo workerHost = SmoWorkerHost.ResolveCurrent();
        try
        {
            SetBusy(true, LevelText.Get("Project.PreparingModel", name));
            SmoProjectExternalModelAddition? addition = null;
            bool changed = await Task.Run(() =>
                _projectSession!.Execute(
                    LevelText.Label("Project.ImportPlaceAction", name),
                    project => addition = SmoProjectImporterBridge.AddExternalModelBatched(
                        project,
                        imported,
                        [transform],
                        sourcePath,
                        name,
                        workerHost.ExecutablePath,
                        workerHost.ManagedEntryAssemblyPath,
                        ResolveProjectFbxBridgePath())));
            if (!changed || addition is null)
            {
                SetBusy(false, LevelText.Get("Project.ImportUnchanged"));
                return;
            }
            await RefreshProjectPreviewAsync(
                LevelText.Get("Project.ModelImported", name, addition.MeshObjectIds.Count,
                    addition.ImportedTextureObjectIds.Count, target.X, target.Y, target.Z),
                addition.PlacementRootObjectIds);
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Project.ModelImportFailed"));
            ShowProjectError(LevelText.Get("Project.ModelImportError"), exception);
        }
    }

    private async Task ReplaceProjectCatalogModelAsync(
        CatalogItem item,
        ImportedScene imported,
        ReplacementTransform fit,
        Matrix4x4 referenceWorld,
        string sourcePath)
    {
        if (!HasProject || _workspace is null || _document is null || _isBusy)
            return;
        if (!Matrix4x4.Invert(referenceWorld, out Matrix4x4 inverseReference))
            throw new InvalidOperationException(
                LevelText.Get("Project.NoninvertibleReplacementReference"));

        var sourcePlacements = new List<(Matrix4x4 World, IReadOnlyList<SmoLevelEntityId> Entities)>();
        if (item.CompositeModels.Length > 0)
        {
            foreach (SmoCompositeModel instance in item.CompositeModels)
            {
                sourcePlacements.Add((
                    instance.Entities[0].WorldTransform,
                    instance.Entities.Select(entity => entity.Id).Distinct().ToArray()));
            }
        }
        else
        {
            SmoLevelAsset asset = item.VisualParts
                .Select(part => part.AssetItem.Asset)
                .FirstOrDefault(value => value is not null)
                ?? throw new InvalidOperationException(
                    LevelText.Get("Project.NoPlacementsForReplacement"));
            var seen = new HashSet<(SmoLevelEntityId Owner, SmoLevelEntityId Branch)>();
            foreach (SmoLevelPlacement placement in asset.Placements)
            {
                var placementId = new SmoPlacementId(
                    asset.ObjectIndex,
                    placement.SceneObjectIndex,
                    placement.OccurrenceKey);
                if (!_document.TryGetPlacement(placementId, out SmoEditablePlacement? editable))
                    throw new InvalidOperationException(LevelText.Get(
                        "Project.PlacementMissingFromPreview", placement.SceneObjectIndex));
                SmoLevelEntity entity = editable.Entity;
                if (_document.RemovedEntityIds.Contains(entity.Id))
                    continue;
                // A shared transform owner can contain unrelated models. In
                // that case only the selected renderable branch is replaced.
                bool ownsOnlyThisAsset = entity.Parts.All(part => part.Asset.ObjectIndex == asset.ObjectIndex);
                if (!ownsOnlyThisAsset &&
                    (placement.SceneObjectIndex == entity.Id.SceneObjectIndex ||
                     entity.Parts.Any(part => part.Asset.ObjectIndex != asset.ObjectIndex &&
                                              part.Source.SceneObjectIndex == placement.SceneObjectIndex)))
                {
                    throw new InvalidOperationException(LevelText.Get("Project.SharedReplacementBranch", item.Name));
                }
                SmoLevelEntityId branch = ownsOnlyThisAsset
                    ? entity.Id
                    : new SmoLevelEntityId(placement.SceneObjectIndex);
                if (seen.Add((entity.Id, branch)))
                    sourcePlacements.Add((editable.WorldTransform, [branch]));
            }
        }

        uint ObjectIdForEntity(SmoLevelEntityId entity)
        {
            int sceneIndex = entity.SceneObjectIndex;
            if ((uint)sceneIndex >= (uint)_workspace.Document.Objects.Count)
                throw new InvalidOperationException(
                    LevelText.Get("Project.PlacementMissingFromPreview", sceneIndex));
            return _workspace.Document.Objects[sceneIndex].Id;
        }

        IReadOnlyList<uint>[] sourcePlacementRoots = sourcePlacements
            .Select(placement => (IReadOnlyList<uint>)placement.Entities
                .Select(ObjectIdForEntity).ToArray())
            .ToArray();
        uint[] oldRootIds = sourcePlacementRoots.SelectMany(roots => roots).Distinct().ToArray();
        if (oldRootIds.Length == 0)
            throw new InvalidOperationException(LevelText.Get("Project.NoRemainingPlacements"));
        foreach (uint objectId in oldRootIds)
        {
            bool importedRoot = _project!.Objects.Any(entry =>
                entry.Id == objectId);
            bool generated = _project.Manifest.ReferencePlacements.Any(entry =>
                entry.NewObjectIds.Count > 0 && entry.NewObjectIds[0] == objectId);
            bool added = _project.Manifest.AddedForests.Any(forest =>
                !_project.Manifest.RemovedAddedForestIds.Contains(forest.BlobId) &&
                forest.Objects.Any(entry => entry.Id == objectId));
            if (!importedRoot && !generated && !added)
            {
                throw new InvalidOperationException(
                    LevelText.Get("Project.PlacementMissingFromCurrentProject", objectId));
            }
        }

        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        Matrix4x4 fittedReferenceWorld = reflection * fit.Matrix * reflection;
        Matrix4x4[] replacementWorlds = sourcePlacements
            .Select(placement => placement.World * inverseReference * fittedReferenceWorld)
            .ToArray();
        if (replacementWorlds.Any(world => !Matrix4x4.Invert(world, out _)))
            throw new InvalidOperationException(
                LevelText.Get("Project.NoninvertibleFittedPlacement"));

        string modelName = string.IsNullOrWhiteSpace(item.Name)
            ? Path.GetFileNameWithoutExtension(sourcePath)
            : item.Name.Trim();
        var effectiveLinks = new List<SmoProjectCollisionLinkOverride>();
        foreach (SmoLevelEntityId entity in sourcePlacements.SelectMany(placement => placement.Entities))
        {
            foreach (SmoLevelCollisionLink link in _document.GetCollisionLinks(entity))
            {
                if (TryGetProjectCollisionLink(
                        _document.GetEntity(entity),
                        _document.GetEntity(link.CollisionEntityId),
                        out SmoProjectCollisionLinkOverride captured))
                {
                    effectiveLinks.Add(captured);
                }
            }
        }
        SmoWorkerHostInfo workerHost = SmoWorkerHost.ResolveCurrent();

        try
        {
            SetBusy(true, LevelText.Get("Project.PreparingFullReplacement", modelName));
            SmoProjectExternalModelAddition? addition = null;
            bool changed = await Task.Run(() =>
                _projectSession!.Execute(
                    LevelText.Label("Project.ReplaceEntireModelAction", modelName),
                    project =>
                    {
                        addition = SmoProjectImporterBridge.AddExternalModelBatched(
                            project,
                            imported,
                            replacementWorlds,
                            sourcePath,
                            modelName,
                            workerHost.ExecutablePath,
                            workerHost.ManagedEntryAssemblyPath,
                            ResolveProjectFbxBridgePath());
                        project.ReplaceVisualBranches(
                            sourcePlacementRoots,
                            addition.PlacementRootObjectIdsByPlacement,
                            effectiveLinks);
                    }));
            if (!changed || addition is null)
            {
                SetBusy(false, LevelText.Get("Project.FullReplacementUnchanged"));
                return;
            }
            await RefreshProjectPreviewAsync(
                LevelText.Get("Project.FullReplacementDone", modelName, oldRootIds.Length,
                    addition.PlacementRootObjectIds.Count, addition.MeshObjectIds.Count,
                    addition.ImportedTextureObjectIds.Count),
                addition.PlacementRootObjectIds);
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Project.FullReplacementFailed"));
            ShowProjectError(LevelText.Get("Project.FullReplacementError"), exception);
        }
    }

    private async Task<bool> PlaceProjectCatalogModelAsync(
        CatalogModelDrag model,
        Vector3 target)
    {
        if (!HasProject || _workspace is null || _isBusy)
            return false;
        if (model.Parts.Length == 0)
            throw new InvalidOperationException(LevelText.Get("Project.NoPlaceableParts"));

        Vector3 delta = target - model.SourceAnchor;
        ProjectResourcePlacementRequest[] requests = model.Parts.Select(part =>
        {
            if ((uint)part.MeshObjectIndex >= (uint)_workspace.Document.Objects.Count)
                throw new InvalidOperationException(LevelText.Get("Project.MissingCatalogMesh"));
            Matrix4x4 world = part.SourceWorldTransform;
            world.M41 += delta.X;
            world.M42 += delta.Y;
            world.M43 += delta.Z;
            return new ProjectResourcePlacementRequest(
                _workspace.Document.Objects[part.MeshObjectIndex].Id,
                world,
                part.Name);
        }).ToArray();

        try
        {
            SetBusy(true, LevelText.Get("Project.PlacingModel", model.Name));
            var rootIds = new List<uint>(requests.Length);
            bool changed = await Task.Run(() =>
                _projectSession!.Execute(
                    requests.Length == 1
                        ? LevelText.Label("Project.PlaceAction", model.Name)
                        : LevelText.Label("Project.PlacePartsAction", model.Name, requests.Length),
                    project =>
                    {
                        foreach (ProjectResourcePlacementRequest request in requests)
                        {
                            rootIds.Add(project.AddReferencePlacementForResource(
                                request.MeshObjectId,
                                request.WorldTransform,
                                $"{request.Name}_copy"));
                        }
                    }));
            if (!changed)
            {
                SetBusy(false, LevelText.Get("Project.PlacementUnchanged"));
                return false;
            }
            await RefreshProjectPreviewAsync(
                LevelText.Get("Project.ReferencedModelPlaced", model.Name, requests.Length, target.X, target.Y, target.Z),
                rootIds);
            return true;
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Project.PlacementFailed"));
            ShowProjectError(LevelText.Get("Project.PlacementError"), exception);
            return false;
        }
    }

    private async Task UndoProjectAsync()
    {
        if (!HasProject || _isBusy)
            return;
        string? label = _projectSession!.UndoLabel;
        if (!_projectSession.Undo())
        {
            StatusText.Text = LevelText.Get("Project.UndoEmpty");
            return;
        }
        await RefreshProjectPreviewAsync(LevelText.Get("Project.UndoStatus", LevelText.Display(label)));
    }

    private static string? ResolveProjectFbxBridgePath()
    {
        string candidate = Path.Combine(AppContext.BaseDirectory, "SmoFbxBridge.exe");
        return File.Exists(candidate) ? candidate : null;
    }

    private async Task RedoProjectAsync()
    {
        if (!HasProject || _isBusy)
            return;
        string? label = _projectSession!.RedoLabel;
        if (!_projectSession.Redo())
        {
            StatusText.Text = LevelText.Get("Project.RedoEmpty");
            return;
        }
        await RefreshProjectPreviewAsync(LevelText.Get("Project.RedoStatus", LevelText.Display(label)));
    }

    private async Task RefreshProjectPreviewAsync(
        string status,
        IReadOnlyCollection<uint>? selectObjectIds = null)
    {
        try
        {
            SetBusy(true, LevelText.Get("Project.RebuildingPreview"));
            SmoLevelWorkspace workspace = await Task.Run(() =>
                BuildProjectPreviewWorkspace(_project!));
            AttachProjectPreview(workspace, selectObjectIds);
            SetBusy(false, status);
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Project.PreviewFailed"));
            ShowProjectError(LevelText.Get("Project.PreviewError"), exception);
        }
    }

    private void AttachProjectPreview(
        SmoLevelWorkspace workspace,
        IReadOnlyCollection<uint>? selectObjectIds = null,
        SmoLevelDocument? preparedDocument = null)
    {
        AttachWorkspace(
            workspace,
            savePath: null,
            preserveEditorState: true,
            preparedDocument: preparedDocument);
        RestoreProjectCollisionLinks();
        if (selectObjectIds is not null)
            SelectProjectObjects(selectObjectIds);
        ApplyProjectUiState();
        UpdateDocumentCaption();
    }

    private static SmoLevelWorkspace BuildProjectPreviewWorkspace(
        SmoProject project)
        => SmoLevelWorkspace.Create(
            SmoProjectSerializer.CreateCurrentDocument(project));

    private bool TryGetProjectCollisionLink(
        SmoLevelEntity visual,
        SmoLevelEntity collision,
        out SmoProjectCollisionLinkOverride link)
    {
        link = new SmoProjectCollisionLinkOverride();
        if (!HasProject || _workspace is null ||
            visual.Kind != SmoLevelEntityKind.Visual ||
            collision.Kind != SmoLevelEntityKind.Collision ||
            (uint)visual.Id.SceneObjectIndex >=
                (uint)_workspace.Document.Objects.Count ||
            (uint)collision.Id.SceneObjectIndex >=
                (uint)_workspace.Document.Objects.Count)
        {
            return false;
        }
        link = new SmoProjectCollisionLinkOverride
        {
            VisualObjectId =
                _workspace.Document.Objects[visual.Id.SceneObjectIndex].Id,
            CollisionObjectId =
                _workspace.Document.Objects[collision.Id.SceneObjectIndex].Id,
            Present = true
        };
        return true;
    }

    private void RememberProjectCollisionLink(
        SmoLevelEntity visual,
        SmoLevelEntity collision,
        bool present)
    {
        if (!TryGetProjectCollisionLink(
                visual,
                collision,
                out SmoProjectCollisionLinkOverride link))
            return;
        _projectSession!.Execute(
            LevelText.Label(present ? "Project.LinkCollisionAction" : "Project.UnlinkCollisionAction"),
            project => project.SetCollisionLinkOverride(
                link.VisualObjectId,
                link.CollisionObjectId,
                present));
    }

    private void RestoreProjectCollisionLinks()
    {
        if (!HasProject || _workspace is null || _document is null)
            return;
        Dictionary<uint, SmoLevelEntity> byObjectId = _document.Entities
            .Where(entity => entity.Id.SceneObjectIndex >= 0 &&
                entity.Id.SceneObjectIndex < _workspace.Document.Objects.Count)
            .ToDictionary(
                entity => _workspace.Document.Objects[entity.Id.SceneObjectIndex].Id,
                entity => entity);
        foreach (SmoProjectCollisionLinkOverride link in
                 _project!.Manifest.CollisionLinkOverrides.Where(item => !item.Present))
        {
            if (byObjectId.TryGetValue(link.VisualObjectId, out SmoLevelEntity? visual) &&
                byObjectId.TryGetValue(link.CollisionObjectId, out SmoLevelEntity? collision))
            {
                _document.RemoveCollisionLink(visual.Id, collision.Id);
            }
        }
        foreach (SmoProjectCollisionLinkOverride link in
                 _project.Manifest.CollisionLinkOverrides.Where(item => item.Present))
        {
            if (byObjectId.TryGetValue(link.VisualObjectId, out SmoLevelEntity? visual) &&
                byObjectId.TryGetValue(link.CollisionObjectId, out SmoLevelEntity? collision))
            {
                _document.SetCollisionLink(visual.Id, collision.Id);
            }
        }
    }

    private void ApplyProjectUiState()
    {
        bool active = HasProject;
        if (EditorToolsPanel is not null)
            EditorToolsPanel.IsEnabled = _document is not null;
        if (CatalogImportModelButton is not null)
        {
            CatalogImportModelButton.IsEnabled = !_isBusy && _document is not null;
            CatalogImportModelButton.SetResourceReference(ToolTipProperty, active
                ? "Loc.Project.ImportPlaceTooltip"
                : "Loc.Project.ImportTooltip");
        }
        if (AssetList?.ContextMenu is not null)
            AssetList.ContextMenu.IsEnabled = _document is not null;
        PositionXBox.IsReadOnly = false;
        PositionYBox.IsReadOnly = false;
        PositionZBox.IsReadOnly = false;
        RotationXBox.IsReadOnly = false;
        RotationYBox.IsReadOnly = false;
        RotationZBox.IsReadOnly = false;
        ScaleXBox.IsReadOnly = false;
        ScaleYBox.IsReadOnly = false;
        ScaleZBox.IsReadOnly = false;
        UpdateProjectCommandAvailability();
    }

    private void UpdateProjectCommandAvailability()
    {
        if (CreateProjectMenuItem is null)
            return;
        bool active = HasProject;
        CreateProjectMenuItem.IsEnabled = false;
        OpenProjectMenuItem.IsEnabled = false;
        SaveProjectMenuItem.IsEnabled = !_isBusy && active;
        SaveProjectAsMenuItem.IsEnabled = !_isBusy && active;
        BuildProjectSmoMenuItem.IsEnabled = !_isBusy && active;
        ValidateProjectMenuItem.IsEnabled = !_isBusy && active;
        AddProjectReferencePlacementMenuItem.IsEnabled =
            !_isBusy && active &&
            TryCaptureProjectPlacementClipboard(out _, out _);
        if (!active)
            return;
        if (CatalogPlaceButton is not null)
        {
            CatalogPlaceButton.IsEnabled = !_isBusy &&
                _catalogSection == CatalogSection.Models &&
                AssetList.SelectedItems.Count == 1;
        }
        ValidateInGameMenuItem.IsEnabled = false;
        bool hasCopyableSelection = !_isBusy && _document is not null &&
            _selectedEntities.Any(id =>
                _document.TryGetEntity(id, out SmoLevelEntity? entity) &&
                entity!.Kind == SmoLevelEntityKind.Visual);
        CopyPlacementMenuItem.IsEnabled = hasCopyableSelection;
        PastePlacementMenuItem.IsEnabled = !_isBusy &&
            _projectPlacementClipboard is not null;
        DuplicatePlacementMenuItem.IsEnabled = hasCopyableSelection;
        if (CreateCollisionButton is not null)
        {
            bool canRegenerate = _activeCollisionEntityIndex is int collisionIndex &&
                _document is not null &&
                _document.GetCollisionLinks(new SmoLevelEntityId(collisionIndex))
                    .Any();
            CreateCollisionButton.IsEnabled = !_isBusy && _document is not null &&
                (canRegenerate || _selectedEntities.Any(id =>
                    _document.TryGetEntity(id, out SmoLevelEntity? entity) &&
                    entity!.Kind == SmoLevelEntityKind.Visual &&
                    entity.Parts.Count > 0));
            CreateCollisionButton.SetResourceReference(ContentControl.ContentProperty, canRegenerate
                ? "Loc.Project.RegenerateCollisionCaption"
                : "Loc.Project.CreateCollisionCaption");
        }
        if (DeleteSelectedButton is not null)
        {
            DeleteSelectedButton.IsEnabled = !_isBusy &&
                TryCaptureProjectTransforms(
                    _selectedEntities,
                    out _,
                    out _);
        }
    }

    private bool TryCaptureProjectPlacementClipboard(
        out ProjectPlacementClipboard? clipboard,
        out string reason)
    {
        clipboard = null;
        if (!HasProject || _document is null || _workspace is null)
        {
            reason = LevelText.Get("Project.ProjectNotOpen");
            return false;
        }

        ProjectPlacementClipboardPart[] parts = _selectedEntities
            .Where(id => _document.TryGetEntity(id, out SmoLevelEntity? entity) &&
                         entity!.Kind == SmoLevelEntityKind.Visual)
            .Select(_document.GetEntity)
            .SelectMany(entity => entity.Parts)
            .Where(part => (uint)part.Asset.ObjectIndex <
                           (uint)_workspace.Document.Objects.Count)
            .Select(part => new ProjectPlacementClipboardPart(
                _workspace.Document.Objects[part.Asset.ObjectIndex].Id,
                part.WorldTransform,
                part.Source.Name.TrimEnd('\0')))
            .GroupBy(part => (
                part.MeshObjectId,
                part.WorldTransform,
                part.Name))
            .Select(group => group.First())
            .ToArray();
        if (parts.Length == 0)
        {
            reason = LevelText.Get("Project.NoSelectedVisualPlacement");
            return false;
        }

        string name = _selectedEntities.Count == 1
            ? _document.GetEntity(_selectedEntities.First()).Name
            : $"Selection_{_selectedEntities.Count}";
        Vector3 anchor = parts.Aggregate(
                Vector3.Zero,
                (sum, part) => sum + part.WorldTransform.Translation) /
            parts.Length;
        clipboard = new ProjectPlacementClipboard(name, parts, anchor);
        reason = string.Empty;
        return true;
    }

    private async Task PasteProjectPlacementClipboardAsync(
        ProjectPlacementClipboard clipboard,
        Vector3 target,
        string action)
    {
        if (!HasProject || _isBusy)
            return;
        Vector3 delta = target - clipboard.SourceAnchor;
        var newRootIds = new List<uint>(clipboard.Parts.Length);
        try
        {
            bool changed = _projectSession!.Execute(
                clipboard.Parts.Length == 1
                    ? LevelText.Label("Project.PlaceAction", clipboard.Name)
                    : LevelText.Label("Project.PlacePartsAction", clipboard.Name, clipboard.Parts.Length),
                project =>
                {
                    foreach (ProjectPlacementClipboardPart part in clipboard.Parts)
                    {
                        Matrix4x4 world = part.WorldTransform;
                        world.M41 += delta.X;
                        world.M42 += delta.Y;
                        world.M43 += delta.Z;
                        newRootIds.Add(project.AddReferencePlacementForResource(
                            part.MeshObjectId,
                            world,
                            $"{part.Name}_copy"));
                    }
                });
            if (!changed)
                return;
            await RefreshProjectPreviewAsync(
                LevelText.Get("Project.ReferencePlacementPosition", action, clipboard.Parts.Length, target.X, target.Y, target.Z),
                newRootIds);
        }
        catch (Exception exception)
        {
            ShowProjectError(LevelText.Get("Project.ReferencePlacementError"), exception);
        }
    }

    private bool TryResolveProjectPlacementTemplates(
        out ProjectPlacementTemplate[] templates,
        out string reason)
    {
        templates = [];
        if (!HasProject || _document is null || _workspace is null)
        {
            reason = LevelText.Get("Project.ProjectNotOpen");
            return false;
        }
        if (_selectedEntities.Count == 0)
        {
            reason = LevelText.Get("Project.SelectVisualInScene");
            return false;
        }

        var resolved = new List<ProjectPlacementTemplate>();
        foreach (SmoLevelEntityId entityId in _selectedEntities.OrderBy(
                     item => item.SceneObjectIndex))
        {
            if (!_document.TryGetEntity(entityId, out SmoLevelEntity? entity) ||
                entity!.Kind != SmoLevelEntityKind.Visual)
                continue;
            if ((uint)entityId.SceneObjectIndex >=
                (uint)_workspace.Document.Objects.Count)
            {
                reason = LevelText.Get("Project.PreviewCatalogObjectMissing", entityId.SceneObjectIndex);
                return false;
            }
            uint objectId = _workspace.Document.Objects[entityId.SceneObjectIndex].Id;
            SmoProjectObject? source = _project!.Objects
                .SingleOrDefault(item => item.Id == objectId);
            if (source is null)
            {
                SmoProjectReferencePlacement? generated =
                    _project.Manifest.ReferencePlacements.SingleOrDefault(item =>
                        item.NewObjectIds.Count > 0 && item.NewObjectIds[0] == objectId);
                if (generated is not null)
                {
                    source = _project.Objects.Single(item =>
                        item.Id == generated.TemplateRootObjectId);
                }
                else
                {
                    reason = LevelText.Get("Project.PlacementTemplateMissing", objectId);
                    return false;
                }
            }
            if (!_project.CanAddReferencePlacement(
                    source.Index,
                    out reason))
                return false;
            Matrix4x4 world = entity.WorldTransform;
            resolved.Add(new ProjectPlacementTemplate(
                source.Index,
                entity.Name,
                new Vector3(world.M41, world.M42, world.M43)));
        }
        if (resolved.Count == 0)
        {
            reason = LevelText.Get("Project.NoSelectedVisualPlacement");
            return false;
        }
        templates = resolved.ToArray();
        reason = string.Empty;
        return true;
    }

    private void SelectProjectObjects(IReadOnlyCollection<uint> objectIds)
    {
        if (_workspace is null || _document is null)
            return;
        HashSet<uint> ids = objectIds.ToHashSet();
        _selectedEntities.Clear();
        int? collisionInfoIndex = null;
        foreach (SmoObjectEntry entry in _workspace.Document.Objects.Where(item =>
                     ids.Contains(item.Id)))
        {
            var entityId = new SmoLevelEntityId(entry.Index);
            if (_document.TryGetEntity(entityId, out SmoLevelEntity? entity) &&
                entity!.Kind == SmoLevelEntityKind.Visual)
            {
                _selectedEntities.Add(entityId);
            }
            else if (entry.TypeHash == SmoClassIds.CollisionInfo)
            {
                collisionInfoIndex ??= entry.Index;
            }
        }
        if (_selectedEntities.Count == 0 && collisionInfoIndex is int collisionIndex)
        {
            SelectCollisionFromTree(collisionIndex, focus: false);
            return;
        }
        ActivateFirstSelectedEntity();
        ApplyPlacementHighlights();
        if (TryResolveActiveSelection(
                out EditorAssetItem? item,
                out _,
                out int placementIndex))
        {
            RevealSelection(item!, placementIndex, updateViewportSelection: false);
        }
    }

    private bool TryCaptureProjectTransforms(
        IEnumerable<SmoLevelEntityId> entityIds,
        out ProjectTransformTarget[] targets,
        out string reason)
    {
        targets = [];
        if (!HasProject || _document is null || _workspace is null)
        {
            reason = LevelText.Get("Project.ProjectNotOpen");
            return false;
        }

        var resolved = new List<ProjectTransformTarget>();
        foreach (SmoLevelEntityId entityId in entityIds.Distinct().OrderBy(
                     item => item.SceneObjectIndex))
        {
            if (!_document.TryGetEntity(entityId, out SmoLevelEntity? entity))
            {
                reason = LevelText.Get("Project.ProjectPreviewObjectMissing", entityId.SceneObjectIndex);
                return false;
            }
            if ((uint)entityId.SceneObjectIndex >=
                (uint)_workspace.Document.Objects.Count)
            {
                reason = LevelText.Get("Project.PreviewCatalogObjectMissing", entityId.SceneObjectIndex);
                return false;
            }
            uint objectId = _workspace.Document.Objects[entityId.SceneObjectIndex].Id;
            if (entity!.Kind == SmoLevelEntityKind.Collision)
            {
                if (_workspace.Document.Objects[entityId.SceneObjectIndex].TypeHash !=
                        SmoClassIds.CollisionInfo ||
                    !SmoPlacementTransformWriter.CanWriteCollisionTransform(
                        _workspace.Document,
                        entityId.SceneObjectIndex))
                {
                    reason = LevelText.Get("Project.CollisionGeometryMissing", objectId);
                    return false;
                }
                resolved.Add(new ProjectTransformTarget(
                    objectId,
                    SmoLevelEntityKind.Collision,
                    entity.OriginalWorldTransform,
                    entity.WorldTransform));
                continue;
            }
            bool imported = _project!.Objects.Any(item =>
                item.Id == objectId && item.TypeHash == SmoClassIds.StaticRenderObject);
            bool generated = _project.Manifest.ReferencePlacements.Any(item =>
                item.NewObjectIds.Count > 0 && item.NewObjectIds[0] == objectId);
            bool added = _project.Manifest.AddedForests.Any(forest =>
                forest.Objects.Any(item =>
                    item.Id == objectId &&
                    item.TypeHash == SmoClassIds.StaticRenderObject));
            if (!imported && !generated && !added)
            {
                reason = LevelText.Get("Project.UnsupportedPlacementTransform", objectId);
                return false;
            }
            resolved.Add(new ProjectTransformTarget(
                objectId,
                SmoLevelEntityKind.Visual,
                entity.OriginalWorldTransform,
                entity.WorldTransform));
        }
        if (resolved.Count == 0)
        {
            reason = LevelText.Get("Project.SelectVisualPlacement");
            return false;
        }
        targets = resolved.ToArray();
        reason = string.Empty;
        return true;
    }

    private async Task CommitProjectTransformsAsync(
        string label,
        IReadOnlyList<ProjectTransformTarget> targets)
    {
        try
        {
            KeyValuePair<uint, Matrix4x4>[] placements = targets
                .Where(item => item.Kind == SmoLevelEntityKind.Visual)
                .Select(item => KeyValuePair.Create(item.ObjectId, item.WorldTransform))
                .ToArray();
            SmoProjectCollisionTransformEdit[] collisions = targets
                .Where(item => item.Kind == SmoLevelEntityKind.Collision)
                .Select(item => new SmoProjectCollisionTransformEdit(
                    item.ObjectId,
                    item.OriginalWorldTransform,
                    item.WorldTransform))
                .ToArray();
            bool changed = _projectSession!.ExecutePlacementTransforms(
                label, placements, collisions);
            if (!changed)
                return;
            await RefreshProjectPreviewAsync(
                LevelText.Get("Project.TransformRecorded", LevelText.Display(label), targets.Count),
                targets.Select(item => item.ObjectId).ToArray());
        }
        catch (Exception exception)
        {
            await RefreshProjectPreviewAsync(
                LevelText.Get("Project.TransformCancelled"));
            ShowProjectError(LevelText.Get("Project.TransformError"), exception);
        }
    }

    private void CommitCurrentProjectTransforms(
        string label,
        IEnumerable<SmoLevelEntityId> entityIds)
    {
        if (TryCaptureProjectTransforms(
                entityIds,
                out ProjectTransformTarget[] targets,
                out string reason))
        {
            _ = CommitProjectTransformsAsync(label, targets);
            return;
        }
        StatusText.Text = reason;
        _ = RefreshProjectPreviewAsync(
            LevelText.Get("Project.TransformCancelled"));
    }

    private async Task DeleteProjectSelectionAsync()
    {
        if (_isBusy || _projectSession is not SmoProjectSession session)
            return;
        if (!TryCaptureProjectTransforms(
                _selectedEntities,
                out ProjectTransformTarget[] targets,
                out string reason))
        {
            StatusText.Text = reason;
            return;
        }
        MessageBoxResult choice = MessageBox.Show(
            this,
            LevelText.Get("Project.ConfirmDelete", targets.Length),
            LevelText.Get("Project.DeleteTitle"),
            MessageBoxButton.YesNo,
            MessageBoxImage.Question,
            MessageBoxResult.No);
        if (choice != MessageBoxResult.Yes)
            return;

        string label = targets.Length == 1
            ? LevelText.Label("Project.DeletePlacementAction")
            : LevelText.Label("Project.DeletePlacementsAction", targets.Length);
        try
        {
            _projectDeletionInProgress = true;
            SetBusy(true, LevelText.Get("Project.PreparingDelete"));
            uint[] objectIds = targets.Select(item => item.ObjectId).ToArray();
            SmoLevelDocument? preview = await Task.Run(() =>
                PrepareProjectDeletion(session, label, objectIds));
            if (preview is null)
            {
                SetBusy(false, LevelText.Get("Project.ProjectUnchanged"));
                return;
            }
            _selectedEntities.Clear();
            AttachProjectPreview(preview.Workspace, preparedDocument: preview);
            SetBusy(false,
                LevelText.Get("Project.PlacementsDeleted", targets.Length));
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Project.DeleteFailed"));
            ShowProjectError(LevelText.Get("Project.DeleteError"), exception);
        }
        finally
        {
            _projectDeletionInProgress = false;
        }
    }

    private static SmoLevelDocument? PrepareProjectDeletion(
        SmoProjectSession session,
        string label,
        IReadOnlyCollection<uint> objectIds)
    {
        SmoLevelDocument? preview = null;
        bool changed = session.Execute(label, project =>
        {
            project.RemoveSceneBranches(objectIds);
            // A bad preview must roll back the deletion before it enters history.
            preview = new SmoLevelDocument(BuildProjectPreviewWorkspace(project));
        });
        return changed ? preview : null;
    }

    private void RestoreOrdinaryMenuHeaders()
    {
        SaveMenuItem.SetResourceReference(HeaderedItemsControl.HeaderProperty, "Loc.Project.SaveCaption");
        SaveAsMenuItem.SetResourceReference(HeaderedItemsControl.HeaderProperty, "Loc.Project.SaveAsCaption");
    }

    private void RefreshProjectLanguage() => UpdateProjectCommandAvailability();

    private bool ConfirmDiscardProjectChanges()
    {
        if (_projectSession?.IsModified != true)
            return true;
        return MessageBox.Show(
                   this,
                   LevelText.Get("Project.DiscardPrompt"),
                   LevelText.Get("Project.DiscardTitle"),
                   MessageBoxButton.YesNo,
                   MessageBoxImage.Warning,
                   MessageBoxResult.No) == MessageBoxResult.Yes;
    }

    private void MainWindow_Closing(
        object? sender,
        System.ComponentModel.CancelEventArgs e)
    {
        if (_projectDeletionInProgress || !ConfirmDiscardProjectChanges())
            e.Cancel = true;
    }

    private void ShowProjectError(string title, Exception exception) =>
        MessageBox.Show(
            this,
            LevelText.Error(exception.Message),
            LevelText.Get("Project.ErrorTitle", title),
            MessageBoxButton.OK,
            MessageBoxImage.Error);

    private sealed record ProjectPlacementTemplate(
        int ObjectIndex,
        string Name,
        Vector3 CurrentPosition);

    private sealed record ProjectTransformTarget(
        uint ObjectId,
        SmoLevelEntityKind Kind,
        Matrix4x4 OriginalWorldTransform,
        Matrix4x4 WorldTransform);

    private sealed record ProjectPlacementClipboard(
        string Name,
        ProjectPlacementClipboardPart[] Parts,
        Vector3 SourceAnchor);

    private sealed record ProjectPlacementClipboardPart(
        uint MeshObjectId,
        Matrix4x4 WorldTransform,
        string Name);

    private sealed record ProjectResourcePlacementRequest(
        uint MeshObjectId,
        Matrix4x4 WorldTransform,
        string Name);

}
