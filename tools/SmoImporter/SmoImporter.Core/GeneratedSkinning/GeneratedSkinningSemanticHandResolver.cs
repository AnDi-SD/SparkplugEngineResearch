using System.Collections.ObjectModel;
using System.Numerics;

namespace SmoImporter.Core;

public static partial class GeneratedSkinningPreparer
{
    private const float MaximumCompleteHandSpanHeightRatio = 0.30f;
    private const float MaximumCompleteHandRadiusHeightRatio = 0.16f;
    private const float MaximumCompleteHandBoundaryRadiusRatio = 2.5f;
    private const float MaximumCoarseHandMotionWeight = 0.65f;
    private const int CoarseHandMotionKnotCount = 9;
    private const float MaximumWristTransitionSurfaceFraction = 0.25f;

    private sealed record SemanticHandLobe(
        int BodyComponentIndex,
        GeometryVertex Seed,
        IReadOnlySet<GeometryVertex> Vertices,
        IReadOnlySet<GeometryVertex> OwnerVertices,
        IReadOnlySet<GeometryVertex> BoundaryVertices,
        IReadOnlyDictionary<GeometryVertex, float> SurfaceDistanceByVertex,
        float CutPlaneProjection,
        float TransitionSurfaceDistance,
        float MaximumSurfaceDistance,
        float MotionStartProjection,
        float DistalProjection);

    private sealed record DonorHandSeedEvidence(
        string Source,
        IReadOnlySet<GeometryVertex> Vertices,
        IReadOnlySet<GeometryVertex> RequiredLobeVertices,
        IReadOnlyList<Vector3> UniquePositions,
        float LateralCenterShift,
        float ForwardCenterShift);

    private sealed record CoarseHandMotionProfile(
        IReadOnlyList<ApproximateFingerBranch> Branches,
        IReadOnlyList<float> ProgressKnots,
        IReadOnlyList<float> WeightKnots,
        float MaximumWeight)
    {
        public float Evaluate(float progress)
        {
            if (ProgressKnots.Count != WeightKnots.Count || ProgressKnots.Count < 2)
                throw new InvalidOperationException("A coarse hand profile is incomplete.");
            progress = Math.Clamp(progress, 0, 1);
            int upper = 1;
            while (upper < ProgressKnots.Count - 1 &&
                   progress > ProgressKnots[upper])
            {
                upper++;
            }
            int lower = upper - 1;
            float span = ProgressKnots[upper] - ProgressKnots[lower];
            float amount = span <= PositionEpsilon
                ? 1
                : Math.Clamp((progress - ProgressKnots[lower]) / span, 0, 1);
            amount = amount * amount * (3 - 2 * amount);
            return Math.Clamp(
                WeightKnots[lower] +
                (WeightKnots[upper] - WeightKnots[lower]) * amount,
                0,
                MaximumCoarseHandMotionWeight);
        }

        public float EvaluateArticulation(float progress)
        {
            if (MaximumWeight <= WeightEpsilon)
                return 0;
            float targetEnvelope = Math.Clamp(
                Evaluate(progress) / MaximumWeight, 0, 1);
            float longitudinal = Math.Clamp((progress - 0.25f) / 0.75f, 0, 1);
            longitudinal = longitudinal * longitudinal * (3 - 2 * longitudinal);
            return MathF.Min(targetEnvelope, longitudinal);
        }

        public ApproximateFingerBranch SelectBranch(
            Vector3 position,
            GeneratedSkinningRegionVolume volume,
            float donorLateralMinimum = -1,
            float donorLateralMaximum = 1)
        {
            if (Branches.Count == 0)
                throw new InvalidOperationException(
                    "An approximate Hand profile contains no finger branches.");
            Vector3 relative = position - volume.Center;
            float donorLateral = Vector3.Dot(relative, volume.LateralAxis);
            float donorLateralSpan = donorLateralMaximum - donorLateralMinimum;
            float lateral = donorLateralSpan > PositionEpsilon
                ? Math.Clamp(
                    (donorLateral - donorLateralMinimum) /
                    donorLateralSpan * 2 - 1,
                    -1,
                    1)
                : donorLateral / volume.LateralRadius;
            return Branches
                .OrderBy(branch =>
                {
                    float dl = lateral - branch.NormalizedLateral;
                    return dl * dl;
                })
                .ThenBy(branch => branch.RootRigJointIndex)
                .First();
        }
    }

    private sealed record ApproximateFingerBranch(
        string RootBoneName,
        int RootRigJointIndex,
        IReadOnlyList<int> SkeletonJointIndices,
        float NormalizedLateral);

    private sealed record TargetHandMotionSample(float Progress, float MotionWeight);

