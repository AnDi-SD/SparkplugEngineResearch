using System.IO;
using System.Numerics;
using SmoImporter.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private sealed record WizardRigState(
        TargetRigFittingPoseSnapshot Pose, TargetRigBodyPoseParameters BodyParameters, bool Accepted,
        bool DeferGeneratedSkinning);

    private WizardRigState? _wizardRigCommitted;
    private TargetRigFittingPoseSnapshot? _wizardRigViewPose;
    private TargetRigFittingPoseSnapshot? _wizardRigDraft;
    private TargetRigBodyPoseParameters? _wizardRigDraftParameters;
    private bool _wizardRigHumanoid;
    private bool _wizardRigHumanoidAutoFit;

    private TargetRigDefinition? WizardRigDefinition => _wizardRigViewPose?.Definition;
    private TargetRigFittingPoseSnapshot? WizardRigPose => _wizardRigDraft ?? _wizardRigViewPose;
    private TargetRigBodyPoseParameters WizardRigBodyParameters => _wizardRigDraftParameters ??
        _wizardRigCommitted?.BodyParameters ?? ToBodyPoseParameters(_committedBodyPoseControls);
    private bool WizardRigCanEdit => WizardRigDefinition is { DeformJointCount: > 0 };
    private bool WizardRigSupportsHumanoid => WizardRigCanEdit && _wizardRigHumanoid;
    private bool WizardRigCanAutoFit => WizardRigSupportsHumanoid && _wizardRigHumanoidAutoFit &&
        _sourceScene is not null && HasInputDonor;
    private bool WizardRigHasDraft => _wizardRigDraft is not null;
    private bool WizardRigPoseAccepted => _wizardRigCommitted?.Accepted == true;
    private bool WizardDefersGeneratedSkinning => _wizardRigCommitted?.DeferGeneratedSkinning == true;
    private string WizardRigStatus => !WizardRigCanEdit ? "Не удалось подготовить скелет для подгонки." :
        UsesPreparedModelPortingMode ? "В этом режиме поза используется для сравнения моделей." : string.Empty;

    private bool EnsureWizardRig()
    {
        if (!EnsureRigFittingState() || _targetRigFittingPose is not { } pose)
        {
            _wizardRigViewPose = null;
            DiscardWizardRigDraft();
            return false;
        }

        if (!ReferenceEquals(_wizardRigViewPose?.Definition, pose.Definition))
        {
            _wizardRigViewPose = pose.Capture();
            DiscardWizardRigDraft();
            _wizardRigHumanoid = SupportsWizardHumanoid(pose.Definition);
            _wizardRigHumanoidAutoFit = _wizardRigHumanoid &&
                pose.Definition.Joints.Any(joint => joint.Name == "Pelvis") &&
                pose.Definition.Joints.Any(joint => joint.Name == "Spine_03");
        }

        return WizardRigCanEdit;
    }

    private static bool SupportsWizardHumanoid(TargetRigDefinition rig)
    {
        try
        {
            // The mapper checks its own named deform chains, hierarchy, and lengths.
            // A nonzero neck value also validates the Neck -> Head control.
            _ = TargetRigBodyPoseMapper.CreateSnapshot(rig, TargetRigBodyPoseParameters.Neutral with { NeckForward = 1 });
            return true;
        }
        catch (Exception exception) when (IsWizardRigException(exception))
        {
            SessionLog.Info("WIZARD_RIG_PROFILE", exception.Message);
            return false;
        }
    }

    private Vector3 GetWizardRigJointDegrees(int jointIndex) => WizardRigPose is { } pose &&
        (uint)jointIndex < (uint)pose.LocalRotationDeltas.Count
        ? TargetRigEulerAngles.FromQuaternion(pose.LocalRotationDeltas[jointIndex]) : Vector3.Zero;

    private bool TrySetWizardRigJoint(int jointIndex, Vector3 degrees)
    {
        if (!_rigStep || WizardBusy || !EnsureWizardRig() || WizardRigPose is not { } current ||
            (uint)jointIndex >= (uint)current.Definition.Joints.Count || !current.Definition.Joints[jointIndex].IsDeformJoint)
            return false;

        try
        {
            TargetRigFittingPose pose = CreateMutablePose(current, localRotationsOnly: false);
            pose.SetLocalRotationDelta(jointIndex, TargetRigEulerAngles.ToQuaternion(degrees));
            StageWizardRigPose(pose.Capture(), WizardRigBodyParameters);
            return true;
        }
        catch (Exception exception) when (IsWizardRigException(exception))
        {
            return ReportWizardRigError(exception, "Не удалось повернуть эту кость. Проверьте углы.");
        }
    }

    private bool TrySetWizardRigBodyPose(TargetRigBodyPoseParameters parameters)
    {
        if (!_rigStep || WizardBusy || !EnsureWizardRig() || !WizardRigSupportsHumanoid || WizardRigPose is not { } current)
            return false;

        try
        {
            TargetRigFittingPoseSnapshot pose = TargetRigBodyPoseMapper.RebasePreservingCorrections(
                current.Definition, current, WizardRigBodyParameters, parameters);
            StageWizardRigPose(pose, parameters);
            return true;
        }
        catch (Exception exception) when (IsWizardRigException(exception))
        {
            return ReportWizardRigError(exception, "Не удалось подогнать позу. Проверьте значения.");
        }
    }

    private bool TryAutoFitWizardRig()
    {
        if (!_rigStep || WizardBusy || !EnsureWizardRig() || !WizardRigCanAutoFit ||
            WizardRigPose is not { } current || _sourceScene is not { } source)
            return false;

        try
        {
            TargetRigAutomaticPoseFitResult fit = TargetRigAutomaticPoseFitter.Fit(
                current.Definition, source, ResolveBodyPoseDonorScene(), _wizardTransform);
            TargetRigFittingPoseSnapshot pose = TargetRigBodyPoseMapper.RebasePreservingCorrections(
                current.Definition, current, WizardRigBodyParameters, fit.Parameters, fit.Pose);
            SessionLog.Info("WIZARD_RIG_AUTOFIT", string.Join(" ", fit.Diagnostics));
            StageWizardRigPose(pose, fit.Parameters);
            return true;
        }
        catch (Exception exception) when (IsWizardRigException(exception))
        {
            return ReportWizardRigError(exception, "Автоподгонка не нашла подходящую позу. Попробуйте настроить её вручную.");
        }
    }

    private bool TryResetWizardRigPose()
    {
        if (!_rigStep || WizardBusy || !EnsureWizardRig() || WizardRigDefinition is not { } rig)
            return false;

        StageWizardRigPose(rig.CreateFittingPose().Capture(), TargetRigBodyPoseParameters.Neutral);
        return true;
    }

    private void StageWizardRigPose(TargetRigFittingPoseSnapshot pose, TargetRigBodyPoseParameters parameters)
    {
        if (_wizardRigViewPose is { } committed && pose.RootRotation == committed.RootRotation &&
            pose.RootTranslation == committed.RootTranslation && pose.LocalRotationDeltas.SequenceEqual(committed.LocalRotationDeltas) &&
            parameters == (_wizardRigCommitted?.BodyParameters ?? ToBodyPoseParameters(_committedBodyPoseControls)))
        {
            DiscardWizardRigDraft();
        }
        else
        {
            _wizardRigDraft = pose;
            _wizardRigDraftParameters = parameters;
        }
        _inputError = null;
    }

    private bool TryApplyWizardRigPose()
    {
        if (!_rigStep || WizardBusy || !EnsureWizardRig() || WizardRigPose is not { } current)
            return false;
        if (WizardRigPoseAccepted && !WizardRigHasDraft)
            return false;

        try
        {
            TargetRigFittingPose candidate = CreateMutablePose(current, localRotationsOnly: UsesGeneratedWeightsPortingMode);
            var state = new WizardRigState(candidate.Capture(), WizardRigBodyParameters, Accepted: true,
                DeferGeneratedSkinning: true);
            if (UsesAdaptDonorWeightsPortingMode)
            {
                SetWizardManualPlacement(true);
                SyncWizardAlignmentForMode();
            }
            PublishWizardRigPose(candidate, state.Pose, state.BodyParameters, state.Accepted);
            _wizardRigCommitted = state;
            _inputError = null;
            return true;
        }
        catch (Exception exception) when (IsWizardRigException(exception))
        {
            return ReportWizardRigError(exception, "Не удалось применить позу. Проверьте положение костей.");
        }
    }

    private void PublishWizardRigPose(TargetRigFittingPose pose, TargetRigFittingPoseSnapshot view,
        TargetRigBodyPoseParameters parameters, bool accepted)
    {
        Vector3[] jointDegrees = GetPoseEulerDegrees(view);
        _targetRigFittingPose = pose;
        _rigLocalEulerDegrees = jointDegrees;
        _rigRootEulerDegrees = TargetRigEulerAngles.FromQuaternion(view.RootRotation);
        _committedBodyPoseControls = _draftBodyPoseControls = FromBodyPoseParameters(parameters);
        _bodyPoseDraftSnapshot = null;
        _bodyPoseEditorDirty = _rigPoseEditorDirty = _manualAlignmentEditorDirty = false;
        _generatedRigPoseAccepted = accepted && UsesGeneratedWeightsPortingMode;
        _rigFittingRevision++;
        _plan = null;
        _glbSkinTransferPlan = null;
        InvalidateAdaptedPortingPreparation();
        InvalidateGeneratedSkinningPreparation();
        WriteBodyPoseControls(_committedBodyPoseControls);
        LoadRigFittingEditorValues();
        _wizardRigViewPose = view;
        DiscardWizardRigDraft();
        if (UsesAdaptDonorWeightsPortingMode)
            UpdateAdaptedPortingPreparation();
        else if (UsesPreparedModelPortingMode && !ReplacementIsSmo)
            UpdateGlbSkinTransferPlan();
    }

    private WizardRigState? CaptureWizardRigState()
    {
        // A command-line preset can already have an accepted pose before the
        // skeleton step is opened. Keep it as the initial history state.
        if (_wizardRigCommitted is null && _startupPreset is not null && _generatedRigPoseAccepted &&
            _targetRigFittingPose is { } presetPose)
        {
            _wizardRigCommitted = new WizardRigState(presetPose.Capture(),
                ToBodyPoseParameters(_committedBodyPoseControls), Accepted: true, DeferGeneratedSkinning: false);
        }
        return _wizardRigCommitted;
    }

    private void RestoreWizardRigState(WizardRigState? state)
    {
        DiscardWizardRigDraft();
        if (state is null)
        {
            if (_wizardRigCommitted is { } previous && _targetRigDefinition is { } current &&
                current.SourceFingerprint == previous.Pose.Definition.SourceFingerprint)
            {
                TargetRigFittingPose bind = current.CreateFittingPose();
                PublishWizardRigPose(bind, bind.Capture(), TargetRigBodyPoseParameters.Neutral, accepted: false);
            }
            _wizardRigCommitted = null;
            return;
        }

        if (!EnsureWizardRig() || WizardRigDefinition is not { } rig ||
            rig.SourceFingerprint != state.Pose.Definition.SourceFingerprint ||
            rig.Joints.Count != state.Pose.LocalRotationDeltas.Count)
            throw new InvalidDataException("The saved fitting pose does not match the current source rig.");

        if (ReferenceEquals(_wizardRigCommitted, state) && ReferenceEquals(_wizardRigViewPose, state.Pose) &&
            _generatedRigPoseAccepted == (state.Accepted && UsesGeneratedWeightsPortingMode))
            return;

        // History is bound to the source bytes, not to the in-memory rig instance.
        // Reloading the same SMO creates a new definition with the same joint order.
        TargetRigFittingPose pose = rig.CreateFittingPose();
        for (int joint = 0; joint < rig.Joints.Count; joint++)
            pose.SetLocalRotationDelta(joint, state.Pose.LocalRotationDeltas[joint]);
        pose.SetRootTransform(state.Pose.RootRotation, state.Pose.RootTranslation);
        TargetRigFittingPoseSnapshot view = ReferenceEquals(rig, state.Pose.Definition) ? state.Pose : pose.Capture();
        PublishWizardRigPose(pose, view, state.BodyParameters, state.Accepted);
        _wizardRigCommitted = state;
    }

    private void ResetWizardRigState()
    {
        _wizardRigCommitted = null;
        _wizardRigViewPose = null;
        DiscardWizardRigDraft();
    }

    private void InvalidateWizardRigAlignment()
    {
        DiscardWizardRigDraft();
        if (_wizardRigCommitted is { Accepted: true } state)
            _wizardRigCommitted = state with { Accepted = false };
        _generatedRigPoseAccepted = false;
    }

    private void DiscardWizardRigDraft()
    {
        _wizardRigDraft = null;
        _wizardRigDraftParameters = null;
    }

    private bool ReportWizardRigError(Exception exception, string message)
    {
        SessionLog.Error("WIZARD_RIG", exception);
        _inputError = message;
        return false;
    }

    private static bool IsWizardRigException(Exception exception) => exception is
        InvalidDataException or InvalidOperationException or OverflowException or ArgumentException or
        NotSupportedException or KeyNotFoundException;
}
