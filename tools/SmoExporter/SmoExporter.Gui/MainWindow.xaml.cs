using System.IO;
using System.Collections.ObjectModel;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using Microsoft.Win32;
using SmoExporter.Core;
using SmoViewer.Core;

namespace SmoExporter.Gui;

public partial class MainWindow : Window
{
    private string? _sourcePath;
    private string? _outputDirectory;
    private string? _blenderPath;
    private string? _configuredBlenderPath;
    private bool _busy;
    private bool _resourceUiReady;
    private bool _updatingResourceTypes;
    private bool _updatingAnimationGroups;
    private bool _updatingContentMode;
    private SmoExportContentProfile? _contentProfile;
    private readonly ObservableCollection<AnimationChoice> _animations = [];
    private readonly ObservableCollection<LevelElementChoice> _levelElements = [];
    private readonly List<LevelElementChoice> _allLevelElements = [];
    private readonly Dictionary<string, HashSet<string>> _animationGroupsByPath =
        new(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<string> _enabledAnimationGroups =
        new(StringComparer.OrdinalIgnoreCase);
    private static readonly string SettingsPath = Path.Combine(
        GetLocalApplicationDataPath(),
        "SparkplugEngineResearch", "SmoExporter", "settings.json");

    private static string GetLocalApplicationDataPath()
    {
        string? configured = Environment.GetEnvironmentVariable("LOCALAPPDATA");
        return string.IsNullOrWhiteSpace(configured)
            ? Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData)
            : Path.GetFullPath(configured);
    }

    public MainWindow()
    {
        InitializeComponent();
        _resourceUiReady = true;
        SetSelectedResourceTypes(SmoExportResourceTypes.All);
        AnimationList.ItemsSource = _animations;
        LevelElementList.ItemsSource = _levelElements;
        _configuredBlenderPath = LoadConfiguredBlenderPath();
        BlenderPathTextBox.Text = _configuredBlenderPath ?? string.Empty;
        CheckBlenderAvailability(writeLog: true);
        AddLog("SmoExporter запущен.");
        string[] arguments = Environment.GetCommandLineArgs().Skip(1).ToArray();
        string? commandLineModel = arguments
            .FirstOrDefault(argument => argument.EndsWith(".smo", StringComparison.OrdinalIgnoreCase) && File.Exists(argument));
        if (commandLineModel is not null)
        {
            string? viewerAnimationList = GetOption(arguments, "--viewer-animation-list");
            LoadModel(commandLineModel, discoverAnimations: viewerAnimationList is null);
            if (viewerAnimationList is not null)
            {
                if (GetEffectiveContentKind() == SmoExportContentKind.Character)
                {
                    LoadViewerAnimations(viewerAnimationList);
                }
                else
                {
                    try { File.Delete(viewerAnimationList); }
                    catch { }
                    AddLog("Каталог SAN из Viewer не используется для профиля уровня.");
                }
            }
        }
    }

