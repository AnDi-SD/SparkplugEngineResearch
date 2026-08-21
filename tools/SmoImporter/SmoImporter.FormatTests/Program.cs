using System.Security.Cryptography;
using System.Numerics;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;

if (args.Length == 3 &&
    args[0] == "--generated-skinning-degenerate-fbx-regression")
{
 DegenerateFbxGeneratedSkinningRegression.Run(args[1], args[2]);
 return 0;
}

if (args.Length == 2 &&
    args[0] == "--generated-topology-normalization-regression")
{
 GeneratedTopologyNormalizationRegression.Run(args[1]);
 return 0;
}

if (args.Length == 2 && args[0] == "--target-fitting-preview-regression")
{
 TargetRigFittingPreviewRegression.Run(args[1]);
 return 0;
}

if (args.Length == 5 &&
    args[0] is "--layla-alpha-candidate" or
        "--layla-face-overlay-candidate" or
        "--layla-face-overlay-regression")
{
 bool useFaceOverlay = args[0] != "--layla-alpha-candidate";
 bool regressionOnly = args[0] == "--layla-face-overlay-regression";
 string targetPath = Path.GetFullPath(args[1]);
 string donorPath = Path.GetFullPath(args[2]);
 float alignmentScale = float.Parse(
  args[3], System.Globalization.CultureInfo.InvariantCulture);
 string outputPath = Path.GetFullPath(args[4]);
 EnsureSeparateTestOutput(targetPath, donorPath, outputPath);
 if (File.Exists(outputPath))
  throw new InvalidOperationException(
   "Layla alpha candidate output already exists; choose a new test-output path.");
 byte[] targetBefore = File.ReadAllBytes(targetPath);
 byte[] donorBefore = File.ReadAllBytes(donorPath);
 SmoDocument target = SmoDocument.Load(targetPath);
 TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
 SmoExportScene targetScene = SmoSceneBuilder.Build(
  target,
  new SmoExportOptions(
   ApplyWorldTransforms: true,
   AnimationPaths: null,
   Resources: SmoExportResourceTypes.Meshes | SmoExportResourceTypes.Skeleton));
 ImportedScene donor = ImportedModelReader.ReadGeometryOnly(donorPath);
 ImportedTexture[] external = ReadDonorDirectoryTextures(donorPath, donor);
 if (external.Length > 0)
  donor = ImportedTextureCatalog.ResolveExternalOverrides(
   donor, Array.AsReadOnly(external)).EffectiveScene;
 var alignment = new ReplacementTransform(
  alignmentScale,
  Vector3.Zero,
  Vector3.Zero);
 TargetRigAutomaticPoseFitResult fit = TargetRigAutomaticPoseFitter.Fit(
  rig,
  targetScene,
  donor,
  alignment);
 GeneratedSkinningPreparationResult automatic = GeneratedSkinningPreparer.Prepare(
 target,
 donor,
  fit.Pose,
  alignment,
  fit.BodySelection);
 GeneratedSkinningComponentOverrides overrides =
  AlphaBranchRegression.CreateLaylaManualOverrides(automatic);
 GeneratedSkinningPreparationResult prepared = GeneratedSkinningPreparer.Prepare(
  target,
  donor,
  fit.Pose,
  alignment,
  fit.BodySelection,
 overrides);
 SkinnedRenderableMaterialProfile? materialProfile = useFaceOverlay
  ? AlphaBranchRegression.CreateLaylaFaceOverlayMaterialProfile(
   prepared.PreparedScene)
  : null;
 if (regressionOnly)
 {
  AlphaBranchRegression.RunFaceOverlay(
   target,
   prepared.PreparedScene,
   outputPath,
   "Layla face-overlay API regression");
  if (File.Exists(outputPath) ||
      !File.ReadAllBytes(targetPath).SequenceEqual(targetBefore) ||
      !File.ReadAllBytes(donorPath).SequenceEqual(donorBefore))
   throw new InvalidOperationException(
    "Layla face-overlay regression left output or modified a source file.");
  Console.WriteLine("LAYLA FACE OVERLAY API REGRESSION PASS");
  return 0;
 }
 GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
  target,
  prepared.PreparedScene,
  ReplacementTransform.Identity,
  outputPath,
  SkinnedGeometryTransferMode.PreservePreparedGeometry,
  texture: null,
  textureMode: SkinnedTextureTransferMode.ImportDonor,
  materialProfile: materialProfile);
 if (useFaceOverlay)
  AlphaBranchRegression.VerifySavedFaceOverlayCandidate(
   target,
   prepared.PreparedScene,
   outputPath,
   "saved Layla face-overlay game candidate");
 else
  AlphaBranchRegression.VerifySavedCandidate(
   target,
   prepared.PreparedScene,
   outputPath,
   "saved Layla alpha game candidate");
 if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBefore) ||
     !File.ReadAllBytes(donorPath).SequenceEqual(donorBefore))
 throw new InvalidOperationException(
   "Layla candidate writer modified a source file.");
 Console.WriteLine(
  $"LAYLA {(useFaceOverlay ? "FACE OVERLAY" : "ALPHA")} CANDIDATE: " +
  $"{result.OutputPath}; vertices={result.VertexCount}; " +
  $"triangles={result.TriangleCount}; palettes={result.PaletteCount}; " +
  $"manualOverrides={overrides.Components.Count}; bytes={result.FileSize}; " +
  $"sha256={result.Sha256}");
 return 0;
}

if (args.Length == 4 && args[0] == "--target-rig-layla-equivalence")
{
 string targetPath = Path.GetFullPath(args[1]);
 string objPath = Path.GetFullPath(args[2]);
 string fbxPath = Path.GetFullPath(args[3]);
 byte[] targetBefore = File.ReadAllBytes(targetPath);
 byte[] objBefore = File.ReadAllBytes(objPath);
 byte[] fbxBefore = File.ReadAllBytes(fbxPath);
 SmoDocument document = SmoDocument.Load(targetPath);
 TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(document);
 SmoExportScene scene = SmoSceneBuilder.Build(
  document,
  new SmoExportOptions(
   ApplyWorldTransforms: true,
   AnimationPaths: null,
   Resources: SmoExportResourceTypes.Meshes | SmoExportResourceTypes.Skeleton));
 TargetRigAutomaticPoseFitResult obj = TargetRigAutomaticPoseFitter.Fit(
  rig,
  scene,
  ImportedModelReader.ReadGeometryOnly(objPath),
  new ReplacementTransform(10, Vector3.Zero, Vector3.Zero));
 TargetRigAutomaticPoseFitResult fbx = TargetRigAutomaticPoseFitter.Fit(
  rig,
  scene,
  ImportedModelReader.ReadGeometryOnly(fbxPath),
  new ReplacementTransform(1000, Vector3.Zero, Vector3.Zero));
 float[] objParameters =
 [
  obj.Parameters.ArmElevationDegrees,
  obj.Parameters.ArmForwardDegrees,
  obj.Parameters.ElbowBendDegrees,
  obj.Parameters.LegSpreadDegrees,
  obj.Parameters.KneeBendDegrees,
  obj.Parameters.TorsoPitchDegrees,
  obj.Parameters.NeckForward
 ];
 float[] fbxParameters =
 [
  fbx.Parameters.ArmElevationDegrees,
  fbx.Parameters.ArmForwardDegrees,
  fbx.Parameters.ElbowBendDegrees,
  fbx.Parameters.LegSpreadDegrees,
  fbx.Parameters.KneeBendDegrees,
  fbx.Parameters.TorsoPitchDegrees,
  fbx.Parameters.NeckForward
 ];
 if (objParameters.Zip(fbxParameters).Any(pair =>
      MathF.Abs(pair.First - pair.Second) > 0.001f) ||
     MathF.Abs(obj.ScoreBefore - fbx.ScoreBefore) > 0.00001f ||
     MathF.Abs(obj.ScoreAfter - fbx.ScoreAfter) > 0.00001f ||
     obj.BodySelection.TotalComponentCount != fbx.BodySelection.TotalComponentCount ||
     !obj.BodySelection.Components.Select(component => component.Role)
      .SequenceEqual(fbx.BodySelection.Components.Select(component => component.Role)) ||
     obj.Pose.WorldMatrices.Zip(fbx.Pose.WorldMatrices).Any(pair =>
      !MatrixApproximatelyEqual(pair.First, pair.Second, 0.0001f)))
  throw new InvalidOperationException(
   "Layla OBJ and FBX units did not produce an equivalent automatic target-rig fit.");
 if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBefore) ||
     !File.ReadAllBytes(objPath).SequenceEqual(objBefore) ||
     !File.ReadAllBytes(fbxPath).SequenceEqual(fbxBefore))
  throw new InvalidOperationException(
   "Layla OBJ/FBX equivalence fit modified an input file.");
 Console.WriteLine(
  $"TARGET RIG LAYLA EQUIVALENCE PASS: " +
  $"OBJ score={obj.ScoreBefore:G9}->{obj.ScoreAfter:G9}; " +
  $"FBX score={fbx.ScoreBefore:G9}->{fbx.ScoreAfter:G9}; " +
  $"maxParameterDelta={objParameters.Zip(fbxParameters).Max(pair => MathF.Abs(pair.First - pair.Second)):G9}");
 return 0;
}

