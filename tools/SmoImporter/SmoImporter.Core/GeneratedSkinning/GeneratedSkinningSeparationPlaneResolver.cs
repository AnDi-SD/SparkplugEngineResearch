using System.Collections.ObjectModel;
using System.Numerics;

namespace SmoImporter.Core;

public static partial class GeneratedSkinningPreparer
{
    private readonly record struct ShoulderPlaneClassification(
        bool LeftActive,
        bool RightActive,
        bool LeftExterior,
        bool RightExterior);

    private sealed record SeparationPlanePreparation(
        IReadOnlyList<GeneratedSkinningSeparationPlaneResolution> Resolutions,
        IReadOnlyDictionary<GeneratedSkinningSeparationPlaneKind,
            GeneratedSkinningSeparationPlaneResolution> ByKind,
        IReadOnlySet<int> LeftArmSkeletonJointIndices,
        IReadOnlySet<int> RightArmSkeletonJointIndices,
        IReadOnlySet<int> HeadSkeletonJointIndices,
        int HeadSkeletonJointIndex);

    private static SeparationPlanePreparation ResolveSeparationPlanes(
        TargetRigDefinition rig,
        TargetSkeletonLayout layout,
        RobustBounds targetBounds,
        IReadOnlyList<Matrix4x4>? fittingWorldMatrices,
        SideCalibration sides,
        IReadOnlyDictionary<int, AnatomicalVolume> anatomicalVolumes,
        GeneratedSkinningRegionOverrides? overrides)
    {
        var adjustments = Enum.GetValues<GeneratedSkinningSeparationPlaneKind>()
            .ToDictionary(
                kind => kind,
                GeneratedSkinningSeparationPlaneAdjustment.Automatic);
        if (overrides is not null)
        {
            ArgumentNullException.ThrowIfNull(overrides.SeparationPlanes);
            foreach (GeneratedSkinningSeparationPlaneAdjustment adjustment in
                     overrides.SeparationPlanes)
            {
                if (!Enum.IsDefined(adjustment.Kind) ||
                    !float.IsFinite(adjustment.Offset) ||
                    !float.IsFinite(adjustment.AngleDegrees))
                {
                    throw new InvalidDataException(
                        "A separation-plane adjustment is unknown or non-finite.");
                }
                if (!adjustments.TryAdd(adjustment.Kind, adjustment))
                {
                    // Defaults occupy every key; replace one default exactly
                    // once, while still rejecting duplicate authored values.
                    if (overrides.SeparationPlanes.Count(value =>
                            value.Kind == adjustment.Kind) != 1)
                    {
                        throw new InvalidDataException(
                            $"Separation plane {adjustment.Kind} is specified more than once.");
                    }
                    adjustments[adjustment.Kind] = adjustment;
                }
            }
        }

        Vector3 lateral = GetCalibratedLateralAxis(sides);
        if (!TryNormalize(lateral, out lateral))
            throw new InvalidDataException(
                "Target rig has no finite lateral axis for separation planes.");

        TargetRigJoint? neck = FindUniqueJoint(layout, "Neck");
        TargetRigJoint? head = FindUniqueJoint(layout, "Head");
        Vector3 up = Vector3.UnitY;
        if (neck is not null && head is not null)
        {
            Vector3 candidate = Translation(GetFittingWorldMatrix(
                head,
                fittingWorldMatrices)) -
                Translation(GetFittingWorldMatrix(neck, fittingWorldMatrices));
            if (TryNormalize(candidate, out Vector3 normalizedUp))
                up = normalizedUp;
        }

        AnatomicalVolume? forwardSource = anatomicalVolumes.Values
            .Where(volume => volume.BoneName is "Spine_03" or "Spine_02" or
                "Spine_01")
            .OrderBy(volume => volume.BoneName == "Spine_03" ? 0 :
                volume.BoneName == "Spine_02" ? 1 : 2)
            .FirstOrDefault();
        Vector3 forward = Vector3.Zero;
        bool hasProvenForward = forwardSource is not null &&
            TryNormalize(forwardSource.ForwardAxis, out forward);
        if (!hasProvenForward)
        {
            Vector3 candidate = Vector3.Cross(lateral, up);
            if (!TryNormalize(candidate, out forward))
                forward = Vector3.UnitZ;
        }
        // Re-orthogonalize the fitting frame before applying authored angles.
        // A malformed target must fail explicitly rather than leak NaN plane
        // coordinates into weights or the viewport.
        Vector3 projectedUp = up - lateral * Vector3.Dot(up, lateral);
        if (!TryNormalize(projectedUp, out up))
        {
            throw new InvalidDataException(
                "Target rig has no finite up axis for separation planes.");
        }
        Vector3 projectedForward =
            forward - lateral * Vector3.Dot(forward, lateral) -
            up * Vector3.Dot(forward, up);
        if (!TryNormalize(projectedForward, out forward))
        {
            throw new InvalidDataException(
                "Target rig has no finite forward axis for separation planes.");
        }

        float height = targetBounds.Size.Y;
        float width = MathF.Max(targetBounds.Size.X, height * 0.2f);
        float depth = MathF.Max(targetBounds.Size.Z, height * 0.1f);
        IReadOnlySet<int> leftArmIndices = CollectArmSkeletonIndices(
            rig,
            layout,
            ["L_Clavicle", "L_Bicep"]);
        IReadOnlySet<int> rightArmIndices = CollectArmSkeletonIndices(
            rig,
            layout,
            ["R_Clavicle", "R_Bicep"]);
        IReadOnlySet<int> headIndices = CollectDeformSkeletonIndices(
            rig,
            layout,
            ["Head"]);
        int headSkeletonIndex = head is not null &&
            layout.SkeletonIndexByRigJoint.TryGetValue(
                head.JointIndex,
                out int resolvedHeadSkeletonIndex)
                    ? resolvedHeadSkeletonIndex
                    : -1;
        var resolutions = new List<GeneratedSkinningSeparationPlaneResolution>(4);
        resolutions.Add(BuildShoulder(
            GeneratedSkinningSeparationPlaneKind.LeftShoulder,
            ["L_Bicep"],
            lateral));
        resolutions.Add(BuildShoulder(
            GeneratedSkinningSeparationPlaneKind.RightShoulder,
            ["R_Bicep"],
            -lateral));
        resolutions.Add(BuildHead());
        resolutions.Add(BuildBack());

        Dictionary<GeneratedSkinningSeparationPlaneKind,
            GeneratedSkinningSeparationPlaneResolution> byKind = resolutions
            .ToDictionary(value => value.Kind);
        return new SeparationPlanePreparation(
            new ReadOnlyCollection<GeneratedSkinningSeparationPlaneResolution>(
                resolutions),
            new ReadOnlyDictionary<GeneratedSkinningSeparationPlaneKind,
                GeneratedSkinningSeparationPlaneResolution>(byKind),
            leftArmIndices,
            rightArmIndices,
            headIndices,
            headSkeletonIndex);

        GeneratedSkinningSeparationPlaneResolution BuildShoulder(
            GeneratedSkinningSeparationPlaneKind kind,
            IReadOnlyList<string> anchorNames,
            Vector3 outward)
        {
            var limits = new GeneratedSkinningSeparationPlaneLimits(
                0, 0, -60, 60);
            GeneratedSkinningSeparationPlaneAdjustment adjustment =
                adjustments[kind];
            ValidateAdjustment(adjustment, limits, offsetAllowed: false);
            TargetRigJoint? anchor = FindFirstUniqueJoint(layout, anchorNames);
            IReadOnlySet<int> armIndices = kind ==
                GeneratedSkinningSeparationPlaneKind.LeftShoulder
                    ? leftArmIndices
                    : rightArmIndices;
            if (anchor is null || armIndices.Count == 0)
                return Unavailable(kind, adjustment, limits,
                    string.Join("/", anchorNames),
                    "The target rig has no unique shoulder anchor and deform subtree.");
            Vector3 point = Translation(GetFittingWorldMatrix(
                anchor,
                fittingWorldMatrices));
            if (!IsFinite(point))
                return Unavailable(kind, adjustment, limits, anchor.Name,
                    "The target shoulder anchor is non-finite.");
            Vector3 normal = GeneratedSkinningSeparationPlaneMath.RotateInPlane(
                outward,
                up,
                adjustment.AngleDegrees);
            Vector3 downward = GeneratedSkinningSeparationPlaneMath.RotateInPlane(
                -up,
                outward,
                adjustment.AngleDegrees);
            return Available(
                kind,
                adjustment,
                limits,
                anchor.Name,
                point,
                normal,
                forward,
                downward,
                MathF.Max(depth * 0.75f, height * 0.08f),
                height * 0.65f) with
            {
                AutomaticPoint = point,
                AutomaticNormal = outward,
                RotationSecondaryAxis = up,
                OffsetAxis = Vector3.Zero
            };
        }

        GeneratedSkinningSeparationPlaneResolution BuildHead()
        {
            var limits = new GeneratedSkinningSeparationPlaneLimits(
                -height * 0.08f,
                height * 0.15f,
                -45,
                45);
            GeneratedSkinningSeparationPlaneAdjustment adjustment =
                adjustments[GeneratedSkinningSeparationPlaneKind.Head];
            ValidateAdjustment(adjustment, limits, offsetAllowed: true);
            if (neck is null || headSkeletonIndex < 0)
                return Unavailable(
                    GeneratedSkinningSeparationPlaneKind.Head,
                    adjustment,
                    limits,
                    "Neck",
                    "The target rig has no unique Neck/Head anchor in the " +
                    "generated deform skeleton.");
            Vector3 anchor = Translation(GetFittingWorldMatrix(
                neck,
                fittingWorldMatrices));
            Vector3 point = anchor + up * adjustment.Offset;
            Vector3 normal = GeneratedSkinningSeparationPlaneMath.RotateInPlane(
                up,
                -forward,
                adjustment.AngleDegrees);
            Vector3 depthAxis = Vector3.Normalize(Vector3.Cross(normal, lateral));
            return Available(
                GeneratedSkinningSeparationPlaneKind.Head,
                adjustment,
                limits,
                neck.Name,
                point,
                normal,
                lateral,
                depthAxis,
                width * 0.65f,
                depth * 0.9f) with
            {
                AutomaticPoint = anchor,
                AutomaticNormal = up,
                RotationSecondaryAxis = -forward,
                OffsetAxis = up
            };
        }

        GeneratedSkinningSeparationPlaneResolution BuildBack()
        {
            var limits = new GeneratedSkinningSeparationPlaneLimits(
                -height * 0.12f,
                height * 0.12f,
                -45,
                45);
            GeneratedSkinningSeparationPlaneAdjustment adjustment =
                adjustments[GeneratedSkinningSeparationPlaneKind.Back];
            ValidateAdjustment(adjustment, limits, offsetAllowed: true);
            TargetRigJoint? spine = FindUniqueJoint(layout, "Spine_03");
            if (spine is null || !hasProvenForward)
                return Unavailable(
                    GeneratedSkinningSeparationPlaneKind.Back,
                    adjustment,
                    limits,
                    spine?.Name ?? "Spine_03",
                    "The target rig has no unique Spine_03 anchor with a target-proven " +
                    "front direction.");
            Vector3 anchor = Translation(GetFittingWorldMatrix(
                spine,
                fittingWorldMatrices));
            Vector3 point = anchor + forward * adjustment.Offset;
            Vector3 normal = GeneratedSkinningSeparationPlaneMath.RotateInPlane(
                forward,
                up,
                adjustment.AngleDegrees);
            Vector3 vertical = Vector3.Normalize(Vector3.Cross(lateral, normal));
            return Available(
                GeneratedSkinningSeparationPlaneKind.Back,
                adjustment,
                limits,
                spine.Name,
                point,
                normal,
                lateral,
                vertical,
                width * 0.65f,
                height * 0.55f) with
            {
                AutomaticPoint = anchor,
                AutomaticNormal = forward,
                RotationSecondaryAxis = up,
                OffsetAxis = forward
            };
        }
    }

