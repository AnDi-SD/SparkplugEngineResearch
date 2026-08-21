using System.Collections.ObjectModel;
using System.Numerics;
using SmoViewer.Core;

namespace SmoImporter.Core;

public sealed record SkinnedModelPortingJointMapping(
    string DonorSkeletonName,
    string DonorJointName,
    string TargetJointName,
    string MatchKind);

public sealed record SkinnedModelPortingAnalysis(
    ModelPortingMode Mode,
    SmoSkeletonCompatibility Compatibility,
    int MeshCount,
    int DonorSkeletonCount,
    int ActiveDonorJointCount,
    int TargetDeformJointCount,
    IReadOnlyList<SkinnedModelPortingJointMapping> JointMappings,
    IReadOnlyList<string> UnusedDonorJoints,
    IReadOnlyList<string> TargetJointsWithoutWeights,
    IReadOnlyList<string> Errors,
    IReadOnlyList<string> Warnings)
{
    public bool CanPrepare =>
        Mode == ModelPortingMode.AdaptDonorWeights &&
        Compatibility != SmoSkeletonCompatibility.Incompatible;

    public IReadOnlyList<string> Messages =>
        Array.AsReadOnly(Errors.Concat(Warnings).ToArray());
}

public sealed record SkinnedModelPortingPreparation(
    ModelPortingMode Mode,
    SkinnedModelPortingAnalysis Analysis,
    ImportedScene PreparedScene)
{
    /// <summary>
    /// Static geometry arranged around the temporary fitting pose. Legacy
    /// preparations have no separate fitting phase, so this defaults to the
    /// canonical prepared scene.
    /// </summary>
    public ImportedScene FittingPreviewScene { get; init; } = PreparedScene;
}

/// <summary>
/// Converts a donor skin to the canonical external-space bind pose of the
/// target game's deform rig. This class deliberately implements only
/// <see cref="ModelPortingMode.AdaptDonorWeights"/>: it consumes existing donor
/// weights and never invents weights for an unskinned mesh.
/// </summary>
public static class SkinnedModelPortingPreparer
{
    private const float WeightEpsilon = 0.000001f;

    // These aliases are intentionally explicit. They cover common unambiguous
    // humanoid tokens and the inspected local-data/Текна ориг.glb skeleton.
    // Unknown active joints are not guessed from spatial proximity.
    private static readonly IReadOnlyDictionary<string, string> HumanoidAliases =
        new ReadOnlyDictionary<string, string>(
            new Dictionary<string, string>(StringComparer.Ordinal)
            {
                ["root"] = "Pelvis",
                ["hip"] = "Pelvis",
                ["hips"] = "Pelvis",
                ["rootjoint"] = "Pelvis",
                ["rootjointl1"] = "Pelvis",
                ["worldroot"] = "Pelvis",

                ["spine"] = "Spine_01",
                ["spine1"] = "Spine_01",
                ["spine01"] = "Spine_01",
                ["spine2"] = "Spine_02",
                ["spine02"] = "Spine_02",
                ["spine3"] = "Spine_03",
                ["spine03"] = "Spine_03",
                ["chest"] = "Spine_03",
                ["upperchest"] = "Spine_03",
                ["l1spine02"] = "Spine_02",
                ["neck"] = "Neck",
                ["head"] = "Head",
                ["face"] = "Head",
                ["l1face"] = "Head",

                ["leftshoulder"] = "L_Clavicle",
                ["lshoulder"] = "L_Clavicle",
                ["leftclavicle"] = "L_Clavicle",
                ["lclavicle"] = "L_Clavicle",
                ["leftarm"] = "L_Bicep",
                ["leftupperarm"] = "L_Bicep",
                ["luparm"] = "L_Bicep",
                ["l1luparm"] = "L_Bicep",
                ["leftforearm"] = "L_UpperArm",
                ["leftlowerarm"] = "L_UpperArm",
                ["lforearm"] = "L_UpperArm",
                ["l1lforearm"] = "L_UpperArm",
                ["lefthand"] = "L_Hand",
                ["lhand"] = "L_Hand",

                ["rightshoulder"] = "R_Clavicle",
                ["rshoulder"] = "R_Clavicle",
                ["rightclavicle"] = "R_Clavicle",
                ["rclavicle"] = "R_Clavicle",
                ["rightarm"] = "R_Bicep",
                ["rightupperarm"] = "R_Bicep",
                ["ruparm"] = "R_Bicep",
                ["l1ruparm"] = "R_Bicep",
                ["rightforearm"] = "R_UpperArm",
                ["rightlowerarm"] = "R_UpperArm",
                ["rforearm"] = "R_UpperArm",
                ["l1rforearm"] = "R_UpperArm",
                ["righthand"] = "R_Hand",
                ["rhand"] = "R_Hand",

                ["leftupleg"] = "L_Thigh",
                ["leftthigh"] = "L_Thigh",
                ["lthigh"] = "L_Thigh",
                ["l1lthigh"] = "L_Thigh",
                ["leftleg"] = "L_calf",
                ["leftlowerleg"] = "L_calf",
                ["leftcalf"] = "L_calf",
                ["lcalf"] = "L_calf",
                ["l1lcalf"] = "L_calf",
                ["leftfoot"] = "L_Ankle",
                ["leftankle"] = "L_Ankle",
                ["lfoot"] = "L_Ankle",
                ["lankle"] = "L_Ankle",
                ["lefttoe"] = "L_Toe",
                ["lefttoebase"] = "L_Toe",
                ["ltoe"] = "L_Toe",

                ["rightupleg"] = "R_Thigh",
                ["rightthigh"] = "R_Thigh",
                ["rthigh"] = "R_Thigh",
                ["l1rthigh"] = "R_Thigh",
                ["rightleg"] = "R_calf",
                ["rightlowerleg"] = "R_calf",
                ["rightcalf"] = "R_calf",
                ["rcalf"] = "R_calf",
                ["l1rcalf"] = "R_calf",
                ["rightfoot"] = "R_Ankle",
                ["rightankle"] = "R_Ankle",
                ["rfoot"] = "R_Ankle",
                ["rankle"] = "R_Ankle",
                ["righttoe"] = "R_Toe",
                ["righttoebase"] = "R_Toe",
                ["rtoe"] = "R_Toe"
            });