if (args.Length == 4 && args[0] == "--target-rig-auto-fit")
{
 string targetPath = Path.GetFullPath(args[1]);
 string donorPath = Path.GetFullPath(args[2]);
 float alignmentScale = float.Parse(
  args[3], System.Globalization.CultureInfo.InvariantCulture);
 byte[] targetBefore = File.ReadAllBytes(targetPath);
 byte[] donorFileBefore = File.ReadAllBytes(donorPath);
 SmoDocument targetDocument = SmoDocument.Load(targetPath);
 TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(targetDocument);
 SmoExportScene targetScene = SmoSceneBuilder.Build(
  targetDocument,
  new SmoExportOptions(
   ApplyWorldTransforms: true,
   AnimationPaths: null,
   Resources: SmoExportResourceTypes.Meshes | SmoExportResourceTypes.Skeleton));
 ImportedScene donor = ImportedModelReader.ReadGeometryOnly(donorPath);
 ImportedTexture[] externalDonorTextures = ReadDonorDirectoryTextures(
  donorPath, donor);
 if (externalDonorTextures.Length > 0)
  donor = ImportedTextureCatalog.ResolveExternalOverrides(
   donor, Array.AsReadOnly(externalDonorTextures)).EffectiveScene;
 Dictionary<string, byte[]> externalDonorTextureFilesBefore = externalDonorTextures
  .Where(texture => !string.IsNullOrWhiteSpace(texture.SourcePath))
  .ToDictionary(
   texture => texture.SourcePath!,
   texture => File.ReadAllBytes(texture.SourcePath!),
   StringComparer.OrdinalIgnoreCase);
 Vector3[][] donorPositionsBefore = donor.Meshes
  .Select(mesh => mesh.Positions.ToArray()).ToArray();
 Vector3[][] donorNormalsBefore = donor.Meshes
  .Select(mesh => mesh.Normals.ToArray()).ToArray();
 Vector2[][] donorUvsBefore = donor.Meshes
  .Select(mesh => mesh.TextureCoordinates.ToArray()).ToArray();
 uint[][] donorIndicesBefore = donor.Meshes
  .Select(mesh => mesh.TriangleIndices.ToArray()).ToArray();
 uint[][] donorColorsBefore = donor.Meshes
  .Select(mesh => mesh.DiffuseColors.ToArray()).ToArray();
 byte[][] donorTexturesBefore = donor.Textures
  .Select(texture => texture.Data.ToArray()).ToArray();

 TargetRigFittingPoseSnapshot neutral = TargetRigBodyPoseMapper.CreateSnapshot(
  rig, TargetRigBodyPoseParameters.Neutral);
 Vector3 NeutralAt(string name)
 {
  Matrix4x4 matrix = neutral.WorldMatrices[rig.GetJointIndex(name)];
  return new Vector3(matrix.M41, matrix.M42, matrix.M43);
 }
 if (MathF.Abs(NeutralAt("L_Hand").Y - NeutralAt("L_Bicep").Y) > 0.001f ||
     MathF.Abs(NeutralAt("R_Hand").Y - NeutralAt("R_Bicep").Y) > 0.001f)
  throw new InvalidOperationException(
   "High-level neutral mapper did not create horizontal symmetric arms.");
 if (MathF.Abs(NeutralAt("L_Ankle").X - NeutralAt("L_Thigh").X) > 0.001f ||
     MathF.Abs(NeutralAt("R_Ankle").X - NeutralAt("R_Thigh").X) > 0.001f)
  throw new InvalidOperationException(
   "High-level neutral mapper did not create vertical symmetric legs.");
 VerifyTargetRigPoseLengths(rig, neutral, "high-level neutral mapper");

 var manualParameters = new TargetRigBodyPoseParameters(
  ArmElevationDegrees: 20,
  ArmForwardDegrees: 10,
  ElbowBendDegrees: 30,
  LegSpreadDegrees: 10,
 KneeBendDegrees: 20,
  TorsoPitchDegrees: 5,
  NeckForward: 12);
 TargetRigFittingPoseSnapshot manual = TargetRigBodyPoseMapper.CreateSnapshot(
  rig, manualParameters);
 VerifyTargetRigPoseLengths(rig, manual, "high-level nonzero mapper");
 Vector3 ManualAt(string name)
 {
  Matrix4x4 matrix = manual.WorldMatrices[rig.GetJointIndex(name)];
  return new Vector3(matrix.M41, matrix.M42, matrix.M43);
 }
 Vector3 leftArmDirection = Vector3.Normalize(
  ManualAt("L_Hand") - ManualAt("L_Bicep"));
 Vector3 leftLegDirection = Vector3.Normalize(
  ManualAt("L_Ankle") - ManualAt("L_Thigh"));
 float armElevation = MathF.Asin(leftArmDirection.Y) * 180 / MathF.PI;
 float armForward = MathF.Atan2(leftArmDirection.Z, leftArmDirection.X) * 180 / MathF.PI;
 float legSpread = MathF.Atan2(leftLegDirection.X, -leftLegDirection.Y) * 180 / MathF.PI;
 float elbowBend = MathF.Acos(Math.Clamp(Vector3.Dot(
  Vector3.Normalize(ManualAt("L_UpperArm") - ManualAt("L_Bicep")),
  Vector3.Normalize(ManualAt("L_Hand") - ManualAt("L_UpperArm"))), -1, 1)) * 180 / MathF.PI;
 float kneeBend = MathF.Acos(Math.Clamp(Vector3.Dot(
  Vector3.Normalize(ManualAt("L_calf") - ManualAt("L_Thigh")),
  Vector3.Normalize(ManualAt("L_Ankle") - ManualAt("L_calf"))), -1, 1)) * 180 / MathF.PI;
 Vector3 torsoDirection = Vector3.Normalize(
  ManualAt("Spine_02") - ManualAt("Spine_01"));
 float torsoPitch = MathF.Atan2(torsoDirection.Z, torsoDirection.Y) * 180 / MathF.PI;
 TargetRigFittingPoseSnapshot inheritedNeck = TargetRigBodyPoseMapper.CreateSnapshot(
  rig, manualParameters with { NeckForward = 0 });
 Vector3 inheritedNeckDirection = Vector3.Normalize(
  TranslationAt(inheritedNeck, rig, "Head") -
  TranslationAt(inheritedNeck, rig, "Neck"));
 Vector3 manualNeckDirection = Vector3.Normalize(
  ManualAt("Head") - ManualAt("Neck"));
 float neckForward = MathF.Atan2(
  Vector3.Dot(Vector3.Cross(inheritedNeckDirection, manualNeckDirection), Vector3.UnitX),
  Vector3.Dot(inheritedNeckDirection, manualNeckDirection)) * 180 / MathF.PI;
 if (MathF.Abs(armElevation - manualParameters.ArmElevationDegrees) > 0.02f ||
     MathF.Abs(armForward - manualParameters.ArmForwardDegrees) > 0.02f ||
     MathF.Abs(elbowBend - manualParameters.ElbowBendDegrees) > 0.02f ||
     MathF.Abs(legSpread - manualParameters.LegSpreadDegrees) > 0.02f ||
     MathF.Abs(kneeBend - manualParameters.KneeBendDegrees) > 0.02f ||
     MathF.Abs(torsoPitch - manualParameters.TorsoPitchDegrees) > 0.02f ||
     MathF.Abs(neckForward - manualParameters.NeckForward) > 0.02f)
  throw new InvalidOperationException(
   "High-level nonzero controls did not reproduce their absolute endpoint directions/bends.");

 var neckIsolationParameters = TargetRigBodyPoseParameters.Neutral with
 {
  TorsoPitchDegrees = 7.5f
 };
 TargetRigFittingPoseSnapshot neckIsolationBaseline =
  TargetRigBodyPoseMapper.CreateSnapshot(rig, neckIsolationParameters);
 TargetRigFittingPoseSnapshot neckForwardPositive =
  TargetRigBodyPoseMapper.CreateSnapshot(
   rig, neckIsolationParameters with { NeckForward = 18 });
 TargetRigFittingPoseSnapshot neckForwardNegative =
  TargetRigBodyPoseMapper.CreateSnapshot(
   rig, neckIsolationParameters with { NeckForward = -18 });
 Vector3 baselineHeadPosition = TranslationAt(
  neckIsolationBaseline, rig, "Head");
 Vector3 positiveHeadPosition = TranslationAt(
  neckForwardPositive, rig, "Head");
 Vector3 negativeHeadPosition = TranslationAt(
  neckForwardNegative, rig, "Head");
 if (positiveHeadPosition.Z <= baselineHeadPosition.Z + 0.001f ||
     negativeHeadPosition.Z >= baselineHeadPosition.Z - 0.001f)
 {
  throw new InvalidOperationException(
   $"Signed NeckForward did not move the Head joint forward/back: " +
   $"baseline Z={baselineHeadPosition.Z:G9}; " +
   $"positive Z={positiveHeadPosition.Z:G9}; " +
   $"negative Z={negativeHeadPosition.Z:G9}.");
 }
 Quaternion baselineHeadRotation = WorldRotationAt(
  neckIsolationBaseline, rig, "Head");
 foreach ((string label, TargetRigFittingPoseSnapshot posed) in new[]
          {
           ("positive", neckForwardPositive),
           ("negative", neckForwardNegative)
          })
 {
  Quaternion posedHeadRotation = WorldRotationAt(posed, rig, "Head");
  float rotationDot = MathF.Abs(Quaternion.Dot(
   baselineHeadRotation, posedHeadRotation));
  if (rotationDot < 0.999999f)
  {
   throw new InvalidOperationException(
    $"{label} NeckForward changed Head world orientation: " +
    $"quaternion dot={rotationDot:G9}.");
  }
  VerifyTargetRigPoseLengths(
   rig, posed, $"{label} counter-rotated NeckForward");
 }

 TargetRigFittingPoseSnapshot elbowDirectionPose =
  TargetRigBodyPoseMapper.CreateSnapshot(
   rig,
   TargetRigBodyPoseParameters.Neutral with
   {
    ElbowBendDegrees = 45,
    KneeBendDegrees = 45
   });
 foreach ((string root, string middle, string end) in new[]
          {
           ("L_Bicep", "L_UpperArm", "L_Hand"),
           ("R_Bicep", "R_UpperArm", "R_Hand")
          })
 {
  Vector3 rootPoint = TranslationAt(elbowDirectionPose, rig, root);
  Vector3 middlePoint = TranslationAt(elbowDirectionPose, rig, middle);
  Vector3 endPoint = TranslationAt(elbowDirectionPose, rig, end);
  Vector3 rootToEnd = endPoint - rootPoint;
  Vector3 middleOffset = middlePoint - rootPoint -
   rootToEnd * (Vector3.Dot(middlePoint - rootPoint, rootToEnd) /
                rootToEnd.LengthSquared());
  if (middleOffset.Z >= -0.001f)
   throw new InvalidOperationException(
    $"High-level elbow bend put {middle} on the forward side; " +
    $"expected backward/-Z, offset Z={middleOffset.Z:G9}.");
 }
 foreach ((string root, string middle, string end) in new[]
          {
           ("L_Thigh", "L_calf", "L_Ankle"),
           ("R_Thigh", "R_calf", "R_Ankle")
          })
 {
  Vector3 rootPoint = TranslationAt(elbowDirectionPose, rig, root);
  Vector3 middlePoint = TranslationAt(elbowDirectionPose, rig, middle);
  Vector3 endPoint = TranslationAt(elbowDirectionPose, rig, end);
  Vector3 rootToEnd = endPoint - rootPoint;
  Vector3 middleOffset = middlePoint - rootPoint -
   rootToEnd * (Vector3.Dot(middlePoint - rootPoint, rootToEnd) /
                rootToEnd.LengthSquared());
  if (middleOffset.Z <= 0.001f)
   throw new InvalidOperationException(
    $"High-level knee bend put {middle} on the backward side; " +
    $"expected forward/+Z, offset Z={middleOffset.Z:G9}.");
 }

 var legacySixArgumentParameters = new TargetRigBodyPoseParameters(
  20, 10, 30, 10, 20, 5);
 TargetRigFittingPoseSnapshot legacySixArgumentPose =
  TargetRigBodyPoseMapper.CreateSnapshot(rig, legacySixArgumentParameters);
 TargetRigFittingPoseSnapshot explicitNeutralNeckPose =
  TargetRigBodyPoseMapper.CreateSnapshot(
   rig, legacySixArgumentParameters with { NeckForward = 0 });
 if (!legacySixArgumentPose.WorldMatrices.SequenceEqual(
      explicitNeutralNeckPose.WorldMatrices))
  throw new InvalidOperationException(
   "Optional neutral neck control broke six-argument body-pose compatibility.");

 TargetRigFittingPose manuallyCorrectedPose =
  TargetRigBodyPoseMapper.CreatePose(rig, manualParameters);
 int correctedJoint = rig.GetJointIndex("L_UpperArm");
 Quaternion manualCorrection = Quaternion.CreateFromAxisAngle(
  Vector3.UnitY, 17 * MathF.PI / 180);
 manuallyCorrectedPose.SetLocalRotationDelta(
  correctedJoint,
  Quaternion.Normalize(
   manualCorrection * manual.LocalRotationDeltas[correctedJoint]));
 int correctedHeadJoint = rig.GetJointIndex("Head");
 Quaternion manualHeadCorrection = Quaternion.CreateFromAxisAngle(
  Vector3.UnitY, -11 * MathF.PI / 180);
 manuallyCorrectedPose.SetLocalRotationDelta(
  correctedHeadJoint,
  Quaternion.Normalize(
   manualHeadCorrection * manual.LocalRotationDeltas[correctedHeadJoint]));
 Quaternion manualRootRotation = Quaternion.CreateFromAxisAngle(
  Vector3.UnitY, 3 * MathF.PI / 180);
 Vector3 manualRootTranslation = new(1.25f, -0.5f, 2.75f);
 manuallyCorrectedPose.SetRootTransform(
  manualRootRotation, manualRootTranslation);
 TargetRigFittingPoseSnapshot effectiveManualPose =
  manuallyCorrectedPose.Capture();
 var changedHumanParameters = manualParameters with
 {
  ArmElevationDegrees = 35,
  ElbowBendDegrees = 55,
  NeckForward = -8
 };
 TargetRigFittingPoseSnapshot rebasedHumanPose =
  TargetRigBodyPoseMapper.RebasePreservingCorrections(
   rig,
   effectiveManualPose,
   manualParameters,
   changedHumanParameters);
 TargetRigFittingPoseSnapshot changedHumanPose =
  TargetRigBodyPoseMapper.CreateSnapshot(rig, changedHumanParameters);
 for (int jointIndex = 0; jointIndex < rig.Joints.Count; jointIndex++)
 {
  Quaternion correction = Quaternion.Normalize(
   effectiveManualPose.LocalRotationDeltas[jointIndex] *
   Quaternion.Inverse(manual.LocalRotationDeltas[jointIndex]));
  Quaternion expected = Quaternion.Normalize(
   correction * changedHumanPose.LocalRotationDeltas[jointIndex]);
  if (MathF.Abs(Quaternion.Dot(
       expected,
       rebasedHumanPose.LocalRotationDeltas[jointIndex])) < 0.999999f)
  {
   throw new InvalidOperationException(
    $"Human/joint mode rebase lost the manual correction of " +
    $"{rig.Joints[jointIndex].Name}.");
  }
 }
 if (MathF.Abs(Quaternion.Dot(
      manualRootRotation,
      rebasedHumanPose.RootRotation)) < 0.999999f ||
     Vector3.Distance(
      manualRootTranslation,
      rebasedHumanPose.RootTranslation) > 0.000001f)
  throw new InvalidOperationException(
   "Human/joint mode rebase did not preserve the root transform.");
 VerifyTargetRigPoseLengths(
  rig, rebasedHumanPose, "human/joint shared-pose rebase");
 TargetRigFittingPoseSnapshot unchangedHumanPose =
  TargetRigBodyPoseMapper.RebasePreservingCorrections(
   rig,
   effectiveManualPose,
   manualParameters,
   manualParameters);
 if (!ReferenceEquals(unchangedHumanPose, effectiveManualPose))
  throw new InvalidOperationException(
   "Switching pose-editor modes without changing controls rebuilt the pose.");

 Vector3[] editorEulerCases =
 [
  new(-180, -180, -180),
  new(-135, 70, -160),
  new(-90, 45, 20),
  new(-89.9f, -165, 30),
  new(-89.2f, 140, -25),
  new(-89.99f, -125, 80),
  new(-45, 90, 135),
  Vector3.Zero,
  new(45, -90, -135),
  new(89.99f, 125, -80),
  new(89.9f, 165, -30),
  new(89.2f, -140, 25),
  new(90, 45, 20),
  new(135, -70, 160),
  new(180, 180, 180)
 ];
 foreach (Vector3 editorEuler in editorEulerCases)
 {
  Quaternion before = TargetRigEulerAngles.ToQuaternion(editorEuler);
  Vector3 displayed = TargetRigEulerAngles.FromQuaternion(before);
  Quaternion after = TargetRigEulerAngles.ToQuaternion(displayed);
  if (MathF.Abs(Quaternion.Dot(before, after)) < 0.99999f)
  {
   throw new InvalidOperationException(
    $"Joint-editor Euler round-trip changed the rotation: " +
    $"input={editorEuler}; displayed={displayed}; " +
    $"dot={MathF.Abs(Quaternion.Dot(before, after)):G9}.");
  }
 }

 bool rejectedOutOfRange = false;
 try
 {
  _ = TargetRigBodyPoseMapper.CreateSnapshot(
   rig, manualParameters with { ElbowBendDegrees = 180 });
 }
 catch (ArgumentOutOfRangeException)
 {
  rejectedOutOfRange = true;
 }
 if (!rejectedOutOfRange)
  throw new InvalidOperationException("High-level mapper accepted an unsafe angle range.");
 bool rejectedNeckOutOfRange = false;
 try
 {
  _ = TargetRigBodyPoseMapper.CreateSnapshot(
   rig, manualParameters with { NeckForward = -46 });
 }
 catch (ArgumentOutOfRangeException)
 {
  rejectedNeckOutOfRange = true;
 }
 if (!rejectedNeckOutOfRange)
  throw new InvalidOperationException(
   "High-level mapper accepted an unsafe neck-forward range.");

 var alignment = new ReplacementTransform(
  alignmentScale, Vector3.Zero, Vector3.Zero);
 // Manual fitting must be a complete workflow of its own. Select the same
 // continuous body surfaces and prepare both neutral and user-authored poses
 // before any automatic pose optimization is invoked.
 TargetRigBodySelection manuallySelectedBody =
  TargetRigAutomaticPoseFitter.SelectBody(rig, donor, alignment);
 GeneratedSkinningPreparationResult neutralWithoutAutomaticFit =
  GeneratedSkinningPreparer.Prepare(
   targetDocument,
   donor,
   neutral,
   alignment,
   manuallySelectedBody);
 ValidateGeneratedPreparation(
  neutralWithoutAutomaticFit,
  rig,
  "Layla neutral manual fitting without automatic pose fit");
 GeneratedSkinningPreparationResult manualWithoutAutomaticFit =
  GeneratedSkinningPreparer.Prepare(
   targetDocument,
   donor,
   manual,
   alignment,
   manuallySelectedBody);
 ValidateGeneratedPreparation(
  manualWithoutAutomaticFit,
  rig,
  "Layla authored manual fitting without automatic pose fit");
 if (!HasGeometryDifference(
      manualWithoutAutomaticFit.FittingPreviewScene,
      manualWithoutAutomaticFit.PreparedScene))
 {
  throw new InvalidOperationException(
   "Manual body pose selected without automatic fitting was not baked back " +
   "to the canonical target bind pose.");
 }

 TargetRigAutomaticPoseFitResult fit = TargetRigAutomaticPoseFitter.Fit(
  rig, targetScene, donor, alignment);
 if (!SameBodySelectionMembership(
      manuallySelectedBody,
      fit.BodySelection))
 {
  throw new InvalidOperationException(
   "SelectBody and automatic Fit selected different donor component IDs or " +
   "original vertex membership for the same target, donor, and alignment.");
 }
 TargetRigAutomaticPoseFitResult repeatedFit = TargetRigAutomaticPoseFitter.Fit(
  rig, targetScene, donor, alignment);
 if (fit.Parameters != repeatedFit.Parameters ||
     fit.ScoreBefore != repeatedFit.ScoreBefore ||
     fit.ScoreAfter != repeatedFit.ScoreAfter ||
     !fit.Pose.WorldMatrices.SequenceEqual(repeatedFit.Pose.WorldMatrices) ||
     !fit.BodySelection.Components.Select(component => component.ComponentIndex)
      .SequenceEqual(repeatedFit.BodySelection.Components.Select(
       component => component.ComponentIndex)))
  throw new InvalidOperationException(
   "Automatic body fit is not bitwise deterministic for identical inputs.");
 ImportedMesh[] renamedMeshes = donor.Meshes
  .Select((mesh, index) => mesh with { Name = $"unlabeled_surface_{index}" })
  .ToArray();
 var renamedDonor = new ImportedScene(
  Array.AsReadOnly(renamedMeshes), donor.Textures, donor.Materials);
 TargetRigAutomaticPoseFitResult renamedFit = TargetRigAutomaticPoseFitter.Fit(
  rig, targetScene, renamedDonor, alignment);
 if (fit.Parameters != renamedFit.Parameters ||
     fit.ScoreBefore != renamedFit.ScoreBefore ||
     fit.ScoreAfter != renamedFit.ScoreAfter ||
     !fit.Pose.WorldMatrices.SequenceEqual(renamedFit.Pose.WorldMatrices))
  throw new InvalidOperationException(
   "Automatic body selection depends on donor mesh/material names instead of geometry.");
 VerifyTargetRigPoseLengths(rig, fit.Pose, "automatic body fit");
 if (!(fit.ScoreAfter < fit.ScoreBefore * 0.25f))
  throw new InvalidOperationException(
   $"Automatic body fit did not improve enough: {fit.ScoreBefore} -> {fit.ScoreAfter}.");
 if (fit.BodySelection.Components.Count != 2 ||
     fit.BodySelection.Components.Select(component => component.Role).ToHashSet()
      .SetEquals(new[]
      {
       TargetRigBodyComponentRole.LowerBody,
       TargetRigBodyComponentRole.TorsoAndArms
      }) == false ||
     fit.BodySelection.ExcludedComponentCount < 1)
  throw new InvalidOperationException(
   "Automatic body fit did not select separate lower/upper body surfaces " +
   "while excluding disconnected attachments.");
 if (fit.BodySelection.Components.Any(component =>
      component.VerticesByMesh.Count == 0 ||
      component.VerticesByMesh.Any(membership =>
       membership.VertexIndices.Count == 0 ||
       membership.VertexIndices.Any(vertex =>
        (uint)vertex >= (uint)donor.Meshes[membership.MeshIndex].Positions.Length))))
  throw new InvalidOperationException(
   "Automatic body selection does not expose valid original donor membership.");

 Vector3 PosedAt(string name)
 {
  Matrix4x4 matrix = fit.Pose.WorldMatrices[rig.GetJointIndex(name)];
  return new Vector3(matrix.M41, matrix.M42, matrix.M43);
 }
 Vector3 BindAt(string name)
 {
  Matrix4x4 matrix = rig.Joints[rig.GetJointIndex(name)].BindWorldMatrix;
  return new Vector3(matrix.M41, matrix.M42, matrix.M43);
 }
 float bindHandY = 0.5f * (BindAt("L_Hand").Y + BindAt("R_Hand").Y);
 float posedHandY = 0.5f * (PosedAt("L_Hand").Y + PosedAt("R_Hand").Y);
 float bindAnkleX = 0.5f * (
  MathF.Abs(BindAt("L_Ankle").X) + MathF.Abs(BindAt("R_Ankle").X));
 float posedAnkleX = 0.5f * (
  MathF.Abs(PosedAt("L_Ankle").X) + MathF.Abs(PosedAt("R_Ankle").X));
 if (posedHandY <= bindHandY + 20)
  throw new InvalidOperationException(
   $"Automatic body fit did not raise the hands: {bindHandY} -> {posedHandY}.");
 if (posedAnkleX >= bindAnkleX - 5)
  throw new InvalidOperationException(
   $"Automatic body fit did not narrow the legs: {bindAnkleX} -> {posedAnkleX}.");
 if (MathF.Abs(PosedAt("L_Hand").X + PosedAt("R_Hand").X) > 0.01f ||
     MathF.Abs(PosedAt("L_Hand").Y - PosedAt("R_Hand").Y) > 0.01f ||
     MathF.Abs(PosedAt("L_Ankle").X + PosedAt("R_Ankle").X) > 0.01f ||
     MathF.Abs(PosedAt("L_Ankle").Y - PosedAt("R_Ankle").Y) > 0.01f)
  throw new InvalidOperationException(
   "Automatic body fit did not preserve bilateral pose symmetry.");

 Vector3[][] targetScenePositionsBefore = targetScene.Meshes
  .Select(mesh => mesh.Positions.ToArray()).ToArray();
 TargetRigFittingPreviewResult targetPreview =
  TargetRigFittingPreviewBuilder.Build(targetScene, fit.Pose);
 if (targetPreview.IsIdentityPose ||
     targetPreview.SkinnedMeshCount <= 0 ||
     targetPreview.SkinnedVertexCount <= 0 ||
     !targetScene.Meshes.Zip(targetPreview.Scene.Meshes).Any(pair =>
      pair.First.SkinObjectIndex is not null &&
      pair.First.Positions.Zip(pair.Second.Positions).Any(vertexPair =>
       Vector3.DistanceSquared(vertexPair.First, vertexPair.Second) > 1e-10f)))
 {
  throw new InvalidOperationException(
   "Automatic fitting pose did not deform the gray target preview model.");
 }

 bool legacySelectionRejected = false;
 try
 {
  _ = GeneratedSkinningPreparer.Prepare(
   targetDocument,
   donor,
   fit.Pose,
   alignment);
 }
 catch (InvalidDataException exception) when (
  exception.Message.Contains(
   "no dominant connected surface", StringComparison.Ordinal))
 {
  legacySelectionRejected = true;
 }
 if (!legacySelectionRejected)
  throw new InvalidOperationException(
   "Legacy generated-skinning overload no longer preserves its strict " +
   "single-dominant-surface behavior for Layla.");

 GeneratedSkinningPreparationResult prepared =
  GeneratedSkinningPreparer.Prepare(
   targetDocument,
   donor,
   fit.Pose,
   alignment,
   fit.BodySelection);
 ValidateGeneratedPreparation(prepared, rig, "Layla selected mode3");
 VerifyExplicitGeneratedAlignment(
  donor,
  prepared.FittingPreviewScene,
  alignment,
  "Layla selected mode3");
 if (!HasGeometryDifference(
      prepared.FittingPreviewScene,
      prepared.PreparedScene))
  throw new InvalidOperationException(
   "Layla automatic fitting pose was not baked back to the canonical target bind pose.");

 int selectedVertexCount = fit.BodySelection.Components.Sum(component =>
  component.VerticesByMesh.Sum(membership => membership.VertexIndices.Count));
 int selectedTriangleCount = fit.BodySelection.Components.Sum(component =>
  component.TriangleCount);
 if (prepared.Analysis.DonorMainComponentVertexCount != selectedVertexCount ||
     prepared.Analysis.DonorMainComponentTriangleCount != selectedTriangleCount ||
     prepared.Analysis.Attachments.Count != fit.BodySelection.ExcludedComponentCount ||
     prepared.Analysis.Attachments.Sum(attachment => attachment.VertexCount) !=
      donor.Meshes.Sum(mesh => mesh.Positions.Length) - selectedVertexCount)
 {
  throw new InvalidOperationException(
   "Layla selected-body analysis does not match exact component membership.");
 }

 foreach (TargetRigSelectedBodyComponent component in fit.BodySelection.Components)
 {
  bool hasSmoothWeights = component.VerticesByMesh.Any(membership =>
   membership.VertexIndices.Any(vertexIndex =>
   {
    Vector4 weights = prepared.FittingPreviewScene.Meshes[membership.MeshIndex]
     .Skinning!.Weights[vertexIndex];
    return new[] { weights.X, weights.Y, weights.Z, weights.W }
     .Count(weight => weight > 0.000001f) > 1;
   }));
  if (!hasSmoothWeights)
   throw new InvalidOperationException(
    $"Selected Layla body component #{component.ComponentIndex} did not receive " +
    "smooth generated weights.");
 }

 HashSet<(int MeshIndex, int VertexIndex)> selectedVertices = fit.BodySelection.Components
  .SelectMany(component => component.VerticesByMesh)
  .SelectMany(membership => membership.VertexIndices.Select(vertexIndex =>
   (membership.MeshIndex, vertexIndex)))
  .ToHashSet();
 GeneratedAnatomyRegressionMetrics anatomyMetrics =
  GeneratedSkinningRegression.VerifyAnatomicalVolumes(
   prepared,
   rig,
   fit.Pose,
   fit.BodySelection);
 Console.WriteLine(
  $"  Layla anatomy metrics: chest n={anatomyMetrics.ChestVertexCount}, " +
  $"central mean={anatomyMetrics.MeanChestCentralMass:G9}, " +
  $"dominant={anatomyMetrics.ChestCentralDominantRatio:P3}; " +
  $"head n={anatomyMetrics.HeadVertexCount}, " +
  $"Head mean={anatomyMetrics.MeanHeadMass:G9}, " +
  $"dominant={anatomyMetrics.HeadDominantRatio:P3}; " +
  $"limbs n={anatomyMetrics.LimbVertexCount}, " +
  $"central-dominant={anatomyMetrics.LimbCentralDominantRatio:P3}; " +
  $"anatomical={prepared.Analysis.AnatomicalVolumeAffectedVertexCount}, " +
  $"legacy={prepared.Analysis.AnatomicalVolumeLegacyVertexCount}.");
 foreach (string warning in prepared.Analysis.Warnings.Where(message =>
           message.Contains("anatomical field", StringComparison.Ordinal) ||
           message.Contains("shifted torso field", StringComparison.Ordinal) ||
           message.Contains("Head ellipsoid", StringComparison.Ordinal)))
  Console.WriteLine("  " + warning);
 int maximumActiveInfluenceCount = prepared.FittingPreviewScene.Meshes
  .SelectMany(mesh => mesh.Skinning!.Weights)
  .Select(weights => new[] { weights.X, weights.Y, weights.Z, weights.W }
   .Count(weight => weight > 0.000001f))
  .Max();
 GeneratedSkinningAnalysis generatedAnalysis = prepared.Analysis;
 float[] approximationMetrics =
 [
  generatedAnalysis.MaximumDiscardedTopFourWeightMass,
  generatedAnalysis.MeanDiscardedTopFourWeightMass,
  generatedAnalysis.MaximumTopFourToFinalWeightL1Distance,
  generatedAnalysis.MeanTopFourToFinalWeightL1Distance
 ];
 if (generatedAnalysis.MaximumInfluencesPerVertex != 3 ||
     maximumActiveInfluenceCount > generatedAnalysis.MaximumInfluencesPerVertex ||
     approximationMetrics.Any(metric => !float.IsFinite(metric) || metric < 0) ||
     generatedAnalysis.MeanDiscardedTopFourWeightMass >
      generatedAnalysis.MaximumDiscardedTopFourWeightMass + 0.000001f ||
     generatedAnalysis.MeanTopFourToFinalWeightL1Distance >
      generatedAnalysis.MaximumTopFourToFinalWeightL1Distance + 0.000001f ||
     MathF.Abs(generatedAnalysis.MaximumTopFourToFinalWeightL1Distance -
               2f * generatedAnalysis.MaximumDiscardedTopFourWeightMass) > 0.00001f ||
     MathF.Abs(generatedAnalysis.MeanTopFourToFinalWeightL1Distance -
               2f * generatedAnalysis.MeanDiscardedTopFourWeightMass) > 0.00001f ||
     generatedAnalysis.FittingPoseComparisonVertexCount != selectedVertexCount ||
     !float.IsFinite(
      generatedAnalysis.MaximumTopFourToFinalFittingPositionDelta) ||
     !float.IsFinite(
      generatedAnalysis.RmsTopFourToFinalFittingPositionDelta) ||
     generatedAnalysis.MaximumTopFourToFinalFittingPositionDelta < 0 ||
     generatedAnalysis.RmsTopFourToFinalFittingPositionDelta < 0 ||
     generatedAnalysis.RmsTopFourToFinalFittingPositionDelta >
      generatedAnalysis.MaximumTopFourToFinalFittingPositionDelta + 0.000001f ||
     !generatedAnalysis.Warnings.Any(message => message.Contains(
      "at most 3 active influences", StringComparison.Ordinal)) ||
     !generatedAnalysis.Warnings.Any(message => message.Contains(
      "selected 3 as the highest compatible", StringComparison.Ordinal)))
 {
  throw new InvalidOperationException(
   "Layla generated-skinning palette-limit diagnostics are inconsistent with " +
   "the actual finite, normalized top-four-to-top-three weights.");
 }
 for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
 {
  ImportedSkinning skinning = prepared.FittingPreviewScene.Meshes[meshIndex].Skinning!;
  for (int vertexIndex = 0;
       vertexIndex < donor.Meshes[meshIndex].Positions.Length;
       vertexIndex++)
  {
   if (selectedVertices.Contains((meshIndex, vertexIndex)))
    continue;
   Vector4 weights = skinning.Weights[vertexIndex];
   if (weights != Vector4.UnitX)
   {
    throw new InvalidOperationException(
     $"Excluded Layla vertex [{meshIndex}:{vertexIndex}] was not kept as a " +
     "rigid attachment.");
   }
  }
 }
 int wingAttachmentCount = prepared.Analysis.Attachments.Count(attachment =>
  attachment.MeshIndices.Contains(4));
 if (donor.Meshes.Count <= 4 || wingAttachmentCount <= 0)
  throw new InvalidOperationException(
   "Layla wing surfaces were not retained on the rigid-attachment path.");

 GeneratedSkinningComponentOverrides componentOverrides =
  AlphaBranchRegression.CreateLaylaManualOverrides(prepared);
 GeneratedSkinningPreparationResult manuallyOverridden =
  GeneratedSkinningPreparer.Prepare(
   targetDocument,
   donor,
   fit.Pose,
   alignment,
   fit.BodySelection,
   componentOverrides);
 ValidateGeneratedPreparation(
  manuallyOverridden, rig, "Layla manual component overrides");
 GeneratedSkinningRegression.VerifyManualAssignments(
  prepared,
  manuallyOverridden,
  componentOverrides,
  "Layla manual component overrides");

 GeneratedSkinningPreparationResult manualPoseBaseline =
  GeneratedSkinningPreparer.Prepare(
   targetDocument,
   donor,
   manual,
   alignment,
   fit.BodySelection);
 GeneratedSkinningPreparationResult manualPoseOverridden =
  GeneratedSkinningPreparer.Prepare(
   targetDocument,
   donor,
   manual,
   alignment,
   fit.BodySelection,
   componentOverrides);
 GeneratedSkinningRegression.VerifyManualAssignments(
  manualPoseBaseline,
  manualPoseOverridden,
  componentOverrides,
  "Layla overrides after pose-mode change");

 var alternateAlignment = new ReplacementTransform(
  alignmentScale * 1.01f,
  Vector3.Zero,
  new Vector3(0.25f, -0.15f, 0.10f));
 TargetRigAutomaticPoseFitResult alternateFit =
  TargetRigAutomaticPoseFitter.Fit(
   rig,
   targetScene,
   donor,
   alternateAlignment);
 GeneratedSkinningPreparationResult alternateAlignmentBaseline =
  GeneratedSkinningPreparer.Prepare(
   targetDocument,
   donor,
   alternateFit.Pose,
   alternateAlignment,
   alternateFit.BodySelection);
 GeneratedSkinningPreparationResult alternateAlignmentOverridden =
  GeneratedSkinningPreparer.Prepare(
   targetDocument,
   donor,
   alternateFit.Pose,
   alternateAlignment,
   alternateFit.BodySelection,
   componentOverrides);
 GeneratedSkinningRegression.VerifyManualAssignments(
  alternateAlignmentBaseline,
  alternateAlignmentOverridden,
  componentOverrides,
  "Layla overrides after scale/translation change");

 void ExpectOverridesRejected(
  GeneratedSkinningComponentOverrides candidate,
  string context)
 {
  bool rejected = false;
  try
  {
   _ = GeneratedSkinningPreparer.Prepare(
    targetDocument,
    donor,
    fit.Pose,
    alignment,
    fit.BodySelection,
    candidate);
  }
  catch (InvalidDataException)
  {
   rejected = true;
  }
  if (!rejected)
   throw new InvalidOperationException(
    $"Manual component assignments accepted stale or invalid {context} identity.");
 }

 ExpectOverridesRejected(
  componentOverrides with { TargetRigFingerprint = "stale-target-rig" },
  "target-rig");
 ExpectOverridesRejected(
  componentOverrides with { DonorGeometryFingerprint = "stale-donor" },
  "donor");
 ExpectOverridesRejected(
  componentOverrides with
  {
   TotalComponentCount = componentOverrides.TotalComponentCount + 1
  },
  "component-count");
 GeneratedSkinningComponentOverride firstOverride = componentOverrides.Components[0];
 ExpectOverridesRejected(
  componentOverrides with
  {
   Components = Array.AsReadOnly(
    componentOverrides.Components.Append(firstOverride).ToArray())
  },
  "duplicate-component");
 TargetRigSelectedBodyComponent selectedBodyComponent =
  fit.BodySelection.Components[0];
 ExpectOverridesRejected(
  componentOverrides with
  {
   Components = Array.AsReadOnly(
    componentOverrides.Components.Append(
     new GeneratedSkinningComponentOverride(
      selectedBodyComponent.ComponentIndex,
      GeneratedSkinningComponentAttachmentTarget.Head,
      selectedBodyComponent.VerticesByMesh)).ToArray())
  },
  "smooth-body-component");
 TargetRigBodyVertexMembership firstOverrideMembership =
  firstOverride.VerticesByMesh[0];
 TargetRigBodyVertexMembership incompleteOverrideMembership =
  firstOverrideMembership with
  {
   VertexIndices = Array.AsReadOnly(
    firstOverrideMembership.VertexIndices.Skip(1).ToArray())
  };
 ExpectOverridesRejected(
  componentOverrides with
  {
   Components = Array.AsReadOnly(
    new[]
    {
     firstOverride with
     {
      VerticesByMesh = Array.AsReadOnly(
       new[] { incompleteOverrideMembership }
        .Concat(firstOverride.VerticesByMesh.Skip(1))
        .ToArray())
     }
    }.Concat(componentOverrides.Components.Skip(1)).ToArray())
  },
  "vertex-membership");
 ExpectOverridesRejected(
  componentOverrides with
  {
   Components = Array.AsReadOnly(
    new[]
    {
     firstOverride with
     {
      Target = (GeneratedSkinningComponentAttachmentTarget)int.MaxValue
     }
    }.Concat(componentOverrides.Components.Skip(1)).ToArray())
  },
  "semantic-target");

 void ExpectSelectionRejected(
  ImportedScene candidateDonor,
  ReplacementTransform candidateAlignment,
  TargetRigBodySelection candidateSelection,
  string context)
 {
  bool rejected = false;
  try
  {
   _ = GeneratedSkinningPreparer.Prepare(
    targetDocument,
    candidateDonor,
    fit.Pose,
    candidateAlignment,
    candidateSelection);
  }
  catch (InvalidDataException)
  {
   rejected = true;
  }
  if (!rejected)
   throw new InvalidOperationException(
    $"Explicit Layla body selection accepted stale {context} identity.");
 }

 ExpectSelectionRejected(
  donor,
  alignment with { Translation = new Vector3(0.01f, 0, 0) },
  fit.BodySelection,
  "alignment");
 ExpectSelectionRejected(
  renamedDonor,
  alignment,
  fit.BodySelection,
  "donor");
 ExpectSelectionRejected(
  donor,
  alignment,
  fit.BodySelection with { TargetRigFingerprint = "stale-target-rig" },
  "target-rig");
 TargetRigSelectedBodyComponent originalComponent = fit.BodySelection.Components[0];
 TargetRigSelectedBodyComponent invalidBoundsComponent = originalComponent with
 {
  AlignedMaximum = originalComponent.AlignedMaximum + Vector3.UnitX
 };
 ExpectSelectionRejected(
  donor,
  alignment,
  fit.BodySelection with
  {
   Components = Array.AsReadOnly(
    new[] { invalidBoundsComponent }
     .Concat(fit.BodySelection.Components.Skip(1))
     .ToArray())
  },
  "component-bounds");
 TargetRigBodyVertexMembership originalMembership =
  originalComponent.VerticesByMesh[0];
 TargetRigBodyVertexMembership duplicateMembership = originalMembership with
 {
  VertexIndices = Array.AsReadOnly(
   originalMembership.VertexIndices
    .Append(originalMembership.VertexIndices[0])
    .ToArray())
 };
 TargetRigSelectedBodyComponent invalidMembershipComponent = originalComponent with
 {
  VerticesByMesh = Array.AsReadOnly(
   new[] { duplicateMembership }
    .Concat(originalComponent.VerticesByMesh.Skip(1))
    .ToArray())
 };
 ExpectSelectionRejected(
  donor,
  alignment,
  fit.BodySelection with
  {
   Components = Array.AsReadOnly(
    new[] { invalidMembershipComponent }
     .Concat(fit.BodySelection.Components.Skip(1))
     .ToArray())
  },
  "vertex-membership");

 string transientOutputPath = Path.Combine(
  Path.GetTempPath(),
  $"smo-layla-auto-fit-{Guid.NewGuid():N}.smo");
 try
 {
  GlbSkinTransferPlan preservePlan = SmoSkinnedGlbReplacer.Analyze(
   targetDocument,
   manuallyOverridden.PreparedScene,
   SkinnedTextureTransferMode.PreserveTarget);
  if (!preservePlan.CanReplace)
   throw new InvalidOperationException(
    "Layla selected-body PreserveTarget writer is blocked: " +
    string.Join(" | ", preservePlan.Messages));
  _ = SmoSkinnedGlbReplacer.Replace(
   targetDocument,
   manuallyOverridden.PreparedScene,
   ReplacementTransform.Identity,
   transientOutputPath,
   SkinnedGeometryTransferMode.PreservePreparedGeometry,
   texture: null,
   textureMode: SkinnedTextureTransferMode.PreserveTarget);
  VerifySkinnedWriterTargetGraph(targetDocument, transientOutputPath);
  SmoDocument written = SmoDocument.Load(transientOutputPath);
  VerifyTextureDataObjectsByteIdentical(
   targetDocument, written, "Layla selected mode3 PreserveTarget");
  VerifyInlinePaletteEntriesPreserved(
   targetDocument, written, "Layla selected mode3 PreserveTarget");
 }
 finally
 {
  if (File.Exists(transientOutputPath))
   File.Delete(transientOutputPath);
 }
 AlphaBranchRegression.Run(
  targetDocument,
  manuallyOverridden.PreparedScene,
  transientOutputPath,
  "Layla selected mode3");
 AlphaBranchRegression.RunFaceOverlay(
  targetDocument,
  manuallyOverridden.PreparedScene,
  transientOutputPath,
  "Layla selected mode3 explicit face overlay");

 if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBefore) ||
     !File.ReadAllBytes(donorPath).SequenceEqual(donorFileBefore) ||
     externalDonorTextureFilesBefore.Any(pair =>
      !File.ReadAllBytes(pair.Key).SequenceEqual(pair.Value)) ||
     targetScene.Meshes.Where((mesh, index) =>
      !mesh.Positions.SequenceEqual(targetScenePositionsBefore[index])).Any() ||
     donor.Meshes.Where((mesh, index) =>
      !mesh.Positions.SequenceEqual(donorPositionsBefore[index]) ||
      !mesh.Normals.SequenceEqual(donorNormalsBefore[index]) ||
      !mesh.TextureCoordinates.SequenceEqual(donorUvsBefore[index]) ||
      !mesh.TriangleIndices.SequenceEqual(donorIndicesBefore[index]) ||
      !mesh.DiffuseColors.SequenceEqual(donorColorsBefore[index])).Any() ||
     donor.Textures.Where((texture, index) =>
      !texture.Data.SequenceEqual(donorTexturesBefore[index])).Any())
  throw new InvalidOperationException(
   "Automatic body fit mutated its target or donor input.");

 Console.WriteLine(
  $"TARGET RIG AUTO FIT PASS: donor={Path.GetFileName(donorPath)}; " +
  $"components={fit.BodySelection.Components.Count}/{fit.BodySelection.TotalComponentCount}; " +
   $"attachments={prepared.Analysis.Attachments.Count}; wings={wingAttachmentCount}; " +
   $"manualOverrides={componentOverrides.Components.Count}; " +
   $"chest={anatomyMetrics.MeanChestCentralMass:P3}/" +
   $"{anatomyMetrics.ChestCentralDominantRatio:P3}; " +
   $"head={anatomyMetrics.MeanHeadMass:P3}/" +
   $"{anatomyMetrics.HeadDominantRatio:P3}; " +
   $"limbCentral={anatomyMetrics.LimbCentralDominantRatio:P3}; " +
   "writer=PreserveTarget verified; mixed-alpha ImportDonor split verified; " +
  $"influences<={maximumActiveInfluenceCount}; " +
  $"top4-discarded=max {generatedAnalysis.MaximumDiscardedTopFourWeightMass:G9}, " +
  $"mean {generatedAnalysis.MeanDiscardedTopFourWeightMass:G9}; " +
  $"top4-L1=max {generatedAnalysis.MaximumTopFourToFinalWeightL1Distance:G9}, " +
  $"mean {generatedAnalysis.MeanTopFourToFinalWeightL1Distance:G9}; " +
  $"fitting-delta=max " +
  $"{generatedAnalysis.MaximumTopFourToFinalFittingPositionDelta:G9}, RMS " +
  $"{generatedAnalysis.RmsTopFourToFinalFittingPositionDelta:G9}; " +
  $"score={fit.ScoreBefore:G9}->{fit.ScoreAfter:G9}; " +
  $"handY={bindHandY:G9}->{posedHandY:G9}; " +
  $"ankle|X|={bindAnkleX:G9}->{posedAnkleX:G9}; " +
  $"parameters={fit.Parameters}");
 return 0;
}

