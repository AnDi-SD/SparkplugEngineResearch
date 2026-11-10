using System.Globalization;
using System.IO;
using System.Numerics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media.Media3D;
using SmoExporter.Core;
using SmoImporter.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private const double MinimumWizardScalePercent = 0.01;
    private const double MaximumWizardScalePercent = 1_000_000;

    private ReplacementTransform _wizardTransform = ReplacementTransform.Identity;
    private ReplacementTransform? _shownWizardTransform;
    private string? _shownWizardScaleText;
    private bool _syncingWizardTransformText;
    private SmoExportScene? _wizardSizeSource;
    private ImportedScene? _wizardSizeDonor;
    private Vector3 _wizardMeasuredRotation;
    private Rect3D _wizardSourceBounds = Rect3D.Empty;
    private Rect3D _wizardDonorBounds = Rect3D.Empty;

    private bool HasUnappliedWizardTransform => _sizeStep && _shownWizardTransform == _wizardTransform &&
        (!TryReadWizardTransform(out ReplacementTransform pending) || pending != _wizardTransform);

    private void RefreshWizardSize()
    {
        if (WizardTransformHintText is null)
            return;

        UpdateWizardSizePreview();
        if (!_sizeStep)
        {
            _shownWizardTransform = null;
            _wizardSizeSource = null;
            _wizardSizeDonor = null;
            _wizardSourceBounds = _wizardDonorBounds = Rect3D.Empty;
            return;
        }

        MeasureWizardBounds();
        if (_shownWizardTransform != _wizardTransform)
            ShowWizardTransform();

        WizardSourceSizeText.Text = FormatWizardSize(_wizardSourceBounds, 1f);
        WizardDonorSizeText.Text = FormatWizardSize(_wizardDonorBounds, _wizardTransform.Scale);

        bool editable = !WizardBusy && HasInputDonor;
        bool placementAvailable = WizardCanAdjustPlacement;
        bool pending = HasUnappliedWizardTransform;
        bool valid = !pending || TryReadWizardTransform(out _);
        WizardScalePercentTextBox.IsEnabled = editable && WizardCanAdjustScale;
        WizardRotationXBox.IsEnabled = WizardRotationYBox.IsEnabled = WizardRotationZBox.IsEnabled =
            WizardOffsetXBox.IsEnabled = WizardOffsetYBox.IsEnabled = WizardOffsetZBox.IsEnabled = editable && placementAvailable;
        WizardManualPlacementButton.Visibility = UsesAdaptDonorWeightsPortingMode ? Visibility.Visible : Visibility.Collapsed;
        WizardManualPlacementButton.IsEnabled = editable && !pending;
        WizardManualPlacementButton.Content = ManualAdaptWeights ? "Вернуть авто" : "Настроить положение вручную";
        WizardTransformApplyButton.IsEnabled = editable && pending && valid;
        WizardTransformResetButton.IsEnabled = editable && (pending || _wizardTransform != ReplacementTransform.Identity);
        WizardPlacementAutoFitButton.IsEnabled = editable && !pending &&
            TryGetWizardFit(out ReplacementTransform fit) && fit != _wizardTransform;
        SetFittingHint(WizardPlacementAvailabilityText, placementAvailable ? null :
            !WizardCanAdjustScale ? "Для этой пары подгонка недоступна." :
            IsStaticReplacementPair ? "Для прямого переноса SMO доступен масштаб." :
            "В выбранном режиме масштаб меняет предпросмотр.");
        SetFittingHint(WizardTransformHintText, !valid ?
            TryReadWizardScale(out _) ? "Проверьте числа поворота и смещения." : "Введите масштаб от 0,01 до 1 000 000%." :
            pending ? "Примените введённые значения." : null);
    }

    private static void SetFittingHint(TextBlock label, string? message)
    {
        label.Text = message ?? string.Empty;
        label.Visibility = message is null ? Visibility.Collapsed : Visibility.Visible;
    }

    private void MeasureWizardBounds()
    {
        if (!ReferenceEquals(_wizardSizeSource, _sourceScene))
        {
            _wizardSizeSource = _sourceScene;
            _wizardSourceBounds = _sourceScene is null ? Rect3D.Empty : MeasureWizardSource(_sourceScene);
        }
        if (!ReferenceEquals(_wizardSizeDonor, _replacementScene) ||
            _wizardMeasuredRotation != _wizardTransform.RotationDegrees)
        {
            _wizardSizeDonor = _replacementScene;
            _wizardMeasuredRotation = _wizardTransform.RotationDegrees;
            _wizardDonorBounds = Rect3D.Empty;
            if (_replacementScene is { } donor)
            {
                Matrix4x4 rotation = new ReplacementTransform(1f, _wizardMeasuredRotation, Vector3.Zero).Matrix;
                foreach (ImportedMesh mesh in donor.Meshes)
                    IncludeWizardMesh(ref _wizardDonorBounds, mesh.Positions, mesh.TriangleIndices, rotation);
            }
        }
    }

    private static Rect3D MeasureWizardSource(SmoExportScene scene)
    {
        var meshes = scene.Meshes.ToDictionary(mesh => mesh.VariantKey);
        Rect3D bounds = Rect3D.Empty;
        foreach (SmoExportMeshPlacement placement in scene.MeshPlacements)
        {
            if (!meshes.TryGetValue(placement.EffectiveMeshKey, out SmoExportMesh? mesh))
                continue;

            // Mesh positions and placement matrices already use the export axes.
            Matrix4x4 world = mesh.SkinObjectIndex is null ? placement.WorldMatrix : Matrix4x4.Identity;
            IncludeWizardMesh(ref bounds, mesh.Positions, mesh.TriangleIndices, world);
        }
        return bounds;
    }

    private static void IncludeWizardMesh(ref Rect3D bounds, Vector3[] positions, uint[] indices, Matrix4x4 world)
    {
        foreach (uint index in indices)
        {
            if (index >= positions.Length)
                continue;
            Vector3 point = Vector3.Transform(positions[index], world);
            if (float.IsFinite(point.X) && float.IsFinite(point.Y) && float.IsFinite(point.Z))
                bounds.Union(new Point3D(point.X, point.Y, point.Z));
        }
    }

    private static string FormatWizardSize(Rect3D bounds, float scale) => bounds.IsEmpty ? "—" :
        $"{bounds.SizeX * scale:G6} × {bounds.SizeY * scale:G6} × {bounds.SizeZ * scale:G6}";

    private static bool TryParseWizardNumber(TextBox input, out double value)
    {
        string text = input.Text;
        return (double.TryParse(text, NumberStyles.Float, CultureInfo.CurrentCulture, out value) ||
                double.TryParse(text.Replace(',', '.'), NumberStyles.Float, CultureInfo.InvariantCulture, out value)) &&
            double.IsFinite(value);
    }

    private static bool TryReadWizardNumber(TextBox input, out float value)
    {
        bool parsed = TryParseWizardNumber(input, out double number);
        value = (float)number;
        return parsed && float.IsFinite(value);
    }

    private bool TryReadWizardScale(out float scale)
    {
        scale = _wizardTransform.Scale;
        if (!WizardCanAdjustScale || WizardScalePercentTextBox.Text == _shownWizardScaleText)
            return true;
        return TryParseWizardNumber(WizardScalePercentTextBox, out double percent) &&
            TryCreateWizardScale(percent, out scale);
    }

    private bool TryReadWizardTransform(out ReplacementTransform transform)
    {
        transform = _wizardTransform;
        if (!TryReadWizardScale(out float scale))
            return false;
        transform = transform with { Scale = scale };
        if (!WizardCanAdjustPlacement)
            return true;
        if (!TryReadWizardNumber(WizardRotationXBox, out float rx) ||
            !TryReadWizardNumber(WizardRotationYBox, out float ry) ||
            !TryReadWizardNumber(WizardRotationZBox, out float rz) ||
            !TryReadWizardNumber(WizardOffsetXBox, out float x) ||
            !TryReadWizardNumber(WizardOffsetYBox, out float y) ||
            !TryReadWizardNumber(WizardOffsetZBox, out float z))
            return false;

        transform = transform with { RotationDegrees = new Vector3(rx, ry, rz), Translation = new Vector3(x, y, z) };
        Matrix4x4 matrix = transform.Matrix;
        return IsFiniteMatrix(matrix) && Matrix4x4.Invert(matrix, out Matrix4x4 inverse) && IsFiniteMatrix(inverse);
    }

    private bool TryGetWizardEntryFit(out ReplacementTransform transform)
    {
        MeasureWizardBounds();
        return TryGetWizardFit(out transform);
    }

    private bool TryGetWizardFit(out ReplacementTransform transform)
    {
        transform = _wizardTransform;
        if (!WizardCanAdjustScale)
            return false;
        if (WizardCanAdjustPlacement)
            return TryGetWizardAutoFit(out transform);
        if (!TryGetWizardHeightScale(out float scale))
            return false;
        transform = transform with { Scale = scale };
        return true;
    }

    private bool TryGetWizardHeightScale(out float scale)
    {
        scale = 1f;
        return !_wizardSourceBounds.IsEmpty && _wizardSourceBounds.SizeY > 0 &&
            !_wizardDonorBounds.IsEmpty && _wizardDonorBounds.SizeY > 0 &&
            TryCreateWizardScale(100 * _wizardSourceBounds.SizeY / _wizardDonorBounds.SizeY, out scale);
    }

    private bool TryGetWizardAutoFit(out ReplacementTransform transform)
    {
        transform = _wizardTransform;
        if (_wizardSourceBounds.IsEmpty || _wizardDonorBounds.IsEmpty)
            return false;

        // The fitter uses only bounds. These are already measured from the
        // rendered source and the donor vertices after the applied rotation.
        static Vector3[] Corners(Rect3D bounds) =>
        [
            new((float)bounds.X, (float)bounds.Y, (float)bounds.Z),
            new((float)(bounds.X + bounds.SizeX), (float)(bounds.Y + bounds.SizeY), (float)(bounds.Z + bounds.SizeZ))
        ];
        try
        {
            ReplacementTransform fit = ReplacementTransformFitter.FitByHeightAndCenter(
                Corners(_wizardSourceBounds), Corners(_wizardDonorBounds));
            transform = fit with { RotationDegrees = _wizardTransform.RotationDegrees };
            return TryCreateWizardScale(100d * fit.Scale, out _) && IsFiniteMatrix(transform.Matrix);
        }
        catch (Exception exception) when (exception is InvalidOperationException or InvalidDataException)
        {
            return false;
        }
    }

    private static bool TryCreateWizardScale(double percent, out float scale)
    {
        scale = 1f;
        if (percent is not (>= MinimumWizardScalePercent and <= MaximumWizardScalePercent))
            return false;

        scale = (float)(percent / 100d);
        return true;
    }

    private void ShowWizardTransform()
    {
        _shownWizardTransform = _wizardTransform;
        _syncingWizardTransformText = true;
        try
        {
            static string Display(float value) => value.ToString("G", CultureInfo.CurrentCulture);
            _shownWizardScaleText = Display(_wizardTransform.Scale * 100f);
            WizardScalePercentTextBox.Text = _shownWizardScaleText;
            Vector3 rotation = _wizardTransform.RotationDegrees;
            WizardRotationXBox.Text = Display(rotation.X);
            WizardRotationYBox.Text = Display(rotation.Y);
            WizardRotationZBox.Text = Display(rotation.Z);
            Vector3 translation = _wizardTransform.Translation;
            WizardOffsetXBox.Text = Display(translation.X);
            WizardOffsetYBox.Text = Display(translation.Y);
            WizardOffsetZBox.Text = Display(translation.Z);
        }
        finally
        {
            _syncingWizardTransformText = false;
        }
    }

    private void ApplyWizardTransform(ReplacementTransform transform)
    {
        if (_wizardTransform != transform)
        {
            InvalidateWizardRigAlignment();
            InvalidateWizardProtectionPose();
        }
        _wizardTransform = transform;
        ShowWizardTransform();
        _inputError = null;
        ApplyWizardAlignmentForMode();
        RecordInputStep();
        RefreshWizardState();
    }

    private void WizardTransform_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (_sizeStep && !_syncingWizardTransformText)
            RefreshWizardState();
    }

    private void WizardTransform_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key != Key.Enter)
            return;

        e.Handled = true;
        WizardTransformApply_Click(sender, e);
    }

    private void WizardTransformApply_Click(object sender, RoutedEventArgs e)
    {
        if (_sizeStep && !WizardBusy && HasUnappliedWizardTransform && TryReadWizardTransform(out ReplacementTransform transform))
            ApplyWizardTransform(transform);
    }

    private void WizardTransformReset_Click(object sender, RoutedEventArgs e)
    {
        if (_sizeStep && !WizardBusy)
            ApplyWizardTransform(ReplacementTransform.Identity);
    }

    private void WizardPlacementAutoFit_Click(object sender, RoutedEventArgs e)
    {
        if (_sizeStep && !WizardBusy && !HasUnappliedWizardTransform && TryGetWizardFit(out ReplacementTransform transform))
        {
            ApplyWizardTransform(transform);
            FrameWizardFittedModel();
        }
    }
}