    private static GeneratedSkinningSeparationPlaneResolution Available(
        GeneratedSkinningSeparationPlaneKind kind,
        GeneratedSkinningSeparationPlaneAdjustment adjustment,
        GeneratedSkinningSeparationPlaneLimits limits,
        string anchorName,
        Vector3 point,
        Vector3 normal,
        Vector3 axisU,
        Vector3 axisV,
        float halfU,
        float halfV) =>
        new(
            kind,
            adjustment.Enabled,
            IsAvailable: true,
            anchorName,
            point,
            normal,
            axisU,
            axisV,
            halfU,
            halfV,
            adjustment,
            limits,
            Array.Empty<string>());

    private static GeneratedSkinningSeparationPlaneResolution Unavailable(
        GeneratedSkinningSeparationPlaneKind kind,
        GeneratedSkinningSeparationPlaneAdjustment adjustment,
        GeneratedSkinningSeparationPlaneLimits limits,
        string anchorName,
        string warning) =>
        new(
            kind,
            adjustment.Enabled,
            IsAvailable: false,
            anchorName,
            Vector3.Zero,
            Vector3.UnitY,
            Vector3.UnitX,
            Vector3.UnitZ,
            0,
            0,
            adjustment,
            limits,
            new ReadOnlyCollection<string>([warning]));

