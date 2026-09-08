using System.Numerics;
using System.Security.Cryptography;
using SmoImporter.Core;
using SmoViewer.Core;

/// <summary>
/// A deliberately bounded real-donor audit for the exact grouped humanoid pose
/// used while reproducing Daphne's face-shell failure. It performs one public
/// generated-skinning preparation and never invokes pose fitting, the SMO
/// writer, texture processing, animation decoding, or output creation.
/// </summary>
internal static class DaphneSemanticCoverageAudit
{
    private const int MaximumVertices = 10_000;
    private const int MaximumTriangles = 20_000;
    private const int MaximumComponents = 256;
    private const int MaximumSelectedBodyComponents = 4;
    // Fixed lower bounds measured from the reproduced Daphne donor at the
    // grouped UI pose. They keep the oracle from passing after a future Head
    // volume shrink: the old bilateral Spine_03 face patches are part of this
    // connected interior surface, not detached production secondary lobes.
    private const int MinimumHighConfidenceHeadVertices = 192;
    private const int MinimumHighConfidenceHeadUniquePositions = 138;
    private const float WeightEpsilon = 0.000001f;
    private const float WeightSumTolerance = 0.00001f;
    private const float HighConfidenceEnvelope = 0.98f;
    private const float StrictFaceWitnessAxialFloor = -0.5f;
    private const float StrictFaceWitnessEnvelope = 0.90f;
    private const float StrictFaceWitnessLateralDeadZone = 0.05f;

    private static readonly ReplacementTransform DaphneAlignment = new(
        83f,
        Vector3.Zero,
        new Vector3(0, 13.5f, -4));

    private static readonly TargetRigBodyPoseParameters DaphneGroupedPose = new(
        ArmElevationDegrees: -2.7f,
        ArmForwardDegrees: 7.4f,
        ElbowBendDegrees: 0,
        LegSpreadDegrees: -7.3f,
        KneeBendDegrees: 0,
        TorsoPitchDegrees: 1f,
        NeckForward: 0);

    private readonly record struct VertexKey(int MeshIndex, int VertexIndex);

    private sealed record HeadSeedCluster(
        int Index,
        IReadOnlyList<VertexKey> Vertices,
        int UniquePositionCount,
        Vector3 Center,
        Vector3 Minimum,
        Vector3 Maximum);

    private sealed record SelectedBodyTopology(
        IReadOnlySet<VertexKey> Vertices,
        IReadOnlyDictionary<VertexKey, HashSet<VertexKey>> Adjacency);

    private sealed record ObservedHandStats(
        int CoarseMotionVertexCount,
        float MaximumMotionProxyWeight,
        IReadOnlySet<string> ActiveCoreBones);

    private sealed record HeadFaceWitnessStats(
        int VertexCount,
        int PositiveSideVertexCount,
        int NegativeSideVertexCount);

