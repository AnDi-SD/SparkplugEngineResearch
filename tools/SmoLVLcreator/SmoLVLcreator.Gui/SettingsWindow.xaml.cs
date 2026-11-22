using Microsoft.Win32;
using SmoLVLcreator.Core;
using System.IO;
using System.Windows;
using System.Windows.Controls;

namespace SmoLVLcreator.Gui;

public partial class SettingsWindow : Window
{
    private readonly SmoLVLcreatorSettings _settings;

    public SettingsWindow(SmoLVLcreatorSettings settings)
    {
        _settings = settings;
        InitializeComponent();
        ExportDirectoryBox.Text = settings.ExportDirectory;
        ExportFormatBox.SelectedIndex = settings.ExportFormat switch
        {
            SmoLevelExportFormat.Fbx => 1,
            SmoLevelExportFormat.Obj => 2,
            _ => 0
        };
    }

    public SmoLVLcreatorSettings? Result { get; private set; }

    private void BrowseExportDirectory_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFolderDialog
        {
            Title = LevelText.Get("Dialog.Settings.QuickExportDirectory"),
            Multiselect = false,
            InitialDirectory = Directory.Exists(ExportDirectoryBox.Text)
                ? ExportDirectoryBox.Text
                : null
        };
        if (dialog.ShowDialog(this) == true)
            ExportDirectoryBox.Text = dialog.FolderName;
    }

    private void Save_Click(object sender, RoutedEventArgs e)
    {
        string directory;
        try
        {
            directory = Path.GetFullPath(ExportDirectoryBox.Text.Trim());
        }
        catch (Exception exception) when (exception is ArgumentException or
                                          NotSupportedException or
                                          PathTooLongException)
        {
            MessageBox.Show(
                this,
                LevelText.Get("Dialog.Settings.InvalidExportDirectory"),
                LevelText.Get("Dialog.Settings.Title"),
                MessageBoxButton.OK,
                MessageBoxImage.Warning);
            return;
        }
        if (string.IsNullOrWhiteSpace(directory))
            return;

        string tag = (ExportFormatBox.SelectedItem as ComboBoxItem)?.Tag as string ??
            nameof(SmoLevelExportFormat.Glb);
        if (!Enum.TryParse(tag, ignoreCase: true, out SmoLevelExportFormat format))
            format = SmoLevelExportFormat.Glb;
        Result = _settings with { ExportDirectory = directory, ExportFormat = format };
        DialogResult = true;
    }
}
