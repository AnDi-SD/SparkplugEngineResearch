using System.Numerics;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class GeneratedSkinningSemanticHandRegression
{
    private const int MaximumFixtureVertices = 192;
    private const int MaximumFixtureTriangles = 319;
    private const float WeightEpsilon = 0.000001f;
    private const float MirrorTolerance = 0.025f;

    private sealed record HandFixtureSide(
        GeneratedSkinningSemanticRegion Region,
        string AnchorName,
        string ProximalName,
        Vector3 Anchor,
        Vector3 Axis,
        IReadOnlyList<int> ExpectedLobeVertices,
        IReadOnlyList<int> BoundaryVertices,
        IReadOnlyList<int> WristBlendVertices,
        IReadOnlyList<int> PalmProbeVertices,
        IReadOnlyList<int> ExtendedTipVertices,
        IReadOnlyList<IReadOnlyList<int>> EqualProgressProngs,
        IReadOnlyList<int> ForearmGuardVertices);

    private sealed record HandFixture(
        ImportedScene Donor,
        TargetRigBodySelection BodySelection,
        int MeshIndex,
        HandFixtureSide Left,
        HandFixtureSide Right,
        IReadOnlyList<(int Left, int Right)> MirroredLobeVertices,
        float ReferenceHeight);

    private readonly record struct CoarseWeights(
        float Proximal,
        float Anchor,
        float Proxy,
        int ActiveCount);

    /// <summary>
    /// Bounded one-Prepare contract for complete topology-derived hands and the
    /// deliberately name-agnostic approximate multi-branch finger profile.
    /// The command hook is intentionally owned by Program.cs.
    /// </summary>
    public static void Run(string targetArgument)
    {
        string targetPath = RequireTarget(targetArgument);
        byte[] targetBytes = File.ReadAllBytes(targetPath);
        SmoDocument target = SmoDocument.Load(targetPath);
        if (target.HasErrors)
        {
            throw new InvalidDataException(
                "Bounded semantic-hand target failed strict parsing.");
        }

        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
        TargetRigFittingPoseSnapshot pose = rig.CreateFittingPose().Capture();
        if (!pose.IsIdentityPose)
        {
            throw new InvalidOperationException(
                "Bounded semantic-hand fixture requires the exact identity pose.");
        }

        HandFixture fixture = BuildFixture(target, rig);
        AssertFixtureBudget(fixture);
        string donorFingerprint =
            TargetRigAutomaticPoseFitter.ComputeDonorGeometryFingerprint(
                fixture.Donor);

        // Deliberately the only Prepare call. There is no automatic pose fit,
        // writer, texture serialization, animation decode, or output file.
        GeneratedSkinningPreparationResult preparation =
            GeneratedSkinningPreparer.Prepare(
                target,
                fixture.Donor,
                pose,
                ReplacementTransform.Identity,
                fixture.BodySelection);
        if (preparation.Analysis.SemanticResolutionPassCount != 1 ||
            preparation.Analysis.InternalPreparationPassCount != 1)
        {
            throw new InvalidOperationException(
                "One public Prepare must use exactly one preparation and " +
                "one semantic-resolution pass.");
        }

        SideResult left = AssertSide(
            preparation.FittingPreviewScene,
            preparation.Analysis.SemanticRegions,
            fixture.MeshIndex,
            fixture.Left,
            fixture.ReferenceHeight);
        SideResult right = AssertSide(
            preparation.FittingPreviewScene,
            preparation.Analysis.SemanticRegions,
            fixture.MeshIndex,
            fixture.Right,
            fixture.ReferenceHeight);
        AssertMirroredProfile(
            preparation.FittingPreviewScene.Meshes[fixture.MeshIndex],
            fixture,
            left,
            right);

        if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBytes) ||
            !string.Equals(
                TargetRigAutomaticPoseFitter.ComputeDonorGeometryFingerprint(
                    fixture.Donor),
                donorFingerprint,
                StringComparison.Ordinal))
        {
            throw new InvalidOperationException(
                "Bounded semantic-hand regression mutated a target or donor input.");
        }

        int vertices = fixture.Donor.Meshes.Sum(mesh => mesh.Positions.Length);
        int triangles = fixture.Donor.Meshes.Sum(mesh =>
            mesh.TriangleIndices.Length / 3);
        Console.WriteLine(
            $"BOUNDED SEMANTIC HAND PASS: {vertices} vertices/{triangles} " +
            "triangles; two complete topology lobes; extended cap witness; " +
            "UpperArm+Hand wrists; five approximate three-joint lanes per hand; " +
            "per-lane tip lengths and mirrored articulation profiles; exactly " +
            "one public Prepare/one " +
            "semantic resolve; no output.");
    }

    private sealed record SideResult(
        GeneratedSkinningRegionResolution Resolution,
        IReadOnlySet<int> FingerJoints,
        IReadOnlySet<int> Core,
        IReadOnlySet<int> Transition);

    private static SideResult AssertSide(
        ImportedScene scene,
        GeneratedSkinningRegionAnalysis analysis,
        int meshIndex,
        HandFixtureSide fixture,
        float referenceHeight)
    {
        GeneratedSkinningRegionResolution region = analysis.Regions.Single(value =>
            value.Region == fixture.Region);
        if (!region.IsApplied ||
            region.Status != GeneratedSkinningRegionStatus.Applied ||
            !string.Equals(region.AnchorBoneName, fixture.AnchorName,
                StringComparison.Ordinal) ||
            !string.Equals(region.ProximalBoneName, fixture.ProximalName,
                StringComparison.Ordinal))
        {
            throw new InvalidOperationException(
                $"{fixture.Region} did not apply with the exact Hand/UpperArm " +
                "contract: " + string.Join(" | ", region.Warnings));
        }

        HashSet<int> core = MembershipForMesh(
            region.CoreVerticesByMesh, meshIndex);
        HashSet<int> transition = MembershipForMesh(
            region.TransitionVerticesByMesh, meshIndex);
        if (core.Overlaps(transition))
        {
            throw new InvalidOperationException(
                $"{fixture.Region} core and transition overlap.");
        }

        HashSet<int> captured = core.Concat(transition).ToHashSet();
        HashSet<int> expected = fixture.ExpectedLobeVertices.ToHashSet();
        if (!captured.SetEquals(expected))
        {
            int[] missing = expected.Except(captured).Order().ToArray();
            int[] unexpected = captured.Except(expected).Order().ToArray();
            throw new InvalidOperationException(
                $"{fixture.Region} did not capture exactly the complete donor " +
                $"hand lobe behind its wrist cut; missing={Join(missing)}, " +
                $"unexpected={Join(unexpected)}.");
        }
        if (fixture.ForearmGuardVertices.Any(captured.Contains))
        {
            throw new InvalidOperationException(
                $"{fixture.Region} crossed the validated wrist cut into the forearm.");
        }
        if (fixture.BoundaryVertices.Any(vertex => !transition.Contains(vertex)) ||
            fixture.WristBlendVertices.Any(vertex => !transition.Contains(vertex)) ||
            fixture.PalmProbeVertices.Any(vertex => !core.Contains(vertex)) ||
            fixture.ExtendedTipVertices.Any(vertex => !core.Contains(vertex)))
        {
            throw new InvalidOperationException(
                $"{fixture.Region} lost its boundary/transition/core topology zones.");
        }

        ImportedMesh mesh = scene.Meshes[meshIndex];
        ImportedSkinning skin = mesh.Skinning ?? throw new InvalidOperationException(
            "Bounded semantic-hand output mesh has no generated skinning.");
        if (skin.Skeleton.ParentJointIndices is not { } parents)
        {
            throw new InvalidOperationException(
                "Bounded semantic-hand skeleton has no hierarchy.");
        }

        if (region.MotionBranchBoneNames.Count < 2)
        {
            throw new InvalidOperationException(
                $"{fixture.Region} exposes fewer than two approximate finger lanes.");
        }
        HashSet<int> fingerJoints = Enumerable.Range(0, parents.Count)
            .Where(joint => joint != region.AnchorSkeletonJointIndex &&
                            IsDescendantOf(
                                joint, region.AnchorSkeletonJointIndex, parents))
            .ToHashSet();
        HashSet<int> nonAnchorCoreJoints = core
            .SelectMany(vertex => ActiveInfluences(skin, vertex))
            .Where(value => value.Joint != region.AnchorSkeletonJointIndex)
            .Select(value => value.Joint)
            .ToHashSet();
        if (nonAnchorCoreJoints.Count < 3 ||
            !nonAnchorCoreJoints.IsSubsetOf(fingerJoints))
        {
            throw new InvalidOperationException(
                $"{fixture.Region} core did not use multiple Hand-descendant " +
                $"finger joints, found {string.Join(", ", nonAnchorCoreJoints
                    .Order().Select(index => skin.Skeleton.JointNames[index]))}.");
        }
        int[] usedRoots = nonAnchorCoreJoints
            .Select(joint => FindDirectChildBelow(
                joint, region.AnchorSkeletonJointIndex, parents))
            .Distinct()
            .ToArray();
        if (usedRoots.Length < 3)
        {
            throw new InvalidOperationException(
                $"{fixture.Region} used only {usedRoots.Length} approximate finger " +
                "lane(s); the bounded fixture requires at least three.");
        }

        foreach (int vertex in core)
        {
            CoarseWeights weights = ReadCoarseWeights(
                skin, vertex, region, fingerJoints, allowProximal: false);
            if (weights.ActiveCount is < 1 or > 2)
            {
                throw new InvalidOperationException(
                    $"{fixture.Region} core vertex {vertex} is not a bounded " +
                    "two-joint Hand/finger-chain profile.");
            }
            AssertAdjacentChainInfluences(
                skin, vertex, region.AnchorSkeletonJointIndex, parents);
        }

        int genuineWristBlend = 0;
        foreach (int vertex in transition)
        {
            CoarseWeights weights = ReadCoarseWeights(
                skin, vertex, region, fingerJoints, allowProximal: true);
            if (weights.Proxy > WeightEpsilon || weights.ActiveCount > 2)
            {
                throw new InvalidOperationException(
                    $"{fixture.Region} wrist vertex {vertex} contains proxy or " +
                    "non-wrist weight mass.");
            }
            if (weights.Proximal > WeightEpsilon &&
                weights.Anchor > WeightEpsilon)
            {
                genuineWristBlend++;
            }
        }
        if (genuineWristBlend == 0)
        {
            throw new InvalidOperationException(
                $"{fixture.Region} has no genuine UpperArm+Hand wrist blend.");
        }
        foreach (int vertex in fixture.BoundaryVertices)
        {
            CoarseWeights weights = ReadCoarseWeights(
                skin, vertex, region, fingerJoints, allowProximal: true);
            if (MathF.Abs(weights.Proximal - 1) > WeightEpsilon ||
                weights.Anchor > WeightEpsilon ||
                weights.Proxy > WeightEpsilon)
            {
                throw new InvalidOperationException(
                    $"{fixture.Region} topological wrist boundary is not exact " +
                    $"one-hot {fixture.ProximalName}.");
            }
        }

        float palmProxy = fixture.PalmProbeVertices.Average(vertex =>
            ReadCoarseWeights(skin, vertex, region, fingerJoints, false).Proxy);
        float palmAnchorMinimum = fixture.PalmProbeVertices.Min(vertex =>
            ReadCoarseWeights(skin, vertex, region, fingerJoints, false).Anchor);
        float tipProxy = fixture.ExtendedTipVertices.Average(vertex =>
            ReadCoarseWeights(skin, vertex, region, fingerJoints, false).Proxy);
        if (palmAnchorMinimum <= 0.5f ||
            tipProxy <= WeightEpsilon ||
            tipProxy <= palmProxy + 0.005f)
        {
            throw new InvalidOperationException(
                $"{fixture.Region} does not have a conservative longitudinal " +
                $"finger profile: palmArticulation={palmProxy:G9}, " +
                $"palmAnchorMin={palmAnchorMinimum:G9}, tipProxy={tipProxy:G9}.");
        }

        foreach (IReadOnlyList<int> probe in fixture.EqualProgressProngs)
        {
            if (probe.Count < 2)
                throw new InvalidOperationException("Equal-progress probe is incomplete.");
            float articulatedMass = probe.Average(vertex =>
                ReadCoarseWeights(
                    skin, vertex, region, fingerJoints, false).Proxy);
            if (articulatedMass <= WeightEpsilon)
            {
                throw new InvalidOperationException(
                    $"{fixture.Region} left an approximate prong rigid at its " +
                    "middle longitudinal probe.");
            }
        }

        float extendedProjection = fixture.ExtendedTipVertices.Min(vertex =>
            Vector3.Dot(mesh.Positions[vertex] - fixture.Anchor, fixture.Axis));
        if (extendedProjection < referenceHeight * 0.105f)
        {
            throw new InvalidOperationException(
                $"{fixture.Region} extended-cap witness was not built beyond " +
                $"the legacy hand extent: {extendedProjection:G9}.");
        }

        return new SideResult(region, fingerJoints, core, transition);
    }

    private static void AssertMirroredProfile(
        ImportedMesh mesh,
        HandFixture fixture,
        SideResult left,
        SideResult right)
    {
        ImportedSkinning skin = mesh.Skinning!;
        foreach ((int leftVertex, int rightVertex) in fixture.MirroredLobeVertices)
        {
            bool leftCore = left.Core.Contains(leftVertex);
            bool rightCore = right.Core.Contains(rightVertex);
            bool leftTransition = left.Transition.Contains(leftVertex);
            bool rightTransition = right.Transition.Contains(rightVertex);
            if (leftCore != rightCore || leftTransition != rightTransition)
            {
                throw new InvalidOperationException(
                    "Mirrored hand vertices resolved to different topology zones.");
            }

            CoarseWeights l = ReadCoarseWeights(
                skin,
                leftVertex,
                left.Resolution,
                left.FingerJoints,
                allowProximal: leftTransition);
            CoarseWeights r = ReadCoarseWeights(
                skin,
                rightVertex,
                right.Resolution,
                right.FingerJoints,
                allowProximal: rightTransition);
            if (!NearlyEqual(l.Proximal, r.Proximal, MirrorTolerance) ||
                !NearlyEqual(l.Anchor, r.Anchor, MirrorTolerance) ||
                !NearlyEqual(l.Proxy, r.Proxy, MirrorTolerance))
            {
                throw new InvalidOperationException(
                    "Mirrored hands lost the same coarse longitudinal profile: " +
                    $"L=({l.Proximal:G6},{l.Anchor:G6},{l.Proxy:G6}), " +
                    $"R=({r.Proximal:G6},{r.Anchor:G6},{r.Proxy:G6}).");
            }
        }
    }

    private static CoarseWeights ReadCoarseWeights(
        ImportedSkinning skin,
        int vertex,
        GeneratedSkinningRegionResolution region,
        IReadOnlySet<int> fingerJoints,
        bool allowProximal)
    {
        float proximal = 0;
        float anchor = 0;
        float proxyWeight = 0;
        int active = 0;
        float total = 0;
        foreach ((int joint, float weight) in ActiveInfluences(skin, vertex))
        {
            active++;
            total += weight;
            if (joint == region.AnchorSkeletonJointIndex)
                anchor += weight;
            else if (fingerJoints.Contains(joint))
                proxyWeight += weight;
            else if (allowProximal && joint == region.ProximalSkeletonJointIndex)
                proximal += weight;
            else
            {
                throw new InvalidOperationException(
                    $"{region.Region} vertex {vertex} contains forbidden joint " +
                    $"'{skin.Skeleton.JointNames[joint]}'.");
            }
        }
        if (!float.IsFinite(total) || MathF.Abs(total - 1) > WeightEpsilon)
        {
            throw new InvalidOperationException(
                $"{region.Region} vertex {vertex} has non-normalized weights " +
                $"({total:G9}).");
        }
        return new CoarseWeights(proximal, anchor, proxyWeight, active);
    }

    private static void AssertAdjacentChainInfluences(
        ImportedSkinning skin,
        int vertex,
        int anchor,
        IReadOnlyList<int> parents)
    {
        int[] joints = ActiveInfluences(skin, vertex)
            .Select(value => value.Joint)
            .ToArray();
        if (joints.Length < 2)
            return;
        if (joints.Length != 2 ||
            parents[joints[0]] != joints[1] && parents[joints[1]] != joints[0])
        {
            throw new InvalidOperationException(
                $"Hand core vertex {vertex} blends non-adjacent or different " +
                $"finger lanes: {string.Join(", ", joints.Select(joint =>
                    skin.Skeleton.JointNames[joint]))}.");
        }
        if (!IsDescendantOf(joints[0], anchor, parents) ||
            !IsDescendantOf(joints[1], anchor, parents))
        {
            throw new InvalidOperationException(
                $"Hand core vertex {vertex} escaped its Hand subtree.");
        }
    }

    private static bool IsDescendantOf(
        int joint,
        int ancestor,
        IReadOnlyList<int> parents)
    {
        var visited = new HashSet<int>();
        for (int current = joint; current >= 0 && visited.Add(current);
             current = parents[current])
        {
            if (current == ancestor)
                return true;
        }
        return false;
    }

    private static int FindDirectChildBelow(
        int joint,
        int anchor,
        IReadOnlyList<int> parents)
    {
        int current = joint;
        var visited = new HashSet<int>();
        while (current >= 0 && visited.Add(current))
        {
            int parent = parents[current];
            if (parent == anchor)
                return current;
            current = parent;
        }
        throw new InvalidOperationException(
            $"Joint {joint} is not below Hand {anchor}.");
    }

    private static IEnumerable<(int Joint, float Weight)> ActiveInfluences(
        ImportedSkinning skin,
        int vertex)
    {
        ImportedJointIndices joints = skin.JointIndices[vertex];
        Vector4 weights = skin.Weights[vertex];
        (int Joint, float Weight)[] values =
        [
            (joints.X, weights.X),
            (joints.Y, weights.Y),
            (joints.Z, weights.Z),
            (joints.W, weights.W)
        ];
        foreach ((int joint, float weight) in values)
        {
            if (!float.IsFinite(weight) || weight < 0)
                throw new InvalidOperationException("Generated hand weight is invalid.");
            if (weight > WeightEpsilon)
                yield return (joint, weight);
        }
    }

    private static HandFixture BuildFixture(
        SmoDocument target,
        TargetRigDefinition rig)
    {
        Vector3[] deformPositions = rig.Joints
            .Where(joint => joint.IsDeformJoint)
            .Select(joint => Translation(joint.BindWorldMatrix))
            .ToArray();
        float minimumY = deformPositions.Min(position => position.Y);
        float maximumY = deformPositions.Max(position => position.Y);
        float height = maximumY - minimumY;
        if (!float.IsFinite(height) || height <= 0.001f)
            throw new InvalidDataException("Target rig has no finite character height.");

        Vector3 pelvis = JointPosition(rig, "Pelvis");
        var positions = new List<Vector3>();
        var triangles = new List<uint>();

        int[] AddRing(
            Vector3 center,
            Vector3 firstAxis,
            Vector3 secondAxis,
            float firstRadius,
            float secondRadius)
        {
            int start = positions.Count;
            positions.Add(center + firstAxis * firstRadius + secondAxis * secondRadius);
            positions.Add(center - firstAxis * firstRadius + secondAxis * secondRadius);
            positions.Add(center - firstAxis * firstRadius - secondAxis * secondRadius);
            positions.Add(center + firstAxis * firstRadius - secondAxis * secondRadius);
            return Enumerable.Range(start, 4).ToArray();
        }

        void AddTriangle(int first, int second, int third)
        {
            triangles.Add(checked((uint)first));
            triangles.Add(checked((uint)second));
            triangles.Add(checked((uint)third));
        }

        void ConnectRings(IReadOnlyList<int> first, IReadOnlyList<int> second)
        {
            for (int edge = 0; edge < 4; edge++)
            {
                int next = (edge + 1) % 4;
                AddTriangle(first[edge], second[edge], first[next]);
                AddTriangle(first[next], second[edge], second[next]);
            }
        }

        void Cap(IReadOnlyList<int> ring, bool reverse)
        {
            if (reverse)
            {
                AddTriangle(ring[0], ring[2], ring[1]);
                AddTriangle(ring[0], ring[3], ring[2]);
            }
            else
            {
                AddTriangle(ring[0], ring[1], ring[2]);
                AddTriangle(ring[0], ring[2], ring[3]);
            }
        }

        float[] torsoLevels =
        [
            minimumY,
            minimumY + height * 0.08f,
            minimumY + height * 0.30f,
            minimumY + height * 0.52f,
            minimumY + height * 0.72f,
            minimumY + height * 0.90f,
            maximumY
        ];
        var torsoRings = new List<int[]>();
        foreach (float level in torsoLevels)
        {
            torsoRings.Add(AddRing(
                new Vector3(pelvis.X, level, pelvis.Z),
                Vector3.UnitX,
                Vector3.UnitZ,
                height * 0.035f,
                height * 0.022f));
        }
        for (int index = 1; index < torsoRings.Count; index++)
            ConnectRings(torsoRings[index - 1], torsoRings[index]);
        Cap(torsoRings[0], reverse: true);
        Cap(torsoRings[^1], reverse: false);

        int armAttachmentRing = Enumerable.Range(0, torsoLevels.Length)
            .OrderBy(index => MathF.Abs(
                torsoLevels[index] - JointPosition(rig, "Spine_03").Y))
            .ThenBy(index => index)
            .First();

        HandFixtureSide BuildSide(
            GeneratedSkinningSemanticRegion region,
            string prefix)
        {
            string anchorName = prefix + "Hand";
            string proximalName = prefix + "UpperArm";
            Vector3 shoulder = JointPosition(rig, prefix + "Bicep");
            Vector3 elbow = JointPosition(rig, proximalName);
            Vector3 hand = JointPosition(rig, anchorName);
            Vector3 axis = hand - elbow;
            if (!float.IsFinite(axis.LengthSquared()) ||
                axis.LengthSquared() <= 0.000000000001f)
            {
                throw new InvalidDataException(
                    $"Target {anchorName} has a degenerate forearm axis.");
            }
            axis = Vector3.Normalize(axis);
            Vector3 spread = ResolveFingerSpreadAxis(rig, anchorName, axis);
            Vector3 thickness = Vector3.Cross(axis, spread);
            if (!float.IsFinite(thickness.LengthSquared()) ||
                thickness.LengthSquared() <= 0.000000000001f)
            {
                throw new InvalidDataException(
                    $"Target {anchorName} has a degenerate hand frame.");
            }
            thickness = Vector3.Normalize(thickness);
            spread = Vector3.Normalize(Vector3.Cross(thickness, axis));

            float armRadius = height * 0.0115f;
            (Vector3 Center, float SpreadRadius, float ThicknessRadius)[] path =
            [
                (shoulder, armRadius, armRadius),
                (elbow, armRadius, armRadius),
                (Vector3.Lerp(elbow, hand, 0.52f), armRadius, armRadius),
                (hand - axis * (height * 0.026f), armRadius, armRadius),
                (hand - axis * (height * 0.012f), height * 0.0075f,
                    height * 0.0065f),
                (hand - axis * (height * 0.003f), height * 0.010f,
                    height * 0.007f),
                (hand + axis * (height * 0.012f), height * 0.017f,
                    height * 0.008f),
                (hand + axis * (height * 0.032f), height * 0.022f,
                    height * 0.0085f),
                (hand + axis * (height * 0.050f), height * 0.020f,
                    height * 0.0075f)
            ];
            int[][] rings = path.Select(value => AddRing(
                    value.Center,
                    spread,
                    thickness,
                    value.SpreadRadius,
                    value.ThicknessRadius))
                .ToArray();
            for (int index = 1; index < rings.Length; index++)
                ConnectRings(rings[index - 1], rings[index]);

            // A bounded surface bridge keeps both arms in the same selected
            // body component without adding a second fitting/body-selection path.
            int[] torso = torsoRings[armAttachmentRing];
            AddTriangle(torso[0], rings[0][0], torso[1]);
            AddTriangle(torso[1], rings[0][0], rings[0][1]);

            var prongMidRings = new List<int[]>();
            var prongTipRings = new List<int[]>();
            float[] offsets = [-0.021f, -0.0105f, 0, 0.0105f, 0.021f];
            for (int prong = 0; prong < offsets.Length; prong++)
            {
                Vector3 offset = spread * (height * offsets[prong]);
                int[] mid = AddRing(
                    hand + axis * (height * 0.070f) + offset,
                    spread,
                    thickness,
                    height * 0.0045f,
                    height * 0.0038f);
                float tipProjection = prong == 2 ? 0.115f : 0.085f;
                int[] tip = AddRing(
                    hand + axis * (height * tipProjection) + offset,
                    spread,
                    thickness,
                    height * 0.0035f,
                    height * 0.0030f);
                prongMidRings.Add(mid);
                prongTipRings.Add(tip);
                int palmEdge = prong switch
                {
                    0 or 1 => 1,
                    2 => 2,
                    _ => 3
                };
                int palmNext = (palmEdge + 1) % 4;
                AddTriangle(rings[^1][palmEdge], mid[0], rings[^1][palmNext]);
                AddTriangle(rings[^1][palmNext], mid[0], mid[1]);
                ConnectRings(mid, tip);
                Cap(tip, reverse: false);
            }

            int[] expectedLobe = rings.Skip(5)
                .SelectMany(value => value)
                .Concat(prongMidRings.SelectMany(value => value))
                .Concat(prongTipRings.SelectMany(value => value))
                .ToArray();
            int[] guards = rings.Take(5).SelectMany(value => value).ToArray();
            return new HandFixtureSide(
                region,
                anchorName,
                proximalName,
                hand,
                axis,
                Array.AsReadOnly(expectedLobe),
                Array.AsReadOnly(rings[5]),
                Array.AsReadOnly(rings[6]),
                Array.AsReadOnly(rings[7]),
                Array.AsReadOnly(prongTipRings[2]),
                Array.AsReadOnly(prongMidRings
                    .Select(ring => (IReadOnlyList<int>)Array.AsReadOnly(ring))
                    .ToArray()),
                Array.AsReadOnly(guards));
        }

        HandFixtureSide left = BuildSide(
            GeneratedSkinningSemanticRegion.LeftHand,
            "L_");
        HandFixtureSide right = BuildSide(
            GeneratedSkinningSemanticRegion.RightHand,
            "R_");
        if (left.ExpectedLobeVertices.Count != right.ExpectedLobeVertices.Count)
        {
            throw new InvalidOperationException(
                "Bounded hand fixture is not topologically mirrored.");
        }
        (int Left, int Right)[] mirrored = left.ExpectedLobeVertices
            .Zip(right.ExpectedLobeVertices, (l, r) => (l, r))
            .ToArray();

        ImportedMaterial[] materials = [new("bounded_hand_material")];
        var mesh = new ImportedMesh(
            "bounded_hand_surface",
            positions.ToArray(),
            Enumerable.Repeat(Vector3.UnitZ, positions.Count).ToArray(),
            Enumerable.Repeat(Vector2.Zero, positions.Count).ToArray(),
            triangles.ToArray(),
            DiffuseColorsArgb: null,
            MaterialIndex: 0,
            Skinning: null);
        var donor = new ImportedScene(
            Array.AsReadOnly(new[] { mesh }),
            EmbeddedTextures: null,
            Array.AsReadOnly(materials));

        Vector3 minimum = positions[0];
        Vector3 maximum = positions[0];
        double area = 0;
        foreach (Vector3 position in positions.Skip(1))
        {
            minimum = Vector3.Min(minimum, position);
            maximum = Vector3.Max(maximum, position);
        }
        for (int index = 0; index < triangles.Count; index += 3)
        {
            Vector3 first = positions[checked((int)triangles[index])];
            Vector3 second = positions[checked((int)triangles[index + 1])];
            Vector3 third = positions[checked((int)triangles[index + 2])];
            area += Vector3.Cross(second - first, third - first).Length() * 0.5;
        }
        var membership = new TargetRigBodyVertexMembership(
            0,
            mesh.Name,
            Array.AsReadOnly(Enumerable.Range(0, positions.Count).ToArray()));
        var component = new TargetRigSelectedBodyComponent(
            0,
            TargetRigBodyComponentRole.WholeBody,
            Array.AsReadOnly(new[] { membership }),
            positions.Distinct().Count(),
            triangles.Count / 3,
            checked((float)area),
            minimum,
            maximum);
        var bodySelection = new TargetRigBodySelection(
            Array.AsReadOnly(new[] { component }),
            TotalComponentCount: 1,
            ExcludedComponentCount: 0,
            rig.SourceFingerprint,
            TargetRigAutomaticPoseFitter.ComputeDonorGeometryFingerprint(donor),
            ReplacementTransform.Identity);
        return new HandFixture(
            donor,
            bodySelection,
            MeshIndex: 0,
            left,
            right,
            Array.AsReadOnly(mirrored),
            height);
    }

    private static Vector3 ResolveFingerSpreadAxis(
        TargetRigDefinition rig,
        string handName,
        Vector3 handAxis)
    {
        int hand = rig.GetJointIndex(handName);
        TargetRigJoint[] roots = rig.Joints
            .Where(joint => joint.IsDeformJoint &&
                            FindNearestDeformParent(rig, joint.JointIndex) == hand)
            .OrderBy(joint => joint.JointIndex)
            .ToArray();
        Vector3[] offsets = roots
            .Select(root =>
            {
                Vector3 value = Translation(root.BindWorldMatrix) -
                                Translation(rig.Joints[hand].BindWorldMatrix);
                return value - handAxis * Vector3.Dot(value, handAxis);
            })
            .ToArray();
        Vector3 best = Vector3.Zero;
        float bestLength = 0;
        for (int first = 0; first < offsets.Length; first++)
        for (int second = first + 1; second < offsets.Length; second++)
        {
            Vector3 difference = offsets[second] - offsets[first];
            float length = difference.LengthSquared();
            if (float.IsFinite(length) && length > bestLength)
            {
                best = difference;
                bestLength = length;
            }
        }
        if (bestLength <= 0.000000000001f)
        {
            best = Vector3.UnitZ -
                   handAxis * Vector3.Dot(Vector3.UnitZ, handAxis);
        }
        if (!float.IsFinite(best.LengthSquared()) ||
            best.LengthSquared() <= 0.000000000001f)
        {
            best = Vector3.UnitY -
                   handAxis * Vector3.Dot(Vector3.UnitY, handAxis);
        }
        if (!float.IsFinite(best.LengthSquared()) ||
            best.LengthSquared() <= 0.000000000001f)
        {
            throw new InvalidDataException(
                $"Target {handName} cannot define a transverse hand axis.");
        }
        return Vector3.Normalize(best);
    }

    private static int FindNearestDeformParent(
        TargetRigDefinition rig,
        int jointIndex)
    {
        int cursor = rig.Joints[jointIndex].ParentJointIndex;
        var visited = new HashSet<int>();
        while (cursor >= 0 && visited.Add(cursor))
        {
            if (rig.Joints[cursor].IsDeformJoint)
                return cursor;
            cursor = rig.Joints[cursor].ParentJointIndex;
        }
        return -1;
    }

    private static void AssertFixtureBudget(HandFixture fixture)
    {
        int vertices = fixture.Donor.Meshes.Sum(mesh => mesh.Positions.Length);
        int triangles = fixture.Donor.Meshes.Sum(mesh =>
            mesh.TriangleIndices.Length / 3);
        if (fixture.Donor.Meshes.Count != 1 ||
            vertices <= 0 || vertices > MaximumFixtureVertices ||
            triangles <= 0 || triangles > MaximumFixtureTriangles ||
            fixture.BodySelection.TotalComponentCount != 1 ||
            fixture.BodySelection.Components.Count != 1)
        {
            throw new InvalidOperationException(
                $"Bounded semantic-hand fixture exceeded its pre-Prepare budget: " +
                $"meshes={fixture.Donor.Meshes.Count}/1, " +
                $"vertices={vertices}/{MaximumFixtureVertices}, " +
                $"triangles={triangles}/{MaximumFixtureTriangles}, " +
                $"components={fixture.BodySelection.TotalComponentCount}/1.");
        }
    }

    private static HashSet<int> MembershipForMesh(
        IReadOnlyList<TargetRigBodyVertexMembership> memberships,
        int meshIndex) => memberships
        .Where(value => value.MeshIndex == meshIndex)
        .SelectMany(value => value.VertexIndices)
        .ToHashSet();

    private static Vector3 JointPosition(TargetRigDefinition rig, string name) =>
        Translation(rig.Joints[rig.GetJointIndex(name)].BindWorldMatrix);

    private static Vector3 Translation(Matrix4x4 value) =>
        new(value.M41, value.M42, value.M43);

    private static bool NearlyEqual(float left, float right, float tolerance) =>
        MathF.Abs(left - right) <= tolerance;

    private static string Join(IReadOnlyList<int> values) =>
        values.Count == 0 ? "none" : string.Join(",", values);

    private static string RequireTarget(string argument)
    {
        string path = Path.GetFullPath(argument);
        if (!File.Exists(path))
            throw new FileNotFoundException("Semantic-hand target was not found.", path);
        if (!Path.GetExtension(path).Equals(".smo", StringComparison.OrdinalIgnoreCase))
            throw new ArgumentException("Semantic-hand target must be an .smo file.");
        return path;
    }
}