if (args.Length == 1 && args[0] == "--texture-catalog-regression")
{
 RunImportedTextureCatalogBatchRegression();
 Console.WriteLine(
  "TEXTURE CATALOG PASS: exact filename priority, unresolved fallback, " +
  "ambiguous batches and unmatched files verified.");
 return 0;
}

if (args.Length is 2 or 3 && args[0] == "--rigid-texture-bundle")
{
 RigidGlbTextureBundle bundle = RigidGlbTextureBundleReader.ReadModel(
  args[1], args.Length == 3 ? args[2] : null);
 int vertexCount = bundle.Scene.Meshes.Sum(mesh => mesh.Positions.Length);
 int triangleCount = bundle.Scene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
 int frameCount = bundle.MaterialGroups.Sum(group => group.Frames.Count);
 if (bundle.MaterialGroups.SelectMany(group => group.Meshes).Count() !=
     bundle.Scene.Meshes.Count)
  throw new InvalidOperationException("Rigid bundle lost or duplicated a source mesh.");
 if (bundle.MaterialGroups.SelectMany(group => group.Frames).Any(frame =>
      frame.Texture.Width < frame.SourceWidth ||
      frame.Texture.Height < frame.SourceHeight))
  throw new InvalidOperationException("Rigid bundle downscaled a texture.");
 Console.WriteLine(
  $"RIGID BUNDLE: groups={bundle.MaterialGroups.Count}; frames={frameCount}; " +
 $"meshes={bundle.Scene.Meshes.Count}; vertices={vertexCount}; triangles={triangleCount}");
 if (bundle.IgnoredMeshes.Count > 0)
  Console.WriteLine("  ignored model meshes: " + string.Join(", ", bundle.IgnoredMeshes));
 if (bundle.IgnoredTextureFiles.Count > 0)
  Console.WriteLine("  ignored PNG files: " + string.Join(", ", bundle.IgnoredTextureFiles));
 foreach (RigidMaterialGroup group in bundle.MaterialGroups)
  Console.WriteLine(
   $"  {group.Name}: meshes={group.Meshes.Count}; frames={group.Frames.Count}; " +
   string.Join(", ", group.Frames.Select(frame =>
    $"{Path.GetFileName(frame.SourcePath)}={frame.Texture.Width}x{frame.Texture.Height}" +
    (frame.WasUpscaled ? " (upscaled)" : " (exact)"))));
 return 0;
}

if (args.Length == 2 && args[0] == "--rigid-texture-resize-regression")
{
 string modelPath = Path.GetFullPath(args[1]);
 RigidGlbTextureBundle sourceBundle = RigidGlbTextureBundleReader.ReadModel(modelPath);
 RigidMaterialGroup firstGroup = sourceBundle.MaterialGroups[0];
 string temporary = Path.Combine(
  Path.GetTempPath(), "smo-rigid-texture-resize-" + Guid.NewGuid().ToString("N"));
 Directory.CreateDirectory(temporary);
 try
 {
  foreach (RigidTextureFrame frame in sourceBundle.MaterialGroups.SelectMany(group => group.Frames))
   File.Copy(frame.SourcePath, Path.Combine(temporary, Path.GetFileName(frame.SourcePath)));

  string resizedSource = Path.Combine(
   temporary, Path.GetFileName(firstGroup.BaseFrame.SourcePath));
  using (var image = new Image<Rgba32>(3, 5))
  {
   image[0, 0] = new Rgba32(17, 91, 203, 0);
   image[2, 4] = new Rgba32(211, 37, 9, 255);
   image.SaveAsPng(resizedSource);
  }

  RigidGlbTextureBundle resized = RigidGlbTextureBundleReader.ReadModel(
   modelPath, temporary);
  RigidTextureFrame normalized = resized.MaterialGroups
   .Single(group => group.MaterialNumber == firstGroup.MaterialNumber).BaseFrame;
  if (!normalized.WasUpscaled || normalized.SourceWidth != 3 ||
      normalized.SourceHeight != 5 || normalized.Texture.Width != 4 ||
      normalized.Texture.Height != 8)
   throw new InvalidOperationException("Non-POT texture was not enlarged from 3x5 to 4x8.");
  using Image<Rgba32> decoded = Image.Load<Rgba32>(normalized.Texture.Data);
  if (decoded[0, 0] != new Rgba32(17, 91, 203, 0) ||
      decoded[3, 7] != new Rgba32(211, 37, 9, 255))
   throw new InvalidOperationException(
    "Alpha-aware POT enlargement did not preserve clamped corner pixels.");

  bool refusedDownscale = false;
  try
  {
   _ = RigidGlbTextureBundleReader.ReadModel(
    modelPath, temporary, maximumTextureDimension: 4);
  }
  catch (InvalidDataException exception) when (
   exception.Message.Contains("Downscaling is forbidden", StringComparison.Ordinal))
  {
   refusedDownscale = true;
  }
  if (!refusedDownscale)
   throw new InvalidOperationException("Texture reader did not reject a required downscale.");
  Console.WriteLine(
   "RIGID TEXTURE RESIZE PASS: 3x5 -> 4x8; hidden RGB/alpha corners preserved; downscale rejected.");
 }
 finally
 {
  if (Directory.Exists(temporary))
   Directory.Delete(temporary, recursive: true);
 }
 return 0;
}

if (args.Length == 2 && args[0] == "--scan-texture-metadata")
{
 string[] files = Directory.Exists(args[1])
  ? Directory.EnumerateFiles(args[1], "*.smo", SearchOption.AllDirectories).ToArray()
  : new[] { args[1] };
 foreach (string file in files)
 {
  SmoDocument document = SmoDocument.Load(file);
  foreach (SmoObjectEntry entry in document.Objects.Where(item =>
             item.TypeHash == SmoClassIds.TextureData))
  {
   if (!SmoTextureDecoder.TryDecode(document, entry, out SmoTexture? texture, out _) ||
       texture is null || texture.FormatCode is not (0x32E3 or 0x43E3) ||
       texture.Width == texture.Height)
    continue;
   byte[] bytes = document.Data.Span.Slice(
    checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize)).ToArray();
   uint U32(int offset) => System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
    bytes.AsSpan(offset));
   Console.WriteLine(
    $"{Path.GetFileName(file)} [{entry.Index}] {entry.Name} " +
    $"{texture.Width}x{texture.Height}: " +
    $"2C={U32(0x2C):X8} 30={U32(0x30):X8} 34={U32(0x34):X8} 38={U32(0x38):X8}");
  }
 }
 return 0;
}

if (args.Length == 2 && args[0] == "--dump-texture-sequences")
{
 const uint sequenceClass = 0x16FB0E47;
 SmoDocument document = SmoDocument.Load(args[1]);
 Dictionary<uint, SmoObjectEntry> byId = document.Objects
  .GroupBy(entry => entry.Id).Where(group => group.Count() == 1)
  .ToDictionary(group => group.Key, group => group.Single());
 foreach (SmoObjectEntry entry in document.Objects.Where(item =>
             item.TypeHash == sequenceClass))
 {
  byte[] bytes = document.Data.Span.Slice(
   checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize)).ToArray();
  if (!SmoDataBlockReader.TryReadHeader(bytes, 8, out SmoDataBlockHeader field) ||
      field.FieldType != 0 || field.PayloadSize < 4)
   throw new InvalidDataException($"Sequence [{entry.Index}] has no field0 payload.");
  uint count = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
   bytes.AsSpan(field.PayloadOffset));
  int cursor = checked(field.PayloadOffset + 4 + (int)count * 4);
  Console.WriteLine($"SEQUENCE [{entry.Index}] {entry.Name}: keys={count}");
  for (int index = 0; index < count; index++)
  {
   float time = BitConverter.Int32BitsToSingle(
    System.Buffers.Binary.BinaryPrimitives.ReadInt32LittleEndian(
     bytes.AsSpan(field.PayloadOffset + 4 + index * 4)));
   uint id = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(cursor));
   uint size = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(cursor + 4));
   Console.WriteLine($"  {index:D2}: t={time:G9} id={id} " +
    $"name={(byId.TryGetValue(id, out SmoObjectEntry? texture) ? texture.Name : "?")} inline=0x{size:X}");
   cursor = checked(cursor + 8 + (int)size);
  }
  if (cursor != field.PayloadEnd)
   throw new InvalidDataException(
    $"Sequence [{entry.Index}] leaves {field.PayloadEnd - cursor} payload bytes.");
 }
 return 0;
}

if (args.Length == 2 && args[0] == "--scan-object-references")
{
 SmoDocument document = SmoDocument.Load(args[1]);
 Dictionary<uint, SmoObjectEntry> byId = document.Objects
  .GroupBy(entry => entry.Id).Where(group => group.Count() == 1)
  .ToDictionary(group => group.Key, group => group.Single());
 foreach (SmoObjectEntry owner in document.Objects)
 {
  ReadOnlySpan<byte> bytes = document.Data.Span.Slice(
   checked((int)owner.PhysicalOffset), checked((int)owner.SerializedSize));
  for (int offset = 8; offset <= bytes.Length - 8; offset++)
  {
   uint id = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(bytes[offset..]);
   uint inlineSize = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(bytes[(offset + 4)..]);
   if (!byId.TryGetValue(id, out SmoObjectEntry? referenced) ||
       inlineSize != 0 && inlineSize != referenced.SerializedSize)
    continue;
   bool physicalPrefix = owner.PhysicalOffset + offset + 8 == referenced.PhysicalOffset;
   Console.WriteLine(
    $"[{owner.Index}] {owner.Name} +0x{offset:X}: id={id} -> " +
    $"[{referenced.Index}] {referenced.Name}; inline=0x{inlineSize:X}; " +
    $"prefix={physicalPrefix}");
   offset += 7;
  }
 }
 return 0;
}

if (args.Length == 2 && args[0] == "--dump-tree")
{
 SmoDocument document = SmoDocument.Load(args[1]);
 foreach (SmoObjectEntry entry in document.Objects)
  Console.WriteLine(
   $"[{entry.Index,3}] depth={entry.NestingDepth} parent={entry.ParentIndex?.ToString() ?? "-",3} " +
   $"id={entry.Id,3} off=0x{entry.LogicalOffset:X6} size=0x{entry.SerializedSize:X6} " +
   $"{entry.ClassName ?? $"0x{entry.TypeHash:X8}"} {entry.Name}");
 foreach (SmoObjectEntry entry in document.Objects.Where(item => item.TypeHash == SmoClassIds.Skin))
  if (SmoSkinDecoder.TryDecode(document, entry, out SmoSkin? skin, out _) && skin is not null)
  {
   Console.WriteLine($"SKIN [{entry.Index}] " + string.Join(", ", skin.Bones.Select(bone =>
    $"{bone.PaletteIndex}:{document.Objects[bone.NodeObjectIndex].Name}/inline=0x{bone.InlineSerializedSize:X}")));
   SmoObjectEntry? meshEntry = document.Objects.FirstOrDefault(item =>
    item.ParentIndex == entry.Index && item.TypeHash == SmoClassIds.MeshData);
   if (meshEntry is not null)
   {
    SmoMesh mesh = SmoMeshDecoder.Decode(document, meshEntry);
    int[] used = Enumerable.Range(0, mesh.VertexCount).SelectMany(index =>
    {
     System.Numerics.Vector4 weights = mesh.BlendWeights[index];
     SmoBlendIndices indices = mesh.BlendIndices[index];
     return new[] { (weights.X, (int)indices.X), (weights.Y, (int)indices.Y),
      (weights.Z, (int)indices.Z), (weights.W, (int)indices.W) };
    }).Where(item => item.Item1 > 0.000001f).Select(item => item.Item2).Distinct().Order().ToArray();
    Console.WriteLine($"USED [{entry.Index}] " + string.Join(", ", used.Select(slot =>
     $"{slot}:{document.Objects[skin.Bones[slot].NodeObjectIndex].Name}")));
   }
  }
 return 0;
}