    private static CoarseHandMotionProfile? TryBuildCoarseHandMotionProfile(
        GeneratedSkinningSemanticRegion region,
        TargetRigDefinition rig,
        TargetSkeletonLayout layout,
        TargetRigJoint anchor,
        IReadOnlyList<DecodedTargetWeightSample> targetSamples,
        Vector3 bindAnchor,
        Vector3 bindAxial,
        Vector3 bindLateral,
        Vector3 bindForward,
        float proximalLower,
        float distalUpper,
        ICollection<string> messages)
    {
        if (region == GeneratedSkinningSemanticRegion.Head)
            return null;

        TargetRigJoint[] branchRoots = layout.DeformJoints
            .Where(joint => joint.JointIndex != anchor.JointIndex &&
                            FindNearestDeformParent(rig, joint.JointIndex) ==
                            anchor.JointIndex)
            .OrderBy(joint => joint.JointIndex)
            .ToArray();
        if (branchRoots.Length < 2)
        {
            messages.Add(
                $"Semantic {region} found only {branchRoots.Length} direct deform " +
                "branch(es) below Hand; the complete hand remains rigid.");
            return null;
        }

        var branchJoints = branchRoots.ToDictionary(
            root => root.JointIndex,
            root => CollectDeformSubtree(rig, layout, root.JointIndex));
        HashSet<int> allDescendants = branchJoints.Values
            .SelectMany(value => value)
            .ToHashSet();
        HashSet<int> completeHand = CollectDeformSubtree(
            rig, layout, anchor.JointIndex);

        var branchEvidence = new List<(
            TargetRigJoint Root,
            float Mass,
            Vector3 Center,
            int[] SkeletonChain,
            float Lateral,
            float Forward)>();
        foreach (TargetRigJoint root in branchRoots)
        {
            HashSet<int> branch = branchJoints[root.JointIndex];
            double mass = 0;
            Vector3 weighted = Vector3.Zero;
            foreach (DecodedTargetWeightSample sample in targetSamples)
            {
                float value = branch.Sum(joint =>
                    sample.WeightByRigJoint.GetValueOrDefault(joint)) /
                    sample.TotalWeight;
                if (!float.IsFinite(value) || value <= WeightEpsilon)
                    continue;
                mass += value;
                weighted += sample.Position * value;
            }
            if (mass > WeightEpsilon && double.IsFinite(mass))
            {
                var rigChain = new List<int> { root.JointIndex };
                int cursor = root.JointIndex;
                while (rigChain.Count < 3)
                {
                    TargetRigJoint[] children = layout.DeformJoints
                        .Where(joint => FindNearestDeformParent(
                            rig, joint.JointIndex) == cursor)
                        .OrderBy(joint => joint.JointIndex)
                        .ToArray();
                    if (children.Length != 1)
                        break;
                    cursor = children[0].JointIndex;
                    rigChain.Add(cursor);
                }
                int[] skeletonChain = rigChain
                    .Where(layout.SkeletonIndexByRigJoint.ContainsKey)
                    .Select(joint => layout.SkeletonIndexByRigJoint[joint])
                    .ToArray();
                if (skeletonChain.Length < 2)
                    continue;
                Vector3 rootOffset = Translation(root.BindWorldMatrix) - bindAnchor;
                branchEvidence.Add((
                    root,
                    (float)mass,
                    weighted / (float)mass,
                    skeletonChain,
                    Vector3.Dot(rootOffset, bindLateral),
                    Vector3.Dot(rootOffset, bindForward)));
            }
        }
        if (branchEvidence.Count < 2)
        {
            messages.Add(
                $"Semantic {region} target weights prove fewer than two active " +
                "deform branches below Hand; the complete hand remains rigid.");
            return null;
        }

        float lateralMinimum = branchEvidence.Min(value => value.Lateral);
        float lateralMaximum = branchEvidence.Max(value => value.Lateral);
        float lateralSpan = lateralMaximum - lateralMinimum;
        ApproximateFingerBranch[] approximateBranches = branchEvidence
            .OrderBy(value => value.Root.JointIndex)
            .Select(value => new ApproximateFingerBranch(
                value.Root.Name,
                value.Root.JointIndex,
                Array.AsReadOnly(value.SkeletonChain),
                NormalizeLane(value.Lateral, lateralMinimum, lateralSpan)))
            .ToArray();

        static float NormalizeLane(float value, float minimum, float span) =>
            span <= PositionEpsilon
                ? 0
                : Math.Clamp((value - minimum) / span * 2 - 1, -1, 1);

        float targetSpan = distalUpper - proximalLower;
        if (!float.IsFinite(targetSpan) || targetSpan <= PositionEpsilon)
            return null;
        var motionSamples = new List<TargetHandMotionSample>();
        foreach (DecodedTargetWeightSample sample in targetSamples)
        {
            float handMass = completeHand.Sum(joint =>
                sample.WeightByRigJoint.GetValueOrDefault(joint)) /
                sample.TotalWeight;
            if (!float.IsFinite(handMass) || handMass < SemanticTransitionWeightThreshold)
                continue;
            float descendantMass = allDescendants.Sum(joint =>
                sample.WeightByRigJoint.GetValueOrDefault(joint)) /
                sample.TotalWeight;
            if (!float.IsFinite(descendantMass) || descendantMass < 0)
                continue;
            float axial = Vector3.Dot(sample.Position - bindAnchor, bindAxial);
            float progress = Math.Clamp(
                (axial - proximalLower) / targetSpan,
                0,
                1);
            motionSamples.Add(new TargetHandMotionSample(
                progress,
                Math.Clamp(descendantMass, 0, 1)));
        }
        if (motionSamples.Count < MinimumSemanticCoreSamples)
        {
            messages.Add(
                $"Semantic {region} has only {motionSamples.Count} target samples " +
                "for coarse Hand motion; the complete hand remains rigid.");
            return null;
        }

        float[] progressKnots = Enumerable.Range(0, CoarseHandMotionKnotCount)
            .Select(index => index / (float)(CoarseHandMotionKnotCount - 1))
            .ToArray();
        var weightKnots = new float[progressKnots.Length];
        float halfWindow = 0.75f / (CoarseHandMotionKnotCount - 1);
        for (int index = 0; index < progressKnots.Length; index++)
        {
            float knot = progressKnots[index];
            TargetHandMotionSample[] local = motionSamples
                .Where(sample => MathF.Abs(sample.Progress - knot) <= halfWindow)
                .ToArray();
            if (local.Length == 0)
            {
                local = motionSamples
                    .OrderBy(sample => MathF.Abs(sample.Progress - knot))
                    .Take(Math.Min(8, motionSamples.Count))
                    .ToArray();
            }
            weightKnots[index] = local.Average(sample => sample.MotionWeight);
        }
        weightKnots[0] = 0;
        for (int index = 1; index < weightKnots.Length; index++)
        {
            weightKnots[index] = Math.Clamp(
                MathF.Max(weightKnots[index - 1], weightKnots[index]),
                0,
                MaximumCoarseHandMotionWeight);
        }
        float maximumWeight = weightKnots.Max();
        if (!float.IsFinite(maximumWeight) || maximumWeight <= 0.05f)
        {
            messages.Add(
                $"Semantic {region} target finger branches carry no meaningful " +
                "distal weight; the complete hand remains rigid.");
            return null;
        }

        messages.Add(
            $"Semantic {region} resolved {approximateBranches.Length} active " +
            $"target finger branch(es), each with 2-3 deform joints. Donor " +
            "vertices will use only approximate transverse lanes and detected " +
            "per-lane tip length; donor finger identity is never inferred.");
        return new CoarseHandMotionProfile(
            Array.AsReadOnly(approximateBranches),
            Array.AsReadOnly(progressKnots),
            Array.AsReadOnly(weightKnots),
            maximumWeight);
    }

