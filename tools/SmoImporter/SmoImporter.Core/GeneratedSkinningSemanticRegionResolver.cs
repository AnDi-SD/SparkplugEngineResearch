using System.Buffers.Binary;
using System.Collections.ObjectModel;
using System.Numerics;
using System.Security.Cryptography;
using SmoExporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Core;

public static partial class GeneratedSkinningPreparer
{
    private const float SemanticCoreWeightThreshold = 0.95f;
    private const float SemanticTransitionWeightThreshold = 0.05f;
    private const float SemanticTransitionPairThreshold = 0.90f;
    private const int MinimumSemanticCoreSamples = 8;
    private const int MinimumSemanticTransitionSamples = 3;
    private const int MinimumCapturedSemanticCorePositions = 4;
    private const float MinimumSemanticRadiusHeightRatio = 0.005f;
    private const float DonorHandEnvelopeMargin = 1.02f;
    private const float MaximumDonorHandRadiusHeightRatio = 0.10f;
    private const float MaximumDonorToTargetHandRadiusRatio = 4f;
    private const float DonorHeadEnvelopeMargin = 1.04f;
    private const float MaximumDonorHeadSpanHeightRatio = 0.45f;
    private const float MaximumDonorHeadRadiusHeightRatio = 0.30f;
    private const float MinimumDonorToTargetHeadExtentRatio = 0.65f;
    private const float MaximumDonorToTargetHeadExtentRatio = 4f;
    private const float MaximumDonorNeckToTargetHeadRadiusRatio = 2f;
    private const float HeadCompanionEnvelopeRatio = 1.25f;
    private const float HeadCompanionMaximumSurfaceGapRatio = 0.35f;
    private const float MaximumHeadForwardTiltDegrees = 45f;
    private const float MaximumProtectedRegionScale = 1.75f;
    private const float MaximumHeadProtectedRegionScale = 3f;

    private enum SemanticVertexZone
    {
        Core,
        ProximalTransition
    }

    private sealed record SemanticVertexAssignment(
        GeneratedSkinningSemanticRegion Region,
        SemanticVertexZone Zone,
        int AnchorSkeletonJointIndex,
        int ProximalSkeletonJointIndex,
        float AnchorWeight)
    {
        public IReadOnlyList<PackedInfluence> CoreMotionInfluences { get; init; } =
            Array.Empty<PackedInfluence>();

        public int MotionProxySkeletonJointIndex => CoreMotionInfluences
            .FirstOrDefault(value => value.Joint != AnchorSkeletonJointIndex)
            ?.Joint ?? -1;

        public float MotionProxyWeight => CoreMotionInfluences
            .Where(value => value.Joint != AnchorSkeletonJointIndex)
            .Sum(value => value.Weight);
    }

    private sealed record SemanticRegionPreparation(
        GeneratedSkinningRegionAnalysis Analysis,
        IReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment> Assignments,
        IReadOnlyDictionary<int, SemanticComponentAssignment> ComponentAssignments,
        IReadOnlyDictionary<GeometryVertex, GeneratedVertexInfluences>
            CapsuleInfluences);

    private sealed record SemanticComponentAssignment(
        GeneratedSkinningSemanticRegion Region,
        string AnchorBoneName,
        int AnchorSkeletonJointIndex);

    private sealed record DecodedTargetWeightSample(
        Vector3 Position,
        IReadOnlyDictionary<int, float> WeightByRigJoint,
        float TotalWeight);

    private sealed record SemanticHeadLobe(
        int BodyComponentIndex,
        GeometryVertex Seed,
        IReadOnlySet<GeometryVertex> Vertices,
        IReadOnlySet<GeometryVertex> PrimaryVertices,
        IReadOnlySet<GeometryVertex> SecondaryVertices,
        IReadOnlySet<GeometryVertex> SeedVertices,
        IReadOnlySet<GeometryVertex> OwnerVertices,
        IReadOnlySet<GeometryVertex> BoundaryVertices,
        IReadOnlySet<GeometryVertex> NeckCollarVertices,
        IReadOnlyDictionary<GeometryVertex, float> NeckCollarHeadWeights,
        float CutPlaneProjection,
        float MaximumCollarSurfaceDistance);

    private sealed record SemanticHeadCluster(
        int OwnerComponentIndex,
        IReadOnlySet<GeometryVertex> Vertices,
        IReadOnlySet<GeometryVertex> SeedVertices,
        IReadOnlySet<GeometryVertex> BoundaryVertices,
        int UniquePositionCount,
        int UniqueBoundaryPositionCount,
        float MaximumBoundaryRadius);

    /// <summary>
    /// Balanced deterministic k-d tree for exact nearest-position queries.
    /// The query uses the same single-precision squared distance as the former
    /// exhaustive scans. The split-axis distance is a conservative lower bound,
    /// so the opposite branch is pruned only when it cannot contain a smaller
    /// result; classification thresholds therefore remain unchanged.
    /// </summary>
    private sealed class ExactVector3NearestIndex
    {
        private static readonly IComparer<Vector3>[] AxisComparers =
        [
            Comparer<Vector3>.Create((left, right) => Compare(left, right, 0)),
            Comparer<Vector3>.Create((left, right) => Compare(left, right, 1)),
            Comparer<Vector3>.Create((left, right) => Compare(left, right, 2))
        ];

        private readonly Vector3[] _positions;
        private readonly byte[] _axes;
        private readonly int[] _left;
        private readonly int[] _right;
        private readonly int _root;

        public ExactVector3NearestIndex(IReadOnlyList<Vector3> positions)
        {
            ArgumentNullException.ThrowIfNull(positions);
            if (positions.Count == 0)
            {
                throw new ArgumentException(
                    "A nearest-position index requires at least one point.",
                    nameof(positions));
            }

            Vector3[] working = positions.ToArray();
            _positions = new Vector3[working.Length];
            _axes = new byte[working.Length];
            _left = Enumerable.Repeat(-1, working.Length).ToArray();
            _right = Enumerable.Repeat(-1, working.Length).ToArray();
            int next = 0;
            _root = Build(working, 0, working.Length, depth: 0, ref next);
            if (next != working.Length)
            {
                throw new InvalidOperationException(
                    "Nearest-position index did not consume every source point.");
            }
        }

        public float FindNearestDistanceSquared(Vector3 query) =>
            Search(_root, query, float.PositiveInfinity);

        private int Build(
            Vector3[] working,
            int start,
            int count,
            int depth,
            ref int next)
        {
            if (count == 0)
                return -1;

            int axis = depth % 3;
            Array.Sort(working, start, count, AxisComparers[axis]);
            int median = start + count / 2;
            int node = next++;
            _positions[node] = working[median];
            _axes[node] = checked((byte)axis);
            _left[node] = Build(
                working,
                start,
                median - start,
                depth + 1,
                ref next);
            _right[node] = Build(
                working,
                median + 1,
                start + count - median - 1,
                depth + 1,
                ref next);
            return node;
        }

        private float Search(int node, Vector3 query, float best)
        {
            if (node < 0)
                return best;

            Vector3 position = _positions[node];
            float distance = Vector3.DistanceSquared(query, position);
            if (distance < best)
                best = distance;

            int axis = _axes[node];
            float delta = Component(query, axis) - Component(position, axis);
            int near = delta <= 0 ? _left[node] : _right[node];
            int far = delta <= 0 ? _right[node] : _left[node];
            best = Search(near, query, best);
            float splitDistance = delta * delta;
            if (splitDistance <= best)
                best = Search(far, query, best);
            return best;
        }

        private static int Compare(Vector3 left, Vector3 right, int axis)
        {
            for (int offset = 0; offset < 3; offset++)
            {
                int component = (axis + offset) % 3;
                int comparison = Component(left, component).CompareTo(
                    Component(right, component));
                if (comparison != 0)
                    return comparison;
            }
            return 0;
        }

        private static float Component(Vector3 value, int axis) => axis switch
        {
            0 => value.X,
            1 => value.Y,
            2 => value.Z,
            _ => throw new ArgumentOutOfRangeException(nameof(axis))
        };
    }

    private sealed record SemanticRegionCalibration(
        GeneratedSkinningSemanticRegion Region,
        string AnchorBoneName,
        int AnchorRigJointIndex,
        int AnchorSkeletonJointIndex,
        string ProximalBoneName,
        int ProximalRigJointIndex,
        int ProximalSkeletonJointIndex,
        Vector3 PosedAnchor,
        GeneratedSkinningRegionVolume AutomaticVolume,
        GeneratedSkinningRegionAdjustmentLimits AdjustmentLimits,
        int CalibrationSampleCount)
    {
        public SemanticHeadLobe? DonorHeadLobe { get; init; }

        public SemanticHandLobe? DonorHandLobe { get; init; }

        public CoarseHandMotionProfile? HandMotionProfile { get; init; }
    }

    private sealed record SemanticRegionCandidate(
        GeneratedSkinningRegionResolution Resolution,
        IReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment> Assignments);

    private static SemanticRegionPreparation ResolveSemanticRegions(
        SmoDocument target,
        ImportedScene donor,
        TargetRigDefinition rig,
        TargetSkeletonLayout targetSkeleton,
        SmoExportScene targetScene,
        IReadOnlyList<SmoExportMesh> targetSkinnedMeshes,
        RobustBounds targetBounds,
        IReadOnlyList<Matrix4x4>? fittingWorldMatrices,
        IReadOnlyList<GeometryComponent> donorBodyComponents,
        IReadOnlyList<GeometrySource> donorSources,
        SceneTopology donorTopology,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        SideCalibration sideCalibration,
        IReadOnlyList<BoneCapsule> capsules,
        IReadOnlyDictionary<int, AnatomicalVolume> anatomicalVolumes,
        int maximumInfluences,
        GeneratedSkinningAlignment alignment,
        TargetRigFittingPoseSnapshot? fittingPose,
        IReadOnlySet<int> manuallyAssignedComponentIndices,
        GeneratedSkinningRegionOverrides? overrides)
    {
        string targetFingerprint = TargetRigDefinition.ComputeSourceFingerprint(target);
        string donorFingerprint =
            TargetRigAutomaticPoseFitter.ComputeDonorGeometryFingerprint(donor);
        string alignmentFingerprint = ComputeRegionAlignmentFingerprint(alignment);
        string fittingPoseFingerprint = ComputeRegionFittingPoseFingerprint(
            rig,
            fittingWorldMatrices);
        Dictionary<GeneratedSkinningSemanticRegion, GeneratedSkinningRegionAdjustment>
            adjustments = ValidateSemanticRegionOverrides(
                overrides,
                targetFingerprint,
                donorFingerprint,
                alignmentFingerprint,
                fittingPoseFingerprint);

        IReadOnlyList<DecodedTargetWeightSample> targetSamples =
            DecodeTargetWeightSamples(rig, targetScene, targetSkinnedMeshes);
        IReadOnlyDictionary<GeometryVertex, IReadOnlyList<GeometryVertex>> adjacency =
            BuildSelectedBodyAdjacency(
                donorBodyComponents,
                donorSources,
                donorTopology);
        IReadOnlyDictionary<GeometryVertex, GeneratedVertexInfluences>
            capsuleInfluences = new ReadOnlyDictionary<GeometryVertex,
                GeneratedVertexInfluences>(donorBodyComponents
                .SelectMany(component => component.Vertices)
                .Distinct()
                .OrderBy(vertex => vertex.MeshIndex)
                .ThenBy(vertex => vertex.VertexIndex)
                .ToDictionary(
                    vertex => vertex,
                    vertex => GenerateVertexInfluences(
                        alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                        capsules,
                        anatomicalVolumes,
                        sideCalibration,
                        targetBounds.Size.Y,
                        maximumInfluences)));
        var candidates = new Dictionary<GeneratedSkinningSemanticRegion,
            SemanticRegionCandidate>();
        foreach (GeneratedSkinningSemanticRegion region in
                 Enum.GetValues<GeneratedSkinningSemanticRegion>())
        {
            GeneratedSkinningRegionAdjustment adjustment = adjustments[region];
            var messages = new List<string>();
            SemanticRegionCalibration? targetCalibration = TryCalibrateSemanticRegion(
                region,
                rig,
                targetSkeleton,
                targetSamples,
                targetBounds,
                fittingWorldMatrices,
                messages);
            SemanticRegionCalibration? calibration = targetCalibration;
            if (targetCalibration is not null)
            {
                calibration = region == GeneratedSkinningSemanticRegion.Head
                    ? TryRefineHeadCalibrationFromDonorTopology(
                        targetCalibration,
                        donorBodyComponents,
                        alignedPositionsByMesh,
                        adjacency,
                        targetBounds.Size.Y,
                        messages)
                    : TryRefineCompleteHandCalibrationFromDonorTopology(
                        targetCalibration,
                        donorBodyComponents,
                        alignedPositionsByMesh,
                        adjacency,
                        sideCalibration,
                        targetSkeleton.Skeleton,
                        capsuleInfluences,
                        targetBounds.Size.Y,
                        messages);
            }
            bool manualFallback = calibration is null &&
                                  targetCalibration is not null &&
                                  overrides is not null &&
                                  adjustment.Enabled;
            if (manualFallback)
            {
                // Automatic donor-topology discovery is deliberately strict, but
                // its failure must not make the editor useless.  An explicit
                // user adjustment may start from the finite target-derived bone
                // volume; the ordinary capture/overlap/seam checks below still
                // decide whether that manually positioned volume is safe enough
                // to commit.
                calibration = targetCalibration;
                messages.Add(
                    $"Semantic {region} uses the manually enabled target-derived " +
                    "fallback volume because automatic donor topology did not " +
                    "resolve an unambiguous lobe.");
            }
            SemanticRegionCalibration? displayCalibration =
                calibration ?? targetCalibration;
            if (!adjustment.Enabled)
            {
                candidates.Add(region, new SemanticRegionCandidate(
                    CreateSemanticRegionResolution(
                        region,
                        adjustment,
                        displayCalibration,
                        GeneratedSkinningRegionStatus.Disabled,
                        isApplied: false,
                        resolvedVolume: displayCalibration?.AutomaticVolume,
                        coreVertices: [],
                        transitionVertices: [],
                        messages),
                    new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
                        new Dictionary<GeometryVertex, SemanticVertexAssignment>())));
                continue;
            }
            if (calibration is null)
            {
                candidates.Add(region, new SemanticRegionCandidate(
                    CreateSemanticRegionResolution(
                        region,
                        adjustment,
                        displayCalibration,
                        GeneratedSkinningRegionStatus.UnsafeCalibration,
                        isApplied: false,
                        resolvedVolume: displayCalibration?.AutomaticVolume,
                        coreVertices: [],
                        transitionVertices: [],
                        messages),
                    new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
                        new Dictionary<GeometryVertex, SemanticVertexAssignment>())));
                continue;
            }

