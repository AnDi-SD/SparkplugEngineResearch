using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Runtime.CompilerServices;
using System.Windows;
using Microsoft.Win32;
using WinxHairPatcher.Core;

namespace WinxHairPatcher.Gui;

public partial class MainWindow : Window
{
    private string? _exePath;
    private readonly ObservableCollection<FashionItem> _items = [];

    public MainWindow() : this(null) { }

    public MainWindow(string? initialExePath)
    {
        InitializeComponent();
        FashionList.ItemsSource = _items;
        LoadItems(WinxExeHairPatcher.OriginalDisabledMask);
        if (!string.IsNullOrWhiteSpace(initialExePath))
            LoadExe(initialExePath, showErrorDialog: false);
    }

    private void SelectExe_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Filter = "Winx Club executable (WinxClub.exe)|WinxClub.exe|Executable (*.exe)|*.exe",
            CheckFileExists = true
        };
        if (dialog.ShowDialog(this) != true) return;
        LoadExe(dialog.FileName, showErrorDialog: true);
    }

    private void LoadExe(string path, bool showErrorDialog)
    {
        try
        {
            string fullPath = Path.GetFullPath(path);
            WinxExePatchState state = WinxExeHairPatcher.Inspect(fullPath);
            _exePath = state.IsSupported ? fullPath : null;
            ExePathText.Text = fullPath;
            ExeStateText.Text = state.Description;
            ExeStateText.Foreground = state.IsSupported
                ? System.Windows.Media.Brushes.DarkGreen : System.Windows.Media.Brushes.DarkRed;
            PatchButton.IsEnabled = state.IsSupported;
            if (state.IsSupported) LoadItems(state.DisabledMask);
        }
        catch (Exception exception)
        {
            _exePath = null;
            ExePathText.Text = path;
            ExeStateText.Text = "Не удалось проверить EXE: " + exception.Message;
            ExeStateText.Foreground = System.Windows.Media.Brushes.DarkRed;
            PatchButton.IsEnabled = false;
            StatusText.Text = "Ошибка: " + exception.Message;
            if (showErrorDialog)
                MessageBox.Show(this, exception.Message, "Ошибка", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void Patch_Click(object sender, RoutedEventArgs e)
    {
        if (_exePath is null) return;
        ushort disabledMask = 0;
        foreach (FashionItem item in _items)
            if (!item.KeepHair) disabledMask |= checked((ushort)(1 << item.Id));

        MessageBoxResult confirmation = MessageBox.Show(this,
            "Будет изменён основной WinxClub.exe. Рядом с ним автоматически сохранится резервная копия. Продолжить?",
            "Подтверждение EXE-патча", MessageBoxButton.YesNo, MessageBoxImage.Warning);
        if (confirmation != MessageBoxResult.Yes) return;

        try
        {
            WinxExePatchResult result = WinxExeHairPatcher.PatchFile(_exePath, disabledMask);
            StatusText.Text = $"Патч для игры и меню костюмов применён. Резервная копия: {result.BackupPath}";
            ExeStateText.Text = "EXE содержит совместимый hair-патч для игрового режима и меню костюмов.";
            MessageBox.Show(this,
                $"Готово. Волосы настроены и в игре, и в меню костюмов.\n\nРезервная копия:\n{result.BackupPath}",
                "WinxClub.exe обновлён", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception exception) { ShowError(exception); }
    }

    private void LoadItems(ushort disabledMask)
    {
        _items.Clear();
        foreach (BloomFashion fashion in WinxExeHairPatcher.Fashions)
        {
            bool keep = fashion.CanEnableExternalHair && (disabledMask & (1 << fashion.Id)) == 0;
            _items.Add(new FashionItem(fashion.Id, fashion.DisplayName, fashion.ModelName,
                fashion.CanEnableExternalHair, keep,
                fashion.CanEnableExternalHair ? "" :
                    "Внешние волосы отключены патчем. Включение заблокировано как небезопасное."));
        }
    }

    private void ShowError(Exception exception)
    {
        StatusText.Text = "Ошибка: " + exception.Message;
        MessageBox.Show(this, exception.Message, "Ошибка", MessageBoxButton.OK, MessageBoxImage.Error);
    }

    private sealed class FashionItem : INotifyPropertyChanged
    {
        private bool _keepHair;
        public FashionItem(int id, string name, string model, bool canEnable, bool keepHair, string hint) =>
            (Id, Name, Model, CanEnable, _keepHair, Hint) = (id, name, model, canEnable, keepHair, hint);
        public int Id { get; }
        public string Name { get; }
        public string Model { get; }
        public bool CanEnable { get; }
        public string Hint { get; }
        public bool KeepHair { get => _keepHair; set { _keepHair = value; OnPropertyChanged(); } }
        public event PropertyChangedEventHandler? PropertyChanged;
        private void OnPropertyChanged([CallerMemberName] string? name = null) =>
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
    }
}