if (args.Length == 4 && args[0] == "--rigid-multitexture")
{
 string targetPath = Path.GetFullPath(args[1]);
 string modelPath = Path.GetFullPath(args[2]);
 string outputPath = Path.GetFullPath(args[3]);
 if (string.Equals(targetPath, outputPath, StringComparison.OrdinalIgnoreCase) ||
     string.Equals(modelPath, outputPath, StringComparison.OrdinalIgnoreCase))
  throw new InvalidOperationException("Rigid multi-texture output must be a new file.");
 byte[] targetBefore = File.ReadAllBytes(targetPath);
 byte[] modelBefore = File.ReadAllBytes(modelPath);
 SmoDocument target = SmoDocument.Load(targetPath);
 RigidGlbTextureBundle sourceBundle = RigidGlbTextureBundleReader.ReadModel(modelPath);
 RigidGlbTextureBundle bundle = WithUniformRigidTextureAlpha(
  sourceBundle,
  byte.MaxValue);
 RigidGlbTextureBundle alphaBundle = WithUniformRigidTextureAlpha(
  sourceBundle,
  254);
 const string rigidAlphaSafetyMessage =
  "cannot safely write attached texture frames containing alpha texels (A < 255) " +
  "into generated one-bone spSkin branches";
 SmoRigidMultiMaterialPackAnalysis alphaAnalysis =
  SmoRigidMultiMaterialPacker.Analyze(target, alphaBundle);
 if (alphaAnalysis.CanPack ||
     !alphaAnalysis.Messages.Any(message => message.Contains(
      rigidAlphaSafetyMessage,
      StringComparison.Ordinal)))
  throw new InvalidOperationException(
   "Rigid multi-texture analysis accepted unsupported generated spSkin alpha: " +
   string.Join(" | ", alphaAnalysis.Messages));
 string blockedAlphaOutput = CreateTransientSiblingPath(
  outputPath,
  "rigid-alpha-blocked");
 if (File.Exists(blockedAlphaOutput))
  throw new InvalidOperationException(
   "Rigid alpha safety-test output unexpectedly exists before packing.");
 bool alphaRejected = false;
 bool alphaOutputCreated;
 try
 {
  _ = SmoRigidMultiMaterialPacker.Pack(
   target,
   alphaBundle,
   ReplacementTransform.Identity,
   blockedAlphaOutput);
 }
 catch (NotSupportedException exception) when (
  exception.Message.Contains(rigidAlphaSafetyMessage, StringComparison.Ordinal))
 {
  alphaRejected = true;
 }
 finally
 {
  alphaOutputCreated = File.Exists(blockedAlphaOutput);
  if (alphaOutputCreated)
   File.Delete(blockedAlphaOutput);
 }
 if (!alphaRejected || alphaOutputCreated)
  throw new InvalidOperationException(
   "Rigid alpha safety gate did not reject the writer or created an output file.");
 SmoRigidMultiMaterialPackAnalysis analysis =
  SmoRigidMultiMaterialPacker.Analyze(target, bundle);
 if (!analysis.CanPack)
  throw new InvalidOperationException(
   "Rigid multi-texture analysis failed: " + string.Join(" | ", analysis.Messages));
 if (analysis.MaterialGroupCount != bundle.MaterialGroups.Count ||
     analysis.MeshCount != bundle.MaterialGroups.Sum(group => group.Meshes.Count) ||
     analysis.TextureCount != bundle.MaterialGroups.Sum(group => group.Frames.Count) ||
     analysis.RigidBoneSlot != 8 ||
     !analysis.RigidBoneName.Equals("Head", StringComparison.OrdinalIgnoreCase))
  throw new InvalidOperationException("Rigid multi-texture analysis summary is inconsistent.");
 SmoExportScene targetScene = SmoSceneBuilder.Build(target);
 ReplacementTransform fit = ReplacementTransformFitter.FitByHeightAndCenter(
  targetScene.Meshes.SelectMany(mesh => mesh.Positions),
  bundle.MaterialGroups.SelectMany(group => group.Meshes).SelectMany(mesh => mesh.Positions));
 SmoRigidMultiMaterialPackResult result = SmoRigidMultiMaterialPacker.Pack(
  target, bundle, fit, outputPath);
 if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBefore))
  throw new InvalidOperationException("Rigid multi-texture packing modified the target SMO.");
 if (!File.ReadAllBytes(modelPath).SequenceEqual(modelBefore))
  throw new InvalidOperationException("Rigid multi-texture packing modified the source model.");
 SmoDocument verified = SmoDocument.Load(outputPath);
 if (verified.HasErrors)
  throw new InvalidOperationException("Rigid multi-texture output failed strict parsing.");
 HashSet<uint> targetIds = target.Objects.Select(entry => entry.Id).ToHashSet();
 Dictionary<uint, SmoObjectEntry> verifiedById = verified.Objects.ToDictionary(entry => entry.Id);
 foreach (SmoRigidPackedTexture packed in result.Textures)
 {
  SmoObjectEntry textureEntry = verifiedById[packed.ObjectId];
  if (!SmoTextureDecoder.TryDecode(
       verified, textureEntry, out SmoTexture? decoded, out string textureError) ||
      decoded is null)
   throw new InvalidOperationException(textureError);
  RigidTextureFrame sourceFrame = bundle.MaterialGroups
   .Single(group => group.MaterialNumber == packed.MaterialNumber).Frames
   .Single(frame => frame.FrameNumber == packed.FrameNumber);
  using Image<Rgba32> expectedImage = Image.Load<Rgba32>(sourceFrame.Texture.Data);
  byte[] expectedBgra = new byte[checked(expectedImage.Width * expectedImage.Height * 4)];
  int pixelOffset = 0;
  expectedImage.ProcessPixelRows(accessor =>
  {
   for (int y = 0; y < accessor.Height; y++)
    foreach (Rgba32 pixel in accessor.GetRowSpan(y))
    {
     expectedBgra[pixelOffset++] = pixel.B;
     expectedBgra[pixelOffset++] = pixel.G;
     expectedBgra[pixelOffset++] = pixel.R;
     expectedBgra[pixelOffset++] = pixel.A;
    }
  });
  if (decoded.Width != sourceFrame.Texture.Width ||
      decoded.Height != sourceFrame.Texture.Height ||
      !decoded.Bgra32Pixels.Span.SequenceEqual(expectedBgra))
   throw new InvalidOperationException(
    $"Packed texture {packed.ObjectName} differs from its normalized PNG pixels.");
 }
 SmoObjectEntry[] addedMeshes = verified.Objects.Where(entry =>
  !targetIds.Contains(entry.Id) && entry.TypeHash == SmoClassIds.MeshData).ToArray();
 SmoObjectEntry[] addedSkins = verified.Objects.Where(entry =>
  !targetIds.Contains(entry.Id) && entry.TypeHash == SmoClassIds.Skin).ToArray();
 if (addedMeshes.Length != bundle.MaterialGroups.Count ||
     addedSkins.Length != bundle.MaterialGroups.Count)
  throw new InvalidOperationException("Rigid multi-texture branch count mismatch.");
 foreach (SmoObjectEntry skinEntry in addedSkins)
 {
  if (!SmoSkinDecoder.TryDecode(
       verified, skinEntry, out SmoSkin? skin, out string skinError) || skin is null)
   throw new InvalidOperationException(skinError);
  if (skin.Bones.Count != 16 || skin.Bones.Any(bone => bone.InlineSerializedSize != 0) ||
      !verified.Objects[skin.Bones[8].NodeObjectIndex].Name.Equals(
       "Head", StringComparison.OrdinalIgnoreCase))
   throw new InvalidOperationException(
    $"Generated skin {skinEntry.Name} does not use a reference-only Head palette.");
 }
  IReadOnlyDictionary<int, SmoTextureBinding> bindings =
   SmoTextureBindingResolver.ResolveAll(verified);
  IReadOnlyDictionary<int, uint> blendOperations =
   SmoMaterialRenderState.ResolveAll(verified);
  SmoObjectEntry targetTemplateMaterial = FindLargestSiblingMeshMaterial(target);
  var targetNativeState = ReadNativeMaterialState(
   target, targetTemplateMaterial, "Rigid target template");
  foreach (RigidMaterialGroup group in bundle.MaterialGroups)
  {
  SmoObjectEntry mesh = addedMeshes.Single(entry =>
   entry.Name == $"layla_mat{group.MaterialNumber}_mesh");
  SmoObjectEntry material = verified.Objects.Single(entry =>
   entry.ParentIndex == mesh.ParentIndex &&
   entry.TypeHash == SmoClassIds.MaterialData);
  var nativeState = ReadNativeMaterialState(
   verified, material, $"Generated {group.Name}");
 if (!bindings.TryGetValue(mesh.Index, out SmoTextureBinding? binding) ||
      binding.Texture is null || binding.Issue is not null ||
      (binding.AnimationFrames?.Count ?? 1) != group.Frames.Count)
   throw new InvalidOperationException(
    $"Generated {group.Name} texture/sequence binding is incomplete.");
   if (result.Textures.Any(texture =>
        texture.MaterialNumber == group.MaterialNumber && texture.UsesAlpha))
    throw new InvalidOperationException(
     $"Generated opaque {group.Name} was incorrectly reported as alpha.");
   if (!blendOperations.TryGetValue(mesh.Index, out uint blendOperation))
    throw new InvalidOperationException(
     $"Generated {group.Name} has no resolved FinalBlendOp.");
   if (blendOperation != targetNativeState.FinalBlendOperation ||
       nativeState.FinalBlendOperation != targetNativeState.FinalBlendOperation ||
       !nativeState.MaterialRenderStates.SequenceEqual(
        targetNativeState.MaterialRenderStates) ||
       !nativeState.LayerTextureStates.SequenceEqual(
        targetNativeState.LayerTextureStates))
    throw new InvalidOperationException(
     $"Generated opaque {group.Name} did not preserve the target material state.");
   SmoMesh decodedMesh = SmoMeshDecoder.Decode(verified, mesh);
  if (decodedMesh.Normals.Length != decodedMesh.VertexCount ||
      decodedMesh.Normals.Any(normal =>
       !float.IsFinite(normal.X) || !float.IsFinite(normal.Y) || !float.IsFinite(normal.Z) ||
       normal.LengthSquared() < 0.5f))
   throw new InvalidOperationException(
    $"Generated {group.Name} has missing, invalid or non-unit vertex normals.");
 }
 const uint textureSequenceClassId = 0x16FB0E47;
 foreach (RigidMaterialGroup group in bundle.MaterialGroups.Where(item => item.Frames.Count > 1))
 {
  SmoObjectEntry sequence = verified.Objects.Single(entry =>
   !targetIds.Contains(entry.Id) && entry.TypeHash == textureSequenceClassId &&
   entry.Name == $"layla_mat{group.MaterialNumber}_sequence");
  ReadOnlySpan<byte> sequenceBytes = verified.Data.Span.Slice(
   checked((int)sequence.PhysicalOffset), checked((int)sequence.SerializedSize));
  if (!SmoDataBlockReader.TryReadHeader(
       sequenceBytes, 8, out SmoDataBlockHeader sequenceField) ||
      sequenceField.FieldType != 0 || sequenceField.PayloadSize < 4)
   throw new InvalidOperationException($"Generated {group.Name} sequence is malformed.");

  var expectedSchedule = new List<int>(checked(4 * group.Frames.Count - 2));
  expectedSchedule.AddRange(Enumerable.Repeat(0, 3));
  for (int frame = 1; frame < group.Frames.Count - 1; frame++)
   expectedSchedule.AddRange(Enumerable.Repeat(frame, 2));
  expectedSchedule.AddRange(Enumerable.Repeat(group.Frames.Count - 1, 3));
  for (int frame = group.Frames.Count - 2; frame >= 1; frame--)
   expectedSchedule.AddRange(Enumerable.Repeat(frame, 2));
  uint keyCount = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
   sequenceBytes[sequenceField.PayloadOffset..]);
  if (keyCount != expectedSchedule.Count)
   throw new InvalidOperationException($"Generated {group.Name} sequence key count is wrong.");

  float keyTime = 0;
  float keyStep = BitConverter.Int32BitsToSingle(0x3D088889);
  for (int key = 0; key < expectedSchedule.Count; key++)
  {
   keyTime += keyStep;
   int actualBits = System.Buffers.Binary.BinaryPrimitives.ReadInt32LittleEndian(
    sequenceBytes[(sequenceField.PayloadOffset + 4 + key * 4)..]);
   if (actualBits != BitConverter.SingleToInt32Bits(keyTime))
    throw new InvalidOperationException($"Generated {group.Name} sequence time {key} is wrong.");
  }

  uint[] textureIds = result.Textures
   .Where(texture => texture.MaterialNumber == group.MaterialNumber)
   .OrderBy(texture => texture.FrameNumber)
   .Select(texture => texture.ObjectId)
   .ToArray();
  var inlineDefinitions = new HashSet<int> { 0 };
  int referenceOffset = checked(
   sequenceField.PayloadOffset + 4 + expectedSchedule.Count * sizeof(float));
  for (int key = 0; key < expectedSchedule.Count; key++)
  {
   int frame = expectedSchedule[key];
   uint actualId = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
    sequenceBytes[referenceOffset..]);
   uint actualSize = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
    sequenceBytes[(referenceOffset + 4)..]);
   uint expectedSize = inlineDefinitions.Add(frame)
    ? verifiedById[textureIds[frame]].SerializedSize
    : 0;
   if (actualId != textureIds[frame] || actualSize != expectedSize)
    throw new InvalidOperationException(
     $"Generated {group.Name} sequence reference {key} is wrong or forward-linked.");
   referenceOffset = checked(referenceOffset + 8 + (int)actualSize);
  }
  if (referenceOffset != sequenceField.PayloadEnd)
   throw new InvalidOperationException($"Generated {group.Name} sequence leaves unread bytes.");
 }
 Console.WriteLine(
  $"RIGID MULTITEXTURE OPAQUE PASS + ALPHA BLOCK PASS: " +
  $"groups={result.MaterialGroupCount}; " +
  $"meshes={result.AddedMeshCount}; textures={result.AddedTextureCount}; " +
  $"sequences={result.AddedSequenceCount}; vertices={result.VertexCount}; " +
  $"triangles={result.TriangleCount}; bone=[{result.RigidBoneSlot}] {result.RigidBoneName}; " +
  $"scale={fit.Scale:G9}; move={fit.Translation}; bytes={result.FileSize}; " +
  $"sha256={result.Sha256}; file={result.OutputPath}");
 return 0;
}

if (args.Length == 4 && args[0] == "--smo-to-smo")
{
 if (!File.Exists(args[1]) || !File.Exists(args[2]))
  throw new FileNotFoundException("Target or donor SMO was not found.");
 string targetHash = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[1])));
 string donorHash = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[2])));
 SmoDocument target = SmoDocument.Load(args[1]);
 SmoDocument donor = SmoDocument.Load(args[2]);
 SmoToSmoReplacementPlan plan = SmoToSmoReplacer.Analyze(target, donor);
 if (!plan.CanReplace)
  throw new InvalidOperationException(
   "SMO skeletons are incompatible: " + string.Join(" | ", plan.Messages));
 SmoToSmoReplacementResult result = SmoToSmoReplacer.Replace(target, donor, args[3]);
 if (Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[1]))) != targetHash)
  throw new InvalidOperationException("Target SMO was modified.");
 if (Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[2]))) != donorHash)
  throw new InvalidOperationException("Donor SMO was modified.");
 SmoDocument verified = SmoDocument.Load(args[3]);
 if (verified.HasErrors)
  throw new InvalidOperationException("SMO replacement failed strict verification.");
 if (verified.Objects.Count <= target.Objects.Count)
  throw new InvalidOperationException("No donor visual objects were added to the target graph.");
 Dictionary<uint, SmoObjectEntry> verifiedById = verified.Objects.ToDictionary(entry => entry.Id);
 HashSet<uint> targetIds = target.Objects.Select(entry => entry.Id).ToHashSet();
 for (int index = 0; index < target.Objects.Count; index++)
 {
  SmoObjectEntry before = target.Objects[index];
  if (!verifiedById.TryGetValue(before.Id, out SmoObjectEntry? after))
   throw new InvalidOperationException($"Target object ID {before.Id} disappeared.");
  if (after.Id != before.Id || after.Name != before.Name ||
      after.TypeHash != before.TypeHash)
   throw new InvalidOperationException(
    $"Target object identity [{index}] changed during visual transplant.");
  uint? beforeParentId = before.ParentIndex is int beforeParent
   ? target.Objects[beforeParent].Id
   : null;
  uint? afterParentId = after.ParentIndex is int afterParent
   ? verified.Objects[afterParent].Id
   : null;
  if (beforeParentId != afterParentId)
   throw new InvalidOperationException(
    $"Target object ID {before.Id} changed its catalog parent.");
 }
 SmoObjectEntry[] packedMeshes = verified.Objects.Where(entry =>
  entry.TypeHash == SmoClassIds.MeshData && !targetIds.Contains(entry.Id)).ToArray();
 SmoObjectEntry[] packedTextures = verified.Objects.Where(entry =>
  entry.TypeHash == SmoClassIds.TextureData && !targetIds.Contains(entry.Id)).ToArray();
 if (packedMeshes.Length != donor.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) ||
     packedTextures.Length != donor.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData))
  throw new InvalidOperationException(
   $"Packed branch counts differ: meshes={packedMeshes.Length}, textures={packedTextures.Length}.");
 if (verified.Objects.Any(entry =>
      entry.TypeHash == SmoClassIds.Node && !targetIds.Contains(entry.Id)))
  throw new InvalidOperationException("Donor node objects leaked into the packed visual graph.");
 bool isBloomFaragondaRegression =
  Path.GetFileName(args[1]).Equals("bloom_jeans.smo", StringComparison.OrdinalIgnoreCase) &&
  Path.GetFileName(args[2]).Equals("Faragonda.smo", StringComparison.OrdinalIgnoreCase);
 if (isBloomFaragondaRegression &&
     (verified.Objects.Count != 142 ||
      verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) != 13 ||
      verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.Skin) != 13 ||
      verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MaterialData) != 5 ||
      verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData) != 5 ||
      verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.UvController) != 1 ||
      result.PackedMeshCount != 7 || result.PackedTextureCount != 3 ||
      result.NonDegenerateTriangleCount != 1440))
  throw new InvalidOperationException(
    "Faragonda→Bloom packed graph does not match the 142-object/7-mesh/3-texture regression profile.");

  bool isStellaToBloomRegression =
   Path.GetFileName(args[1]).Equals("bloom_jeans.smo", StringComparison.OrdinalIgnoreCase) &&
   Path.GetFileName(args[2]).Equals("StellaX.smo", StringComparison.OrdinalIgnoreCase);
  bool isBloomToStellaRegression =
   Path.GetFileName(args[1]).Equals("StellaX.smo", StringComparison.OrdinalIgnoreCase) &&
   Path.GetFileName(args[2]).Equals("bloom_jeans.smo", StringComparison.OrdinalIgnoreCase);
  if (isStellaToBloomRegression &&
      (verified.Objects.Count != 158 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) != 14 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.Skin) != 12 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MaterialData) != 7 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData) != 14 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.RenderNode) != 5 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.Model) != 2 ||
       result.PackedMeshCount != 8 || result.PackedTextureCount != 12 ||
       result.NonDegenerateTriangleCount != 1357))
   throw new InvalidOperationException(
    "StellaX→Bloom packed forest does not match the 158-object/8-mesh/12-texture profile.");
  if (isBloomToStellaRegression &&
      (verified.Objects.Count != 141 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) != 14 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.Skin) != 12 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MaterialData) != 7 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData) != 14 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.RenderNode) != 5 ||
       verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.Model) != 2 ||
       result.PackedMeshCount != 6 || result.PackedTextureCount != 2 ||
       result.NonDegenerateTriangleCount != 1880))
   throw new InvalidOperationException(
    "Bloom→StellaX packed forest does not match the 141-object/6-mesh/2-texture profile.");

  if (isStellaToBloomRegression || isBloomToStellaRegression)
  {
   const uint sharedHelperClassId = 0x7AC95AEC;
   if (verified.Objects.Count(entry => entry.TypeHash == sharedHelperClassId) !=
       target.Objects.Count(entry => entry.TypeHash == sharedHelperClassId))
    throw new InvalidOperationException(
     "The byte-identical shared visual helper was copied instead of reusing target identity.");

   (string Root, string Parent)[] expectedForestParents = isStellaToBloomRegression
    ? [("stella_head", "Scene Root"), ("WingR", "Spine_03"), ("WingL", "Spine_03")]
    : [("bloom_eyes", "Head")];
   foreach ((string rootName, string parentName) in expectedForestParents)
   {
    SmoObjectEntry root = verified.Objects.Single(entry =>
     entry.TypeHash == SmoClassIds.RenderNode &&
     !targetIds.Contains(entry.Id) &&
     entry.Name.Equals(rootName, StringComparison.Ordinal));
    if (root.ParentIndex is not int parentIndex ||
        !targetIds.Contains(verified.Objects[parentIndex].Id) ||
        !verified.Objects[parentIndex].Name.Equals(parentName, StringComparison.Ordinal))
     throw new InvalidOperationException(
      $"Packed render {rootName} is not attached to target node {parentName}.");
   }
   if (isStellaToBloomRegression)
   {
    SmoObjectEntry relocatedAtlas = verified.Objects.Single(entry =>
     entry.TypeHash == SmoClassIds.TextureData &&
     !targetIds.Contains(entry.Id) &&
     entry.Name.Equals("stella_x", StringComparison.Ordinal));
    SmoObjectEntry? ancestor = relocatedAtlas;
    bool belongsToWingR = false;
    while (ancestor?.ParentIndex is int parentIndex)
    {
     ancestor = verified.Objects[parentIndex];
     if (ancestor.TypeHash == SmoClassIds.RenderNode &&
         ancestor.Name.Equals("WingR", StringComparison.Ordinal))
     {
      belongsToWingR = true;
      break;
     }
    }
    if (!belongsToWingR)
     throw new InvalidOperationException(
      "The shared stella_x atlas was not relocated inline to the earliest WingR consumer.");
   }
  }
 foreach (string serviceName in new[]
          { "model_root_master", "collision_volume_root", "SubMaster" })
  if (!verified.Objects.Any(entry => entry.Name.Equals(
       serviceName, StringComparison.OrdinalIgnoreCase)))
   throw new InvalidOperationException(
    $"Target service node {serviceName} is absent from the hybrid graph.");

 string outputHash = Convert.ToHexString(SHA256.HashData(verified.Data.Span));
 if (outputHash == targetHash || outputHash == donorHash)
  throw new InvalidOperationException("Output is a copy of an input instead of a visual transplant.");

 int NonDegenerateTriangles(SmoMesh mesh) => Enumerable
  .Range(0, mesh.TriangleIndices.Length / 3)
  .Count(triangle =>
  {
   uint first = mesh.TriangleIndices[triangle * 3];
   uint second = mesh.TriangleIndices[triangle * 3 + 1];
   uint third = mesh.TriangleIndices[triangle * 3 + 2];
   return first != second && second != third && first != third;
  });
 int DonorTriangles(SmoDocument document) => document.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
  .Sum(entry => NonDegenerateTriangles(SmoMeshDecoder.Decode(document, entry)));
 int SerializedTriangles(SmoDocument document) => document.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
  .Sum(entry => SmoMeshDecoder.Decode(document, entry).TriangleCount);
 int Vertices(SmoDocument document) => document.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
  .Sum(entry => SmoMeshDecoder.Decode(document, entry).VertexCount);
 if (DonorTriangles(verified) != DonorTriangles(donor))
  throw new InvalidOperationException("Output triangle payload does not match the donor.");

 string FloatBits(float value) =>
  BitConverter.SingleToInt32Bits(value).ToString("X8");
 string[] GeometryFingerprints(SmoDocument document, bool includeUv) => document.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
  .SelectMany(entry =>
  {
   SmoMesh mesh = SmoMeshDecoder.Decode(document, entry);
   return Enumerable.Range(0, mesh.TriangleIndices.Length / 3)
    .Where(triangle =>
    {
     uint first = mesh.TriangleIndices[triangle * 3];
     uint second = mesh.TriangleIndices[triangle * 3 + 1];
     uint third = mesh.TriangleIndices[triangle * 3 + 2];
     return first != second && second != third && first != third;
    })
    .Select(triangle => string.Join("|", Enumerable.Range(0, 3)
     .Select(corner => checked((int)mesh.TriangleIndices[triangle * 3 + corner]))
     .Select(vertex =>
     {
      System.Numerics.Vector3 position = mesh.Positions[vertex];
      System.Numerics.Vector2 uv = mesh.HasTextureCoordinates
       ? mesh.TextureCoordinates[vertex]
       : System.Numerics.Vector2.Zero;
      uint color = mesh.HasDiffuseColors ? mesh.DiffuseColorsArgb[vertex] : 0xFFFFFFFF;
      string geometry =
       $"{FloatBits(position.X)}:{FloatBits(position.Y)}:{FloatBits(position.Z)}:{color:X8}";
      return includeUv
       ? $"{geometry}:{FloatBits(uv.X)}:{FloatBits(uv.Y)}"
       : geometry;
     })
     .Order(StringComparer.Ordinal)));
  })
  .Order(StringComparer.Ordinal)
  .ToArray();
 if (!GeometryFingerprints(verified, true)
      .SequenceEqual(GeometryFingerprints(donor, true)))
  throw new InvalidOperationException(
   "Output triangle positions, UV0 or diffuse colors do not match the donor.");

 string TextureFingerprint(SmoTexture texture) =>
  $"{texture.Width}x{texture.Height}:0x{texture.FormatCode:X4}:" +
  Convert.ToHexString(SHA256.HashData(texture.Bgra32Pixels.Span));
 SmoTexture[] DecodedTextures(SmoDocument document) => document.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
  .Select(entry => SmoTextureDecoder.TryDecode(document, entry, out SmoTexture? texture, out string error)
   ? texture
   : throw new InvalidDataException(error))
  .ToArray();
 SmoTexture[] donorTextures = DecodedTextures(donor);
 SmoTexture[] outputTextures = DecodedTextures(verified);
 string[] donorTextureFingerprints = donorTextures
  .Select(TextureFingerprint).Order(StringComparer.Ordinal).ToArray();
 string[] outputTextureFingerprints = outputTextures
  .Select(TextureFingerprint).Order(StringComparer.Ordinal).ToArray();
 if (donorTextureFingerprints.Except(outputTextureFingerprints).Any())
  throw new InvalidOperationException(
   "At least one donor texture is absent from the target visual slots.");

 IReadOnlyDictionary<int, SmoTextureBinding> outputBindings =
  SmoTextureBindingResolver.ResolveAll(verified);
 IReadOnlyDictionary<int, SmoTextureBinding> donorBindings =
  SmoTextureBindingResolver.ResolveAll(donor);
 foreach (SmoObjectEntry visibleMeshEntry in verified.Objects.Where(entry =>
           entry.TypeHash == SmoClassIds.MeshData &&
           NonDegenerateTriangles(SmoMeshDecoder.Decode(verified, entry)) > 0))
 {
  if (outputBindings.TryGetValue(
       visibleMeshEntry.Index, out SmoTextureBinding? binding) &&
      binding.Texture is not null && binding.Issue is null)
   continue;
  byte[] visibleBytes = verified.Data.Span.Slice(
   checked((int)visibleMeshEntry.PhysicalOffset),
   checked((int)visibleMeshEntry.SerializedSize)).ToArray();
  SmoObjectEntry? donorMesh = donor.Objects.SingleOrDefault(entry =>
   entry.TypeHash == SmoClassIds.MeshData &&
   entry.Name == visibleMeshEntry.Name &&
   entry.SerializedSize == visibleMeshEntry.SerializedSize &&
   donor.Data.Span.Slice(
    checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize))
    .SequenceEqual(visibleBytes));
  bool donorAlsoHasNoResolvedTexture = donorMesh is not null &&
   (!donorBindings.TryGetValue(donorMesh.Index, out SmoTextureBinding? donorBinding) ||
    donorBinding.Texture is null || donorBinding.Issue is not null);
  if (!donorAlsoHasNoResolvedTexture)
    throw new InvalidOperationException(
     $"Visible output mesh [{visibleMeshEntry.Index}] is not bound to a donor texture.");
 }

 foreach (SmoObjectEntry legacyMeshEntry in verified.Objects.Where(entry =>
           entry.TypeHash == SmoClassIds.MeshData && targetIds.Contains(entry.Id)))
 {
  SmoMesh legacy = SmoMeshDecoder.Decode(verified, legacyMeshEntry);
  if (legacy.StripIndices.Length < 3 ||
      legacy.StripIndices.Any(index => index != legacy.StripIndices[0]) ||
      NonDegenerateTriangles(legacy) != 0)
   throw new InvalidOperationException(
    $"Legacy target mesh ID {legacyMeshEntry.Id} is not strictly degenerate.");
 }

 foreach (SmoObjectEntry meshEntry in verified.Objects.Where(entry =>
           entry.TypeHash == SmoClassIds.MeshData))
 {
  SmoMesh mesh = SmoMeshDecoder.Decode(verified, meshEntry);
  SmoObjectEntry? owner = meshEntry;
  while (owner?.ParentIndex is int parentIndex && owner.TypeHash != SmoClassIds.Skin)
   owner = verified.Objects[parentIndex];
  if (owner?.TypeHash != SmoClassIds.Skin)
  {
   if (mesh.HasSkinningData)
    throw new InvalidDataException(
     $"Skinned mesh [{meshEntry.Index}] has no target skin palette owner.");
   continue;
  }
  if (!SmoSkinDecoder.TryDecode(
       verified, owner, out SmoSkin? skin, out string skinError) || skin is null)
   throw new InvalidDataException(
    $"Mesh [{meshEntry.Index}] has no valid target skin palette: {skinError}");
  if (!mesh.HasSkinningData)
   continue;
  for (int vertex = 0; vertex < mesh.VertexCount; vertex++)
  {
   System.Numerics.Vector4 weights = mesh.BlendWeights[vertex];
   SmoBlendIndices indices = mesh.BlendIndices[vertex];
   (float Weight, byte Index)[] influences =
   [
    (weights.X, indices.X), (weights.Y, indices.Y),
    (weights.Z, indices.Z), (weights.W, indices.W)
   ];
   if (influences.Any(influence =>
        influence.Weight > 0.000001f && influence.Index >= skin.Bones.Count))
    throw new InvalidDataException(
     $"Mesh [{meshEntry.Index}] has an active bone index outside its target palette.");
  }
 }
 Console.WriteLine(
  $"SMO→SMO PASS: compatibility={plan.Compatibility}; " +
  $"donor-meshes={result.DonorMeshCount}; packed-meshes={result.PackedMeshCount}; " +
  $"donor-textures={result.DonorTextureCount}; packed-textures={result.PackedTextureCount}; " +
  $"bones={result.BoneCount}; target-objects={target.Objects.Count}; " +
  $"donor-objects={donor.Objects.Count}; triangles={DonorTriangles(verified)}; " +
  $"donor-serialized-triangles={SerializedTriangles(donor)}; " +
  $"target-serialized-triangles={SerializedTriangles(target)}; " +
  $"donor-vertices={Vertices(donor)}; target-vertices={Vertices(target)}; " +
  $"matched-bones={plan.MatchedBoneNames.Count}; ignored-donor-bones=" +
  $"{plan.IgnoredDonorBones.Count}; unbound-target-bones={plan.UnboundTargetBones.Count}; " +
  $"helper-path-adaptations={plan.HierarchyAdaptations.Count}; " +
  $"SHA-256={result.Sha256}; output={result.OutputPath}");
 foreach (string message in plan.Messages)
  Console.WriteLine("  " + message);
 return 0;
}

