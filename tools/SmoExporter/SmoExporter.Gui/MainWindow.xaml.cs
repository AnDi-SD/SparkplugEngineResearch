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
    private readonly ObservableCollection<AnimationChoice> _animations = [];
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
        AnimationList.ItemsSource = _animations;
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
                LoadViewerAnimations(viewerAnimationList);
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
        _outputDirectory = Path.Combine(
            Path.GetDirectoryName(_sourcePath)!,
            Path.GetFileNameWithoutExtension(_sourcePath) + "_export");
        ModelPathText.Text = _sourcePath;
        OutputPathTextBox.Text = _outputDirectory;
        StatusText.Text = "Модель выбрана. Результат будет сохранён в соседнюю папку экспорта.";
        ResetButton.IsEnabled = true;
        _animations.Clear();
        if (discoverAnimations)
            DiscoverAnimations();
        AddLog($"Выбрана модель: {_sourcePath}");
        if (discoverAnimations)
            AddLog($"Найдено соседних SAN: {_animations.Count}.");
        UpdateExportAvailability();
    }

    private void LoadViewerAnimations(string manifestPath)
    {
        try
        {
            string[] paths = File.ReadAllLines(manifestPath)
                .Where(path => !string.IsNullOrWhiteSpace(path) && File.Exists(path)).ToArray();
            AddAnimationPaths(paths);
            StatusText.Text += $" Viewer передал SAN: {_animations.Count}.";
            AddLog($"Получен список анимаций из SmoViewer: {_animations.Count} SAN. Автопоиск отключён.");
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

    private static string? GetOption(string[] arguments, string name)
    {
        int index = Array.FindIndex(arguments,
            argument => argument.Equals(name, StringComparison.OrdinalIgnoreCase));
        return index >= 0 && index + 1 < arguments.Length ? arguments[index + 1] : null;
    }

    private void DiscoverAnimations()
    {
        _animations.Clear();
        string? directory = _sourcePath is null ? null : Path.GetDirectoryName(_sourcePath);
        if (directory is null || !Directory.Exists(directory)) return;
        AddAnimationPaths(Directory.EnumerateFiles(directory, "*.san", SearchOption.AllDirectories));
        StatusText.Text += $" Найдено SAN: {_animations.Count}.";
    }

    private void AddAnimationFiles_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Title = "Добавить анимации SAN",
            Filter = "Sparkplug animations (*.san)|*.san|Все файлы (*.*)|*.*",
            CheckFileExists = true,
            Multiselect = true
        };
        if (dialog.ShowDialog(this) == true)
        {
            AddAnimationPaths(dialog.FileNames, selected: true);
            AddLog($"Добавлены SAN-файлы: {dialog.FileNames.Length}.");
        }
    }

    private void AddAnimationFolder_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFolderDialog { Title = "Папка с анимациями SAN" };
        if (dialog.ShowDialog(this) == true)
        {
            AddAnimationPaths(Directory.EnumerateFiles(
                dialog.FolderName, "*.san", SearchOption.AllDirectories));
            AddLog($"Просканирована папка SAN: {dialog.FolderName}");
        }
    }

    private void AddAnimationPaths(IEnumerable<string> paths, bool selected = false)
    {
        HashSet<string> known = _animations.Select(item => item.Path)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        foreach (string path in paths.OrderBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase))
        {
            string fullPath = Path.GetFullPath(path);
            if (known.Add(fullPath))
                _animations.Add(new AnimationChoice(fullPath,
                    Path.GetFileNameWithoutExtension(fullPath), selected));
        }
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
            FbxExporter.ResolveBlenderExecutable(_blenderPath) is null)
        {
            CheckBlenderAvailability(writeLog: false);
        }
        if (selectedFormat is "fbx" or "all" && _blenderPath is null)
        {
            AddLog("ОШИБКА: FBX недоступен — Blender не найден.");
            UpdateExportAvailability();
            return;
        }

        SetBusy(true);
        try
        {
            string sourcePath = _sourcePath;
            string outputDirectory = _outputDirectory;
            string format = selectedFormat;
            string[] animations = _animations.Where(item => item.IsSelected)
                .Select(item => item.Path).ToArray();
            StatusText.Text = "Чтение SMO и подготовка геометрии…";
            AddLog($"Начат экспорт {Path.GetFileName(sourcePath)}; формат: {format.ToUpperInvariant()}; выбрано SAN: {animations.Length}.");
            IProgress<string> progress = new Progress<string>(message =>
            {
                StatusText.Text = message;
                AddLog(message);
            });

            ExportResult result = await Task.Run(() =>
            {
                progress.Report("Чтение структуры SMO…");
                SmoDocument document = SmoDocument.Load(sourcePath);
                progress.Report("Декодирование meshes, skeleton, palettes и анимаций…");
                SmoExportScene scene = SmoSceneBuilder.Build(
                    document, new SmoExportOptions(AnimationPaths: animations));
                progress.Report($"Сцена подготовлена: meshes {scene.Meshes.Count}, nodes {scene.Nodes.Count}, skins {scene.Skins.Count}, animations {scene.Animations.Count}.");
                Directory.CreateDirectory(outputDirectory);
                string stem = Path.GetFileNameWithoutExtension(sourcePath);
                var files = new List<string>();
                if (format is "glb" or "all")
                {
                    progress.Report("Запись GLB…");
                    string glb = Path.Combine(outputDirectory, stem + ".glb");
                    GlbExporter.Export(scene, glb);
                    files.Add(glb);
                }
                if (format is "fbx" or "all")
                {
                    progress.Report("Подготовка FBX и запуск Blender. Большая модель может обрабатываться несколько минут…");
                    string fbx = Path.Combine(outputDirectory, stem + ".fbx");
                    FbxExporter.Export(scene, fbx, _blenderPath);
                    files.Add(fbx);
                }
                if (format is "obj" or "all")
                {
                    progress.Report("Запись OBJ, MTL и текстур…");
                    string obj = Path.Combine(outputDirectory, stem + ".obj");
                    ObjExporter.Export(scene, obj);
                    files.Add(obj);
                }
                return new ExportResult(scene.Meshes.Count, scene.Warnings.ToArray(), files);
            });

            StatusText.Text =
                $"Готово: {result.MeshCount} mesh, предупреждений: {result.Warnings.Count}. " +
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
        ModelPathText.Text = "Файл не выбран";
        OutputPathTextBox.Text = "Не выбрана";
        FormatComboBox.SelectedIndex = 0;
        _animations.Clear();
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
        BrowseOutputButton.IsEnabled = !busy;
        BlenderPathTextBox.IsEnabled = !busy;
        BrowseBlenderButton.IsEnabled = !busy;
        ApplyBlenderPathButton.IsEnabled = !busy;
        AutoFindBlenderButton.IsEnabled = !busy;
        AnimationList.IsEnabled = !busy;
        UpdateExportAvailability();
    }

    private void FormatComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e) =>
        UpdateExportAvailability();

    private void CheckBlender_Click(object sender, RoutedEventArgs e)
    {
        _configuredBlenderPath = null;
        SaveConfiguredBlenderPath(null);
        BlenderPathTextBox.Text = string.Empty;
        CheckBlenderAvailability(writeLog: true);
    }

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

        string? resolved = FbxExporter.ResolveBlenderExecutable(enteredPath);
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
    }

    private void CheckBlenderAvailability(bool writeLog)
    {
        _blenderPath = FbxExporter.FindBlenderExecutable(_configuredBlenderPath);
        bool found = _blenderPath is not null;
        bool manuallyConfigured = found &&
            FbxExporter.ResolveBlenderExecutable(_configuredBlenderPath)?.Equals(
                _blenderPath, StringComparison.OrdinalIgnoreCase) == true;
        if (found)
            BlenderPathTextBox.Text = _blenderPath!;
        BlenderStatusDot.Fill = found ? Brushes.SeaGreen : Brushes.Firebrick;
        BlenderStatusText.Foreground = found ? Brushes.SeaGreen : Brushes.Firebrick;
        BlenderStatusText.Text = found
            ? $"Blender {(manuallyConfigured ? "указан вручную" : "найден автоматически")}: {_blenderPath}. FBX доступен."
            : "Blender не найден. Введите папку установки или путь к blender.exe; GLB и OBJ доступны без Blender.";
        FbxFormatItem.IsEnabled = found;
        AllFormatsItem.IsEnabled = found;
        if (!found && GetSelectedFormat() is "fbx" or "all")
            FormatComboBox.SelectedIndex = 0;
        if (writeLog)
            AddLog(found ? $"Blender найден: {_blenderPath}" : "Blender не найден; FBX отключён.");
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

    private void UpdateExportAvailability()
    {
        if (ExportButton is null) return;
        bool needsBlender = GetSelectedFormat() is "fbx" or "all";
        ExportButton.IsEnabled = !_busy && _sourcePath is not null &&
            _outputDirectory is not null && (!needsBlender || _blenderPath is not null);
        ExportButton.ToolTip = needsBlender && _blenderPath is null
            ? "Для выбранного формата требуется Blender."
            : null;
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

    private sealed record ExportResult(
        int MeshCount,
        IReadOnlyList<string> Warnings,
        IReadOnlyList<string> Files);

    private sealed record ExporterSettings(string? BlenderPath);

    private sealed class AnimationChoice(string path, string display, bool selected)
    {
        public string Path { get; } = path;
        public string Display { get; } = display;
        public bool IsSelected { get; set; } = selected;
    }
}