    private static void ValidateAdjustment(
        GeneratedSkinningSeparationPlaneAdjustment adjustment,
        GeneratedSkinningSeparationPlaneLimits limits,
        bool offsetAllowed)
    {
        if (!float.IsFinite(adjustment.Offset) ||
            !float.IsFinite(adjustment.AngleDegrees) ||
            adjustment.Offset < limits.MinimumOffset - PositionEpsilon ||
            adjustment.Offset > limits.MaximumOffset + PositionEpsilon ||
            adjustment.AngleDegrees < limits.MinimumAngleDegrees - PositionEpsilon ||
            adjustment.AngleDegrees > limits.MaximumAngleDegrees + PositionEpsilon ||
            !offsetAllowed && MathF.Abs(adjustment.Offset) > PositionEpsilon)
        {
            throw new InvalidDataException(
                $"Separation plane {adjustment.Kind} adjustment is outside its " +
                "calibrated editor limits.");
        }
    }

    private static IReadOnlySet<int> CollectArmSkeletonIndices(
        TargetRigDefinition rig,
        TargetSkeletonLayout layout,
        IReadOnlyList<string> rootNames) =>
        CollectDeformSkeletonIndices(rig, layout, rootNames);

    private static IReadOnlySet<int> CollectDeformSkeletonIndices(
        TargetRigDefinition rig,
        TargetSkeletonLayout layout,
        IReadOnlyList<string> rootNames)
    {
        TargetRigJoint? root = FindFirstUniqueJoint(layout, rootNames);
        if (root is null)
            return new HashSet<int>();
        HashSet<int> subtree = CollectDeformSubtree(
            rig,
            layout,
            root.JointIndex);
        return subtree
            .Where(layout.SkeletonIndexByRigJoint.ContainsKey)
            .Select(joint => layout.SkeletonIndexByRigJoint[joint])
            .ToHashSet();
    }