if (args.Length == 4 && args[0] == "--porting-mode2")
{
 string targetPath = Path.GetFullPath(args[1]);
 string donorPath = Path.GetFullPath(args[2]);
 string outputPath = Path.GetFullPath(args[3]);
 EnsureSeparateTestOutput(targetPath, donorPath, outputPath);
 byte[] targetBefore = File.ReadAllBytes(targetPath);
 byte[] donorBefore = File.ReadAllBytes(donorPath);
 SmoDocument target = SmoDocument.Load(targetPath);
 ImportedScene donor = GlbModelReader.Read(donorPath);
 string donorDirectory = Path.GetDirectoryName(donorPath) ?? Directory.GetCurrentDirectory();
 ImportedTexture[] adjacentTextures = donor.Materials
  .Select(material => material.BaseColorTextureName)
  .Where(name => !string.IsNullOrWhiteSpace(name))
  .Select(name => Path.Combine(donorDirectory, Path.GetFileName(name!)))
  .Distinct(StringComparer.OrdinalIgnoreCase)
  .Where(File.Exists)
  .Select(ImportedTextureFileReader.Read)
  .ToArray();
 if (adjacentTextures.Length > 0)
 {
  donor = ImportedTextureCatalog.ResolveExternalOverrides(donor, adjacentTextures)
   .EffectiveScene;
 }
 if (donor.Meshes.Count == 0 || donor.Meshes.Any(mesh => mesh.Skinning is null))
  throw new InvalidDataException("Mode-2 test donor must have skinning on every mesh.");

 ModelPortingModeRecommendation recommendation =
  ModelPortingModeAnalyzer.Recommend(target, donor);
 if (recommendation.Mode != ModelPortingMode.AdaptDonorWeights)
  throw new InvalidOperationException(
   $"Auto recommended {recommendation.Mode} instead of AdaptDonorWeights: " +
   recommendation.Reason);
 SkinnedModelPortingAnalysis analysis =
  SkinnedModelPortingPreparer.AnalyzeAdaptDonorWeights(target, donor);
 if (!analysis.CanPrepare || analysis.JointMappings.Count != analysis.ActiveDonorJointCount)
  throw new InvalidOperationException(
   "Mode-2 safety analysis did not map every active donor joint: " +
   string.Join(" | ", analysis.Messages));

 SkinnedModelPortingPreparation automatic =
  SkinnedModelPortingPreparer.PrepareAdaptDonorWeights(target, donor);
 TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
 ValidatePreparedTargetSkin(automatic.PreparedScene, rig, "mode2 automatic");

 string poseJointName = analysis.JointMappings
  .Select(mapping => mapping.TargetJointName)
  .FirstOrDefault(name => name.Equals("L_Bicep", StringComparison.Ordinal)) ??
  analysis.JointMappings
   .Select(mapping => mapping.TargetJointName)
   .First(name => !name.Equals("Pelvis", StringComparison.Ordinal));
 TargetRigFittingPose pose = rig.CreateFittingPose();
 pose.SetLocalRotationDelta(
  poseJointName,
  Quaternion.CreateFromAxisAngle(Vector3.UnitZ, MathF.PI / 90f));
 TargetRigFittingPoseSnapshot snapshot = pose.Capture();
 if (snapshot.IsIdentityPose)
  throw new InvalidOperationException("Mode-2 test pose unexpectedly remained identity.");
 SkinnedModelPortingPreparation posed =
  SkinnedModelPortingPreparer.PrepareAdaptDonorWeights(
   target,
   donor,
   snapshot,
   ReplacementTransform.Identity);
 ValidatePreparedTargetSkin(posed.FittingPreviewScene, rig, "mode2 fitting preview");
 ValidatePreparedTargetSkin(posed.PreparedScene, rig, "mode2 canonical prepared");
 if (!HasGeometryDifference(posed.FittingPreviewScene, posed.PreparedScene))
  throw new InvalidOperationException(
   "Mode-2 local rotation did not produce a distinct canonical bake.");

 VerifyPreserveTargetTextureTransfer(
  target, posed.PreparedScene, outputPath, "mode2");
 VerifyImportDonorExternalCatalogComposition(
  target, posed.PreparedScene, outputPath, "mode2");

 GlbSkinTransferPlan writerPlan = SmoSkinnedGlbReplacer.Analyze(
  target,
  posed.PreparedScene,
  SkinnedTextureTransferMode.ImportDonor);
 if (!writerPlan.CanReplace)
  throw new InvalidOperationException(
   "Mode-2 prepared scene cannot be written: " +
   string.Join(" | ", writerPlan.Messages));
 GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
  target,
  posed.PreparedScene,
  ReplacementTransform.Identity,
  outputPath,
  SkinnedGeometryTransferMode.PreservePreparedGeometry,
  texture: null,
  textureMode: SkinnedTextureTransferMode.ImportDonor);
 VerifySkinnedWriterTargetGraph(target, outputPath);
 VerifyTestSourcesUnchanged(targetPath, targetBefore, donorPath, donorBefore);
 Console.WriteLine(
  $"PORTING MODE 2 PASS: mapped={analysis.JointMappings.Count}; " +
  $"pose-joint={poseJointName}; meshes={result.MeshSlotCount}; " +
  $"vertices={result.VertexCount}; triangles={result.TriangleCount}; " +
  $"palettes={result.PaletteCount}; SHA-256={result.Sha256}; output={result.OutputPath}");
 return 0;
}

if (args.Length == 4 && args[0] == "--porting-mode3")
{
 string targetPath = Path.GetFullPath(args[1]);
 string donorPath = Path.GetFullPath(args[2]);
 string outputPath = Path.GetFullPath(args[3]);
 EnsureSeparateTestOutput(targetPath, donorPath, outputPath);
 byte[] targetBefore = File.ReadAllBytes(targetPath);
 byte[] donorBefore = File.ReadAllBytes(donorPath);
 SmoDocument target = SmoDocument.Load(targetPath);
 ImportedScene donor = ImportedModelReader.ReadGeometryOnly(donorPath);
 if (donor.Meshes.Count == 0 || donor.Meshes.Any(mesh => mesh.Skinning is not null))
  throw new InvalidDataException("Mode-3 test donor must be a non-empty geometry-only scene.");
 Vector3[][] donorPositionsBefore = donor.Meshes
  .Select(mesh => mesh.Positions.ToArray()).ToArray();
 Vector3[][] donorNormalsBefore = donor.Meshes
  .Select(mesh => mesh.Normals.ToArray()).ToArray();
 Vector2[][] donorUvsBefore = donor.Meshes
  .Select(mesh => mesh.TextureCoordinates.ToArray()).ToArray();
 uint[][] donorTrianglesBefore = donor.Meshes
  .Select(mesh => mesh.TriangleIndices.ToArray()).ToArray();
 uint[][] donorColorsBefore = donor.Meshes
  .Select(mesh => mesh.DiffuseColors.ToArray()).ToArray();
 byte[][] donorTexturesBefore = donor.Textures
  .Select(texture => texture.Data.ToArray()).ToArray();
 ModelPortingModeRecommendation recommendation =
  ModelPortingModeAnalyzer.Recommend(target, donor);
 if (recommendation.Mode != ModelPortingMode.GenerateWeights)
  throw new InvalidOperationException(
   $"Auto recommended {recommendation.Mode} instead of GenerateWeights: " +
   recommendation.Reason);

 TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
 GeneratedSkinningPreparationResult legacy =
  GeneratedSkinningPreparer.Prepare(target, donor);
 ValidateGeneratedPreparation(legacy, rig, "mode3 identity legacy");
 TargetRigFittingPose identityPose = rig.CreateFittingPose();
 GeneratedSkinningPreparationResult identity =
  GeneratedSkinningPreparer.Prepare(target, donor, identityPose.Capture());
 ValidateGeneratedPreparation(identity, rig, "mode3 identity snapshot");
 if (!ReferenceEquals(identity.FittingPreviewScene, identity.PreparedScene) ||
     !SceneSkinGeometryExactlyEqual(legacy.PreparedScene, identity.PreparedScene))
  throw new InvalidOperationException(
   "Mode-3 identity snapshot did not reproduce the legacy prepared scene exactly.");

 GeneratedSkinningAlignment automaticAlignment = legacy.Analysis.Alignment;
 var automaticAsExplicit = new ReplacementTransform(
  automaticAlignment.Scale,
  Vector3.Zero,
  automaticAlignment.Translation);
 GeneratedSkinningPreparationResult explicitAutomatic =
  GeneratedSkinningPreparer.Prepare(
   target,
   donor,
   identityPose.Capture(),
   automaticAsExplicit);
 ValidateGeneratedPreparation(
  explicitAutomatic, rig, "mode3 explicit automatic alignment control");
 if (explicitAutomatic.Analysis.Alignment != automaticAlignment ||
     !ReferenceEquals(
      explicitAutomatic.FittingPreviewScene,
      explicitAutomatic.PreparedScene) ||
     !SceneSkinGeometryExactlyEqual(
      legacy.PreparedScene,
      explicitAutomatic.PreparedScene))
 {
  throw new InvalidOperationException(
   "Mode-3 explicit automatic alignment did not reproduce automatic preparation exactly.");
 }

 Vector3[] automaticallyAlignedPositions = legacy.FittingPreviewScene.Meshes
  .SelectMany(mesh => mesh.Positions)
  .ToArray();
 Vector3 alignedMin = automaticallyAlignedPositions.Aggregate(Vector3.Min);
 Vector3 alignedMax = automaticallyAlignedPositions.Aggregate(Vector3.Max);
 Vector3 alignedCenter = (alignedMin + alignedMax) * 0.5f;
 float alignedHeight = alignedMax.Y - alignedMin.Y;
 if (!float.IsFinite(alignedHeight) || alignedHeight <= 0.000001f)
  throw new InvalidOperationException("Mode-3 automatic alignment has no measurable height.");
 const float manualScaleFactor = 1.05f;
 Vector3 manualOffset = new(
  alignedHeight * 0.0125f,
  alignedHeight * -0.0075f,
  alignedHeight * 0.005f);
 var manualAlignment = new ReplacementTransform(
  automaticAlignment.Scale * manualScaleFactor,
  Vector3.Zero,
  automaticAlignment.Translation * manualScaleFactor +
   alignedCenter * (1f - manualScaleFactor) + manualOffset);
 GeneratedSkinningPreparationResult manuallyAlignedIdentity =
  GeneratedSkinningPreparer.Prepare(
   target,
   donor,
   identityPose.Capture(),
   manualAlignment);
 ValidateGeneratedPreparation(
  manuallyAlignedIdentity, rig, "mode3 explicit nonidentity alignment");
 if (manuallyAlignedIdentity.Analysis.Alignment.Scale != manualAlignment.Scale ||
     manuallyAlignedIdentity.Analysis.Alignment.Translation != manualAlignment.Translation)
 {
  throw new InvalidOperationException(
   "Mode-3 analysis does not report the exact explicit alignment.");
 }
 if (!ReferenceEquals(
      manuallyAlignedIdentity.FittingPreviewScene,
      manuallyAlignedIdentity.PreparedScene) ||
     !HasGeometryDifference(
      legacy.PreparedScene,
      manuallyAlignedIdentity.PreparedScene))
 {
  throw new InvalidOperationException(
   "Mode-3 nonidentity alignment was not baked exactly once into the identity preparation.");
 }
 VerifyExplicitGeneratedAlignment(
  donor,
  manuallyAlignedIdentity.FittingPreviewScene,
  manualAlignment,
  "mode3 explicit nonidentity alignment");

 bool rejectedRotation = false;
 try
 {
  _ = GeneratedSkinningPreparer.Prepare(
   target,
   donor,
   identityPose.Capture(),
   manualAlignment with { RotationDegrees = new Vector3(0, 0.01f, 0) });
 }
 catch (ArgumentException)
 {
  rejectedRotation = true;
 }
 if (!rejectedRotation)
  throw new InvalidOperationException("Mode-3 explicit alignment accepted a rotation.");
 ReplacementTransform[] invalidAlignments =
 [
  manualAlignment with { Scale = 0 },
  manualAlignment with { Translation = new Vector3(float.NaN, 0, 0) },
  manualAlignment with { Scale = 1e-30f }
 ];
 foreach (ReplacementTransform invalidAlignment in invalidAlignments)
 {
  bool rejected = false;
  try
  {
   _ = GeneratedSkinningPreparer.Prepare(
    target,
    donor,
    identityPose.Capture(),
    invalidAlignment);
  }
  catch (ArgumentException)
  {
   rejected = true;
  }
  if (!rejected)
  {
   throw new InvalidOperationException(
    "Mode-3 explicit alignment accepted a non-positive, non-finite, or " +
    "non-invertible transform.");
  }
 }

 string poseJointName = rig.Joints.Any(joint =>
   joint.IsDeformJoint && joint.Name.Equals("Spine_02", StringComparison.Ordinal))
  ? "Spine_02"
  : rig.Joints.First(joint => joint.IsDeformJoint && joint.ParentJointIndex >= 0).Name;
 TargetRigFittingPose pose = rig.CreateFittingPose();
 pose.SetLocalRotationDelta(
  poseJointName,
  Quaternion.CreateFromAxisAngle(Vector3.UnitZ, MathF.PI / 90f));
 TargetRigFittingPoseSnapshot snapshot = pose.Capture();
 if (snapshot.IsIdentityPose)
  throw new InvalidOperationException("Mode-3 test pose unexpectedly remained identity.");
 GeneratedSkinningPreparationResult posed =
  GeneratedSkinningPreparer.Prepare(
   target,
   donor,
   snapshot,
   manualAlignment);
 ValidateGeneratedPreparation(posed, rig, "mode3 posed");
 VerifyExplicitGeneratedAlignment(
  donor,
  posed.FittingPreviewScene,
  manualAlignment,
  "mode3 explicit posed alignment");
 if (!HasGeometryDifference(posed.FittingPreviewScene, posed.PreparedScene))
  throw new InvalidOperationException(
   "Mode-3 local rotation did not produce a distinct canonical bake.");

 VerifyPreserveTargetTextureTransfer(
  target, posed.PreparedScene, outputPath, "mode3");
 VerifyImportDonorExternalCatalogComposition(
  target, posed.PreparedScene, outputPath, "mode3");

 string identityOutputPath = Path.Combine(
  Path.GetDirectoryName(outputPath) ?? throw new InvalidOperationException(
   "Mode-3 output directory is unavailable."),
  $".{Path.GetFileName(outputPath)}.identity.{Guid.NewGuid():N}.smo");
 try
 {
  GlbSkinTransferPlan identityWriterPlan = SmoSkinnedGlbReplacer.Analyze(
   target,
   identity.PreparedScene,
   SkinnedTextureTransferMode.ImportDonor);
  if (!identityWriterPlan.CanReplace)
   throw new InvalidOperationException(
    "Mode-3 identity prepared scene cannot be written: " +
    string.Join(" | ", identityWriterPlan.Messages));
  _ = SmoSkinnedGlbReplacer.Replace(
   target,
   identity.PreparedScene,
   ReplacementTransform.Identity,
   identityOutputPath,
   SkinnedGeometryTransferMode.PreservePreparedGeometry,
   texture: null,
   textureMode: SkinnedTextureTransferMode.ImportDonor);
  VerifySkinnedWriterTargetGraph(target, identityOutputPath);
 }
 finally
 {
  if (File.Exists(identityOutputPath))
   File.Delete(identityOutputPath);
 }

 GlbSkinTransferPlan writerPlan = SmoSkinnedGlbReplacer.Analyze(
  target,
  posed.PreparedScene,
  SkinnedTextureTransferMode.ImportDonor);
 if (!writerPlan.CanReplace)
  throw new InvalidOperationException(
   "Mode-3 prepared scene cannot be written: " +
   string.Join(" | ", writerPlan.Messages));
 GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
  target,
  posed.PreparedScene,
  ReplacementTransform.Identity,
  outputPath,
  SkinnedGeometryTransferMode.PreservePreparedGeometry,
  texture: null,
  textureMode: SkinnedTextureTransferMode.ImportDonor);
 VerifySkinnedWriterTargetGraph(target, outputPath);
 for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
 {
  ImportedMesh mesh = donor.Meshes[meshIndex];
  if (!mesh.Positions.SequenceEqual(donorPositionsBefore[meshIndex]) ||
      !mesh.Normals.SequenceEqual(donorNormalsBefore[meshIndex]) ||
      !mesh.TextureCoordinates.SequenceEqual(donorUvsBefore[meshIndex]) ||
      !mesh.TriangleIndices.SequenceEqual(donorTrianglesBefore[meshIndex]) ||
      !mesh.DiffuseColors.SequenceEqual(donorColorsBefore[meshIndex]) ||
      mesh.Skinning is not null)
  {
   throw new InvalidOperationException(
    $"Mode-3 preparation mutated donor mesh [{meshIndex}] '{mesh.Name}'.");
  }
 }
 if (donor.Textures.Count != donorTexturesBefore.Length ||
     donor.Textures.Where((texture, index) =>
      !texture.Data.SequenceEqual(donorTexturesBefore[index])).Any())
  throw new InvalidOperationException("Mode-3 preparation mutated donor textures.");
 VerifyTestSourcesUnchanged(targetPath, targetBefore, donorPath, donorBefore);
 Console.WriteLine(
  $"PORTING MODE 3 PASS: generated={posed.Analysis.PreparedVertexCount}; " +
   $"attachments={posed.Analysis.Attachments.Count}; pose-joint={poseJointName}; " +
   $"alignment-scale={posed.Analysis.Alignment.Scale:G9}; " +
   $"meshes={result.MeshSlotCount}; vertices={result.VertexCount}; " +
  $"triangles={result.TriangleCount}; palettes={result.PaletteCount}; " +
  $"SHA-256={result.Sha256}; output={result.OutputPath}");
 return 0;
}

