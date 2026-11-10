using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using Microsoft.Win32;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private string? _wizardResultGameExecutablePath;
    private bool _wizardResultGameLocationDone;
    private int _wizardResultGameLocationVersion;
    private Task? _wizardResultGameLocationTask;
    private CancellationTokenSource? _wizardResultGameLocationCancellation;
    private string? _wizardResultAnimationDirectory;
    private string? _wizardResultAnimationSource;
    private object? _wizardResultAnimationCatalogKey;
    private bool _syncingWizardResultControls;
    private bool _syncingWizardResultTime;

    private void RefreshWizardResultControls()
    {
        if (!_preparedStep || WizardResultExportButton is null) return;
        bool ready = WizardResultIsReady;
        WizardProtectionVisibilityPanel.Visibility = Visibility.Collapsed;
        PreparedStatusText.Text = _wizardResultIssue ?? WizardResultPreviewIssue ?? (WizardResultIsPreparing ? "Собираем готовую модель с текстурами…" :
            ready ? "Готовая модель. Можно проверить анимацию, экспортировать SMO или проверить его загрузку в игре." :
            GetWizardPreparedSaveBlockMessage() ?? "Подготавливаем результат…");
        PreparedStatusText.Foreground = _wizardResultIssue is null ? Brushes.LightSlateGray : Brushes.Salmon;
        WizardResultRetryButton.Visibility = _wizardResultIssue is null ? Visibility.Collapsed : Visibility.Visible;
        WizardResultRetryButton.IsEnabled = !WizardBaseBusy && !WizardResultIsPreparing;
        WizardResultExportButton.IsEnabled = ready && !WizardBusy;
        WizardResultExportStatus.Text = _wizardResultExportNotice ?? string.Empty;
        WizardResultCheckButton.IsEnabled = _nativeValidationRunning || ready && !WizardBusy &&
            _wizardResultGameExecutablePath is { } executable && File.Exists(executable);
        WizardResultCheckButton.Content = _nativeValidationRunning ? "Отменить проверку" : "Проверить загрузку в игре";
        WizardResultGamePathText.Text = !_wizardResultGameLocationDone ? "Ищем установленную игру…" :
            _wizardResultGameExecutablePath is null ? "Игра не найдена. Укажите WinxClub.exe." : "Игра: " + _wizardResultGameExecutablePath;
        WizardResultGamePathText.ToolTip = _wizardResultGameExecutablePath;
        WizardResultChooseGameButton.Content = _wizardResultGameExecutablePath is null ? "Указать исполняемый файл…" : "Другой EXE…";
        WizardResultChooseGameButton.IsEnabled = !_nativeValidationRunning;
        WizardResultValidationText.Text = _wizardResultValidationNotice ?? "Проверяется только загрузка SMO, без просмотра сцены.";
        WizardResultValidationText.Foreground = _wizardResultValidationNotice is null || _nativeValidationRunning ? Brushes.LightSlateGray :
            _wizardResultValidationVerdict == ImporterNativeVerdict.Suitable ? Brushes.LightGreen :
            _wizardResultValidationVerdict == ImporterNativeVerdict.Unsuitable ? Brushes.Salmon : Brushes.LightSlateGray;
        EnsureWizardResultGameLocation();
        RefreshWizardResultAnimationCatalog();
        RefreshWizardResultAnimationTime();
    }

    private void EnsureWizardResultGameLocation()
    {
        if (_wizardResultGameLocationDone || _wizardResultGameLocationTask is not null || _isClosing) return;
        int version = _wizardResultGameLocationVersion;
        var cancellation = new CancellationTokenSource();
        _wizardResultGameLocationCancellation = cancellation;
        _wizardResultGameLocationTask = LocateWizardResultGameAsync(version, cancellation);
    }

    private async Task LocateWizardResultGameAsync(int version, CancellationTokenSource cancellation)
    {
        try
        {
            string? located = await _nativeValidator.LocateInstalledExecutableAsync(cancellation.Token);
            if (!_isClosing && version == _wizardResultGameLocationVersion)
            {
                _wizardResultGameExecutablePath = located;
                if (located is not null) SetGameExecutablePath(located);
            }
        }
        catch (OperationCanceledException) { }
        catch (Exception error) { SessionLog.Error("RESULT_GAME_LOCATE", error); }
        finally
        {
            cancellation.Dispose();
            if (ReferenceEquals(_wizardResultGameLocationCancellation, cancellation)) _wizardResultGameLocationCancellation = null;
            _wizardResultGameLocationDone = true;
            if (!_isClosing) RefreshWizardResultControls();
        }
    }

    private void WizardResultChooseGame_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning) return;
        var dialog = new OpenFileDialog { Filter = "Winx Club (*.exe)|*.exe", FileName = "WinxClub.exe", CheckFileExists = true };
        if (dialog.ShowDialog(this) != true) return;
        ++_wizardResultGameLocationVersion;
        _wizardResultGameLocationDone = true;
        _wizardResultGameExecutablePath = Path.GetFullPath(dialog.FileName);
        SetGameExecutablePath(_wizardResultGameExecutablePath);
        _nativeValidator.SaveManualExecutablePath(_wizardResultGameExecutablePath);
        RefreshWizardResultControls();
    }

    private void RefreshWizardResultAnimationCatalog()
    {
        if (WizardResultAnimationCombo is null) return;
        if (!string.Equals(_sourcePath, _wizardResultAnimationSource, StringComparison.OrdinalIgnoreCase))
        {
            _wizardResultAnimationSource = _sourcePath;
            _wizardResultAnimationDirectory = null;
        }
        bool wings = _wizardResultScene?.Nodes.Any(node => node.Name is "WingL" or "WingR") == true || _wizardProtectionWingAttachments is not null;
        var key = (_wizardResultDocument, _sourcePath, WizardRigSupportsHumanoid, wings, _wizardResultGameExecutablePath, _wizardResultAnimationDirectory);
        if (Equals(key, _wizardResultAnimationCatalogKey)) return;
        _wizardResultAnimationCatalogKey = key;
        string? selected = (WizardResultAnimationCombo.SelectedItem as WizardResultAnimationEntry)?.Path;
        WizardResultAnimationDiscovery discovery = WizardResultAnimationCatalog.Discover(_sourcePath,
            WizardRigSupportsHumanoid, wings, _wizardResultGameExecutablePath, _wizardResultAnimationDirectory);
        _syncingWizardResultControls = true;
        try
        {
            WizardResultAnimationCombo.ItemsSource = discovery.Entries;
            WizardResultAnimationCombo.SelectedItem = discovery.Entries.FirstOrDefault(entry =>
                string.Equals(entry.Path, selected, StringComparison.OrdinalIgnoreCase));
            WizardResultAnimationSourceText.Text = discovery.Status;
            WizardResultAnimationSourceText.ToolTip = string.Join(Environment.NewLine, discovery.Folders);
        }
        finally { _syncingWizardResultControls = false; }
        if (selected is not null && WizardResultAnimationCombo.SelectedItem is null) ClearWizardResultAnimation();
        else if (WizardResultIsReady && _wizardResultAnimationRuntime is null &&
            WizardResultAnimationCombo.SelectedItem is WizardResultAnimationEntry entry)
            LoadWizardResultAnimation(entry.Path);
    }

    private void WizardResultAnimation_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_syncingWizardResultControls || !_preparedStep || !WizardResultIsReady) return;
        if (WizardResultAnimationCombo.SelectedItem is WizardResultAnimationEntry entry) LoadWizardResultAnimation(entry.Path);
        else ClearWizardResultAnimation();
        RefreshWizardResultAnimationTime();
    }

    private void WizardResultAnimationFolder_Click(object sender, RoutedEventArgs e)
    {
        if (WizardBusy) return;
        var dialog = new OpenFolderDialog { Title = "Папка тестовых анимаций SAN", Multiselect = false };
        if (dialog.ShowDialog(this) != true) return;
        _wizardResultAnimationDirectory = dialog.FolderName;
        _wizardResultAnimationCatalogKey = null;
        StopWizardResultAnimation();
        RefreshWizardResultControls();
    }

    private void WizardResultAnimationPlay_Click(object sender, RoutedEventArgs e)
    {
        if (WizardResultAnimationPlaying) PauseWizardResultAnimation(); else PlayWizardResultAnimation();
        RefreshWizardResultAnimationTime();
    }

    private void WizardResultAnimationStop_Click(object sender, RoutedEventArgs e)
    {
        StopWizardResultAnimation();
        RefreshWizardResultAnimationTime();
    }

    private void WizardResultAnimationSpeed_Changed(object sender, SelectionChangedEventArgs e)
    {
        if (WizardResultAnimationSpeedCombo?.SelectedItem is ComboBoxItem { Tag: string tag } &&
            double.TryParse(tag, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out double speed))
            WizardResultAnimationSpeed = speed;
    }

    private void WizardResultAnimationLoop_Changed(object sender, RoutedEventArgs e)
    {
        if (WizardResultAnimationLoopCheckBox is not null) WizardResultAnimationLoop = WizardResultAnimationLoopCheckBox.IsChecked == true;
    }

    private void WizardResultAnimationSeek_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (!_syncingWizardResultTime && _preparedStep && WizardResultAnimationDuration > 0)
            SeekWizardResultAnimation(e.NewValue);
    }

    private void RefreshWizardResultAnimationTime()
    {
        if (!_preparedStep || WizardResultAnimationSeek is null) return;
        bool available = WizardResultIsReady && WizardResultAnimationDuration > 0 && !WizardBaseBusy;
        WizardResultAnimationCombo.IsEnabled = WizardResultIsReady && !WizardBaseBusy;
        WizardResultAnimationFolderButton.IsEnabled = !WizardBusy;
        WizardResultAnimationPlayButton.IsEnabled = WizardResultAnimationStopButton.IsEnabled = available;
        WizardResultAnimationSpeedCombo.IsEnabled = WizardResultAnimationLoopCheckBox.IsEnabled = available;
        WizardResultAnimationPlayButton.Content = WizardResultAnimationPlaying ? "Пауза" : "Воспроизвести";
        _syncingWizardResultTime = true;
        try
        {
            WizardResultAnimationSeek.IsEnabled = available;
            WizardResultAnimationSeek.Maximum = Math.Max(WizardResultAnimationDuration, 0.001);
            WizardResultAnimationSeek.Value = Math.Clamp(WizardResultAnimationTime, 0, WizardResultAnimationSeek.Maximum);
            WizardResultAnimationTimeText.Text = $"{WizardResultAnimationTime:0.0} / {WizardResultAnimationDuration:0.0} с";
            WizardResultAnimationIssueText.Text = WizardResultAnimationIssue ?? string.Empty;
            WizardResultAnimationIssueText.Visibility = string.IsNullOrEmpty(WizardResultAnimationIssue) ? Visibility.Collapsed : Visibility.Visible;
        }
        finally { _syncingWizardResultTime = false; }
        RefreshWizardNotices();
    }
}
