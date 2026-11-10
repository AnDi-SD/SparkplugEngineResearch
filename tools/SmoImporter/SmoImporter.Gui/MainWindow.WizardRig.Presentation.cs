using System.Globalization;
using System.Numerics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using SmoImporter.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private sealed record WizardRigJointRow(int Index, string Name, Thickness Indent, string Detail);

    private TargetRigDefinition? _wizardRigPresentedDefinition;
    private TargetRigFittingPoseSnapshot? _wizardRigPresentedPose;
    private WizardRigJointRow[] _wizardRigJointRows = [];
    private int? _wizardRigSelectedJoint;
    private bool _wizardRigHumanoidMode;
    private bool _wizardRigJointInputInvalid;
    private bool _syncingWizardRigControls;
    private bool _wizardRigWasVisible;

    private int? WizardRigSelectedJointIndex => _rigStep && !_wizardRigHumanoidMode ? _wizardRigSelectedJoint : null;

    private string? GetWizardRigNextBlockMessage()
    {
        if (WizardBusy)
            return "Дождитесь завершения текущей операции.";
        if (WizardRigDefinition is null || !WizardRigCanEdit)
            return string.IsNullOrEmpty(WizardRigStatus) ? "Подгонка скелета недоступна для этой пары." : WizardRigStatus;
        if (_wizardRigJointInputInvalid)
            return "Введите углы от −180° до 180°.";
        return null;
    }

    private void RefreshWizardRigPresentation()
    {
        if (WizardRigDetectionText is null || _syncingWizardRigControls)
            return;
        if (!_rigStep)
        {
            _wizardRigWasVisible = false;
            _wizardRigJointInputInvalid = false;
            return;
        }

        bool entering = !_wizardRigWasVisible;
        _wizardRigWasVisible = true;
        EnsureWizardRig();
        TargetRigDefinition? definition = WizardRigDefinition;
        bool hasRig = definition is { DeformJointCount: > 0 };
        bool humanoid = hasRig && WizardRigSupportsHumanoid;
        bool definitionChanged = !ReferenceEquals(_wizardRigPresentedDefinition, definition);
        if (definitionChanged)
        {
            _wizardRigPresentedDefinition = definition;
            _wizardRigJointRows = definition is null ? [] : BuildWizardRigJointRows(definition);
            _wizardRigSelectedJoint = _wizardRigJointRows.FirstOrDefault()?.Index;
            _wizardRigHumanoidMode = humanoid;
            _wizardRigJointInputInvalid = false;
            UpdateWizardRigJointFilter(clearSearch: true);
        }
        if (!humanoid)
            _wizardRigHumanoidMode = false;

        TargetRigFittingPoseSnapshot? pose = WizardRigPose;
        if (entering || definitionChanged || !ReferenceEquals(_wizardRigPresentedPose, pose))
        {
            _wizardRigJointInputInvalid = false;
            _wizardRigPresentedPose = pose;
            WriteWizardRigJointFields();
        }
        if (humanoid)
            WriteWizardRigBodySliders(WizardRigBodyParameters);

        bool editable = hasRig && WizardRigCanEdit && !WizardBusy;
        bool canChoose = hasRig && !WizardBusy && !_wizardRigJointInputInvalid;
        WizardRigDetectionText.Text = !hasRig ? "Скелет недоступен." :
            humanoid ? $"Распознана гуманоидная схема · костей: {definition!.DeformJointCount}" :
            $"Ручная подгонка · костей: {definition!.DeformJointCount}";
        WizardRigModesPanel.Visibility = humanoid ? Visibility.Visible : Visibility.Collapsed;
        WizardRigModesPanel.IsEnabled = canChoose;
        WizardRigHumanoidModeButton.Foreground = _wizardRigHumanoidMode ? Brushes.NavajoWhite : Brushes.Gainsboro;
        WizardRigJointModeButton.Foreground = _wizardRigHumanoidMode ? Brushes.Gainsboro : Brushes.NavajoWhite;
        WizardRigHumanoidPanel.Visibility = hasRig && _wizardRigHumanoidMode ? Visibility.Visible : Visibility.Collapsed;
        WizardRigHumanoidPanel.IsEnabled = editable;
        WizardRigAutoFitButton.IsEnabled = editable && WizardRigCanAutoFit;
        WizardRigJointPanel.Visibility = hasRig && !_wizardRigHumanoidMode ? Visibility.Visible : Visibility.Collapsed;
        WizardRigJointList.IsEnabled = WizardRigJointSearchBox.IsEnabled = canChoose;
        WizardRigJointEditor.IsEnabled = editable && _wizardRigSelectedJoint is not null;
        WizardRigResetButton.Visibility = hasRig ? Visibility.Visible : Visibility.Collapsed;
        WizardRigResetButton.IsEnabled = editable;
        SetFittingHint(WizardRigFeedbackText, _wizardRigJointInputInvalid ? "Введите углы от −180° до 180°." :
            WizardRigHasDraft ? "Изменения позы видны справа." :
            !string.IsNullOrEmpty(WizardRigStatus) ? WizardRigStatus : WizardRigPoseAccepted ? "Поза применена." : null);
    }

    private static WizardRigJointRow[] BuildWizardRigJointRows(TargetRigDefinition definition)
    {
        IReadOnlyList<TargetRigJoint> joints = definition.Joints;
        return joints.Where(joint => joint.IsDeformJoint).Select(joint =>
        {
            int depth = 0;
            string? parentName = null;
            for (int parent = joint.ParentJointIndex; parent >= 0; parent = joints[parent].ParentJointIndex)
            {
                if (!joints[parent].IsDeformJoint)
                    continue;
                parentName ??= joints[parent].Name;
                depth++;
            }
            string detail = parentName is null ? joint.Name : $"{joint.Name} · родительская кость {parentName}";
            return new WizardRigJointRow(joint.JointIndex, joint.Name, new Thickness(Math.Min(depth, 5) * 8, 0, 0, 0), detail);
        }).ToArray();
    }

    private void UpdateWizardRigJointFilter(bool clearSearch = false)
    {
        _syncingWizardRigControls = true;
        try
        {
            if (clearSearch)
                WizardRigJointSearchBox.Clear();
            string search = WizardRigJointSearchBox.Text.Trim();
            WizardRigJointRow[] visible = search.Length == 0 ? _wizardRigJointRows :
                _wizardRigJointRows.Where(row => row.Name.Contains(search, StringComparison.OrdinalIgnoreCase)).ToArray();
            WizardRigJointList.ItemsSource = visible;
            WizardRigJointList.SelectedItem = visible.FirstOrDefault(row => row.Index == _wizardRigSelectedJoint);
            SetFittingHint(WizardRigSearchHintText, search.Length != 0 && visible.Length == 0 ? "Кости с таким именем не найдены." : null);
        }
        finally
        {
            _syncingWizardRigControls = false;
        }
    }

    private bool SelectWizardRigJoint(int index)
    {
        if (!_rigStep || WizardBusy || _wizardRigJointInputInvalid || !_wizardRigJointRows.Any(row => row.Index == index))
            return false;

        _wizardRigHumanoidMode = false;
        _wizardRigSelectedJoint = index;
        bool hiddenBySearch = !WizardRigJointList.Items.OfType<WizardRigJointRow>().Any(row => row.Index == index);
        UpdateWizardRigJointFilter(clearSearch: hiddenBySearch);
        WriteWizardRigJointFields();
        RefreshWizardState();
        if (WizardRigJointList.SelectedItem is { } selected)
            WizardRigJointList.ScrollIntoView(selected);
        return true;
    }

    private void WriteWizardRigJointFields()
    {
        WizardRigJointRow? row = _wizardRigJointRows.FirstOrDefault(item => item.Index == _wizardRigSelectedJoint);
        WizardRigSelectedJointText.Text = row?.Name ?? "Выберите кость в списке или на модели.";
        WriteWizardRigJointControls(row is null ? Vector3.Zero : GetWizardRigJointDegrees(row.Index), writeText: true);
    }

    private void WriteWizardRigJointControls(Vector3 degrees, bool writeText)
    {
        _syncingWizardRigControls = true;
        try
        {
            if (writeText)
            {
                WizardRigJointXBox.Text = DisplayWizardRigAngle(degrees.X);
                WizardRigJointYBox.Text = DisplayWizardRigAngle(degrees.Y);
                WizardRigJointZBox.Text = DisplayWizardRigAngle(degrees.Z);
            }
            WizardRigJointXSlider.Value = degrees.X;
            WizardRigJointYSlider.Value = degrees.Y;
            WizardRigJointZSlider.Value = degrees.Z;
        }
        finally
        {
            _syncingWizardRigControls = false;
        }
    }

    private static string DisplayWizardRigAngle(float value) =>
        value == 0 ? "0" : value.ToString("G", CultureInfo.CurrentCulture);

    private void WriteWizardRigBodySliders(TargetRigBodyPoseParameters parameters)
    {
        _syncingWizardRigControls = true;
        try
        {
            WizardRigArmRaiseSlider.Value = parameters.ArmElevationDegrees;
            WizardRigArmForwardSlider.Value = parameters.ArmForwardDegrees;
            WizardRigElbowBendSlider.Value = parameters.ElbowBendDegrees;
            WizardRigLegSpreadSlider.Value = parameters.LegSpreadDegrees;
            WizardRigKneeBendSlider.Value = parameters.KneeBendDegrees;
            WizardRigTorsoPitchSlider.Value = parameters.TorsoPitchDegrees;
            WizardRigNeckForwardSlider.Value = parameters.NeckForward;
        }
        finally
        {
            _syncingWizardRigControls = false;
        }
    }

    private void WizardRigMode_Click(object sender, RoutedEventArgs e)
    {
        if (!_rigStep || WizardBusy || _wizardRigJointInputInvalid)
            return;
        _wizardRigHumanoidMode = ReferenceEquals(sender, WizardRigHumanoidModeButton) && WizardRigSupportsHumanoid;
        WriteWizardRigJointFields();
        RefreshWizardState();
    }

    private void WizardRigSearch_Changed(object sender, TextChangedEventArgs e)
    {
        if (_rigStep && !_syncingWizardRigControls && !_wizardRigJointInputInvalid)
            UpdateWizardRigJointFilter();
    }

    private void WizardRigJointSelection_Changed(object sender, SelectionChangedEventArgs e)
    {
        if (!_rigStep || _syncingWizardRigControls || _wizardRigJointInputInvalid || WizardBusy ||
            WizardRigJointList.SelectedItem is not WizardRigJointRow row)
            return;
        _wizardRigSelectedJoint = row.Index;
        WriteWizardRigJointFields();
        RefreshWizardState();
    }

    private void WizardRigJointText_Changed(object sender, TextChangedEventArgs e)
    {
        if (!_rigStep || _syncingWizardRigControls || WizardBusy || !WizardRigCanEdit ||
            WizardRigSelectedJointIndex is not int index)
            return;

        if (!TryReadWizardRigJointAngles(out Vector3 degrees))
        {
            _wizardRigJointInputInvalid = true;
            RefreshWizardState();
            return;
        }

        _wizardRigJointInputInvalid = false;
        _inputError = null;
        if (degrees == GetWizardRigJointDegrees(index) || TrySetWizardRigJoint(index, degrees))
        {
            _wizardRigPresentedPose = WizardRigPose;
            WriteWizardRigJointControls(degrees, writeText: false);
        }
        else
        {
            WriteWizardRigJointFields();
        }
        RefreshWizardState();
    }

    private bool TryReadWizardRigJointAngles(out Vector3 degrees)
    {
        degrees = Vector3.Zero;
        if (!TryReadWizardNumber(WizardRigJointXBox, out float x) || !TryReadWizardNumber(WizardRigJointYBox, out float y) ||
            !TryReadWizardNumber(WizardRigJointZBox, out float z) || x is < -180 or > 180 || y is < -180 or > 180 || z is < -180 or > 180)
            return false;
        degrees = new Vector3(x, y, z);
        return true;
    }

    private void WizardRigJointSlider_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (!_rigStep || _syncingWizardRigControls || WizardBusy || !WizardRigCanEdit ||
            WizardRigSelectedJointIndex is null)
            return;

        TextBox input = ReferenceEquals(sender, WizardRigJointXSlider) ? WizardRigJointXBox :
            ReferenceEquals(sender, WizardRigJointYSlider) ? WizardRigJointYBox : WizardRigJointZBox;
        input.Text = DisplayWizardRigAngle((float)e.NewValue);
    }

    private void WizardRigBodySlider_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (!_rigStep || _syncingWizardRigControls || WizardBusy || !_wizardRigHumanoidMode || !WizardRigCanEdit)
            return;
        _inputError = null;
        TrySetWizardRigBodyPose(new TargetRigBodyPoseParameters(
            (float)WizardRigArmRaiseSlider.Value, (float)WizardRigArmForwardSlider.Value, (float)WizardRigElbowBendSlider.Value,
            (float)WizardRigLegSpreadSlider.Value, (float)WizardRigKneeBendSlider.Value, (float)WizardRigTorsoPitchSlider.Value,
            (float)WizardRigNeckForwardSlider.Value));
        RefreshWizardState();
    }

    private void WizardRigAutoFit_Click(object sender, RoutedEventArgs e)
    {
        if (!_rigStep || WizardBusy || !WizardRigCanAutoFit)
            return;
        _inputError = null;
        TryAutoFitWizardRig();
        RefreshWizardState();
    }

    private void WizardRigReset_Click(object sender, RoutedEventArgs e)
    {
        if (!_rigStep || WizardBusy || !WizardRigCanEdit)
            return;
        _wizardRigJointInputInvalid = false;
        _inputError = null;
        TryResetWizardRigPose();
        WriteWizardRigJointFields();
        RefreshWizardState();
    }
}
