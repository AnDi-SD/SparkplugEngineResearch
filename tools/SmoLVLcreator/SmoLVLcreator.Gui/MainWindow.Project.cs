using Microsoft.Win32;
using SmoImporter.Core;
using SmoLVLcreator.Core;
using SmoViewer.Core;
using System.IO;
using System.Numerics;
using System.Windows;

namespace SmoLVLcreator.Gui;

public partial class MainWindow
{
    private SmoProject? _project;
    private SmoProjectSession? _projectSession;
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
                "Сначала сохраните или заново откройте обычный SMO. " +
                "Проект импортирует только исходный контейнер, " +
                "не смешивая его со старым журналом редактора.",
                "SmoLVLcreator — проект уровня",
                MessageBoxButton.OK,
                MessageBoxImage.Information);
            return;
        }

        string sourcePath = _workspace.SourcePath;
        var dialog = new SaveFileDialog
        {
            Title = "Создать проект уровня",
            Filter = "SmoLVLcreator project (*.smolvlproj)|*.smolvlproj",
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
            SetBusy(true, $"Создание {Path.GetFileName(dialog.FileName)}…");
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
                $"Проект создан · {project.Objects.Count:N0} объектов · " +
                "исходный data.bin неизменяем");
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось создать проект");
            ShowProjectError("Ошибка создания проекта", exception);
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
            Title = "Открыть проект уровня",
            Filter = "SmoLVLcreator project (*.smolvlproj)|*.smolvlproj",
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
            SetBusy(true, $"Открытие {Path.GetFileName(fullPath)}…");
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
                $"Проект открыт · " +
                $"{loaded.Project.Objects.Count:N0} исходных объектов · " +
                $"{loaded.Project.Manifest.ReferencePlacements.Count:N0} новых размещений");
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось открыть проект");
            ShowProjectError("Ошибка открытия проекта", exception);
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
            Title = "Сохранить проект уровня",
            Filter = "SmoLVLcreator project (*.smolvlproj)|*.smolvlproj",
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
            SetBusy(true, $"Сохранение {Path.GetFileName(fullPath)}…");
            await Task.Run(() =>
                SmoProjectArchive.Save(_project!, fullPath));
            _projectPath = fullPath;
            _projectSession!.MarkSaved();
            UpdateDocumentCaption();
            SetBusy(false,
                $"Проект сохранён · data.bin {_project!.DataSection.Length:N0} байт " +
                "не перепаковывался");
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось сохранить проект");
            ShowProjectError("Ошибка сохранения проекта", exception);
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
            Title = "Собрать SMO из проекта",
            Filter = "Sparkplug model or level (*.smo)|*.smo",
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
                $"По приблизительной оценке сборке может понадобиться около " +
                $"{memory.EstimatedPeakMiB:N0} МиБ памяти.\n\n" +
                $"Сейчас свободно: {memory.AvailablePhysicalMiB:N0} МиБ.\n" +
                $"Рекомендуемый объём с запасом для Windows: " +
                $"{memory.RecommendedAvailableMiB:N0} МиБ.\n\n" +
                "Это предупреждение, а не запрет. Сборка будет запущена в " +
                $"отдельном процессе с верхним пределом {memory.WorkerLimitMiB:N0} МиБ; " +
                "при его достижении завершится только сборка, а проект останется цел.\n\n" +
                "Продолжить?",
                "SmoLVLcreator — оценка памяти",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning,
                MessageBoxResult.No);
            if (choice != MessageBoxResult.Yes)
            {
                StatusText.Text = "Сборка отменена после предупреждения о памяти";
                return;
            }
        }
        _isSaving = true;
        using var buildCancellation = new CancellationTokenSource();
        _saveCancellation = buildCancellation;
        try
        {
            SetBusy(true, $"Сборка {Path.GetFileName(outputPath)}…");
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
                    $"{update.CompletedSteps:N0} / {update.TotalSteps:N0} · " +
                    $"{update.CurrentContainerBytes / (1024d * 1024d):N1} МиБ";
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
                : $" · backup: {Path.GetFileName(result.BackupPath)}";
            SetBusy(false,
                $"SMO собран и повторно проверен · {result.FileSize:N0} байт · " +
                $"{result.ObjectCount:N0} объектов{backup} · log: " +
                Path.GetFileName(diagnosticLogPath));
        }
        catch (OperationCanceledException)
        {
            SetBusy(false, "Сборка отменена · проект и существующий SMO не повреждены");
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось собрать SMO");
            SmoLevelSaveService.TryAppendDiagnostic(
                diagnosticLogPath,
                "ERROR",
                "GUI_PROJECT_BUILD_ERROR",
                exception.ToString());
            MessageBox.Show(
                this,
                exception.Message +
                $"\n\nПодробный журнал:\n{diagnosticLogPath}",
                "SmoLVLcreator — ошибка сборки SMO",
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
            SetBusy(true, "Сборка проекта для нативной проверки…");
            SmoProjectBuildResult result =
                await SmoProjectIsolatedBuildClient.BuildAsync(
                    _project,
                    candidatePath,
                    progress: null,
                    CancellationToken.None);
            SetBusy(false,
                $"Кандидат собран · {result.FileSize:N0} байт · " +
                $"{result.ObjectCount:N0} объектов");
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
            SetBusy(false, "Не удалось подготовить проект к проверке");
            ShowProjectError("Ошибка подготовки нативной проверки", exception);
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
                "SmoLVLcreator — ссылочное размещение",
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
            "Размещена ссылочная копия");
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
            SetBusy(true, "Подготовка проектной коллизии…");
            SmoProjectCollisionAddition? addition = null;
            bool changed = await Task.Run(() =>
                _projectSession!.Execute(
                    replacedCollisionObjectId is null
                        ? $"Создать коллизию {name}"
                        : $"Пересоздать коллизию {name}",
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
                SetBusy(false, "Проектная коллизия не изменила уровень");
                return;
            }
            await RefreshProjectPreviewAsync(
                $"{(replacedCollisionObjectId is null ? "Создана" : "Пересоздана")} проектная коллизия · " +
                $"{generated.Positions.Count:N0} вершин · " +
                $"{generated.TriangleCount:N0} треугольников",
                [addition.CollisionInfoObjectId]);
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось создать проектную коллизию");
            ShowProjectError("Ошибка проектной коллизии", exception);
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
            SetBusy(true, $"Подготовка проектной модели {name}…");
            SmoProjectExternalModelAddition? addition = null;
            bool changed = await Task.Run(() =>
                _projectSession!.Execute(
                    $"Импортировать и разместить {name}",
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
                SetBusy(false, "Импорт не изменил проект");
                return;
            }
            await RefreshProjectPreviewAsync(
                $"Импортирована модель {name} · " +
                $"{addition.MeshObjectIds.Count:N0} мешей · " +
                $"{addition.ImportedTextureObjectIds.Count:N0} текстур · " +
                $"{target.X:0.##}, {target.Y:0.##}, {target.Z:0.##}",
                addition.PlacementRootObjectIds);
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось импортировать модель в проект");
            ShowProjectError("Ошибка проектного импорта модели", exception);
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
                "Опорная матрица заменяемой модели необратима.");

        var oldRoots = new List<uint>();
        var sourceWorlds = new List<Matrix4x4>();
        if (item.CompositeModels.Length > 0)
        {
            foreach (SmoCompositeModel instance in item.CompositeModels)
            {
                sourceWorlds.Add(instance.Entities[0].WorldTransform);
                foreach (SmoLevelEntity entity in instance.Entities)
                {
                    int sceneIndex = entity.Id.SceneObjectIndex;
                    if ((uint)sceneIndex >= (uint)_workspace.Document.Objects.Count)
                        throw new InvalidOperationException(
                            $"Размещение [{sceneIndex}] отсутствует в проектном preview.");
                    uint objectId = _workspace.Document.Objects[sceneIndex].Id;
                    oldRoots.Add(objectId);
                }
            }
        }
        else
        {
            SmoLevelAsset asset = item.VisualParts
                .Select(part => part.AssetItem.Asset)
                .FirstOrDefault(value => value is not null)
                ?? throw new InvalidOperationException(
                    "У выбранной модели нет размещений для полной замены.");
            foreach (SmoLevelPlacement placement in asset.Placements)
            {
                var entityId = new SmoLevelEntityId(placement.SceneObjectIndex);
                if (_document.RemovedEntityIds.Contains(entityId))
                    continue;
                Matrix4x4 world = _document.TryGetEntity(
                        entityId,
                        out SmoLevelEntity? entity)
                    ? entity!.WorldTransform
                    : placement.WorldTransform;
                uint objectId = _workspace.Document.Objects[placement.SceneObjectIndex].Id;
                oldRoots.Add(objectId);
                sourceWorlds.Add(world);
            }
        }

        uint[] oldRootIds = oldRoots.Distinct().ToArray();
        if (oldRootIds.Length == 0)
            throw new InvalidOperationException("У модели не осталось размещений для замены.");
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
                    $"Размещение {objectId} отсутствует в текущем проекте.");
            }
        }

        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        Matrix4x4 fittedReferenceWorld = reflection * fit.Matrix * reflection;
        Matrix4x4[] replacementWorlds = sourceWorlds
            .Select(world => world * inverseReference * fittedReferenceWorld)
            .ToArray();
        if (replacementWorlds.Any(world => !Matrix4x4.Invert(world, out _)))
            throw new InvalidOperationException(
                "После подгонки одно из новых размещений получило необратимую матрицу.");

        string modelName = string.IsNullOrWhiteSpace(item.Name)
            ? Path.GetFileNameWithoutExtension(sourcePath)
            : item.Name.Trim();
        SmoWorkerHostInfo workerHost = SmoWorkerHost.ResolveCurrent();

        try
        {
            SetBusy(true, $"Полная замена {modelName}: подготовка новых ресурсов…");
            SmoProjectExternalModelAddition? addition = null;
            bool changed = await Task.Run(() =>
                _projectSession!.Execute(
                    $"Полностью заменить {modelName}",
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
                        project.RemoveSceneBranches(oldRootIds);
                    }));
            if (!changed || addition is null)
            {
                SetBusy(false, "Полная замена не изменила проект");
                return;
            }
            await RefreshProjectPreviewAsync(
                $"{modelName} заменена целиком · " +
                $"{oldRootIds.Length:N0} старых размещений → " +
                $"{addition.PlacementRootObjectIds.Count:N0} новых частей · " +
                $"{addition.MeshObjectIds.Count:N0} мешей · " +
                $"{addition.ImportedTextureObjectIds.Count:N0} текстур",
                addition.PlacementRootObjectIds);
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось выполнить полную замену модели");
            ShowProjectError("Ошибка полной замены модели", exception);
        }
    }

    private async Task<bool> PlaceProjectCatalogModelAsync(
        CatalogModelDrag model,
        Vector3 target)
    {
        if (!HasProject || _workspace is null || _isBusy)
            return false;
        if (model.Parts.Length == 0)
            throw new InvalidOperationException("У модели нет размещаемых mesh-частей.");

        Vector3 delta = target - model.SourceAnchor;
        ProjectResourcePlacementRequest[] requests = model.Parts.Select(part =>
        {
            if ((uint)part.MeshObjectIndex >= (uint)_workspace.Document.Objects.Count)
                throw new InvalidOperationException("Mesh-ресурс каталога больше не существует.");
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
            SetBusy(true, $"Размещение {model.Name} в проекте…");
            var rootIds = new List<uint>(requests.Length);
            bool changed = await Task.Run(() =>
                _projectSession!.Execute(
                    requests.Length == 1
                        ? $"Разместить {model.Name}"
                        : $"Разместить {model.Name} ({requests.Length} частей)",
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
                SetBusy(false, "Размещение не изменило проект");
                return false;
            }
            await RefreshProjectPreviewAsync(
                $"Размещена ссылочная модель {model.Name} · " +
                $"{requests.Length:N0} частей · " +
                $"{target.X:0.##}, {target.Y:0.##}, {target.Z:0.##}",
                rootIds);
            return true;
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось разместить модель в проекте");
            ShowProjectError("Ошибка проектного размещения", exception);
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
            StatusText.Text = "Undo · история проекта пуста";
            return;
        }
        await RefreshProjectPreviewAsync($"Undo · {label}");
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
            StatusText.Text = "Redo · история проекта пуста";
            return;
        }
        await RefreshProjectPreviewAsync($"Redo · {label}");
    }

    private async Task RefreshProjectPreviewAsync(
        string status,
        IReadOnlyCollection<uint>? selectObjectIds = null)
    {
        try
        {
            SetBusy(true, "Пересборка preview проекта…");
            SmoLevelWorkspace workspace = await Task.Run(() =>
                BuildProjectPreviewWorkspace(_project!));
            AttachWorkspace(
                workspace,
                savePath: null,
                preserveEditorState: true);
            RestoreProjectCollisionLinks();
            if (selectObjectIds is not null)
                SelectProjectObjects(selectObjectIds);
            ApplyProjectUiState();
            UpdateDocumentCaption();
            SetBusy(false, status);
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось обновить preview проекта");
            ShowProjectError("Ошибка preview проекта", exception);
        }
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
            present ? "Link visual and collision" : "Unlink visual and collision",
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
            CatalogImportModelButton.ToolTip = active
                ? "Импортировать модель и создать первое размещение в точке взгляда камеры"
                : "Добавить внешнюю модель в каталог";
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
            CreateCollisionButton.Content = canRegenerate
                ? "Регенерировать коллизию"
                : "Создать коллизию";
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
            reason = "Проект уровня не открыт.";
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
            reason = "В выделении нет размещённой визуальной модели.";
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
                    ? $"Разместить {clipboard.Name}"
                    : $"Разместить {clipboard.Name} ({clipboard.Parts.Length} частей)",
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
                $"{action} · {clipboard.Parts.Length:N0} частей · " +
                $"{target.X:0.##}, {target.Y:0.##}, {target.Z:0.##}",
                newRootIds);
        }
        catch (Exception exception)
        {
            ShowProjectError("Ошибка ссылочного размещения", exception);
        }
    }

    private bool TryResolveProjectPlacementTemplates(
        out ProjectPlacementTemplate[] templates,
        out string reason)
    {
        templates = [];
        if (!HasProject || _document is null || _workspace is null)
        {
            reason = "Проект уровня не открыт.";
            return false;
        }
        if (_selectedEntities.Count == 0)
        {
            reason = "Выберите размещённую модель в сцене.";
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
                reason = $"Объект [{entityId.SceneObjectIndex}] отсутствует в preview-каталоге.";
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
                    reason = $"Для размещения {objectId} не найден исходный шаблон.";
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
            reason = "В выделении нет размещённой визуальной модели.";
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
            reason = "Проект уровня не открыт.";
            return false;
        }

        var resolved = new List<ProjectTransformTarget>();
        foreach (SmoLevelEntityId entityId in entityIds.Distinct().OrderBy(
                     item => item.SceneObjectIndex))
        {
            if (!_document.TryGetEntity(entityId, out SmoLevelEntity? entity))
            {
                reason = $"Объект [{entityId.SceneObjectIndex}] отсутствует в проектном preview.";
                return false;
            }
            if ((uint)entityId.SceneObjectIndex >=
                (uint)_workspace.Document.Objects.Count)
            {
                reason = $"Объект [{entityId.SceneObjectIndex}] отсутствует в preview-каталоге.";
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
                    reason = $"Коллизия {objectId} не содержит подтверждённую геометрию spMeshBV.";
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
                reason = $"Размещение {objectId} не поддерживает проектную трансформацию.";
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
            reason = "Выберите размещённую визуальную модель.";
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
            bool changed = _projectSession!.Execute(label, project =>
            {
                foreach (ProjectTransformTarget target in targets.Where(item =>
                             item.Kind == SmoLevelEntityKind.Visual))
                    project.SetPlacementTransform(target.ObjectId, target.WorldTransform);
                SmoProjectCollisionTransformEdit[] collisions = targets
                    .Where(item => item.Kind == SmoLevelEntityKind.Collision)
                    .Select(item => new SmoProjectCollisionTransformEdit(
                        item.ObjectId,
                        item.OriginalWorldTransform,
                        item.WorldTransform))
                    .ToArray();
                project.SetCollisionTransforms(collisions);
            });
            if (!changed)
                return;
            await RefreshProjectPreviewAsync(
                $"{label} · {targets.Count:N0} размещений · записано в журнал проекта",
                targets.Select(item => item.ObjectId).ToArray());
        }
        catch (Exception exception)
        {
            await RefreshProjectPreviewAsync(
                "Проектная трансформация отменена; preview восстановлен");
            ShowProjectError("Ошибка проектной трансформации", exception);
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
            "Проектная трансформация отменена; preview восстановлен");
    }

    private async Task DeleteProjectSelectionAsync()
    {
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
            $"Удалить объектов: {targets.Length:N0}?\n\n" +
            "Общие mesh/material/texture ресурсы сохранятся. " +
            "Если исходная ветка физически владеет общим ресурсом, проект " +
            "сначала перенесёт владение к оставшемуся потребителю.",
            "SmoLVLcreator — удалить из проекта",
            MessageBoxButton.YesNo,
            MessageBoxImage.Question,
            MessageBoxResult.No);
        if (choice != MessageBoxResult.Yes)
            return;

        string label = targets.Length == 1
            ? "Удалить размещение"
            : $"Удалить размещения ({targets.Length})";
        try
        {
            bool changed = _projectSession!.Execute(label, project =>
                project.RemoveSceneBranches(
                    targets.Select(item => item.ObjectId).ToArray()));
            if (!changed)
                return;
            _selectedEntities.Clear();
            await RefreshProjectPreviewAsync(
                $"Удалено размещений: {targets.Length:N0} · общие ресурсы сохранены");
        }
        catch (Exception exception)
        {
            ShowProjectError("Ошибка удаления из проекта", exception);
        }
    }

    private void RestoreOrdinaryMenuHeaders()
    {
        SaveMenuItem.Header = "Сохранить проект";
        SaveAsMenuItem.Header = "Сохранить проект как…";
    }

    private bool ConfirmDiscardProjectChanges()
    {
        if (_projectSession?.IsModified != true)
            return true;
        return MessageBox.Show(
                   this,
                   "В проекте есть несохранённые операции. " +
                   "Закрыть его без сохранения?",
                   "SmoLVLcreator — несохранённый проект",
                   MessageBoxButton.YesNo,
                   MessageBoxImage.Warning,
                   MessageBoxResult.No) == MessageBoxResult.Yes;
    }

    private void MainWindow_Closing(
        object? sender,
        System.ComponentModel.CancelEventArgs e)
    {
        if (!ConfirmDiscardProjectChanges())
            e.Cancel = true;
    }

    private void ShowProjectError(string title, Exception exception) =>
        MessageBox.Show(
            this,
            exception.Message,
            $"SmoLVLcreator — {title}",
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
