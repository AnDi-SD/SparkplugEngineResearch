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
    private const float HeadAssemblyMaximumIslandGapRatio = 0.10f;
    private const float HeadAssemblyMaximumProtectedVertexFraction = 0.70f;
    private const float MaximumProtectedRegionScale = 1.75f;

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
            CapsuleInfluences,
        SeparationPlanePreparation SeparationPlanes);

    private sealed record SemanticComponentAssignment(
        GeneratedSkinningSemanticRegion Region,
        string AnchorBoneName,
        int AnchorSkeletonJointIndex);

    private sealed record DecodedTargetWeightSample(
        Vector3 Position,
        IReadOnlyDictionary<int, float> WeightByRigJoint,
        float TotalWeight);

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
        SeparationPlanePreparation separationPlanes,
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
                        maximumInfluences,
                        separationPlanes)));
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
            if (targetCalibration is not null &&
                region != GeneratedSkinningSemanticRegion.Head)
            {
                calibration = TryRefineCompleteHandCalibrationFromDonorTopology(
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
            SemanticRegionCalibration? displayCalibration =
                calibration ?? targetCalibration;
            if (region == GeneratedSkinningSemanticRegion.Head)
            {
                separationPlanes.ByKind.TryGetValue(
                    GeneratedSkinningSeparationPlaneKind.Head,
                    out GeneratedSkinningSeparationPlaneResolution? headPlane);
                bool enabled = headPlane is { IsEnabled: true };
                if (!enabled ||
                    calibration is null ||
                    headPlane is not { IsAvailable: true })
                {
                    if (enabled && headPlane is not { IsAvailable: true })
                    {
                        messages.AddRange(
                            headPlane?.Warnings ??
                            ["The Head separation plane is unavailable."]);
                    }
                    GeneratedSkinningRegionStatus status = !enabled
                        ? GeneratedSkinningRegionStatus.Disabled
                        : GeneratedSkinningRegionStatus.UnsafeCalibration;
                    GeneratedSkinningRegionResolution resolution =
                        CreateSemanticRegionResolution(
                            region,
                            adjustment,
                            displayCalibration,
                            status,
                            isApplied: false,
                            resolvedVolume: null,
                            coreVertices: [],
                            transitionVertices: [],
                            messages) with
                        {
                            IsEnabled = enabled,
                            AutomaticVolume = null,
                            SeparationPlane = headPlane
                        };
                    candidates.Add(region, new SemanticRegionCandidate(
                        resolution,
                        new ReadOnlyDictionary<GeometryVertex,
                            SemanticVertexAssignment>(
                            new Dictionary<GeometryVertex,
                                SemanticVertexAssignment>())));
                    continue;
                }

                if (!TryCaptureSemanticHeadPlane(
                        calibration,
                        headPlane,
                        donorBodyComponents,
                        donorSources,
                        alignedPositionsByMesh,
                        out IReadOnlyDictionary<GeometryVertex,
                            SemanticVertexAssignment> headAssignments,
                        out IReadOnlyList<TargetRigBodyVertexMembership>
                            headCoreVertices,
                        out string? headCaptureDiagnostic))
                {
                    if (!string.IsNullOrWhiteSpace(headCaptureDiagnostic))
                        messages.Add(headCaptureDiagnostic);
                    GeneratedSkinningRegionResolution resolution =
                        CreateSemanticRegionResolution(
                            region,
                            adjustment,
                            calibration,
                            GeneratedSkinningRegionStatus.UnsafeCalibration,
                            isApplied: false,
                            resolvedVolume: null,
                            coreVertices: [],
                            transitionVertices: [],
                            messages) with
                        {
                            IsEnabled = enabled,
                            AutomaticVolume = null,
                            SeparationPlane = headPlane
                        };
                    candidates.Add(region, new SemanticRegionCandidate(
                        resolution,
                        new ReadOnlyDictionary<GeometryVertex,
                            SemanticVertexAssignment>(
                            new Dictionary<GeometryVertex,
                                SemanticVertexAssignment>())));
                    continue;
                }

                if (!string.IsNullOrWhiteSpace(headCaptureDiagnostic))
                    messages.Add(headCaptureDiagnostic);
                messages.Add(
                    $"Semantic Head hard plane captured " +
                    $"{CountMembershipVertices(headCoreVertices)} selected-body " +
                    "vertex/vertices as rigid one-hot Head weights; it has no " +
                    "deforming Neck transition.");
                GeneratedSkinningRegionResolution applied =
                    CreateSemanticRegionResolution(
                        region,
                        adjustment,
                        calibration,
                        GeneratedSkinningRegionStatus.Applied,
                        isApplied: true,
                        resolvedVolume: null,
                        headCoreVertices,
                        transitionVertices: [],
                        messages) with
                    {
                        IsEnabled = enabled,
                        AutomaticVolume = null,
                        SeparationPlane = headPlane
                    };
                candidates.Add(region, new SemanticRegionCandidate(
                    applied,
                    headAssignments));
                continue;
            }
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
            if (!TryCaptureCompleteSemanticHandLobe(
                    calibration,
                    resolvedVolume,
                    donorSources,
                    alignedPositionsByMesh,
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
            ResolveHeadPlaneCompanionComponents(
                candidates,
                donorBodyComponents,
                donorTopology,
                alignedPositionsByMesh,
                manuallyAssignedComponentIndices,
                separationPlanes);
        var analysis = new GeneratedSkinningRegionAnalysis(
            new ReadOnlyCollection<GeneratedSkinningRegionResolution>(
                candidates.OrderBy(pair => pair.Key)
                    .Select(pair => pair.Value.Resolution)
                    .ToArray()),
            targetFingerprint,
            donorFingerprint,
            alignmentFingerprint,
            fittingPoseFingerprint)
        {
            SeparationPlanes = separationPlanes.Resolutions
        };
        return new SemanticRegionPreparation(
            analysis,
            new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
                finalAssignments),
            componentAssignments,
            capsuleInfluences,
            separationPlanes);
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
                adjustment.AxialScale <= 0 || adjustment.RadialScale <= 0)
            {
                throw new InvalidDataException(
                    $"Semantic {adjustment.Region} adjustment must contain a finite " +
                    "axial offset and positive finite scales.");
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

        if (region == GeneratedSkinningSemanticRegion.Head)
        {
            Vector3 headPosedProximal = Translation(GetFittingWorldMatrix(
                proximal,
                fittingWorldMatrices));
            Matrix4x4 headPosedAnchorMatrix = GetFittingWorldMatrix(
                anchor,
                fittingWorldMatrices);
            Vector3 headPosedAnchor = Translation(headPosedAnchorMatrix);
            SideCalibration headPosedSides = CalibrateSides(
                layout.DeformJoints,
                targetBounds,
                fittingWorldMatrices);
            BuildSemanticFrame(
                headPosedProximal,
                headPosedAnchor,
                headPosedAnchorMatrix,
                GetCalibratedLateralAxis(headPosedSides),
                out Vector3 axial,
                out Vector3 lateral,
                out Vector3 forward);
            float diagnosticRadius = MathF.Max(
                targetBounds.Size.Y * MinimumSemanticRadiusHeightRatio,
                PositionEpsilon * 16);
            var diagnosticVolume = new GeneratedSkinningRegionVolume(
                headPosedAnchor,
                axial,
                lateral,
                forward,
                diagnosticRadius,
                diagnosticRadius,
                diagnosticRadius,
                0);
            messages.Add(
                "Semantic Head calibration uses exact Head/Neck joints and the " +
                "hard Head plane; target transition samples are not required.");
            return new SemanticRegionCalibration(
                region,
                anchorName,
                anchor.JointIndex,
                anchorSkeletonIndex,
                proximal.Name,
                proximalRigJoint,
                proximalSkeletonIndex,
                headPosedAnchor,
                diagnosticVolume,
                new GeneratedSkinningRegionAdjustmentLimits(0, 0, 1, 1, 1, 1),
                CalibrationSampleCount: 0);
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
            ShapeExponent = 4
        };
        var limits = new GeneratedSkinningRegionAdjustmentLimits(
            -axialRadius * 0.35f,
            axialRadius * 0.35f,
            0.5f,
            MaximumProtectedRegionScale,
            0.5f,
            MaximumProtectedRegionScale);
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
        return automatic with
        {
            Center = center,
            AxialRadius = axialRadius,
            LateralRadius = automatic.LateralRadius * adjustment.RadialScale,
            ForwardRadius = automatic.ForwardRadius * adjustment.RadialScale,
            ProximalTransitionLength = MathF.Min(
                automatic.ProximalTransitionLength * adjustment.AxialScale,
                axialRadius * 1.5f)
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

    private static bool TryCaptureSemanticHeadPlane(
        SemanticRegionCalibration calibration,
        GeneratedSkinningSeparationPlaneResolution plane,
        IReadOnlyList<GeometryComponent> bodyComponents,
        IReadOnlyList<GeometrySource> donorSources,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        out IReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment> assignments,
        out IReadOnlyList<TargetRigBodyVertexMembership> coreVertices,
        out string? failure)
    {
        GeometryVertex[] captured = bodyComponents
            .SelectMany(component => component.Vertices)
            .Distinct()
            .Where(vertex =>
                GeneratedSkinningSeparationPlaneMath.SignedDistance(
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                    plane) >= -PositionEpsilon)
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        int uniquePositionCount = captured
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .Count();
        if (uniquePositionCount < MinimumCapturedSemanticCorePositions)
        {
            assignments = new ReadOnlyDictionary<GeometryVertex,
                SemanticVertexAssignment>(
                new Dictionary<GeometryVertex, SemanticVertexAssignment>());
            coreVertices = [];
            failure =
                $"Semantic Head plane contains only {uniquePositionCount} unique " +
                "selected-body position(s) on its Head side; adjust its height " +
                "or angle before applying.";
            return false;
        }

        var mutable = new Dictionary<GeometryVertex,
            SemanticVertexAssignment>(captured.Length);
        foreach (GeometryVertex vertex in captured)
        {
            mutable.Add(vertex, new SemanticVertexAssignment(
                GeneratedSkinningSemanticRegion.Head,
                SemanticVertexZone.Core,
                calibration.AnchorSkeletonJointIndex,
                calibration.ProximalSkeletonJointIndex,
                AnchorWeight: 1));
        }
        assignments = new ReadOnlyDictionary<GeometryVertex,
            SemanticVertexAssignment>(mutable);
        coreVertices = BuildSemanticMembership(captured, donorSources);
        failure = null;
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
        ResolveHeadPlaneCompanionComponents(
            IDictionary<GeneratedSkinningSemanticRegion, SemanticRegionCandidate> candidates,
            IReadOnlyList<GeometryComponent> bodyComponents,
            SceneTopology topology,
            IReadOnlyList<Vector3[]> alignedPositionsByMesh,
            IReadOnlySet<int> manuallyAssignedComponentIndices,
            SeparationPlanePreparation separationPlanes)
    {
        var result = new Dictionary<int, SemanticComponentAssignment>();
        if (!candidates.TryGetValue(
                GeneratedSkinningSemanticRegion.Head,
                out SemanticRegionCandidate? headCandidate) ||
            !headCandidate.Resolution.IsApplied ||
            headCandidate.Resolution.SeparationPlane is not
                { IsEnabled: true, IsAvailable: true } plane)
        {
            return new ReadOnlyDictionary<int, SemanticComponentAssignment>(result);
        }

        if (!separationPlanes.ByKind.TryGetValue(
                GeneratedSkinningSeparationPlaneKind.Back,
                out GeneratedSkinningSeparationPlaneResolution? backPlane) ||
            !backPlane.IsEnabled ||
            !backPlane.IsAvailable)
        {
            return new ReadOnlyDictionary<int, SemanticComponentAssignment>(result);
        }

        HashSet<int> knownBodyComponentIndices = bodyComponents
            .Select(component => component.ComponentIndex)
            .ToHashSet();
        HashSet<GeometryVertex> headAssignedVertices = headCandidate.Assignments.Keys
            .ToHashSet();
        int headOwnerComponentIndex =
            headCandidate.Resolution.TopologyOwnerComponentIndex;
        if (headOwnerComponentIndex < 0)
        {
            headOwnerComponentIndex = topology.Components
                .Select(component => new
                {
                    component.ComponentIndex,
                    Captured = component.Vertices.Count(
                        headAssignedVertices.Contains),
                    component.TriangleCount
                })
                .Where(value => value.Captured > 0)
                .OrderByDescending(value => value.Captured)
                .ThenByDescending(value => value.TriangleCount)
                .ThenBy(value => value.ComponentIndex)
                .Select(value => value.ComponentIndex)
                .DefaultIfEmpty(-1)
                .First();
        }

        var headAssemblyProtectedComponentIndices = new HashSet<int>();
        var propagatedHeadComponentIndices = new HashSet<int>();
        var headAssemblyProtection = new List<(
            int MeshIndex,
            int SeedCount,
            int ProtectedCount,
            float MaximumGap)>();
        var rejectedHeadAssemblyProtection = new List<(
            int MeshIndex,
            int SeedCount,
            int ProposedCount,
            int ProposedVertexCount,
            int SurfaceVertexCount)>();
        int surfaceVertexCount = topology.Components.Sum(component =>
            component.Vertices.Length);

        // An imported mesh can contain many disconnected triangle islands.
        // Hair commonly does, so classifying and weighting each island alone
        // splits one authored assembly between Head and Back. Start from the
        // components that independently prove the strict Head rule, then add
        // only islands which directly neighbour one of those proved seeds and
        // remain inside the finite vicinity of the Head cut. Never flood-fill
        // through newly inherited islands: a whole character exported as one
        // technical OBJ mesh can contain a chain of nearby disconnected body,
        // clothing and hair surfaces, and transitive growth would eventually
        // classify almost the entire character as rigid Head.
        // A distant rear object (for example a wing packed into the same
        // technical mesh) cannot inherit Head merely through that container.
        // The semantic anatomical owner component and manual assignments are
        // never eligible. Names, materials and textures never participate.
        foreach (IGrouping<int, GeometryComponent> meshGroup in
                 topology.Components
                     .SelectMany(component => component.Vertices
                         .Select(vertex => vertex.MeshIndex)
                         .Distinct()
                         .Select(meshIndex => (meshIndex, component)))
                     .GroupBy(value => value.meshIndex, value => value.component)
                     .OrderBy(group => group.Key))
        {
            int meshIndex = meshGroup.Key;
            GeometryComponent[] meshComponents = meshGroup
                .DistinctBy(component => component.ComponentIndex)
                .OrderBy(component => component.ComponentIndex)
                .ToArray();
            GeometryComponent[] eligibleComponents = meshComponents
                .Where(component =>
                    component.ComponentIndex != headOwnerComponentIndex &&
                    !manuallyAssignedComponentIndices.Contains(
                        component.ComponentIndex))
                .ToArray();
            if (eligibleComponents.Length == 0)
                continue;

            GeometryComponent[] seeds = eligibleComponents
                .Where(component =>
                {
                    ComponentPlaneCoverage headCoverage =
                        MeasureComponentPlaneCoverage(
                            component,
                            alignedPositionsByMesh,
                            plane);
                    ComponentPlaneCoverage backCoverage =
                        MeasureComponentPlaneCoverage(
                            component,
                            alignedPositionsByMesh,
                            backPlane);
                    return headCoverage.PositionCount > 0 &&
                           backCoverage.PositionCount > 0 &&
                           IsProtectedHeadComponent(
                               headCoverage,
                               backCoverage);
                })
                .ToArray();
            if (seeds.Length == 0)
                continue;

            var geometryByComponent = eligibleComponents.ToDictionary(
                component => component.ComponentIndex,
                component =>
                {
                    Vector3[] positions = component.Vertices
                        .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                            [vertex.VertexIndex])
                        .Distinct()
                        .ToArray();
                    return (
                        Minimum: positions.Aggregate(Vector3.Min),
                        Maximum: positions.Aggregate(Vector3.Max),
                        MaximumHeadDistance: positions.Max(position =>
                            GeneratedSkinningSeparationPlaneMath.SignedDistance(
                                position,
                                plane)));
                });
            var seedIndices = seeds
                .Select(component => component.ComponentIndex)
                .ToHashSet();
            var protectedInMesh = seedIndices.ToHashSet();
            float maximumGap = MathF.Max(
                PositionEpsilon * 16,
                MathF.Min(
                    plane.PreviewHalfExtentU,
                    backPlane.PreviewHalfExtentV) *
                HeadAssemblyMaximumIslandGapRatio);
            foreach (GeometryComponent candidate in eligibleComponents)
            {
                if (protectedInMesh.Contains(candidate.ComponentIndex))
                    continue;
                (Vector3 candidateMinimum, Vector3 candidateMaximum,
                    float candidateMaximumHeadDistance) =
                    geometryByComponent[candidate.ComponentIndex];
                if (candidateMaximumHeadDistance < -maximumGap)
                    continue;
                bool neighboursDirectSeed = seedIndices.Any(seedIndex =>
                {
                    (Vector3 seedMinimum, Vector3 seedMaximum, _) =
                        geometryByComponent[seedIndex];
                    float dx = MathF.Max(
                        0,
                        MathF.Max(
                            candidateMinimum.X - seedMaximum.X,
                            seedMinimum.X - candidateMaximum.X));
                    float dy = MathF.Max(
                        0,
                        MathF.Max(
                            candidateMinimum.Y - seedMaximum.Y,
                            seedMinimum.Y - candidateMaximum.Y));
                    float dz = MathF.Max(
                        0,
                        MathF.Max(
                            candidateMinimum.Z - seedMaximum.Z,
                            seedMinimum.Z - candidateMaximum.Z));
                    return dx * dx + dy * dy + dz * dz <=
                           maximumGap * maximumGap;
                });
                if (neighboursDirectSeed)
                    protectedInMesh.Add(candidate.ComponentIndex);
            }

            int protectedVertexCount = meshComponents
                .Where(component => protectedInMesh.Contains(
                    component.ComponentIndex))
                .Sum(component => component.Vertices.Length);
            if (protectedInMesh.Count > seedIndices.Count &&
                surfaceVertexCount > 0 &&
                (double)protectedVertexCount / surfaceVertexCount >
                    HeadAssemblyMaximumProtectedVertexFraction)
            {
                rejectedHeadAssemblyProtection.Add((
                    meshIndex,
                    seeds.Length,
                    protectedInMesh.Count,
                    protectedVertexCount,
                    surfaceVertexCount));
                protectedInMesh = seedIndices;
            }

            headAssemblyProtectedComponentIndices.UnionWith(protectedInMesh);
            propagatedHeadComponentIndices.UnionWith(
                protectedInMesh.Except(
                    seeds.Select(component => component.ComponentIndex)));
            if (protectedInMesh.Count > seeds.Length)
            {
                headAssemblyProtection.Add((
                    meshIndex,
                    seeds.Length,
                    protectedInMesh.Count,
                    maximumGap));
            }
        }

        foreach (GeometryComponent component in topology.Components
                     .Where(component =>
                         component.ComponentIndex != headOwnerComponentIndex &&
                         !manuallyAssignedComponentIndices.Contains(
                             component.ComponentIndex))
                     .OrderBy(component => component.ComponentIndex))
        {
            bool isProtected = headAssemblyProtectedComponentIndices.Contains(
                component.ComponentIndex);
            if (!isProtected)
            {
                ComponentPlaneCoverage coverage = MeasureComponentPlaneCoverage(
                    component,
                    alignedPositionsByMesh,
                    plane);
                ComponentPlaneCoverage backCoverage = MeasureComponentPlaneCoverage(
                    component,
                    alignedPositionsByMesh,
                    backPlane);
                isProtected =
                    coverage.PositionCount > 0 &&
                    backCoverage.PositionCount > 0 &&
                    IsProtectedHeadComponent(coverage, backCoverage);
            }
            if (!isProtected)
                continue;

            result.Add(
                component.ComponentIndex,
                new SemanticComponentAssignment(
                    GeneratedSkinningSemanticRegion.Head,
                    headCandidate.Resolution.AnchorBoneName,
                    headCandidate.Resolution.AnchorSkeletonJointIndex));
        }

        int[] companionIndices = result.Keys.Order().ToArray();
        var diagnostics = headCandidate.Resolution.Warnings.ToList();
        if (companionIndices.Length > 0)
        {
            int selectedBodyCount = companionIndices.Count(
                knownBodyComponentIndices.Contains);
            int propagatedCount = companionIndices.Count(
                propagatedHeadComponentIndices.Contains);
            diagnostics.Add(
                $"The Head plane placed {companionIndices.Length} whole " +
                $"component(s) on rigid one-hot Head: " +
                $"{string.Join(", ", companionIndices.Select(index => $"#{index}"))}. " +
                $"{companionIndices.Length - propagatedCount} directly passed a " +
                $"strict majority above Head with at least one percent in front " +
                $"of Back; {propagatedCount} neighbouring island(s) inherited the " +
                $"same whole-mesh assembly ownership. " +
                $"{selectedBodyCount} remained inside the logical deform body and " +
                "all are rigid one-hot Head components; their geometry is not split.");
        }
        if (headAssemblyProtection.Count > 0)
        {
            diagnostics.Add(
                $"Before Back extraction, Head protection expanded through " +
                $"{headAssemblyProtection.Count} spatially coherent imported-mesh " +
                $"assembly/assemblies: " +
                string.Join(", ", headAssemblyProtection.Select(value =>
                    $"mesh #{value.MeshIndex} ({value.SeedCount} direct seed(s) -> " +
                    $"{value.ProtectedCount} protected components; maximum island " +
                    $"gap {value.MaximumGap:G6})")) +
                ". Distant rear components, the anatomical owner component and " +
                "manual assignments are excluded; no names were used.");
        }
        if (rejectedHeadAssemblyProtection.Count > 0)
        {
            diagnostics.Add(
                "Head whole-assembly propagation was rejected as pathological: " +
                string.Join(", ", rejectedHeadAssemblyProtection.Select(value =>
                    $"mesh #{value.MeshIndex} ({value.SeedCount} direct seed(s) " +
                    $"would expand to {value.ProposedCount} components and " +
                    $"{value.ProposedVertexCount}/{value.SurfaceVertexCount} " +
                    "surface vertices)")) +
                ". Only independently proved Head components were retained.");
        }
        if (companionIndices.Length > 0)
        {
            GeneratedSkinningRegionResolution resolution =
                headCandidate.Resolution with
                {
                    RigidCompanionComponentIndices =
                        Array.AsReadOnly(companionIndices),
                    TopologyOwnerComponentIndex = headOwnerComponentIndex,
                    Warnings = new ReadOnlyCollection<string>(
                        diagnostics.ToArray())
                };
            candidates[GeneratedSkinningSemanticRegion.Head] =
                headCandidate with { Resolution = resolution };
        }

        else if (headOwnerComponentIndex >= 0 &&
                 headCandidate.Resolution.TopologyOwnerComponentIndex !=
                    headOwnerComponentIndex)
        {
            candidates[GeneratedSkinningSemanticRegion.Head] =
                headCandidate with
                {
                    Resolution = headCandidate.Resolution with
                    {
                        TopologyOwnerComponentIndex = headOwnerComponentIndex
                    }
                };
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
            MotionBranchBoneNames = calibration?.HandMotionProfile?.Branches
                .Select(branch => branch.RootBoneName)
                .ToArray() ?? [],
            MaximumMotionProxyWeight =
                calibration?.HandMotionProfile is null ? 0 : 1,
            TopologyOwnerComponentIndex = -1
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