if (args.Length == 4 && args[0] == "--skinned-glb-diagnostics")
{
 SmoDocument target = SmoDocument.Load(args[1]);
 ImportedScene donor = GlbModelReader.Read(args[2]);
 ImportedTexture texture = donor.Textures.FirstOrDefault() ??
  throw new InvalidDataException("Diagnostic GLB has no embedded base-color texture.");
 string outputDirectory = Path.GetFullPath(args[3]);
 Directory.CreateDirectory(outputDirectory);

 WriteTextureVariant("01_texture_rgb_only.smo", replaceAlpha: false);
 WriteTextureVariant("02_texture_rgba.smo", replaceAlpha: true);

 ImportedMesh[] bodyMeshes = donor.Meshes
  .GroupBy(mesh => mesh.MaterialIndex)
  .OrderByDescending(group => group.Sum(mesh => mesh.TriangleIndices.Length / 3))
  .First().ToArray();
 var bodyScene = new ImportedScene(bodyMeshes, donor.Textures);
 WriteSkinnedVariant(
  "04_body_skinned_original_texture.smo", bodyScene,
  rebaseToTargetBindPose: true);

 ImportedSkeleton skeleton = bodyMeshes[0].Skinning?.Skeleton ??
  throw new InvalidDataException("Diagnostic body primitive has no skin.");
 int pelvis = skeleton.JointNames
  .Select((name, index) => (name, index))
  .Single(item => item.name == "Pelvis").index;
 if (pelvis > ushort.MaxValue)
  throw new InvalidDataException("Pelvis joint index exceeds UInt16.");
 ImportedMesh[] rigidMeshes = bodyMeshes.Select(mesh => mesh with
 {
  Skinning = new ImportedSkinning(
   skeleton,
   Enumerable.Repeat(
    new ImportedJointIndices((ushort)pelvis, 0, 0, 0), mesh.Positions.Length).ToArray(),
   Enumerable.Repeat(Vector4.UnitX, mesh.Positions.Length).ToArray())
 }).ToArray();
 WriteSkinnedVariant(
  "03_body_rigid_pelvis_original_texture.smo",
  new ImportedScene(rigidMeshes, donor.Textures),
  rebaseToTargetBindPose: false);
 return 0;

 void WriteTextureVariant(string fileName, bool replaceAlpha)
 {
  byte[] output = target.Data.ToArray();
  int[] textureIndices = SMOTextureTool.Core.SmoDocument.Parse(output).Textures
   .Select(item => item.Index).ToArray();
  foreach (int textureIndex in textureIndices)
   output = replaceAlpha
    ? FixedSizeTextureWriter.ReplaceRgbaDiagnosticUnsafe(
       output, textureIndex, texture.Data)
    : FixedSizeTextureWriter.ReplaceRgb(output, textureIndex, texture.Data);
  string path = Path.Combine(outputDirectory, fileName);
  File.WriteAllBytes(path, output);
  SmoDocument verified = SmoDocument.Load(path);
  if (verified.HasErrors || verified.Objects.Count != target.Objects.Count)
   throw new InvalidDataException($"Diagnostic texture variant {fileName} is invalid.");
  Console.WriteLine(
   $"{fileName}: texture-only; alpha={(replaceAlpha ? "donor" : "target")}; " +
   $"SHA-256={Convert.ToHexString(SHA256.HashData(output))}");
 }

 void WriteSkinnedVariant(
  string fileName,
  ImportedScene scene,
  bool rebaseToTargetBindPose)
 {
  string path = Path.Combine(outputDirectory, fileName);
  GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
   target, scene, ReplacementTransform.Identity, path,
   rebaseToTargetBindPose, texture: null);
  Console.WriteLine(
   $"{fileName}: triangles={result.TriangleCount}; palettes={result.PaletteCount}; " +
   $"SHA-256={result.Sha256}");
 }
}

if (args.Length is 4 or 5 && args[0] is "--skinned-glb" or "--skinned-fbx")
{
 SmoDocument target = SmoDocument.Load(args[1]);
 ImportedScene donor = args[0] == "--skinned-fbx"
  ? FbxModelReader.Read(args[2])
  : GlbModelReader.Read(args[2]);
 GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(target, donor);
 Console.WriteLine(
  $"Skinned model plan: {plan.Compatibility}; joints={plan.JointCount}; " +
  $"active={plan.ActiveJointCount}; exact={plan.MatchedBoneNames.Count}; " +
  $"remapped={plan.RemappedBones.Count}; bind-pose-diff={plan.DifferentBindPoseJointCount}.");
 foreach (GlbBoneRemap mapping in plan.RemappedBones)
  Console.WriteLine($"  {mapping.DonorBoneName} -> {mapping.TargetBoneName}: {mapping.Reason}");
 foreach (string message in plan.Messages)
  Console.WriteLine("  " + message);
 if (!plan.CanReplace)
  throw new InvalidOperationException("Skinned GLB plan is blocked.");
 GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
  target, donor, ReplacementTransform.Identity, args[3],
  SkinnedGeometryTransferMode.RetargetToGameBindPose,
  texture: args.Length == 5
   ? ImportedTextureFileReader.Read(args[4])
   : null);
 SmoDocument verified = SmoDocument.Load(args[3]);
 if (verified.HasErrors || verified.Objects.Count != target.Objects.Count)
  throw new InvalidDataException("Skinned GLB result did not preserve target graph.");
 for (int index = 0; index < target.Objects.Count; index++)
 {
  SmoObjectEntry before = target.Objects[index];
  SmoObjectEntry after = verified.Objects[index];
  if (before.Id != after.Id || before.Name != after.Name ||
      before.TypeHash != after.TypeHash || before.ParentIndex != after.ParentIndex)
   throw new InvalidDataException($"Target object identity [{index}] changed.");
 }
 foreach (SmoObjectEntry targetSkinEntry in target.Objects.Where(entry =>
              entry.TypeHash == SmoClassIds.Skin))
 {
  if (!SmoSkinDecoder.TryDecode(
       target, targetSkinEntry, out SmoSkin? targetSkin, out _) || targetSkin is null)
   continue;
  string[] targetNames = targetSkin.Bones.Select(bone =>
   target.Objects[bone.NodeObjectIndex].Name).ToArray();
  if (targetNames.Distinct(StringComparer.Ordinal).Count() != 1)
   continue;
  SmoObjectEntry outputSkinEntry = verified.Objects[targetSkinEntry.Index];
  if (!SmoSkinDecoder.TryDecode(
       verified, outputSkinEntry, out SmoSkin? outputSkin, out string outputError) ||
      outputSkin is null)
   throw new InvalidDataException(
    $"Rigid target skin [{targetSkinEntry.Index}] is invalid: {outputError}");
  string[] outputNames = outputSkin.Bones.Select(bone =>
   verified.Objects[bone.NodeObjectIndex].Name).ToArray();
  if (!targetNames.SequenceEqual(outputNames, StringComparer.Ordinal))
   throw new InvalidDataException(
    $"Rigid target skin [{targetSkinEntry.Index}] palette changed.");
 }
 Console.WriteLine(
  $"SKINNED MODEL PASS: output-meshes={result.MeshSlotCount}; " +
  $"vertices={result.VertexCount}; triangles={result.TriangleCount}; " +
  $"palettes={result.PaletteCount}; SHA-256={result.Sha256}; output={result.OutputPath}");
 return 0;
}

if (args.Length == 5 && args[0] == "--fixed-texture-probe")
{
 byte[] source = File.ReadAllBytes(args[1]);
 ImportedScene scene = GlbModelReader.Read(args[2]);
 int imageIndex = int.Parse(args[3], System.Globalization.CultureInfo.InvariantCulture);
 if ((uint)imageIndex >= (uint)scene.Textures.Count)
  throw new ArgumentOutOfRangeException(nameof(imageIndex), $"GLB has {scene.Textures.Count} embedded base-color textures.");
 byte[] output = FixedSizeTextureWriter.ReplaceRgb(source, 1, scene.Textures[imageIndex].Data);
 File.WriteAllBytes(args[4], output);
 SMOTextureTool.Core.SmoDocument before = SMOTextureTool.Core.SmoDocument.Parse(source);
 SMOTextureTool.Core.SmoDocument after = SMOTextureTool.Core.SmoDocument.Parse(output);
 SMOTextureTool.Core.TextureInfo target = before.Textures[0];
 int outside = source.Select((value, index) => (value, index))
  .Count(item => item.index < target.PixelDataOffset || item.index >= target.PixelDataOffset + target.PixelDataSize
   ? item.value != output[item.index] : false);
 if (source.Length != output.Length || outside != 0)
  throw new InvalidOperationException("Fixed texture probe changed file structure or bytes outside pixel data.");
 if (after.Textures[0].Material is null)
  throw new InvalidOperationException("Fixed texture probe lost the material owner.");
 Console.WriteLine($"FIXED TEXTURE PROBE: {target.Width}x{target.Height}, same length, 0 changes outside pixels, alpha preserved; output={Path.GetFullPath(args[4])}");
 return 0;
}

if (args.Length == 2 && args[0] == "--fixed-texture-writer-regression")
{
 byte[] source = File.ReadAllBytes(args[1]);
 SMOTextureTool.Core.SmoDocument textures =
  SMOTextureTool.Core.SmoDocument.Parse(source);
 int regressionChecks = 0;
 RunFixedSizeTextureWriterRegression(source, textures, (value, message) =>
 {
  regressionChecks++;
  if (!value)
   throw new InvalidOperationException("FAIL: " + message);
 });
 Console.WriteLine(
  $"FIXED TEXTURE WRITER PASS: {regressionChecks} assertions; " +
  $"source={Path.GetFullPath(args[1])}");
 return 0;
}

if (args.Length == 4 && args[0] == "--raw-texture-probe")
{
 byte[] probe = File.ReadAllBytes(args[1]);
 SMOTextureTool.Core.SmoDocument probeDocument = SMOTextureTool.Core.SmoDocument.Parse(probe);
 if (probeDocument.Textures.Count == 0) throw new InvalidOperationException("Texture probe source has no textures.");
 SMOTextureTool.Core.TextureInfo texture = probeDocument.Textures[0];
 int relativeOffset = args[2] switch
 {
  "first-blue" => 1,
  "center-blue" => checked(((texture.Height / 2) * texture.Width + texture.Width / 2) * 4 + 1),
  _ => throw new ArgumentException("Probe channel must be first-blue or center-blue.")
 };
 int changedOffset = checked(texture.PixelDataOffset + relativeOffset);
 probe[changedOffset] ^= 1;
 File.WriteAllBytes(args[3], probe);
 byte[] verified = File.ReadAllBytes(args[3]);
 int differences = verified.Zip(File.ReadAllBytes(args[1])).Count(pair => pair.First != pair.Second);
 if (differences != 1) throw new InvalidOperationException($"Raw probe changed {differences} bytes instead of one.");
 _ = SmoDocument.Load(args[3]);
 _ = SMOTextureTool.Core.SmoDocument.Load(args[3]);
 Console.WriteLine($"RAW TEXTURE PROBE: changed exactly one RGB byte at 0x{changedOffset:X}; output={Path.GetFullPath(args[3])}");
 return 0;
}