    private static TargetRigJoint? FindUniqueJoint(
        TargetSkeletonLayout layout,
        string name)
    {
        TargetRigJoint[] matches = layout.DeformJoints
            .Where(joint => string.Equals(
                joint.Name,
                name,
                StringComparison.Ordinal))
            .ToArray();
        return matches.Length == 1 ? matches[0] : null;
    }

    private static TargetRigJoint? FindFirstUniqueJoint(
        TargetSkeletonLayout layout,
        IReadOnlyList<string> names)
    {
        foreach (string name in names)
        {
            TargetRigJoint? match = FindUniqueJoint(layout, name);
            if (match is not null)
                return match;
        }
        return null;
    }

    private static bool TryNormalize(Vector3 value, out Vector3 normalized)
    {
        float lengthSquared = value.LengthSquared();
        if (!IsFinite(value) || !float.IsFinite(lengthSquared) ||
            lengthSquared <= PositionEpsilon * PositionEpsilon)
        {
            normalized = Vector3.Zero;
            return false;
        }
        normalized = Vector3.Normalize(value);
        return IsFinite(normalized);
    }

    private static ShoulderPlaneClassification ClassifyShoulderPlanes(
        Vector3 position,
        SeparationPlanePreparation? preparation)
    {
        if (preparation is null)
            return default;

        (bool leftActive, bool leftExterior) = Classify(
            GeneratedSkinningSeparationPlaneKind.LeftShoulder);
        (bool rightActive, bool rightExterior) = Classify(
            GeneratedSkinningSeparationPlaneKind.RightShoulder);
        if (leftExterior && rightExterior)
        {
            throw new InvalidDataException(
                "The edited shoulder planes overlap: one donor body vertex is " +
                "outside both shoulder walls.");
        }
        return new ShoulderPlaneClassification(
            leftActive,
            rightActive,
            leftExterior,
            rightExterior);

        (bool Active, bool Exterior) Classify(
            GeneratedSkinningSeparationPlaneKind kind)
        {
            if (!preparation.ByKind.TryGetValue(
                    kind,
                    out GeneratedSkinningSeparationPlaneResolution? plane) ||
                !plane.IsEnabled ||
                !plane.IsAvailable)
            {
                return default;
            }
            bool active = Vector3.Dot(position - plane.Point, plane.AxisV) >=
                -PositionEpsilon;
            bool exterior = active &&
                GeneratedSkinningSeparationPlaneMath.SignedDistance(
                    position,
                    plane) >= -PositionEpsilon;
            return (active, exterior);
        }
    }