    private static SemanticRegionCalibration?
        TryRefineCompleteHandCalibrationFromDonorTopology(
            SemanticRegionCalibration calibration,
            IReadOnlyList<GeometryComponent> bodyComponents,
            IReadOnlyList<Vector3[]> alignedPositionsByMesh,
            IReadOnlyDictionary<GeometryVertex, IReadOnlyList<GeometryVertex>> adjacency,
            SideCalibration sideCalibration,
            ImportedSkeleton skeleton,
            IReadOnlyDictionary<GeometryVertex, GeneratedVertexInfluences>
                capsuleInfluences,
            float targetHeight,
            ICollection<string> messages)
    {
        GeneratedSkinningRegionVolume targetVolume = calibration.AutomaticVolume;
        GeometryVertex[] bodyVertices = bodyComponents
            .SelectMany(component => component.Vertices)
            .Distinct()
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        float? originalAxialShift = TryResolveMinimumHandSeedCenterShift(
            calibration.Region,
            targetVolume,
            bodyVertices,
            alignedPositionsByMesh,
            sideCalibration);
        if (originalAxialShift.HasValue &&
            MathF.Abs(originalAxialShift.Value) <= PositionEpsilon)
        {
            return TryRefineCompleteHandCalibrationAtAxialCenter(
                calibration,
                bodyComponents,
                alignedPositionsByMesh,
                adjacency,
                sideCalibration,
                allowedSeedVertices: null,
                requiredLobeVertices: null,
                targetHeight,
                messages);
        }

        var capsuleEvidenceMessages = new List<string>();
        DonorHandSeedEvidence? evidence = TryBuildDonorHandCapsuleEvidence(
            calibration,
            skeleton,
            capsuleInfluences,
            alignedPositionsByMesh,
            sideCalibration,
            capsuleEvidenceMessages);
        if (evidence is not null)
        {
            foreach (string message in capsuleEvidenceMessages)
                messages.Add(message);
        }
        evidence ??= TryBuildDonorHandCorridorEvidence(
            calibration,
            bodyComponents,
            alignedPositionsByMesh,
            adjacency,
            sideCalibration,
            targetHeight,
            messages);
        if (evidence is null)
        {
            foreach (string message in capsuleEvidenceMessages)
                messages.Add(message);
            return TryRefineCompleteHandCalibrationAtAxialCenter(
                calibration,
                bodyComponents,
                alignedPositionsByMesh,
                adjacency,
                sideCalibration,
                allowedSeedVertices: null,
                requiredLobeVertices: null,
                targetHeight,
                messages);
        }

        float radialShiftLength = MathF.Sqrt(
            evidence.LateralCenterShift * evidence.LateralCenterShift +
            evidence.ForwardCenterShift * evidence.ForwardCenterShift);
        if (!float.IsFinite(radialShiftLength) ||
            radialShiftLength > targetHeight * 0.75f)
        {
            messages.Add(
                $"Semantic {calibration.Region} {evidence.Source} requires a " +
                $"non-finite or out-of-range transverse center shift " +
                $"{radialShiftLength:G6}; automatic Hand recentering was rejected.");
            return null;
        }

        GeneratedSkinningRegionVolume transverselyShiftedVolume = targetVolume with
        {
            Center = targetVolume.Center +
                     targetVolume.LateralAxis * evidence.LateralCenterShift +
                     targetVolume.ForwardAxis * evidence.ForwardCenterShift
        };
        float? axialCenterShift = TryResolveMinimumHandSeedCenterShift(
            calibration.Region,
            transverselyShiftedVolume,
            evidence.Vertices,
            alignedPositionsByMesh,
            sideCalibration);
        if (!axialCenterShift.HasValue)
        {
            messages.Add(
                $"Semantic {calibration.Region} found " +
                $"{evidence.Vertices.Count} {evidence.Source} record(s) at " +
                $"{evidence.UniquePositions.Count} unique position(s), but no " +
                "target-transition-bounded axial recenter produced a strict seed.");
            return null;
        }

        float axialShift = axialCenterShift.Value;
        GeneratedSkinningRegionVolume shiftedVolume =
            transverselyShiftedVolume with
            {
                Center = transverselyShiftedVolume.Center +
                         transverselyShiftedVolume.AxialAxis * axialShift
            };
        SemanticRegionCalibration shiftedCalibration = calibration with
        {
            AutomaticVolume = shiftedVolume
        };
        var shiftedMessages = new List<string>();
        SemanticRegionCalibration? refined =
            TryRefineCompleteHandCalibrationAtAxialCenter(
                shiftedCalibration,
                bodyComponents,
                alignedPositionsByMesh,
                adjacency,
                sideCalibration,
                evidence.Vertices,
                evidence.RequiredLobeVertices,
                targetHeight,
                shiftedMessages);
        if (refined is null)
        {
            messages.Add(
                $"Semantic {calibration.Region} used {evidence.Source}: " +
                $"{evidence.Vertices.Count} record(s)/" +
                $"{evidence.UniquePositions.Count} unique position(s) to derive " +
                $"center shifts axial={axialShift:G6}, " +
                $"lateral={evidence.LateralCenterShift:G6}, " +
                $"forward={evidence.ForwardCenterShift:G6}, but the complete " +
                "topology lobe failed an unchanged safety guard.");
            foreach (string message in shiftedMessages)
                messages.Add(message);
            return null;
        }

        string recenteringProof = evidence.RequiredLobeVertices.Count > 0
            ? $"must contain all {evidence.RequiredLobeVertices.Count} " +
              "trusted evidence record(s)"
            : "must contain every evidence record that becomes a strict " +
              "post-recenter seed";
        messages.Add(
            $"Semantic {calibration.Region} used {evidence.Source}: " +
            $"{evidence.Vertices.Count} record(s)/" +
            $"{evidence.UniquePositions.Count} unique position(s) only to " +
            $"recenter the automatic Hand volume: axial={axialShift:G6}, " +
            $"lateral={evidence.LateralCenterShift:G6}, " +
            $"forward={evidence.ForwardCenterShift:G6}. The axial shift is the " +
            "minimum strict-seed shift bounded by the target transition; final " +
            $"membership still comes solely from the complete topology lobe, " +
            $"which {recenteringProof}.");
        foreach (string message in shiftedMessages)
            messages.Add(message);
        return refined;
    }

    private static DonorHandSeedEvidence? TryBuildDonorHandCapsuleEvidence(
        SemanticRegionCalibration calibration,
        ImportedSkeleton skeleton,
        IReadOnlyDictionary<GeometryVertex, GeneratedVertexInfluences>
            capsuleInfluences,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        SideCalibration sideCalibration,
        ICollection<string> messages)
    {
        if (skeleton.ParentJointIndices is not { } parents ||
            (uint)calibration.AnchorSkeletonJointIndex >= (uint)parents.Count)
        {
            messages.Add(
                $"Semantic {calibration.Region} cannot derive target-capsule Hand " +
                "evidence because the deform skeleton hierarchy is unavailable.");
            return null;
        }

        var subtree = new HashSet<int>();
        for (int joint = 0; joint < parents.Count; joint++)
        {
            int cursor = joint;
            var visited = new HashSet<int>();
            while (cursor >= 0 && visited.Add(cursor))
            {
                if (cursor == calibration.AnchorSkeletonJointIndex)
                {
                    subtree.Add(joint);
                    break;
                }
                cursor = parents[cursor];
            }
        }

        BodySide requiredSide = calibration.Region ==
                                GeneratedSkinningSemanticRegion.LeftHand
            ? BodySide.Left
            : BodySide.Right;
        GeometryVertex[] evidenceVertices = capsuleInfluences
            .Where(pair => pair.Value.TopFourInfluences
                .Where(influence => subtree.Contains(influence.Joint))
                .Sum(influence => influence.Weight) > WeightEpsilon)
            .Select(pair => pair.Key)
            .Where(vertex =>
            {
                Vector3 position =
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
                return !IsOpposite(
                    requiredSide,
                    ClassifyPositionSide(position, sideCalibration));
            })
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        Vector3[] uniquePositions = evidenceVertices
            .Select(vertex =>
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex])
            .Distinct()
            .ToArray();
        if (uniquePositions.Length < MinimumCapturedSemanticCorePositions)
        {
            messages.Add(
                $"Semantic {calibration.Region} target-capsule comparison path " +
                $"found {evidenceVertices.Length} Hand-subtree record(s) at " +
                $"{uniquePositions.Length} unique position(s); donor-local " +
                "recentering requires a non-degenerate evidence cluster.");
            return null;
        }