if (args.Length is not 1 and not 3 and not 4 || !File.Exists(args[0]) ||
    (args.Length >= 3 && !File.Exists(args[1])) || (args.Length == 4 && !File.Exists(args[3])))
{
 Console.Error.WriteLine("Usage: SmoImporter.FormatTests <sample.smo> [replacement.glb output.smo [texture.png]]");
 Console.Error.WriteLine("       SmoImporter.FormatTests --smo-to-smo <target.smo> <donor.smo> <output.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --porting-mode2 <target.smo> <donor.glb> <output.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --porting-mode3 <target.smo> <donor.glb|obj|fbx> <output.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --target-rig-auto-fit <target.smo> <donor.obj|fbx> <alignment-scale>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --generated-skinning-degenerate-fbx-regression <target.smo> <donor.fbx>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --generated-topology-normalization-regression <target.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --target-rig-layla-equivalence <target.smo> <layla.obj> <layla.fbx>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --texture-catalog-regression");
 Console.Error.WriteLine("       SmoImporter.FormatTests --skinned-glb <target.smo> <donor.glb> <output.smo> [texture.png]");
 Console.Error.WriteLine("       SmoImporter.FormatTests --skinned-fbx <target.smo> <donor.fbx> <output.smo> [texture.png]");
 Console.Error.WriteLine("       SmoImporter.FormatTests --skinned-glb-diagnostics <target.smo> <donor.glb> <output-dir>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --rigid-multitexture <target.smo> <donor.glb|obj|fbx> <output.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --rigid-texture-bundle <donor.glb|obj|fbx> [texture-folder]");
 Console.Error.WriteLine("       SmoImporter.FormatTests --rigid-texture-resize-regression <donor.glb|obj|fbx>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --scan-texture-metadata <media-directory>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --dump-texture-sequences <sample.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --fixed-texture-writer-regression <sample.smo>");
 Console.Error.WriteLine("       SmoImporter.FormatTests --dump-tree <sample.smo>");
 return 2;
}
int checks = 0;
void Check(bool value, string message) { checks++; if (!value) throw new InvalidOperationException("FAIL: " + message); }
string temp = Path.Combine(Path.GetTempPath(), "smo-import-tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temp);
try
{
 byte[] originalBytes = File.ReadAllBytes(args[0]);
 string originalHash = Convert.ToHexString(SHA256.HashData(originalBytes));
 SmoDocument sourceDocument = SmoDocument.Load(args[0]);
 SMOTextureTool.Core.SmoDocument sourceTextures = SMOTextureTool.Core.SmoDocument.Load(args[0]);
 Console.WriteLine("SOURCE TEXTURES: " + string.Join("; ", sourceTextures.Textures.Select(texture =>
  $"#{texture.Index} {texture.Width}x{texture.Height} fmt=0x{texture.FormatCode:X4} layout={texture.Layout} " +
  $"owner={(texture.Material is null ? "none" : $"material#{texture.Material.Index}/pass{texture.Material.PassIndex}/layer{texture.Material.LayerIndex}")}")));
 SmoExportScene exported = SmoSceneBuilder.Build(sourceDocument);
 Check(exported.Meshes.Count > 0, "exported mesh exists");
 string glb = Path.Combine(temp, "source.glb");
 GlbExporter.Export(exported, glb);
 ImportedScene imported = GlbModelReader.Read(glb);
 Check(imported.Meshes.Count == exported.Meshes.Count, "GLB mesh count round-trip");
 Check(imported.Meshes.Where(mesh => mesh.Skinning is not null).All(mesh =>
  mesh.Skinning!.Skeleton.JointNames.Distinct(StringComparer.Ordinal).Count() ==
  mesh.Skinning.Skeleton.JointNames.Count),
  "repeated GLB palette padding is canonicalized to unique joints");
 Check(exported.Meshes.Zip(imported.Meshes).All(pair =>
 {
  if (pair.First.SkinObjectIndex is not int skinObjectIndex)
   return pair.Second.Skinning is null;
  SmoExportSkin sourceSkin = exported.Skins.Single(skin => skin.ObjectIndex == skinObjectIndex);
  return pair.Second.Skinning is not null &&
   pair.Second.Skinning.Skeleton.JointNames.Count ==
   sourceSkin.JointObjectIndices.Distinct().Count();
 }), "canonical GLB joint count matches unique source palette nodes");
 Check(imported.Meshes.Where(mesh => mesh.Skinning is not null).All(mesh =>
 {
  int jointCount = mesh.Skinning!.Skeleton.JointNames.Count;
  return mesh.Skinning.JointIndices.All(joints =>
   joints.X < jointCount && joints.Y < jointCount &&
   joints.Z < jointCount && joints.W < jointCount);
 }), "remapped GLB vertex joints stay inside the canonical palette");
 Check(imported.Meshes.Where(mesh => mesh.Skinning is not null).All(mesh =>
 {
  ImportedSkeleton skeleton = mesh.Skinning!.Skeleton;
  IReadOnlyList<int>? parents = skeleton.ParentJointIndices;
  if (!skeleton.HasHierarchy || parents is null)
   return false;
  return parents.Select((parent, joint) =>
   parent >= -1 && parent < parents.Count && parent != joint).All(valid => valid);
 }), "canonical GLB skeleton retains nearest-joint hierarchy");
 Check(imported.Meshes.Where(mesh => mesh.Skinning is not null).All(mesh =>
 {
  ImportedSkeleton skeleton = mesh.Skinning!.Skeleton;
  IReadOnlyList<int>? parents = skeleton.ParentJointIndices;
  IReadOnlyList<Matrix4x4>? worlds = skeleton.BindWorldMatrices;
  IReadOnlyList<Matrix4x4>? locals = skeleton.BindLocalMatrices;
  if (!skeleton.HasBindPose || parents is null || worlds is null || locals is null)
   return false;
  for (int joint = 0; joint < skeleton.JointNames.Count; joint++)
  {
   if (!Matrix4x4.Invert(
        skeleton.InverseBindMatrices[joint], out Matrix4x4 expectedWorld) ||
       !worlds[joint].Equals(expectedWorld))
    return false;
   Matrix4x4 expectedLocal = parents[joint] < 0
    ? expectedWorld
    : expectedWorld * skeleton.InverseBindMatrices[parents[joint]];
   if (!locals[joint].Equals(expectedLocal))
    return false;
  }
  return true;
 }), "canonical GLB skeleton retains world and hierarchy-relative bind pose");
 MeshSplitPolicy splitPolicy = new(1000, 1500, 500, 1);
 MeshSplitPlan split = MeshSplitter.Split(imported, splitPolicy);
 Check(split.Chunks.Count > 1, "whole scene is split into multiple chunks");
 Check(split.Chunks.All(chunk => chunk.Mesh.Positions.Length <= splitPolicy.MaxVerticesPerChunk &&
  chunk.Mesh.TriangleIndices.Length <= splitPolicy.MaxIndicesPerChunk &&
  chunk.SourceTriangleCount <= splitPolicy.MaxTrianglesPerChunk), "every chunk respects limits");
 Check(split.Chunks.Sum(chunk => chunk.SourceTriangleCount) == split.SourceTriangleCount, "all triangles are preserved");
 MeshSplitPlan repeatedSplit = MeshSplitter.Split(imported, splitPolicy);
 Check(split.Chunks.SelectMany(chunk => chunk.Mesh.TriangleIndices).SequenceEqual(
  repeatedSplit.Chunks.SelectMany(chunk => chunk.Mesh.TriangleIndices)), "split is deterministic");
 SmoExportMesh exportMesh = exported.Meshes[0];
 SmoObjectEntry entry = sourceDocument.Objects[exportMesh.ObjectIndex];
 SmoMesh before = SmoMeshDecoder.Decode(sourceDocument, entry);
 IReadOnlyList<SmoBoneSlot> slots = SmoMeshReplacer.GetBoneSlots(sourceDocument, entry);
 string output = Path.Combine(temp, "replaced.smo");
 ReplacementResult result = SmoMeshReplacer.Replace(
  sourceDocument, entry, imported.Meshes[0], ReplacementTransform.Identity,
  slots.Count > 0 ? slots[0].Slot : 0, output);
 Check(new FileInfo(output).Length == originalBytes.Length, "in-place output length");
 Check(Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[0]))) == originalHash, "source remains unchanged");
 SmoDocument afterDocument = SmoDocument.Load(output);
 SmoMesh after = SmoMeshDecoder.Decode(afterDocument, afterDocument.Objects[entry.Index]);
 Check(after.VertexCount == before.VertexCount, "vertex count preserved");
 Check(after.StripIndices.SequenceEqual(before.StripIndices), "original strip preserved");
 Check(after.Positions.Zip(before.Positions).All(pair => System.Numerics.Vector3.Distance(pair.First, pair.Second) < 0.0001f), "identity geometry round-trip");
 if (after.HasSkinningData)
  Check(after.BlendWeights.All(weight => weight.X == 1f && weight.Y == 0f && weight.Z == 0f && weight.W == 0f), "rigid weights written");
 if (args.Length >= 3)
 {
  ImportedScene wholeScene = ImportedModelReader.Read(args[1]);
  Check(wholeScene.Textures.Count > 0, "GLB embedded base-color texture is discovered");
  IReadOnlyList<SmoBoneSlot> rigidChoices = SmoWholeModelReplacer.GetRigidBoneChoices(sourceDocument);
  Check(rigidChoices.Count > 0, "whole replacement exposes rigid bone choices");
  int chosenRigidSlot = rigidChoices[0].Slot;
  WholeModelReplacementResult whole = SmoWholeModelReplacer.Replace(
   sourceDocument, wholeScene, ReplacementTransform.Identity, args[2], chosenRigidSlot,
   args.Length == 4 ? args[3] : null,
   args.Length == 3 ? wholeScene.Textures[0] : null);
  SmoDocument wholeDocument = SmoDocument.Load(args[2]);
  SmoMesh[] wholeMeshes = wholeDocument.Objects.Where(item => item.TypeHash == SmoClassIds.MeshData)
   .Select(item => SmoMeshDecoder.Decode(wholeDocument, item)).ToArray();
  int importedTriangles = wholeScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
  Check(wholeMeshes.Length == exported.Meshes.Count, "whole replacement keeps mesh slot count");
  Check(wholeMeshes.Max(mesh => mesh.TriangleCount) == importedTriangles, "whole replacement preserves every visible triangle in one slot");
  Check(wholeMeshes.All(mesh => mesh.PrimitiveType == SmoMeshDecoder.TriangleListPrimitive), "whole replacement uses triangle lists");
  Check(wholeMeshes.Where(mesh => mesh.TriangleCount > 0).All(mesh =>
   mesh.HasSkinningData && mesh.BlendWeights.All(weight => weight.X == 1f)), "visible whole replacement is rigid-skinned");
  Check(wholeMeshes.Single(mesh => mesh.TriangleCount == importedTriangles).BlendIndices.All(
   indices => indices.X == chosenRigidSlot), "selected rigid bone slot is written to every visible vertex");
  Check(wholeMeshes.Count(mesh => mesh.TriangleCount == importedTriangles) == 1, "whole replacement uses one visible rigid mesh slot");
  Check(wholeMeshes.Where(mesh => mesh.TriangleCount != importedTriangles).All(mesh =>
   mesh.Stride == 56 && mesh.VertexCount == 1 && mesh.TriangleIndices.SequenceEqual(new uint[] { 0, 0, 0 })),
   "disabled slots keep a valid degenerate primitive");
  Check(!wholeDocument.HasErrors, "whole replacement passes strict container validation");
  if (args.Length >= 3)
  {
   SMOTextureTool.Core.SmoDocument textured = SMOTextureTool.Core.SmoDocument.Load(args[2]);
   Check(textured.Textures.Count > 0, "textured replacement keeps texture catalog");
   Check(textured.Textures[0].Material is not null, "primary texture keeps its material owner");
   Console.WriteLine("TEXTURES: " + string.Join("; ", textured.Textures.Select(texture =>
    $"#{texture.Index} {texture.Width}x{texture.Height} fmt=0x{texture.FormatCode:X4} layout={texture.Layout} " +
    $"owner={(texture.Material is null ? "none" : $"material#{texture.Material.Index}/pass{texture.Material.PassIndex}/layer{texture.Material.LayerIndex}")}")));
  }
  Console.WriteLine($"WHOLE: meshes={whole.MeshCount}; vertices={whole.VertexCount}; triangles={whole.TriangleCount}; bytes={whole.FileSize}; file={whole.OutputPath}");
 }
 Console.WriteLine($"PASS: {checks} assertions; mesh={entry.Name}; output={result.VertexCount} vertices");
 return 0;
}
finally { Directory.Delete(temp, true); }

static void RunFixedSizeTextureWriterRegression(
 byte[] source,
 SMOTextureTool.Core.SmoDocument document,
 Action<bool, string> check)
{
 SMOTextureTool.Core.TextureInfo? texture = document.Textures
  .Where(item => item.FormatCode is 0x32E3 or 0x43E3)
  .OrderBy(item => item.PixelDataSize)
  .FirstOrDefault();
 check(texture is not null, "a 0x32E3/0x43E3 texture exists for fixed-size writer tests");
 if (texture is null)
  return;

 int markerOffset = checked(texture.BlockOffset + 0x3C);
 check(texture.Layout == SMOTextureTool.Core.TextureLayout.Bgra,
  "0x32E3/0x43E3 fixed-size texture uses BGRA layout");
 check(texture.PixelDataOffset == texture.BlockOffset + 0x3D,
  "0x32E3/0x43E3 pixel payload starts at +0x3D");
 check(source[markerOffset] == 0, "serializer marker at +0x3C is 00");

 var first = new Rgba32(17, 34, 51, 68);
 var last = new Rgba32(85, 102, 119, 136);
 using var replacement = new Image<Rgba32>(texture.Width, texture.Height, first);
 replacement[texture.Width - 1, texture.Height - 1] = last;
 using var png = new MemoryStream();
 replacement.SaveAsPng(png);
 byte[] imageData = png.ToArray();

 byte[] rgb = FixedSizeTextureWriter.ReplaceRgb(source, texture.Index, imageData);
 check(rgb[markerOffset] == source[markerOffset],
  "RGB replacement keeps serializer marker unchanged");
 CheckRawBgra(rgb, texture.PixelDataOffset, first, preserveAlpha: true,
  source[texture.PixelDataOffset + 3], check, "RGB first pixel");
 int lastOffset = checked(texture.PixelDataOffset + texture.PixelDataSize - 4);
 CheckRawBgra(rgb, lastOffset, last, preserveAlpha: true,
  source[lastOffset + 3], check, "RGB last pixel");
 bool alphaPreserved = true;
 for (int offset = texture.PixelDataOffset;
      alphaPreserved && offset < texture.PixelDataOffset + texture.PixelDataSize;
      offset += 4)
  alphaPreserved = rgb[offset + 3] == source[offset + 3];
 check(alphaPreserved, "RGB replacement preserves every host Alpha byte");
 CheckOutsidePixelPayloadUnchanged(source, rgb, texture, check, "RGB replacement");

 byte[] rgba = FixedSizeTextureWriter.ReplaceRgbaDiagnosticUnsafe(
  source, texture.Index, imageData);
 check(rgba[markerOffset] == source[markerOffset],
  "full BGRA replacement keeps serializer marker unchanged");
 CheckRawBgra(rgba, texture.PixelDataOffset, first, preserveAlpha: false,
  0, check, "full BGRA first pixel");
 CheckRawBgra(rgba, lastOffset, last, preserveAlpha: false,
  0, check, "full BGRA last pixel");
 CheckOutsidePixelPayloadUnchanged(source, rgba, texture, check, "full BGRA replacement");
}

static void CheckRawBgra(
 byte[] data,
 int offset,
 Rgba32 expected,
 bool preserveAlpha,
 byte preservedAlpha,
 Action<bool, string> check,
 string context)
{
 check(data[offset] == expected.B &&
       data[offset + 1] == expected.G &&
       data[offset + 2] == expected.R,
  $"{context} stores B, G and R at +0, +1 and +2");
 check(data[offset + 3] == (preserveAlpha ? preservedAlpha : expected.A),
  $"{context} stores the expected Alpha at +3");
}

static void CheckOutsidePixelPayloadUnchanged(
 byte[] source,
 byte[] output,
 SMOTextureTool.Core.TextureInfo texture,
 Action<bool, string> check,
 string context)
{
 int payloadEnd = checked(texture.PixelDataOffset + texture.PixelDataSize);
 bool unchanged = source.Length == output.Length;
 for (int index = 0; unchanged && index < source.Length; index++)
  if (index < texture.PixelDataOffset || index >= payloadEnd)
   unchanged = source[index] == output[index];
 check(unchanged, $"{context} changes no bytes outside the pixel payload");
}

static void RunImportedTextureCatalogBatchRegression()
{
 ImportedTexture SourceTexture(string name, byte marker) =>
  new(name, "image/png", 1, 1, [marker], Path.Combine("embedded", name));

 ImportedTexture sourceBody = SourceTexture("body.png", 1);
 ImportedTexture sourceFace = SourceTexture("face.png", 2);
 var source = new ImportedScene(
  Array.Empty<ImportedMesh>(),
  Array.AsReadOnly([sourceBody, sourceFace]),
  Array.AsReadOnly(
  [
   new ImportedMaterial("body", "body.png", 0),
   new ImportedMaterial("face", "face.png", 1)
  ]));
 ImportedTexture exactBody = SourceTexture("body.png", 11) with
 {
  SourcePath = Path.Combine("external", "body.png")
 };
 ImportedTexture sameBaseBody = SourceTexture("body.jpg", 12) with
 {
  SourcePath = Path.Combine("external", "body.jpg")
 };
 ImportedTexture unmatched = SourceTexture("unmatched.png", 13) with
 {
  SourcePath = Path.Combine("external", "unmatched.png")
 };
 ImportedTextureCatalogResult exact = ImportedTextureCatalog.ResolveExternalOverrides(
  source,
  Array.AsReadOnly([exactBody, sameBaseBody, unmatched]));
 if (!ReferenceEquals(exact.EffectiveScene.Textures[0], exactBody) ||
     !ReferenceEquals(exact.EffectiveScene.Textures[1], sourceFace) ||
     exact.EffectiveScene.Materials[0].BaseColorTextureIndex != 0 ||
     !exact.UnusedExternalTextures.SequenceEqual([sameBaseBody, unmatched]) ||
     !ReferenceEquals(source.Textures[0], sourceBody) || sourceBody.Data[0] != 1)
 {
  throw new InvalidOperationException(
   "Texture catalog did not prefer the exact filename or mutated its source scene.");
 }
 if (!exact.Messages.Any(message =>
      message.Contains("overrides source texture [0]", StringComparison.Ordinal)) ||
     exact.Messages.Count(message =>
      message.Contains("remains unused", StringComparison.Ordinal)) != 2)
 {
  throw new InvalidOperationException(
   "Texture catalog did not report the exact override and both unmatched batch files.");
 }

 var unresolved = new ImportedScene(
  Array.Empty<ImportedMesh>(),
  Array.Empty<ImportedTexture>(),
  Array.AsReadOnly([new ImportedMaterial("cloth", "cloth_missing.png", -1)]));
 ImportedTexture fallbackTexture = SourceTexture("manual_pick.png", 21);
 ImportedTextureCatalogResult fallback = ImportedTextureCatalog.ResolveExternalOverrides(
  unresolved,
  Array.AsReadOnly([fallbackTexture]));
 if (fallback.EffectiveScene.Textures.Count != 1 ||
     !ReferenceEquals(fallback.EffectiveScene.Textures[0], fallbackTexture) ||
     fallback.EffectiveScene.Materials[0].BaseColorTextureIndex != 0 ||
     fallback.UnusedExternalTextures.Count != 0 ||
     !fallback.Messages.Any(message => message.Contains(
      "unique unresolved-group fallback", StringComparison.Ordinal)))
 {
  throw new InvalidOperationException(
   "Texture catalog did not apply its unique unresolved-group fallback.");
 }

 var ambiguousGroups = new ImportedScene(
  Array.Empty<ImportedMesh>(),
  Array.AsReadOnly(
  [
   SourceTexture("hair.png", 31),
   SourceTexture("hair.jpg", 32)
  ]),
  Array.AsReadOnly(
  [
   new ImportedMaterial("hair-a", "hair.png", 0),
   new ImportedMaterial("hair-b", "hair.jpg", 1)
  ]));
 ExpectCatalogAmbiguity(
  ambiguousGroups,
  [SourceTexture("hair.webp", 33)],
  "matches multiple source texture groups");
 ExpectCatalogAmbiguity(
  new ImportedScene(
   Array.Empty<ImportedMesh>(),
   Array.AsReadOnly([SourceTexture("body.png", 41)]),
   Array.AsReadOnly([new ImportedMaterial("body", "body.png", 0)])),
  [SourceTexture("body.jpg", 42), SourceTexture("body.webp", 43)],
  "matches multiple external files");

 static void ExpectCatalogAmbiguity(
  ImportedScene scene,
  ImportedTexture[] external,
  string expectedMessage)
 {
  try
  {
   _ = ImportedTextureCatalog.ResolveExternalOverrides(
    scene, Array.AsReadOnly(external));
  }
  catch (InvalidDataException exception) when (
   exception.Message.Contains(expectedMessage, StringComparison.Ordinal))
  {
   return;
  }
  throw new InvalidOperationException(
   $"Texture catalog did not reject an ambiguous batch ({expectedMessage}).");
 }
}

static ImportedTexture[] ReadDonorDirectoryTextures(
 string donorPath,
 ImportedScene donor)
{
 string? directory = Path.GetDirectoryName(Path.GetFullPath(donorPath));
 if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
  return [];
 string[] files = Directory.EnumerateFiles(
   directory, "*", SearchOption.TopDirectoryOnly)
  .Where(path => Path.GetExtension(path).ToLowerInvariant() is
   ".png" or ".jpg" or ".jpeg" or ".bmp" or ".tga")
  .OrderBy(path => path, StringComparer.OrdinalIgnoreCase)
  .ToArray();
 var exactFileNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
 var bareStems = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
 void AddReference(string? reference)
 {
  if (string.IsNullOrWhiteSpace(reference))
   return;
  string fileName = Path.GetFileName(reference.Trim());
  string extension = Path.GetExtension(fileName).ToLowerInvariant();
  if (extension is ".png" or ".jpg" or ".jpeg" or ".bmp" or ".tga")
   exactFileNames.Add(fileName);
  else
   bareStems.Add(fileName);
 }
 foreach (ImportedMaterial material in donor.Materials)
 {
  AddReference(material.BaseColorTextureName);
  AddReference(material.Name);
 }
 foreach (ImportedTexture texture in donor.Textures)
 {
  AddReference(texture.SourcePath);
  AddReference(texture.Name);
 }
 HashSet<string> exactStems = exactFileNames
  .Select(fileName => Path.GetFileNameWithoutExtension(fileName) ?? fileName)
  .ToHashSet(StringComparer.OrdinalIgnoreCase);
 string[] selected = files.Where(path =>
  {
   string fileName = Path.GetFileName(path);
   string stem = Path.GetFileNameWithoutExtension(fileName);
   return exactFileNames.Contains(fileName) ||
          !exactStems.Contains(stem) && bareStems.Contains(stem);
  }).ToArray();
 return selected.Select(ImportedTextureFileReader.Read).ToArray();
}

static RigidGlbTextureBundle WithUniformRigidTextureAlpha(
 RigidGlbTextureBundle source,
 byte alpha)
{
 RigidMaterialGroup[] groups = source.MaterialGroups.Select(group => group with
 {
  Frames = Array.AsReadOnly(group.Frames.Select(frame =>
  {
   ImportedTexture texture = frame.Texture;
   using Image<Rgba32> image = Image.Load<Rgba32>(texture.Data);
   if (image.Width != texture.Width || image.Height != texture.Height)
    throw new InvalidDataException(
     $"Texture {texture.Name} dimensions changed before rigid alpha safety regression.");
   image.ProcessPixelRows(accessor =>
   {
    for (int y = 0; y < accessor.Height; y++)
    {
     Span<Rgba32> row = accessor.GetRowSpan(y);
     for (int x = 0; x < row.Length; x++)
     {
      Rgba32 pixel = row[x];
      pixel.A = alpha;
      row[x] = pixel;
     }
    }
   });
   using var encoded = new MemoryStream();
   image.SaveAsPng(encoded);
   return frame with
   {
    Texture = texture with
    {
     MimeType = "image/png",
     Data = encoded.ToArray()
    }
   };
  }).ToArray())
 }).ToArray();
 return source with { MaterialGroups = Array.AsReadOnly(groups) };
}

static void VerifyPreserveTargetTextureTransfer(
 SmoDocument target,
 ImportedScene preparedScene,
 string requestedOutputPath,
 string context)
{
 string transientPath = CreateTransientSiblingPath(
  requestedOutputPath, context + ".preserve-target");
 try
 {
  GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(
   target,
   preparedScene,
   SkinnedTextureTransferMode.PreserveTarget);
  if (!plan.CanReplace)
   throw new InvalidOperationException(
    $"{context}: PreserveTarget analysis rejected the prepared scene: " +
    string.Join(" | ", plan.Messages));
  _ = SmoSkinnedGlbReplacer.Replace(
   target,
   preparedScene,
   ReplacementTransform.Identity,
   transientPath,
   SkinnedGeometryTransferMode.PreservePreparedGeometry,
   texture: null,
   textureMode: SkinnedTextureTransferMode.PreserveTarget);
  VerifySkinnedWriterTargetGraph(target, transientPath);
  VerifyTextureDataObjectsByteIdentical(
   target, SmoDocument.Load(transientPath), context + " PreserveTarget");
  Console.WriteLine(
   $"  {context} PreserveTarget PASS: every target TextureData is byte-identical.");
 }
 finally
 {
  if (File.Exists(transientPath))
   File.Delete(transientPath);
 }
}

static void VerifyImportDonorExternalCatalogComposition(
 SmoDocument target,
 ImportedScene preparedScene,
 string requestedOutputPath,
 string context)
{
 int materialIndex = preparedScene.Meshes
  .Select(mesh => mesh.MaterialIndex)
  .Where(index => index >= 0 && index < preparedScene.Materials.Count)
  .Distinct()
  .FirstOrDefault(index =>
   preparedScene.Materials[index].BaseColorTextureIndex >= 0 &&
   preparedScene.Materials[index].BaseColorTextureIndex < preparedScene.Textures.Count,
   -1);
 if (materialIndex < 0)
  throw new InvalidOperationException(
   $"{context}: prepared scene has no used material with a resolved donor texture.");
 ImportedMaterial material = preparedScene.Materials[materialIndex];
 int textureIndex = material.BaseColorTextureIndex;
 ImportedTexture sourceTexture = preparedScene.Textures[textureIndex];
 string sourceAlias = new[]
  {
   material.BaseColorTextureName,
   sourceTexture.SourcePath,
   sourceTexture.Name
  }
  .Where(value => !string.IsNullOrWhiteSpace(value))
  .Select(value => Path.GetFileName(value!))
  .FirstOrDefault(value => !string.IsNullOrWhiteSpace(value)) ??
  throw new InvalidOperationException(
   $"{context}: resolved donor texture has no usable filename alias.");
 string externalFileName = string.IsNullOrWhiteSpace(Path.GetExtension(sourceAlias))
  ? sourceAlias + ".png"
  : sourceAlias;

 Rgba32 marker = ChooseUnusedSolidMarker(target);
 byte[] markerPng = EncodeSolidPng(marker);
 var exactExternal = new ImportedTexture(
  externalFileName,
  "image/png",
  2,
  2,
  markerPng,
  Path.Combine("external-batch", externalFileName));
 var unmatchedExternal = new ImportedTexture(
  $"__format_tests_unmatched_{context}.png",
  "image/png",
  2,
  2,
  EncodeSolidPng(new Rgba32(239, 17, 113, 59)),
  Path.Combine("external-batch", $"__format_tests_unmatched_{context}.png"));
 byte[] sourceTextureBefore = sourceTexture.Data.ToArray();
 ImportedTextureCatalogResult catalog = ImportedTextureCatalog.ResolveExternalOverrides(
  preparedScene,
  Array.AsReadOnly([exactExternal, unmatchedExternal]));
 if (!ReferenceEquals(catalog.EffectiveScene.Meshes, preparedScene.Meshes) ||
     catalog.EffectiveScene.Materials[materialIndex].BaseColorTextureIndex != textureIndex ||
     !ReferenceEquals(catalog.EffectiveScene.Textures[textureIndex], exactExternal) ||
     catalog.UnusedExternalTextures.Count != 1 ||
     !ReferenceEquals(catalog.UnusedExternalTextures[0], unmatchedExternal) ||
     !sourceTexture.Data.SequenceEqual(sourceTextureBefore))
 {
  throw new InvalidOperationException(
   $"{context}: external texture batch was not composed into the prepared scene exactly.");
 }

 string transientPath = CreateTransientSiblingPath(
  requestedOutputPath, context + ".import-donor-catalog");
 try
 {
  GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(
   target,
   catalog.EffectiveScene,
   SkinnedTextureTransferMode.ImportDonor);
  if (!plan.CanReplace)
   throw new InvalidOperationException(
    $"{context}: ImportDonor analysis rejected the catalog-composed scene: " +
    string.Join(" | ", plan.Messages));
  _ = SmoSkinnedGlbReplacer.Replace(
   target,
   catalog.EffectiveScene,
   ReplacementTransform.Identity,
   transientPath,
   SkinnedGeometryTransferMode.PreservePreparedGeometry,
   texture: null,
   textureMode: SkinnedTextureTransferMode.ImportDonor);
  VerifySkinnedWriterTargetGraph(target, transientPath);
  SmoDocument output = SmoDocument.Load(transientPath);
  if (!ContainsSolidRgbTexture(output, marker))
   throw new InvalidOperationException(
    $"{context}: ImportDonor did not serialize the exact external catalog override.");
  Console.WriteLine(
   $"  {context} ImportDonor catalog PASS: matched external override written; " +
   "unmatched batch file stayed unused.");
 }
 finally
 {
  if (File.Exists(transientPath))
   File.Delete(transientPath);
 }
}

static string CreateTransientSiblingPath(string requestedOutputPath, string label)
{
 string fullOutput = Path.GetFullPath(requestedOutputPath);
 string directory = Path.GetDirectoryName(fullOutput) ?? throw new InvalidOperationException(
  "Porting output directory is unavailable.");
 return Path.Combine(
  directory,
  $".{Path.GetFileName(fullOutput)}.{label}.{Guid.NewGuid():N}.smo");
}

static void VerifyTextureDataObjectsByteIdentical(
 SmoDocument target,
 SmoDocument output,
 string context)
{
 SmoObjectEntry[] textureEntries = target.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
  .ToArray();
 if (textureEntries.Length == 0)
  throw new InvalidOperationException($"{context}: target has no TextureData objects.");
 foreach (SmoObjectEntry before in textureEntries)
 {
  SmoObjectEntry after = output.Objects[before.Index];
  if (before.SerializedSize != after.SerializedSize ||
      !target.Data.Span.Slice(
       checked((int)before.PhysicalOffset), checked((int)before.SerializedSize))
       .SequenceEqual(output.Data.Span.Slice(
        checked((int)after.PhysicalOffset), checked((int)after.SerializedSize))))
  {
   throw new InvalidOperationException(
    $"{context}: target TextureData [{before.Index}] '{before.Name}' changed.");
  }
 }
}

static void VerifyInlinePaletteEntriesPreserved(
 SmoDocument target,
 SmoDocument output,
 string context)
{
 SmoObjectEntry[] inlineSkins = target.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.Skin)
  .Where(entry => SmoSkinDecoder.TryDecode(
    target, entry, out SmoSkin? skin, out _) &&
   skin is not null && skin.Bones.Any(bone => bone.InlineSerializedSize != 0))
  .ToArray();
 if (inlineSkins.Length == 0)
 throw new InvalidOperationException($"{context}: target has no inline skin palette.");
 foreach (SmoObjectEntry before in inlineSkins)
 {
  SmoObjectEntry after = output.Objects[before.Index];
  string beforeError = string.Empty;
  string afterError = string.Empty;
  if (!SmoSkinDecoder.TryDecode(
       target, before, out SmoSkin? beforeSkin, out beforeError) ||
      beforeSkin is null ||
      !SmoSkinDecoder.TryDecode(
       output, after, out SmoSkin? afterSkin, out afterError) ||
      afterSkin is null)
  {
   throw new InvalidOperationException(
    $"{context}: inline target skin [{before.Index}] '{before.Name}' could not " +
    $"be decoded before/after: {beforeError} | {afterError}.");
  }
  if (beforeSkin.Bones.Count != afterSkin.Bones.Count)
  {
   throw new InvalidOperationException(
    $"{context}: inline target skin [{before.Index}] '{before.Name}' changed " +
    "palette size.");
  }
  foreach (SmoSkinBone beforeBone in beforeSkin.Bones.Where(
            bone => bone.InlineSerializedSize != 0))
  {
   SmoSkinBone afterBone = afterSkin.Bones[beforeBone.PaletteIndex];
   // InlineSerializedSize spans the nested node subtree and therefore changes
   // when descendant geometry is replaced. The palette invariant is that the
   // entry stays inline and keeps the exact target node and inverse bind.
   if (beforeBone.PaletteIndex != afterBone.PaletteIndex ||
       beforeBone.NodeObjectIndex != afterBone.NodeObjectIndex ||
       beforeBone.NodeObjectId != afterBone.NodeObjectId ||
       afterBone.InlineSerializedSize == 0 ||
       !MatrixApproximatelyEqual(
        beforeBone.InverseBindMatrix, afterBone.InverseBindMatrix, 0))
   {
    throw new InvalidOperationException(
     $"{context}: inline palette entry {beforeBone.PaletteIndex} in target skin " +
     $"[{before.Index}] '{before.Name}' changed " +
     $"(node {beforeBone.NodeObjectIndex}/{beforeBone.NodeObjectId} -> " +
     $"{afterBone.NodeObjectIndex}/{afterBone.NodeObjectId}; inline size " +
     $"{beforeBone.InlineSerializedSize} -> {afterBone.InlineSerializedSize}; " +
     $"IBM equal={MatrixApproximatelyEqual(beforeBone.InverseBindMatrix, afterBone.InverseBindMatrix, 0)}).");
   }
  }
 }
}

static Rgba32 ChooseUnusedSolidMarker(SmoDocument target)
{
 Rgba32[] candidates =
 [
  new(17, 91, 203, 137),
  new(211, 37, 9, 173),
  new(29, 223, 71, 199)
 ];
 foreach (Rgba32 candidate in candidates)
  if (!ContainsSolidRgbTexture(target, candidate))
   return candidate;
 throw new InvalidOperationException(
  "All deterministic external-catalog marker colors already exist in the target.");
}

static bool ContainsSolidRgbTexture(SmoDocument document, Rgba32 color)
{
 foreach (SmoObjectEntry entry in document.Objects.Where(item =>
           item.TypeHash == SmoClassIds.TextureData))
 {
  if (!SmoTextureDecoder.TryDecode(
       document, entry, out SmoTexture? texture, out _) || texture is null ||
      texture.Bgra32Pixels.Length == 0)
   continue;
  ReadOnlySpan<byte> pixels = texture.Bgra32Pixels.Span;
  bool solid = true;
  for (int offset = 0; solid && offset < pixels.Length; offset += 4)
  {
   solid = pixels[offset] == color.B &&
           pixels[offset + 1] == color.G &&
           pixels[offset + 2] == color.R;
  }
  if (solid)
   return true;
 }
 return false;
}

static byte[] EncodeSolidPng(Rgba32 color)
{
 using var image = new Image<Rgba32>(2, 2, color);
 using var output = new MemoryStream();
 image.SaveAsPng(output);
 return output.ToArray();
}

static void EnsureSeparateTestOutput(
 string targetPath,
 string donorPath,
 string outputPath)
{
 if (string.Equals(targetPath, outputPath, StringComparison.OrdinalIgnoreCase) ||
     string.Equals(donorPath, outputPath, StringComparison.OrdinalIgnoreCase))
  throw new InvalidOperationException("Porting test output must be a new file.");
}

static void VerifyTestSourcesUnchanged(
 string targetPath,
 byte[] targetBefore,
 string donorPath,
 byte[] donorBefore)
{
 if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBefore))
  throw new InvalidOperationException("Porting test modified the target SMO.");
 if (!File.ReadAllBytes(donorPath).SequenceEqual(donorBefore))
  throw new InvalidOperationException("Porting test modified the donor model.");
}

static void VerifyExplicitGeneratedAlignment(
 ImportedScene donor,
 ImportedScene fittingScene,
 ReplacementTransform alignment,
 string context)
{
 if (donor.Meshes.Count != fittingScene.Meshes.Count)
  throw new InvalidOperationException($"{context}: mesh count changed during alignment.");
 for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
 {
  ImportedMesh source = donor.Meshes[meshIndex];
  ImportedMesh fitting = fittingScene.Meshes[meshIndex];
  uint[] expectedRenderableIndices = FilterRenderableTriangleIndices(
   source, $"{context}: donor mesh [{meshIndex}] '{source.Name}'");
  if (source.Positions.Length != fitting.Positions.Length ||
      !source.Normals.SequenceEqual(fitting.Normals) ||
      !source.TextureCoordinates.SequenceEqual(fitting.TextureCoordinates) ||
      !expectedRenderableIndices.SequenceEqual(fitting.TriangleIndices) ||
      !source.DiffuseColors.SequenceEqual(fitting.DiffuseColors))
  {
   throw new InvalidOperationException(
    $"{context}: non-position attributes changed for mesh [{meshIndex}] '{source.Name}'.");
  }
  for (int vertex = 0; vertex < source.Positions.Length; vertex++)
  {
   Vector3 expected = source.Positions[vertex] * alignment.Scale +
    alignment.Translation;
   Vector3 actual = fitting.Positions[vertex];
   float tolerance = 0.00001f * MathF.Max(1f, expected.Length());
   if (!IsFinite(actual) ||
       Vector3.DistanceSquared(expected, actual) > tolerance * tolerance)
   {
    throw new InvalidOperationException(
     $"{context}: mesh [{meshIndex}] '{source.Name}' vertex {vertex} " +
     "does not contain the requested final alignment.");
   }
  }
 }
}

static uint[] FilterRenderableTriangleIndices(ImportedMesh mesh, string context)
{
 const float positionEpsilon = 0.000001f;
 if (mesh.Positions.Any(position => !IsFinite(position)))
  throw new InvalidOperationException($"{context}: non-finite position.");
 if (mesh.TriangleIndices.Length % 3 != 0)
  throw new InvalidOperationException($"{context}: incomplete triangle index list.");
 var result = new List<uint>(mesh.TriangleIndices.Length);
 for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
 {
  uint first = mesh.TriangleIndices[index];
  uint second = mesh.TriangleIndices[index + 1];
  uint third = mesh.TriangleIndices[index + 2];
  if (first >= mesh.Positions.Length ||
      second >= mesh.Positions.Length ||
      third >= mesh.Positions.Length)
  {
   throw new InvalidOperationException($"{context}: out-of-range triangle index.");
  }
  Vector3 a = mesh.Positions[(int)first];
  Vector3 b = mesh.Positions[(int)second];
  Vector3 c = mesh.Positions[(int)third];
  float area = Vector3.Cross(b - a, c - a).Length() * 0.5f;
  if (!float.IsFinite(area))
   throw new InvalidOperationException($"{context}: non-finite triangle area.");
  if (first == second || second == third || first == third ||
      area <= positionEpsilon * positionEpsilon)
  {
   continue;
  }
  result.Add(first);
  result.Add(second);
  result.Add(third);
 }
 return result.ToArray();
}

static void ValidateGeneratedPreparation(
 GeneratedSkinningPreparationResult preparation,
 TargetRigDefinition rig,
 string context)
{
 if (!preparation.Analysis.RequiresConfirmation)
  throw new InvalidOperationException($"{context}: generated weights do not require confirmation.");
 if (!float.IsFinite(preparation.Analysis.Alignment.Scale) ||
     preparation.Analysis.Alignment.Scale <= 0 ||
     !IsFinite(preparation.Analysis.Alignment.Translation))
  throw new InvalidOperationException($"{context}: generated alignment is invalid.");
 if (preparation.Analysis.PreparedVertexCount !=
     preparation.PreparedScene.Meshes.Sum(mesh => mesh.Positions.Length))
  throw new InvalidOperationException($"{context}: prepared vertex summary is inconsistent.");
 foreach (GeneratedSkinningAttachment attachment in preparation.Analysis.Attachments)
 {
  if (attachment.ComponentIndex < 0 || attachment.MeshIndices.Count == 0 ||
      attachment.MeshNames.Count == 0 || attachment.VertexCount <= 0 ||
      attachment.TriangleCount <= 0 || !float.IsFinite(attachment.DistanceToBone) ||
      attachment.DistanceToBone < 0 || !IsFinite(attachment.AlignedCenter))
   throw new InvalidOperationException($"{context}: invalid rigid attachment diagnostics.");
  TargetRigJoint targetJoint = rig.Joints[rig.GetJointIndex(attachment.TargetBoneName)];
  if (!targetJoint.IsDeformJoint)
   throw new InvalidOperationException(
    $"{context}: attachment targets non-deform joint '{attachment.TargetBoneName}'.");
 }
 ValidatePreparedTargetSkin(preparation.FittingPreviewScene, rig, context + " fitting");
 ValidatePreparedTargetSkin(preparation.PreparedScene, rig, context + " canonical");
}

static void ValidatePreparedTargetSkin(
 ImportedScene scene,
 TargetRigDefinition rig,
 string context)
{
 if (scene.Meshes.Count == 0)
  throw new InvalidOperationException($"{context}: prepared scene has no meshes.");
 TargetRigJoint[] deformJoints = rig.Joints.Where(joint => joint.IsDeformJoint).ToArray();
 if (deformJoints.Length == 0)
  throw new InvalidOperationException($"{context}: target has no deform joints.");
 foreach (ImportedMesh mesh in scene.Meshes)
 {
  ImportedSkinning skinning = mesh.Skinning ?? throw new InvalidOperationException(
   $"{context}: mesh '{mesh.Name}' has no target skinning.");
  if (mesh.Positions.Length == 0 || mesh.TriangleIndices.Length % 3 != 0 ||
      mesh.TriangleIndices.Any(index => index >= mesh.Positions.Length) ||
      skinning.JointIndices.Length != mesh.Positions.Length ||
      skinning.Weights.Length != mesh.Positions.Length ||
      mesh.Positions.Any(position => !IsFinite(position)) ||
      mesh.Normals.Length != 0 &&
      (mesh.Normals.Length != mesh.Positions.Length ||
       mesh.Normals.Any(normal => !IsFinite(normal))))
  {
   throw new InvalidOperationException($"{context}: mesh '{mesh.Name}' attributes are invalid.");
  }
  ImportedSkeleton skeleton = skinning.Skeleton;
  if (!skeleton.JointNames.SequenceEqual(
       deformJoints.Select(joint => joint.Name), StringComparer.Ordinal) ||
      skeleton.InverseBindMatrices.Count != deformJoints.Length)
  {
   throw new InvalidOperationException(
    $"{context}: mesh '{mesh.Name}' does not use the exact target deform skeleton.");
  }
  for (int joint = 0; joint < deformJoints.Length; joint++)
  {
   if (!Matrix4x4.Invert(
        deformJoints[joint].BindWorldMatrix, out Matrix4x4 expectedInverseBind) ||
       !MatrixApproximatelyEqual(
        expectedInverseBind, skeleton.InverseBindMatrices[joint], 0.0001f))
   {
    throw new InvalidOperationException(
     $"{context}: joint '{deformJoints[joint].Name}' has a non-target inverse bind.");
   }
  }
  for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
  {
   Vector4 weightVector = skinning.Weights[vertex];
   ImportedJointIndices jointVector = skinning.JointIndices[vertex];
   float[] weights = [weightVector.X, weightVector.Y, weightVector.Z, weightVector.W];
   ushort[] joints = [jointVector.X, jointVector.Y, jointVector.Z, jointVector.W];
   float total = 0;
   var activeJoints = new HashSet<ushort>();
   for (int influence = 0; influence < 4; influence++)
   {
    float weight = weights[influence];
    if (!float.IsFinite(weight) || weight < 0)
     throw new InvalidOperationException(
      $"{context}: mesh '{mesh.Name}' vertex {vertex} has an invalid weight.");
    if (weight <= 0.000001f)
     continue;
    if (joints[influence] >= skeleton.JointNames.Count ||
        !activeJoints.Add(joints[influence]))
    {
     throw new InvalidOperationException(
      $"{context}: mesh '{mesh.Name}' vertex {vertex} has invalid target influences.");
    }
    total += weight;
   }
   if (!float.IsFinite(total) || MathF.Abs(total - 1f) > 0.0001f)
    throw new InvalidOperationException(
     $"{context}: mesh '{mesh.Name}' vertex {vertex} weights sum to {total:G9}.");
  }
 }
}

static void VerifySkinnedWriterTargetGraph(SmoDocument target, string outputPath)
{
 SmoDocument output = SmoDocument.Load(outputPath);
 if (output.HasErrors || output.Objects.Count != target.Objects.Count)
  throw new InvalidOperationException("Porting output failed strict target-graph verification.");
 for (int index = 0; index < target.Objects.Count; index++)
 {
  SmoObjectEntry before = target.Objects[index];
  SmoObjectEntry after = output.Objects[index];
  if (before.Id != after.Id || before.Name != after.Name ||
      before.TypeHash != after.TypeHash || before.ParentIndex != after.ParentIndex ||
      before.NestingDepth != after.NestingDepth)
   throw new InvalidOperationException($"Porting output changed target object [{index}].");
 }
 foreach (SmoObjectEntry skinEntry in output.Objects.Where(entry =>
           entry.TypeHash == SmoClassIds.Skin))
 {
  if (!SmoSkinDecoder.TryDecode(
       output, skinEntry, out SmoSkin? skin, out string error) || skin is null)
   throw new InvalidOperationException(
    $"Porting output skin [{skinEntry.Index}] is invalid: {error}");
  foreach (SmoSkinBone bone in skin.Bones)
  {
   if ((uint)bone.NodeObjectIndex >= (uint)target.Objects.Count ||
       target.Objects[bone.NodeObjectIndex].TypeHash != SmoClassIds.Node)
    throw new InvalidOperationException(
     $"Porting output skin [{skinEntry.Index}] references a non-target node.");
  }
 }
}

static bool SceneSkinGeometryExactlyEqual(ImportedScene left, ImportedScene right)
{
 if (left.Meshes.Count != right.Meshes.Count)
  return false;
 for (int index = 0; index < left.Meshes.Count; index++)
 {
  ImportedMesh a = left.Meshes[index];
  ImportedMesh b = right.Meshes[index];
  if (a.Name != b.Name || a.MaterialIndex != b.MaterialIndex ||
      !a.Positions.SequenceEqual(b.Positions) ||
      !a.Normals.SequenceEqual(b.Normals) ||
      !a.TextureCoordinates.SequenceEqual(b.TextureCoordinates) ||
      !a.TriangleIndices.SequenceEqual(b.TriangleIndices) ||
      !a.DiffuseColors.SequenceEqual(b.DiffuseColors) ||
      a.Skinning is null || b.Skinning is null ||
      !a.Skinning.JointIndices.SequenceEqual(b.Skinning.JointIndices) ||
      !a.Skinning.Weights.SequenceEqual(b.Skinning.Weights) ||
      !a.Skinning.Skeleton.JointNames.SequenceEqual(
       b.Skinning.Skeleton.JointNames, StringComparer.Ordinal) ||
      !a.Skinning.Skeleton.InverseBindMatrices.SequenceEqual(
       b.Skinning.Skeleton.InverseBindMatrices))
   return false;
 }
 return true;
}

static SmoObjectEntry FindLargestSiblingMeshMaterial(SmoDocument document)
{
 SmoObjectEntry? material = document.Objects
  .Where(entry => entry.TypeHash == SmoClassIds.MaterialData &&
                  entry.ParentIndex.HasValue)
  .Select(entry => new
  {
   Material = entry,
   LargestSiblingMesh = document.Objects
    .Where(candidate => candidate.ParentIndex == entry.ParentIndex &&
                        candidate.TypeHash == SmoClassIds.MeshData)
    .Select(candidate => SmoMeshDecoder.Decode(document, candidate).VertexCount)
    .DefaultIfEmpty(-1)
    .Max()
  })
  .OrderByDescending(item => item.LargestSiblingMesh)
  .ThenBy(item => item.Material.Index)
  .Select(item => item.Material)
  .FirstOrDefault();
 return material ?? throw new InvalidOperationException(
  "No material with a sibling mesh was found.");
}

static (
 uint FinalBlendOperation,
 uint[] MaterialRenderStates,
 uint[] LayerTextureStates) ReadNativeMaterialState(
 SmoDocument document,
 SmoObjectEntry material,
 string context)
{
 ReadOnlySpan<byte> bytes = document.Data.Span.Slice(
  checked((int)material.PhysicalOffset),
  checked((int)material.SerializedSize));
 uint? operation = null;
 uint[]? materialStates = null;
 uint[]? layerStates = null;
 int offset = 8;
 while (offset < bytes.Length &&
        SmoDataBlockReader.TryReadHeader(
         bytes, offset, out SmoDataBlockHeader field))
 {
  if (materialStates is null &&
      field.FieldType == 0 && field.PayloadSize == 11 * sizeof(uint))
  {
   materialStates = new uint[11];
   for (int index = 0; index < materialStates.Length; index++)
    materialStates[index] =
     System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
      bytes[(field.PayloadOffset + index * sizeof(uint))..]);
  }
  else if (!operation.HasValue &&
           field.FieldType == 3 && field.PayloadSize == sizeof(uint))
  {
   operation = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
    bytes[field.PayloadOffset..]);
  }
  else if (layerStates is null &&
           field.FieldType == 17 && field.PayloadSize == 9 * sizeof(uint))
  {
   layerStates = new uint[9];
   for (int index = 0; index < layerStates.Length; index++)
    layerStates[index] =
     System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(
      bytes[(field.PayloadOffset + index * sizeof(uint))..]);
  }
  offset = checked((int)field.PayloadEnd);
 }
 if (!operation.HasValue || materialStates is null || layerStates is null)
  throw new InvalidOperationException(
   $"{context}: material [{material.Index}] lacks the complete native render state.");
 return (operation.Value, materialStates, layerStates);
}