    private static bool IsShoulderCapsuleCompatible(
        int skeletonJointIndex,
        ShoulderPlaneClassification classification,
        SeparationPlanePreparation? preparation)
    {
        if (preparation is null)
            return true;
        if (classification.LeftExterior)
            return preparation.LeftArmSkeletonJointIndices.Contains(
                skeletonJointIndex);
        if (classification.RightExterior)
            return preparation.RightArmSkeletonJointIndices.Contains(
                skeletonJointIndex);
        if (classification.LeftActive &&
            preparation.LeftArmSkeletonJointIndices.Contains(skeletonJointIndex))
        {
            return false;
        }
        if (classification.RightActive &&
            preparation.RightArmSkeletonJointIndices.Contains(skeletonJointIndex))
        {
            return false;
        }
        return true;
    }

    private static bool IsHeadPlaneCapsuleCompatible(
        Vector3 position,
        int skeletonJointIndex,
        SeparationPlanePreparation? preparation)
    {
        if (preparation is null ||
            preparation.HeadSkeletonJointIndices.Count == 0 ||
            !preparation.ByKind.TryGetValue(
                GeneratedSkinningSeparationPlaneKind.Head,
                out GeneratedSkinningSeparationPlaneResolution? plane) ||
            !plane.IsEnabled ||
            !plane.IsAvailable)
        {
            return true;
        }
        bool headSide = GeneratedSkinningSeparationPlaneMath.SignedDistance(
            position,
            plane) >= -PositionEpsilon;
        bool headJoint = preparation.HeadSkeletonJointIndices.Contains(
            skeletonJointIndex);
        return headSide == headJoint;
    }

    private static bool TryGetRigidHeadPlaneJoint(
        Vector3 position,
        SeparationPlanePreparation? preparation,
        out int skeletonJointIndex)
    {
        skeletonJointIndex = -1;
        if (preparation is null || preparation.HeadSkeletonJointIndex < 0 ||
            !preparation.ByKind.TryGetValue(
                GeneratedSkinningSeparationPlaneKind.Head,
                out GeneratedSkinningSeparationPlaneResolution? plane) ||
            !plane.IsEnabled ||
            !plane.IsAvailable ||
            GeneratedSkinningSeparationPlaneMath.SignedDistance(
                position,
                plane) < -PositionEpsilon)
        {
            return false;
        }
        skeletonJointIndex = preparation.HeadSkeletonJointIndex;
        return true;
    }