        GeneratedSkinningRegionVolume volume = calibration.AutomaticVolume;
        float lateralShift = Quantile(
            Project(uniquePositions, volume.Center, volume.LateralAxis),
            0.5f);
        float forwardShift = Quantile(
            Project(uniquePositions, volume.Center, volume.ForwardAxis),
            0.5f);
        if (!float.IsFinite(lateralShift) || !float.IsFinite(forwardShift))
            return null;
        messages.Add(
            $"Semantic {calibration.Region} target-capsule comparison path " +
            $"localized {evidenceVertices.Length} Hand-subtree record(s) at " +
            $"{uniquePositions.Length} unique position(s).");
        return new DonorHandSeedEvidence(
            "target-capsule comparison evidence",
            new HashSet<GeometryVertex>(evidenceVertices),
            new HashSet<GeometryVertex>(evidenceVertices),
            Array.AsReadOnly(uniquePositions),
            lateralShift,
            forwardShift);
    }

    private static DonorHandSeedEvidence? TryBuildDonorHandCorridorEvidence(
        SemanticRegionCalibration calibration,
        IReadOnlyList<GeometryComponent> bodyComponents,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        IReadOnlyDictionary<GeometryVertex, IReadOnlyList<GeometryVertex>> adjacency,
        SideCalibration sideCalibration,
        float targetHeight,
        ICollection<string> messages)
    {
        GeneratedSkinningRegionVolume volume = calibration.AutomaticVolume;
        BodySide requiredSide = calibration.Region ==
                                GeneratedSkinningSemanticRegion.LeftHand
            ? BodySide.Left
            : BodySide.Right;
        float proximal = -volume.AxialRadius;
        float targetSpan = volume.AxialRadius * 2;
        float maximumSpan = MathF.Max(
            targetSpan,
            MathF.Min(
                targetHeight * MaximumCompleteHandSpanHeightRatio,
                targetSpan * 5f));
        float distal = proximal + maximumSpan;
        if (!float.IsFinite(distal))
            return null;

        var corridor = new HashSet<GeometryVertex>(bodyComponents
            .SelectMany(component => component.Vertices)
            .Distinct()
            .Where(vertex =>
            {
                Vector3 position =
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
                if (IsOpposite(
                        requiredSide,
                        ClassifyPositionSide(position, sideCalibration)))
                {
                    return false;
                }
                float axial = Vector3.Dot(
                    position - volume.Center,
                    volume.AxialAxis);
                return float.IsFinite(axial) &&
                       axial >= proximal - PositionEpsilon &&
                       axial <= distal + PositionEpsilon;
            }));

        var pending = new HashSet<GeometryVertex>(corridor);
        var clusters = new List<GeometryVertex[]>();
        while (pending.Count > 0)
        {
            GeometryVertex seed = pending
                .OrderBy(vertex => vertex.MeshIndex)
                .ThenBy(vertex => vertex.VertexIndex)
                .First();
            pending.Remove(seed);
            var cluster = new List<GeometryVertex>();
            var queue = new Queue<GeometryVertex>();
            queue.Enqueue(seed);
            while (queue.Count > 0)
            {
                GeometryVertex current = queue.Dequeue();
                cluster.Add(current);
                if (!adjacency.TryGetValue(
                        current,
                        out IReadOnlyList<GeometryVertex>? neighbours))
                {
                    continue;
                }
                foreach (GeometryVertex neighbour in neighbours)
                {
                    if (corridor.Contains(neighbour) && pending.Remove(neighbour))
                        queue.Enqueue(neighbour);
                }
            }
            if (cluster
                    .Select(vertex => alignedPositionsByMesh
                        [vertex.MeshIndex][vertex.VertexIndex])
                    .Distinct()
                    .Count() >= MinimumCapturedSemanticCorePositions)
            {
                clusters.Add(cluster
                    .OrderBy(vertex => vertex.MeshIndex)
                    .ThenBy(vertex => vertex.VertexIndex)
                    .ToArray());
            }
        }

        var candidates = clusters.Select(vertices =>
            {
                Vector3[] uniquePositions = vertices
                    .Select(vertex => alignedPositionsByMesh
                        [vertex.MeshIndex][vertex.VertexIndex])
                    .Distinct()
                    .ToArray();
                float lateralShift = Quantile(
                    Project(uniquePositions, volume.Center, volume.LateralAxis),
                    0.5f);
                float forwardShift = Quantile(
                    Project(uniquePositions, volume.Center, volume.ForwardAxis),
                    0.5f);
                float normalizedDistance = MathF.Sqrt(
                    MathF.Pow(lateralShift / volume.LateralRadius, 2) +
                    MathF.Pow(forwardShift / volume.ForwardRadius, 2));
                return (
                    Vertices: vertices,
                    UniquePositions: uniquePositions,
                    LateralShift: lateralShift,
                    ForwardShift: forwardShift,
                    NormalizedDistance: normalizedDistance);
            })
            .Where(value => float.IsFinite(value.NormalizedDistance))
            .OrderBy(value => value.NormalizedDistance)
            .ThenByDescending(value => value.UniquePositions.Length)
            .ThenBy(value => value.Vertices[0].MeshIndex)
            .ThenBy(value => value.Vertices[0].VertexIndex)
            .ToArray();
        if (candidates.Length == 0)
        {
            messages.Add(
                $"Semantic {calibration.Region} target-axis safety corridor " +
                $"contains {corridor.Count} record(s), but no connected cluster " +
                "has enough unique positions for donor-local Hand recentering.");
            return null;
        }
        float nearestDistance = candidates[0].NormalizedDistance;
        float coNearestSeparation = MathF.Max(
            0.25f,
            nearestDistance * 0.05f);
        var coNearest = candidates
            .Where(value =>
                value.NormalizedDistance - nearestDistance <=
                coNearestSeparation + PositionEpsilon)
            .ToArray();
        GeometryVertex[] selectedVertices = coNearest
            .SelectMany(value => value.Vertices)
            .Distinct()
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        Vector3[] selectedUniquePositions = selectedVertices
            .Select(vertex => alignedPositionsByMesh
                [vertex.MeshIndex][vertex.VertexIndex])
            .Distinct()
            .ToArray();
        float selectedLateralShift = Quantile(
            Project(selectedUniquePositions, volume.Center, volume.LateralAxis),
            0.5f);
        float selectedForwardShift = Quantile(
            Project(selectedUniquePositions, volume.Center, volume.ForwardAxis),
            0.5f);
        if (!float.IsFinite(selectedLateralShift) ||
            !float.IsFinite(selectedForwardShift))
        {
            return null;
        }

        messages.Add(
            $"Semantic {calibration.Region} target-axis safety corridor resolved " +
            $"{candidates.Length} non-degenerate topology cluster(s). Its " +
            $"{coNearest.Length} cluster(s) within normalized separation " +
            $"{coNearestSeparation:G6} of the nearest center contribute " +
            $"{selectedVertices.Length} record(s)/" +
            $"{selectedUniquePositions.Length} unique position(s) to seed evidence. " +
            "All co-nearest clusters therefore affect recentering; after " +
            "recentering, every strict seed must resolve to one final captured " +
            "lobe. Proximal corridor intersections are not Hand evidence.");
        return new DonorHandSeedEvidence(
            "target-axis topology-corridor evidence",
            new HashSet<GeometryVertex>(selectedVertices),
            new HashSet<GeometryVertex>(),
            Array.AsReadOnly(selectedUniquePositions),
            selectedLateralShift,
            selectedForwardShift);
    }

    private static float? TryResolveMinimumHandSeedCenterShift(
        GeneratedSkinningSemanticRegion region,
        GeneratedSkinningRegionVolume volume,
        IEnumerable<GeometryVertex> candidateVertices,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        SideCalibration sideCalibration)
    {
        BodySide requiredSide = region ==
                                GeneratedSkinningSemanticRegion.LeftHand
            ? BodySide.Left
            : BodySide.Right;
        float transitionEnd = -volume.AxialRadius +
                              volume.ProximalTransitionLength;
        float maximumProximalShift = volume.ProximalTransitionLength;
        float exponent = volume.ShapeExponent;
        float interiorMargin = PositionEpsilon * 16;
        if (!float.IsFinite(maximumProximalShift) ||
            maximumProximalShift <= interiorMargin ||
            !float.IsFinite(exponent) || exponent < 1)
        {
            return null;
        }

        float? bestShift = null;
        foreach (GeometryVertex vertex in candidateVertices
                     .Distinct()
                     .OrderBy(value => value.MeshIndex)
                     .ThenBy(value => value.VertexIndex))
        {
            Vector3 position =
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
            if (IsOpposite(
                    requiredSide,
                    ClassifyPositionSide(position, sideCalibration)))
            {
                continue;
            }

            Vector3 relative = position - volume.Center;
            float axial = Vector3.Dot(relative, volume.AxialAxis);
            float lateral = MathF.Abs(Vector3.Dot(
                relative,
                volume.LateralAxis) / volume.LateralRadius);
            float forward = MathF.Abs(Vector3.Dot(
                relative,
                volume.ForwardAxis) / volume.ForwardRadius);
            float radialPowered = MathF.Pow(lateral, exponent) +
                                  MathF.Pow(forward, exponent);
            if (!float.IsFinite(axial) || !float.IsFinite(radialPowered) ||
                radialPowered >= 1)
            {
                continue;
            }

            float axialLimit = volume.AxialRadius * MathF.Pow(
                MathF.Max(0, 1 - radialPowered),
                1 / exponent);
            float requiredAxial = MathF.Max(
                transitionEnd + interiorMargin,
                -axialLimit + interiorMargin);
            if (!float.IsFinite(axialLimit) ||
                requiredAxial > axialLimit - interiorMargin ||
                axial > axialLimit)
            {
                continue;
            }

            float shift = axial >= requiredAxial
                ? 0
                : axial - requiredAxial;
            if (shift < -maximumProximalShift - PositionEpsilon)
                continue;
            shift = Math.Clamp(shift, -maximumProximalShift, 0);
            if (!bestShift.HasValue || shift > bestShift.Value)
                bestShift = shift;
        }
        return bestShift;
    }

    private static SemanticRegionCalibration?
        TryRefineCompleteHandCalibrationAtAxialCenter(
            SemanticRegionCalibration calibration,
            IReadOnlyList<GeometryComponent> bodyComponents,
            IReadOnlyList<Vector3[]> alignedPositionsByMesh,
            IReadOnlyDictionary<GeometryVertex, IReadOnlyList<GeometryVertex>> adjacency,
            SideCalibration sideCalibration,
            IReadOnlySet<GeometryVertex>? allowedSeedVertices,
            IReadOnlySet<GeometryVertex>? requiredLobeVertices,
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
        float targetProximal = -targetVolume.AxialRadius;
        float targetTransitionEnd = targetProximal +
                                    targetVolume.ProximalTransitionLength;
        float targetSpan = targetVolume.AxialRadius * 2;
        float maximumSpan = MathF.Max(
            targetSpan,
            MathF.Min(
                targetHeight * MaximumCompleteHandSpanHeightRatio,
                targetSpan * 5f));
        if (!float.IsFinite(maximumSpan) || maximumSpan <= PositionEpsilon)
            return null;

        GeometryVertex[] bodyVertices = bodyComponents
            .SelectMany(component => component.Vertices)
            .Distinct()
            .ToArray();
        IReadOnlyDictionary<GeometryVertex, int> componentByVertex = bodyComponents
            .SelectMany(component => component.Vertices.Select(vertex =>
                (Vertex: vertex, component.ComponentIndex)))
            .ToDictionary(pair => pair.Vertex, pair => pair.ComponentIndex);
        float safetyDistal = targetProximal + maximumSpan;
        var distalHalfSpace = new HashSet<GeometryVertex>(bodyVertices.Where(vertex =>
        {
            Vector3 position = alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
            if (IsOpposite(
                    requiredSide,
                    ClassifyPositionSide(position, sideCalibration)))
                return false;
            float axial = Vector3.Dot(
                position - targetVolume.Center,
                targetVolume.AxialAxis);
            return float.IsFinite(axial) &&
                   axial >= targetProximal - PositionEpsilon &&
                   axial <= safetyDistal + PositionEpsilon;
        }));
        GeometryVertex[] seedCandidates = distalHalfSpace
            .Where(vertex => allowedSeedVertices is null ||
                             allowedSeedVertices.Contains(vertex))
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
                $"Semantic {calibration.Region} found no strict topology seed " +
                $"beyond the target-derived transition after donor-local " +
                $"recentering; the finite axial safety corridor contained " +
                $"{distalHalfSpace.Count} side-compatible record(s).");
            return null;
        }
        int[] ownerIndices = seedCandidates
            .Select(vertex => componentByVertex[vertex])
            .Distinct()
            .Order()
            .ToArray();
        if (ownerIndices.Length != 1)
        {
            messages.Add(
                $"Semantic {calibration.Region} found wrist seeds in " +
                $"{ownerIndices.Length} selected-body components; complete Hand " +
                "membership is ambiguous.");
            return null;
        }
        int ownerIndex = ownerIndices[0];
        HashSet<GeometryVertex> ownerVertices = bodyComponents
            .Single(component => component.ComponentIndex == ownerIndex)
            .Vertices
            .ToHashSet();
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
                if (ownerVertices.Contains(neighbour) &&
                    distalHalfSpace.Contains(neighbour) &&
                    captured.Add(neighbour))
                {
                    pending.Enqueue(neighbour);
                }
            }
        }
        GeometryVertex[] missedSeeds = seedCandidates
            .Where(vertex => !captured.Contains(vertex))
            .ToArray();
        if (missedSeeds.Length > 0)
        {
            messages.Add(
                $"Semantic {calibration.Region} has {missedSeeds.Length} plausible " +
                "wrist seed vertex/vertices outside one complete distal topology " +
                "lobe; partial Hand capture is forbidden.");
            return null;
        }
        GeometryVertex[] missedRequired = requiredLobeVertices is null
            ? []
            : requiredLobeVertices
                .Where(vertex => !captured.Contains(vertex))
                .OrderBy(vertex => vertex.MeshIndex)
                .ThenBy(vertex => vertex.VertexIndex)
                .ToArray();
        if (missedRequired.Length > 0)
        {
            messages.Add(
                $"Semantic {calibration.Region} complete topology lobe leaves " +
                $"{missedRequired.Length} required donor-local evidence " +
                "record(s) in another surface cluster; automatic Hand capture " +
                "failed closed.");
            return null;
        }

        var boundary = new HashSet<GeometryVertex>();
        var boundaryCrossSection = new List<Vector3>();
        foreach (GeometryVertex vertex in captured)
        {
            if (!adjacency.TryGetValue(
                    vertex, out IReadOnlyList<GeometryVertex>? neighbours))
                continue;
            foreach (GeometryVertex neighbour in neighbours)
            {
                if (!ownerVertices.Contains(neighbour) || captured.Contains(neighbour))
                    continue;
                Vector3 neighbourPosition =
                    alignedPositionsByMesh[neighbour.MeshIndex][neighbour.VertexIndex];
                float neighbourAxial = Vector3.Dot(
                    neighbourPosition - targetVolume.Center,
                    targetVolume.AxialAxis);
                if (neighbourAxial > safetyDistal + PositionEpsilon)
                {
                    messages.Add(
                        $"Semantic {calibration.Region} reaches the finite complete-Hand " +
                        "safety boundary; the region is rejected instead of clipping a tip.");
                    return null;
                }
                if (neighbourAxial >= targetProximal - PositionEpsilon)
                {
                    messages.Add(
                        $"Semantic {calibration.Region} has an unexplained internal " +
                        "topology boundary beyond the wrist cut.");
                    return null;
                }
                boundary.Add(vertex);
                Vector3 position =
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
                boundaryCrossSection.Add(position);
                boundaryCrossSection.Add(neighbourPosition);
            }
        }
        if (boundary.Count == 0)
        {
            messages.Add(
                $"Semantic {calibration.Region} has no proved topology boundary " +
                "at the target-derived wrist cut.");
            return null;
        }

        Vector3[] boundaryPositions = boundaryCrossSection
            .Distinct()
            .ToArray();
        if (boundaryPositions.Length == 0)
            return null;
        Vector3 wristPole = targetVolume.Center +
                            targetVolume.AxialAxis * targetProximal;
        float boundaryLateralCenter = Quantile(
            Project(boundaryPositions, wristPole, targetVolume.LateralAxis),
            0.5f);
        float boundaryForwardCenter = Quantile(
            Project(boundaryPositions, wristPole, targetVolume.ForwardAxis),
            0.5f);
        float boundaryCenterShift = MathF.Sqrt(
            boundaryLateralCenter * boundaryLateralCenter +
            boundaryForwardCenter * boundaryForwardCenter);
        float maximumBoundaryCenterShift = MathF.Min(
            targetHeight * MaximumCompleteHandRadiusHeightRatio,
            MathF.Max(
                targetVolume.LateralRadius,
                targetVolume.ForwardRadius) *
            MaximumDonorToTargetHandRadiusRatio);
        if (!float.IsFinite(boundaryCenterShift) ||
            !float.IsFinite(maximumBoundaryCenterShift) ||
            boundaryCenterShift > maximumBoundaryCenterShift)
        {
            messages.Add(
                $"Semantic {calibration.Region} robust wrist center is displaced " +
                $"{boundaryCenterShift:G6} from the donor-local seed line, beyond " +
                $"the conservative target/height bound " +
                $"{maximumBoundaryCenterShift:G6}.");
            return null;
        }
        Vector3 wristPlaneCenter = wristPole +
            targetVolume.LateralAxis * boundaryLateralCenter +
            targetVolume.ForwardAxis * boundaryForwardCenter;
        float maximumBoundaryRadius = boundaryPositions
            .Select(position =>
            {
                Vector3 relative = position - wristPlaneCenter;
                float lateral = Vector3.Dot(
                    relative,
                    targetVolume.LateralAxis) / targetVolume.LateralRadius;
                float forward = Vector3.Dot(
                    relative,
                    targetVolume.ForwardAxis) / targetVolume.ForwardRadius;
                return MathF.Sqrt(lateral * lateral + forward * forward);
            })
            .DefaultIfEmpty(float.PositiveInfinity)
            .Max();
        if (!float.IsFinite(maximumBoundaryRadius) ||
            maximumBoundaryRadius > MaximumCompleteHandBoundaryRadiusRatio)
        {
            messages.Add(
                $"Semantic {calibration.Region} crosses its wrist plane with a " +
                $"robust-centered normalized radius {maximumBoundaryRadius:G6}, " +
                $"beyond the conservative bound " +
                $"{MaximumCompleteHandBoundaryRadiusRatio:G6}.");
            return null;
        }

        var surfaceDistance = boundary.ToDictionary(vertex => vertex, _ => 0f);
        var distancePending = new PriorityQueue<GeometryVertex, float>();
        foreach (GeometryVertex vertex in boundary)
            distancePending.Enqueue(vertex, 0);
        while (distancePending.TryDequeue(
                   out GeometryVertex current, out float queuedDistance))
        {
            if (!surfaceDistance.TryGetValue(current, out float currentDistance) ||
                queuedDistance > currentDistance + PositionEpsilon)
                continue;
            if (!adjacency.TryGetValue(
                    current, out IReadOnlyList<GeometryVertex>? neighbours))
                continue;
            Vector3 currentPosition =
                alignedPositionsByMesh[current.MeshIndex][current.VertexIndex];
            foreach (GeometryVertex neighbour in neighbours)
            {
                if (!captured.Contains(neighbour))
                    continue;
                Vector3 neighbourPosition =
                    alignedPositionsByMesh[neighbour.MeshIndex][neighbour.VertexIndex];
                float candidate = currentDistance +
                                  Vector3.Distance(currentPosition, neighbourPosition);
                if (!float.IsFinite(candidate) ||
                    surfaceDistance.TryGetValue(neighbour, out float known) &&
                    known <= candidate + PositionEpsilon)
                    continue;
                surfaceDistance[neighbour] = candidate;
                distancePending.Enqueue(neighbour, candidate);
            }
        }
        if (surfaceDistance.Count != captured.Count)
        {
            messages.Add(
                $"Semantic {calibration.Region} complete lobe has vertices which " +
                "cannot be reached from its wrist boundary.");
            return null;
        }
        float maximumSurfaceDistance = surfaceDistance.Values.Max();
        // Target units alone are not a stable wrist boundary for donors with
        // very dense topology or several joined hand shells. Limit the blend
        // to the proximal quarter of the donor's actual geodesic hand span so
        // the palm/fingers cannot be swallowed almost entirely by the
        // UpperArm<->Hand transition.
        float transitionSurfaceDistance = MathF.Min(
            targetVolume.ProximalTransitionLength,
            maximumSurfaceDistance * MaximumWristTransitionSurfaceFraction);
        int transitionPositions = surfaceDistance
            .Where(pair => pair.Value <= transitionSurfaceDistance + PositionEpsilon)
            .Select(pair => alignedPositionsByMesh[pair.Key.MeshIndex]
                [pair.Key.VertexIndex])
            .Distinct()
            .Count();
        int corePositions = surfaceDistance
            .Where(pair => pair.Value > transitionSurfaceDistance + PositionEpsilon)
            .Select(pair => alignedPositionsByMesh[pair.Key.MeshIndex]
                [pair.Key.VertexIndex])
            .Distinct()
            .Count();
        if (transitionPositions == 0 ||
            corePositions < MinimumCapturedSemanticCorePositions ||
            maximumSurfaceDistance <= transitionSurfaceDistance + PositionEpsilon)
        {
            messages.Add(
                $"Semantic {calibration.Region} complete topology contains " +
                $"{corePositions} core and {transitionPositions} wrist-transition " +
                "position(s); an articulated hand needs both zones.");
            return null;
        }

        float[] coreMotionProjections = surfaceDistance
            .Where(pair => pair.Value > transitionSurfaceDistance + PositionEpsilon)
            .Select(pair => Vector3.Dot(
                alignedPositionsByMesh[pair.Key.MeshIndex][pair.Key.VertexIndex],
                targetVolume.AxialAxis))
            .Order()
            .ToArray();
        float motionStartProjection = coreMotionProjections[0];
        float distalProjection = coreMotionProjections[^1];
        if (!float.IsFinite(motionStartProjection) ||
            !float.IsFinite(distalProjection) ||
            distalProjection <= motionStartProjection + PositionEpsilon)
        {
            messages.Add(
                $"Semantic {calibration.Region} has no finite longitudinal span " +
                "for approximate finger articulation.");
            return null;
        }

        Vector3[] positions = captured
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex])
            .Distinct()
            .ToArray();
        float[] axial = Project(positions, targetVolume.Center, targetVolume.AxialAxis);
        float observedSpan = axial[^1] - axial[0];
        float axialMargin = MathF.Max(
            targetHeight * MinimumSemanticRadiusHeightRatio,
            observedSpan * 0.04f);
        float resolvedProximal = MathF.Min(targetProximal, axial[0] - axialMargin);
        float resolvedDistal = MathF.Max(
            targetVolume.AxialRadius,
            axial[^1] + axialMargin);
        float resolvedSpan = resolvedDistal - resolvedProximal;
        float maximumResolvedSpan = maximumSpan + axialMargin * 2;
        if (!float.IsFinite(resolvedSpan) || resolvedSpan <= PositionEpsilon ||
            !float.IsFinite(maximumResolvedSpan) ||
            resolvedSpan > maximumResolvedSpan)
        {
            messages.Add(
                $"Semantic {calibration.Region} complete lobe requires axial span " +
                $"{resolvedSpan:G6}, beyond the padded conservative bound " +
                $"{maximumResolvedSpan:G6}.");
            return null;
        }

        float[] lateralValues = Project(
            positions, targetVolume.Center, targetVolume.LateralAxis);
        float[] forwardValues = Project(
            positions, targetVolume.Center, targetVolume.ForwardAxis);
        float lateralOffset = (lateralValues[0] + lateralValues[^1]) * 0.5f;
        float forwardOffset = (forwardValues[0] + forwardValues[^1]) * 0.5f;
        float minimumRadius = MathF.Max(
            targetHeight * MinimumSemanticRadiusHeightRatio,
            PositionEpsilon * 16);
        float lateralRadius = MathF.Max(
            (lateralValues[^1] - lateralValues[0]) * 0.5f,
            minimumRadius);
        float forwardRadius = MathF.Max(
            (forwardValues[^1] - forwardValues[0]) * 0.5f,
            minimumRadius);
        float axialRadius = resolvedSpan * 0.5f;
        float axialCenter = (resolvedProximal + resolvedDistal) * 0.5f;
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
                return null;
            float lateralNormalized = MathF.Abs(Vector3.Dot(
                relative, targetVolume.LateralAxis) / lateralRadius);
            float forwardNormalized = MathF.Abs(Vector3.Dot(
                relative, targetVolume.ForwardAxis) / forwardRadius);
            float remaining = 1 - MathF.Pow(axialNormalized, 4);
            float radialPowered = MathF.Pow(lateralNormalized, 4) +
                                  MathF.Pow(forwardNormalized, 4);
            requiredRadialScale = MathF.Max(
                requiredRadialScale,
                MathF.Pow(
                    radialPowered / MathF.Max(remaining, PositionEpsilon),
                    0.25f));
        }
        lateralRadius *= requiredRadialScale * DonorHandEnvelopeMargin;
        forwardRadius *= requiredRadialScale * DonorHandEnvelopeMargin;
        float maximumRadius = MathF.Min(
            targetHeight * MaximumCompleteHandRadiusHeightRatio,
            MathF.Max(
                targetVolume.LateralRadius,
                targetVolume.ForwardRadius) *
            MaximumDonorToTargetHandRadiusRatio);
        if (!float.IsFinite(lateralRadius) || !float.IsFinite(forwardRadius) ||
            lateralRadius > maximumRadius || forwardRadius > maximumRadius)
        {
            messages.Add(
                $"Semantic {calibration.Region} complete lobe requires radii " +
                $"({lateralRadius:G6}, {forwardRadius:G6}), beyond the " +
                $"conservative bound {maximumRadius:G6}.");
            return null;
        }

        float[] boundaryProjection = boundary
            .Select(vertex => Vector3.Dot(
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                targetVolume.AxialAxis))
            .Order()
            .ToArray();
        var donorVolume = targetVolume with
        {
            Center = center,
            AxialRadius = axialRadius,
            LateralRadius = lateralRadius,
            ForwardRadius = forwardRadius,
            ProximalTransitionLength = MathF.Min(
                transitionSurfaceDistance,
                axialRadius * 1.5f),
            ShapeExponent = 4
        };
        messages.Add(
            $"Semantic {calibration.Region} donor topology captured the complete " +
            $"distal lobe across {positions.Length} unique position(s), including " +
            "all tips beyond the old target cap. The wrist boundary has normalized " +
            $"radius {maximumBoundaryRadius:G6}; its robust-center displacement " +
            $"{boundaryCenterShift:G6} remains within the finite " +
            $"{maximumBoundaryCenterShift:G6} target/height bound.");
        return calibration with
        {
            AutomaticVolume = donorVolume,
            DonorHandLobe = new SemanticHandLobe(
                ownerIndex,
                seed,
                captured,
                ownerVertices,
                boundary,
                new ReadOnlyDictionary<GeometryVertex, float>(surfaceDistance),
                Quantile(boundaryProjection, 0.5f),
                transitionSurfaceDistance,
                maximumSurfaceDistance,
                motionStartProjection,
                distalProjection)
        };
    }

    private static bool TryCaptureCompleteSemanticHandLobe(
        SemanticRegionCalibration calibration,
        GeneratedSkinningRegionVolume volume,
        IReadOnlyList<GeometrySource> donorSources,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        out IReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment> assignments,
        out IReadOnlyList<TargetRigBodyVertexMembership> coreVertices,
        out IReadOnlyList<TargetRigBodyVertexMembership> transitionVertices,
        out string? failure)
    {
        SemanticHandLobe? lobe = calibration.DonorHandLobe;
        if (lobe is null || lobe.Vertices.Count == 0)
        {
            assignments = EmptySemanticAssignments();
            coreVertices = [];
            transitionVertices = [];
            failure =
                $"Semantic {calibration.Region} has no complete donor Hand lobe.";
            return false;
        }
        GeometryVertex[] outside = lobe.Vertices
            .Where(vertex => DistanceToSemanticVolume(
                alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                volume) > 1 + PositionEpsilon * 16)
            .ToArray();
        if (outside.Length > 0)
        {
            assignments = EmptySemanticAssignments();
            coreVertices = [];
            transitionVertices = [];
            failure =
                $"Semantic {calibration.Region} adjusted volume would cut " +
                $"{outside.Length} vertex/vertices from the complete Hand lobe; " +
                "partial fingertips are forbidden.";
            return false;
        }
        GeometryVertex[] newlyEnclosedOwner = lobe.OwnerVertices
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
            .ToArray();
        if (newlyEnclosedOwner.Length > 0)
        {
            assignments = EmptySemanticAssignments();
            coreVertices = [];
            transitionVertices = [];
            failure =
                $"Semantic {calibration.Region} adjusted volume newly encloses " +
                $"{newlyEnclosedOwner.Length} owner-surface vertex/vertices beyond " +
                "the immutable wrist cut.";
            return false;
        }

        var mutable = new Dictionary<GeometryVertex, SemanticVertexAssignment>();
        var core = new List<GeometryVertex>();
        var transition = new List<GeometryVertex>();
        CoarseHandMotionProfile? motionProfile = calibration.HandMotionProfile;
        GeometryVertex[] motionVertices = lobe.Vertices
            .Where(vertex => lobe.SurfaceDistanceByVertex[vertex] >
                             lobe.TransitionSurfaceDistance + PositionEpsilon)
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        var branchByVertex = new Dictionary<GeometryVertex, ApproximateFingerBranch>();
        var distalByBranch = new Dictionary<int, float>();
        if (motionProfile is not null)
        {
            float[] donorLateral = motionVertices
                .Select(vertex => Vector3.Dot(
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex] -
                    volume.Center,
                    volume.LateralAxis))
                .Order()
                .ToArray();
            // Use the observed hand width, but ignore isolated lateral outliers.
            // Otherwise a single loose vertex can consume an outer lane and leave
            // the actual thumb or little-finger side almost entirely under Hand.
            float donorLateralMinimum = Quantile(
                donorLateral,
                RobustLowerQuantile);
            float donorLateralMaximum = Quantile(
                donorLateral,
                RobustUpperQuantile);
            foreach (GeometryVertex vertex in motionVertices)
            {
                Vector3 position =
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
                ApproximateFingerBranch branch = motionProfile.SelectBranch(
                    position,
                    volume,
                    donorLateralMinimum,
                    donorLateralMaximum);
                branchByVertex.Add(vertex, branch);
                float axial = Vector3.Dot(
                    position, calibration.AutomaticVolume.AxialAxis);
                distalByBranch[branch.RootRigJointIndex] = MathF.Max(
                    distalByBranch.GetValueOrDefault(
                        branch.RootRigJointIndex, float.NegativeInfinity),
                    axial);
            }
        }
        foreach (GeometryVertex vertex in lobe.Vertices)
        {
            float distance = lobe.SurfaceDistanceByVertex[vertex];
            if (distance <= lobe.TransitionSurfaceDistance + PositionEpsilon)
            {
                float amount = lobe.TransitionSurfaceDistance <= PositionEpsilon
                    ? 1
                    : Math.Clamp(
                        distance / lobe.TransitionSurfaceDistance,
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
                Vector3 position =
                    alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex];
                float axialProjection = Vector3.Dot(
                    position,
                    calibration.AutomaticVolume.AxialAxis);
                PackedInfluence[] motionInfluences = [];
                if (motionProfile is not null &&
                    branchByVertex.TryGetValue(
                        vertex, out ApproximateFingerBranch? branch) &&
                    distalByBranch.TryGetValue(
                        branch.RootRigJointIndex, out float laneDistal))
                {
                    float laneSpan = laneDistal - lobe.MotionStartProjection;
                    float progress = laneSpan <= PositionEpsilon
                        ? 1
                        : Math.Clamp(
                            (axialProjection - lobe.MotionStartProjection) /
                            laneSpan,
                            0,
                            1);
                    float articulation =
                        motionProfile.EvaluateArticulation(progress);
                    motionInfluences = BuildApproximateFingerInfluences(
                        calibration.AnchorSkeletonJointIndex,
                        branch.SkeletonJointIndices,
                        articulation);
                }
                core.Add(vertex);
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
        assignments = new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
            mutable);
        coreVertices = BuildSemanticMembership(core, donorSources);
        transitionVertices = BuildSemanticMembership(transition, donorSources);
        int articulated = mutable.Values.Count(value =>
            value.MotionProxyWeight > WeightEpsilon);
        failure = calibration.HandMotionProfile is null
            ? $"Semantic {calibration.Region} captured the complete Hand lobe, but " +
              "the target supplied no safe finger branches; its core remains rigid."
            : $"Semantic {calibration.Region} applies " +
              $"{calibration.HandMotionProfile.Branches.Count} approximate " +
              $"three-segment finger lanes to {articulated} distal " +
              "vertex/vertices using per-lane detected tips.";
        return true;
    }

    private static PackedInfluence[] BuildApproximateFingerInfluences(
        int anchorSkeletonJointIndex,
        IReadOnlyList<int> fingerChain,
        float articulation)
    {
        if (fingerChain.Count == 0 || !float.IsFinite(articulation) ||
            articulation <= WeightEpsilon)
        {
            return [];
        }
        articulation = Math.Clamp(articulation, 0, 1);
        int[] nodes = [anchorSkeletonJointIndex, .. fingerChain];
        float scaled = articulation * fingerChain.Count;
        int lower = Math.Min((int)MathF.Floor(scaled), fingerChain.Count);
        int upper = Math.Min(lower + 1, fingerChain.Count);
        float amount = Math.Clamp(scaled - lower, 0, 1);
        if (lower == upper || amount <= WeightEpsilon)
        {
            return
            [
                new PackedInfluence(checked((ushort)nodes[lower]), 1)
            ];
        }
        if (amount >= 1 - WeightEpsilon)
        {
            return
            [
                new PackedInfluence(checked((ushort)nodes[upper]), 1)
            ];
        }
        return
        [
            new PackedInfluence(checked((ushort)nodes[lower]), 1 - amount),
            new PackedInfluence(checked((ushort)nodes[upper]), amount)
        ];
    }

    private static IReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>
        EmptySemanticAssignments() =>
        new ReadOnlyDictionary<GeometryVertex, SemanticVertexAssignment>(
            new Dictionary<GeometryVertex, SemanticVertexAssignment>());
}
