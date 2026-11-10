using System.IO;
using System.Windows;
using Microsoft.Win32;
using SmoExporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private sealed record WizardResultKey(SmoDocument Target, object Donor, InputStepState Inputs, long RigRevision);
    private WizardResultKey? _wizardResultKey;
    private CancellationTokenSource? _wizardResultCancellation;
    private Task? _wizardResultOperation;
    private SmoDocument? _wizardResultDocument;
    private SmoExportScene? _wizardResultScene;
    private string? _wizardResultPath;
    private string? _wizardResultIssue;
    private string? _wizardResultActionIssue;
    private string? _wizardResultExportNotice;
    private string? _wizardResultValidationNotice;
    private ImporterNativeVerdict _wizardResultValidationVerdict;
    private bool WizardResultIsPreparing => _wizardResultCancellation is not null;
    private SmoDocument? WizardResultDocument => _wizardResultDocument;
    private SmoExportScene? WizardResultScene => _wizardResultScene;
    private bool WizardResultIsReady => _preparedStep && _wizardResultDocument is not null &&
        _wizardResultPath is not null && _wizardResultKey == GetWizardResultKey();

    private WizardResultKey? GetWizardResultKey() => _document is { } target && _replacementScene is { } donor
        ? new(target, donor, CaptureInputStep(), _rigFittingRevision) : null;

    private void EnsureWizardResult()
    {
        if (!_preparedStep || _isClosing)
        {
            CancelWizardResult();
            return;
        }
        WizardResultKey? key = GetWizardResultKey();
        if (key == _wizardResultKey)
            return;
        Task? previous = _wizardResultOperation;
        CancelWizardResult();
        if (GetWizardPreparedSaveBlockMessage() is not null || key is null)
            return;
        _wizardResultKey = key;
        _wizardResultIssue = null;
        string directory = Path.Combine(Path.GetTempPath(), "Sparkplug", "SmoImporter", "Result", Guid.NewGuid().ToString("N"));
        string path = Path.Combine(directory, "result.smo");
        try
        {
            Func<CancellationToken, string> writer = CreateModelWriteOperation(path, IsStaticReplacementPair);
            var cancellation = new CancellationTokenSource();
            _wizardResultCancellation = cancellation;
            _wizardResultOperation = BuildWizardResultAsync(key, path, writer, previous, cancellation);
        }
        catch (Exception error)
        {
            SessionLog.Error("RESULT_PREPARE", error);
            _wizardResultIssue = DescribeUserError(error, "готовую модель") + " Вернитесь к настройкам или повторите подготовку.";
        }
    }

    private async Task BuildWizardResultAsync(WizardResultKey key, string path, Func<CancellationToken, string> writer,
        Task? previous, CancellationTokenSource cancellation)
    {
        bool published = false;
        try
        {
            await Task.Yield();
            if (previous is not null) await previous;
            cancellation.Token.ThrowIfCancellationRequested();
            await RunSaveOperationAsync(writer, cancellation.Token);
            var loaded = await Task.Run(() =>
            {
                cancellation.Token.ThrowIfCancellationRequested();
                SmoDocument document = SmoDocument.Load(path);
                if (document.HasErrors) throw new InvalidDataException("Result SMO failed strict readback.");
                return (Document: document, Scene: SmoSceneBuilder.Build(document));
            }, cancellation.Token);
            if (_isClosing || !_preparedStep || !ReferenceEquals(_wizardResultCancellation, cancellation) || key != GetWizardResultKey())
                return;
            _wizardResultDocument = loaded.Document;
            _wizardResultScene = loaded.Scene;
            _wizardResultPath = path;
            published = true;
            SessionLog.Info("RESULT_PREPARE", $"Strict readback ready: {path}");
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested) { }
        catch (Exception error)
        {
            SessionLog.Error("RESULT_PREPARE", error);
            if (ReferenceEquals(_wizardResultCancellation, cancellation) && !_isClosing)
                _wizardResultIssue = DescribeUserError(error, "готовую модель") + " Вернитесь к настройкам или повторите подготовку.";
        }
        finally
        {
            if (!published) DeleteWizardResultCacheFile(path);
            if (ReferenceEquals(_wizardResultCancellation, cancellation))
            {
                _wizardResultCancellation = null;
                if (!_isClosing) RefreshWizardState();
            }
            cancellation.Dispose();
        }
    }

    private void CancelWizardResult()
    {
        _wizardResultCancellation?.Cancel();
        _wizardResultCancellation = null;
        _wizardResultKey = null;
        _wizardResultDocument = null;
        _wizardResultScene = null;
        ClearWizardResultPreview();
        if (_wizardResultPath is { } path) DeleteWizardResultCacheFile(path);
        _wizardResultPath = null;
        _wizardResultIssue = null;
        _wizardResultActionIssue = null;
        _wizardResultExportNotice = null;
        _wizardResultValidationNotice = null;
    }

    private static void DeleteWizardResultCacheFile(string path)
    {
        try
        {
            File.Delete(path);
            if (Path.GetDirectoryName(path) is { } directory && Directory.Exists(directory)) Directory.Delete(directory);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException) { }
    }

    private void WizardResultRetry_Click(object sender, RoutedEventArgs e)
    {
        if (!WizardResultIsPreparing && !WizardBaseBusy)
        {
            _wizardResultKey = null;
            RefreshWizardState();
        }
    }

    private async void WizardResultExport_Click(object sender, RoutedEventArgs e)
    {
        if (!WizardResultIsReady || WizardBusy) return;
        var dialog = new SaveFileDialog
        {
            Filter = "Sparkplug model (*.smo)|*.smo",
            FileName = Path.GetFileNameWithoutExtension(_sourcePath) + "_" + Path.GetFileNameWithoutExtension(_replacementPath) + ".smo",
            InitialDirectory = Path.GetDirectoryName(_sourcePath)
        };
        if (dialog.ShowDialog(this) != true) return;
        try { await ExportWizardResultAsync(dialog.FileName); }
        catch (OperationCanceledException) { _wizardResultExportNotice = "Экспорт отменён."; }
        catch (Exception error)
        {
            SessionLog.Error("RESULT_EXPORT", error);
            _wizardResultActionIssue = DescribeUserError(error, "экспорт SMO");
            _wizardResultExportNotice = _wizardResultActionIssue;
        }
        RefreshWizardResultControls();
    }

    private async Task ExportWizardResultAsync(string destination)
    {
        if (!WizardResultIsReady || WizardBusy || _wizardResultPath is not { } cached) return;
        _wizardResultActionIssue = null;
        EnsureSeparateOutput(destination, _sourcePath, "исходный SMO");
        EnsureSeparateOutput(destination, _replacementPath, "файл-донор");
        using var cancellation = new CancellationTokenSource();
        _saveCancellation = cancellation;
        _saveRunning = true;
        SaveOperationStatusText.Text = "Сохраняется и проверяется готовый SMO…";
        CancelSaveOperationButton.IsEnabled = true;
        SaveOperationOverlay.Visibility = Visibility.Visible;
        PauseWizardResultAnimation();
        RefreshWizardState();
        try
        {
            string output = await RunSaveOperationAsync(token => SmoVerifiedOutputInstaller.Install(destination,
                File.ReadAllBytes(cached), temporary =>
                {
                    if (SmoDocument.Load(temporary).HasErrors) throw new InvalidDataException("Export failed strict readback.");
                }, token, _sourcePath, _replacementPath, cached).OutputPath, cancellation.Token);
            _lastSavedOutputPath = output;
            _lastSavedSourcePath = _sourcePath;
            _wizardResultExportNotice = "Сохранено: " + Path.GetFileName(output);
            SessionLog.Info("RESULT_EXPORT", output);
        }
        finally
        {
            if (ReferenceEquals(_saveCancellation, cancellation)) _saveCancellation = null;
            _saveRunning = false;
            SaveOperationOverlay.Visibility = Visibility.Collapsed;
            if (!_isClosing) RefreshWizardState();
        }
    }

    private async void WizardResultCheck_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning) { _nativeValidationCancellation?.Cancel(); return; }
        if (!WizardResultIsReady || WizardBusy || _wizardResultGameExecutablePath is not { } executable || _wizardResultPath is not { } asset) return;
        _wizardResultActionIssue = null;
        using var cancellation = new CancellationTokenSource();
        _nativeValidationCancellation = cancellation;
        _nativeValidationRunning = true;
        _wizardResultValidationVerdict = ImporterNativeVerdict.Indeterminate;
        _wizardResultValidationNotice = "Проверяем загрузку SMO…";
        PauseWizardResultAnimation();
        RefreshWizardState();
        try
        {
            ImporterNativeValidationResult result = await _nativeValidator.ValidateLoadAsync(executable, asset,
                InferLogicalGameAssetPath(_sourcePath!), cancellation.Token);
            _wizardResultValidationVerdict = result.Verdict;
            _wizardResultValidationNotice = result.Message;
        }
        catch (OperationCanceledException) { _wizardResultValidationNotice = "Проверка отменена."; }
        catch (Exception error)
        {
            SessionLog.Error("RESULT_CHECK", error);
            _wizardResultValidationVerdict = ImporterNativeVerdict.Indeterminate;
            _wizardResultActionIssue = DescribeUserError(error, "проверку загрузки в игре");
            _wizardResultValidationNotice = _wizardResultActionIssue;
        }
        finally
        {
            if (ReferenceEquals(_nativeValidationCancellation, cancellation)) _nativeValidationCancellation = null;
            _nativeValidationRunning = false;
            if (!_isClosing) RefreshWizardState();
        }
    }
}
