using System.IO;
using System.Windows;
using System.Windows.Controls;
using Microsoft.Win32;
using SmoExporter.Core;
using SmoViewer.Core;

namespace SmoExporter.Gui;

public partial class MainWindow : Window
{
    private string? _sourcePath;
    private string? _outputDirectory;

    public MainWindow()
    {
        InitializeComponent();
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

        _sourcePath = dialog.FileName;
        _outputDirectory = Path.Combine(
            Path.GetDirectoryName(_sourcePath)!,
            Path.GetFileNameWithoutExtension(_sourcePath) + "_export");
        ModelPathText.Text = _sourcePath;
        OutputPathTextBox.Text = _outputDirectory;
        StatusText.Text = "Модель выбрана. Результат будет сохранён в соседнюю папку экспорта.";
        ExportButton.IsEnabled = true;
        ResetButton.IsEnabled = true;
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
    }

    private async void Export_Click(object sender, RoutedEventArgs e)
    {
        if (_sourcePath is null || _outputDirectory is null)
            return;

        SetBusy(true);
        try
        {
            string sourcePath = _sourcePath;
            string outputDirectory = _outputDirectory;
            string format = (FormatComboBox.SelectedItem as ComboBoxItem)?.Tag as string ?? "glb";
            StatusText.Text = "Чтение SMO и подготовка геометрии…";

            ExportResult result = await Task.Run(() =>
            {
                SmoExportScene scene = SmoSceneBuilder.Build(SmoDocument.Load(sourcePath));
                Directory.CreateDirectory(outputDirectory);
                string stem = Path.GetFileNameWithoutExtension(sourcePath);
                var files = new List<string>();
                if (format is "glb" or "both")
                {
                    string glb = Path.Combine(outputDirectory, stem + ".glb");
                    GlbExporter.Export(scene, glb);
                    files.Add(glb);
                }
                if (format is "obj" or "both")
                {
                    string obj = Path.Combine(outputDirectory, stem + ".obj");
                    ObjExporter.Export(scene, obj);
                    files.Add(obj);
                }
                return new ExportResult(scene.Meshes.Count, scene.Warnings.Count, files);
            });

            StatusText.Text =
                $"Готово: {result.MeshCount} mesh, предупреждений: {result.WarningCount}. " +
                $"Папка: {outputDirectory}";
        }
        catch (Exception exception)
        {
            StatusText.Text = "Ошибка экспорта: " + exception.Message;
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
        StatusText.Text = "Выберите исходный SMO-файл.";
        ExportButton.IsEnabled = false;
        ResetButton.IsEnabled = false;
    }

    private void SetBusy(bool busy)
    {
        SelectButton.IsEnabled = !busy;
        ExportButton.IsEnabled = !busy && _sourcePath is not null;
        ResetButton.IsEnabled = !busy && _sourcePath is not null;
        FormatComboBox.IsEnabled = !busy;
        BrowseOutputButton.IsEnabled = !busy;
    }

    private sealed record ExportResult(
        int MeshCount,
        int WarningCount,
        IReadOnlyList<string> Files);
}