            ValidateSemanticAdjustment(adjustment, calibration.AdjustmentLimits);
            GeneratedSkinningRegionVolume resolvedVolume = ApplySemanticAdjustment(
                calibration.AutomaticVolume,
                adjustment);
            bool useManualVolumeMembership =
                region == GeneratedSkinningSemanticRegion.Head &&
                overrides is not null &&
                IsManualSemanticVolumeAdjustment(adjustment);
            if (!TryCaptureSemanticRegion(
                    calibration,
                    resolvedVolume,
                    donorBodyComponents,
                    donorSources,
                    alignedPositionsByMesh,
                    adjacency,
                    sideCalibration,
                    useManualVolumeMembership,
                    out IReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>
                        assignments,
                    out IReadOnlyList<TargetRigBodyVertexMembership> coreVertices,
                    out IReadOnlyList<TargetRigBodyVertexMembership> transitionVertices,
                    out string? captureDiagnostic))
            {
                if (!string.IsNullOrWhiteSpace(captureDiagnostic))
                    messages.Add(captureDiagnostic);
                candidates.Add(region, new SemanticRegionCandidate(
                    CreateSemanticRegionResolution(
                        region,
                        adjustment,
                        calibration,
                        GeneratedSkinningRegionStatus.UnsafeCalibration,
                        isApplied: false,
                        resolvedVolume,
                        coreVertices: [],
                        transitionVertices: [],
                        messages),
                    new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
                        new Dictionary<GeometryVertex, SemanticVertexAssignment>())));
                continue;
            }

            if (!string.IsNullOrWhiteSpace(captureDiagnostic))
                messages.Add(captureDiagnostic);

            messages.Add(
                $"Semantic {region} protected core captured " +
                $"{CountMembershipVertices(coreVertices)} donor vertex/vertices; " +
                $"its proximal two-bone transition captured " +
                $"{CountMembershipVertices(transitionVertices)} vertex/vertices.");
            GeneratedSkinningRegionResolution appliedResolution =
                CreateSemanticRegionResolution(
                    region,
                    adjustment,
                    calibration,
                    GeneratedSkinningRegionStatus.Applied,
                    isApplied: true,
                    resolvedVolume,
                    coreVertices,
                    transitionVertices,
                    messages) with
                {
                    CoarseMotionVertexCount = assignments.Values.Count(
                        value => value.MotionProxyWeight > WeightEpsilon),
                    MaximumMotionProxyWeight = assignments.Values
                        .Select(value => value.MotionProxyWeight)
                        .DefaultIfEmpty()
                        .Max()
                };
            candidates.Add(region, new SemanticRegionCandidate(
                appliedResolution,
                assignments));
        }

        HashSet<GeneratedSkinningSemanticRegion> overlapRejected =
            FindOverlappingRegions(
            candidates);
        var seamDiagnostics = new Dictionary<GeneratedSkinningSemanticRegion,
            List<string>>();
        var rejected = new HashSet<GeneratedSkinningSemanticRegion>(overlapRejected);
        rejected.UnionWith(FindSeamDivergentRegions(
            candidates,
            donorBodyComponents,
            donorSources,
            rejected,
            seamDiagnostics));
        foreach (GeneratedSkinningSemanticRegion region in rejected)
        {
            SemanticRegionCandidate candidate = candidates[region];
            string diagnostic = overlapRejected.Contains(region)
                ? $"Semantic {region} was conservatively disabled because its exact " +
                  "captured membership overlaps another semantic region."
                : $"Semantic {region} was conservatively disabled because its exact " +
                  "captured membership diverges across coincident attribute-seam " +
                  "vertices.";
            if (seamDiagnostics.TryGetValue(region, out List<string>? details))
                diagnostic += " " + string.Join(" | ", details);
            GeneratedSkinningRegionResolution resolution = candidate.Resolution with
            {
                IsApplied = false,
                Status = GeneratedSkinningRegionStatus.UnsafeCalibration,
                CoreVerticesByMesh = Array.Empty<TargetRigBodyVertexMembership>(),
                TransitionVerticesByMesh = Array.Empty<TargetRigBodyVertexMembership>(),
                Warnings = new ReadOnlyCollection<string>(
                    candidate.Resolution.Warnings.Append(diagnostic).ToArray())
            };
            candidates[region] = new SemanticRegionCandidate(
                resolution,
                new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
                    new Dictionary<GeometryVertex, SemanticVertexAssignment>()));
        }

        var finalAssignments = new Dictionary<GeometryVertex, SemanticVertexAssignment>();
        foreach (SemanticRegionCandidate candidate in candidates.Values
                     .Where(value => value.Resolution.IsApplied)
                     .OrderBy(value => value.Resolution.Region))
        {
            foreach ((GeometryVertex vertex, SemanticVertexAssignment assignment) in
                     candidate.Assignments)
            {
                if (!finalAssignments.TryAdd(vertex, assignment))
                {
                    throw new InvalidDataException(
                        "Semantic-region overlap survived conservative validation.");
                }
            }
        }

        IReadOnlyDictionary<int, SemanticComponentAssignment> componentAssignments =
            ResolveHeadCompanionComponents(
                candidates,
                donorBodyComponents,
                donorSources,
                donorTopology,
                alignedPositionsByMesh,
                manuallyAssignedComponentIndices);
        var analysis = new GeneratedSkinningRegionAnalysis(
            new ReadOnlyCollection<GeneratedSkinningRegionResolution>(
                candidates.OrderBy(pair => pair.Key)
                    .Select(pair => pair.Value.Resolution)
                    .ToArray()),
            targetFingerprint,
            donorFingerprint,
            alignmentFingerprint,
            fittingPoseFingerprint);
        return new SemanticRegionPreparation(
            analysis,
            new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
                finalAssignments),
            componentAssignments,
            capsuleInfluences);
    }

    private static Dictionary<GeneratedSkinningSemanticRegion,
        GeneratedSkinningRegionAdjustment> ValidateSemanticRegionOverrides(
        GeneratedSkinningRegionOverrides? overrides,
        string targetFingerprint,
        string donorFingerprint,
        string alignmentFingerprint,
        string fittingPoseFingerprint)
    {
        var result = Enum.GetValues<GeneratedSkinningSemanticRegion>()
            .ToDictionary(
                region => region,
                GeneratedSkinningRegionAdjustment.Automatic);
        if (overrides is null)
            return result;
        ArgumentNullException.ThrowIfNull(overrides.Adjustments);
        ValidateRegionFingerprint(
            overrides.TargetRigFingerprint,
            targetFingerprint,
            "target rig");
        ValidateRegionFingerprint(
            overrides.DonorGeometryFingerprint,
            donorFingerprint,
            "donor geometry");
        ValidateRegionFingerprint(
            overrides.AlignmentFingerprint,
            alignmentFingerprint,
            "donor alignment");
        ValidateRegionFingerprint(
            overrides.FittingPoseFingerprint,
            fittingPoseFingerprint,
            "fitting pose");
        foreach (GeneratedSkinningRegionAdjustment adjustment in overrides.Adjustments)
        {
            if (!Enum.IsDefined(adjustment.Region))
            {
                throw new InvalidDataException(
                    $"Unknown generated-skinning semantic region {adjustment.Region}.");
            }
            if (!float.IsFinite(adjustment.AxialOffset) ||
                !float.IsFinite(adjustment.AxialScale) ||
                !float.IsFinite(adjustment.RadialScale) ||
                !float.IsFinite(adjustment.ForwardTiltDegrees) ||
                adjustment.AxialScale <= 0 || adjustment.RadialScale <= 0)
            {
                throw new InvalidDataException(
                    $"Semantic {adjustment.Region} adjustment must contain a finite " +
                    "axial offset and positive finite scales.");
            }
            if (adjustment.Region == GeneratedSkinningSemanticRegion.Head)
            {
                if (MathF.Abs(adjustment.ForwardTiltDegrees) >
                    MaximumHeadForwardTiltDegrees)
                {
                    throw new InvalidDataException(
                        $"Semantic Head forward tilt must be between " +
                        $"{-MaximumHeadForwardTiltDegrees:G6} and " +
                        $"{MaximumHeadForwardTiltDegrees:G6} degrees.");
                }
            }
            else if (MathF.Abs(adjustment.ForwardTiltDegrees) > PositionEpsilon)
            {
                throw new InvalidDataException(
                    $"Semantic {adjustment.Region} does not support head tilt.");
            }
            if (!result.TryAdd(adjustment.Region, adjustment))
            {
                // The automatic dictionary already owns every key. Replace it only
                // after proving that this is the first explicit descriptor.
                if (overrides.Adjustments.Count(value =>
                        value.Region == adjustment.Region) != 1)
                {
                    throw new InvalidDataException(
                        $"Semantic {adjustment.Region} has duplicate adjustments.");
                }
                result[adjustment.Region] = adjustment;
            }
        }
        return result;
    }

    private static void ValidateRegionFingerprint(
        string supplied,
        string expected,
        string label)
    {
        if (string.IsNullOrWhiteSpace(supplied) ||
            !string.Equals(supplied, expected, StringComparison.Ordinal))
        {
            throw new InvalidOperationException(
                $"Semantic-region overrides were captured for stale {label} state.");
        }
    }

    private static IReadOnlyList<DecodedTargetWeightSample> DecodeTargetWeightSamples(
        TargetRigDefinition rig,
        SmoExportScene targetScene,
        IReadOnlyList<SmoExportMesh> meshes)
    {
        Dictionary<int, SmoExportSkin> skins = targetScene.Skins
            .GroupBy(skin => skin.ObjectIndex)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single());
        var result = new List<DecodedTargetWeightSample>();
        foreach (SmoExportMesh mesh in meshes)
        {
            if (mesh.SkinObjectIndex is not int skinObjectIndex ||
                !skins.TryGetValue(skinObjectIndex, out SmoExportSkin? skin))
                continue;
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
                var accumulated = new Dictionary<int, float>();
                float total = 0;
                for (int slot = 0; slot < 4; slot++)
                {
                    float weight = VectorComponent(mesh.BlendWeights[vertex], slot);
                    if (!float.IsFinite(weight) || weight < 0)
                    {
                        throw new InvalidDataException(
                            "Target SMO contains a non-finite or negative skin weight.");
                    }
                    if (weight <= WeightEpsilon)
                        continue;
                    total += weight;
                    float indexValue = VectorComponent(mesh.JointIndices[vertex], slot);
                    if (!float.IsFinite(indexValue) ||
                        indexValue < int.MinValue ||
                        indexValue > int.MaxValue)
                    {
                        throw new InvalidDataException(
                            "Target SMO contains an out-of-range weighted skin " +
                            "palette index.");
                    }
                    int paletteIndex = checked((int)MathF.Round(indexValue));
                    if (MathF.Abs(indexValue - paletteIndex) > 0.001f ||
                        (uint)paletteIndex >= (uint)skin.JointObjectIndices.Count)
                    {
                        throw new InvalidDataException(
                            "Target SMO contains an invalid weighted skin palette index.");
                    }
                    int rigJoint;
                    try
                    {
                        rigJoint = rig.GetJointIndexByObjectIndex(
                            skin.JointObjectIndices[paletteIndex]);
                    }
                    catch (KeyNotFoundException)
                    {
                        continue;
                    }
                    accumulated[rigJoint] =
                        accumulated.GetValueOrDefault(rigJoint) + weight;
                }
                if (float.IsFinite(total) && total > WeightEpsilon)
                {
                    result.Add(new DecodedTargetWeightSample(
                        mesh.Positions[vertex],
                        new ReadOnlyDictionary<int, float>(accumulated),
                        total));
                }
            }
        }
        return new ReadOnlyCollection<DecodedTargetWeightSample>(result);
    }

    private static SemanticRegionCalibration? TryCalibrateSemanticRegion(
        GeneratedSkinningSemanticRegion region,
        TargetRigDefinition rig,
        TargetSkeletonLayout layout,
        IReadOnlyList<DecodedTargetWeightSample> targetSamples,
        RobustBounds targetBounds,
        IReadOnlyList<Matrix4x4>? fittingWorldMatrices,
        ICollection<string> messages)
    {
        string anchorName = region switch
        {
            GeneratedSkinningSemanticRegion.Head => "Head",
            GeneratedSkinningSemanticRegion.LeftHand => "L_Hand",
            GeneratedSkinningSemanticRegion.RightHand => "R_Hand",
            _ => throw new ArgumentOutOfRangeException(nameof(region))
        };
        TargetRigJoint[] anchors = layout.DeformJoints
            .Where(joint => string.Equals(
                joint.Name, anchorName, StringComparison.Ordinal))
            .ToArray();
        if (anchors.Length != 1)
        {
            messages.Add(
                $"Semantic {region} calibration requires one exact deform joint " +
                $"named '{anchorName}', but found {anchors.Length}.");
            return null;
        }
        TargetRigJoint anchor = anchors[0];
        int proximalRigJoint = FindNearestDeformParent(rig, anchor.JointIndex);
        if (proximalRigJoint < 0)
        {
            messages.Add(
                $"Semantic {region} calibration found no proximal deform parent for " +
                $"'{anchorName}'.");
            return null;
        }
        TargetRigJoint proximal = rig.Joints[proximalRigJoint];
        if (region == GeneratedSkinningSemanticRegion.Head &&
            !string.Equals(proximal.Name, "Neck", StringComparison.Ordinal))
        {
            messages.Add(
                $"Semantic Head calibration requires Neck to be the nearest deform " +
                $"parent of Head; found '{proximal.Name}'.");
            return null;
        }
        if (!layout.SkeletonIndexByRigJoint.TryGetValue(
                anchor.JointIndex, out int anchorSkeletonIndex) ||
            !layout.SkeletonIndexByRigJoint.TryGetValue(
                proximalRigJoint, out int proximalSkeletonIndex))
        {
            messages.Add(
                $"Semantic {region} anchor or proximal joint is absent from the " +
                "generated target skeleton.");
            return null;
        }

        HashSet<int> subtree = CollectDeformSubtree(rig, layout, anchor.JointIndex);
        int[] orderedSubtree = subtree.Order().ToArray();
        TargetRigJoint[] unsafeDescendants = layout.DeformJoints
            .Where(joint => subtree.Contains(joint.JointIndex) &&
                            !IsSafeAutomaticWeightBone(joint.Name))
            .ToArray();
        if (unsafeDescendants.Length > 0)
        {
            messages.Add(
                $"Semantic {region} subtree contains unsafe deform descendants: " +
                string.Join(", ", unsafeDescendants.Select(joint => joint.Name)) + ".");
            return null;
        }

        Vector3[] coreSamples = targetSamples
            .Where(sample =>
            {
                float semantic = orderedSubtree.Sum(joint =>
                    sample.WeightByRigJoint.GetValueOrDefault(joint));
                return semantic / sample.TotalWeight >= SemanticCoreWeightThreshold;
            })
            .Select(sample => sample.Position)
            .Distinct()
            .ToArray();
        Vector3[] transitionSamples = targetSamples
            .Where(sample =>
            {
                float semantic = orderedSubtree.Sum(joint =>
                    sample.WeightByRigJoint.GetValueOrDefault(joint));
                float parent = sample.WeightByRigJoint.GetValueOrDefault(
                    proximalRigJoint);
                return semantic / sample.TotalWeight >=
                           SemanticTransitionWeightThreshold &&
                       parent / sample.TotalWeight >=
                           SemanticTransitionWeightThreshold &&
                       (semantic + parent) / sample.TotalWeight >=
                           SemanticTransitionPairThreshold;
            })
            .Select(sample => sample.Position)
            .Distinct()
            .ToArray();
        if (coreSamples.Length < MinimumSemanticCoreSamples ||
            transitionSamples.Length < MinimumSemanticTransitionSamples)
        {
            messages.Add(
                $"Semantic {region} calibration was skipped: target subtree " +
                $"'{anchorName}' supplied {coreSamples.Length} strict core and " +
                $"{transitionSamples.Length} proximal transition sample(s); at least " +
                $"{MinimumSemanticCoreSamples}/{MinimumSemanticTransitionSamples} are " +
                "required.");
            return null;
        }

        Vector3 bindProximal = Translation(proximal.BindWorldMatrix);
        Vector3 bindAnchor = Translation(anchor.BindWorldMatrix);
        SideCalibration bindSides = CalibrateSides(
            layout.DeformJoints,
            targetBounds,
            fittingWorldMatrices: null);
        BuildSemanticFrame(
            bindProximal,
            bindAnchor,
            anchor.BindWorldMatrix,
            GetCalibratedLateralAxis(bindSides),
            out Vector3 bindAxial,
            out Vector3 bindLateral,
            out Vector3 bindForward);

        float[] coreAxial = Project(coreSamples, bindAnchor, bindAxial);
        float[] transitionAxial = Project(
            transitionSamples, bindAnchor, bindAxial);
        float coreMedian = Quantile(coreAxial, 0.5f);
        float transitionMedian = Quantile(transitionAxial, 0.5f);
        float minimumRadius = MathF.Max(
            targetBounds.Size.Y * MinimumSemanticRadiusHeightRatio,
            PositionEpsilon * 16);
        if (!float.IsFinite(coreMedian) || !float.IsFinite(transitionMedian) ||
            transitionMedian >= coreMedian - minimumRadius * 0.25f)
        {
            messages.Add(
                $"Semantic {region} calibration was skipped because target " +
                "subtree/parent weights do not form a separated proximal transition.");
            return null;
        }

        float transitionObservedLower = Quantile(
            transitionAxial, RobustLowerQuantile);
        float transitionObservedUpper = Quantile(
            transitionAxial, RobustUpperQuantile);
        float observedTransitionLength = MathF.Max(
            transitionObservedUpper - transitionObservedLower,
            minimumRadius);
        float proximalLower = transitionObservedLower -
                              observedTransitionLength * 0.5f;
        float transitionEnd = MathF.Max(
            transitionObservedUpper,
            Quantile(coreAxial, RobustLowerQuantile));
        if (transitionEnd <= proximalLower + PositionEpsilon)
        {
            messages.Add(
                $"Semantic {region} target transition has a degenerate axial extent.");
            return null;
        }

        float observedDistalUpper = Quantile(coreAxial, RobustUpperQuantile);
        if (observedDistalUpper <= transitionEnd + minimumRadius)
            observedDistalUpper = transitionEnd + minimumRadius;
        CoarseHandMotionProfile? handMotionProfile =
            TryBuildCoarseHandMotionProfile(
                region,
                rig,
                layout,
                anchor,
                targetSamples,
                bindAnchor,
                bindAxial,
                bindLateral,
                bindForward,
                proximalLower,
                observedDistalUpper,
                messages);
        float distalUpper = observedDistalUpper;
        float distalExpansion = region == GeneratedSkinningSemanticRegion.Head
            ? 1.5f
            : 2.75f;
        distalUpper = proximalLower +
                      (distalUpper - proximalLower) * distalExpansion;

        Vector3[] radialSamples = coreSamples.Concat(transitionSamples).ToArray();
        (float lateralCenter, float lateralRadius) = ProjectedBounds(
            radialSamples,
            bindAnchor,
            bindLateral,
            minimumRadius);
        (float forwardCenter, float forwardRadius) = ProjectedBounds(
            radialSamples,
            bindAnchor,
            bindForward,
            minimumRadius);
        float radialExpansion = region == GeneratedSkinningSemanticRegion.Head
            ? 1.35f
            : 1.75f;
        lateralRadius *= radialExpansion;
        forwardRadius *= radialExpansion;

        float axialCenter = (proximalLower + distalUpper) * 0.5f;
        float axialRadius = MathF.Max(
            (distalUpper - proximalLower) * 0.5f,
            minimumRadius);
        float transitionLength = Math.Clamp(
            transitionEnd - proximalLower,
            minimumRadius,
            axialRadius * 1.5f);

        Matrix4x4 posedAnchorMatrix = GetFittingWorldMatrix(
            anchor,
            fittingWorldMatrices);
        Vector3 posedProximal = Translation(GetFittingWorldMatrix(
            proximal,
            fittingWorldMatrices));
        Vector3 posedAnchor = Translation(posedAnchorMatrix);
        SideCalibration posedSides = fittingWorldMatrices is null
            ? bindSides
            : CalibrateSides(layout.DeformJoints, targetBounds, fittingWorldMatrices);
        BuildPosedSemanticFrame(
            bindAxial,
            bindLateral,
            bindForward,
            anchor.BindWorldMatrix,
            posedProximal,
            posedAnchor,
            posedAnchorMatrix,
            GetCalibratedLateralAxis(posedSides),
            out Vector3 posedAxial,
            out Vector3 posedLateral,
            out Vector3 posedForward);
        Vector3 center = posedAnchor +
                         posedAxial * axialCenter +
                         posedLateral * lateralCenter +
                         posedForward * forwardCenter;
        var automaticVolume = new GeneratedSkinningRegionVolume(
            center,
            posedAxial,
            posedLateral,
            posedForward,
            axialRadius,
            lateralRadius,
            forwardRadius,
            transitionLength)
        {
            ShapeExponent = region == GeneratedSkinningSemanticRegion.Head
                ? 2
                : 4
        };
        // A modular character head is commonly split into a face, scalp, hair
        // cap, eyes and mouth shells.  Automatic topology proof intentionally
        // stays conservative and may use only the compact connected head lobe
        // as its base.  Giving Head the same 1.75x editor ceiling as a hand can
        // therefore leave the largest user-authored ellipsoid visibly inside
        // the assembled head.  The larger Head-only range remains finite and
        // explicit; exact membership, semantic-overlap and seam validation are
        // still performed when the user presses Apply.  Hands retain their
        // tighter range so they cannot expand into forearms or the torso.
        float maximumEditorScale =
            region == GeneratedSkinningSemanticRegion.Head
                ? MaximumHeadProtectedRegionScale
                : MaximumProtectedRegionScale;
        var limits = new GeneratedSkinningRegionAdjustmentLimits(
            -axialRadius * 0.35f,
            axialRadius * 0.35f,
            0.5f,
            maximumEditorScale,
            0.5f,
            maximumEditorScale);
        messages.Add(
            $"Semantic {region} target calibration used {coreSamples.Length} strict " +
            $"subtree core and {transitionSamples.Length} {anchorName}+" +
            $"{proximal.Name} transition samples; automatic radii are " +
            $"({axialRadius:G6}, {lateralRadius:G6}, {forwardRadius:G6}).");
        return new SemanticRegionCalibration(
            region,
            anchorName,
            anchor.JointIndex,
            anchorSkeletonIndex,
            proximal.Name,
            proximalRigJoint,
            proximalSkeletonIndex,
            posedAnchor,
            automaticVolume,
            limits,
            coreSamples.Length + transitionSamples.Length)
        {
            HandMotionProfile = handMotionProfile
        };
    }

    private static HashSet<int> CollectDeformSubtree(
        TargetRigDefinition rig,
        TargetSkeletonLayout layout,
        int anchorRigJoint)
    {
        var result = new HashSet<int>();
        foreach (TargetRigJoint joint in layout.DeformJoints)
        {
            var visited = new HashSet<int>();
            int cursor = joint.JointIndex;
            bool reachedAnchor = false;
            bool cycleDetected = false;
            while (cursor >= 0)
            {
                if (!visited.Add(cursor))
                {
                    cycleDetected = true;
                    break;
                }
                if (cursor == anchorRigJoint)
                {
                    reachedAnchor = true;
                    break;
                }
                cursor = rig.Joints[cursor].ParentJointIndex;
            }
            if (cycleDetected)
            {
                throw new InvalidDataException(
                    "Target fitting rig contains a cycle in a semantic subtree.");
            }
            if (reachedAnchor)
                result.Add(joint.JointIndex);
        }
        result.Add(anchorRigJoint);
        return result;
    }

    private static void BuildSemanticFrame(
        Vector3 proximal,
        Vector3 anchor,
        Matrix4x4 anchorWorld,
        Vector3 preferredLateral,
        out Vector3 axial,
        out Vector3 lateral,
        out Vector3 forward)
    {
        Vector3 direction = anchor - proximal;
        if (!IsFinite(direction) || direction.LengthSquared() <= PositionEpsilon)
            throw new InvalidDataException("A semantic region has a degenerate bone axis.");
        axial = Vector3.Normalize(direction);
        Vector3[] hints =
        [
            preferredLateral,
            Vector3.TransformNormal(Vector3.UnitX, anchorWorld),
            Vector3.TransformNormal(Vector3.UnitY, anchorWorld),
            Vector3.TransformNormal(Vector3.UnitZ, anchorWorld)
        ];
        Vector3 axialValue = axial;
        Vector3 projected = hints
            .Where(IsFinite)
            .Select(hint =>
                hint - axialValue * Vector3.Dot(hint, axialValue))
            .OrderByDescending(value => value.LengthSquared())
            .FirstOrDefault();
        if (!IsFinite(projected) || projected.LengthSquared() <= PositionEpsilon)
            throw new InvalidDataException("A semantic region frame is degenerate.");
        lateral = Vector3.Normalize(projected);
        forward = Vector3.Normalize(Vector3.Cross(lateral, axial));
        if (!IsFinite(forward))
            throw new InvalidDataException("A semantic region forward axis is invalid.");
    }

    private static void BuildPosedSemanticFrame(
        Vector3 bindAxial,
        Vector3 bindLateral,
        Vector3 bindForward,
        Matrix4x4 bindAnchorWorld,
        Vector3 posedProximal,
        Vector3 posedAnchor,
        Matrix4x4 posedAnchorWorld,
        Vector3 posedLateralFallback,
        out Vector3 posedAxial,
        out Vector3 posedLateral,
        out Vector3 posedForward)
    {
        Vector3 posedDirection = posedAnchor - posedProximal;
        if (!IsFinite(posedDirection) ||
            posedDirection.LengthSquared() <= PositionEpsilon)
        {
            throw new InvalidDataException(
                "A posed semantic region has a degenerate bone axis.");
        }
        posedAxial = Vector3.Normalize(posedDirection);
        if (!Matrix4x4.Invert(bindAnchorWorld, out Matrix4x4 inverseBindAnchor) ||
            !IsFinite(inverseBindAnchor))
        {
            throw new InvalidDataException(
                "A semantic anchor has no finite inverse bind transform.");
        }
        Vector3 localLateral = Vector3.TransformNormal(
            bindLateral, inverseBindAnchor);
        Vector3 localForward = Vector3.TransformNormal(
            bindForward, inverseBindAnchor);
        Vector3 transportedLateral = Vector3.TransformNormal(
            localLateral, posedAnchorWorld);
        transportedLateral -= posedAxial *
                              Vector3.Dot(transportedLateral, posedAxial);
        if (!IsFinite(transportedLateral) ||
            transportedLateral.LengthSquared() <= PositionEpsilon)
        {
            transportedLateral = posedLateralFallback -
                posedAxial * Vector3.Dot(posedLateralFallback, posedAxial);
        }
        if (!IsFinite(transportedLateral) ||
            transportedLateral.LengthSquared() <= PositionEpsilon)
        {
            // This is an internal frame transport fallback, not a semantic
            // donor-name heuristic. Reuse the deterministic frame builder only
            // when the target anchor's transported transverse axis collapses.
            BuildSemanticFrame(
                posedProximal,
                posedAnchor,
                posedAnchorWorld,
                posedLateralFallback,
                out posedAxial,
                out posedLateral,
                out posedForward);
            return;
        }
        posedLateral = Vector3.Normalize(transportedLateral);
        posedForward = Vector3.Normalize(Vector3.Cross(
            posedLateral, posedAxial));
        Vector3 transportedForward = Vector3.TransformNormal(
            localForward, posedAnchorWorld);
        if (IsFinite(transportedForward) &&
            transportedForward.LengthSquared() > PositionEpsilon &&
            Vector3.Dot(posedForward, transportedForward) < 0)
        {
            posedLateral = -posedLateral;
            posedForward = -posedForward;
        }
        if (!IsFinite(posedAxial) || !IsFinite(posedLateral) ||
            !IsFinite(posedForward) ||
            Vector3.Dot(bindAxial, bindAxial) <= PositionEpsilon)
        {
            throw new InvalidDataException(
                "A transported semantic region frame is invalid.");
        }
    }

    private static float[] Project(
        IReadOnlyList<Vector3> positions,
        Vector3 origin,
        Vector3 axis) =>
        positions.Select(position => Vector3.Dot(position - origin, axis))
            .Order()
            .ToArray();

    private static (float Center, float Radius) ProjectedBounds(
        IReadOnlyList<Vector3> positions,
        Vector3 origin,
        Vector3 axis,
        float minimumRadius)
    {
        float[] projected = Project(positions, origin, axis);
        float lower = Quantile(projected, RobustLowerQuantile);
        float upper = Quantile(projected, RobustUpperQuantile);
        return ((lower + upper) * 0.5f,
            MathF.Max((upper - lower) * 0.5f, minimumRadius));
    }

    private static GeneratedSkinningRegionVolume ApplySemanticAdjustment(
        GeneratedSkinningRegionVolume automatic,
        GeneratedSkinningRegionAdjustment adjustment)
    {
        float axialRadius = automatic.AxialRadius * adjustment.AxialScale;
        Vector3 automaticProximal = automatic.Center -
                                    automatic.AxialAxis * automatic.AxialRadius;
        Vector3 center = automaticProximal +
                         automatic.AxialAxis * axialRadius +
                         automatic.AxialAxis * adjustment.AxialOffset;
        float angle = adjustment.ForwardTiltDegrees * (MathF.PI / 180f);
        float cosine = MathF.Cos(angle);
        float sine = MathF.Sin(angle);
        Vector3 axialAxis = Vector3.Normalize(
            automatic.AxialAxis * cosine - automatic.ForwardAxis * sine);
        Vector3 forwardAxis = Vector3.Normalize(
            automatic.ForwardAxis * cosine + automatic.AxialAxis * sine);
        return automatic with
        {
            Center = center,
            AxialAxis = axialAxis,
            ForwardAxis = forwardAxis,
            AxialRadius = axialRadius,
            LateralRadius = automatic.LateralRadius * adjustment.RadialScale,
            ForwardRadius = automatic.ForwardRadius * adjustment.RadialScale,
            ProximalTransitionLength = MathF.Min(
                automatic.ProximalTransitionLength * adjustment.AxialScale,
                axialRadius * 1.5f)
        };
    }

    private static SemanticRegionCalibration?
        TryRefineHeadCalibrationFromDonorTopology(
            SemanticRegionCalibration calibration,
            IReadOnlyList<GeometryComponent> bodyComponents,
            IReadOnlyList<Vector3[]> alignedPositionsByMesh,
            IReadOnlyDictionary<GeometryVertex, IReadOnlyList<GeometryVertex>> adjacency,
            float targetHeight,
            ICollection<string> messages)
    {
        if (calibration.Region != GeneratedSkinningSemanticRegion.Head)
            return calibration;

        GeneratedSkinningRegionVolume targetVolume = calibration.AutomaticVolume;
        float targetProximal = -targetVolume.AxialRadius;
        float targetTransitionEnd = targetProximal +
                                    targetVolume.ProximalTransitionLength;
        float targetCutProjection =
            Vector3.Dot(targetVolume.Center, targetVolume.AxialAxis) +
            targetProximal;
        Vector3 neckPlaneCenter = calibration.PosedAnchor +
            targetVolume.AxialAxis *
            (targetCutProjection -
             Vector3.Dot(calibration.PosedAnchor, targetVolume.AxialAxis));
        float maximumSpan = MathF.Min(
            targetHeight * MaximumDonorHeadSpanHeightRatio,
            targetVolume.AxialRadius * 2 *
            MaximumDonorToTargetHeadExtentRatio);
        if (!float.IsFinite(maximumSpan) || maximumSpan <= PositionEpsilon)
        {
            messages.Add(
                "Semantic Head donor-topology calibration has no finite anatomical span.");
            return null;
        }

        GeometryVertex[] bodyVertices = bodyComponents
            .SelectMany(component => component.Vertices)
            .Distinct()
            .ToArray();
        IReadOnlyDictionary<GeometryVertex, int> componentByVertex = bodyComponents
            .SelectMany(component => component.Vertices.Select(vertex =>
                (Vertex: vertex, component.ComponentIndex)))
            .ToDictionary(pair => pair.Vertex, pair => pair.ComponentIndex);
        float safetyDistal = targetProximal + maximumSpan;
        var headHalfSpace = new HashSet<GeometryVertex>(bodyVertices.Where(vertex =>
        {
            Vector3 position = alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
            float axial = Vector3.Dot(
                position - targetVolume.Center,
                targetVolume.AxialAxis);
            return float.IsFinite(axial) &&
                   axial >= targetProximal - PositionEpsilon &&
                   axial <= safetyDistal + PositionEpsilon;
        }));
        GeometryVertex[] seedCandidates = headHalfSpace
            .Where(vertex =>
            {
                Vector3 position =
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
                float axial = Vector3.Dot(
                    position - targetVolume.Center,
                    targetVolume.AxialAxis);
                return axial > targetTransitionEnd &&
                       DistanceToSemanticVolume(position, targetVolume) <= 1;
            })
            .OrderBy(vertex => Vector3.DistanceSquared(
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                calibration.PosedAnchor))
            .ThenBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        if (seedCandidates.Length == 0)
        {
            messages.Add(
                "Semantic Head donor-topology calibration could not prove a head " +
                "seed inside the target-derived volume and distal to its neck plane.");
            return null;
        }

        int[] seedOwnerComponents = seedCandidates
            .Select(vertex => componentByVertex[vertex])
            .Distinct()
            .Order()
            .ToArray();
        var seedSet = seedCandidates.ToHashSet();
        var seededClusters = new List<SemanticHeadCluster>();
        foreach (int seedOwnerComponent in seedOwnerComponents)
        {
            HashSet<GeometryVertex> candidateOwnerVertices = bodyComponents
                .Single(component =>
                    component.ComponentIndex == seedOwnerComponent)
                .Vertices
                .ToHashSet();
            GeometryVertex[] deterministicClusterStarts = candidateOwnerVertices
                .Where(headHalfSpace.Contains)
                .OrderBy(vertex => vertex.MeshIndex)
                .ThenBy(vertex => vertex.VertexIndex)
                .ToArray();
            var remaining = deterministicClusterStarts.ToHashSet();
            foreach (GeometryVertex start in deterministicClusterStarts)
            {
                // A single deterministic sort per owner keeps fragmented/clipped
                // modular surfaces O(V log V + E).
                if (!remaining.Remove(start))
                    continue;
                var cluster = new HashSet<GeometryVertex> { start };
                var pending = new Queue<GeometryVertex>();
                pending.Enqueue(start);
                while (pending.Count > 0)
                {
                    GeometryVertex current = pending.Dequeue();
                    if (!adjacency.TryGetValue(
                            current,
                            out IReadOnlyList<GeometryVertex>? neighbours))
                        continue;
                    foreach (GeometryVertex neighbour in neighbours)
                    {
                        if (remaining.Remove(neighbour))
                        {
                            cluster.Add(neighbour);
                            pending.Enqueue(neighbour);
                        }
                    }
                }

                HashSet<GeometryVertex> clusterSeeds = cluster
                    .Where(seedSet.Contains)
                    .ToHashSet();
                if (clusterSeeds.Count == 0)
                    continue;

                var clusterBoundary = new HashSet<GeometryVertex>();
                float clusterMaximumBoundaryRadius = 0;
                bool invalidBoundary = false;
                foreach (GeometryVertex vertex in cluster)
                {
                    if (!adjacency.TryGetValue(
                            vertex,
                            out IReadOnlyList<GeometryVertex>? neighbours))
                        continue;
                    foreach (GeometryVertex neighbour in neighbours)
                    {
                        if (!candidateOwnerVertices.Contains(neighbour) ||
                            cluster.Contains(neighbour))
                            continue;
                        Vector3 neighbourPosition = alignedPositionsByMesh
                            [neighbour.MeshIndex][neighbour.VertexIndex];
                        float neighbourAxial = Vector3.Dot(
                            neighbourPosition - targetVolume.Center,
                            targetVolume.AxialAxis);
                        if (neighbourAxial > safetyDistal + PositionEpsilon ||
                            neighbourAxial >= targetProximal - PositionEpsilon)
                        {
                            invalidBoundary = true;
                            break;
                        }

                        clusterBoundary.Add(vertex);
                        Vector3 position = alignedPositionsByMesh
                            [vertex.MeshIndex][vertex.VertexIndex];
                        float boundaryLateral = MathF.Max(
                            MathF.Abs(Vector3.Dot(
                                position - neckPlaneCenter,
                                targetVolume.LateralAxis) /
                                targetVolume.LateralRadius),
                            MathF.Abs(Vector3.Dot(
                                neighbourPosition - neckPlaneCenter,
                                targetVolume.LateralAxis) /
                                targetVolume.LateralRadius));
                        float boundaryForward = MathF.Max(
                            MathF.Abs(Vector3.Dot(
                                position - neckPlaneCenter,
                                targetVolume.ForwardAxis) /
                                targetVolume.ForwardRadius),
                            MathF.Abs(Vector3.Dot(
                                neighbourPosition - neckPlaneCenter,
                                targetVolume.ForwardAxis) /
                                targetVolume.ForwardRadius));
                        float boundaryRadius = MathF.Sqrt(
                            boundaryLateral * boundaryLateral +
                            boundaryForward * boundaryForward);
                        if (!float.IsFinite(boundaryLateral) ||
                            !float.IsFinite(boundaryForward) ||
                            !float.IsFinite(boundaryRadius) ||
                            boundaryRadius >
                                MaximumDonorNeckToTargetHeadRadiusRatio)
                        {
                            invalidBoundary = true;
                            break;
                        }
                        clusterMaximumBoundaryRadius = MathF.Max(
                            clusterMaximumBoundaryRadius,
                            boundaryRadius);
                    }
                    if (invalidBoundary)
                        break;
                }
                if (invalidBoundary)
                {
                    messages.Add(
                        $"Semantic Head ignored unsafe seeded shell from body " +
                        $"component #{seedOwnerComponent}: its topology boundary " +
                        "does not form a bounded neck cut.");
                    continue;
                }

                HashSet<Vector3> provedBoundaryPositions = clusterBoundary
                    .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                        [vertex.VertexIndex])
                    .ToHashSet();
                clusterBoundary.UnionWith(cluster.Where(vertex =>
                    provedBoundaryPositions.Contains(
                        alignedPositionsByMesh[vertex.MeshIndex]
                            [vertex.VertexIndex])));

                int uniquePositionCount = cluster
                    .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                        [vertex.VertexIndex])
                    .Distinct()
                    .Count();
                int uniqueBoundaryPositionCount = clusterBoundary
                    .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                        [vertex.VertexIndex])
                    .Distinct()
                    .Count();
                seededClusters.Add(new SemanticHeadCluster(
                    seedOwnerComponent,
                    cluster,
                    clusterSeeds,
                    clusterBoundary,
                    uniquePositionCount,
                    uniqueBoundaryPositionCount,
                    clusterMaximumBoundaryRadius));
            }
        }
        if (seededClusters.Count == 0)
        {
            messages.Add(
                "Semantic Head donor-topology calibration found no complete " +
                "seeded cluster inside its selected owner surface.");
            return null;
        }

        SemanticHeadCluster[] primaryOrder = seededClusters
            .Where(cluster => cluster.UniqueBoundaryPositionCount > 0)
            .OrderByDescending(cluster => cluster.UniqueBoundaryPositionCount)
            .ThenByDescending(cluster => cluster.UniquePositionCount)
            .ToArray();
        if (primaryOrder.Length == 0)
        {
            messages.Add(
                "Semantic Head found detached distal shells but no selected body " +
                "component with a proved external neck boundary.");
            return null;
        }
        if (primaryOrder.Length > 1 &&
            primaryOrder[0].UniqueBoundaryPositionCount ==
            primaryOrder[1].UniqueBoundaryPositionCount &&
            primaryOrder[0].UniquePositionCount ==
            primaryOrder[1].UniquePositionCount)
        {
            messages.Add(
                "Semantic Head donor-topology calibration found two equally " +
                "supported seeded clusters; one unambiguous primary neck lobe " +
                "is required.");
            return null;
        }
        SemanticHeadCluster primary = primaryOrder[0];
        int ownerComponentIndex = primary.OwnerComponentIndex;
        HashSet<GeometryVertex> ownerVertices = bodyComponents
            .Single(component => component.ComponentIndex == ownerComponentIndex)
            .Vertices
            .ToHashSet();
        Vector3[] primaryPositions = primary.Vertices
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .ToArray();
        if (primaryPositions.Length < MinimumCapturedSemanticCorePositions)
        {
            messages.Add(
                "Semantic Head primary donor cluster contains too few unique " +
                "surface positions.");
            return null;
        }

        HashSet<GeometryVertex> allSeededClusterVertices = seededClusters
            .SelectMany(cluster => cluster.Vertices)
            .ToHashSet();
        Vector3[] nonHeadBodyPositions = bodyVertices
            .Where(vertex => !allSeededClusterVertices.Contains(vertex))
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .ToArray();
        var primarySurfaceIndex = new ExactVector3NearestIndex(primaryPositions);
        ExactVector3NearestIndex? nonHeadBodyIndex =
            nonHeadBodyPositions.Length == 0
                ? null
                : new ExactVector3NearestIndex(nonHeadBodyPositions);
        float maximumSecondarySurfaceGap = MathF.Max(
            MathF.Min(targetVolume.LateralRadius, targetVolume.ForwardRadius) *
            HeadCompanionMaximumSurfaceGapRatio,
            PositionEpsilon * 16);
        var secondaryVertices = new HashSet<GeometryVertex>();
        var secondaryBoundaryVertices = new HashSet<GeometryVertex>();
        int acceptedSecondaryCount = 0;
        foreach (SemanticHeadCluster secondary in seededClusters
                     .Where(cluster => !ReferenceEquals(cluster, primary))
                     .OrderByDescending(cluster => cluster.UniquePositionCount)
                     .ThenBy(cluster => cluster.OwnerComponentIndex))
        {
            Vector3[] secondaryPositions = secondary.Vertices
                .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                    [vertex.VertexIndex])
                .Distinct()
                .ToArray();
            float[] secondaryAxial = Project(
                secondaryPositions, targetVolume.Center, targetVolume.AxialAxis);
            float[] secondaryLateral = Project(
                secondaryPositions, targetVolume.Center, targetVolume.LateralAxis);
            float[] secondaryForward = Project(
                secondaryPositions, targetVolume.Center, targetVolume.ForwardAxis);
            bool compactAndFullyEnclosed =
                secondaryPositions.Length >= MinimumCapturedSemanticCorePositions &&
                secondaryAxial[0] > targetTransitionEnd + PositionEpsilon &&
                secondaryAxial[^1] - secondaryAxial[0] <=
                    targetVolume.AxialRadius * 1.5f &&
                secondaryLateral[^1] - secondaryLateral[0] <=
                    targetVolume.LateralRadius * 2 &&
                secondaryForward[^1] - secondaryForward[0] <=
                    targetVolume.ForwardRadius * 2 &&
                secondaryPositions.All(position =>
                    DistanceToSemanticVolume(position, targetVolume) <=
                    HeadCompanionEnvelopeRatio + PositionEpsilon * 16);
            if (!compactAndFullyEnclosed)
            {
                messages.Add(
                    $"Semantic Head ignored body component " +
                    $"#{secondary.OwnerComponentIndex}: its seeded shell is not a " +
                    "compact, fully enclosed distal face layer.");
                continue;
            }

            var primarySurfaceDistances = new float[secondaryPositions.Length];
            int primaryPreferred = 0;
            bool hasFiniteDistances = true;
            for (int index = 0; index < secondaryPositions.Length; index++)
            {
                float primarySquared =
                    primarySurfaceIndex.FindNearestDistanceSquared(
                        secondaryPositions[index]);
                if (!float.IsFinite(primarySquared))
                {
                    messages.Add(
                        "Semantic Head secondary seeded cluster has no finite " +
                        "distance to its primary head surface.");
                    hasFiniteDistances = false;
                    break;
                }
                primarySurfaceDistances[index] = MathF.Sqrt(primarySquared);
                if (nonHeadBodyIndex is null)
                {
                    primaryPreferred++;
                    continue;
                }
                float otherSquared =
                    nonHeadBodyIndex.FindNearestDistanceSquared(
                        secondaryPositions[index]);
                if (!float.IsFinite(otherSquared))
                {
                    messages.Add(
                        "Semantic Head secondary seeded cluster has a non-finite " +
                        "distance to the remaining body surface.");
                    hasFiniteDistances = false;
                    break;
                }
                if (primarySquared <= otherSquared + PositionEpsilon * 16)
                    primaryPreferred++;
            }
            if (!hasFiniteDistances)
                continue;
            Array.Sort(primarySurfaceDistances);
            if (primaryPreferred != secondaryPositions.Length ||
                Quantile(primarySurfaceDistances, RobustUpperQuantile) >
                maximumSecondarySurfaceGap + PositionEpsilon)
            {
                messages.Add(
                    $"Semantic Head ignored body component " +
                    $"#{secondary.OwnerComponentIndex}: its distal shell is not " +
                    "uniformly nearest to the primary head surface within the " +
                    "conservative face-shell gap.");
                continue;
            }
            secondaryVertices.UnionWith(secondary.Vertices);
            secondaryBoundaryVertices.UnionWith(secondary.BoundaryVertices);
            acceptedSecondaryCount++;
        }

        var captured = primary.Vertices.ToHashSet();
        captured.UnionWith(secondaryVertices);
        HashSet<GeometryVertex> primaryBoundaryVertices =
            primary.BoundaryVertices.ToHashSet();
        HashSet<GeometryVertex> boundaryVertices =
            primaryBoundaryVertices.ToHashSet();
        boundaryVertices.UnionWith(secondaryBoundaryVertices);
        float maximumBoundaryRadius = primary.MaximumBoundaryRadius;
        GeometryVertex seed = primary.SeedVertices
            .OrderBy(vertex => Vector3.DistanceSquared(
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                calibration.PosedAnchor))
            .ThenBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .First();

        // The immutable anatomical cut is defined by the primary neck lobe.
        // Accepted secondary face shells contribute their topology boundaries to
        // collar coverage, but cannot move the cut into the face.
        float[] boundaryProjections = primaryBoundaryVertices
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .Select(position => Vector3.Dot(
                position, targetVolume.AxialAxis))
            .Order()
            .ToArray();
        float donorCutProjection = Quantile(boundaryProjections, 0.5f);
        float targetTransitionSpan = targetVolume.ProximalTransitionLength;
        if (!float.IsFinite(donorCutProjection) ||
            !float.IsFinite(targetTransitionSpan) ||
            targetTransitionSpan <= PositionEpsilon)
        {
            messages.Add(
                "Semantic Head donor neck boundary produced a non-finite " +
                "external Neck collar length.");
            return null;
        }

        float transitionEnvelopeRadius = MathF.Max(
            maximumBoundaryRadius * 1.15f,
            PositionEpsilon * 16);
        bool IsExternalCollarPosition(GeometryVertex vertex)
        {
            if (captured.Contains(vertex))
                return false;
            Vector3 position =
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
            float axialProjection = Vector3.Dot(
                position,
                targetVolume.AxialAxis);
            float lateral = Vector3.Dot(
                position - neckPlaneCenter,
                targetVolume.LateralAxis) / targetVolume.LateralRadius;
            float forward = Vector3.Dot(
                position - neckPlaneCenter,
                targetVolume.ForwardAxis) / targetVolume.ForwardRadius;
            float radius = MathF.Sqrt(lateral * lateral + forward * forward);
            return float.IsFinite(axialProjection) && float.IsFinite(radius) &&
                   radius <= transitionEnvelopeRadius + PositionEpsilon &&
                   axialProjection < donorCutProjection - PositionEpsilon;
        }

        var collarSeeds = new HashSet<GeometryVertex>();
        var boundaryPositionsWithExternalSeed = new HashSet<Vector3>();
        foreach (GeometryVertex boundary in boundaryVertices)
        {
            if (!adjacency.TryGetValue(
                    boundary, out IReadOnlyList<GeometryVertex>? neighbours))
                continue;
            bool connected = false;
            foreach (GeometryVertex neighbour in neighbours)
            {
                if (!ownerVertices.Contains(neighbour) ||
                    !IsExternalCollarPosition(neighbour))
                {
                    continue;
                }
                collarSeeds.Add(neighbour);
                connected = true;
            }
            if (connected)
            {
                boundaryPositionsWithExternalSeed.Add(
                    alignedPositionsByMesh[boundary.MeshIndex]
                        [boundary.VertexIndex]);
            }
        }
        Vector3[] uncoveredBoundaryPositions = boundaryVertices
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .Where(position => !boundaryPositionsWithExternalSeed.Contains(position))
            .ToArray();
        if (collarSeeds.Count == 0 || uncoveredBoundaryPositions.Length > 0)
        {
            messages.Add(
                $"Semantic Head external Neck collar could not cover every " +
                $"proved primary/secondary rigid-boundary position " +
                $"({uncoveredBoundaryPositions.Length} uncovered); refusing an " +
                "asymmetric or partial collar.");
            return null;
        }

        float localCollarStep = float.PositiveInfinity;
        foreach (GeometryVertex seedVertex in collarSeeds)
        {
            if (!adjacency.TryGetValue(
                    seedVertex, out IReadOnlyList<GeometryVertex>? neighbours))
                continue;
            Vector3 seedPosition = alignedPositionsByMesh[seedVertex.MeshIndex]
                [seedVertex.VertexIndex];
            float seedProjection = Vector3.Dot(
                seedPosition, targetVolume.AxialAxis);
            foreach (GeometryVertex neighbour in neighbours)
            {
                if (!ownerVertices.Contains(neighbour) ||
                    !IsExternalCollarPosition(neighbour))
                    continue;
                Vector3 neighbourPosition = alignedPositionsByMesh
                    [neighbour.MeshIndex][neighbour.VertexIndex];
                float neighbourProjection = Vector3.Dot(
                    neighbourPosition, targetVolume.AxialAxis);
                float edgeLength = Vector3.Distance(
                    seedPosition, neighbourPosition);
                if (float.IsFinite(neighbourProjection) &&
                    neighbourProjection < seedProjection - PositionEpsilon &&
                    float.IsFinite(edgeLength) && edgeLength > PositionEpsilon)
                {
                    localCollarStep = MathF.Min(localCollarStep, edgeLength);
                }
            }
        }
        float maximumCollarSurfaceDistance = targetTransitionSpan;
        if (float.IsFinite(localCollarStep))
        {
            maximumCollarSurfaceDistance = MathF.Max(
                maximumCollarSurfaceDistance,
                localCollarStep * 1.25f);
        }
        maximumCollarSurfaceDistance = MathF.Min(
            maximumCollarSurfaceDistance,
            targetVolume.AxialRadius);
        if (!float.IsFinite(maximumCollarSurfaceDistance) ||
            maximumCollarSurfaceDistance <= PositionEpsilon)
        {
            messages.Add(
                "Semantic Head external Neck collar has no finite bounded " +
                "surface distance.");
            return null;
        }

        float maximumSeedProjection = collarSeeds.Max(vertex => Vector3.Dot(
            alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
            targetVolume.AxialAxis));
        float minimumSeedProjection = collarSeeds.Min(vertex => Vector3.Dot(
            alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
            targetVolume.AxialAxis));
        var transitionEligible = new HashSet<GeometryVertex>(ownerVertices.Where(vertex =>
        {
            if (!IsExternalCollarPosition(vertex))
                return false;
            float axialProjection = Vector3.Dot(
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                targetVolume.AxialAxis);
            return axialProjection <= maximumSeedProjection + PositionEpsilon &&
                   axialProjection >= minimumSeedProjection -
                    maximumCollarSurfaceDistance - PositionEpsilon;
        }));
        var transitionDistance = collarSeeds.ToDictionary(
            vertex => vertex,
            _ => 0f);
        var transitionPending = new PriorityQueue<GeometryVertex, float>();
        foreach (GeometryVertex seedVertex in collarSeeds)
            transitionPending.Enqueue(seedVertex, 0);
        while (transitionPending.TryDequeue(
                   out GeometryVertex current,
                   out float queuedDistance))
        {
            if (!transitionDistance.TryGetValue(
                    current, out float currentDistance) ||
                queuedDistance > currentDistance + PositionEpsilon)
            {
                continue;
            }
            if (!adjacency.TryGetValue(
                    current, out IReadOnlyList<GeometryVertex>? neighbours))
                continue;
            foreach (GeometryVertex neighbour in neighbours)
            {
                if (transitionEligible.Contains(neighbour))
                {
                    Vector3 currentPosition = alignedPositionsByMesh
                        [current.MeshIndex][current.VertexIndex];
                    Vector3 neighbourPosition = alignedPositionsByMesh
                        [neighbour.MeshIndex][neighbour.VertexIndex];
                    float candidateDistance = currentDistance +
                        Vector3.Distance(currentPosition, neighbourPosition);
                    if (!float.IsFinite(candidateDistance) ||
                        candidateDistance >
                        maximumCollarSurfaceDistance + PositionEpsilon ||
                        transitionDistance.TryGetValue(
                            neighbour, out float knownDistance) &&
                        knownDistance <= candidateDistance + PositionEpsilon)
                    {
                        continue;
                    }
                    transitionDistance[neighbour] = candidateDistance;
                    transitionPending.Enqueue(neighbour, candidateDistance);
                }
            }
        }
        HashSet<GeometryVertex> donorNeckCollar = transitionDistance.Keys.ToHashSet();

        GeometryVertex[] unguardedRigidBoundaryNeighbours = captured
            .SelectMany(vertex => adjacency.TryGetValue(
                    vertex, out IReadOnlyList<GeometryVertex>? neighbours)
                ? neighbours
                : [])
            .Where(ownerVertices.Contains)
            .Where(vertex => !captured.Contains(vertex))
            .Where(vertex => !donorNeckCollar.Contains(vertex))
            .Distinct()
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        if (unguardedRigidBoundaryNeighbours.Length > 0)
        {
            messages.Add(
                $"Semantic Head external Neck collar leaves " +
                $"{unguardedRigidBoundaryNeighbours.Length} same-owner topology " +
                "neighbour(s) directly adjacent to the complete rigid " +
                "primary/secondary assembly; refusing an unguarded Head-to-capsule " +
                "edge.");
            return null;
        }

        // Exact attribute-seam copies are one rendered position. Closure over
        // positions is required before assigning the collar so that no OBJ/glTF
        // duplicate can retain legacy weights beside an otherwise identical
        // Head+Neck blend. The topology traversal above already bridges these
        // copies with zero-length edges; this explicit postcondition keeps that
        // invariant fail-closed if topology construction ever changes.
        HashSet<Vector3> donorNeckCollarPositions = donorNeckCollar
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .ToHashSet();
        GeometryVertex[] missingCollarSeamCopies = ownerVertices
            .Where(vertex => !captured.Contains(vertex))
            .Where(vertex => donorNeckCollarPositions.Contains(
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex]))
            .Where(vertex => !donorNeckCollar.Contains(vertex))
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        if (missingCollarSeamCopies.Length > 0)
        {
            messages.Add(
                $"Semantic Head external Neck collar left " +
                $"{missingCollarSeamCopies.Length} exact owner-surface seam " +
                "copy/copies outside its topology/geodesic proof.");
            return null;
        }

        var collarDistanceByPosition = transitionDistance
            .GroupBy(pair => alignedPositionsByMesh[pair.Key.MeshIndex]
                [pair.Key.VertexIndex])
            .ToDictionary(
                group => group.Key,
                group => group.Min(pair => pair.Value));
        var donorNeckCollarWeights = new ReadOnlyDictionary<GeometryVertex, float>(
            donorNeckCollar.ToDictionary(
                vertex => vertex,
                vertex =>
                {
                    Vector3 position = alignedPositionsByMesh[vertex.MeshIndex]
                        [vertex.VertexIndex];
                    float distance = collarDistanceByPosition[position];
                    float amount = Math.Clamp(
                        1 - distance / maximumCollarSurfaceDistance,
                        0,
                        1);
                    return amount * amount * (3 - 2 * amount);
                }));
        if (!donorNeckCollarWeights.Values.Any(weight =>
                weight > WeightEpsilon && weight < 1 - WeightEpsilon))
        {
            messages.Add(
                "Semantic Head external Neck collar contains no genuine bounded " +
                "Head+Neck blend below the rigid lobe.");
            return null;
        }

        Vector3[] positions = captured
            .Select(vertex =>
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex])
            .Distinct()
            .ToArray();
        float[] axialValues = Project(
            positions, targetVolume.Center, targetVolume.AxialAxis);
        int transitionPositionCount = donorNeckCollar
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .Count();
        int corePositionCount = captured
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .Count();
        if (corePositionCount < MinimumCapturedSemanticCorePositions ||
            transitionPositionCount == 0)
        {
            messages.Add(
                $"Semantic Head donor-topology cluster contains " +
                $"{corePositionCount} unique core and {transitionPositionCount} " +
                "external Neck-collar position(s); both zones are required.");
            return null;
        }

        float observedSpan = axialValues[^1] - axialValues[0];
        float axialMargin = MathF.Max(
            targetHeight * MinimumSemanticRadiusHeightRatio,
            observedSpan * 0.04f);
        float resolvedProximal = MathF.Min(
            targetProximal,
            axialValues[0] - axialMargin);
        float resolvedDistal = MathF.Max(
            targetVolume.AxialRadius,
            axialValues[^1] + axialMargin);
        float resolvedSpan = resolvedDistal - resolvedProximal;
        if (!float.IsFinite(resolvedSpan) || resolvedSpan <= PositionEpsilon ||
            resolvedSpan > maximumSpan)
        {
            messages.Add(
                $"Semantic Head donor topology requires axial span " +
                $"{resolvedSpan:G6}, beyond the conservative target/height bound " +
                $"{maximumSpan:G6}.");
            return null;
        }

        float[] lateral = Project(
            positions, targetVolume.Center, targetVolume.LateralAxis);
        float[] forward = Project(
            positions, targetVolume.Center, targetVolume.ForwardAxis);
        float lateralOffset = (lateral[0] + lateral[^1]) * 0.5f;
        float forwardOffset = (forward[0] + forward[^1]) * 0.5f;
        float minimumRadius = MathF.Max(
            targetHeight * MinimumSemanticRadiusHeightRatio,
            PositionEpsilon * 16);
        float lateralRadius = MathF.Max(
            (lateral[^1] - lateral[0]) * 0.5f,
            MathF.Max(targetVolume.LateralRadius * 0.5f, minimumRadius));
        float forwardRadius = MathF.Max(
            (forward[^1] - forward[0]) * 0.5f,
            MathF.Max(targetVolume.ForwardRadius * 0.5f, minimumRadius));
        float axialCenter = (resolvedProximal + resolvedDistal) * 0.5f;
        float axialRadius = resolvedSpan * 0.5f;
        Vector3 center = targetVolume.Center +
                         targetVolume.AxialAxis * axialCenter +
                         targetVolume.LateralAxis * lateralOffset +
                         targetVolume.ForwardAxis * forwardOffset;

        float requiredRadialScale = 1;
        foreach (Vector3 position in positions)
        {
            Vector3 relative = position - center;
            float axialNormalized = MathF.Abs(Vector3.Dot(
                relative, targetVolume.AxialAxis) / axialRadius);
            if (!float.IsFinite(axialNormalized) ||
                axialNormalized >= 1 - PositionEpsilon)
            {
                messages.Add(
                    "Semantic Head donor cluster reaches its finite axial safety boundary.");
                return null;
            }
            float lateralNormalized = MathF.Abs(Vector3.Dot(
                relative, targetVolume.LateralAxis) / lateralRadius);
            float forwardNormalized = MathF.Abs(Vector3.Dot(
                relative, targetVolume.ForwardAxis) / forwardRadius);
            float radialSquared = lateralNormalized * lateralNormalized +
                                  forwardNormalized * forwardNormalized;
            float radialRemainder = 1 - axialNormalized * axialNormalized;
            float scale = MathF.Sqrt(
                radialSquared / MathF.Max(radialRemainder, PositionEpsilon));
            if (!float.IsFinite(scale))
            {
                messages.Add(
                    "Semantic Head donor envelope requires a non-finite radial scale.");
                return null;
            }
            requiredRadialScale = MathF.Max(requiredRadialScale, scale);
        }
        lateralRadius *= requiredRadialScale * DonorHeadEnvelopeMargin;
        forwardRadius *= requiredRadialScale * DonorHeadEnvelopeMargin;
        if (lateralRadius <
                targetVolume.LateralRadius *
                MinimumDonorToTargetHeadExtentRatio ||
            forwardRadius <
                targetVolume.ForwardRadius *
                MinimumDonorToTargetHeadExtentRatio)
        {
            messages.Add(
                $"Semantic Head donor lobe is too narrow to represent the target " +
                $"head volume: radii ({lateralRadius:G6}, {forwardRadius:G6}); " +
                $"minimum target-relative ratio is " +
                $"{MinimumDonorToTargetHeadExtentRatio:G6}.");
            return null;
        }
        float maximumAnatomicalRadius = MathF.Min(
            targetHeight * MaximumDonorHeadRadiusHeightRatio,
            MathF.Max(targetVolume.LateralRadius, targetVolume.ForwardRadius) *
            MaximumDonorToTargetHeadExtentRatio);
        if (!float.IsFinite(lateralRadius) || !float.IsFinite(forwardRadius) ||
            lateralRadius > maximumAnatomicalRadius ||
            forwardRadius > maximumAnatomicalRadius)
        {
            messages.Add(
                $"Semantic Head donor lobe requires radii " +
                $"({lateralRadius:G6}, {forwardRadius:G6}), beyond the conservative " +
                $"target/height bound {maximumAnatomicalRadius:G6}.");
            return null;
        }

        float transitionLength = targetTransitionEnd - resolvedProximal;
        if (!float.IsFinite(transitionLength) ||
            transitionLength <= PositionEpsilon ||
            transitionLength >= resolvedSpan)
        {
            messages.Add(
                "Semantic Head donor refinement produced an invalid neck transition.");
            return null;
        }
        GeneratedSkinningRegionVolume donorVolume = targetVolume with
        {
            Center = center,
            AxialRadius = axialRadius,
            LateralRadius = lateralRadius,
            ForwardRadius = forwardRadius,
            ProximalTransitionLength = transitionLength,
            ShapeExponent = 2
        };

        GeometryVertex[] uncapturedAutomaticSeeds = bodyVertices
            .Where(vertex => !captured.Contains(vertex))
            .Where(vertex =>
            {
                Vector3 position = alignedPositionsByMesh
                    [vertex.MeshIndex][vertex.VertexIndex];
                float targetAxial = Vector3.Dot(
                    position - targetVolume.Center,
                    targetVolume.AxialAxis);
                return float.IsFinite(targetAxial) &&
                       targetAxial > targetTransitionEnd + PositionEpsilon &&
                       DistanceToSemanticVolume(position, donorVolume) <=
                       1 + PositionEpsilon * 16;
            })
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        if (uncapturedAutomaticSeeds.Length > 0)
        {
            messages.Add(
                $"Semantic Head refined automatic volume still contains " +
                $"{uncapturedAutomaticSeeds.Length} uncaptured distal " +
                "selected-body vertex/vertices from unproved modular shells; " +
                "they retain ordinary bounded capsule weights instead of " +
                "invalidating the topology-proved primary head lobe.");
        }

        HashSet<GeometryVertex> allSeedVertices = seededClusters
            .SelectMany(cluster => cluster.SeedVertices)
            .Where(captured.Contains)
            .ToHashSet();
        messages.Add(
            $"Semantic Head donor-topology calibration followed one primary " +
            $"neck lobe plus {acceptedSecondaryCount} compact secondary " +
            $"face-shell cluster(s) across {positions.Length} unique position(s), " +
            "preserving the complete lobe as rigid Head and placing the " +
            "target-derived Head+Neck blend in an external Neck collar; " +
            "resolved radii are " +
            $"({axialRadius:G6}, {lateralRadius:G6}, {forwardRadius:G6}) without " +
            $"donor mesh/material/name conditions. Relative to Head along the " +
            $"target axis, the donor-boundary cut/collar surface bound are " +
            $"{(donorCutProjection - Vector3.Dot(
                calibration.PosedAnchor, targetVolume.AxialAxis)) / targetHeight:G6}/" +
            $"{maximumCollarSurfaceDistance / targetHeight:G6} target-height " +
            "units (cut projection / maximum collar surface distance).");
        return calibration with
        {
            AutomaticVolume = donorVolume,
            DonorHeadLobe = new SemanticHeadLobe(
                ownerComponentIndex,
                seed,
                captured.ToHashSet(),
                primary.Vertices.ToHashSet(),
                secondaryVertices.ToHashSet(),
                allSeedVertices,
                ownerVertices,
                boundaryVertices,
                donorNeckCollar,
                donorNeckCollarWeights,
                donorCutProjection,
                maximumCollarSurfaceDistance)
        };
    }

    private static SemanticRegionCalibration?
        TryRefineHandCalibrationFromDonorTopology(
            SemanticRegionCalibration calibration,
            IReadOnlyList<GeometryComponent> bodyComponents,
            IReadOnlyList<Vector3[]> alignedPositionsByMesh,
            IReadOnlyDictionary<GeometryVertex, IReadOnlyList<GeometryVertex>> adjacency,
            SideCalibration sideCalibration,
            float targetHeight,
            ICollection<string> messages)
    {
        if (calibration.Region == GeneratedSkinningSemanticRegion.Head)
            return calibration;
        GeneratedSkinningRegionVolume targetVolume = calibration.AutomaticVolume;
        BodySide requiredSide = calibration.Region ==
                                GeneratedSkinningSemanticRegion.LeftHand
            ? BodySide.Left
            : BodySide.Right;
        float coreStart = -targetVolume.AxialRadius +
                          targetVolume.ProximalTransitionLength;
        GeometryVertex[] bodyVertices = bodyComponents
            .SelectMany(component => component.Vertices)
            .Distinct()
            .ToArray();
        var distalSlab = new HashSet<GeometryVertex>(bodyVertices.Where(vertex =>
        {
            Vector3 position = alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
            if (IsOpposite(
                    requiredSide,
                    ClassifyPositionSide(position, sideCalibration)))
                return false;
            float axial = Vector3.Dot(
                position - targetVolume.Center,
                targetVolume.AxialAxis);
            return axial > coreStart && axial < targetVolume.AxialRadius;
        }));
        if (distalSlab.Count == 0)
        {
            messages.Add(
                $"Semantic {calibration.Region} donor-topology calibration found " +
                "no selected-body vertices beyond its target-derived wrist plane.");
            return null;
        }

        GeometryVertex[] seedCandidates = distalSlab
            .Where(vertex => DistanceToSemanticVolume(
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                targetVolume) <= 1)
            .OrderBy(vertex => Vector3.DistanceSquared(
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                calibration.PosedAnchor))
            .ThenBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        if (seedCandidates.Length == 0)
        {
            messages.Add(
                $"Semantic {calibration.Region} donor-topology calibration could " +
                "not prove a wrist seed inside the target-derived hand volume.");
            return null;
        }
        GeometryVertex seed = seedCandidates[0];
        var captured = new HashSet<GeometryVertex> { seed };
        var pending = new Queue<GeometryVertex>();
        pending.Enqueue(seed);
        while (pending.Count > 0)
        {
            GeometryVertex current = pending.Dequeue();
            if (!adjacency.TryGetValue(
                    current, out IReadOnlyList<GeometryVertex>? neighbours))
                continue;
            foreach (GeometryVertex neighbour in neighbours)
            {
                if (distalSlab.Contains(neighbour) && captured.Add(neighbour))
                    pending.Enqueue(neighbour);
            }
        }
        Vector3[] positions = captured
            .Select(vertex =>
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex])
            .Distinct()
            .ToArray();
        if (positions.Length < MinimumCapturedSemanticCorePositions)
        {
            messages.Add(
                $"Semantic {calibration.Region} donor-topology wrist cluster has " +
                $"only {positions.Length} unique position(s).");
            return null;
        }

        float[] lateral = Project(
            positions, targetVolume.Center, targetVolume.LateralAxis);
        float[] forward = Project(
            positions, targetVolume.Center, targetVolume.ForwardAxis);
        float lateralOffset = (lateral[0] + lateral[^1]) * 0.5f;
        float forwardOffset = (forward[0] + forward[^1]) * 0.5f;
        Vector3 center = targetVolume.Center +
                         targetVolume.LateralAxis * lateralOffset +
                         targetVolume.ForwardAxis * forwardOffset;
        float lateralRadius = MathF.Max(
            (lateral[^1] - lateral[0]) * 0.5f,
            targetVolume.LateralRadius * 0.5f);
        float forwardRadius = MathF.Max(
            (forward[^1] - forward[0]) * 0.5f,
            targetVolume.ForwardRadius * 0.5f);
        float exponent = targetVolume.ShapeExponent;
        float requiredRadialScale = 1;
        foreach (Vector3 position in positions)
        {
            Vector3 relative = position - center;
            float axial = MathF.Abs(Vector3.Dot(
                relative, targetVolume.AxialAxis) / targetVolume.AxialRadius);
            if (!float.IsFinite(axial) || axial >= 1 - PositionEpsilon)
            {
                messages.Add(
                    $"Semantic {calibration.Region} donor wrist cluster reaches the " +
                    "target-derived distal axial safety boundary.");
                return null;
            }
            float lateralNormalized = MathF.Abs(Vector3.Dot(
                relative, targetVolume.LateralAxis) / lateralRadius);
            float forwardNormalized = MathF.Abs(Vector3.Dot(
                relative, targetVolume.ForwardAxis) / forwardRadius);
            float remaining = 1 - MathF.Pow(axial, exponent);
            float radialPowered = MathF.Pow(lateralNormalized, exponent) +
                                  MathF.Pow(forwardNormalized, exponent);
            float scale = MathF.Pow(
                radialPowered / MathF.Max(remaining, PositionEpsilon),
                1 / exponent);
            if (!float.IsFinite(scale))
            {
                messages.Add(
                    $"Semantic {calibration.Region} donor wrist envelope is non-finite.");
                return null;
            }
            requiredRadialScale = MathF.Max(requiredRadialScale, scale);
        }
        lateralRadius *= requiredRadialScale * DonorHandEnvelopeMargin;
        forwardRadius *= requiredRadialScale * DonorHandEnvelopeMargin;
        float maximumAnatomicalRadius = MathF.Min(
            targetHeight * MaximumDonorHandRadiusHeightRatio,
            MathF.Max(
                targetVolume.LateralRadius,
                targetVolume.ForwardRadius) *
            MaximumDonorToTargetHandRadiusRatio);
        if (!float.IsFinite(lateralRadius) || !float.IsFinite(forwardRadius) ||
            lateralRadius > maximumAnatomicalRadius ||
            forwardRadius > maximumAnatomicalRadius)
        {
            messages.Add(
                $"Semantic {calibration.Region} donor wrist cluster requires " +
                $"radii ({lateralRadius:G6}, {forwardRadius:G6}), beyond the " +
                $"conservative target/height bound {maximumAnatomicalRadius:G6}.");
            return null;
        }

        GeneratedSkinningRegionVolume donorVolume = targetVolume with
        {
            Center = center,
            LateralRadius = lateralRadius,
            ForwardRadius = forwardRadius
        };
        messages.Add(
            $"Semantic {calibration.Region} donor-topology wrist calibration " +
            $"captured {positions.Length} unique distal position(s) and resolved " +
            $"radii ({lateralRadius:G6}, {forwardRadius:G6}) without donor " +
            "mesh/material/name conditions.");
        return calibration with
        {
            AutomaticVolume = donorVolume
        };
    }

    private static void ValidateSemanticAdjustment(
        GeneratedSkinningRegionAdjustment adjustment,
        GeneratedSkinningRegionAdjustmentLimits limits)
    {
        if (adjustment.AxialOffset < limits.MinimumAxialOffset ||
            adjustment.AxialOffset > limits.MaximumAxialOffset ||
            adjustment.AxialScale < limits.MinimumAxialScale ||
            adjustment.AxialScale > limits.MaximumAxialScale ||
            adjustment.RadialScale < limits.MinimumRadialScale ||
            adjustment.RadialScale > limits.MaximumRadialScale)
        {
            throw new InvalidDataException(
                $"Semantic {adjustment.Region} adjustment is outside its calibrated " +
                "safe editor limits.");
        }
    }

    private static bool IsManualSemanticVolumeAdjustment(
        GeneratedSkinningRegionAdjustment adjustment) =>
        MathF.Abs(adjustment.AxialOffset) > PositionEpsilon ||
        MathF.Abs(adjustment.AxialScale - 1) > PositionEpsilon ||
        MathF.Abs(adjustment.RadialScale - 1) > PositionEpsilon ||
        MathF.Abs(adjustment.ForwardTiltDegrees) > PositionEpsilon;

    private static bool TryCaptureSemanticRegion(
        SemanticRegionCalibration calibration,
        GeneratedSkinningRegionVolume volume,
        IReadOnlyList<GeometryComponent> bodyComponents,
        IReadOnlyList<GeometrySource> donorSources,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        IReadOnlyDictionary<GeometryVertex, IReadOnlyList<GeometryVertex>> adjacency,
        SideCalibration sideCalibration,
        bool useManualVolumeMembership,
        out IReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment> assignments,
        out IReadOnlyList<TargetRigBodyVertexMembership> coreVertices,
        out IReadOnlyList<TargetRigBodyVertexMembership> transitionVertices,
        out string? failure)
    {
        if (calibration.Region == GeneratedSkinningSemanticRegion.Head)
        {
            if (calibration.DonorHeadLobe is null || useManualVolumeMembership)
            {
                return TryCaptureManualSemanticVolume(
                    calibration,
                    volume,
                    bodyComponents,
                    donorSources,
                    alignedPositionsByMesh,
                    sideCalibration,
                    out assignments,
                    out coreVertices,
                    out transitionVertices,
                    out failure);
            }
            return TryCaptureSemanticHeadLobe(
                calibration,
                volume,
                donorSources,
                alignedPositionsByMesh,
                adjacency,
                out assignments,
                out coreVertices,
                out transitionVertices,
                out failure);
        }

        return TryCaptureCompleteSemanticHandLobe(
            calibration,
            volume,
            bodyComponents,
            donorSources,
            alignedPositionsByMesh,
            sideCalibration,
            out assignments,
            out coreVertices,
            out transitionVertices,
            out failure);
    }

    private static bool TryCaptureManualSemanticVolume(
        SemanticRegionCalibration calibration,
        GeneratedSkinningRegionVolume volume,
        IReadOnlyList<GeometryComponent> bodyComponents,
        IReadOnlyList<GeometrySource> donorSources,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        SideCalibration sideCalibration,
        out IReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment> assignments,
        out IReadOnlyList<TargetRigBodyVertexMembership> coreVertices,
        out IReadOnlyList<TargetRigBodyVertexMembership> transitionVertices,
        out string? failure)
    {
        BodySide requiredSide = calibration.Region switch
        {
            GeneratedSkinningSemanticRegion.LeftHand => BodySide.Left,
            GeneratedSkinningSemanticRegion.RightHand => BodySide.Right,
            _ => BodySide.Center
        };
        GeometryVertex[] captured = bodyComponents
            .SelectMany(component => component.Vertices)
            .Distinct()
            .Where(vertex =>
            {
                Vector3 position = alignedPositionsByMesh[vertex.MeshIndex]
                    [vertex.VertexIndex];
                return (requiredSide == BodySide.Center ||
                        !IsOpposite(
                            requiredSide,
                            ClassifyPositionSide(position, sideCalibration))) &&
                       DistanceToSemanticVolume(position, volume) <=
                       1 + PositionEpsilon * 16;
            })
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        int uniquePositions = captured
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .Count();
        if (uniquePositions < MinimumCapturedSemanticCorePositions)
        {
            assignments = new ReadOnlyDictionary<GeometryVertex,
                SemanticVertexAssignment>(
                new Dictionary<GeometryVertex, SemanticVertexAssignment>());
            coreVertices = [];
            transitionVertices = [];
            failure =
                $"Semantic {calibration.Region} manual ellipsoid contains only " +
                $"{uniquePositions} unique selected-body position(s); move or " +
                "expand it before applying.";
            return false;
        }

        float transitionLength = Math.Clamp(
            volume.ProximalTransitionLength,
            PositionEpsilon,
            MathF.Max(PositionEpsilon, volume.AxialRadius * 0.5f));
        var mutable = new Dictionary<GeometryVertex, SemanticVertexAssignment>();
        var core = new List<GeometryVertex>();
        var transition = new List<GeometryVertex>();
        float[] handLateral = calibration.Region ==
                              GeneratedSkinningSemanticRegion.Head
            ? []
            : captured.Select(vertex => Vector3.Dot(
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex] -
                    volume.Center,
                    volume.LateralAxis))
                .Order()
                .ToArray();
        float handLateralMinimum = handLateral.Length == 0
            ? -1
            : Quantile(handLateral, RobustLowerQuantile);
        float handLateralMaximum = handLateral.Length == 0
            ? 1
            : Quantile(handLateral, RobustUpperQuantile);
        foreach (GeometryVertex vertex in captured)
        {
            Vector3 position = alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex];
            float axial = Vector3.Dot(position - volume.Center, volume.AxialAxis);
            float distanceFromProximalPole = MathF.Max(0, axial + volume.AxialRadius);
            if (calibration.Region != GeneratedSkinningSemanticRegion.Head &&
                distanceFromProximalPole < transitionLength)
            {
                float amount = Math.Clamp(
                    distanceFromProximalPole / transitionLength,
                    0,
                    1);
                amount = amount * amount * (3 - 2 * amount);
                transition.Add(vertex);
                mutable.Add(vertex, new SemanticVertexAssignment(
                    calibration.Region,
                    SemanticVertexZone.ProximalTransition,
                    calibration.AnchorSkeletonJointIndex,
                    calibration.ProximalSkeletonJointIndex,
                    amount));
            }
            else
            {
                core.Add(vertex);
                PackedInfluence[] motionInfluences = [];
                if (calibration.Region != GeneratedSkinningSemanticRegion.Head &&
                    calibration.HandMotionProfile is { } motionProfile)
                {
                    ApproximateFingerBranch branch = motionProfile.SelectBranch(
                        position,
                        volume,
                        handLateralMinimum,
                        handLateralMaximum);
                    float motionStart = -volume.AxialRadius + transitionLength;
                    float motionSpan = MathF.Max(
                        PositionEpsilon,
                        volume.AxialRadius - motionStart);
                    float progress = Math.Clamp(
                        (axial - motionStart) / motionSpan,
                        0,
                        1);
                    motionInfluences = BuildApproximateFingerInfluences(
                        calibration.AnchorSkeletonJointIndex,
                        branch.SkeletonJointIndices,
                        motionProfile.EvaluateArticulation(progress));
                }
                mutable.Add(vertex, new SemanticVertexAssignment(
                    calibration.Region,
                    SemanticVertexZone.Core,
                    calibration.AnchorSkeletonJointIndex,
                    calibration.ProximalSkeletonJointIndex,
                    1)
                {
                    CoreMotionInfluences = motionInfluences
                });
            }
        }
        if (core.Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex]).Distinct().Count() <
            MinimumCapturedSemanticCorePositions)
        {
            assignments = new ReadOnlyDictionary<GeometryVertex,
                SemanticVertexAssignment>(
                new Dictionary<GeometryVertex, SemanticVertexAssignment>());
            coreVertices = [];
            transitionVertices = [];
            failure =
                $"Semantic {calibration.Region} manual ellipsoid leaves too few " +
                "vertices outside its proximal transition; increase its length " +
                "or move it distally.";
            return false;
        }

        assignments = new ReadOnlyDictionary<GeometryVertex,
            SemanticVertexAssignment>(mutable);
        coreVertices = BuildSemanticMembership(core, donorSources);
        transitionVertices = BuildSemanticMembership(transition, donorSources);
        int articulated = mutable.Values.Count(value =>
            value.MotionProxyWeight > WeightEpsilon);
        failure =
            $"Semantic {calibration.Region} used the explicitly applied manual " +
            $"ellipsoid membership: {core.Count} rigid core and " +
            $"{transition.Count} proximal-transition vertex/vertices" +
            (articulated > 0
                ? $", including {articulated} approximate finger-motion vertex/vertices"
                : string.Empty) +
            ". The user-authored finite volume, rather than automatic topology " +
            "lobe containment, defined this protected region.";
        return true;
    }

    private static bool TryCaptureSemanticHeadLobe(
        SemanticRegionCalibration calibration,
        GeneratedSkinningRegionVolume volume,
        IReadOnlyList<GeometrySource> donorSources,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        IReadOnlyDictionary<GeometryVertex, IReadOnlyList<GeometryVertex>> adjacency,
        out IReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment> assignments,
        out IReadOnlyList<TargetRigBodyVertexMembership> coreVertices,
        out IReadOnlyList<TargetRigBodyVertexMembership> transitionVertices,
        out string? failure)
    {
        SemanticHeadLobe? lobe = calibration.DonorHeadLobe;
        if (lobe is null || lobe.Vertices.Count == 0)
        {
            assignments = new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
                new Dictionary<GeometryVertex, SemanticVertexAssignment>());
            coreVertices = [];
            transitionVertices = [];
            failure =
                "Semantic Head has no validated donor-topology lobe to capture.";
            return false;
        }

        GeometryVertex[] outside = lobe.Vertices
            .Where(vertex => DistanceToSemanticVolume(
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                volume) > 1 + PositionEpsilon * 16)
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        if (outside.Length > 0)
        {
            assignments = new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
                new Dictionary<GeometryVertex, SemanticVertexAssignment>());
            coreVertices = [];
            transitionVertices = [];
            failure =
                $"Semantic Head adjusted ellipsoid would cut through its validated " +
                $"connected donor head lobe at {outside.Length} vertex/vertices; " +
                "expand or reset the protected volume instead of partially deforming a face.";
            return false;
        }

        // The ellipsoid is a finite containment guard, not the semantic cut.
        // The immutable donor-topology membership and neck plane above remain the
        // source of truth even though the ellipsoid needs a pole below a neck ring
        // with non-zero radius. A manual expansion may not silently make additional
        // owner-surface vertices newly eligible inside that guard.
        GeometryVertex[] newlyEnclosedOwnerVertices = lobe.OwnerVertices
            .Where(vertex => !lobe.Vertices.Contains(vertex))
            .Where(vertex =>
            {
                Vector3 position =
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
                return DistanceToSemanticVolume(position, volume) <=
                           1 + PositionEpsilon * 16 &&
                       DistanceToSemanticVolume(
                           position, calibration.AutomaticVolume) >
                           1 + PositionEpsilon * 16;
            })
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        if (newlyEnclosedOwnerVertices.Length > 0)
        {
            assignments = new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
                new Dictionary<GeometryVertex, SemanticVertexAssignment>());
            coreVertices = [];
            transitionVertices = [];
            failure =
                $"Semantic Head adjusted ellipsoid newly encloses " +
                $"{newlyEnclosedOwnerVertices.Length} owner-surface vertex/vertices " +
                "outside the immutable donor neck cut; reset or contract the " +
                "protected volume instead of changing membership implicitly.";
            return false;
        }

        float transitionSpan = lobe.MaximumCollarSurfaceDistance;
        if (!float.IsFinite(transitionSpan) ||
            transitionSpan <= PositionEpsilon)
        {
            assignments = new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
                new Dictionary<GeometryVertex, SemanticVertexAssignment>());
            coreVertices = [];
            transitionVertices = [];
            failure = "Semantic Head has an invalid immutable external Neck collar.";
            return false;
        }

        var provedAssembly = lobe.PrimaryVertices.ToHashSet();
        provedAssembly.UnionWith(lobe.SecondaryVertices);
        bool hasUnguardedRigidBoundaryEdge = lobe.Vertices.Any(vertex =>
            adjacency.TryGetValue(
                vertex, out IReadOnlyList<GeometryVertex>? neighbours) &&
            neighbours.Any(neighbour =>
                lobe.OwnerVertices.Contains(neighbour) &&
                !lobe.Vertices.Contains(neighbour) &&
                !lobe.NeckCollarVertices.Contains(neighbour)));
        bool invalidClusterProof =
            calibration.Region != GeneratedSkinningSemanticRegion.Head ||
            !provedAssembly.SetEquals(lobe.Vertices) ||
            lobe.PrimaryVertices.Overlaps(lobe.SecondaryVertices) ||
            !lobe.SeedVertices.IsSubsetOf(lobe.Vertices) ||
            lobe.BoundaryVertices.Count == 0 ||
            !lobe.BoundaryVertices.IsSubsetOf(lobe.Vertices) ||
            lobe.NeckCollarVertices.Count == 0 ||
            !lobe.NeckCollarVertices.IsSubsetOf(lobe.OwnerVertices) ||
            lobe.Vertices.Overlaps(lobe.NeckCollarVertices) ||
            hasUnguardedRigidBoundaryEdge ||
            lobe.NeckCollarHeadWeights.Count != lobe.NeckCollarVertices.Count ||
            !lobe.NeckCollarHeadWeights.Keys.ToHashSet()
                .SetEquals(lobe.NeckCollarVertices);
        if (invalidClusterProof)
        {
            assignments = new ReadOnlyDictionary<GeometryVertex,
                SemanticVertexAssignment>(
                new Dictionary<GeometryVertex, SemanticVertexAssignment>());
            coreVertices = [];
            transitionVertices = [];
            failure =
                "Semantic Head donor-cluster proof is internally inconsistent; " +
                "refusing to emit weights outside one proved rigid Head assembly " +
                "and its external same-owner Neck collar.";
            return false;
        }

        var mutableAssignments = new Dictionary<GeometryVertex,
            SemanticVertexAssignment>();
        var core = new List<GeometryVertex>();
        var transition = new List<GeometryVertex>();
        foreach (GeometryVertex vertex in lobe.Vertices
                     .OrderBy(vertex => vertex.MeshIndex)
                     .ThenBy(vertex => vertex.VertexIndex))
        {
            core.Add(vertex);
            mutableAssignments.Add(vertex, new SemanticVertexAssignment(
                calibration.Region,
                SemanticVertexZone.Core,
                calibration.AnchorSkeletonJointIndex,
                calibration.ProximalSkeletonJointIndex,
                1));
        }
        foreach (GeometryVertex vertex in lobe.NeckCollarVertices
                     .OrderBy(vertex => vertex.MeshIndex)
                     .ThenBy(vertex => vertex.VertexIndex))
        {
            float anchorWeight = lobe.NeckCollarHeadWeights[vertex];
            transition.Add(vertex);
            mutableAssignments.Add(vertex, new SemanticVertexAssignment(
                calibration.Region,
                SemanticVertexZone.ProximalTransition,
                calibration.AnchorSkeletonJointIndex,
                calibration.ProximalSkeletonJointIndex,
                anchorWeight));
        }

        bool invalidAssignmentPostcondition =
            mutableAssignments.Count !=
                lobe.Vertices.Count + lobe.NeckCollarVertices.Count ||
            !mutableAssignments.Keys.ToHashSet().SetEquals(
                lobe.Vertices.Concat(lobe.NeckCollarVertices)) ||
            mutableAssignments.Any(pair =>
            {
                bool isTransition = lobe.NeckCollarVertices.Contains(pair.Key);
                SemanticVertexAssignment assignment = pair.Value;
                return assignment.Region != GeneratedSkinningSemanticRegion.Head ||
                       assignment.AnchorSkeletonJointIndex !=
                       calibration.AnchorSkeletonJointIndex ||
                       assignment.ProximalSkeletonJointIndex !=
                       calibration.ProximalSkeletonJointIndex ||
                       !float.IsFinite(assignment.AnchorWeight) ||
                       assignment.AnchorWeight < 0 ||
                       assignment.AnchorWeight > 1 ||
                       isTransition !=
                       (assignment.Zone ==
                        SemanticVertexZone.ProximalTransition) ||
                       !isTransition &&
                       (assignment.Zone != SemanticVertexZone.Core ||
                        assignment.AnchorWeight != 1);
            }) ||
            lobe.Vertices.Any(vertex =>
                !mutableAssignments.TryGetValue(
                    vertex, out SemanticVertexAssignment? headAssignment) ||
                headAssignment is null ||
                headAssignment.Zone != SemanticVertexZone.Core ||
                headAssignment.AnchorWeight != 1) ||
            lobe.NeckCollarVertices.Any(vertex =>
            {
                Vector3 position = alignedPositionsByMesh[vertex.MeshIndex]
                    [vertex.VertexIndex];
                float axialProjection = Vector3.Dot(position, volume.AxialAxis);
                return !float.IsFinite(axialProjection) ||
                       axialProjection >=
                        lobe.CutPlaneProjection - PositionEpsilon;
            });
        if (invalidAssignmentPostcondition)
        {
            assignments = new ReadOnlyDictionary<GeometryVertex,
                SemanticVertexAssignment>(
                new Dictionary<GeometryVertex, SemanticVertexAssignment>());
            coreVertices = [];
            transitionVertices = [];
            failure =
                "Semantic Head assignment postcondition failed: every proved " +
                "primary/secondary Head vertex must be one-hot Head, while every " +
                "transition vertex must remain in the external Neck collar.";
            return false;
        }

        int uniqueCorePositions = core
            .Select(vertex => donorSources[vertex.MeshIndex].Positions[vertex.VertexIndex])
            .Distinct()
            .Count();
        int uniqueTransitionPositions = transition
            .Select(vertex => donorSources[vertex.MeshIndex].Positions[vertex.VertexIndex])
            .Distinct()
            .Count();
        if (uniqueCorePositions < MinimumCapturedSemanticCorePositions ||
            uniqueTransitionPositions == 0)
        {
            assignments = new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
                new Dictionary<GeometryVertex, SemanticVertexAssignment>());
            coreVertices = [];
            transitionVertices = [];
            failure =
                $"Semantic Head donor lobe contains {uniqueCorePositions} unique core " +
                $"and {uniqueTransitionPositions} external Neck-collar position(s) after " +
                "the requested adjustment.";
            return false;
        }

        assignments = new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
            mutableAssignments);
        coreVertices = BuildSemanticMembership(core, donorSources);
        transitionVertices = BuildSemanticMembership(transition, donorSources);
        failure =
            $"Semantic Head rigidly captured the complete validated donor assembly from " +
            $"body component #{lobe.BodyComponentIndex}: " +
            $"{lobe.PrimaryVertices.Count} primary and " +
            $"{lobe.SecondaryVertices.Count} secondary face-shell vertices, plus " +
            $"{lobe.NeckCollarVertices.Count} external same-owner Neck-collar " +
            "vertices; every proved face vertex is one-hot Head and none remains " +
            "on the legacy capsule path.";
        return true;
    }

    private static float DistanceToSemanticVolume(
        Vector3 position,
        GeneratedSkinningRegionVolume volume)
    {
        Vector3 relative = position - volume.Center;
        float axial = Vector3.Dot(relative, volume.AxialAxis) / volume.AxialRadius;
        float lateral = Vector3.Dot(relative, volume.LateralAxis) /
                        volume.LateralRadius;
        float forward = Vector3.Dot(relative, volume.ForwardAxis) /
                        volume.ForwardRadius;
        float exponent = volume.ShapeExponent;
        if (!float.IsFinite(exponent) || exponent < 2)
        {
            throw new InvalidDataException(
                "A semantic region has an invalid superellipsoid exponent.");
        }
        float powered = MathF.Pow(MathF.Abs(axial), exponent) +
                        MathF.Pow(MathF.Abs(lateral), exponent) +
                        MathF.Pow(MathF.Abs(forward), exponent);
        return MathF.Pow(MathF.Max(0, powered), 1 / exponent);
    }

    private static IReadOnlyDictionary<GeometryVertex,
        IReadOnlyList<GeometryVertex>> BuildSelectedBodyAdjacency(
        IReadOnlyList<GeometryComponent> bodyComponents,
        IReadOnlyList<GeometrySource> donorSources,
        SceneTopology topology)
    {
        var body = bodyComponents
            .SelectMany(component => component.Vertices)
            .ToHashSet();
        var neighbours = body.ToDictionary(
            vertex => vertex,
            _ => new HashSet<GeometryVertex>());
        for (int meshIndex = 0; meshIndex < donorSources.Count; meshIndex++)
        {
            IReadOnlyList<uint> indices = topology.RenderableTriangleIndicesByMesh[meshIndex];
            for (int index = 0; index < indices.Count; index += 3)
            {
                var a = new GeometryVertex(meshIndex, checked((int)indices[index]));
                var b = new GeometryVertex(meshIndex, checked((int)indices[index + 1]));
                var c = new GeometryVertex(meshIndex, checked((int)indices[index + 2]));
                AddSemanticEdge(a, b, body, neighbours);
                AddSemanticEdge(b, c, body, neighbours);
                AddSemanticEdge(c, a, body, neighbours);
            }
        }
        // A single exact contact between separate components is not proof of a
        // surface seam (a hand may touch clothing or the torso in the fitting
        // pose). BuildTopology already merges raw components which share at
        // least two exact positions, so only bridge duplicates inside each
        // proven selected-body component here. The global validation below
        // still rejects a semantic region if coincident selected components
        // would otherwise receive different classifications.
        foreach (GeometryComponent component in bodyComponents)
        {
            foreach (IGrouping<Vector3, GeometryVertex> seam in component.Vertices
                         .GroupBy(vertex =>
                             donorSources[vertex.MeshIndex].Positions[vertex.VertexIndex]))
            {
                GeometryVertex[] duplicates = seam
                    .OrderBy(vertex => vertex.MeshIndex)
                    .ThenBy(vertex => vertex.VertexIndex)
                    .ToArray();
                for (int index = 1; index < duplicates.Length; index++)
                {
                    AddSemanticEdge(
                        duplicates[0], duplicates[index], body, neighbours);
                }
            }
        }
        return new ReadOnlyDictionary<GeometryVertex, IReadOnlyList<GeometryVertex>>(
            neighbours.ToDictionary(
                pair => pair.Key,
                pair => (IReadOnlyList<GeometryVertex>)Array.AsReadOnly(
                    pair.Value.OrderBy(value => value.MeshIndex)
                        .ThenBy(value => value.VertexIndex)
                        .ToArray())));
    }

    private static void AddSemanticEdge(
        GeometryVertex first,
        GeometryVertex second,
        IReadOnlySet<GeometryVertex> body,
        IReadOnlyDictionary<GeometryVertex, HashSet<GeometryVertex>> neighbours)
    {
        if (!body.Contains(first) || !body.Contains(second))
            return;
        neighbours[first].Add(second);
        neighbours[second].Add(first);
    }

    private static IReadOnlyList<TargetRigBodyVertexMembership> BuildSemanticMembership(
        IReadOnlyList<GeometryVertex> vertices,
        IReadOnlyList<GeometrySource> donorSources)
    {
        TargetRigBodyVertexMembership[] result = vertices
            .GroupBy(vertex => vertex.MeshIndex)
            .OrderBy(group => group.Key)
            .Select(group => new TargetRigBodyVertexMembership(
                group.Key,
                donorSources[group.Key].Name,
                Array.AsReadOnly(group.Select(vertex => vertex.VertexIndex)
                    .Distinct()
                    .Order()
                    .ToArray())))
            .ToArray();
        return new ReadOnlyCollection<TargetRigBodyVertexMembership>(result);
    }

    private static IReadOnlyDictionary<int, SemanticComponentAssignment>
        ResolveHeadCompanionComponents(
            IDictionary<GeneratedSkinningSemanticRegion, SemanticRegionCandidate> candidates,
            IReadOnlyList<GeometryComponent> bodyComponents,
            IReadOnlyList<GeometrySource> donorSources,
            SceneTopology topology,
            IReadOnlyList<Vector3[]> alignedPositionsByMesh,
            IReadOnlySet<int> manuallyAssignedComponentIndices)
    {
        var result = new Dictionary<int, SemanticComponentAssignment>();
        if (!candidates.TryGetValue(
                GeneratedSkinningSemanticRegion.Head,
                out SemanticRegionCandidate? headCandidate) ||
            !headCandidate.Resolution.IsApplied ||
            headCandidate.Resolution.ResolvedVolume is not { } volume ||
            IsManualSemanticVolumeAdjustment(headCandidate.Resolution.Adjustment))
        {
            return new ReadOnlyDictionary<int, SemanticComponentAssignment>(result);
        }

        HashSet<GeometryVertex> coreVertices = headCandidate.Assignments
            .Where(pair => pair.Value.Zone == SemanticVertexZone.Core)
            .Select(pair => pair.Key)
            .ToHashSet();
        Vector3[] corePositions = coreVertices
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .ToArray();
        if (corePositions.Length < MinimumCapturedSemanticCorePositions)
            return new ReadOnlyDictionary<int, SemanticComponentAssignment>(result);
        var corePositionIndex = new ExactVector3NearestIndex(corePositions);

        Vector3[] nonHeadBodyPositions = bodyComponents
            .SelectMany(component => component.Vertices)
            .Distinct()
            .Where(vertex => !coreVertices.Contains(vertex))
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .ToArray();
        ExactVector3NearestIndex? nonHeadBodyIndex =
            nonHeadBodyPositions.Length == 0
                ? null
                : new ExactVector3NearestIndex(nonHeadBodyPositions);

        HashSet<int> bodyComponentIndices = bodyComponents
            .Select(component => component.ComponentIndex)
            .ToHashSet();
        IReadOnlyDictionary<Vector3, GeometryVertex[]> bodyVerticesByPosition =
            bodyComponents
                .SelectMany(component => component.Vertices)
                .Distinct()
                .GroupBy(vertex =>
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex])
                .ToDictionary(
                    group => group.Key,
                    group => group.OrderBy(vertex => vertex.MeshIndex)
                        .ThenBy(vertex => vertex.VertexIndex)
                        .ToArray());
        float maximumSurfaceGap = MathF.Max(
            MathF.Min(volume.LateralRadius, volume.ForwardRadius) *
            HeadCompanionMaximumSurfaceGapRatio,
            PositionEpsilon * 16);
        var strictCompanionPositions = new Dictionary<int, Vector3[]>();
        var clusteredCandidates = new List<(
            GeometryComponent Component,
            Vector3[] Positions,
            int HeadPreferredPositionCount)>();
        foreach (GeometryComponent component in topology.Components
                     .Where(component =>
                         !bodyComponentIndices.Contains(component.ComponentIndex) &&
                         !manuallyAssignedComponentIndices.Contains(
                             component.ComponentIndex))
                     .OrderBy(component => component.ComponentIndex))
        {
            Vector3[] positions = component.Vertices
                .Select(vertex =>
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex])
                .Distinct()
                .ToArray();
            if (positions.Length < 3)
                continue;

            float[] axial = Project(positions, volume.Center, volume.AxialAxis);
            float[] lateral = Project(positions, volume.Center, volume.LateralAxis);
            float[] forward = Project(positions, volume.Center, volume.ForwardAxis);
            float axialSpan = axial[^1] - axial[0];
            float lateralSpan = lateral[^1] - lateral[0];
            float forwardSpan = forward[^1] - forward[0];
            if (axialSpan > volume.AxialRadius * 1.5f ||
                lateralSpan > volume.LateralRadius * 2 ||
                forwardSpan > volume.ForwardRadius * 2)
            {
                continue;
            }

            float[] envelopeDistances = positions
                .Select(position => DistanceToSemanticVolume(position, volume))
                .Order()
                .ToArray();
            if (Quantile(envelopeDistances, RobustUpperQuantile) >
                HeadCompanionEnvelopeRatio)
            {
                continue;
            }

            bool coincidentWithNonCore = positions.Any(position =>
                bodyVerticesByPosition.TryGetValue(
                    position, out GeometryVertex[]? bodyAtPosition) &&
                bodyAtPosition.Any(vertex =>
                    !headCandidate.Assignments.TryGetValue(
                        vertex, out SemanticVertexAssignment? assignment) ||
                    assignment.Zone != SemanticVertexZone.Core));
            if (coincidentWithNonCore)
                continue;

            var surfaceDistances = new float[positions.Length];
            bool invalidSurfaceDistance = false;
            int headPreferredPositionCount = nonHeadBodyIndex is null
                ? positions.Length
                : 0;
            for (int positionIndex = 0;
                 positionIndex < positions.Length;
                 positionIndex++)
            {
                float minimumSquared =
                    corePositionIndex.FindNearestDistanceSquared(
                        positions[positionIndex]);
                if (!float.IsFinite(minimumSquared))
                {
                    invalidSurfaceDistance = true;
                    break;
                }
                float headDistance = MathF.Sqrt(minimumSquared);
                surfaceDistances[positionIndex] = headDistance;

                if (nonHeadBodyIndex is not null)
                {
                    float nonHeadMinimumSquared =
                        nonHeadBodyIndex.FindNearestDistanceSquared(
                            positions[positionIndex]);
                    if (!float.IsFinite(nonHeadMinimumSquared) ||
                        !float.IsFinite(MathF.Sqrt(nonHeadMinimumSquared)))
                    {
                        invalidSurfaceDistance = true;
                        break;
                    }
                    if (headDistance <= MathF.Sqrt(nonHeadMinimumSquared) +
                        PositionEpsilon * 16)
                    {
                        headPreferredPositionCount++;
                    }
                }
            }
            if (invalidSurfaceDistance)
                continue;
            Array.Sort(surfaceDistances);
            if (Quantile(surfaceDistances, RobustUpperQuantile) >
                maximumSurfaceGap)
            {
                continue;
            }

            if (headPreferredPositionCount == positions.Length)
            {
                result.Add(
                    component.ComponentIndex,
                    new SemanticComponentAssignment(
                        GeneratedSkinningSemanticRegion.Head,
                        headCandidate.Resolution.AnchorBoneName,
                        headCandidate.Resolution.AnchorSkeletonJointIndex));
                strictCompanionPositions.Add(component.ComponentIndex, positions);
            }
            else if (headPreferredPositionCount > 0)
            {
                // Preserve an ambiguous compact candidate only for the bounded
                // one-hop assembly proof below. It cannot seed or recursively
                // extend a Head cluster on its own.
                clusteredCandidates.Add((
                    component,
                    positions,
                    headPreferredPositionCount));
            }
        }

        if (strictCompanionPositions.Count > 0)
        {
            Vector3[] strictPositions = strictCompanionPositions.Values
                .SelectMany(value => value)
                .Distinct()
                .ToArray();
            var strictPositionIndex =
                new ExactVector3NearestIndex(strictPositions);
            float maximumAssemblyGap = maximumSurfaceGap * 0.25f;
            foreach ((GeometryComponent component,
                         Vector3[] positions,
                         int headPreferredPositionCount) in clusteredCandidates
                         .OrderBy(value => value.Component.ComponentIndex))
            {
                if (headPreferredPositionCount * 4 < positions.Length)
                    continue;
                float minimumAssemblySquared = positions.Min(position =>
                    strictPositionIndex.FindNearestDistanceSquared(position));
                if (!float.IsFinite(minimumAssemblySquared) ||
                    MathF.Sqrt(minimumAssemblySquared) >
                    maximumAssemblyGap + PositionEpsilon)
                {
                    continue;
                }
                result.Add(
                    component.ComponentIndex,
                    new SemanticComponentAssignment(
                        GeneratedSkinningSemanticRegion.Head,
                        headCandidate.Resolution.AnchorBoneName,
                        headCandidate.Resolution.AnchorSkeletonJointIndex));
            }
        }

        if (result.Count > 0)
        {
            int[] companionIndices = result.Keys.Order().ToArray();
            GeneratedSkinningRegionResolution resolution =
                headCandidate.Resolution with
                {
                    RigidCompanionComponentIndices =
                        Array.AsReadOnly(companionIndices),
                    Warnings = new ReadOnlyCollection<string>(
                        headCandidate.Resolution.Warnings.Append(
                            $"Semantic Head assembly proved {companionIndices.Length} " +
                            $"compact detached companion component(s): " +
                            $"{string.Join(", ", companionIndices.Select(index => $"#{index}"))}. " +
                            "Manual component assignments retain precedence.")
                            .ToArray())
                };
            candidates[GeneratedSkinningSemanticRegion.Head] =
                headCandidate with { Resolution = resolution };
        }

        return new ReadOnlyDictionary<int, SemanticComponentAssignment>(result);
    }

    private static GeneratedSkinningRegionResolution CreateSemanticRegionResolution(
        GeneratedSkinningSemanticRegion region,
        GeneratedSkinningRegionAdjustment adjustment,
        SemanticRegionCalibration? calibration,
        GeneratedSkinningRegionStatus status,
        bool isApplied,
        GeneratedSkinningRegionVolume? resolvedVolume,
        IReadOnlyList<TargetRigBodyVertexMembership> coreVertices,
        IReadOnlyList<TargetRigBodyVertexMembership> transitionVertices,
        IReadOnlyList<string> messages)
    {
        var unavailableLimits = new GeneratedSkinningRegionAdjustmentLimits(
            0, 0, 1, 1, 1, 1);
        return new GeneratedSkinningRegionResolution(
            region,
            adjustment.Enabled,
            isApplied,
            status,
            adjustment,
            calibration?.AdjustmentLimits ?? unavailableLimits,
            calibration?.AnchorBoneName ?? string.Empty,
            calibration?.AnchorSkeletonJointIndex ?? -1,
            calibration?.ProximalBoneName ?? string.Empty,
            calibration?.ProximalSkeletonJointIndex ?? -1,
            calibration?.AutomaticVolume,
            resolvedVolume,
            calibration?.CalibrationSampleCount ?? 0,
            coreVertices,
            transitionVertices,
            new ReadOnlyCollection<string>(messages.ToArray()))
        {
            MotionProxyBoneName =
                calibration?.HandMotionProfile?.ProxyBoneName ?? string.Empty,
            MotionProxySkeletonJointIndex =
                calibration?.HandMotionProfile?.ProxySkeletonJointIndex ?? -1,
            MotionBranchBoneNames = calibration?.HandMotionProfile?.Branches
                .Select(branch => branch.RootBoneName)
                .ToArray() ?? [],
            MaximumMotionProxyWeight =
                calibration?.HandMotionProfile is null ? 0 : 1
        };
    }

    private static int CountMembershipVertices(
        IReadOnlyList<TargetRigBodyVertexMembership> membership) =>
        membership.Sum(value => value.VertexIndices.Count);

    private static HashSet<GeneratedSkinningSemanticRegion> FindOverlappingRegions(
        IReadOnlyDictionary<GeneratedSkinningSemanticRegion,
            SemanticRegionCandidate> candidates)
    {
        var owners = new Dictionary<GeometryVertex,
            GeneratedSkinningSemanticRegion>();
        var rejected = new HashSet<GeneratedSkinningSemanticRegion>();
        foreach ((GeneratedSkinningSemanticRegion region, SemanticRegionCandidate candidate) in
                 candidates.OrderBy(pair => pair.Key))
        {
            if (!candidate.Resolution.IsApplied)
                continue;
            foreach (GeometryVertex vertex in candidate.Assignments.Keys)
            {
                if (owners.TryGetValue(
                        vertex, out GeneratedSkinningSemanticRegion previous))
                {
                    rejected.Add(previous);
                    rejected.Add(region);
                }
                else
                {
                    owners.Add(vertex, region);
                }
            }
        }
        return rejected;
    }

    private static HashSet<GeneratedSkinningSemanticRegion> FindSeamDivergentRegions(
        IReadOnlyDictionary<GeneratedSkinningSemanticRegion,
            SemanticRegionCandidate> candidates,
        IReadOnlyList<GeometryComponent> bodyComponents,
        IReadOnlyList<GeometrySource> donorSources,
        IReadOnlySet<GeneratedSkinningSemanticRegion> alreadyRejected,
        IDictionary<GeneratedSkinningSemanticRegion, List<string>>
            diagnosticsByRegion)
    {
        var assignments = candidates.Values
            .Where(value => value.Resolution.IsApplied &&
                            !alreadyRejected.Contains(value.Resolution.Region))
            .SelectMany(value => value.Assignments)
            .ToDictionary(pair => pair.Key, pair => pair.Value);
        var rejected = new HashSet<GeneratedSkinningSemanticRegion>();
        GeometryVertex[] body = bodyComponents
            .SelectMany(component => component.Vertices)
            .Distinct()
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        foreach (IGrouping<Vector3, GeometryVertex> seam in body
                     .GroupBy(vertex =>
                         donorSources[vertex.MeshIndex].Positions[vertex.VertexIndex]))
        {
            GeometryVertex[] vertices = seam.ToArray();
            SemanticVertexAssignment[] values = vertices
                .Where(assignments.ContainsKey)
                .Select(vertex => assignments[vertex])
                .ToArray();
            if (values.Length == 0)
                continue;
            bool divergent = values.Length != vertices.Length ||
                             values.Any(value =>
                                 value.Region != values[0].Region ||
                                 value.Zone != values[0].Zone ||
                                 BitConverter.SingleToInt32Bits(value.AnchorWeight) !=
                                 BitConverter.SingleToInt32Bits(
                                     values[0].AnchorWeight) ||
                                 !value.CoreMotionInfluences.SequenceEqual(
                                     values[0].CoreMotionInfluences));
            if (divergent)
            {
                string detail =
                    $"Coincident position ({seam.Key.X:G9},{seam.Key.Y:G9}," +
                    $"{seam.Key.Z:G9}): " +
                    string.Join(", ", vertices
                        .OrderBy(vertex => vertex.MeshIndex)
                        .ThenBy(vertex => vertex.VertexIndex)
                        .Select(vertex => assignments.TryGetValue(
                            vertex, out SemanticVertexAssignment? value)
                            ? $"m{vertex.MeshIndex}:v{vertex.VertexIndex}=" +
                              $"{value.Region}/{value.Zone}/" +
                              $"w{value.AnchorWeight:G9}"
                            : $"m{vertex.MeshIndex}:v{vertex.VertexIndex}=unassigned"));
                foreach (GeneratedSkinningSemanticRegion region in values
                             .Select(value => value.Region)
                             .Distinct())
                {
                    rejected.Add(region);
                    if (!diagnosticsByRegion.TryGetValue(
                            region, out List<string>? diagnostics))
                    {
                        diagnostics = [];
                        diagnosticsByRegion.Add(region, diagnostics);
                    }
                    if (diagnostics.Count < 8)
                        diagnostics.Add(detail);
                }
            }
        }
        return rejected;
    }

    private static GeneratedVertexInfluences BuildSemanticVertexInfluences(
        SemanticVertexAssignment assignment)
    {
        PackedInfluence[] influences;
        if (assignment.Zone == SemanticVertexZone.Core &&
            assignment.CoreMotionInfluences.Count > 0)
        {
            influences = assignment.CoreMotionInfluences.ToArray();
            float total = influences.Sum(value => value.Weight);
            if (influences.Length is < 1 or > 2 ||
                influences.Select(value => value.Joint).Distinct().Count() !=
                influences.Length ||
                influences.Any(value =>
                    !float.IsFinite(value.Weight) || value.Weight <= 0) ||
                !float.IsFinite(total) || MathF.Abs(total - 1) > 0.00001f)
            {
                throw new InvalidDataException(
                    $"Semantic {assignment.Region} has an invalid approximate " +
                    "finger-chain assignment.");
            }
        }
        else if (assignment.Zone == SemanticVertexZone.Core ||
            assignment.AnchorWeight >= 1 - WeightEpsilon)
        {
            influences =
            [new PackedInfluence(
                checked((ushort)assignment.AnchorSkeletonJointIndex), 1)];
        }
        else if (assignment.AnchorWeight <= WeightEpsilon)
        {
            influences =
            [new PackedInfluence(
                checked((ushort)assignment.ProximalSkeletonJointIndex), 1)];
        }
        else
        {
            influences =
            [
                new PackedInfluence(
                    checked((ushort)assignment.AnchorSkeletonJointIndex),
                    assignment.AnchorWeight),
                new PackedInfluence(
                    checked((ushort)assignment.ProximalSkeletonJointIndex),
                    1 - assignment.AnchorWeight)
            ];
        }
        return new GeneratedVertexInfluences(
            influences,
            influences.ToArray(),
            DiscardedTopFourWeightMass: 0,
            TopFourToFinalWeightL1Distance: 0,
            AnatomicalVolumeAffected: false);
    }

    private static string ComputeRegionAlignmentFingerprint(
        GeneratedSkinningAlignment alignment)
    {
        using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        AppendRegionHashFloat(hash, alignment.Scale);
        AppendRegionHashVector(hash, alignment.RotationDegrees);
        AppendRegionHashVector(hash, alignment.Translation);
        return Convert.ToHexString(hash.GetHashAndReset());
    }

    private static string ComputeRegionFittingPoseFingerprint(
        TargetRigDefinition rig,
        IReadOnlyList<Matrix4x4>? fittingWorldMatrices)
    {
        using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        AppendRegionHashInt(hash, rig.Joints.Count);
        for (int index = 0; index < rig.Joints.Count; index++)
        {
            Matrix4x4 matrix = fittingWorldMatrices is null
                ? rig.Joints[index].BindWorldMatrix
                : fittingWorldMatrices[index];
            AppendRegionHashMatrix(hash, matrix);
        }
        return Convert.ToHexString(hash.GetHashAndReset());
    }

    private static void AppendRegionHashMatrix(
        IncrementalHash hash,
        Matrix4x4 value)
    {
        AppendRegionHashFloat(hash, value.M11);
        AppendRegionHashFloat(hash, value.M12);
        AppendRegionHashFloat(hash, value.M13);
        AppendRegionHashFloat(hash, value.M14);
        AppendRegionHashFloat(hash, value.M21);
        AppendRegionHashFloat(hash, value.M22);
        AppendRegionHashFloat(hash, value.M23);
        AppendRegionHashFloat(hash, value.M24);
        AppendRegionHashFloat(hash, value.M31);
        AppendRegionHashFloat(hash, value.M32);
        AppendRegionHashFloat(hash, value.M33);
        AppendRegionHashFloat(hash, value.M34);
        AppendRegionHashFloat(hash, value.M41);
        AppendRegionHashFloat(hash, value.M42);
        AppendRegionHashFloat(hash, value.M43);
        AppendRegionHashFloat(hash, value.M44);
    }

    private static void AppendRegionHashVector(
        IncrementalHash hash,
        Vector3 value)
    {
        AppendRegionHashFloat(hash, value.X);
        AppendRegionHashFloat(hash, value.Y);
        AppendRegionHashFloat(hash, value.Z);
    }

    private static void AppendRegionHashFloat(
        IncrementalHash hash,
        float value) =>
        AppendRegionHashInt(hash, BitConverter.SingleToInt32Bits(value));

    private static void AppendRegionHashInt(
        IncrementalHash hash,
        int value)
    {
        Span<byte> bytes = stackalloc byte[sizeof(int)];
        BinaryPrimitives.WriteInt32LittleEndian(bytes, value);
        hash.AppendData(bytes);
    }
}