    private void SelectModel_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Title = "Выберите модель Sparkplug",
            Filter = "Sparkplug model (*.smo)|*.smo|Все файлы (*.*)|*.*",
            CheckFileExists = true,
            Multiselect = false
        };
        if (dialog.ShowDialog(this) != true)
            return;

        LoadModel(dialog.FileName);
    }

    private void LoadModel(string path, bool discoverAnimations = true)
    {
        _sourcePath = Path.GetFullPath(path);
        SmoDocument document = SmoDocument.Load(_sourcePath);
        _contentProfile = SmoExportContentProfileAnalyzer.Analyze(document);
        _updatingContentMode = true;
        try
        {
            AutoContentModeItem.Content =
                $"Авто — {GetContentKindDisplayName(_contentProfile.Kind).ToLowerInvariant()}";
            ContentModeComboBox.SelectedIndex = 0;
        }
        finally
        {
            _updatingContentMode = false;
        }
        _outputDirectory = Path.Combine(
            Path.GetDirectoryName(_sourcePath)!,
            Path.GetFileNameWithoutExtension(_sourcePath) + "_export");
        ModelPathText.Text = _sourcePath;
        OutputPathTextBox.Text = _outputDirectory;
        StatusText.Text = _contentProfile.Kind == SmoExportContentKind.Level
            ? "Определён уровень. Выберите способ сборки сцены."
            : "Определён персонаж. Результат будет сохранён в соседнюю папку экспорта.";
        ResetButton.IsEnabled = true;
        SetSelectedResourceTypes(SmoExportResourceTypes.All);
        ResetAnimationCatalog();
        ApplyContentProfile(resetLevelState: true);
        if (discoverAnimations && GetEffectiveContentKind() == SmoExportContentKind.Character)
            DiscoverAnimations();
        AddLog($"Выбран файл: {_sourcePath}");
        AddLog($"Автоопределение: {_contentProfile.Kind} — {_contentProfile.Reason}. " +
               $"Физических мешей: {_contentProfile.PhysicalMeshCount}; " +
               $"размещений: {_contentProfile.PlacementCount}.");
        if (discoverAnimations && GetEffectiveContentKind() == SmoExportContentKind.Character)
            AddLog($"Найдено соседних SAN: {_animations.Count}.");
        UpdateExportAvailability();
    }

    private void LoadViewerAnimations(string manifestPath)
    {
        try
        {
            string manifest = File.ReadAllText(manifestPath);
            int declaredCount;
            string manifestKind;
            if (manifest.TrimStart().StartsWith('{'))
            {
                SmoAnimationCatalogManifest catalog =
                    JsonSerializer.Deserialize<SmoAnimationCatalogManifest>(
                        manifest,
                        new JsonSerializerOptions
                        {
                            PropertyNameCaseInsensitive = true
                        }) ?? throw new InvalidDataException(
                            "Каталог анимаций Viewer пуст.");
                if (catalog.Version != SmoAnimationCatalogManifest.CurrentVersion)
                {
                    throw new InvalidDataException(
                        $"Версия каталога анимаций Viewer {catalog.Version} не поддерживается.");
                }

                IReadOnlyList<SmoAnimationCatalogEntry> entries =
                    catalog.Animations ?? throw new InvalidDataException(
                        "В каталоге Viewer отсутствует список animations.");
                declaredCount = entries.Count;
                manifestKind = $"JSON-каталог v{catalog.Version}";
                foreach (SmoAnimationCatalogEntry entry in entries)
                {
                    if (string.IsNullOrWhiteSpace(entry.Path) || !File.Exists(entry.Path))
                        continue;
                    string display = string.IsNullOrWhiteSpace(entry.Display)
                        ? Path.GetFileNameWithoutExtension(entry.Path)
                        : entry.Display;
                    string[] groups = (entry.Groups ?? [])
                        .Where(group => !string.IsNullOrWhiteSpace(group))
                        .Distinct(StringComparer.OrdinalIgnoreCase)
                        .ToArray();
                    MergeAnimationCatalogItem(
                        entry.Path,
                        display,
                        groups.Length > 0 ? groups : ["SmoViewer"],
                        selected: false);
                }
                RefreshAnimationList();
            }
            else
            {
                string[] paths = File.ReadAllLines(manifestPath)
                    .Where(path => !string.IsNullOrWhiteSpace(path) && File.Exists(path))
                    .ToArray();
                declaredCount = paths.Length;
                manifestKind = "построчный legacy-список";
                AddAnimationPaths(paths);
                RefreshAnimationList();
            }

            AnimationSourceText.Text = $"Получено из SmoViewer: {_animations.Count} SAN";
            StatusText.Text += $" Viewer передал SAN: {_animations.Count}.";
            AddLog($"Получен {manifestKind} из SmoViewer: " +
                   $"загружено {_animations.Count} из {declaredCount} SAN. Автопоиск отключён.");
        }
        catch (Exception exception)
        {
            StatusText.Text += " Не удалось прочитать список анимаций Viewer.";
            AddLog($"ОШИБКА чтения списка Viewer: {exception.Message}. Автопоиск не выполнялся.");
        }
        finally
        {
            try { File.Delete(manifestPath); }
            catch { }
        }
    }

    private void ApplyContentProfile(bool resetLevelState = false)
    {
        bool isLevel = GetEffectiveContentKind() == SmoExportContentKind.Level;
        ResourceTypesPanel.Visibility = isLevel
            ? Visibility.Collapsed
            : Visibility.Visible;
        AnimationSelectionPanel.Visibility = isLevel
            ? Visibility.Collapsed
            : Visibility.Visible;
        LevelModePanel.Visibility = isLevel
            ? Visibility.Visible
            : Visibility.Collapsed;
        LevelElementSelectionPanel.Visibility = isLevel
            ? Visibility.Visible
            : Visibility.Collapsed;

        if (isLevel && _contentProfile is not null)
        {
            HashSet<int> selectedElements = resetLevelState
                ? []
                : _allLevelElements.Where(item => item.IsSelected)
                    .Select(item => item.MeshObjectIndex).ToHashSet();
            _allLevelElements.Clear();
            _levelElements.Clear();
            foreach (SmoExportElementInfo element in _contentProfile.Elements)
            {
                _allLevelElements.Add(new LevelElementChoice(element)
                {
                    IsSelected = selectedElements.Contains(element.MeshObjectIndex)
                });
            }
            bool automatic = GetSelectedContentMode() == "auto";
            LevelProfileSummaryText.Text =
                (automatic
                    ? $"Определено автоматически: {_contentProfile.Reason}. "
                    : $"Профиль уровня выбран вручную; автоопределение: " +
                      $"{GetContentKindDisplayName(_contentProfile.Kind).ToLowerInvariant()} " +
                      $"({_contentProfile.Reason}). ") +
                $"Мешей {_contentProfile.PhysicalMeshCount}, размещений " +
                $"{_contentProfile.PlacementCount}, ссылочных копий " +
                $"{_contentProfile.SharedInstanceCount}.";
            LevelElementSummaryText.Text =
                $"Доступно уникальных физических мешей: {_allLevelElements.Count}. " +
                "Список используется режимом раздельного экспорта.";
            LevelElementFilterBox.Text = string.Empty;
            RefreshLevelElementList();
            if (resetLevelState)
                LevelEverythingRadio.IsChecked = true;
        }
        else if (resetLevelState)
        {
            _allLevelElements.Clear();
            _levelElements.Clear();
        }
        ApplyFormatCapabilities();
        UpdateLevelModeUi();
    }

    private SmoExportSceneMode GetSelectedLevelMode()
    {
        if (LevelOnlyRadio.IsChecked == true)
            return SmoExportSceneMode.LevelOnly;
        if (LevelBakedRadio.IsChecked == true)
            return SmoExportSceneMode.LevelWithBakedObjects;
        if (LevelInstancesRadio.IsChecked == true)
            return SmoExportSceneMode.LevelWithInstances;
        if (LevelSeparateRadio.IsChecked == true)
            return SmoExportSceneMode.SeparateMeshes;
        return SmoExportSceneMode.All;
    }

    private void LevelMode_Changed(object sender, RoutedEventArgs e)
    {
        if (!_resourceUiReady)
            return;
        UpdateLevelModeUi();
        UpdateExportAvailability();
    }

    private void UpdateLevelModeUi()
    {
        if (LevelElementSelectionPanel is null)
            return;
        bool separate = GetSelectedLevelMode() == SmoExportSceneMode.SeparateMeshes;
        LevelElementSelectionPanel.IsEnabled = !_busy && separate;
        LevelModeHintText.Text = GetSelectedLevelMode() switch
        {
            SmoExportSceneMode.LevelOnly =>
                "Экспортируются физические меши SMO без reference-only размещений.",
            SmoExportSceneMode.LevelWithBakedObjects =>
                "Каждое размещение получает независимую геометрию; файлы будут крупнее.",
            SmoExportSceneMode.LevelWithInstances =>
                "GLB/FBX сохраняют одну геометрию и отдельные узлы-размещения.",
            SmoExportSceneMode.SeparateMeshes =>
                "Для каждого отмеченного меша будет создан самостоятельный файл.",
            _ =>
                "Сохраняются все поддерживаемые ресурсы и полная собранная сцена."
        };
    }

    private void LevelElementFilter_Changed(object sender, TextChangedEventArgs e) =>
        RefreshLevelElementList();

    private void RefreshLevelElementList()
    {
        if (LevelElementFilterBox is null)
            return;
        string filter = LevelElementFilterBox.Text.Trim();
        IEnumerable<LevelElementChoice> visible = _allLevelElements;
        if (filter.Length > 0)
        {
            visible = visible.Where(item =>
                item.Display.Contains(filter, StringComparison.OrdinalIgnoreCase));
        }
        _levelElements.Clear();
        foreach (LevelElementChoice item in visible)
            _levelElements.Add(item);
    }

    private void SelectAllLevelElements_Click(object sender, RoutedEventArgs e)
    {
        foreach (LevelElementChoice item in _levelElements)
            item.IsSelected = true;
        LevelElementList.Items.Refresh();
        UpdateExportAvailability();
    }

    private void ClearLevelElements_Click(object sender, RoutedEventArgs e)
    {
        foreach (LevelElementChoice item in _allLevelElements)
            item.IsSelected = false;
        LevelElementList.Items.Refresh();
        UpdateExportAvailability();
    }

    private void LevelElementCheckBox_Changed(object sender, RoutedEventArgs e)
    {
        if (sender is CheckBox { DataContext: LevelElementChoice item } checkBox)
            item.IsSelected = checkBox.IsChecked == true;
        UpdateExportAvailability();
    }

    private static string? GetOption(string[] arguments, string name)
    {
        int index = Array.FindIndex(arguments,
            argument => argument.Equals(name, StringComparison.OrdinalIgnoreCase));
        return index >= 0 && index + 1 < arguments.Length ? arguments[index + 1] : null;
    }

    private void DiscoverAnimations()
    {
        string? directory = _sourcePath is null ? null : Path.GetDirectoryName(_sourcePath);
        if (directory is null || !Directory.Exists(directory))
        {
            RefreshAnimationList();
            return;
        }

        AddAnimationsFromDirectory(directory);
        if (_animations.Count == 0 && _sourcePath is not null &&
            TryFindDefaultBloomAnimationDirectory([_sourcePath], out string bloomDirectory))
        {
            AddAnimationsFromDirectory(bloomDirectory);
            AnimationSourceText.Text = $"Автоподстановка Bloom: {bloomDirectory}";
            AddLog($"Рядом с моделью нет SAN/ANM; подключена папка Bloom: {bloomDirectory}");
        }
        RefreshAnimationList();
        StatusText.Text += $" Найдено SAN: {_animations.Count}.";
    }

    private void AddAnimationFiles_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Title = "Добавить анимации",
            Filter = "Sparkplug animations (*.san;*.anm)|*.san;*.anm|Все файлы (*.*)|*.*",
            CheckFileExists = true,
            Multiselect = true
        };
        if (dialog.ShowDialog(this) != true)
            return;

        foreach (string file in dialog.FileNames)
        {
            if (Path.GetExtension(file).Equals(".anm", StringComparison.OrdinalIgnoreCase))
                AddAnimationsFromAnm(file, selected: true);
            else
                AddAnimationFile(file, null, "Ручные SAN", selected: true);
        }
        AnimationSourceText.Text = $"Добавлено вручную: {dialog.FileNames.Length}";
        RefreshAnimationList();
        AddLog($"Добавлены SAN/ANM-файлы: {dialog.FileNames.Length}.");
    }

    private void AddAnimationFolder_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFolderDialog { Title = "Папка с SAN/ANM-анимациями" };
        if (dialog.ShowDialog(this) != true)
            return;

        AddAnimationsFromDirectory(dialog.FolderName);
        AnimationSourceText.Text = dialog.FolderName;
        RefreshAnimationList();
        AddLog($"Просканирована папка SAN/ANM: {dialog.FolderName}");
    }

    private void AddAnimationPaths(IEnumerable<string> paths, bool selected = false)
    {
        foreach (string path in paths.OrderBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase))
            AddAnimationFile(path, null, "SmoViewer", selected);
    }

    private void AddAnimationsFromDirectory(string directory, bool selected = false)
    {
        if (!Directory.Exists(directory))
            return;

        foreach (string anm in Directory.EnumerateFiles(
                     directory, "*.anm", SearchOption.AllDirectories))
        {
            AddAnimationsFromAnm(anm, selected);
        }
        foreach (string san in Directory.EnumerateFiles(
                     directory, "*.san", SearchOption.AllDirectories))
        {
            if (_animations.Any(item => item.Path.Equals(
                    Path.GetFullPath(san), StringComparison.OrdinalIgnoreCase)))
            {
                continue;
            }

            AddAnimationFile(
                san,
                null,
                $"{new DirectoryInfo(Path.GetDirectoryName(san)!).Name} · без ANM",
                selected);
        }
        AnimationSourceText.Text = $"Автопоиск: {directory}";
    }

    private static bool TryFindDefaultBloomAnimationDirectory(
        IEnumerable<string> modelPaths,
        out string directory)
    {
        directory = string.Empty;
        IEnumerable<string> starts = modelPaths
            .Select(path => Path.GetDirectoryName(path) ?? string.Empty)
            .Where(path => path.Length > 0)
            .Concat([Environment.CurrentDirectory, AppContext.BaseDirectory]);
        foreach (string start in starts)
        {
            DirectoryInfo? cursor;
            try
            {
                cursor = new DirectoryInfo(start);
            }
            catch
            {
                continue;
            }

            while (cursor is not null)
            {
                string candidate = cursor.Name.Equals("Media", StringComparison.OrdinalIgnoreCase)
                    ? Path.Combine(cursor.FullName, "Characters", "Bloom")
                    : Path.Combine(cursor.FullName, "Media", "Characters", "Bloom");
                if (Directory.Exists(candidate) &&
                    (Directory.EnumerateFiles(candidate, "*.san", SearchOption.AllDirectories).Any() ||
                     Directory.EnumerateFiles(candidate, "*.anm", SearchOption.AllDirectories).Any()))
                {
                    directory = candidate;
                    return true;
                }
                cursor = cursor.Parent;
            }
        }
        return false;
    }

    private void AddAnimationsFromAnm(string anmPath, bool selected = false)
    {
        string directory = Path.GetDirectoryName(anmPath) ?? string.Empty;
        string group = Path.GetFileNameWithoutExtension(anmPath);
        foreach (string line in File.ReadLines(anmPath))
        {
            string clean = line.Trim();
            if (clean.Length == 0 || clean.StartsWith('#') ||
                clean.StartsWith("end", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            string[] fields = clean.TrimEnd(';').Split(',')
                .Select(value => value.Trim()).ToArray();
            if (fields.Length < 8 ||
                !fields[^1].EndsWith(".san", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            string san = Path.Combine(directory, fields[^1]);
            string state = string.Join(" / ", fields.Take(6).Where(value =>
                value.Length > 0 &&
                !value.Equals("none", StringComparison.OrdinalIgnoreCase)));
            AddAnimationFile(
                san,
                $"{group}: {state} [{fields[6]}]",
                group,
                selected);
        }
    }

    private void AddAnimationFile(
        string path,
        string? state,
        string group,
        bool selected = false)
    {
        if (!File.Exists(path))
            return;

        string fullPath = Path.GetFullPath(path);
        AnimationChoice? existing = _animations.FirstOrDefault(item =>
            item.Path.Equals(fullPath, StringComparison.OrdinalIgnoreCase));
        if (existing is not null)
        {
            AddAnimationGroup(fullPath, group);
            existing.IsSelected |= selected;
            if (!string.IsNullOrWhiteSpace(state) &&
                !existing.Display.Contains(state, StringComparison.OrdinalIgnoreCase))
            {
                existing.Display += $" · {state}";
            }
            return;
        }

        string display = Path.GetFileNameWithoutExtension(fullPath);
        if (!string.IsNullOrWhiteSpace(state))
            display += $"  ·  {state}";
        _animations.Add(new AnimationChoice(fullPath, display, selected));
        AddAnimationGroup(fullPath, group);
    }

    private void MergeAnimationCatalogItem(
        string path,
        string display,
        IReadOnlyList<string> groups,
        bool selected = false)
    {
        if (!File.Exists(path))
            return;

        string fullPath = Path.GetFullPath(path);
        AnimationChoice? existing = _animations.FirstOrDefault(item =>
            item.Path.Equals(fullPath, StringComparison.OrdinalIgnoreCase));
        if (existing is null)
        {
            existing = new AnimationChoice(
                fullPath,
                string.IsNullOrWhiteSpace(display)
                    ? Path.GetFileNameWithoutExtension(fullPath)
                    : display,
                selected);
            _animations.Add(existing);
        }
        else
        {
            existing.IsSelected |= selected;
            if (!string.IsNullOrWhiteSpace(display))
                existing.Display = display;
        }

        string[] usableGroups = groups
            .Where(group => !string.IsNullOrWhiteSpace(group))
            .Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        if (usableGroups.Length == 0)
            usableGroups = ["SmoViewer"];
        foreach (string group in usableGroups)
            AddAnimationGroup(fullPath, group);
    }

    private void AddAnimationGroup(string path, string group)
    {
        if (!_animationGroupsByPath.TryGetValue(path, out HashSet<string>? groups))
        {
            groups = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            _animationGroupsByPath.Add(path, groups);
        }
        groups.Add(group);
        _enabledAnimationGroups.Add(group);
    }

    private void ResetAnimationCatalog()
    {
        _animations.Clear();
        _animationGroupsByPath.Clear();
        _enabledAnimationGroups.Clear();
        if (AnimationGroupsPanel is not null)
            AnimationGroupsPanel.Children.Clear();
        if (AnimationList is not null)
            AnimationList.ItemsSource = _animations;
        if (AnimationFilterBox is not null)
            AnimationFilterBox.Clear();
        if (AnimationSourceText is not null)
            AnimationSourceText.Text = "Анимации будут найдены рядом с моделью.";
    }

    private void AnimationFilter_Changed(object sender, TextChangedEventArgs e) =>
        RefreshAnimationList();

    private void RefreshAnimationList()
    {
        if (AnimationList is null || AnimationFilterBox is null)
            return;

        RebuildAnimationGroupControls();
        string filter = AnimationFilterBox.Text.Trim();
        AnimationList.ItemsSource = _animations.Where(item =>
                _animationGroupsByPath.TryGetValue(
                    item.Path, out HashSet<string>? groups) &&
                groups.Any(_enabledAnimationGroups.Contains) &&
                (filter.Length == 0 ||
                 item.Display.Contains(filter, StringComparison.OrdinalIgnoreCase)))
            .OrderBy(item => item.Display, StringComparer.OrdinalIgnoreCase).ToArray();
    }

    private void RebuildAnimationGroupControls()
    {
        if (AnimationGroupsPanel is null)
            return;

        string[] groups = _animationGroupsByPath.Values.SelectMany(value => value)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(value => value, StringComparer.OrdinalIgnoreCase).ToArray();
        CheckBox[] existing = AnimationGroupsPanel.Children.OfType<CheckBox>().ToArray();
        if (existing.Length == groups.Length &&
            existing.Select(checkBox => checkBox.Tag as string)
                .SequenceEqual(groups, StringComparer.OrdinalIgnoreCase) &&
            existing.All(checkBox => checkBox.Tag is string group &&
                checkBox.IsChecked == _enabledAnimationGroups.Contains(group)))
        {
            return;
        }

        _updatingAnimationGroups = true;
        AnimationGroupsPanel.Children.Clear();
        foreach (string group in groups)
        {
            var checkBox = new CheckBox
            {
                Content = group,
                Tag = group,
                IsChecked = _enabledAnimationGroups.Contains(group),
                Margin = new Thickness(0, 2, 0, 2)
            };
            checkBox.Checked += AnimationGroupCheck_Changed;
            checkBox.Unchecked += AnimationGroupCheck_Changed;
            AnimationGroupsPanel.Children.Add(checkBox);
        }
        _updatingAnimationGroups = false;
    }

    private void AnimationGroupCheck_Changed(object sender, RoutedEventArgs e)
    {
        if (_updatingAnimationGroups ||
            sender is not CheckBox { Tag: string group } checkBox)
        {
            return;
        }

        if (checkBox.IsChecked == true)
            _enabledAnimationGroups.Add(group);
        else
            _enabledAnimationGroups.Remove(group);
        RefreshAnimationList();
    }

    private void EnableAllAnimationGroups_Click(object sender, RoutedEventArgs e)
    {
        foreach (string group in _animationGroupsByPath.Values.SelectMany(value => value))
            _enabledAnimationGroups.Add(group);
        SetAnimationGroupChecks(true);
    }

    private void DisableAllAnimationGroups_Click(object sender, RoutedEventArgs e)
    {
        _enabledAnimationGroups.Clear();
        SetAnimationGroupChecks(false);
    }

    private void SetAnimationGroupChecks(bool enabled)
    {
        _updatingAnimationGroups = true;
        foreach (CheckBox checkBox in AnimationGroupsPanel.Children.OfType<CheckBox>())
            checkBox.IsChecked = enabled;
        _updatingAnimationGroups = false;
        RefreshAnimationList();
    }

    private void BrowseOutput_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFolderDialog
        {
            Title = "Выберите папку сохранения",
            Multiselect = false,
            InitialDirectory = _outputDirectory ??
                (_sourcePath is null ? null : Path.GetDirectoryName(_sourcePath))
        };
        if (dialog.ShowDialog(this) != true)
            return;

        _outputDirectory = dialog.FolderName;
        OutputPathTextBox.Text = _outputDirectory;
        ResetButton.IsEnabled = true;
        StatusText.Text = _sourcePath is null
            ? "Папка выбрана. Теперь выберите исходный SMO-файл."
            : "Папка сохранения изменена. Можно экспортировать.";
        AddLog($"Папка экспорта: {_outputDirectory}");
    }

    private async void Export_Click(object sender, RoutedEventArgs e)
    {
        if (_sourcePath is null || _outputDirectory is null)
            return;

        string selectedFormat = GetSelectedFormat();
        if (selectedFormat is "fbx" or "all" &&
            NativeFbxBridge.ResolveExecutable(_blenderPath) is null)
        {
            CheckBlenderAvailability(writeLog: false);
        }
        if (selectedFormat is "fbx" or "all" && _blenderPath is null)
        {
            AddLog("ОШИБКА: FBX недоступен — нативный модуль не найден.");
            UpdateExportAvailability();
            return;
        }

        SmoExportResourceTypes resources = GetResourcesForCurrentProfile();
        string? resourceValidationIssue = GetExportValidationIssue(selectedFormat);
        if (resourceValidationIssue is not null)
        {
            StatusText.Text = resourceValidationIssue;
            UpdateExportAvailability();
            return;
        }
        SmoExportResourceTypes sceneResources = GetSceneResourcesForFormat(
            selectedFormat,
            resources);

        SetBusy(true);
        try
        {
            string sourcePath = _sourcePath;
            string outputDirectory = _outputDirectory;
            string format = selectedFormat;
            SmoExportSceneMode sceneMode =
                GetEffectiveContentKind() == SmoExportContentKind.Level
                    ? GetSelectedLevelMode()
                    : SmoExportSceneMode.All;
            int[] selectedMeshObjectIndices = sceneMode == SmoExportSceneMode.SeparateMeshes
                ? _allLevelElements.Where(item => item.IsSelected)
                    .Select(item => item.MeshObjectIndex).ToArray()
                : [];
            string[] animations = (sceneResources & SmoExportResourceTypes.Animations) != 0
                ? _animations.Where(item => item.IsSelected)
                    .Select(item => item.Path).ToArray()
                : [];
            StatusText.Text = "Чтение SMO и подготовка выбранных ресурсов…";
            AddLog($"Начат экспорт {Path.GetFileName(sourcePath)}; " +
                   $"формат: {format.ToUpperInvariant()}; ресурсы: {sceneResources}; " +
                   $"режим: {sceneMode}; выбрано SAN: {animations.Length}; " +
                   $"отдельных мешей: {selectedMeshObjectIndices.Length}.");
            IProgress<string> progress = new Progress<string>(message =>
            {
                StatusText.Text = message;
                AddLog(message);
            });

            ExportResult result = await Task.Run(() =>
            {
                progress.Report("Чтение структуры SMO…");
                SmoDocument document = SmoDocument.Load(sourcePath);
                progress.Report("Декодирование выбранных типов ресурсов…");
                SmoExportScene scene = SmoSceneBuilder.Build(
                    document, new SmoExportOptions(
                        AnimationPaths: animations,
                        Resources: sceneResources,
                        SceneMode: sceneMode,
                        SelectedMeshObjectIndices:
                            selectedMeshObjectIndices.ToHashSet()));
                progress.Report($"Сцена подготовлена: уникальных meshes " +
                    $"{scene.Meshes.Count}, размещений {scene.MeshPlacements.Count}, " +
                    $"nodes {scene.Nodes.Count}, skins {scene.Skins.Count}, " +
                    $"animations {scene.Animations.Count}.");
                Directory.CreateDirectory(outputDirectory);
                string stem = Path.GetFileNameWithoutExtension(sourcePath);
                var files = new List<string>();
                var warnings = scene.Warnings.ToList();

                void WriteFormats(SmoExportScene exportScene, string fileStem)
                {
                    if (format is "glb" or "all")
                    {
                        progress.Report($"Запись {fileStem}.glb…");
                        string glb = Path.Combine(outputDirectory, fileStem + ".glb");
                        GlbExporter.Export(exportScene, glb);
                        files.Add(glb);
                    }
                    if (format is "fbx" or "all")
                    {
                        progress.Report($"Прямая запись {fileStem}.fbx нативным модулем…");
                        string fbx = Path.Combine(outputDirectory, fileStem + ".fbx");
                        FbxExporter.Export(exportScene, fbx, _blenderPath);
                        files.Add(fbx);
                        warnings.AddRange(FbxExporter.GetConversionNotes(exportScene));
                    }
                    if (format is "obj" or "all")
                    {
                        progress.Report($"Запись {fileStem}.obj…");
                        string obj = Path.Combine(outputDirectory, fileStem + ".obj");
                        ObjExporter.Export(exportScene, obj);
                        files.Add(obj);
                    }
                }

                if (sceneMode == SmoExportSceneMode.SeparateMeshes)
                {
                    foreach (var variant in scene.Meshes.Where(mesh => selectedMeshObjectIndices.Contains(mesh.ObjectIndex)))
                    {
                        SmoExportScene single =
                            SmoExportSceneSplitter.CreateSingleMeshScene(
                                scene, variant.VariantKey);
                        SmoExportMesh mesh = single.Meshes[0];
                        string fileStem = $"{stem}_{SafeFileName(mesh.Name)}_{mesh.ObjectIndex}_model_{mesh.RenderableObjectIndex}";
                        WriteFormats(single, fileStem);
                    }
                }
                else
                {
                    WriteFormats(scene, stem);
                }

                int sharedInstances = scene.MeshPlacements.Count(placement =>
                    placement.IsSharedInstance);
                if ((format is "obj" or "all") && sharedInstances > 0)
                {
                    warnings.Add(
                        $"OBJ не поддерживает mesh-инстансы: {sharedInstances} " +
                        "ссылочных размещений запечены в независимые вершины. " +
                        "GLB/FBX в том же экспорте сохраняют ссылки, если выбран соответствующий режим.");
                }
                return new ExportResult(
                    scene.Meshes.Count,
                    scene.MeshPlacements.Count,
                    scene.Nodes.Count,
                    scene.Skins.Count,
                    scene.Animations.Count,
                    warnings,
                    files);
            });

            StatusText.Text =
                $"Готово: уникальных мешей {result.MeshCount}, размещений " +
                $"{result.PlacementCount}, узлов {result.NodeCount}, " +
                $"скинов {result.SkinCount}, анимаций {result.AnimationCount}; " +
                $"предупреждений: {result.Warnings.Count}. " +
                $"Папка: {outputDirectory}";
            foreach (string warning in result.Warnings)
                AddLog("ПРЕДУПРЕЖДЕНИЕ: " + warning);
            foreach (string file in result.Files)
                AddLog($"Создан {Path.GetFileName(file)} ({new FileInfo(file).Length:N0} байт).");
            AddLog("Экспорт успешно завершён.");
        }
        catch (Exception exception)
        {
            StatusText.Text = "Ошибка экспорта: " + exception.Message;
            AddLog($"ОШИБКА {exception.GetType().Name}: {exception.Message}");
            MessageBox.Show(
                this,
                exception.Message,
                "Ошибка экспорта",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
        finally
        {
            SetBusy(false);
        }
    }

    private void Reset_Click(object sender, RoutedEventArgs e)
    {
        _sourcePath = null;
        _outputDirectory = null;
        _contentProfile = null;
        ModelPathText.Text = "Файл не выбран";
        OutputPathTextBox.Text = "Не выбрана";
        FormatComboBox.SelectedIndex = 0;
        _updatingContentMode = true;
        try
        {
            AutoContentModeItem.Content = "Авто";
            ContentModeComboBox.SelectedIndex = 0;
        }
        finally
        {
            _updatingContentMode = false;
        }
        ResetAnimationCatalog();
        _allLevelElements.Clear();
        _levelElements.Clear();
        ApplyContentProfile();
        SetSelectedResourceTypes(SmoExportResourceTypes.All);
        ExportLog.Items.Clear();
        AddLog("Форма очищена.");
        StatusText.Text = "Выберите исходный SMO-файл.";
        ResetButton.IsEnabled = false;
        UpdateExportAvailability();
    }

    private void SetBusy(bool busy)
    {
        _busy = busy;
        SelectButton.IsEnabled = !busy;
        ResetButton.IsEnabled = !busy && _sourcePath is not null;
        FormatComboBox.IsEnabled = !busy;
        ContentModeComboBox.IsEnabled = !busy;
        BrowseOutputButton.IsEnabled = !busy;
        BlenderPathTextBox.IsEnabled = !busy;
        BrowseBlenderButton.IsEnabled = !busy;
        ApplyBlenderPathButton.IsEnabled = !busy;
        AutoFindBlenderButton.IsEnabled = !busy;
        BlenderSettingsButton.IsEnabled = !busy;
        if (busy)
            BlenderSettingsPopup.IsOpen = false;
        ResourceTypesPanel.IsEnabled = !busy;
        LevelModePanel.IsEnabled = !busy;
        UpdateAnimationControlsState();
        ApplyFormatCapabilities();
        UpdateLevelModeUi();
        UpdateExportAvailability();
    }

    private void FormatComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        ApplyFormatCapabilities();
        UpdateExportAvailability();
    }

    private void ContentModeComboBox_SelectionChanged(
        object sender,
        SelectionChangedEventArgs e)
    {
        if (!_resourceUiReady || _updatingContentMode || _contentProfile is null)
            return;

        SmoExportContentKind effectiveKind = GetEffectiveContentKind()!.Value;
        ApplyContentProfile();
        if (effectiveKind == SmoExportContentKind.Character && _animations.Count == 0)
            DiscoverAnimations();

        string selectedMode = GetSelectedContentMode();
        StatusText.Text = selectedMode == "auto"
            ? $"Автоматически определён {GetContentKindDisplayName(effectiveKind).ToLowerInvariant()}."
            : $"Ручной профиль: {GetContentKindDisplayName(effectiveKind).ToLowerInvariant()}. " +
              $"Автоопределение: {GetContentKindDisplayName(_contentProfile.Kind).ToLowerInvariant()}.";
        AddLog(selectedMode == "auto"
            ? $"Включено автоопределение профиля: {effectiveKind}."
            : $"Профиль вручную изменён на {effectiveKind}; " +
              $"автоопределение сохранено: {_contentProfile.Kind} — {_contentProfile.Reason}.");
        UpdateExportAvailability();
    }

    private void ApplyFormatCapabilities()
    {
        if (!_resourceUiReady || FormatComboBox is null)
            return;
        string format = GetSelectedFormat();
        bool objOnly = format == "obj";
        SkeletonResourceCheckBox.IsEnabled = !objOnly && !_busy;
        ServiceNodesResourceCheckBox.IsEnabled = !objOnly && !_busy;
        AnimationsResourceCheckBox.IsEnabled = !objOnly && !_busy;
        MeshesResourceCheckBox.IsEnabled = !_busy;
        MaterialsResourceCheckBox.IsEnabled = !_busy;
        TexturesResourceCheckBox.IsEnabled = !_busy;
        if (objOnly && GetEffectiveContentKind() != SmoExportContentKind.Level)
        {
            SmoExportResourceTypes compatible = GetSelectedResourceTypes() &
                (SmoExportResourceTypes.Meshes |
                 SmoExportResourceTypes.Materials |
                 SmoExportResourceTypes.Textures);
            SetSelectedResourceTypes(compatible);
        }

        bool supportsInstances = format is "glb" or "fbx";
        LevelInstancesRadio.IsEnabled = supportsInstances && !_busy;
        if (!supportsInstances && LevelInstancesRadio.IsChecked == true)
            LevelBakedRadio.IsChecked = true;
        UpdateLevelModeUi();
    }

    private void CheckBlender_Click(object sender, RoutedEventArgs e)
    {
        _configuredBlenderPath = null;
        SaveConfiguredBlenderPath(null);
        BlenderPathTextBox.Text = string.Empty;
        CheckBlenderAvailability(writeLog: true);
    }

    private void BlenderSettings_Click(object sender, RoutedEventArgs e) =>
        BlenderSettingsPopup.IsOpen = true;

    private void BrowseBlender_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Title = "Укажите исполняемый файл Blender",
            Filter = "Blender (blender.exe)|blender.exe|Исполняемые файлы (*.exe)|*.exe",
            CheckFileExists = true,
            Multiselect = false,
            FileName = "blender.exe",
            InitialDirectory = _blenderPath is null ? null : Path.GetDirectoryName(_blenderPath)
        };
        if (dialog.ShowDialog(this) != true)
            return;
        BlenderPathTextBox.Text = dialog.FileName;
        ApplyBlenderPath();
    }

    private void ApplyBlenderPath_Click(object sender, RoutedEventArgs e) => ApplyBlenderPath();

    private void BlenderPathTextBox_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key != Key.Enter)
            return;
        ApplyBlenderPath();
        e.Handled = true;
    }

    private void ApplyBlenderPath()
    {
        string enteredPath = BlenderPathTextBox.Text.Trim();
        if (string.IsNullOrWhiteSpace(enteredPath))
        {
            _configuredBlenderPath = null;
            SaveConfiguredBlenderPath(null);
            CheckBlenderAvailability(writeLog: true);
            return;
        }

        string? resolved = NativeFbxBridge.ResolveExecutable(enteredPath);
        if (resolved is null)
        {
            MessageBox.Show(this,
                "В указанном месте не найден blender.exe. Укажите папку установки Blender или сам файл blender.exe.",
                "Blender не найден", MessageBoxButton.OK, MessageBoxImage.Warning);
            AddLog($"Указанный путь Blender недействителен: {enteredPath}");
            return;
        }

        _configuredBlenderPath = resolved;
        BlenderPathTextBox.Text = resolved;
        SaveConfiguredBlenderPath(resolved);
        CheckBlenderAvailability(writeLog: true);
        BlenderSettingsPopup.IsOpen = false;
    }

    private void CheckBlenderAvailability(bool writeLog)
    {
        _blenderPath = FbxExporter.FindNativeBridgeExecutable(_configuredBlenderPath);
        bool found = _blenderPath is not null;
        bool manuallyConfigured = found &&
            NativeFbxBridge.ResolveExecutable(_configuredBlenderPath)?.Equals(
                _blenderPath, StringComparison.OrdinalIgnoreCase) == true;
        if (found)
            BlenderPathTextBox.Text = _blenderPath!;
        BlenderStatusDot.Fill = found ? Brushes.SeaGreen : Brushes.Firebrick;
        BlenderSummaryText.Foreground = found ? Brushes.SeaGreen : Brushes.Firebrick;
        BlenderSummaryText.Text = found ? "Найден" : "Не найден";
        BlenderSummaryText.ToolTip = found
            ? $"Blender найден: {_blenderPath}"
            : "Нативный модуль FBX не найден рядом с программой.";
        BlenderStatusText.Foreground = found ? Brushes.SeaGreen : Brushes.Firebrick;
        BlenderStatusText.Text = found
            ? $"Blender {(manuallyConfigured ? "указан вручную" : "найден автоматически")}: {_blenderPath}. FBX доступен."
            : "Нативный модуль FBX не найден; проверьте комплект поставки программы.";
        FbxFormatItem.IsEnabled = found;
        AllFormatsItem.IsEnabled = found;
        if (!found && GetSelectedFormat() is "fbx" or "all")
            FormatComboBox.SelectedIndex = 0;
        if (writeLog)
            AddLog(found ? $"Нативный модуль FBX найден: {_blenderPath}" : "Нативный модуль FBX не найден; FBX отключён.");
        UpdateExportAvailability();
    }

    private static string? LoadConfiguredBlenderPath()
    {
        try
        {
            if (!File.Exists(SettingsPath))
                return null;
            ExporterSettings? settings = JsonSerializer.Deserialize<ExporterSettings>(
                File.ReadAllText(SettingsPath));
            return settings?.BlenderPath;
        }
        catch
        {
            return null;
        }
    }

    private void SaveConfiguredBlenderPath(string? path)
    {
        try
        {
            string directory = Path.GetDirectoryName(SettingsPath)!;
            Directory.CreateDirectory(directory);
            string temporaryPath = SettingsPath + ".tmp";
            File.WriteAllText(temporaryPath,
                JsonSerializer.Serialize(new ExporterSettings(path), new JsonSerializerOptions { WriteIndented = true }));
            File.Move(temporaryPath, SettingsPath, overwrite: true);
        }
        catch (Exception exception)
        {
            AddLog($"Не удалось сохранить путь Blender: {exception.Message}");
        }
    }

    private string GetSelectedFormat() =>
        (FormatComboBox.SelectedItem as ComboBoxItem)?.Tag as string ?? "glb";

    private string GetSelectedContentMode() =>
        (ContentModeComboBox.SelectedItem as ComboBoxItem)?.Tag as string ?? "auto";

    private SmoExportContentKind? GetEffectiveContentKind()
    {
        if (_contentProfile is null)
            return null;
        return GetSelectedContentMode() switch
        {
            "character" => SmoExportContentKind.Character,
            "level" => SmoExportContentKind.Level,
            _ => _contentProfile.Kind
        };
    }

    private static string GetContentKindDisplayName(SmoExportContentKind kind) =>
        kind == SmoExportContentKind.Level ? "Уровень" : "Персонаж";

    private void UpdateExportAvailability()
    {
        if (ExportButton is null) return;
        string selectedFormat = GetSelectedFormat();
        bool needsBlender = selectedFormat is "fbx" or "all";
        string? resourceValidationIssue = GetExportValidationIssue(selectedFormat);
        ExportButton.IsEnabled = !_busy && _sourcePath is not null &&
            _outputDirectory is not null && resourceValidationIssue is null &&
            (!needsBlender || _blenderPath is not null);
        ExportButton.ToolTip = resourceValidationIssue ??
            (needsBlender && _blenderPath is null
                ? "В комплекте программы отсутствует нативный модуль FBX."
                : null);
    }

    private void ResourceTypeCheckBox_Changed(
        object sender,
        RoutedEventArgs e)
    {
        if (!_resourceUiReady || _updatingResourceTypes)
            return;

        _updatingResourceTypes = true;
        try
        {
            if (ReferenceEquals(sender, MeshesResourceCheckBox) &&
                MeshesResourceCheckBox.IsChecked != true)
            {
                MaterialsResourceCheckBox.IsChecked = false;
                TexturesResourceCheckBox.IsChecked = false;
            }
            else if (ReferenceEquals(sender, MaterialsResourceCheckBox))
            {
                if (MaterialsResourceCheckBox.IsChecked == true)
                    MeshesResourceCheckBox.IsChecked = true;
                else
                    TexturesResourceCheckBox.IsChecked = false;
            }
            else if (ReferenceEquals(sender, TexturesResourceCheckBox) &&
                     TexturesResourceCheckBox.IsChecked == true)
            {
                MaterialsResourceCheckBox.IsChecked = true;
                MeshesResourceCheckBox.IsChecked = true;
            }
            else if (ReferenceEquals(sender, SkeletonResourceCheckBox) &&
                     SkeletonResourceCheckBox.IsChecked != true)
            {
                ServiceNodesResourceCheckBox.IsChecked = false;
                AnimationsResourceCheckBox.IsChecked = false;
            }
            else if (ReferenceEquals(sender, ServiceNodesResourceCheckBox) &&
                     ServiceNodesResourceCheckBox.IsChecked == true)
            {
                SkeletonResourceCheckBox.IsChecked = true;
            }
            else if (ReferenceEquals(sender, AnimationsResourceCheckBox) &&
                     AnimationsResourceCheckBox.IsChecked == true)
            {
                SkeletonResourceCheckBox.IsChecked = true;
            }
        }
        finally
        {
            _updatingResourceTypes = false;
        }

        UpdateAnimationControlsState();
        UpdateExportAvailability();
    }

    private void SelectAllResourceTypes_Click(object sender, RoutedEventArgs e) =>
        SetSelectedResourceTypes(SmoExportResourceTypes.All);

    private void ClearResourceTypes_Click(object sender, RoutedEventArgs e) =>
        SetSelectedResourceTypes(SmoExportResourceTypes.None);

    private void SetSelectedResourceTypes(SmoExportResourceTypes resources)
    {
        if (!_resourceUiReady)
            return;

        _updatingResourceTypes = true;
        try
        {
            MeshesResourceCheckBox.IsChecked =
                (resources & SmoExportResourceTypes.Meshes) != 0;
            SkeletonResourceCheckBox.IsChecked =
                (resources & SmoExportResourceTypes.Skeleton) != 0;
            ServiceNodesResourceCheckBox.IsChecked =
                (resources & SmoExportResourceTypes.ServiceNodes) != 0;
            MaterialsResourceCheckBox.IsChecked =
                (resources & SmoExportResourceTypes.Materials) != 0;
            TexturesResourceCheckBox.IsChecked =
                (resources & SmoExportResourceTypes.Textures) != 0;
            AnimationsResourceCheckBox.IsChecked =
                (resources & SmoExportResourceTypes.Animations) != 0;
        }
        finally
        {
            _updatingResourceTypes = false;
        }

        UpdateAnimationControlsState();
        UpdateExportAvailability();
    }

    private SmoExportResourceTypes GetSelectedResourceTypes()
    {
        SmoExportResourceTypes resources = SmoExportResourceTypes.None;
        if (MeshesResourceCheckBox.IsChecked == true)
            resources |= SmoExportResourceTypes.Meshes;
        if (SkeletonResourceCheckBox.IsChecked == true)
            resources |= SmoExportResourceTypes.Skeleton;
        if (ServiceNodesResourceCheckBox.IsChecked == true)
            resources |= SmoExportResourceTypes.ServiceNodes;
        if (MaterialsResourceCheckBox.IsChecked == true)
            resources |= SmoExportResourceTypes.Materials;
        if (TexturesResourceCheckBox.IsChecked == true)
            resources |= SmoExportResourceTypes.Textures;
        if (AnimationsResourceCheckBox.IsChecked == true)
            resources |= SmoExportResourceTypes.Animations;
        return resources;
    }

    private static string? GetResourceValidationIssue(
        string selectedFormat,
        SmoExportResourceTypes resources)
    {
        bool hasMeshes = (resources & SmoExportResourceTypes.Meshes) != 0;
        bool hasSkeleton = (resources & SmoExportResourceTypes.Skeleton) != 0;
        if ((selectedFormat is "obj" or "all") && !hasMeshes)
        {
            return selectedFormat == "all"
                ? "Режим «Все форматы» включает OBJ. Включите ресурс «Меши»."
                : "Для экспорта OBJ включите ресурс «Меши».";
        }

        return hasMeshes || hasSkeleton
            ? null
            : "Выберите хотя бы «Меши» или «Деформирующие кости».";
    }

    private string? GetExportValidationIssue(string selectedFormat)
    {
        if (GetEffectiveContentKind() != SmoExportContentKind.Level)
        {
            return GetResourceValidationIssue(
                selectedFormat, GetSelectedResourceTypes());
        }

        SmoExportSceneMode mode = GetSelectedLevelMode();
        if (mode == SmoExportSceneMode.LevelWithInstances &&
            selectedFormat is not ("glb" or "fbx"))
        {
            return "Mesh-инстансы поддерживаются только GLB и FBX; " +
                   "для OBJ выберите режим без ссылок.";
        }
        if (mode == SmoExportSceneMode.SeparateMeshes &&
            !_allLevelElements.Any(item => item.IsSelected))
        {
            return "Отметьте хотя бы один меш для раздельного экспорта.";
        }
        return null;
    }

    private SmoExportResourceTypes GetResourcesForCurrentProfile()
    {
        if (GetEffectiveContentKind() != SmoExportContentKind.Level)
            return GetSelectedResourceTypes();
        return GetSelectedLevelMode() == SmoExportSceneMode.All
            ? SmoExportResourceTypes.All
            : SmoExportResourceTypes.Meshes |
              SmoExportResourceTypes.Materials |
              SmoExportResourceTypes.Textures;
    }

    private static SmoExportResourceTypes GetSceneResourcesForFormat(
        string selectedFormat,
        SmoExportResourceTypes resources) =>
        selectedFormat == "obj"
            ? resources & (SmoExportResourceTypes.Meshes |
                           SmoExportResourceTypes.Materials |
                           SmoExportResourceTypes.Textures)
            : resources;

    private void UpdateAnimationControlsState()
    {
        if (!_resourceUiReady)
            return;

        AnimationSelectionPanel.IsEnabled =
            !_busy && AnimationsResourceCheckBox.IsChecked == true;
    }

    private void AddLog(string message)
    {
        if (ExportLog is null) return;
        ExportLog.Items.Add($"[{DateTime.Now:HH:mm:ss}] {message}");
        if (ExportLog.Items.Count > 1000)
            ExportLog.Items.RemoveAt(0);
        ExportLog.ScrollIntoView(ExportLog.Items[^1]);
    }

    private void SelectAllAnimations_Click(object sender, RoutedEventArgs e)
    {
        foreach (AnimationChoice animation in _animations) animation.IsSelected = true;
        AnimationList.Items.Refresh();
    }

    private void ClearAnimations_Click(object sender, RoutedEventArgs e)
    {
        foreach (AnimationChoice animation in _animations) animation.IsSelected = false;
        AnimationList.Items.Refresh();
    }

    private static string SafeFileName(string value)
    {
        HashSet<char> invalid = Path.GetInvalidFileNameChars().ToHashSet();
        string result = new(value.Select(character =>
            invalid.Contains(character) || char.IsControl(character)
                ? '_'
                : character).ToArray());
        result = result.Trim().TrimEnd('.');
        return string.IsNullOrWhiteSpace(result) ? "mesh" : result;
    }

    private sealed record ExportResult(
        int MeshCount,
        int PlacementCount,
        int NodeCount,
        int SkinCount,
        int AnimationCount,
        IReadOnlyList<string> Warnings,
        IReadOnlyList<string> Files);

    private sealed record ExporterSettings(string? BlenderPath);

    private sealed class AnimationChoice(string path, string display, bool selected)
    {
        public string Path { get; } = path;
        public string Display { get; set; } = display;
        public bool IsSelected { get; set; } = selected;
    }

    private sealed class LevelElementChoice(SmoExportElementInfo element)
    {
        public int MeshObjectIndex { get; } = element.MeshObjectIndex;
        public string Display { get; } = element.Display;
        public bool IsSelected { get; set; }
    }
}