    /// <summary>
    /// Entry point intended for a thin Program.cs command dispatch:
    /// <c>DaphneSemanticCoverageAudit.Run(targetSmo, donorObj)</c>.
    /// </summary>
    public static void Run(string targetArgument, string donorArgument)
    {
        string targetPath = RequireFile(targetArgument, ".smo", "target SMO");
        string donorPath = RequireFile(donorArgument, ".obj", "Daphne donor OBJ");
        string targetHash = HashFile(targetPath);
        string donorHash = HashFile(donorPath);

        SmoDocument target = SmoDocument.Load(targetPath);
        if (target.HasErrors)
            throw new InvalidDataException(
                "Daphne semantic-coverage target failed strict parsing.");
        ImportedScene donor = ImportedModelReader.ReadGeometryOnly(donorPath);
        AssertDonorBudget(donor);

        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
        TargetRigFittingPoseSnapshot pose = TargetRigBodyPoseMapper.CreatePose(
            rig,
            DaphneGroupedPose).Capture();
        if (pose.IsIdentityPose ||
            pose.RootRotation != Quaternion.Identity ||
            pose.RootTranslation != Vector3.Zero)
        {
            throw new InvalidOperationException(
                "Daphne semantic-coverage audit did not reproduce the expected " +
                "non-identity, local-rotation-only grouped pose.");
        }

        // SelectBody is a deterministic topology selection, not the iterative
        // pose fitter. Its immutable result is passed to the sole Prepare call.
        TargetRigBodySelection body = TargetRigAutomaticPoseFitter.SelectBody(
            rig,
            donor,
            DaphneAlignment);
        if (body.TotalComponentCount > MaximumComponents ||
            body.Components.Count is 0 or > MaximumSelectedBodyComponents)
        {
            throw new InvalidOperationException(
                $"Daphne semantic-coverage topology exceeds its fixed budget: " +
                $"components={body.TotalComponentCount}/{MaximumComponents}, " +
                $"selected={body.Components.Count}/{MaximumSelectedBodyComponents}.");
        }

        // Deliberately the only public Prepare call in this audit. Do not add a
        // disabled/identity comparison here: the real-donor command must remain
        // safe on modest developer machines.
        GeneratedSkinningPreparationResult preparation =
            GeneratedSkinningPreparer.Prepare(
                target,
                donor,
                pose,
                DaphneAlignment,
                body);
        if (preparation.Analysis.SemanticResolutionPassCount != 1 ||
            preparation.Analysis.InternalPreparationPassCount != 1)
        {
            throw new InvalidOperationException(
                "Daphne's single public Prepare repeated preparation or " +
                "semantic resolution.");
        }

        GeneratedSkinningRegionResolution head = preparation.Analysis
            .SemanticRegions.Regions.Single(value =>
                value.Region == GeneratedSkinningSemanticRegion.Head);
        HeadSeedCluster[] clusters = BuildHighConfidenceHeadSeedClusters(
            donor,
            body,
            head);
        WriteRegionDiagnostics(preparation, clusters);
        RequireAppliedRegion(head);
        Dictionary<int, GeneratedSkinningAttachment> attachmentsByComponent =
            preparation.Analysis.Attachments.ToDictionary(value => value.ComponentIndex);
        Console.WriteLine(
            "HEAD COMPANIONS: " + string.Join(
                " | ",
                head.RigidCompanionComponentIndices.Order().Select(componentIndex =>
                {
                    GeneratedSkinningAttachment attachment =
                        attachmentsByComponent[componentIndex];
                    Vector3[] positions = attachment.VerticesByMesh
                        .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                            preparation.FittingPreviewScene.Meshes[membership.MeshIndex]
                                .Positions[vertex]))
                        .ToArray();
                    Vector3 minimum = positions.Aggregate(Vector3.Min);
                    Vector3 maximum = positions.Aggregate(Vector3.Max);
                    Vector3 center = (minimum + maximum) * 0.5f;
                    return $"#{componentIndex}:{positions.Length}v " +
                           $"center=({center.X:G6},{center.Y:G6},{center.Z:G6}) " +
                           $"size=({(maximum.X - minimum.X):G6}," +
                           $"{(maximum.Y - minimum.Y):G6}," +
                           $"{(maximum.Z - minimum.Z):G6})";
                })));
        HeadFaceWitnessStats faceWitness = AssertStrictHeadCoverage(
            preparation,
            donor,
            body,
            head,
            clusters);

        string handDiagnostics = string.Join(
            "; ",
            new[]
            {
                GeneratedSkinningSemanticRegion.LeftHand,
                GeneratedSkinningSemanticRegion.RightHand
            }.Select(region => DescribeHand(preparation, region)));

        if (HashFile(targetPath) != targetHash || HashFile(donorPath) != donorHash)
        {
            throw new InvalidOperationException(
                "Daphne semantic-coverage audit mutated a target or donor input.");
        }

        int vertices = donor.Meshes.Sum(mesh => mesh.Positions.Length);
        int triangles = donor.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
        int highConfidenceVertices = clusters.Sum(cluster =>
            cluster.Vertices.Count);
        int highConfidencePositions = clusters.Sum(cluster =>
            cluster.UniquePositionCount);
        Console.WriteLine(
            $"DAPHNE POSED SEMANTIC COVERAGE PASS: {vertices} vertices/" +
            $"{triangles} triangles; pose=(-2.7,7.4,0,-7.3,0,1,0); " +
            $"alignment=(83,0,13.5,-4); high-confidence Head interior=" +
            $"{highConfidenceVertices} vertices/{highConfidencePositions} " +
            $"unique positions in {clusters.Length} topology island(s); " +
            $"strict bilateral face witness={faceWitness.VertexCount} " +
            $"vertices (+{faceWitness.PositiveSideVertexCount}/" +
            $"-{faceWitness.NegativeSideVertexCount}); " +
            $"Head core={Count(head.CoreVerticesByMesh)}, transition=" +
            $"{Count(head.TransitionVerticesByMesh)}; {handDiagnostics}; " +
            "exactly one public Prepare/one semantic resolve; " +
            "no Fit/writer/texture/SAN/output.");
        Console.WriteLine(
            "HEAD CLUSTERS: " + string.Join(
                " | ",
                clusters.Select(cluster =>
                    $"#{cluster.Index}:{cluster.Vertices.Count}v/" +
                    $"{cluster.UniquePositionCount}p center=" +
                    $"({cluster.Center.X:G6},{cluster.Center.Y:G6}," +
                    $"{cluster.Center.Z:G6})")));
    }

    private static HeadSeedCluster[] BuildHighConfidenceHeadSeedClusters(
        ImportedScene donor,
        TargetRigBodySelection body,
        GeneratedSkinningRegionResolution head)
    {
        GeneratedSkinningRegionVolume volume = head.ResolvedVolume ??
            throw new InvalidOperationException(
                "Applied Head has no resolved fitting-space volume.");
        SelectedBodyTopology topology = BuildSelectedBodyTopology(donor, body);
        IReadOnlySet<VertexKey> selected = topology.Vertices;
        IReadOnlyDictionary<VertexKey, HashSet<VertexKey>> adjacency =
            topology.Adjacency;

        float transitionEnd = -volume.AxialRadius +
                              volume.ProximalTransitionLength;
        float axialMargin = MathF.Max(volume.AxialRadius * 0.01f, 0.00001f);
        HashSet<VertexKey> highConfidence = selected.Where(vertex =>
        {
            Vector3 position = Vector3.Transform(
                Position(donor, vertex),
                DaphneAlignment.Matrix);
            Vector3 relative = position - volume.Center;
            float axial = Vector3.Dot(relative, volume.AxialAxis);
            float distance = DistanceToVolume(position, volume);
            return float.IsFinite(axial) && float.IsFinite(distance) &&
                   axial > transitionEnd + axialMargin &&
                   distance <= HighConfidenceEnvelope;
        }).ToHashSet();
        if (highConfidence.Count == 0)
        {
            throw new InvalidOperationException(
                "Daphne semantic-coverage oracle found no high-confidence Head " +
                "seed vertices in the reproduced grouped pose.");
        }

        var pending = new HashSet<VertexKey>(highConfidence);
        var rawClusters = new List<VertexKey[]>();
        while (pending.Count > 0)
        {
            VertexKey seed = pending
                .OrderBy(vertex => vertex.MeshIndex)
                .ThenBy(vertex => vertex.VertexIndex)
                .First();
            pending.Remove(seed);
            var vertices = new List<VertexKey>();
            var queue = new Queue<VertexKey>();
            queue.Enqueue(seed);
            while (queue.Count > 0)
            {
                VertexKey current = queue.Dequeue();
                vertices.Add(current);
                foreach (VertexKey neighbour in adjacency[current])
                {
                    if (highConfidence.Contains(neighbour) &&
                        pending.Remove(neighbour))
                    {
                        queue.Enqueue(neighbour);
                    }
                }
            }
            rawClusters.Add(vertices.ToArray());
        }

        HeadSeedCluster[] clusters = rawClusters.Select(vertices =>
            {
                Vector3[] positions = vertices
                    .Select(vertex => Vector3.Transform(
                        Position(donor, vertex),
                        DaphneAlignment.Matrix))
                    .Distinct()
                    .ToArray();
                Vector3 minimum = positions.Aggregate(Vector3.Min);
                Vector3 maximum = positions.Aggregate(Vector3.Max);
                Vector3 center = positions.Aggregate(Vector3.Zero,
                    (sum, position) => sum + position) / positions.Length;
                return new HeadSeedCluster(
                    Index: 0,
                    Array.AsReadOnly(vertices),
                    positions.Length,
                    center,
                    minimum,
                    maximum);
            })
            .OrderByDescending(cluster => cluster.UniquePositionCount)
            .ThenBy(cluster => cluster.Center.X)
            .ThenBy(cluster => cluster.Center.Y)
            .ThenBy(cluster => cluster.Center.Z)
            .Select((cluster, index) => cluster with { Index = index })
            .ToArray();
        int coveredVertices = clusters.Sum(cluster => cluster.Vertices.Count);
        int coveredUniquePositions = clusters.Sum(cluster =>
            cluster.UniquePositionCount);
        if (coveredVertices != highConfidence.Count ||
            coveredVertices < MinimumHighConfidenceHeadVertices ||
            coveredUniquePositions < MinimumHighConfidenceHeadUniquePositions)
        {
            throw new InvalidOperationException(
                $"Daphne high-confidence Head interior coverage regressed: " +
                $"vertices={coveredVertices}/" +
                $"{MinimumHighConfidenceHeadVertices}, unique positions=" +
                $"{coveredUniquePositions}/" +
                $"{MinimumHighConfidenceHeadUniquePositions}. The reproduced " +
                "bilateral legacy face patches must remain inside the strict " +
                "coverage oracle even though they are not detached topology " +
                "clusters.");
        }
        return clusters;
    }

    private static HeadFaceWitnessStats AssertStrictHeadCoverage(
        GeneratedSkinningPreparationResult preparation,
        ImportedScene donor,
        TargetRigBodySelection body,
        GeneratedSkinningRegionResolution head,
        IReadOnlyList<HeadSeedCluster> clusters)
    {
        GeneratedSkinningRegionVolume volume = head.ResolvedVolume ??
            throw new InvalidOperationException(
                "Applied Head has no resolved fitting-space volume.");
        HashSet<VertexKey> core = Membership(head.CoreVerticesByMesh);
        HashSet<VertexKey> transition = Membership(head.TransitionVerticesByMesh);
        SelectedBodyTopology topology = BuildSelectedBodyTopology(donor, body);
        if (core.Count == 0 || transition.Count == 0 ||
            core.Overlaps(transition) ||
            core.Any(vertex => !topology.Vertices.Contains(vertex)) ||
            transition.Any(vertex => !topology.Vertices.Contains(vertex)))
        {
            throw new InvalidOperationException(
                "Daphne protected Head and its exterior transition collar are " +
                "empty, overlapping, or outside the selected body topology.");
        }
        VertexKey[] highConfidence = clusters
            .SelectMany(cluster => cluster.Vertices)
            .Distinct()
            .ToArray();
        VertexKey[] missing = highConfidence
            .Where(vertex => !core.Contains(vertex))
            .Take(8)
            .ToArray();
        if (missing.Length > 0)
        {
            throw new InvalidOperationException(
                "Daphne high-confidence Head surface escaped the exact protected " +
                "one-hot Head core: " +
                string.Join(", ", missing.Select(Describe)) + ".");
        }

        // Core is the complete proved protected lobe, not merely a diagnostic
        // subset. Every raw skin slot must therefore be exact one-hot Head in
        // both representations; grouped bone-name checks would miss a hidden
        // non-zero capsule slot or a duplicate Head slot.
        foreach (VertexKey vertex in core)
        {
            AssertHeadInfluences(
                preparation.FittingPreviewScene,
                vertex,
                requireOneHotHead: true,
                "Protected Head core fitting");
            AssertHeadInfluences(
                preparation.PreparedScene,
                vertex,
                requireOneHotHead: true,
                "Protected Head core canonical");
        }

        int mixedTransitionVertices = 0;
        foreach (VertexKey vertex in transition)
        {
            if (ActiveInfluences(
                    preparation.PreparedScene,
                    vertex).Length == 2)
            {
                mixedTransitionVertices++;
            }
            AssertHeadTransitionInfluences(
                preparation.FittingPreviewScene,
                vertex,
                head,
                "External Head collar fitting");
            AssertHeadTransitionInfluences(
                preparation.PreparedScene,
                vertex,
                head,
                "External Head collar canonical");
        }
        if (mixedTransitionVertices == 0)
        {
            throw new InvalidOperationException(
                "Daphne external Head collar contains no genuine Head+Neck " +
                "blend vertex.");
        }

        VertexKey[] transitionSeeds = transition
            .Where(vertex => topology.Adjacency[vertex].Any(core.Contains))
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .ToArray();
        VertexKey[] unguardedCoreBoundary = core
            .Where(vertex => topology.Adjacency[vertex].Any(neighbour =>
                !core.Contains(neighbour)))
            .Where(vertex => !topology.Adjacency[vertex].Any(
                transition.Contains))
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .Take(8)
            .ToArray();
        if (transitionSeeds.Length == 0 ||
            unguardedCoreBoundary.Length != 0 ||
            transition.Overlaps(highConfidence))
        {
            throw new InvalidOperationException(
                "Daphne Head transition is not a disjoint exterior topology " +
                "collar between the protected lobe and the capsule path: seeds=" +
                $"{transitionSeeds.Length}, unguarded=" +
                string.Join(", ", unguardedCoreBoundary.Select(Describe)) + ".");
        }
        var unreachableTransition = transition.ToHashSet();
        var queue = new Queue<VertexKey>();
        foreach (VertexKey seed in transitionSeeds)
        {
            if (unreachableTransition.Remove(seed))
                queue.Enqueue(seed);
        }
        while (queue.Count > 0)
        {
            VertexKey current = queue.Dequeue();
            foreach (VertexKey neighbour in topology.Adjacency[current])
            {
                if (unreachableTransition.Remove(neighbour))
                    queue.Enqueue(neighbour);
            }
        }
        if (unreachableTransition.Count != 0)
        {
            throw new InvalidOperationException(
                "Daphne Head transition contains a disconnected non-collar " +
                "island: " + string.Join(", ", unreachableTransition
                    .OrderBy(vertex => vertex.MeshIndex)
                    .ThenBy(vertex => vertex.VertexIndex)
                    .Take(8)
                    .Select(Describe)) + ".");
        }

        // A stricter spatial witness is independent of production membership:
        // it takes the distal/interior part of the already non-vacuous surface
        // using only normalized target-derived axes. The lateral evidence must
        // be bilateral, and every witness vertex must be exact one-hot Head.
        VertexKey[] strictFaceWitness = highConfidence.Where(vertex =>
        {
            (float axial, _, _, float distance) =
                NormalizedHeadCoordinates(donor, vertex, volume);
            return axial >= StrictFaceWitnessAxialFloor &&
                   distance <= StrictFaceWitnessEnvelope;
        }).ToArray();
        VertexKey[] positiveSide = strictFaceWitness.Where(vertex =>
            NormalizedHeadCoordinates(donor, vertex, volume).Lateral >=
            StrictFaceWitnessLateralDeadZone).ToArray();
        VertexKey[] negativeSide = strictFaceWitness.Where(vertex =>
            NormalizedHeadCoordinates(donor, vertex, volume).Lateral <=
            -StrictFaceWitnessLateralDeadZone).ToArray();
        if (strictFaceWitness.Length == 0 ||
            positiveSide.Length == 0 ||
            negativeSide.Length == 0)
        {
            throw new InvalidOperationException(
                "Daphne strict Head face witness is empty or not bilateral: " +
                $"vertices={strictFaceWitness.Length}, lateral+=" +
                $"{positiveSide.Length}, lateral-={negativeSide.Length}.");
        }
        VertexKey[] witnessOutsideCore = strictFaceWitness
            .Where(vertex => !core.Contains(vertex))
            .Take(8)
            .ToArray();
        if (witnessOutsideCore.Length > 0)
        {
            throw new InvalidOperationException(
                "Daphne strict bilateral face witness escaped the protected " +
                "one-hot Head core: " +
                string.Join(", ", witnessOutsideCore.Select(Describe)) + ".");
        }

        foreach (VertexKey vertex in strictFaceWitness)
        {
            AssertHeadInfluences(
                preparation.FittingPreviewScene,
                vertex,
                requireOneHotHead: true,
                "Strict bilateral Head face witness fitting");
            AssertHeadInfluences(
                preparation.PreparedScene,
                vertex,
                requireOneHotHead: true,
                "Strict bilateral Head face witness canonical");
        }

        float distanceTolerance = MathF.Max(
            0.00001f,
            MathF.Max(
                volume.AxialRadius,
                MathF.Max(volume.LateralRadius, volume.ForwardRadius)) *
            0.00001f);
        AssertTopologyEdgeLengthsPreserved(
            preparation.FittingPreviewScene,
            preparation.PreparedScene,
            core,
            distanceTolerance,
            "Daphne protected Head");
        VertexKey[] pairwiseWitness = TakeEvenly(
            highConfidence
                .OrderBy(vertex => vertex.MeshIndex)
                .ThenBy(vertex => vertex.VertexIndex)
                .ToArray(),
            maximumCount: 32);
        AssertPairwiseDistancesPreserved(
            preparation.FittingPreviewScene,
            preparation.PreparedScene,
            pairwiseWitness,
            distanceTolerance,
            "Daphne protected Head witness");
        AssertRigidHeadCompanions(
            preparation,
            head,
            core,
            distanceTolerance);
        return new HeadFaceWitnessStats(
            strictFaceWitness.Length,
            positiveSide.Length,
            negativeSide.Length);
    }

    private static (float Axial, float Lateral, float Forward, float Distance)
        NormalizedHeadCoordinates(
            ImportedScene donor,
            VertexKey vertex,
            GeneratedSkinningRegionVolume volume)
    {
        Vector3 position = Vector3.Transform(
            Position(donor, vertex),
            DaphneAlignment.Matrix);
        Vector3 relative = position - volume.Center;
        return (
            Vector3.Dot(relative, volume.AxialAxis) / volume.AxialRadius,
            Vector3.Dot(relative, volume.LateralAxis) / volume.LateralRadius,
            Vector3.Dot(relative, volume.ForwardAxis) / volume.ForwardRadius,
            DistanceToVolume(position, volume));
    }

    private static void AssertHeadInfluences(
        ImportedScene scene,
        VertexKey vertex,
        bool requireOneHotHead,
        string context)
    {
        ImportedSkinning skinning = scene.Meshes[vertex.MeshIndex].Skinning ??
            throw new InvalidOperationException(
                $"{context} {Describe(vertex)} has no target skinning.");
        ImportedJointIndices rawJoints =
            skinning.JointIndices[vertex.VertexIndex];
        Vector4 rawWeights = skinning.Weights[vertex.VertexIndex];
        if (requireOneHotHead &&
            (rawWeights != Vector4.UnitX ||
             rawJoints.Y != 0 || rawJoints.Z != 0 || rawJoints.W != 0 ||
             rawJoints.X >= skinning.Skeleton.JointNames.Count ||
             skinning.Skeleton.JointNames[rawJoints.X] != "Head"))
        {
            throw new InvalidOperationException(
                $"{context} {Describe(vertex)} is not exact raw one-hot Head; " +
                "a transition or capsule slot leaked into the protected " +
                "region.");
        }
        (string Bone, float Weight)[] influences = ActiveInfluences(scene, vertex);
        float total = influences.Sum(value => value.Weight);
        if (!float.IsFinite(total) || MathF.Abs(total - 1) > WeightSumTolerance)
        {
            throw new InvalidOperationException(
                $"{context} {Describe(vertex)} has weight sum {total:G9}.");
        }
        string[] forbidden = influences
            .Where(value => value.Bone is not "Head" and not "Neck")
            .Select(value => value.Bone)
            .Distinct(StringComparer.Ordinal)
            .Order(StringComparer.Ordinal)
            .ToArray();
        if (forbidden.Length > 0)
        {
            throw new InvalidOperationException(
                $"{context} {Describe(vertex)} retained forbidden legacy " +
                $"influence(s): {string.Join(", ", forbidden)}.");
        }
        if (requireOneHotHead &&
            (influences.Length != 1 ||
             influences[0].Bone != "Head" ||
             MathF.Abs(influences[0].Weight - 1) > WeightSumTolerance))
        {
            throw new InvalidOperationException(
                $"{context} {Describe(vertex)} is not " +
                $"one-hot Head: {Describe(influences)}.");
        }
    }

    private static void AssertHeadTransitionInfluences(
        ImportedScene scene,
        VertexKey vertex,
        GeneratedSkinningRegionResolution head,
        string context)
    {
        (string Bone, float Weight)[] influences = ActiveInfluences(scene, vertex);
        AssertNormalizedWeights(influences, vertex, context);
        if (influences.Length is < 1 or > 2 ||
            influences.Any(value =>
                value.Bone != head.AnchorBoneName &&
                value.Bone != head.ProximalBoneName))
        {
            throw new InvalidOperationException(
                $"{context} {Describe(vertex)} is not a strict external " +
                $"{head.AnchorBoneName}+{head.ProximalBoneName} collar weight: " +
                Describe(influences) + ".");
        }
    }

    private static void AssertRigidHeadCompanions(
        GeneratedSkinningPreparationResult preparation,
        GeneratedSkinningRegionResolution head,
        IReadOnlySet<VertexKey> protectedCore,
        float tolerance)
    {
        int[] componentIndices = head.RigidCompanionComponentIndices
            .Distinct()
            .Order()
            .ToArray();
        if (componentIndices.Length == 0)
        {
            throw new InvalidOperationException(
                "Daphne Head resolved no detached rigid face companion.");
        }
        Dictionary<int, GeneratedSkinningAttachment> attachments = preparation
            .Analysis.Attachments.ToDictionary(value => value.ComponentIndex);
        var companionVertices = new HashSet<VertexKey>();
        foreach (int componentIndex in componentIndices)
        {
            if (!attachments.TryGetValue(
                    componentIndex,
                    out GeneratedSkinningAttachment? attachment) ||
                attachment.SemanticAssignment !=
                    GeneratedSkinningSemanticRegion.Head ||
                attachment.ManualAssignment is not null ||
                attachment.TargetBoneName != head.AnchorBoneName ||
                attachment.TargetSkeletonJointIndex !=
                    head.AnchorSkeletonJointIndex)
            {
                throw new InvalidOperationException(
                    $"Daphne detached component #{componentIndex} is not an " +
                    "automatic semantic Head companion.");
            }
            foreach (TargetRigBodyVertexMembership membership in
                     attachment.VerticesByMesh)
            foreach (int vertexIndex in membership.VertexIndices)
            {
                var vertex = new VertexKey(membership.MeshIndex, vertexIndex);
                if (!companionVertices.Add(vertex))
                {
                    throw new InvalidOperationException(
                        $"Daphne Head companion #{componentIndex} overlaps " +
                        $"another rigid companion at {Describe(vertex)}.");
                }
                AssertHeadInfluences(
                    preparation.FittingPreviewScene,
                    vertex,
                    requireOneHotHead: true,
                    $"Daphne Head companion #{componentIndex} fitting");
                AssertHeadInfluences(
                    preparation.PreparedScene,
                    vertex,
                    requireOneHotHead: true,
                    $"Daphne Head companion #{componentIndex} canonical");
            }
        }
        AssertTopologyEdgeLengthsPreserved(
            preparation.FittingPreviewScene,
            preparation.PreparedScene,
            companionVertices,
            tolerance,
            "Daphne rigid Head companions");

        VertexKey anchor = protectedCore
            .OrderBy(vertex => vertex.MeshIndex)
            .ThenBy(vertex => vertex.VertexIndex)
            .First();
        foreach (VertexKey companion in companionVertices)
        {
            AssertDistancePreserved(
                preparation.FittingPreviewScene,
                preparation.PreparedScene,
                anchor,
                companion,
                tolerance,
                $"Daphne Head companion {Describe(companion)} to protected " +
                $"anchor {Describe(anchor)}");
        }
    }

    private static void AssertTopologyEdgeLengthsPreserved(
        ImportedScene fitting,
        ImportedScene canonical,
        IReadOnlySet<VertexKey> vertices,
        float tolerance,
        string context)
    {
        var edges = new HashSet<(VertexKey First, VertexKey Second)>();
        for (int meshIndex = 0; meshIndex < fitting.Meshes.Count; meshIndex++)
        {
            ImportedMesh fittingMesh = fitting.Meshes[meshIndex];
            ImportedMesh canonicalMesh = canonical.Meshes[meshIndex];
            if (!fittingMesh.TriangleIndices.SequenceEqual(
                    canonicalMesh.TriangleIndices))
            {
                throw new InvalidOperationException(
                    $"{context}: fitting-to-canonical bake changed mesh " +
                    $"{meshIndex} topology.");
            }
            for (int index = 0;
                 index < fittingMesh.TriangleIndices.Length;
                 index += 3)
            {
                var first = new VertexKey(
                    meshIndex,
                    checked((int)fittingMesh.TriangleIndices[index]));
                var second = new VertexKey(
                    meshIndex,
                    checked((int)fittingMesh.TriangleIndices[index + 1]));
                var third = new VertexKey(
                    meshIndex,
                    checked((int)fittingMesh.TriangleIndices[index + 2]));
                Add(first, second);
                Add(second, third);
                Add(third, first);
            }
        }
        if (edges.Count == 0)
        {
            throw new InvalidOperationException(
                $"{context}: no internal topology edge was available as a " +
                "rigidity witness.");
        }
        foreach ((VertexKey first, VertexKey second) in edges)
        {
            AssertDistancePreserved(
                fitting,
                canonical,
                first,
                second,
                tolerance,
                $"{context} edge {Describe(first)}-{Describe(second)}");
        }
        return;

        void Add(VertexKey first, VertexKey second)
        {
            if (first == second ||
                !vertices.Contains(first) ||
                !vertices.Contains(second))
            {
                return;
            }
            if (Compare(first, second) > 0)
                (first, second) = (second, first);
            edges.Add((first, second));
        }
    }

    private static void AssertPairwiseDistancesPreserved(
        ImportedScene fitting,
        ImportedScene canonical,
        IReadOnlyList<VertexKey> vertices,
        float tolerance,
        string context)
    {
        if (vertices.Count < 2)
        {
            throw new InvalidOperationException(
                $"{context}: no pairwise rigidity witness was available.");
        }
        for (int first = 0; first < vertices.Count; first++)
        for (int second = first + 1; second < vertices.Count; second++)
        {
            AssertDistancePreserved(
                fitting,
                canonical,
                vertices[first],
                vertices[second],
                tolerance,
                $"{context} pair {Describe(vertices[first])}-" +
                Describe(vertices[second]));
        }
    }

    private static void AssertDistancePreserved(
        ImportedScene fitting,
        ImportedScene canonical,
        VertexKey first,
        VertexKey second,
        float tolerance,
        string context)
    {
        float fittingDistance = Vector3.Distance(
            fitting.Meshes[first.MeshIndex].Positions[first.VertexIndex],
            fitting.Meshes[second.MeshIndex].Positions[second.VertexIndex]);
        float canonicalDistance = Vector3.Distance(
            canonical.Meshes[first.MeshIndex].Positions[first.VertexIndex],
            canonical.Meshes[second.MeshIndex].Positions[second.VertexIndex]);
        if (!float.IsFinite(fittingDistance) ||
            !float.IsFinite(canonicalDistance) ||
            MathF.Abs(fittingDistance - canonicalDistance) > tolerance)
        {
            throw new InvalidOperationException(
                $"{context}: rigid fitting-to-canonical distance changed from " +
                $"{fittingDistance:G9} to {canonicalDistance:G9} " +
                $"(tolerance {tolerance:G9}).");
        }
    }

    private static VertexKey[] TakeEvenly(
        IReadOnlyList<VertexKey> vertices,
        int maximumCount)
    {
        if (vertices.Count <= maximumCount)
            return vertices.ToArray();
        var result = new VertexKey[maximumCount];
        for (int index = 0; index < maximumCount; index++)
        {
            int source = (int)Math.Round(
                (double)index * (vertices.Count - 1) /
                (maximumCount - 1),
                MidpointRounding.AwayFromZero);
            result[index] = vertices[source];
        }
        return result.Distinct().ToArray();
    }

    private static int Compare(VertexKey left, VertexKey right)
    {
        int mesh = left.MeshIndex.CompareTo(right.MeshIndex);
        return mesh != 0 ? mesh : left.VertexIndex.CompareTo(right.VertexIndex);
    }

    private static string DescribeHand(
        GeneratedSkinningPreparationResult preparation,
        GeneratedSkinningSemanticRegion region)
    {
        GeneratedSkinningRegionResolution hand = preparation.Analysis.SemanticRegions
            .Regions.Single(value => value.Region == region);
        if (!hand.IsApplied ||
            hand.Status != GeneratedSkinningRegionStatus.Applied)
        {
            throw new InvalidOperationException(
                $"Daphne posed semantic audit requires applied {region}: " +
                string.Join(" | ", hand.Warnings));
        }
        if (hand.MotionBranchBoneNames.Count != 5 ||
            hand.MotionBranchBoneNames.Distinct(StringComparer.Ordinal).Count() != 5)
        {
            throw new InvalidOperationException(
                $"Daphne {region} does not expose five distinct target-derived " +
                "approximate finger branches.");
        }
        if (hand.CoarseMotionVertexCount <= 0 ||
            !float.IsFinite(hand.MaximumMotionProxyWeight) ||
            hand.MaximumMotionProxyWeight <= 0 ||
            hand.MaximumMotionProxyWeight > 1f)
        {
            throw new InvalidOperationException(
                $"Daphne {region} reports invalid coarse motion diagnostics: " +
                $"vertices={hand.CoarseMotionVertexCount}, maximum proxy weight=" +
                $"{hand.MaximumMotionProxyWeight:G9}.");
        }

        int coreCount = Count(hand.CoreVerticesByMesh);
        int transitionCount = Count(hand.TransitionVerticesByMesh);
        if (coreCount == 0 || transitionCount == 0)
        {
            throw new InvalidOperationException(
                $"Daphne {region} has no complete core/transition hand lobe: " +
                $"core={coreCount}, transition={transitionCount}.");
        }

        ObservedHandStats fitting = ObserveHand(
            preparation.FittingPreviewScene,
            hand,
            $"Daphne {region} fitting");
        ObservedHandStats canonical = ObserveHand(
            preparation.PreparedScene,
            hand,
            $"Daphne {region} canonical");
        AssertHandDiagnosticsMatch(hand, fitting, "fitting");
        AssertHandDiagnosticsMatch(hand, canonical, "canonical");
        if (fitting.CoarseMotionVertexCount != canonical.CoarseMotionVertexCount ||
            MathF.Abs(
                fitting.MaximumMotionProxyWeight -
                canonical.MaximumMotionProxyWeight) > WeightSumTolerance ||
            !fitting.ActiveCoreBones.SetEquals(canonical.ActiveCoreBones))
        {
            throw new InvalidOperationException(
                $"Daphne {region} changed coarse hand weights during the " +
                "fitting-to-canonical geometry bake.");
        }

        return $"{region}:status={hand.Status},core={coreCount}," +
               $"transition={transitionCount},branches=" +
               $"{string.Join('+', hand.MotionBranchBoneNames)},articulated=" +
               $"{hand.CoarseMotionVertexCount}/" +
               $"{fitting.CoarseMotionVertexCount},maxArticulation=" +
               $"{hand.MaximumMotionProxyWeight:G6}/" +
               $"{fitting.MaximumMotionProxyWeight:G6},bones=" +
               (fitting.ActiveCoreBones.Count == 0
                   ? "none"
                   : string.Join(
                       "+",
                       fitting.ActiveCoreBones.Order(StringComparer.Ordinal)));
    }

    private static ObservedHandStats ObserveHand(
        ImportedScene scene,
        GeneratedSkinningRegionResolution hand,
        string context)
    {
        HashSet<VertexKey> core = Membership(hand.CoreVerticesByMesh);
        HashSet<VertexKey> transition = Membership(hand.TransitionVerticesByMesh);
        var activeBones = new HashSet<string>(StringComparer.Ordinal);
        int coarseVertices = 0;
        float maximumArticulationWeight = 0;
        foreach (VertexKey vertex in core)
        {
            ValidateHandSkeletonIndices(scene, vertex, hand, context);
            ImportedSkeleton skeleton = scene.Meshes[vertex.MeshIndex]
                .Skinning!.Skeleton;
            (string Bone, float Weight)[] influences = ActiveInfluences(
                scene,
                vertex);
            AssertNormalizedWeights(influences, vertex, context);
            int[] joints = influences
                .Select(value => FindJoint(skeleton, value.Bone))
                .ToArray();
            if (influences.Length is < 1 or > 2 || joints.Any(joint =>
                    !IsDescendantOf(skeleton, joint, hand.AnchorSkeletonJointIndex)) ||
                (joints.Length == 2 && !AreAdjacent(skeleton, joints[0], joints[1])))
            {
                throw new InvalidOperationException(
                    $"{context} core {Describe(vertex)} is not a one/two-joint " +
                    $"adjacent approximate finger-chain weight: {Describe(influences)}.");
            }
            foreach ((string bone, _) in influences)
                activeBones.Add(bone);
            float articulationWeight = influences
                .Where(value => value.Bone != hand.AnchorBoneName)
                .Sum(value => value.Weight);
            if (articulationWeight > WeightEpsilon)
                coarseVertices++;
            maximumArticulationWeight = MathF.Max(
                maximumArticulationWeight,
                articulationWeight);
        }

        foreach (VertexKey vertex in transition)
        {
            ValidateHandSkeletonIndices(scene, vertex, hand, context);
            (string Bone, float Weight)[] influences = ActiveInfluences(
                scene,
                vertex);
            AssertNormalizedWeights(influences, vertex, context);
            if (influences.Length is < 1 or > 2 || influences.Any(value =>
                    value.Bone != hand.AnchorBoneName &&
                    value.Bone != hand.ProximalBoneName))
            {
                throw new InvalidOperationException(
                    $"{context} transition {Describe(vertex)} is not a strict " +
                    $"Proximal+Hand wrist weight: {Describe(influences)}.");
            }
        }
        return new ObservedHandStats(
            coarseVertices,
            maximumArticulationWeight,
            activeBones);
    }

    private static void ValidateHandSkeletonIndices(
        ImportedScene scene,
        VertexKey vertex,
        GeneratedSkinningRegionResolution hand,
        string context)
    {
        ImportedSkeleton skeleton = scene.Meshes[vertex.MeshIndex].Skinning?.Skeleton ??
            throw new InvalidOperationException(
                $"{context} mesh {vertex.MeshIndex} has no target skeleton.");
        Validate(hand.AnchorSkeletonJointIndex, hand.AnchorBoneName, "anchor");
        Validate(hand.ProximalSkeletonJointIndex, hand.ProximalBoneName, "proximal");
        foreach (string branch in hand.MotionBranchBoneNames)
        {
            int joint = FindJoint(skeleton, branch);
            Validate(joint, branch, "approximate finger branch");
            IReadOnlyList<int> parents = skeleton.ParentJointIndices ??
                throw new InvalidOperationException(
                    $"{context} skeleton hierarchy is unavailable.");
            if (parents[joint] != hand.AnchorSkeletonJointIndex)
            {
                throw new InvalidOperationException(
                    $"{context} branch '{branch}' is not a direct Hand child.");
            }
        }
        return;

        void Validate(int joint, string expectedName, string role)
        {
            if (joint < 0 || joint >= skeleton.JointNames.Count ||
                skeleton.JointNames[joint] != expectedName)
            {
                throw new InvalidOperationException(
                    $"{context} {role} joint {joint} does not resolve to " +
                    $"'{expectedName}' in mesh {vertex.MeshIndex}.");
            }
        }
    }

    private static void AssertHandDiagnosticsMatch(
        GeneratedSkinningRegionResolution hand,
        ObservedHandStats observed,
        string sceneLabel)
    {
        if (observed.CoarseMotionVertexCount != hand.CoarseMotionVertexCount ||
            MathF.Abs(
                observed.MaximumMotionProxyWeight -
                hand.MaximumMotionProxyWeight) > WeightSumTolerance ||
            !observed.ActiveCoreBones.Contains(hand.AnchorBoneName) ||
            observed.ActiveCoreBones.Count < 2)
        {
            throw new InvalidOperationException(
                $"Daphne {hand.Region} {sceneLabel} weights disagree with " +
                $"coarse-motion diagnostics: reported/observed vertices=" +
                $"{hand.CoarseMotionVertexCount}/" +
                $"{observed.CoarseMotionVertexCount}, max=" +
                $"{hand.MaximumMotionProxyWeight:G9}/" +
                $"{observed.MaximumMotionProxyWeight:G9}, bones=" +
                string.Join(
                    "+",
                    observed.ActiveCoreBones.Order(StringComparer.Ordinal)) + ".");
        }
    }

    private static int FindJoint(ImportedSkeleton skeleton, string bone)
    {
        for (int joint = 0; joint < skeleton.JointNames.Count; joint++)
        {
            if (skeleton.JointNames[joint] == bone)
                return joint;
        }
        throw new InvalidOperationException(
            $"Skeleton does not contain joint '{bone}'.");
    }

    private static bool IsDescendantOf(
        ImportedSkeleton skeleton,
        int joint,
        int anchor)
    {
        IReadOnlyList<int> parents = skeleton.ParentJointIndices ??
            throw new InvalidOperationException("Skeleton hierarchy is unavailable.");
        var visited = new HashSet<int>();
        for (int current = joint; current >= 0 && visited.Add(current);
             current = parents[current])
        {
            if (current == anchor)
                return true;
        }
        return false;
    }

    private static bool AreAdjacent(
        ImportedSkeleton skeleton,
        int first,
        int second)
    {
        IReadOnlyList<int> parents = skeleton.ParentJointIndices ??
            throw new InvalidOperationException("Skeleton hierarchy is unavailable.");
        return parents[first] == second || parents[second] == first;
    }

    private static void AssertNormalizedWeights(
        IReadOnlyList<(string Bone, float Weight)> influences,
        VertexKey vertex,
        string context)
    {
        float total = influences.Sum(value => value.Weight);
        if (influences.Count == 0 ||
            influences.Any(value =>
                !float.IsFinite(value.Weight) || value.Weight <= 0) ||
            !float.IsFinite(total) ||
            MathF.Abs(total - 1) > WeightSumTolerance)
        {
            throw new InvalidOperationException(
                $"{context} {Describe(vertex)} has invalid normalized weights: " +
                Describe(influences) + ".");
        }
    }

    private static void WriteRegionDiagnostics(
        GeneratedSkinningPreparationResult preparation,
        IReadOnlyList<HeadSeedCluster> headClusters)
    {
        Console.WriteLine(
            "HEAD HIGH-CONFIDENCE ISLANDS: " +
            (headClusters.Count == 0
                ? "none"
                : string.Join(
                    " | ",
                    headClusters.Select(cluster =>
                        $"#{cluster.Index}:{cluster.Vertices.Count}v/" +
                        $"{cluster.UniquePositionCount}p center=" +
                        $"({cluster.Center.X:G9},{cluster.Center.Y:G9}," +
                        $"{cluster.Center.Z:G9}) min=" +
                        $"({cluster.Minimum.X:G9},{cluster.Minimum.Y:G9}," +
                        $"{cluster.Minimum.Z:G9}) max=" +
                        $"({cluster.Maximum.X:G9},{cluster.Maximum.Y:G9}," +
                        $"{cluster.Maximum.Z:G9})"))));
        foreach (GeneratedSkinningRegionResolution region in preparation.Analysis
                     .SemanticRegions.Regions.OrderBy(value => value.Region))
        {
            Console.WriteLine(
                $"REGION {region.Region}: applied={region.IsApplied}, " +
                $"status={region.Status}, core={Count(region.CoreVerticesByMesh)}, " +
                $"transition={Count(region.TransitionVerticesByMesh)}, " +
                $"branches={string.Join('+', region.MotionBranchBoneNames)}, " +
                $"articulated={region.CoarseMotionVertexCount}, maxArticulation=" +
                $"{region.MaximumMotionProxyWeight:G9}");
            foreach (string warning in region.Warnings)
                Console.WriteLine($"  WARNING {region.Region}: {warning}");
        }
    }

    private static void RequireAppliedRegion(
        GeneratedSkinningRegionResolution resolution)
    {
        if (!resolution.IsApplied ||
            resolution.Status != GeneratedSkinningRegionStatus.Applied)
        {
            throw new InvalidOperationException(
                $"Daphne posed semantic audit requires applied " +
                $"{resolution.Region}: " +
                string.Join(" | ", resolution.Warnings));
        }
    }

    private static (string Bone, float Weight)[] ActiveInfluences(
        ImportedScene scene,
        VertexKey vertex)
    {
        ImportedMesh mesh = scene.Meshes[vertex.MeshIndex];
        ImportedSkinning skinning = mesh.Skinning ?? throw new InvalidOperationException(
            $"Prepared mesh [{vertex.MeshIndex}] has no skinning.");
        ImportedJointIndices joints = skinning.JointIndices[vertex.VertexIndex];
        Vector4 weights = skinning.Weights[vertex.VertexIndex];
        return new[]
            {
                (Joint: joints.X, Weight: weights.X),
                (Joint: joints.Y, Weight: weights.Y),
                (Joint: joints.Z, Weight: weights.Z),
                (Joint: joints.W, Weight: weights.W)
            }
            .Where(value => value.Weight > WeightEpsilon)
            .Select(value => (
                skinning.Skeleton.JointNames[value.Joint],
                value.Weight))
            .GroupBy(value => value.Item1, StringComparer.Ordinal)
            .Select(group => (group.Key, group.Sum(value => value.Weight)))
            .OrderByDescending(value => value.Item2)
            .ThenBy(value => value.Key, StringComparer.Ordinal)
            .Select(value => (value.Key, value.Item2))
            .ToArray();
    }

    private static HashSet<VertexKey> Membership(
        IReadOnlyList<TargetRigBodyVertexMembership> memberships) =>
        memberships.SelectMany(membership => membership.VertexIndices.Select(vertex =>
            new VertexKey(membership.MeshIndex, vertex))).ToHashSet();

    private static SelectedBodyTopology BuildSelectedBodyTopology(
        ImportedScene donor,
        TargetRigBodySelection body)
    {
        HashSet<VertexKey> selected = body.Components
            .SelectMany(component => component.VerticesByMesh)
            .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                new VertexKey(membership.MeshIndex, vertex)))
            .ToHashSet();
        var adjacency = selected.ToDictionary(
            vertex => vertex,
            _ => new HashSet<VertexKey>());

        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = donor.Meshes[meshIndex];
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                var first = new VertexKey(
                    meshIndex,
                    checked((int)mesh.TriangleIndices[index]));
                var second = new VertexKey(
                    meshIndex,
                    checked((int)mesh.TriangleIndices[index + 1]));
                var third = new VertexKey(
                    meshIndex,
                    checked((int)mesh.TriangleIndices[index + 2]));
                AddEdge(first, second, selected, adjacency);
                AddEdge(second, third, selected, adjacency);
                AddEdge(third, first, selected, adjacency);
            }
        }

        // Match production: bridge exact attribute-seam duplicates only inside
        // a body component which SelectBody already proved to be one surface.
        foreach (TargetRigSelectedBodyComponent component in body.Components)
        {
            VertexKey[] componentVertices = component.VerticesByMesh
                .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                    new VertexKey(membership.MeshIndex, vertex)))
                .ToArray();
            foreach (IGrouping<Vector3, VertexKey> seam in componentVertices
                         .GroupBy(vertex => Position(donor, vertex)))
            {
                VertexKey[] duplicates = seam
                    .OrderBy(vertex => vertex.MeshIndex)
                    .ThenBy(vertex => vertex.VertexIndex)
                    .ToArray();
                for (int index = 1; index < duplicates.Length; index++)
                {
                    AddEdge(
                        duplicates[0],
                        duplicates[index],
                        selected,
                        adjacency);
                }
            }
        }
        return new SelectedBodyTopology(selected, adjacency);
    }

    private static void AddEdge(
        VertexKey first,
        VertexKey second,
        IReadOnlySet<VertexKey> selected,
        IReadOnlyDictionary<VertexKey, HashSet<VertexKey>> adjacency)
    {
        if (!selected.Contains(first) || !selected.Contains(second) ||
            first == second)
        {
            return;
        }
        adjacency[first].Add(second);
        adjacency[second].Add(first);
    }

    private static Vector3 Position(ImportedScene donor, VertexKey vertex) =>
        donor.Meshes[vertex.MeshIndex].Positions[vertex.VertexIndex];

    private static float DistanceToVolume(
        Vector3 position,
        GeneratedSkinningRegionVolume volume)
    {
        Vector3 relative = position - volume.Center;
        float axial = Vector3.Dot(relative, volume.AxialAxis) /
                      volume.AxialRadius;
        float lateral = Vector3.Dot(relative, volume.LateralAxis) /
                        volume.LateralRadius;
        float forward = Vector3.Dot(relative, volume.ForwardAxis) /
                        volume.ForwardRadius;
        float exponent = volume.ShapeExponent;
        float powered = MathF.Pow(MathF.Abs(axial), exponent) +
                        MathF.Pow(MathF.Abs(lateral), exponent) +
                        MathF.Pow(MathF.Abs(forward), exponent);
        return MathF.Pow(MathF.Max(0, powered), 1 / exponent);
    }

    private static void AssertDonorBudget(ImportedScene donor)
    {
        int vertices = donor.Meshes.Sum(mesh => mesh.Positions.Length);
        int triangles = donor.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
        if (vertices <= 0 || vertices > MaximumVertices ||
            triangles <= 0 || triangles > MaximumTriangles)
        {
            throw new InvalidOperationException(
                $"Daphne semantic-coverage donor exceeds its fixed budget: " +
                $"vertices={vertices}/{MaximumVertices}, " +
                $"triangles={triangles}/{MaximumTriangles}.");
        }
    }

    private static string RequireFile(
        string argument,
        string extension,
        string label)
    {
        string path = Path.GetFullPath(argument);
        if (!File.Exists(path))
            throw new FileNotFoundException($"{label} was not found.", path);
        if (!string.Equals(
                Path.GetExtension(path),
                extension,
                StringComparison.OrdinalIgnoreCase))
        {
            throw new ArgumentException(
                $"{label} must use the {extension} extension.",
                nameof(argument));
        }
        return path;
    }

    private static string HashFile(string path) =>
        Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));

    private static int Count(
        IReadOnlyList<TargetRigBodyVertexMembership> membership) =>
        membership.Sum(value => value.VertexIndices.Count);

    private static string Describe(VertexKey vertex) =>
        $"mesh {vertex.MeshIndex} vertex {vertex.VertexIndex}";

    private static string Describe(
        IReadOnlyList<(string Bone, float Weight)> influences) =>
        string.Join(", ", influences.Select(value =>
            $"{value.Bone}={value.Weight:G6}"));
}