    private static ComponentPlaneClassification
        ClassifyDetachedComponentAgainstPlanes(
            GeometryComponent component,
            IReadOnlyList<Vector3[]> alignedPositionsByMesh,
            SeparationPlanePreparation? preparation)
    {
        if (preparation is null)
        {
            return new ComponentPlaneClassification(
                null,
                Array.Empty<GeneratedSkinningSeparationPlaneKind>());
        }

        GeneratedSkinningComponentAttachmentTarget? assignment = null;
        bool hasHeadCoverage = TryMeasureCoverage(
            GeneratedSkinningSeparationPlaneKind.Head,
            out ComponentPlaneCoverage headCoverage);
        bool hasBackCoverage = TryMeasureCoverage(
            GeneratedSkinningSeparationPlaneKind.Back,
            out ComponentPlaneCoverage backCoverage);
        bool isProtectedHead = hasHeadCoverage && hasBackCoverage &&
                               IsProtectedHeadComponent(
                                   headCoverage,
                                   backCoverage);
        if (isProtectedHead)
        {
            assignment = GeneratedSkinningComponentAttachmentTarget.Head;
        }
        else if (hasBackCoverage && backCoverage.HasNegativeMajority)
        {
            assignment = GeneratedSkinningComponentAttachmentTarget.UpperBack;
        }

        // Head is a protection rule, not an extraction rule: a component with
        // a strict majority above the neck cut stays protected when at least
        // one percent of its unique positions is in front of Back. Only an
        // unprotected strict majority behind Back is extracted. Neither plane
        // ever cuts a connected component, and minority/tie coverage does not
        // create a rigid detail by itself.
        return new ComponentPlaneClassification(
            assignment,
            Array.Empty<GeneratedSkinningSeparationPlaneKind>());

        bool TryMeasureCoverage(
            GeneratedSkinningSeparationPlaneKind kind,
            out ComponentPlaneCoverage coverage)
        {
            coverage = default;
            if (!preparation.ByKind.TryGetValue(
                    kind,
                    out GeneratedSkinningSeparationPlaneResolution? plane) ||
                !plane.IsEnabled ||
                !plane.IsAvailable)
            {
                return false;
            }
            coverage = MeasureComponentPlaneCoverage(
                component,
                alignedPositionsByMesh,
                plane);
            return coverage.PositionCount > 0;
        }
    }

    private static bool IsProtectedHeadComponent(
        ComponentPlaneCoverage headCoverage,
        ComponentPlaneCoverage backCoverage) =>
        headCoverage.HasPositiveMajority &&
        (long)backCoverage.PositivePositionCount * 100 >=
        backCoverage.PositionCount;

    private static ComponentPlaneCoverage MeasureComponentPlaneCoverage(
        GeometryComponent component,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        GeneratedSkinningSeparationPlaneResolution plane)
        => MeasurePlaneCoverage(
            component.Vertices,
            alignedPositionsByMesh,
            plane,
            $"Component #{component.ComponentIndex}");

    private static ComponentPlaneCoverage MeasurePlaneCoverage(
        IEnumerable<GeometryVertex> vertices,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        GeneratedSkinningSeparationPlaneResolution plane,
        string subjectLabel)
    {
        Vector3[] positions = vertices
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .ToArray();
        int positive = 0;
        int negative = 0;
        foreach (Vector3 position in positions)
        {
            float distance = GeneratedSkinningSeparationPlaneMath.SignedDistance(
                position,
                plane);
            if (!float.IsFinite(distance))
            {
                throw new InvalidDataException(
                    $"{subjectLabel} has a non-finite " +
                    $"distance to separation plane {plane.Kind}.");
            }
            if (distance >= -PositionEpsilon)
                positive++;
            else
                negative++;
        }
        return new ComponentPlaneCoverage(positions.Length, positive, negative);
    }
}
