using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using SmoImporter.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private sealed record WizardProtectionComponentRow(
        int Index, string Name, string Badge, string BadgeBackground);

    private static readonly GeneratedSkinningSemanticRegion[] WizardBothHands =
        [GeneratedSkinningSemanticRegion.LeftHand, GeneratedSkinningSemanticRegion.RightHand];
    private static readonly GeneratedSkinningSemanticRegion[] WizardLeftHand =
        [GeneratedSkinningSemanticRegion.LeftHand];
    private static readonly GeneratedSkinningSemanticRegion[] WizardRightHand =
        [GeneratedSkinningSemanticRegion.RightHand];

    private bool _wizardProtectionShowParts;
    private bool _syncingWizardProtectionControls;
    private bool _wizardProtectionRegionEditable;
    private string? _wizardProtectionComponentFingerprint;
    private WizardProtectionComponentRow[] _wizardProtectionComponentRows = [];

    private void RefreshWizardProtectionProgress()
    {
        if (WizardProtectionProgressPanel is null) return;
        bool running = WizardProtectionIsRunning;
        bool visible = (_postRigStep || _preparedStep) && WizardProtectionModeAvailable && (running || WizardProtectionIssue is not null);
        WizardProtectionProgressPanel.Visibility = visible ? Visibility.Visible : Visibility.Collapsed;
        if (!visible) return;
        WizardProtectionProgressText.Text = running
            ? (_wizardProtectionStopping ? _wizardProtectionProgressStage :
                $"{_wizardProtectionProgressFraction * 100:0}% — {_wizardProtectionProgressStage}")
            : "Можно повторить расчёт после исправления настроек.";
        WizardProtectionProgressBar.Visibility = running ? Visibility.Visible : Visibility.Collapsed;
        WizardProtectionProgressBar.IsIndeterminate = running && (_wizardProtectionProgressFraction <= 0 || _wizardProtectionStopping);
        WizardProtectionProgressBar.Value = _wizardProtectionProgressFraction * 100;
        WizardProtectionCancelButton.Visibility = running ? Visibility.Visible : Visibility.Collapsed;
        WizardProtectionCancelButton.IsEnabled = running && !_wizardProtectionStopping &&
            _wizardProtectionCancellation is { IsCancellationRequested: false };
        WizardProtectionRetryButton.Visibility = !running && WizardProtectionIssue is not null ? Visibility.Visible : Visibility.Collapsed;
        WizardProtectionRetryButton.IsEnabled = !running && !WizardBaseBusy;
    }

    private void WizardProtectionCancel_Click(object sender, RoutedEventArgs e)
    {
        if (_wizardProtectionCancellation is not { } cancellation || _wizardProtectionStopping) return;
        _wizardProtectionStopping = true;
        _wizardProtectionProgressStage = "Запрошена отмена. Завершаем текущий блок расчёта…";
        cancellation.Cancel();
        RefreshWizardProtectionProgress();
    }

    private void WizardProtectionRetry_Click(object sender, RoutedEventArgs e)
    {
        if (WizardProtectionIsRunning || WizardBaseBusy || !(_postRigStep || _preparedStep) || !WizardProtectionModeAvailable) return;
        _wizardProtectionRequest = null;
        EnsureWizardProtectionPreview();
    }

    private bool CanEditWizardProtectionPresentation => _postRigStep && WizardProtectionModeAvailable &&
        !WizardBaseBusy;

    private GeneratedSkinningSeparationPlaneKind? WizardProtectionSelectedPlane =>
        !_postRigStep || _wizardProtectionShowParts ? null : WizardProtectionRegionList?.SelectedIndex switch
        {
            0 => GeneratedSkinningSeparationPlaneKind.LeftShoulder,
            1 => GeneratedSkinningSeparationPlaneKind.RightShoulder,
            2 => GeneratedSkinningSeparationPlaneKind.Head,
            _ => null
        };

    private GeneratedSkinningSemanticRegion? WizardProtectionSelectedHand =>
        !_postRigStep || _wizardProtectionShowParts ? null : WizardProtectionRegionList?.SelectedIndex switch
        {
            3 or 4 => GeneratedSkinningSemanticRegion.LeftHand,
            5 => GeneratedSkinningSemanticRegion.RightHand,
            _ => null
        };

    private bool WizardProtectionMirrorSelection => WizardProtectionRegionList.SelectedIndex == 3 ||
        WizardProtectionMirrorHands.IsChecked == true;

    private IReadOnlyList<GeneratedSkinningSemanticRegion> WizardProtectionSelectedHands =>
        WizardProtectionSelectedHand is not { } hand ? [] : WizardProtectionMirrorSelection ? WizardBothHands :
        hand == GeneratedSkinningSemanticRegion.LeftHand ? WizardLeftHand : WizardRightHand;

    private IReadOnlyCollection<int> WizardProtectionSelectedComponentIndices =>
        _postRigStep && _wizardProtectionShowParts ? ReadWizardProtectionComponentSelection() : [];

    private int[] ReadWizardProtectionComponentSelection() => WizardProtectionComponentList?.SelectedItems
        .OfType<WizardProtectionComponentRow>().Select(row => row.Index).ToArray() ?? [];

    private void RefreshWizardProtectionPresentation()
    {
        RefreshWizardProtectionProgress();
        if (!_postRigStep || WizardProtectionStatusText is null || _syncingWizardProtectionControls)
            return;

        bool available = WizardProtectionModeAvailable;
        bool ready = WizardProtectionPreparation is not null;
        string? issue = WizardProtectionIssue;
        string status = !available ? "Защита и назначение деталей доступны в режиме создания весов." :
            !string.IsNullOrWhiteSpace(issue) ? issue :
            WizardProtectionIsRunning ? "Рассчитываем защиту и детали…" :
            ready ? "Проверьте области и детали. «Далее» примет настройки." : "Подготавливаем защитные области…";
        WizardProtectionStatusText.Text = status;
        WizardProtectionStatusText.Foreground = !string.IsNullOrWhiteSpace(issue) ? Brushes.Salmon : Brushes.LightSlateGray;
        WizardProtectionModeButtons.Visibility = available ? Visibility.Visible : Visibility.Collapsed;
        WizardProtectionAreasPanel.Visibility = available && !_wizardProtectionShowParts ? Visibility.Visible : Visibility.Collapsed;
        WizardProtectionPartsPanel.Visibility = available && _wizardProtectionShowParts ? Visibility.Visible : Visibility.Collapsed;
        WizardProtectionAreasButton.Foreground = _wizardProtectionShowParts ? Brushes.Gainsboro : Brushes.NavajoWhite;
        WizardProtectionPartsButton.Foreground = _wizardProtectionShowParts ? Brushes.NavajoWhite : Brushes.Gainsboro;
        if (!available)
            return;

        _syncingWizardProtectionControls = true;
        try
        {
            if (_wizardProtectionShowParts)
            {
                RefreshWizardProtectionComponentList();
                RefreshWizardProtectionWingControls();
            }
            else
                RefreshWizardProtectionRegionControls();
        }
        finally
        {
            _syncingWizardProtectionControls = false;
        }
    }

    private void RefreshWizardProtectionRegionControls()
    {
        bool handMode = WizardProtectionSelectedHand.HasValue;
        WizardProtectionPlaneControls.Visibility = handMode ? Visibility.Collapsed : Visibility.Visible;
        WizardProtectionHandControls.Visibility = handMode ? Visibility.Visible : Visibility.Collapsed;
        WizardProtectionMirrorHands.Visibility = handMode && WizardProtectionRegionList.SelectedIndex != 3
            ? Visibility.Visible : Visibility.Collapsed;

        bool available = handMode ? RefreshWizardProtectionHandControls() : RefreshWizardProtectionPlaneControls();
        _wizardProtectionRegionEditable = available && CanEditWizardProtectionPresentation;
        WizardProtectionRegionEditor.IsEnabled = _wizardProtectionRegionEditable;
        WizardProtectionMirrorHands.IsEnabled = CanEditWizardProtectionPresentation && handMode &&
            WizardProtectionRegions?.Regions.Any(region =>
                region.Region == WizardProtectionSelectedHand && region.AutomaticVolume is not null) == true;
    }

    private bool RefreshWizardProtectionPlaneControls()
    {
        if (WizardProtectionSelectedPlane is not { } kind)
            return false;

        GeneratedSkinningSeparationPlaneResolution? plane = WizardProtectionRegions?.SeparationPlanes
            .FirstOrDefault(value => value.Kind == kind);
        GeneratedSkinningSeparationPlaneAdjustment adjustment = GetWizardProtectionPlaneAdjustment(kind);
        bool hasOffset = kind == GeneratedSkinningSeparationPlaneKind.Head;
        Visibility offsetVisibility = hasOffset ? Visibility.Visible : Visibility.Collapsed;
        WizardProtectionPlaneOffset.Visibility = WizardProtectionPlaneOffsetLabel.Visibility =
            WizardProtectionPlaneOffsetValue.Visibility = offsetVisibility;
        if (plane is null)
        {
            WizardProtectionEnabled.IsChecked = false;
            WizardProtectionRegionInfoText.Text = "Плоскость появится после расчёта.";
            return false;
        }

        bool offsetAvailable = SetWizardProtectionSlider(WizardProtectionPlaneOffset,
            plane.Limits.MinimumOffset, plane.Limits.MaximumOffset, adjustment.Offset);
        bool angleAvailable = SetWizardProtectionSlider(WizardProtectionPlaneAngle,
            plane.Limits.MinimumAngleDegrees, plane.Limits.MaximumAngleDegrees, adjustment.AngleDegrees);
        bool available = plane.IsAvailable && offsetAvailable && angleAvailable;
        WizardProtectionEnabled.IsChecked = plane.IsAvailable && adjustment.Enabled;
        WizardProtectionPlaneControls.IsEnabled = adjustment.Enabled;
        WizardProtectionRegionInfoText.Text = !available ? "Не удалось надёжно построить эту плоскость." :
            hasOffset ? "Граница между головой и шеей." : "Граница между плечом и корпусом.";
        return available;
    }

    private bool RefreshWizardProtectionHandControls()
    {
        if (WizardProtectionSelectedHand is not { } hand)
            return false;

        IReadOnlyList<GeneratedSkinningSemanticRegion> selected = WizardProtectionSelectedHands;
        GeneratedSkinningRegionResolution[] resolved = WizardProtectionRegions?.Regions
            .Where(region => selected.Contains(region.Region) && region.AutomaticVolume is not null).ToArray() ?? [];
        if (resolved.Length != selected.Count)
        {
            WizardProtectionEnabled.IsChecked = false;
            WizardProtectionRegionInfoText.Text = WizardProtectionRegions is null ? "Объём появится после расчёта." :
                "Выбранную область кисти не удалось надёжно определить.";
            return false;
        }

        GeneratedSkinningRegionAdjustment adjustment = GetWizardProtectionHandAdjustment(hand);
        bool limitsAvailable = SetWizardProtectionSlider(WizardProtectionHandOffset,
                resolved.Max(value => value.AdjustmentLimits.MinimumAxialOffset),
                resolved.Min(value => value.AdjustmentLimits.MaximumAxialOffset), adjustment.AxialOffset) &
            SetWizardProtectionSlider(WizardProtectionHandLength,
                resolved.Max(value => value.AdjustmentLimits.MinimumAxialScale),
                resolved.Min(value => value.AdjustmentLimits.MaximumAxialScale), adjustment.AxialScale) &
            SetWizardProtectionSlider(WizardProtectionHandThickness,
                resolved.Max(value => value.AdjustmentLimits.MinimumRadialScale),
                resolved.Min(value => value.AdjustmentLimits.MaximumRadialScale), adjustment.RadialScale);
        bool enabled = selected.All(region => GetWizardProtectionHandAdjustment(region).Enabled);
        WizardProtectionEnabled.IsChecked = enabled;
        WizardProtectionHandControls.IsEnabled = enabled;
        WizardProtectionRegionInfoText.Text = limitsAvailable ? "Объём помогает сохранять силуэт кисти." :
            "У кистей разные допустимые границы. Настройте их отдельно.";
        return limitsAvailable;
    }

    private static bool SetWizardProtectionSlider(Slider slider, double minimum, double maximum, double value)
    {
        if (!double.IsFinite(minimum) || !double.IsFinite(maximum) || !double.IsFinite(value) || minimum > maximum)
            return false;

        slider.Minimum = Math.Min(minimum, slider.Minimum);
        slider.Maximum = maximum;
        slider.Minimum = minimum;
        double span = Math.Max(maximum - minimum, 0.001);
        slider.SmallChange = span / 100;
        slider.LargeChange = span / 10;
        slider.Value = Math.Clamp(value, minimum, maximum);
        return true;
    }

    private void RefreshWizardProtectionComponentList()
    {
        GeneratedSkinningAnalysis? analysis = WizardProtectionPreparation?.Analysis;
        IReadOnlyList<GeneratedSkinningComponentInfo> components = analysis?.Components ?? [];
        WizardProtectionComponentRow[] rows = components.Select(BuildWizardProtectionComponentRow).ToArray();
        bool sameDonor = string.Equals(_wizardProtectionComponentFingerprint, analysis?.DonorGeometryFingerprint, StringComparison.Ordinal);
        if (!sameDonor || !_wizardProtectionComponentRows.SequenceEqual(rows))
        {
            HashSet<int> selected = sameDonor ? ReadWizardProtectionComponentSelection().ToHashSet() : [];
            _wizardProtectionComponentFingerprint = analysis?.DonorGeometryFingerprint;
            _wizardProtectionComponentRows = rows;
            WizardProtectionComponentList.ItemsSource = rows;
            foreach (WizardProtectionComponentRow row in rows.Where(row => selected.Contains(row.Index)))
                WizardProtectionComponentList.SelectedItems.Add(row);
        }

        int[] selection = ReadWizardProtectionComponentSelection();
        bool editable = CanEditWizardProtectionPresentation && selection.Length > 0;
        WizardProtectionComponentList.IsEnabled = components.Count > 0;
        WizardProtectionWholeMeshButton.IsEnabled = editable &&
            FindWizardProtectionRelatedComponents(selection).Length > selection.Length;
        WizardProtectionAttachmentAutoButton.IsEnabled = editable;
        WizardProtectionAttachmentHeadButton.IsEnabled = editable;
        bool enoughWingParts = components.Count(component => selection.Contains(component.ComponentIndex) ||
            GetWizardProtectionComponentAttachment(component.ComponentIndex) == WizardProtectionAttachment.Wings) >= 2;
        WizardProtectionAttachmentWingsButton.IsEnabled = editable && enoughWingParts;
        SetFittingHint(WizardProtectionWingStatusText, WizardProtectionWingStatus ??
            (enoughWingParts ? "Назначьте выбранные детали крыльям." :
             "Выберите обе стороны крыльев. ЛКМ — рамка, Ctrl + щелчок — добавить к выбору."));
    }

    private WizardProtectionComponentRow BuildWizardProtectionComponentRow(GeneratedSkinningComponentInfo component)
    {
        WizardProtectionAttachment? assignment = GetWizardProtectionComponentAttachment(component.ComponentIndex);
        (string badge, string color) = assignment switch
        {
            WizardProtectionAttachment.Wings => ("Крылья", "#6043A2"),
            WizardProtectionAttachment.Head => ("Голова", "#26598E"),
            _ when component.AutomaticBinding == GeneratedSkinningAutomaticComponentBinding.Head => ("Голова · авто", "#26598E"),
            _ => ("Авто", "#344354")
        };
        string mesh = string.Join(" / ", component.MeshNames.Where(name => !string.IsNullOrWhiteSpace(name)).Distinct().Take(2));
        string title = string.IsNullOrEmpty(mesh) ? $"Деталь {component.ComponentIndex + 1}" : $"{mesh} · {component.ComponentIndex + 1}";
        return new WizardProtectionComponentRow(component.ComponentIndex, title, badge, color);
    }

    private void RefreshWizardProtectionWingControls()
    {
        bool hasWings = _wizardProtectionState.Components.Values.Any(choice => choice.Attachment == WizardProtectionAttachment.Wings);
        WizardProtectionComponentList.MaxHeight = hasWings ? 144 : 240;
        WizardProtectionWingHeightPanel.Visibility = hasWings ? Visibility.Visible : Visibility.Collapsed;
        WizardProtectionWingHeightPanel.IsEnabled = CanEditWizardProtection;
        WizardProtectionWingHeight.Value = _wizardProtectionState.WingHeightOffsetPercent;
        WizardProtectionWingHeightValue.Text = _wizardProtectionState.WingHeightOffsetPercent
            .ToString("+0.0;-0.0;0.0", System.Globalization.CultureInfo.InvariantCulture) + "%";
        WizardProtectionWingRotationPanel.Visibility = hasWings ? Visibility.Visible : Visibility.Collapsed;
        WizardProtectionWingRotationPanel.IsEnabled = CanEditWizardProtection;
        WizardProtectionWingRotation.Value = _wizardProtectionState.WingRotationDegrees;
        WizardProtectionWingRotationValue.Text = _wizardProtectionState.WingRotationDegrees
            .ToString("+0.0;-0.0;0.0", System.Globalization.CultureInfo.InvariantCulture) + "°";
    }

    private void WizardProtectionWingHeight_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_syncingWizardProtectionControls || WizardProtectionWingHeight is null || !CanEditWizardProtection)
            return;

        TrySetWizardProtectionWingHeight((float)WizardProtectionWingHeight.Value);
        RefreshWizardState();
    }

    private void WizardProtectionWingHeightReset_Click(object sender, RoutedEventArgs e)
    {
        TrySetWizardProtectionWingHeight(0);
        RefreshWizardState();
    }

    private void WizardProtectionWingRotation_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_syncingWizardProtectionControls || WizardProtectionWingRotation is null || !CanEditWizardProtection)
            return;

        TrySetWizardProtectionWingRotation((float)WizardProtectionWingRotation.Value);
        RefreshWizardState();
    }

    private void WizardProtectionWingRotationReset_Click(object sender, RoutedEventArgs e)
    {
        TrySetWizardProtectionWingRotation(0);
        RefreshWizardState();
    }

    private int[] FindWizardProtectionRelatedComponents(IReadOnlyCollection<int> selection)
    {
        IReadOnlyList<GeneratedSkinningComponentInfo> components = WizardProtectionPreparation?.Analysis.Components ?? [];
        HashSet<int> meshes = components.Where(component => selection.Contains(component.ComponentIndex))
            .SelectMany(component => component.MeshIndices).ToHashSet();
        return components.Where(component => component.MeshIndices.Any(meshes.Contains))
            .Select(component => component.ComponentIndex).ToArray();
    }

    private bool SelectWizardProtectionComponent(int componentIndex) =>
        SelectWizardProtectionComponents([componentIndex], Keyboard.Modifiers.HasFlag(ModifierKeys.Control));

    private bool SelectWizardProtectionComponents(IReadOnlyCollection<int> componentIndices, bool toggle)
    {
        if (!_postRigStep || !WizardProtectionModeAvailable || WizardProtectionPreparation?.Analysis.Components
            is not { } components || componentIndices.Any(index => !components.Any(component => component.ComponentIndex == index)))
            return false;

        _wizardProtectionShowParts = true;
        RefreshWizardProtectionPresentation();
        _syncingWizardProtectionControls = true;
        try
        {
            if (!toggle)
                WizardProtectionComponentList.SelectedItems.Clear();
            foreach (WizardProtectionComponentRow row in _wizardProtectionComponentRows.Where(row => componentIndices.Contains(row.Index)))
            {
                if (toggle && WizardProtectionComponentList.SelectedItems.Contains(row))
                    WizardProtectionComponentList.SelectedItems.Remove(row);
                else
                    WizardProtectionComponentList.SelectedItems.Add(row);
            }
            if (WizardProtectionComponentList.SelectedItems.Count > 0)
                WizardProtectionComponentList.ScrollIntoView(WizardProtectionComponentList.SelectedItems[0]);
        }
        finally
        {
            _syncingWizardProtectionControls = false;
        }
        RefreshWizardState();
        return true;
    }

    private void WizardProtectionMode_Click(object sender, RoutedEventArgs e)
    {
        _wizardProtectionShowParts = ReferenceEquals(sender, WizardProtectionPartsButton);
        RefreshWizardState();
    }

    private void WizardProtectionRegion_Changed(object sender, SelectionChangedEventArgs e) => RefreshWizardProtectionSelection();
    private void WizardProtectionMirror_Changed(object sender, RoutedEventArgs e) => RefreshWizardProtectionSelection();
    private void WizardProtectionComponent_SelectionChanged(object sender, SelectionChangedEventArgs e) => RefreshWizardProtectionSelection();

    private void RefreshWizardProtectionSelection()
    {
        if (_postRigStep && !_syncingWizardProtectionControls)
            RefreshWizardState();
    }

    private void WizardProtectionEnabled_Changed(object sender, RoutedEventArgs e) => UpdateWizardProtectionRegion(automatic: false);
    private void WizardProtectionSlider_Changed(object sender, RoutedPropertyChangedEventArgs<double> e) => UpdateWizardProtectionRegion(automatic: false);
    private void WizardProtectionRegionAuto_Click(object sender, RoutedEventArgs e) => UpdateWizardProtectionRegion(automatic: true);

    private void UpdateWizardProtectionRegion(bool automatic)
    {
        if (!CanEditWizardProtectionPresentation || _syncingWizardProtectionControls || !_wizardProtectionRegionEditable)
            return;

        bool enabled = WizardProtectionEnabled.IsChecked == true;
        if (WizardProtectionSelectedPlane is { } kind)
        {
            var adjustment = automatic ? GeneratedSkinningSeparationPlaneAdjustment.Automatic(kind) :
                new GeneratedSkinningSeparationPlaneAdjustment(kind, enabled, (float)WizardProtectionPlaneOffset.Value,
                    (float)WizardProtectionPlaneAngle.Value);
            TrySetWizardProtectionPlane(kind, adjustment);
        }
        else if (WizardProtectionSelectedHand is { } hand)
        {
            var adjustment = automatic ? GeneratedSkinningRegionAdjustment.Automatic(hand) :
                new GeneratedSkinningRegionAdjustment(hand, enabled, (float)WizardProtectionHandOffset.Value,
                    (float)WizardProtectionHandLength.Value, (float)WizardProtectionHandThickness.Value);
            TrySetWizardProtectionHand(hand, adjustment, WizardProtectionMirrorSelection);
        }
        RefreshWizardState();
    }

    private void WizardProtectionWholeMesh_Click(object sender, RoutedEventArgs e)
    {
        HashSet<int> indices = FindWizardProtectionRelatedComponents(ReadWizardProtectionComponentSelection()).ToHashSet();
        _syncingWizardProtectionControls = true;
        try
        {
            WizardProtectionComponentList.SelectedItems.Clear();
            foreach (WizardProtectionComponentRow row in _wizardProtectionComponentRows.Where(row => indices.Contains(row.Index)))
                WizardProtectionComponentList.SelectedItems.Add(row);
        }
        finally
        {
            _syncingWizardProtectionControls = false;
        }
        RefreshWizardState();
    }

    private void WizardProtectionAttachmentAuto_Click(object sender, RoutedEventArgs e) => AssignWizardProtectionComponents(null);
    private void WizardProtectionAttachmentHead_Click(object sender, RoutedEventArgs e) => AssignWizardProtectionComponents(WizardProtectionAttachment.Head);
    private void WizardProtectionAttachmentWings_Click(object sender, RoutedEventArgs e) => AssignWizardProtectionComponents(WizardProtectionAttachment.Wings);

    private void AssignWizardProtectionComponents(WizardProtectionAttachment? target)
    {
        if (!CanEditWizardProtectionPresentation)
            return;
        TrySetWizardProtectionComponents(ReadWizardProtectionComponentSelection(), target);
        RefreshWizardState();
    }
}
