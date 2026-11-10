using System.IO;
using System.Windows;
using Microsoft.Win32;
using SmoExporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private sealed record SourceLoadResult(string Path, SmoDocument Document, SmoExportScene Scene);
    private CancellationTokenSource? _sourceLoadCancellation;

    private async void SelectSource_Click(object sender, RoutedEventArgs e)
    {
        if (WizardBusy) return;
        var dialog = new OpenFileDialog { Filter = "Sparkplug model (*.smo)|*.smo", CheckFileExists = true };
        if (dialog.ShowDialog(this) == true) await LoadSelectedSourceAsync(dialog.FileName);
    }

    private static SourceLoadResult ReadSource(string path, CancellationToken token)
    {
        string fullPath = Path.GetFullPath(path);
        SessionLog.Info("SOURCE_LOAD", $"Requested source SMO: {fullPath}");
        token.ThrowIfCancellationRequested();
        ValidateSafeSmoFile(fullPath);
        SmoDocument document = SmoDocument.Load(fullPath);
        if (document.HasErrors)
            throw new InvalidDataException("Исходный SMO повреждён: " + string.Join(" | ", document.Diagnostics
                .Where(diagnostic => diagnostic.Severity == SmoDiagnosticSeverity.Error)
                .Take(3).Select(diagnostic => diagnostic.Code + ": " + diagnostic.Message)));
        token.ThrowIfCancellationRequested();
        SmoExportScene scene = SmoSceneBuilder.Build(document);
        token.ThrowIfCancellationRequested();
        return new SourceLoadResult(fullPath, document, scene);
    }

    private bool LoadSource(string path)
    {
        try { ApplyLoadedSource(ReadSource(path, CancellationToken.None)); return true; }
        catch (Exception exception)
        {
            ReportInputError(exception, DescribeUserError(exception, "Исходный SMO", path));
            return false;
        }
    }

    private async Task<bool> LoadSourceAsync(string path)
    {
        CancelSourceLoad();
        using var cancellation = new CancellationTokenSource();
        _sourceLoadCancellation = cancellation;
        RefreshWizardNotices();
        SourceLoadResult? previous = _sourcePath is { } previousPath && _document is { } previousDocument &&
            _sourceScene is { } previousScene ? new(previousPath, previousDocument, previousScene) : null;
        bool applying = false;
        try
        {
            SourceLoadResult loaded = await Task.Run(() => ReadSource(path, cancellation.Token), cancellation.Token);
            cancellation.Token.ThrowIfCancellationRequested();
            if (_isClosing || !ReferenceEquals(_sourceLoadCancellation, cancellation)) return false;
            applying = true;
            ApplyLoadedSource(loaded);
            return true;
        }
        catch (Exception exception) when (!_isClosing)
        {
            if (ReferenceEquals(_sourceLoadCancellation, cancellation))
            {
                if (applying)
                {
                    try
                    {
                        if (previous is null) ClearSourceInput();
                        else ApplyLoadedSource(previous);
                    }
                    catch (Exception recoveryError) { SessionLog.Error("SOURCE_RECOVERY", recoveryError); }
                }
                ReportInputError(exception, exception is OperationCanceledException ? "Загрузка исходного SMO отменена." :
                    DescribeUserError(exception, "Исходный SMO", path));
            }
            return false;
        }
        catch (Exception) when (_isClosing) { return false; }
        finally
        {
            if (ReferenceEquals(_sourceLoadCancellation, cancellation)) _sourceLoadCancellation = null;
        }
    }

    private void CancelSourceLoad() => _sourceLoadCancellation?.Cancel();

    private void ApplyLoadedSource(SourceLoadResult loaded)
    {
        _sourcePath = loaded.Path;
        _document = loaded.Document;
        _targetFittingGpuPreview?.Clear();
        _sourceScene = loaded.Scene;
        _plan = null;
        ResetRigFittingState();
        InvalidateAdaptedPortingPreparation();
        InvalidateGeneratedSkinningPreparation();
        SourcePathText.Text = _sourcePath;
        SourceSummaryText.Text = $"{_sourceScene.Meshes.Count} mesh slots; {_sourceScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; {_sourceScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles.";
        UpdatePortingModeRecommendation();
        if (UsesPreparedModelPortingMode && (ReplacementIsSmo || _replacementScene?.HasSkinning == true))
            UpdateGlbSkinTransferPlan();
        else if (UsesAdaptDonorWeightsPortingMode && _replacementScene?.HasSkinning == true)
            UpdateAdaptedPortingPreparation();
        else if (UsesGeneratedWeightsPortingMode)
            InitializeGeneratedSkinningForManualApply();
        else
            StatusText.Text = "Шаблон SMO загружен.";
        _framePreviewOnRefresh = true;
        RefreshState();
        if (GetPortingModeBlockMessage() is string blocked) StatusText.Text = blocked;
        if (IsLoaded && (ResolveGameExecutablePath() is null || string.IsNullOrWhiteSpace(_nativeValidator.SavedExecutablePath)))
            _ = LocateGameExecutableAsync();
        SessionLog.Info("SOURCE_LOAD", $"Loaded {_sourcePath}; meshes={_sourceScene.Meshes.Count}; " +
            $"vertices={_sourceScene.Meshes.Sum(mesh => mesh.Positions.Length)}; " +
            $"triangles={_sourceScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3)}; bytes={_document.Data.Length}");
    }
}
