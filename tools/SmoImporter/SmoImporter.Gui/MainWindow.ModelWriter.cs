using System.IO;
using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    // Capture the exact writer inputs on the UI thread for both file export and result preview.
    private Func<CancellationToken, string> CreateModelWriteOperation(string outputPath, bool useWizardStaticTransform)
    {
        bool rebuildStaticNative = NativeStaticFallbackReady;
        bool staticModelMode = rebuildStaticNative || (UsesStaticModelPortingMode && AllReplacementMeshesAreUnskinned);
        bool skinnedGlbMode = (((UsesPreparedModelPortingMode || UsesAdaptDonorWeightsPortingMode) && AllReplacementMeshesAreSkinned) ||
            (UsesGeneratedWeightsPortingMode && GeneratedSkinningPreparationIsCurrent));
        SmoDocument targetDocument = _document ?? throw new InvalidOperationException("Исходный SMO не выбран.");
        string? donorPath = _replacementPath;
        ReplacementTransform wizardTransform = useWizardStaticTransform ? _wizardTransform : ReplacementTransform.Identity;
        Func<CancellationToken, string> saveOperation;
        if (ReplacementIsSmo && !rebuildStaticNative)
        {
            SmoDocument donorDocument = CurrentNativeDonorDocument;
            saveOperation = cancellationToken => wizardTransform.Scale == 1f
                ? SmoNativeVisualGraphReplacer.Replace(
                    targetDocument, donorDocument, outputPath, cancellationToken).OutputPath
                : SmoNativeVisualGraphReplacer.ReplaceScaledStatic(
                    targetDocument, donorDocument, outputPath, wizardTransform.Scale, cancellationToken).OutputPath;
        }
        else if (staticModelMode)
        {
            ReplacementTransform transform = useWizardStaticTransform ? wizardTransform : ReadTransform();
            ImportedScene staticScene = rebuildStaticNative ? _staticNativeReplacementScene! :
                _replacementScene ?? throw new InvalidOperationException("Модель-донор не выбрана.");
            saveOperation = cancellationToken =>
                SmoLevelModelGraphReplacer.ReplaceFile(
                    targetDocument,
                    GetFirstMeshObjectIndex(targetDocument),
                    staticScene,
                    transform,
                    outputPath,
                    SmoLevelModelGraphWriteOptions.StandaloneStatic,
                    cancellationToken,
                    donorPath).OutputPath;
        }
        else if (skinnedGlbMode)
        {
            ImportedScene skinnedScene;
            SkinnedGeometryTransferMode geometryMode;
            if (UsesGeneratedWeightsPortingMode)
            {
                skinnedScene = GeneratedSkinningPreparationIsCurrent
                    ? _generatedSkinningPreparation!.PreparedScene
                    :
                    throw new InvalidOperationException(
                        _generatedSkinningPreparationIssue ??
                        "Автоматические веса не подготовлены.");
                geometryMode = SkinnedGeometryTransferMode.PreservePreparedGeometry;
            }
            else if (UsesAdaptDonorWeightsPortingMode)
            {
                skinnedScene = AdaptedPortingPreparationIsCurrent
                    ? _adaptedPortingPreparation!.PreparedScene
                    :
                    throw new InvalidOperationException(
                        _adaptedPortingPreparationIssue ??
                        "Адаптация весов не подготовлена.");
                geometryMode = SkinnedGeometryTransferMode.PreservePreparedGeometry;
            }
            else
            {
                skinnedScene = _replacementScene!;
                geometryMode = RebaseBindPoseCheckBox.IsChecked == true
                    ? SkinnedGeometryTransferMode.RetargetToGameBindPose
                    : SkinnedGeometryTransferMode.PreservePreparedGeometry;
            }
            SkinnedRenderableMaterialProfile materialProfile =
                ResolveSkinnedMaterialProfile(skinnedScene);
            SmoWingAttachmentDraft? wingAttachments = UsesGeneratedWeightsPortingMode && WizardProtectionCommitIsCurrent
                ? _wizardProtectionWingAttachments : null;
            saveOperation = cancellationToken =>
                SmoSkinnedGlbReplacer.Replace(
                    targetDocument,
                    skinnedScene,
                    ReplacementTransform.Identity,
                    outputPath,
                    geometryMode,
                    materialProfile: materialProfile,
                    cancellationToken: cancellationToken,
                    wingAttachments: wingAttachments).OutputPath;
        }
        else
        {
            throw new InvalidOperationException(
                "Для модели не выбран поддерживаемый режим записи.");
        }
        return saveOperation;
    }
}