static bool HasGeometryDifference(ImportedScene left, ImportedScene right)
{
 if (left.Meshes.Count != right.Meshes.Count)
  return true;
 for (int mesh = 0; mesh < left.Meshes.Count; mesh++)
 {
  if (left.Meshes[mesh].Positions.Length != right.Meshes[mesh].Positions.Length)
   return true;
  for (int vertex = 0; vertex < left.Meshes[mesh].Positions.Length; vertex++)
   if (Vector3.DistanceSquared(
        left.Meshes[mesh].Positions[vertex],
        right.Meshes[mesh].Positions[vertex]) > 1e-12f)
    return true;
 }
 return false;
}

static bool IsFinite(Vector3 value) =>
 float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);

static Vector3 TranslationAt(
 TargetRigFittingPoseSnapshot pose,
 TargetRigDefinition rig,
 string jointName)
{
 Matrix4x4 matrix = pose.WorldMatrices[rig.GetJointIndex(jointName)];
 return new Vector3(matrix.M41, matrix.M42, matrix.M43);
}

static bool SameBodySelectionMembership(
 TargetRigBodySelection left,
 TargetRigBodySelection right)
{
 if (left.TotalComponentCount != right.TotalComponentCount ||
     left.ExcludedComponentCount != right.ExcludedComponentCount ||
     left.TargetRigFingerprint != right.TargetRigFingerprint ||
     left.DonorGeometryFingerprint != right.DonorGeometryFingerprint ||
     left.DonorAlignment != right.DonorAlignment ||
     left.Components.Count != right.Components.Count)
  return false;

 for (int componentIndex = 0;
      componentIndex < left.Components.Count;
      componentIndex++)
 {
  TargetRigSelectedBodyComponent a = left.Components[componentIndex];
  TargetRigSelectedBodyComponent b = right.Components[componentIndex];
  if (a.ComponentIndex != b.ComponentIndex ||
      a.Role != b.Role ||
      a.VerticesByMesh.Count != b.VerticesByMesh.Count)
   return false;
  for (int meshIndex = 0;
       meshIndex < a.VerticesByMesh.Count;
       meshIndex++)
  {
   TargetRigBodyVertexMembership aMembership = a.VerticesByMesh[meshIndex];
   TargetRigBodyVertexMembership bMembership = b.VerticesByMesh[meshIndex];
   if (aMembership.MeshIndex != bMembership.MeshIndex ||
       aMembership.MeshName != bMembership.MeshName ||
       !aMembership.VertexIndices.SequenceEqual(bMembership.VertexIndices))
    return false;
  }
 }
 return true;
}

static Quaternion WorldRotationAt(
 TargetRigFittingPoseSnapshot pose,
 TargetRigDefinition rig,
 string jointName)
{
 Matrix4x4 matrix = pose.WorldMatrices[rig.GetJointIndex(jointName)];
 if (!Matrix4x4.Decompose(
      matrix,
      out Vector3 scale,
      out Quaternion rotation,
      out Vector3 translation) ||
     !IsFinite(scale) || !IsFinite(translation) ||
     !float.IsFinite(rotation.X) || !float.IsFinite(rotation.Y) ||
     !float.IsFinite(rotation.Z) || !float.IsFinite(rotation.W) ||
     rotation.LengthSquared() <= 0.000001f)
 {
  throw new InvalidOperationException(
   $"World transform of {jointName} has no finite rotation.");
 }
 return Quaternion.Normalize(rotation);
}

static void VerifyTargetRigPoseLengths(
 TargetRigDefinition rig,
 TargetRigFittingPoseSnapshot pose,
 string context)
{
 foreach (TargetRigJoint joint in rig.Joints.Where(joint => joint.ParentJointIndex >= 0))
 {
  Matrix4x4 parentMatrix = pose.WorldMatrices[joint.ParentJointIndex];
  Matrix4x4 jointMatrix = pose.WorldMatrices[joint.JointIndex];
  var parent = new Vector3(parentMatrix.M41, parentMatrix.M42, parentMatrix.M43);
  var child = new Vector3(jointMatrix.M41, jointMatrix.M42, jointMatrix.M43);
  float actual = Vector3.Distance(parent, child);
  float tolerance = 0.0001f * MathF.Max(1, joint.BindLengthFromParent);
  if (!float.IsFinite(actual) || MathF.Abs(actual - joint.BindLengthFromParent) > tolerance)
   throw new InvalidOperationException(
    $"{context} changed length of {joint.Name}: " +
    $"bind={joint.BindLengthFromParent}, posed={actual}.");
 }
}

static bool MatrixApproximatelyEqual(
 Matrix4x4 left,
 Matrix4x4 right,
 float epsilon) =>
 MathF.Abs(left.M11 - right.M11) <= epsilon &&
 MathF.Abs(left.M12 - right.M12) <= epsilon &&
 MathF.Abs(left.M13 - right.M13) <= epsilon &&
 MathF.Abs(left.M14 - right.M14) <= epsilon &&
 MathF.Abs(left.M21 - right.M21) <= epsilon &&
 MathF.Abs(left.M22 - right.M22) <= epsilon &&
 MathF.Abs(left.M23 - right.M23) <= epsilon &&
 MathF.Abs(left.M24 - right.M24) <= epsilon &&
 MathF.Abs(left.M31 - right.M31) <= epsilon &&
 MathF.Abs(left.M32 - right.M32) <= epsilon &&
 MathF.Abs(left.M33 - right.M33) <= epsilon &&
 MathF.Abs(left.M34 - right.M34) <= epsilon &&
 MathF.Abs(left.M41 - right.M41) <= epsilon &&
 MathF.Abs(left.M42 - right.M42) <= epsilon &&
 MathF.Abs(left.M43 - right.M43) <= epsilon &&
 MathF.Abs(left.M44 - right.M44) <= epsilon;