    public static SkinnedModelPortingAnalysis AnalyzeAdaptDonorWeights(
        SmoDocument target,
        ImportedScene donor)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        return BuildPlan(target, donor).Analysis;
    }

    public static SkinnedModelPortingPreparation PrepareAdaptDonorWeights(
        SmoDocument target,
        ImportedScene donor)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);

        PreparationPlan plan = BuildPlan(target, donor);
        if (!plan.Analysis.CanPrepare || plan.TargetSkeleton is null)
        {
            string details = plan.Analysis.Errors.Count == 0
                ? "The donor cannot be prepared in AdaptDonorWeights mode."
                : string.Join(Environment.NewLine, plan.Analysis.Errors);
            throw new InvalidOperationException(details);
        }

        var preparedMeshes = new ImportedMesh[donor.Meshes.Count];
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
            preparedMeshes[meshIndex] = PrepareMesh(
                donor.Meshes[meshIndex], plan.TargetSkeleton, plan.SkeletonPlans);

        ImportedTexture[] textures = donor.Textures
            .Select(texture => texture with { Data = texture.Data.ToArray() })
            .ToArray();
        ImportedMaterial[] materials = donor.Materials.ToArray();
        var preparedScene = new ImportedScene(
            Array.AsReadOnly(preparedMeshes),
            Array.AsReadOnly(textures),
            Array.AsReadOnly(materials));
        return new SkinnedModelPortingPreparation(
            ModelPortingMode.AdaptDonorWeights,
            plan.Analysis,
            preparedScene);
    }

    /// <summary>
    /// Uses donor weights and semantic joint mapping, but deliberately ignores
    /// donor inverse-bind matrices for geometry. The aligned donor is treated as
    /// authored around <paramref name="fittingPose"/>, then safely baked back to
    /// the immutable canonical target bind pose.
    /// </summary>
    public static SkinnedModelPortingPreparation PrepareAdaptDonorWeights(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose) =>
        PrepareAdaptDonorWeights(
            target,
            donor,
            fittingPose,
            ReplacementTransform.Identity);

    /// <summary>
    /// Pose-aware mode-2 preparation with an explicit uniform donor alignment.
    /// Matrices use the importer row-vector external space. The fitting pose is
    /// temporary; output joint names and inverse binds remain the target's
    /// canonical values.
    /// </summary>
    public static SkinnedModelPortingPreparation PrepareAdaptDonorWeights(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform donorAlignment)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        ArgumentNullException.ThrowIfNull(fittingPose);
        ArgumentNullException.ThrowIfNull(donorAlignment);
        fittingPose.ValidateForTarget(target);

        Matrix4x4 alignment = donorAlignment.Matrix;
        if (!float.IsFinite(donorAlignment.Scale) || donorAlignment.Scale <= 0 ||
            !IsFinite(alignment) ||
            !Matrix4x4.Invert(alignment, out Matrix4x4 inverseAlignment) ||
            !IsFinite(inverseAlignment))
        {
            throw new ArgumentException(
                "Donor fitting alignment must be finite, invertible, and have " +
                "a positive uniform scale.",
                nameof(donorAlignment));
        }
        Matrix4x4 normalAlignment = Matrix4x4.Transpose(inverseAlignment);

        PreparationPlan plan = BuildPlan(
            target,
            donor,
            fittingPose.Definition,
            useDonorBindGeometry: false);
        if (!plan.Analysis.CanPrepare || plan.TargetSkeleton is null)
        {
            string details = plan.Analysis.Errors.Count == 0
                ? "The donor cannot be prepared in AdaptDonorWeights mode."
                : string.Join(Environment.NewLine, plan.Analysis.Errors);
            throw new InvalidOperationException(details);
        }

        ImportedMesh[] fittingMeshes = donor.Meshes
            .Select(mesh => PrepareFittingMesh(
                mesh,
                plan.TargetSkeleton,
                plan.SkeletonPlans,
                alignment,
                normalAlignment))
            .ToArray();
        ImportedTexture[] textures = donor.Textures
            .Select(texture => texture with { Data = texture.Data.ToArray() })
            .ToArray();
        ImportedMaterial[] materials = donor.Materials.ToArray();
        var fittingScene = new ImportedScene(
            Array.AsReadOnly(fittingMeshes),
            Array.AsReadOnly(textures),
            Array.AsReadOnly(materials));
        ImportedScene preparedScene = TargetRigFittingSkinBaker.BakeToCanonical(
            fittingScene,
            fittingPose);
        return new SkinnedModelPortingPreparation(
            ModelPortingMode.AdaptDonorWeights,
            plan.Analysis,
            preparedScene)
        {
            FittingPreviewScene = fittingScene
        };
    }

    private static PreparationPlan BuildPlan(
        SmoDocument target,
        ImportedScene donor,
        TargetRigDefinition? suppliedTargetRig = null,
        bool useDonorBindGeometry = true)
    {
        var errors = new List<string>();
        var warnings = new List<string>();
        var mappings = new List<SkinnedModelPortingJointMapping>();
        var unusedDonorJoints = new List<string>();
        var targetJointsWithoutWeights = new List<string>();
        var skeletonUses = new Dictionary<ImportedSkeleton, SkeletonUse>(
            ReferenceEqualityComparer.Instance);

        if (donor.Meshes.Count == 0)
            errors.Add("The donor scene contains no meshes.");

        var validatedSkeletons = new HashSet<ImportedSkeleton>(
            ReferenceEqualityComparer.Instance);
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = donor.Meshes[meshIndex];
            ValidateMesh(mesh, meshIndex, errors, warnings);
            if (mesh.Skinning is null)
            {
                errors.Add(
                    $"Mesh [{meshIndex}] '{mesh.Name}' has no skin. " +
                    "AdaptDonorWeights never generates missing weights.");
                continue;
            }

            ImportedSkeleton skeleton = mesh.Skinning.Skeleton;
            if (validatedSkeletons.Add(skeleton))
                ValidateSkeleton(
                    skeleton,
                    errors,
                    requireInverseBinds: useDonorBindGeometry);
            if (!skeletonUses.TryGetValue(skeleton, out SkeletonUse? use))
            {
                use = new SkeletonUse(skeleton);
                skeletonUses.Add(skeleton, use);
            }
            CollectActiveJoints(mesh, meshIndex, use.ActiveJointIndices, errors);
        }

        TargetRigDefinition? targetRig = null;
        ImportedSkeleton? targetSkeleton = null;
        try
        {
            targetRig = suppliedTargetRig ?? TargetRigDefinition.FromSmoDocument(target);
            targetSkeleton = BuildCanonicalTargetSkeleton(targetRig);
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          ArgumentException or
                                          OverflowException)
        {
            errors.Add("The target SMO cannot define a canonical fitting rig: " +
                       exception.Message);
        }

        var skeletonPlans = new Dictionary<ImportedSkeleton, SkeletonMappingPlan>(
            ReferenceEqualityComparer.Instance);
        var usedTargetIndices = new HashSet<int>();
        int nonExactMappingCount = 0;
        if (targetSkeleton is not null)
        {
            BuildTargetLookups(
                targetSkeleton,
                errors,
                out Dictionary<string, int> targetByExactName,
                out Dictionary<string, int> targetByNormalizedName);

            foreach (SkeletonUse use in skeletonUses.Values)
            {
                var mappingPlan = new SkeletonMappingPlan(use.Skeleton.JointNames.Count);
                skeletonPlans.Add(use.Skeleton, mappingPlan);
                var activeNames = new HashSet<string>(
                    StringComparer.OrdinalIgnoreCase);
                foreach (int donorJointIndex in use.ActiveJointIndices.Order())
                {
                    if (donorJointIndex < 0 ||
                        donorJointIndex >= use.Skeleton.JointNames.Count ||
                        useDonorBindGeometry &&
                        donorJointIndex >= use.Skeleton.InverseBindMatrices.Count)
                    {
                        continue;
                    }

                    string donorJointName = use.Skeleton.JointNames[donorJointIndex];
                    if (string.IsNullOrWhiteSpace(donorJointName))
                    {
                        errors.Add(
                            $"Active donor joint {donorJointIndex} in skeleton " +
                            $"'{use.Skeleton.Name}' has no semantic name.");
                        continue;
                    }
                    if (!activeNames.Add(donorJointName))
                    {
                        errors.Add(
                            $"Skeleton '{use.Skeleton.Name}' has more than one active " +
                            $"joint named '{donorJointName}'. Semantic mapping is " +
                            "ambiguous.");
                        continue;
                    }
                    if (!TryResolveTargetJoint(
                            donorJointName,
                            targetByExactName,
                            targetByNormalizedName,
                            out int targetJointIndex,
                            out string matchKind,
                            out string? resolutionError))
                    {
                        errors.Add(
                            $"Active donor joint '{donorJointName}' in skeleton " +
                            $"'{use.Skeleton.Name}' is incompatible: {resolutionError}");
                        continue;
                    }

                    Matrix4x4 rebase = Matrix4x4.Identity;
                    Matrix4x4 normalMatrix = Matrix4x4.Identity;
                    if (useDonorBindGeometry)
                    {
                        Matrix4x4 donorInverseBind =
                            use.Skeleton.InverseBindMatrices[donorJointIndex];
                        Matrix4x4 targetBind =
                            targetSkeleton.BindWorldMatrices![targetJointIndex];
                        rebase = donorInverseBind * targetBind;
                        if (!IsFinite(rebase) ||
                            !Matrix4x4.Invert(
                                rebase,
                                out Matrix4x4 inverseRebase) ||
                            !IsFinite(inverseRebase))
                        {
                            errors.Add(
                                $"Rebase matrix donor '{donorJointName}' -> target " +
                                $"'{targetSkeleton.JointNames[targetJointIndex]}' is " +
                                "singular or non-finite.");
                            continue;
                        }
                        normalMatrix = Matrix4x4.Transpose(inverseRebase);
                    }

                    mappingPlan.TargetJointIndices[donorJointIndex] = targetJointIndex;
                    mappingPlan.RebaseMatrices[donorJointIndex] = rebase;
                    mappingPlan.NormalMatrices[donorJointIndex] = normalMatrix;
                    usedTargetIndices.Add(targetJointIndex);
                    mappings.Add(new SkinnedModelPortingJointMapping(
                        use.Skeleton.Name,
                        donorJointName,
                        targetSkeleton.JointNames[targetJointIndex],
                        matchKind));
                    if (matchKind != "exact target name")
                        nonExactMappingCount++;
                }

                foreach (int inactiveIndex in Enumerable.Range(
                             0, use.Skeleton.JointNames.Count)
                         .Except(use.ActiveJointIndices))
                {
                    unusedDonorJoints.Add(
                        $"{use.Skeleton.Name}: " +
                        use.Skeleton.JointNames[inactiveIndex]);
                }

                var collapsedTargets = use.ActiveJointIndices
                    .Where(index => index >= 0 &&
                                    index < mappingPlan.TargetJointIndices.Length &&
                                    mappingPlan.TargetJointIndices[index] >= 0)
                    .GroupBy(index => mappingPlan.TargetJointIndices[index])
                    .Where(group => group.Count() > 1)
                    .ToArray();
                foreach (IGrouping<int, int> collapsed in collapsedTargets)
                {
                    string donorNames = string.Join(
                        ", ",
                        collapsed.Select(index => use.Skeleton.JointNames[index]));
                    warnings.Add(
                        $"Skeleton '{use.Skeleton.Name}' donor joints [{donorNames}] " +
                        $"all map to target '{targetSkeleton.JointNames[collapsed.Key]}'; " +
                        "their resulting vertex weights will be summed.");
                }
            }

            for (int targetJointIndex = 0;
                 targetJointIndex < targetSkeleton.JointNames.Count;
                 targetJointIndex++)
            {
                if (!usedTargetIndices.Contains(targetJointIndex))
                    targetJointsWithoutWeights.Add(
                        targetSkeleton.JointNames[targetJointIndex]);
            }
        }

        if (nonExactMappingCount > 0)
        {
            warnings.Add(
                $"{nonExactMappingCount} active donor joint mapping(s) use " +
                "case/punctuation normalization or a reviewed humanoid alias.");
        }
        if (targetJointsWithoutWeights.Count > 0)
        {
            warnings.Add(
                $"{targetJointsWithoutWeights.Count} target deform joint(s) receive " +
                "no donor weights. They remain in the canonical prepared skeleton.");
        }

        SmoSkeletonCompatibility compatibility = errors.Count > 0
            ? SmoSkeletonCompatibility.Incompatible
            : warnings.Count > 0
                ? SmoSkeletonCompatibility.CompatibleWithWarnings
                : SmoSkeletonCompatibility.Exact;
        int targetDeformCount = targetSkeleton?.JointNames.Count ??
                                targetRig?.DeformJointCount ?? 0;
        var analysis = new SkinnedModelPortingAnalysis(
            ModelPortingMode.AdaptDonorWeights,
            compatibility,
            donor.Meshes.Count,
            skeletonUses.Count,
            skeletonUses.Values.Sum(use => use.ActiveJointIndices.Count),
            targetDeformCount,
            Array.AsReadOnly(mappings.ToArray()),
            Array.AsReadOnly(unusedDonorJoints.Distinct(StringComparer.Ordinal).ToArray()),
            Array.AsReadOnly(targetJointsWithoutWeights.ToArray()),
            Array.AsReadOnly(errors.ToArray()),
            Array.AsReadOnly(warnings.ToArray()));
        return new PreparationPlan(analysis, targetSkeleton, skeletonPlans);
    }

    private static ImportedSkeleton BuildCanonicalTargetSkeleton(
        TargetRigDefinition targetRig)
    {
        TargetRigJoint[] deformJoints = targetRig.Joints
            .Where(joint => joint.IsDeformJoint)
            .OrderBy(joint => joint.JointIndex)
            .ToArray();
        if (deformJoints.Length == 0)
            throw new InvalidDataException(
                "The target rig has no deform joints referenced by skin palettes.");
        if (deformJoints.Length > ushort.MaxValue)
            throw new InvalidDataException(
                $"The target rig has {deformJoints.Length} deform joints; " +
                $"the imported skin representation supports at most {ushort.MaxValue}.");

        var compactByRigIndex = deformJoints
            .Select((joint, compactIndex) => (joint.JointIndex, compactIndex))
            .ToDictionary(pair => pair.JointIndex, pair => pair.compactIndex);
        var names = new string[deformJoints.Length];
        var inverseBinds = new Matrix4x4[deformJoints.Length];
        var bindWorlds = new Matrix4x4[deformJoints.Length];
        var bindLocals = new Matrix4x4[deformJoints.Length];
        var parentIndices = new int[deformJoints.Length];

        for (int compactIndex = 0; compactIndex < deformJoints.Length; compactIndex++)
        {
            TargetRigJoint joint = deformJoints[compactIndex];
            names[compactIndex] = joint.Name;
            bindWorlds[compactIndex] = joint.BindWorldMatrix;
            if (!Matrix4x4.Invert(
                    joint.BindWorldMatrix, out inverseBinds[compactIndex]) ||
                !IsFinite(inverseBinds[compactIndex]))
            {
                throw new InvalidDataException(
                    $"Target deform joint '{joint.Name}' has a singular bind matrix.");
            }

            int parentRigIndex = joint.ParentJointIndex;
            while (parentRigIndex >= 0 &&
                   !compactByRigIndex.ContainsKey(parentRigIndex))
            {
                parentRigIndex = targetRig.Joints[parentRigIndex].ParentJointIndex;
            }
            int parentCompactIndex = parentRigIndex < 0
                ? -1
                : compactByRigIndex[parentRigIndex];
            parentIndices[compactIndex] = parentCompactIndex;

            if (parentCompactIndex < 0)
            {
                bindLocals[compactIndex] = joint.BindWorldMatrix;
            }
            else
            {
                Matrix4x4 parentWorld = bindWorlds[parentCompactIndex];
                if (!Matrix4x4.Invert(parentWorld, out Matrix4x4 inverseParent) ||
                    !IsFinite(inverseParent))
                {
                    throw new InvalidDataException(
                        $"Target parent bind for '{joint.Name}' is singular.");
                }
                bindLocals[compactIndex] = joint.BindWorldMatrix * inverseParent;
            }
            if (!IsFinite(bindLocals[compactIndex]))
                throw new InvalidDataException(
                    $"Target local bind for '{joint.Name}' is non-finite.");
        }

        return new ImportedSkeleton(
            "SMO target canonical deform rig",
            Array.AsReadOnly(names),
            Array.AsReadOnly(inverseBinds))
        {
            ParentJointIndices = Array.AsReadOnly(parentIndices),
            BindWorldMatrices = Array.AsReadOnly(bindWorlds),
            BindLocalMatrices = Array.AsReadOnly(bindLocals)
        };
    }

    private static void BuildTargetLookups(
        ImportedSkeleton targetSkeleton,
        List<string> errors,
        out Dictionary<string, int> targetByExactName,
        out Dictionary<string, int> targetByNormalizedName)
    {
        targetByExactName = new Dictionary<string, int>(StringComparer.Ordinal);
        var normalizedGroups = new Dictionary<string, List<int>>(StringComparer.Ordinal);
        for (int index = 0; index < targetSkeleton.JointNames.Count; index++)
        {
            string name = targetSkeleton.JointNames[index];
            if (!targetByExactName.TryAdd(name, index))
                errors.Add($"Target deform name '{name}' is duplicated.");
            string normalized = NormalizeJointName(name);
            if (!normalizedGroups.TryGetValue(normalized, out List<int>? indices))
            {
                indices = [];
                normalizedGroups.Add(normalized, indices);
            }
            indices.Add(index);
        }

        targetByNormalizedName = new Dictionary<string, int>(StringComparer.Ordinal);
        foreach ((string normalized, List<int> indices) in normalizedGroups)
        {
            if (indices.Count == 1)
            {
                targetByNormalizedName.Add(normalized, indices[0]);
                continue;
            }

            errors.Add(
                $"Target deform names [{string.Join(", ", indices.Select(
                    index => targetSkeleton.JointNames[index]))}] collapse to the " +
                $"same normalized token '{normalized}'.");
        }
    }

    private static bool TryResolveTargetJoint(
        string donorJointName,
        IReadOnlyDictionary<string, int> targetByExactName,
        IReadOnlyDictionary<string, int> targetByNormalizedName,
        out int targetJointIndex,
        out string matchKind,
        out string? error)
    {
        if (targetByExactName.TryGetValue(donorJointName, out targetJointIndex))
        {
            matchKind = "exact target name";
            error = null;
            return true;
        }

        string normalized = NormalizeJointName(donorJointName);
        if (targetByNormalizedName.TryGetValue(normalized, out targetJointIndex))
        {
            matchKind = "case/punctuation-normalized target name";
            error = null;
            return true;
        }

        if (HumanoidAliases.TryGetValue(normalized, out string? targetName))
        {
            if (targetByExactName.TryGetValue(targetName, out targetJointIndex))
            {
                matchKind = "reviewed humanoid alias";
                error = null;
                return true;
            }

            error = $"reviewed alias '{normalized}' requires target joint " +
                    $"'{targetName}', which is absent from this target";
            matchKind = string.Empty;
            targetJointIndex = -1;
            return false;
        }

        targetJointIndex = -1;
        matchKind = string.Empty;
        error = "there is no exact, normalized, or reviewed semantic mapping; " +
                "spatial nearest-joint fallback is intentionally disabled";
        return false;
    }

    private static ImportedMesh PrepareMesh(
        ImportedMesh source,
        ImportedSkeleton targetSkeleton,
        IReadOnlyDictionary<ImportedSkeleton, SkeletonMappingPlan> skeletonPlans)
    {
        ImportedSkinning skinning = source.Skinning ?? throw new InvalidOperationException(
            $"Mesh '{source.Name}' unexpectedly has no skin after successful analysis.");
        if (!skeletonPlans.TryGetValue(
                skinning.Skeleton, out SkeletonMappingPlan? mappingPlan))
        {
            throw new InvalidOperationException(
                $"Mesh '{source.Name}' has no analyzed donor skeleton plan.");
        }

        Vector3[] sourceNormals = GetSanitizedDonorNormals(source);
        var positions = new Vector3[source.Positions.Length];
        var normals = new Vector3[source.Positions.Length];
        var jointIndices = new ImportedJointIndices[source.Positions.Length];
        var weights = new Vector4[source.Positions.Length];
        Span<ushort> donorJointIndices = stackalloc ushort[4];
        Span<float> donorWeights = stackalloc float[4];
        Span<int> compactTargetIndices = stackalloc int[4];
        Span<float> compactWeights = stackalloc float[4];
        Span<ushort> targetJointSlots = stackalloc ushort[4];
        Span<float> targetWeightSlots = stackalloc float[4];

        for (int vertexIndex = 0; vertexIndex < source.Positions.Length; vertexIndex++)
        {
            ImportedJointIndices sourceJoints = skinning.JointIndices[vertexIndex];
            Vector4 sourceWeights = skinning.Weights[vertexIndex];
            donorJointIndices[0] = sourceJoints.X;
            donorJointIndices[1] = sourceJoints.Y;
            donorJointIndices[2] = sourceJoints.Z;
            donorJointIndices[3] = sourceJoints.W;
            donorWeights[0] = sourceWeights.X;
            donorWeights[1] = sourceWeights.Y;
            donorWeights[2] = sourceWeights.Z;
            donorWeights[3] = sourceWeights.W;
            float sourceWeightSum = 0;
            for (int influence = 0; influence < 4; influence++)
            {
                if (donorWeights[influence] > WeightEpsilon)
                    sourceWeightSum += donorWeights[influence];
            }
            if (!float.IsFinite(sourceWeightSum) || sourceWeightSum <= WeightEpsilon)
                throw new InvalidOperationException(
                    $"Mesh '{source.Name}' vertex {vertexIndex} has no usable weights.");

            Vector3 preparedPosition = Vector3.Zero;
            Vector3 preparedNormal = Vector3.Zero;
            compactTargetIndices.Fill(-1);
            compactWeights.Clear();
            int compactCount = 0;
            for (int influence = 0; influence < 4; influence++)
            {
                float donorWeight = donorWeights[influence];
                if (donorWeight <= WeightEpsilon)
                    continue;

                int donorJointIndex = donorJointIndices[influence];
                int targetJointIndex =
                    mappingPlan.TargetJointIndices[donorJointIndex];
                if (targetJointIndex < 0)
                {
                    throw new InvalidOperationException(
                        $"Mesh '{source.Name}' vertex {vertexIndex} uses an " +
                        $"unmapped donor joint {donorJointIndex}.");
                }

                float normalizedDonorWeight = donorWeight / sourceWeightSum;
                preparedPosition += Vector3.Transform(
                    source.Positions[vertexIndex],
                    mappingPlan.RebaseMatrices[donorJointIndex]) * normalizedDonorWeight;
                preparedNormal += Vector3.TransformNormal(
                    sourceNormals[vertexIndex],
                    mappingPlan.NormalMatrices[donorJointIndex]) * normalizedDonorWeight;

                int existing = -1;
                for (int compactIndex = 0; compactIndex < compactCount; compactIndex++)
                {
                    if (compactTargetIndices[compactIndex] == targetJointIndex)
                    {
                        existing = compactIndex;
                        break;
                    }
                }
                if (existing >= 0)
                {
                    compactWeights[existing] += normalizedDonorWeight;
                }
                else
                {
                    compactTargetIndices[compactCount] = targetJointIndex;
                    compactWeights[compactCount] = normalizedDonorWeight;
                    compactCount++;
                }
            }

            if (!IsFinite(preparedPosition) || !IsFinite(preparedNormal))
                throw new InvalidOperationException(
                    $"Mesh '{source.Name}' vertex {vertexIndex} produced a " +
                    "non-finite prepared position or normal.");
            if (preparedNormal.LengthSquared() <= WeightEpsilon)
                throw new InvalidOperationException(
                    $"Mesh '{source.Name}' vertex {vertexIndex} produced a " +
                    "zero-length prepared normal.");
            preparedNormal = Vector3.Normalize(preparedNormal);

            SortInfluences(compactTargetIndices, compactWeights, compactCount);
            int retainedCount = Math.Min(4, compactCount);
            float retainedWeightSum = 0;
            for (int compactIndex = 0; compactIndex < retainedCount; compactIndex++)
                retainedWeightSum += compactWeights[compactIndex];
            if (!float.IsFinite(retainedWeightSum) ||
                retainedWeightSum <= WeightEpsilon)
            {
                throw new InvalidOperationException(
                    $"Mesh '{source.Name}' vertex {vertexIndex} has no mapped weights.");
            }

            targetJointSlots.Clear();
            targetWeightSlots.Clear();
            for (int compactIndex = 0; compactIndex < retainedCount; compactIndex++)
            {
                targetJointSlots[compactIndex] = checked(
                    (ushort)compactTargetIndices[compactIndex]);
                targetWeightSlots[compactIndex] =
                    compactWeights[compactIndex] / retainedWeightSum;
            }

            positions[vertexIndex] = preparedPosition;
            normals[vertexIndex] = preparedNormal;
            jointIndices[vertexIndex] = new ImportedJointIndices(
                targetJointSlots[0], targetJointSlots[1],
                targetJointSlots[2], targetJointSlots[3]);
            weights[vertexIndex] = new Vector4(
                targetWeightSlots[0], targetWeightSlots[1],
                targetWeightSlots[2], targetWeightSlots[3]);
        }

        return new ImportedMesh(
            source.Name,
            positions,
            normals,
            source.TextureCoordinates.ToArray(),
            source.TriangleIndices.ToArray(),
            source.DiffuseColorsArgb?.ToArray(),
            source.MaterialIndex,
            new ImportedSkinning(targetSkeleton, jointIndices, weights));
    }

    private static ImportedMesh PrepareFittingMesh(
        ImportedMesh source,
        ImportedSkeleton targetSkeleton,
        IReadOnlyDictionary<ImportedSkeleton, SkeletonMappingPlan> skeletonPlans,
        Matrix4x4 alignment,
        Matrix4x4 normalAlignment)
    {
        ImportedSkinning skinning = source.Skinning ?? throw new InvalidOperationException(
            $"Mesh '{source.Name}' unexpectedly has no skin after successful analysis.");
        if (!skeletonPlans.TryGetValue(
                skinning.Skeleton, out SkeletonMappingPlan? mappingPlan))
        {
            throw new InvalidOperationException(
                $"Mesh '{source.Name}' has no analyzed donor skeleton plan.");
        }

        Vector3[] sourceNormals = GetSanitizedDonorNormals(source);
        var positions = new Vector3[source.Positions.Length];
        var normals = new Vector3[source.Positions.Length];
        var jointIndices = new ImportedJointIndices[source.Positions.Length];
        var weights = new Vector4[source.Positions.Length];
        Span<ushort> donorJointIndices = stackalloc ushort[4];
        Span<float> donorWeights = stackalloc float[4];
        Span<int> compactTargetIndices = stackalloc int[4];
        Span<float> compactWeights = stackalloc float[4];
        Span<ushort> targetJointSlots = stackalloc ushort[4];
        Span<float> targetWeightSlots = stackalloc float[4];

        for (int vertexIndex = 0; vertexIndex < source.Positions.Length; vertexIndex++)
        {
            Vector3 fittingPosition = Vector3.Transform(
                source.Positions[vertexIndex], alignment);
            Vector3 fittingNormal = Vector3.TransformNormal(
                sourceNormals[vertexIndex], normalAlignment);
            if (!IsFinite(fittingPosition) || !IsFinite(fittingNormal) ||
                fittingNormal.LengthSquared() <= WeightEpsilon)
            {
                throw new InvalidOperationException(
                    $"Mesh '{source.Name}' vertex {vertexIndex} produced an " +
                    "invalid fitting-space position or normal.");
            }
            positions[vertexIndex] = fittingPosition;
            normals[vertexIndex] = Vector3.Normalize(fittingNormal);

            ImportedJointIndices sourceJoints = skinning.JointIndices[vertexIndex];
            Vector4 sourceWeights = skinning.Weights[vertexIndex];
            donorJointIndices[0] = sourceJoints.X;
            donorJointIndices[1] = sourceJoints.Y;
            donorJointIndices[2] = sourceJoints.Z;
            donorJointIndices[3] = sourceJoints.W;
            donorWeights[0] = sourceWeights.X;
            donorWeights[1] = sourceWeights.Y;
            donorWeights[2] = sourceWeights.Z;
            donorWeights[3] = sourceWeights.W;
            float sourceWeightSum = 0;
            for (int influence = 0; influence < 4; influence++)
            {
                float donorWeight = donorWeights[influence];
                if (!float.IsFinite(donorWeight) || donorWeight < 0)
                {
                    throw new InvalidOperationException(
                        $"Mesh '{source.Name}' vertex {vertexIndex} has an " +
                        "invalid donor weight.");
                }
                if (donorWeight > WeightEpsilon)
                    sourceWeightSum += donorWeight;
            }
            if (!float.IsFinite(sourceWeightSum) || sourceWeightSum <= WeightEpsilon)
            {
                throw new InvalidOperationException(
                    $"Mesh '{source.Name}' vertex {vertexIndex} has no usable weights.");
            }

            compactTargetIndices.Fill(-1);
            compactWeights.Clear();
            int compactCount = 0;
            for (int influence = 0; influence < 4; influence++)
            {
                float donorWeight = donorWeights[influence];
                if (donorWeight <= WeightEpsilon)
                    continue;
                int donorJointIndex = donorJointIndices[influence];
                if ((uint)donorJointIndex >=
                    (uint)mappingPlan.TargetJointIndices.Length)
                {
                    throw new InvalidOperationException(
                        $"Mesh '{source.Name}' vertex {vertexIndex} uses donor " +
                        $"joint {donorJointIndex} outside the analyzed skeleton.");
                }
                int targetJointIndex =
                    mappingPlan.TargetJointIndices[donorJointIndex];
                if (targetJointIndex < 0)
                {
                    throw new InvalidOperationException(
                        $"Mesh '{source.Name}' vertex {vertexIndex} uses an " +
                        $"unmapped donor joint {donorJointIndex}.");
                }

                float normalizedDonorWeight = donorWeight / sourceWeightSum;
                int existing = -1;
                for (int compactIndex = 0;
                     compactIndex < compactCount;
                     compactIndex++)
                {
                    if (compactTargetIndices[compactIndex] == targetJointIndex)
                    {
                        existing = compactIndex;
                        break;
                    }
                }
                if (existing >= 0)
                {
                    compactWeights[existing] += normalizedDonorWeight;
                }
                else
                {
                    compactTargetIndices[compactCount] = targetJointIndex;
                    compactWeights[compactCount] = normalizedDonorWeight;
                    compactCount++;
                }
            }

            SortInfluences(compactTargetIndices, compactWeights, compactCount);
            int retainedCount = Math.Min(4, compactCount);
            float retainedWeightSum = 0;
            for (int compactIndex = 0;
                 compactIndex < retainedCount;
                 compactIndex++)
            {
                retainedWeightSum += compactWeights[compactIndex];
            }
            if (!float.IsFinite(retainedWeightSum) ||
                retainedWeightSum <= WeightEpsilon)
            {
                throw new InvalidOperationException(
                    $"Mesh '{source.Name}' vertex {vertexIndex} has no mapped weights.");
            }

            targetJointSlots.Clear();
            targetWeightSlots.Clear();
            for (int compactIndex = 0;
                 compactIndex < retainedCount;
                 compactIndex++)
            {
                targetJointSlots[compactIndex] = checked(
                    (ushort)compactTargetIndices[compactIndex]);
                targetWeightSlots[compactIndex] =
                    compactWeights[compactIndex] / retainedWeightSum;
            }
            jointIndices[vertexIndex] = new ImportedJointIndices(
                targetJointSlots[0], targetJointSlots[1],
                targetJointSlots[2], targetJointSlots[3]);
            weights[vertexIndex] = new Vector4(
                targetWeightSlots[0], targetWeightSlots[1],
                targetWeightSlots[2], targetWeightSlots[3]);
        }

        return new ImportedMesh(
            source.Name,
            positions,
            normals,
            source.TextureCoordinates.ToArray(),
            source.TriangleIndices.ToArray(),
            source.DiffuseColorsArgb?.ToArray(),
            source.MaterialIndex,
            new ImportedSkinning(targetSkeleton, jointIndices, weights));
    }

    private static void ValidateMesh(
        ImportedMesh mesh,
        int meshIndex,
        List<string> errors,
        List<string> warnings)
    {
        string label = $"Mesh [{meshIndex}] '{mesh.Name}'";
        if (mesh.Positions.Length == 0)
            errors.Add(label + " contains no vertices.");
        if (mesh.TriangleIndices.Length == 0 ||
            mesh.TriangleIndices.Length % 3 != 0)
        {
            errors.Add(label + " has no complete triangle index list.");
        }
        foreach (uint index in mesh.TriangleIndices)
        {
            if (index >= mesh.Positions.Length)
            {
                errors.Add(label + $" has out-of-range triangle index {index}.");
                break;
            }
        }
        for (int vertexIndex = 0; vertexIndex < mesh.Positions.Length; vertexIndex++)
        {
            if (!IsFinite(mesh.Positions[vertexIndex]))
            {
                errors.Add(label + $" vertex {vertexIndex} position is non-finite.");
                break;
            }
        }

        if (mesh.Normals.Length == 0)
        {
            warnings.Add(label + " has no normals; smooth normals will be generated.");
        }
        else if (mesh.Normals.Length != mesh.Positions.Length)
        {
            warnings.Add(
                label + $" has {mesh.Normals.Length} normals for " +
                $"{mesh.Positions.Length} vertices; smooth normals for the complete " +
                "mesh will be generated.");
        }
        else
        {
            int invalidNormalCount = 0;
            for (int vertexIndex = 0; vertexIndex < mesh.Normals.Length; vertexIndex++)
            {
                if (!IsFinite(mesh.Normals[vertexIndex]) ||
                    mesh.Normals[vertexIndex].LengthSquared() <= WeightEpsilon)
                {
                    invalidNormalCount++;
                }
            }
            if (invalidNormalCount > 0)
            {
                warnings.Add(
                    label + $" has {invalidNormalCount} non-finite or zero-length " +
                    "normal(s); smooth normals for the complete mesh will be generated.");
            }
        }

        if (mesh.TextureCoordinates.Length != 0 &&
            mesh.TextureCoordinates.Length != mesh.Positions.Length)
        {
            errors.Add(
                label + $" has {mesh.TextureCoordinates.Length} UV values for " +
                $"{mesh.Positions.Length} vertices.");
        }
        for (int vertexIndex = 0;
             vertexIndex < mesh.TextureCoordinates.Length;
             vertexIndex++)
        {
            if (!IsFinite(mesh.TextureCoordinates[vertexIndex]))
            {
                errors.Add(label + $" UV {vertexIndex} is non-finite.");
                break;
            }
        }
        if (mesh.DiffuseColorsArgb is { Length: > 0 } colors &&
            colors.Length != mesh.Positions.Length)
        {
            errors.Add(
                label + $" has {colors.Length} colors for " +
                $"{mesh.Positions.Length} vertices.");
        }
    }

    private static void ValidateSkeleton(
        ImportedSkeleton skeleton,
        List<string> errors,
        bool requireInverseBinds)
    {
        if (skeleton.JointNames.Count == 0)
        {
            errors.Add($"Donor skeleton '{skeleton.Name}' contains no joints.");
            return;
        }
        if (requireInverseBinds &&
            skeleton.InverseBindMatrices.Count != skeleton.JointNames.Count)
        {
            errors.Add(
                $"Donor skeleton '{skeleton.Name}' has " +
                $"{skeleton.JointNames.Count} joint names but " +
                $"{skeleton.InverseBindMatrices.Count} inverse bind matrices.");
        }

        for (int jointIndex = 0; jointIndex < skeleton.JointNames.Count; jointIndex++)
        {
            string name = skeleton.JointNames[jointIndex];
            if (requireInverseBinds &&
                jointIndex < skeleton.InverseBindMatrices.Count &&
                !IsFinite(skeleton.InverseBindMatrices[jointIndex]))
            {
                errors.Add(
                    $"Donor skeleton '{skeleton.Name}' joint '{name}' has a " +
                    "non-finite inverse bind matrix.");
            }
        }
    }

    private static void CollectActiveJoints(
        ImportedMesh mesh,
        int meshIndex,
        HashSet<int> activeJointIndices,
        List<string> errors)
    {
        ImportedSkinning skinning = mesh.Skinning!;
        string label = $"Mesh [{meshIndex}] '{mesh.Name}'";
        if (skinning.JointIndices.Length != mesh.Positions.Length ||
            skinning.Weights.Length != mesh.Positions.Length)
        {
            errors.Add(
                label + $" has {skinning.JointIndices.Length} JOINTS values and " +
                $"{skinning.Weights.Length} WEIGHTS values for " +
                $"{mesh.Positions.Length} vertices.");
            return;
        }

        int jointCount = skinning.Skeleton.JointNames.Count;
        Span<ushort> jointSlots = stackalloc ushort[4];
        Span<float> weightSlots = stackalloc float[4];
        for (int vertexIndex = 0; vertexIndex < mesh.Positions.Length; vertexIndex++)
        {
            ImportedJointIndices joints = skinning.JointIndices[vertexIndex];
            Vector4 weights = skinning.Weights[vertexIndex];
            jointSlots[0] = joints.X;
            jointSlots[1] = joints.Y;
            jointSlots[2] = joints.Z;
            jointSlots[3] = joints.W;
            weightSlots[0] = weights.X;
            weightSlots[1] = weights.Y;
            weightSlots[2] = weights.Z;
            weightSlots[3] = weights.W;
            float sum = 0;
            for (int influence = 0; influence < 4; influence++)
            {
                float weight = weightSlots[influence];
                if (!float.IsFinite(weight) || weight < 0)
                {
                    errors.Add(
                        label + $" vertex {vertexIndex} influence {influence} " +
                        $"has invalid weight {weight:G9}.");
                    continue;
                }
                int jointIndex = jointSlots[influence];
                if (jointIndex >= jointCount)
                {
                    errors.Add(
                        label + $" vertex {vertexIndex} influence {influence} " +
                        $"references joint {jointIndex}, but the skeleton has " +
                        $"{jointCount} joints.");
                    continue;
                }
                if (weight > WeightEpsilon)
                {
                    sum += weight;
                    activeJointIndices.Add(jointIndex);
                }
            }
            if (!float.IsFinite(sum) || sum <= WeightEpsilon)
                errors.Add(label + $" vertex {vertexIndex} has no positive skin weight.");
        }
    }

    private static Vector3[] GetSanitizedDonorNormals(ImportedMesh mesh) =>
        DonorNormalsRequireRegeneration(mesh)
            ? GenerateSmoothNormals(mesh.Positions, mesh.TriangleIndices)
            : mesh.Normals;

    private static bool DonorNormalsRequireRegeneration(ImportedMesh mesh)
    {
        if (mesh.Normals.Length != mesh.Positions.Length)
            return true;
        return mesh.Normals.Any(normal =>
            !IsFinite(normal) || normal.LengthSquared() <= WeightEpsilon);
    }

    private static Vector3[] GenerateSmoothNormals(
        IReadOnlyList<Vector3> positions,
        IReadOnlyList<uint> triangleIndices)
    {
        var normals = new Vector3[positions.Count];
        for (int index = 0; index + 2 < triangleIndices.Count; index += 3)
        {
            int a = checked((int)triangleIndices[index]);
            int b = checked((int)triangleIndices[index + 1]);
            int c = checked((int)triangleIndices[index + 2]);
            Vector3 faceNormal = Vector3.Cross(
                positions[b] - positions[a], positions[c] - positions[a]);
            if (!IsFinite(faceNormal) || faceNormal.LengthSquared() <= WeightEpsilon)
                continue;
            normals[a] += faceNormal;
            normals[b] += faceNormal;
            normals[c] += faceNormal;
        }

        for (int index = 0; index < normals.Length; index++)
        {
            normals[index] = normals[index].LengthSquared() > WeightEpsilon
                ? Vector3.Normalize(normals[index])
                : Vector3.UnitY;
        }
        return normals;
    }

    private static void SortInfluences(
        Span<int> jointIndices,
        Span<float> weights,
        int count)
    {
        for (int left = 0; left < count - 1; left++)
        {
            int best = left;
            for (int right = left + 1; right < count; right++)
            {
                if (weights[right] > weights[best] ||
                    (weights[right] == weights[best] &&
                     jointIndices[right] < jointIndices[best]))
                {
                    best = right;
                }
            }
            if (best == left)
                continue;
            (weights[left], weights[best]) = (weights[best], weights[left]);
            (jointIndices[left], jointIndices[best]) =
                (jointIndices[best], jointIndices[left]);
        }
    }

    private static string NormalizeJointName(string name) =>
        new string(name.Where(char.IsLetterOrDigit)
            .Select(char.ToLowerInvariant)
            .ToArray());

    private static bool IsFinite(Vector2 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y);

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);

    private static bool IsFinite(Matrix4x4 value) =>
        float.IsFinite(value.M11) && float.IsFinite(value.M12) &&
        float.IsFinite(value.M13) && float.IsFinite(value.M14) &&
        float.IsFinite(value.M21) && float.IsFinite(value.M22) &&
        float.IsFinite(value.M23) && float.IsFinite(value.M24) &&
        float.IsFinite(value.M31) && float.IsFinite(value.M32) &&
        float.IsFinite(value.M33) && float.IsFinite(value.M34) &&
        float.IsFinite(value.M41) && float.IsFinite(value.M42) &&
        float.IsFinite(value.M43) && float.IsFinite(value.M44);

    private sealed class SkeletonUse(ImportedSkeleton skeleton)
    {
        public ImportedSkeleton Skeleton { get; } = skeleton;
        public HashSet<int> ActiveJointIndices { get; } = [];
    }

    private sealed class SkeletonMappingPlan
    {
        public SkeletonMappingPlan(int donorJointCount)
        {
            TargetJointIndices = Enumerable.Repeat(-1, donorJointCount).ToArray();
            RebaseMatrices = new Matrix4x4[donorJointCount];
            NormalMatrices = new Matrix4x4[donorJointCount];
        }

        public int[] TargetJointIndices { get; }
        public Matrix4x4[] RebaseMatrices { get; }
        public Matrix4x4[] NormalMatrices { get; }
    }

    private sealed record PreparationPlan(
        SkinnedModelPortingAnalysis Analysis,
        ImportedSkeleton? TargetSkeleton,
        IReadOnlyDictionary<ImportedSkeleton, SkeletonMappingPlan> SkeletonPlans);
}
