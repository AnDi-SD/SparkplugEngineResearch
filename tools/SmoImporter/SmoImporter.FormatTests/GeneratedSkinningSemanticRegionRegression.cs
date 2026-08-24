using System.Numerics;
using System.Security.Cryptography;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class GeneratedSkinningSemanticRegionRegression
{
    private const float DaphneScale = 84.5f;
    private static readonly Vector3 DaphneTranslation = new(0, 12, -3);
    private const int BoundedHeadMaximumVertices = 112;
    private const int BoundedHeadMaximumTriangles = 170;
    private const int BoundedHeadExpectedComponents = 6;

    private sealed record BoundedHeadFixture(
        ImportedScene Donor,
        TargetRigBodySelection BodySelection,
        int BodyMeshIndex,
        int DetailMeshIndex,
        IReadOnlyList<int> FaceTipVertices,
        IReadOnlyList<int> ProtectedNeckBoundaryVertices,
        IReadOnlyList<int> TorsoVertices,
        IReadOnlyList<int> ExpectedHeadVertices,
        IReadOnlyList<int> FaceLandmarkVertices,
        IReadOnlyList<int> SecondaryFaceVertices,
        IReadOnlyList<int> SecondaryFaceBoundaryVertices,
        IReadOnlyDictionary<int, IReadOnlyList<int>> DetailVerticesByComponent,
        int WingComponentIndex,
        float ReferenceHeight);

    public static void RunSynthetic(string targetArgument)
    {
        string targetPath = RequireInput(targetArgument, ".smo", "target SMO");
        byte[] targetBytes = File.ReadAllBytes(targetPath);
        SmoDocument target = SmoDocument.Load(targetPath);
        if (target.HasErrors)
            throw new InvalidDataException("Semantic-region target failed strict parsing.");

        SmoExportScene targetScene = BuildTargetScene(target);
        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
        ImportedScene donor = BuildIdentifierAgnosticDonor(targetScene, rig);
        string donorFingerprint = FingerprintDonor(donor);
        var alignment = ReplacementTransform.Identity;
        TargetRigFittingPoseSnapshot pose = rig.CreateFittingPose().Capture();
        if (!pose.IsIdentityPose)
            throw new InvalidOperationException("Synthetic fixture did not capture bind pose.");
        TargetRigBodySelection body = TargetRigAutomaticPoseFitter.SelectBody(
            rig,
            donor,
            alignment);

        GeneratedSkinningPreparationResult automatic = GeneratedSkinningPreparer.Prepare(
            target,
            donor,
            pose,
            alignment,
            body);
        GeneratedSkinningRegionAnalysis analysis = automatic.Analysis.SemanticRegions;
        AssertHandsAppliedAndUnsafeHeadRejected(
            analysis,
            "identifier-agnostic synthetic donor");
        GeneratedSkinningPreparationResult disabled = PrepareAllDisabled(
            target,
            donor,
            rig,
            pose,
            alignment,
            body,
            analysis);

        AssertSemanticWeightContract(automatic, analysis, "synthetic auto");
        AssertOutsideBitIdentity(automatic, disabled, analysis, "synthetic auto");
        AssertSeamConsistency(donor, body, analysis);
        AssertMirroredAdjustments(
            target,
            donor,
            pose,
            alignment,
            body,
            automatic);

        // Keep this command a bounded three-Prepare smoke test. The former
        // stale/empty-capture/overlap stress matrix repeated full target scene,
        // topology and weight construction up to 23 additional times. Those
        // destructive-fixture cases need dedicated tiny unit scenes before
        // they can safely return to a normal developer command.

        if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBytes) ||
            FingerprintDonor(donor) != donorFingerprint)
        {
            throw new InvalidOperationException(
                "Semantic-region regression mutated a target or donor input.");
        }

        Console.WriteLine(
            "GENERATED SEMANTIC REGION PASS: identifier-free shuffled donor; " +
            DescribeRegions(analysis) + "; unsafe unbounded Head rejected; " +
            "outside weights bit-identical; hand cores/finger lanes and " +
            "proximal-only transitions valid; mirrored edits; exact seams.");
    }

    public static void RunBoundedHeadTopology(string targetArgument)
    {
        string targetPath = RequireInput(targetArgument, ".smo", "target SMO");
        byte[] targetBytes = File.ReadAllBytes(targetPath);
        SmoDocument target = SmoDocument.Load(targetPath);
        if (target.HasErrors)
            throw new InvalidDataException(
                "Bounded head-topology target failed strict parsing.");

        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
        TargetRigFittingPose editablePose = rig.CreateFittingPose();
        editablePose.SetLocalRotationDelta(
            "Neck",
            Quaternion.CreateFromAxisAngle(Vector3.UnitX, MathF.PI / 18f));
        TargetRigFittingPoseSnapshot pose = editablePose.Capture();
        if (pose.IsIdentityPose)
            throw new InvalidOperationException(
                "Bounded head-topology fixture did not create a fitting pose.");

        BoundedHeadFixture fixture = BuildBoundedHeadFixture(target, rig, pose);
        AssertBoundedHeadFixtureBudget(fixture.Donor);
        string donorFingerprint = FingerprintDonor(fixture.Donor);

        // The first Prepare exercises automatic head-lobe topology. A second
        // bounded pass below verifies that an explicit pitched editor volume
        // becomes the actual membership rule instead of being forced to retain
        // the automatically proven back-of-head lobe.
        GeneratedSkinningPreparationResult preparation =
            GeneratedSkinningPreparer.Prepare(
                target,
                fixture.Donor,
                pose,
                ReplacementTransform.Identity,
                fixture.BodySelection);
        if (preparation.Analysis.SemanticResolutionPassCount != 1 ||
            preparation.Analysis.InternalPreparationPassCount is < 2 or > 4)
        {
            throw new InvalidOperationException(
                "One bounded Head Prepare must use one final semantic resolve " +
                "after one to three legacy palette probes.");
        }

        AssertBoundedHeadTopology(preparation, fixture);
        GeneratedSkinningRegionResolution editableHead = Region(
            preparation.Analysis.SemanticRegions,
            GeneratedSkinningSemanticRegion.Head);
        if (editableHead.AdjustmentLimits.MaximumAxialScale < 2.99f ||
            editableHead.AdjustmentLimits.MaximumRadialScale < 2.99f)
        {
            throw new InvalidOperationException(
                "Head editor range regressed below the finite 3x modular-head " +
                "override required when automatic topology starts from a small " +
                "primary face lobe.");
        }
        GeneratedSkinningRegionAdjustment[] tiltedAdjustments = preparation
            .Analysis.SemanticRegions.Regions
            .Select(region => region.Region == GeneratedSkinningSemanticRegion.Head
                ? region.Adjustment with { ForwardTiltDegrees = 25 }
                : region.Adjustment with { Enabled = false })
            .ToArray();
        GeneratedSkinningPreparationResult tilted =
            GeneratedSkinningPreparer.Prepare(
                target,
                fixture.Donor,
                pose,
                ReplacementTransform.Identity,
                fixture.BodySelection,
                preparation.Analysis.SemanticRegions.CreateOverrides(
                    tiltedAdjustments));
        GeneratedSkinningRegionResolution tiltedHead = Region(
            tilted.Analysis.SemanticRegions,
            GeneratedSkinningSemanticRegion.Head);
        GeneratedSkinningRegionVolume automaticHead = tiltedHead.AutomaticVolume ??
            throw new InvalidOperationException("Tilted Head lost its automatic volume.");
        GeneratedSkinningRegionVolume resolvedHead = tiltedHead.ResolvedVolume ??
            throw new InvalidOperationException("Tilted Head lost its resolved volume.");
        Vector3 automaticLowerPole = automaticHead.Center -
                                     automaticHead.AxialAxis * automaticHead.AxialRadius;
        Vector3 resolvedLowerPole = resolvedHead.Center -
                                    resolvedHead.AxialAxis * resolvedHead.AxialRadius;
        if (!tiltedHead.IsApplied ||
            MathF.Abs(Vector3.Dot(resolvedHead.AxialAxis,
                automaticHead.AxialAxis)) > 0.999f ||
            Vector3.Dot(resolvedLowerPole - automaticLowerPole,
                automaticHead.ForwardAxis) <= 0 ||
            tiltedHead.TransitionVerticesByMesh.Sum(value =>
                value.VertexIndices.Count) != 0)
        {
            throw new InvalidOperationException(
                "Explicit Head pitch did not produce a forward lower pole and " +
                "fully rigid manual ellipsoid membership.");
        }
        if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBytes) ||
            FingerprintDonor(fixture.Donor) != donorFingerprint)
        {
            throw new InvalidOperationException(
                "Bounded head-topology regression mutated a target or donor input.");
        }

        Console.WriteLine(
            "BOUNDED HEAD TOPOLOGY PASS: one primary plus two symmetric " +
            "same-owner protected face shells; " +
            "every primary/secondary rigid boundary guarded by one disjoint " +
            "external Neck collar; " +
            "four semantic Head companions; unrelated long wing excluded; " +
            "manual +25 degree face tilt uses rigid ellipsoid membership; " +
            "<=112 vertices/<=170 triangles; two bounded Prepare calls.");
    }

    public static void RunHeadDonorAudit(
        string targetArgument,
        string donorArgument,
        string scaleArgument,
        string translationXArgument,
        string translationYArgument,
        string translationZArgument,
        string minimumCompanionsArgument)
    {
        string targetPath = RequireInput(targetArgument, ".smo", "target SMO");
        string donorPath = Path.GetFullPath(donorArgument);
        if (!File.Exists(donorPath))
            throw new FileNotFoundException("Head-donor audit input was not found.", donorPath);
        byte[] targetBytes = File.ReadAllBytes(targetPath);
        byte[] donorBytes = File.ReadAllBytes(donorPath);
        float ParseFinite(string value, string label)
        {
            if (!float.TryParse(
                    value,
                    System.Globalization.NumberStyles.Float,
                    System.Globalization.CultureInfo.InvariantCulture,
                    out float parsed) ||
                !float.IsFinite(parsed))
            {
                throw new ArgumentException(
                    $"Head-donor audit {label} must be a finite invariant float.");
            }
            return parsed;
        }

        float scale = ParseFinite(scaleArgument, "scale");
        if (scale <= 0)
            throw new ArgumentOutOfRangeException(
                nameof(scaleArgument),
                "Head-donor audit scale must be positive.");
        Vector3 translation = new(
            ParseFinite(translationXArgument, "translation X"),
            ParseFinite(translationYArgument, "translation Y"),
            ParseFinite(translationZArgument, "translation Z"));
        if (!int.TryParse(
                minimumCompanionsArgument,
                System.Globalization.NumberStyles.None,
                System.Globalization.CultureInfo.InvariantCulture,
                out int minimumCompanions) ||
            minimumCompanions < 0 || minimumCompanions > 256)
        {
            throw new ArgumentException(
                "Head-donor audit minimum companions must be an integer from 0 to 256.");
        }

        SmoDocument target = SmoDocument.Load(targetPath);
        if (target.HasErrors)
            throw new InvalidDataException(
                "Head-donor audit target failed strict parsing.");
        ImportedScene donor = ImportedModelReader.ReadGeometryOnly(donorPath);
        int vertices = donor.Meshes.Sum(mesh => mesh.Positions.Length);
        int triangles = donor.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
        if (vertices <= 0 || vertices > 10_000 ||
            triangles <= 0 || triangles > 20_000)
        {
            throw new InvalidOperationException(
                $"Head-donor audit refused an input outside its fixed budget: " +
                $"vertices={vertices}/10000, triangles={triangles}/20000.");
        }

        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
        var alignment = new ReplacementTransform(
            scale,
            Vector3.Zero,
            translation);
        TargetRigBodySelection body = TargetRigAutomaticPoseFitter.SelectBody(
            rig,
            donor,
            alignment);
        if (body.TotalComponentCount > 256 || body.Components.Count > 4)
        {
            throw new InvalidOperationException(
                $"Head-donor audit refused an excessive topology selection: " +
                $"components={body.TotalComponentCount}/256, selected=" +
                $"{body.Components.Count}/4.");
        }

        // Deliberately the only Prepare call in this command. There is no pose
        // optimizer, writer, atlas, animation decode or output-file creation.
        GeneratedSkinningPreparationResult preparation =
            GeneratedSkinningPreparer.Prepare(
                target,
                donor,
                rig.CreateFittingPose().Capture(),
                alignment,
                body);
        GeneratedSkinningRegionResolution head = Region(
            preparation.Analysis.SemanticRegions,
            GeneratedSkinningSemanticRegion.Head);
        if (!head.IsApplied ||
            head.Status != GeneratedSkinningRegionStatus.Applied)
        {
            throw new InvalidOperationException(
                "Head-donor audit could not prove a safe complete Head lobe: " +
                string.Join(" | ", head.Warnings));
        }
        ValidateCore(
            preparation.FittingPreviewScene,
            head,
            "head-donor fitting");
        ValidateCore(
            preparation.PreparedScene,
            head,
            "head-donor canonical");
        ValidateTransition(
            preparation.FittingPreviewScene,
            head,
            "head-donor fitting");
        ValidateTransition(
            preparation.PreparedScene,
            head,
            "head-donor canonical");

        GeneratedSkinningAttachment[] companions = preparation.Analysis.Attachments
            .Where(attachment =>
                attachment.SemanticAssignment == GeneratedSkinningSemanticRegion.Head)
            .OrderBy(attachment => attachment.ComponentIndex)
            .ToArray();
        if (companions.Length < minimumCompanions)
        {
            throw new InvalidOperationException(
                $"Head-donor audit proved only {companions.Length} semantic " +
                $"companion(s), below the required minimum {minimumCompanions}: " +
                string.Join(", ", companions.Select(value =>
                    $"#{value.ComponentIndex}")));
        }
        foreach (GeneratedSkinningAttachment companion in companions)
        {
            AssertAttachmentOneHot(
                preparation.FittingPreviewScene,
                companion,
                $"head-donor companion #{companion.ComponentIndex} fitting");
            AssertAttachmentOneHot(
                preparation.PreparedScene,
                companion,
                $"head-donor companion #{companion.ComponentIndex} canonical");
        }
        if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBytes) ||
            !File.ReadAllBytes(donorPath).SequenceEqual(donorBytes))
        {
            throw new InvalidOperationException(
                "Head-donor audit mutated a target or donor input.");
        }

        Console.WriteLine(
            $"HEAD DONOR AUDIT PASS: {vertices} vertices/{triangles} triangles; " +
            $"selected body component(s) " +
            $"{string.Join(",", body.Components.Select(value => $"#{value.ComponentIndex}"))}; " +
            $"Head core={Count(head.CoreVerticesByMesh)}, " +
            $"transition={Count(head.TransitionVerticesByMesh)}; semantic companions=" +
            (companions.Length == 0
                ? "none"
                : string.Join(", ", companions.Select(attachment =>
                    $"#{attachment.ComponentIndex}({attachment.VertexCount}v)"))) +
            "; exactly one Prepare; no output written.");
        Console.WriteLine("HEAD DIAGNOSTICS: " + string.Join(" | ", head.Warnings));
        Console.WriteLine(
            "ATTACHMENTS: " +
            string.Join(", ", preparation.Analysis.Attachments
                .OrderBy(attachment => attachment.ComponentIndex)
                .Select(attachment =>
                    $"#{attachment.ComponentIndex}=" +
                    $"{attachment.TargetBoneName}" +
                    (attachment.SemanticAssignment is null
                        ? string.Empty
                        : $"[semantic {attachment.SemanticAssignment}]"))));

        GeneratedSkinningRegionVolume volume = head.ResolvedVolume ??
            throw new InvalidOperationException("Applied Head has no resolved volume.");
        var headCoreKeys = head.CoreVerticesByMesh
            .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                (membership.MeshIndex, Vertex: vertex)))
            .ToHashSet();
        Vector3[] headCorePositions = headCoreKeys
            .Select(value => preparation.FittingPreviewScene.Meshes[value.MeshIndex]
                .Positions[value.Vertex])
            .Distinct()
            .ToArray();
        Vector3[] nonHeadBodyPositions = body.Components
            .SelectMany(component => component.VerticesByMesh)
            .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                (membership.MeshIndex, Vertex: vertex)))
            .Where(value => !headCoreKeys.Contains(value))
            .Select(value => preparation.FittingPreviewScene.Meshes[value.MeshIndex]
                .Positions[value.Vertex])
            .Distinct()
            .ToArray();
        float Upper95(IEnumerable<float> values)
        {
            float[] ordered = values.Order().ToArray();
            if (ordered.Length == 0)
                return float.PositiveInfinity;
            int index = Math.Clamp(
                (int)MathF.Ceiling(ordered.Length * 0.95f) - 1,
                0,
                ordered.Length - 1);
            return ordered[index];
        }
        float EnvelopeDistance(Vector3 position)
        {
            Vector3 relative = position - volume.Center;
            float axial = Vector3.Dot(relative, volume.AxialAxis) /
                          volume.AxialRadius;
            float lateral = Vector3.Dot(relative, volume.LateralAxis) /
                            volume.LateralRadius;
            float forward = Vector3.Dot(relative, volume.ForwardAxis) /
                            volume.ForwardRadius;
            return MathF.Sqrt(axial * axial + lateral * lateral +
                              forward * forward);
        }
        Console.WriteLine(
            "ATTACHMENT HEAD PROXIMITY: " +
            string.Join(", ", preparation.Analysis.Attachments
                .OrderBy(attachment => attachment.ComponentIndex)
                .Select(attachment =>
                {
                    Vector3[] positions = attachment.VerticesByMesh
                        .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                            preparation.FittingPreviewScene.Meshes[membership.MeshIndex]
                                .Positions[vertex]))
                        .Distinct()
                        .ToArray();
                    float[] headDistances = positions.Select(position =>
                        MathF.Sqrt(headCorePositions.Min(core =>
                            Vector3.DistanceSquared(position, core)))).ToArray();
                    int closerToOther = positions.Count(position =>
                    {
                        float headSquared = headCorePositions.Min(core =>
                            Vector3.DistanceSquared(position, core));
                        float otherSquared = nonHeadBodyPositions.Min(other =>
                            Vector3.DistanceSquared(position, other));
                        return headSquared > otherSquared + 0.000001f;
                    });
                    return $"#{attachment.ComponentIndex}:gap95=" +
                           $"{Upper95(headDistances):G5},env95=" +
                           $"{Upper95(positions.Select(EnvelopeDistance)):G5}," +
                           $"other={closerToOther}/{positions.Length}";
                })));
    }

    public static void RunDaphne(
        string targetArgument,
        string donorArgument,
        string outputArgument,
        string? animationDirectoryArgument)
    {
        string targetPath = RequireInput(targetArgument, ".smo", "target SMO");
        string donorPath = RequireInput(donorArgument, ".obj", "Daphne OBJ donor");
        string outputPath = Path.GetFullPath(outputArgument);
        byte[] targetBytes = File.ReadAllBytes(targetPath);
        byte[] donorBytes = File.ReadAllBytes(donorPath);

        SmoDocument target = SmoDocument.Load(targetPath);
        ImportedScene donor = ImportedModelReader.ReadGeometryOnly(donorPath);
        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
        SmoExportScene targetScene = BuildTargetScene(target);
        var alignment = new ReplacementTransform(
            DaphneScale,
            Vector3.Zero,
            DaphneTranslation);
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
        GeneratedSkinningRegionAnalysis analysis = automatic.Analysis.SemanticRegions;
        AssertAllRegionsApplied(analysis, "Daphne");
        GeneratedSkinningPreparationResult disabled = PrepareAllDisabled(
            target,
            donor,
            rig,
            fit.Pose,
            alignment,
            fit.BodySelection,
            analysis);

        Console.WriteLine("DAPHNE SEMANTIC COVERAGE: " + DescribeRegions(analysis));
        AssertSemanticWeightContract(automatic, analysis, "Daphne auto");
        AssertDaphneHandCoverage(
            donor,
            fit.BodySelection,
            automatic,
            disabled,
            analysis);
        AssertDaphneLegacyContamination(disabled, automatic, analysis);
        AssertOutsideBitIdentity(automatic, disabled, analysis, "Daphne auto");
        AssertDaphneCounts(automatic);
        if (!string.IsNullOrWhiteSpace(animationDirectoryArgument))
        {
            AssertRigidCoreAnimationResidual(
                target,
                automatic,
                analysis,
                Path.GetFullPath(animationDirectoryArgument));
        }

        if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBytes) ||
            !File.ReadAllBytes(donorPath).SequenceEqual(donorBytes))
        {
            throw new InvalidOperationException(
                "Daphne semantic validation mutated a source file.");
        }

        // Reuse the established end-to-end Daphne oracle instead of duplicating
        // atlas, opacity, eye-branch and strict-writer checks here. Its Prepare
        // call intentionally uses the old overload, which now auto-enables safe
        // semantic regions.
        DaphneMode3Regression.Run(targetPath, donorPath, outputPath);
        Console.WriteLine(
            "DAPHNE SEMANTIC REGION PASS: " + DescribeRegions(analysis) +
            "; 1701 vertices/2158 triangles; Thigh/Spine/Bicep contamination " +
            "removed; atlas/1979+0+179 opacity/eye branch preserved" +
            (string.IsNullOrWhiteSpace(animationDirectoryArgument)
                ? "."
                : "; blidme/blwalk/blru Head-core residual <1e-4."));
    }

    private static SmoExportScene BuildTargetScene(SmoDocument target) =>
        SmoSceneBuilder.Build(
            target,
            new SmoExportOptions(
                ApplyWorldTransforms: true,
                AnimationPaths: null,
                Resources: SmoExportResourceTypes.Meshes |
                           SmoExportResourceTypes.Skeleton));

    private static ImportedScene BuildIdentifierAgnosticDonor(
        SmoExportScene scene,
        TargetRigDefinition rig)
    {
        SmoExportMesh[] source = scene.Meshes
            .Where(mesh => mesh.SkinObjectIndex is not null &&
                           mesh.Positions.Length > 0 &&
                           mesh.TriangleIndices.Length > 0)
            .Reverse()
            .ToArray();
        if (source.Length == 0)
            throw new InvalidDataException("Synthetic target has no skinned surface.");

        ImportedMaterial[] materials =
        [
            new("material_delta"),
            new("material_zeta")
        ];
        var positions = new List<Vector3>();
        var normals = new List<Vector3>();
        var textureCoordinates = new List<Vector2>();
        var triangles = new List<(uint A, uint B, uint C)>();
        foreach (SmoExportMesh mesh in source)
        {
            uint offset = checked((uint)positions.Count);
            positions.AddRange(mesh.Positions);
            normals.AddRange(mesh.Normals.Length == mesh.Positions.Length
                ? mesh.Normals
                : Enumerable.Repeat(Vector3.Zero, mesh.Positions.Length));
            textureCoordinates.AddRange(
                mesh.TextureCoordinates0.Length == mesh.Positions.Length
                    ? mesh.TextureCoordinates0
                    : Enumerable.Repeat(Vector2.Zero, mesh.Positions.Length));
            uint[] reversed = ReverseTriangleOrder(mesh.TriangleIndices);
            for (int index = 0; index < reversed.Length; index += 3)
            {
                triangles.Add((
                    checked(offset + reversed[index]),
                    checked(offset + reversed[index + 1]),
                    checked(offset + reversed[index + 2])));
            }
        }

        // This fixture used to duplicate the complete position array into three
        // meshes and add one long fan triangle for every source triangle. The
        // semantic suite invokes Prepare many times, so that accidental stress
        // multiplier could exhaust a workstation without increasing coverage.
        // Keep one anonymous mesh and add only one topology bridge per genuinely
        // disconnected geometric surface. Original vertices and positions stay intact.
        int[] parent = Enumerable.Range(0, positions.Count).ToArray();
        bool[] referenced = new bool[positions.Count];

        int Find(int value)
        {
            int root = value;
            while (parent[root] != root)
                root = parent[root];
            while (parent[value] != value)
            {
                int next = parent[value];
                parent[value] = root;
                value = next;
            }
            return root;
        }

        void UnionIndices(int firstValue, int secondValue)
        {
            int first = Find(firstValue);
            int second = Find(secondValue);
            if (first != second)
                parent[second] = first;
        }

        void Union(uint firstValue, uint secondValue) => UnionIndices(
            checked((int)firstValue), checked((int)secondValue));

        foreach ((uint a, uint b, uint c) in triangles)
        {
            referenced[checked((int)a)] = true;
            referenced[checked((int)b)] = true;
            referenced[checked((int)c)] = true;
            Union(a, b);
            Union(a, c);
        }

        // SMO surfaces may duplicate almost every triangle corner. Reproduce
        // the production topology rule before adding bridges: two raw index
        // components are one geometric surface only after they share at least
        // two exact positions. Without this step a triangle-soup target would
        // still create O(triangle count) long hub bridges.
        var sharedPositionCount = new Dictionary<(int First, int Second), int>();
        foreach (IGrouping<Vector3, int> equalPosition in
                 Enumerable.Range(0, positions.Count)
                     .Where(index => referenced[index])
                     .GroupBy(index => positions[index]))
        {
            int[] roots = equalPosition.Select(Find).Distinct().Order().ToArray();
            if (roots.Length > 32)
            {
                throw new InvalidDataException(
                    "Synthetic donor has an ambiguous exact-position seam.");
            }
            for (int first = 0; first < roots.Length; first++)
            for (int second = first + 1; second < roots.Length; second++)
            {
                (int First, int Second) key = (roots[first], roots[second]);
                sharedPositionCount[key] =
                    sharedPositionCount.GetValueOrDefault(key) + 1;
            }
        }
        foreach (KeyValuePair<(int First, int Second), int> pair in
                 sharedPositionCount)
        {
            if (pair.Value >= 2)
                UnionIndices(pair.Key.First, pair.Key.Second);
        }

        uint hub = checked((uint)Enumerable.Range(0, positions.Count)
            .Where(index => referenced[index])
            .OrderBy(index => positions[index].LengthSquared())
            .ThenBy(index => index)
            .First());
        int hubRoot = Find(checked((int)hub));
        (uint A, uint B, uint C)[] sourceTriangles = triangles.ToArray();
        IGrouping<int, (uint A, uint B, uint C)>[] geometricComponents =
            sourceTriangles
                .GroupBy(triangle => Find(checked((int)triangle.A)))
                .OrderBy(group => group.Key)
                .ToArray();
        if (geometricComponents.Length > 64)
        {
            throw new InvalidDataException(
                $"Synthetic donor refused {geometricComponents.Length} geometric " +
                "components; the bounded fixture allows at most 64.");
        }
        foreach (IGrouping<int, (uint A, uint B, uint C)> component in
                 geometricComponents)
        {
            if (component.Key == hubRoot)
                continue;
            (uint First, uint Second, float Area) best = component
                .SelectMany(triangle => new[]
                {
                    (triangle.A, triangle.B),
                    (triangle.B, triangle.C),
                    (triangle.C, triangle.A)
                })
                .Select(edge => (edge.Item1, edge.Item2, Area: Vector3.Cross(
                    positions[checked((int)edge.Item1)] -
                    positions[checked((int)hub)],
                    positions[checked((int)edge.Item2)] -
                    positions[checked((int)hub)]).LengthSquared()))
                .OrderByDescending(value => value.Area)
                .ThenBy(value => value.Item1)
                .ThenBy(value => value.Item2)
                .First();
            if (hub == best.First || hub == best.Second ||
                best.Area <= 0.000000000001f)
            {
                throw new InvalidDataException(
                    "Synthetic donor could not build a finite component bridge.");
            }
            triangles.Add((hub, best.First, best.Second));
        }

        uint[] shuffled = triangles.AsEnumerable().Reverse()
            .SelectMany(triangle => new[] { triangle.A, triangle.B, triangle.C })
            .ToArray();
        Vector3[] semanticAnchors = new[] { "Head", "L_Hand", "R_Hand" }
            .Select(name => Vector3.Transform(
                Vector3.Zero,
                rig.Joints[rig.GetJointIndex(name)].BindWorldMatrix))
            .ToArray();
        (uint A, uint B, uint C)[] seamTriangles = semanticAnchors
            .Select(anchor => sourceTriangles
                .OrderBy(triangle => MathF.Max(
                    Vector3.DistanceSquared(
                        positions[checked((int)triangle.A)], anchor),
                    MathF.Max(
                        Vector3.DistanceSquared(
                            positions[checked((int)triangle.B)], anchor),
                        Vector3.DistanceSquared(
                            positions[checked((int)triangle.C)], anchor))))
                .ThenBy(triangle => triangle.A)
                .ThenBy(triangle => triangle.B)
                .ThenBy(triangle => triangle.C)
                .First())
            .Distinct()
            .ToArray();
        var seamPositions = new List<Vector3>();
        var seamNormals = new List<Vector3>();
        var seamTextureCoordinates = new List<Vector2>();
        var seamIndices = new List<uint>();
        foreach ((uint a, uint b, uint c) in seamTriangles)
        {
            foreach (uint sourceVertex in new[] { a, b, c })
            {
                seamIndices.Add(checked((uint)seamPositions.Count));
                seamPositions.Add(positions[checked((int)sourceVertex)]);
                seamNormals.Add(normals[checked((int)sourceVertex)]);
                seamTextureCoordinates.Add(
                    textureCoordinates[checked((int)sourceVertex)]);
            }
        }
        ImportedMesh[] meshes =
        [
            new ImportedMesh(
                "anonymous_surface_01",
                positions.ToArray(),
                normals.ToArray(),
                textureCoordinates.ToArray(),
                shuffled,
                DiffuseColorsArgb: null,
                MaterialIndex: 1,
                Skinning: null),
            new ImportedMesh(
                "anonymous_surface_00",
                seamPositions.ToArray(),
                seamNormals.ToArray(),
                seamTextureCoordinates.ToArray(),
                seamIndices.ToArray(),
                DiffuseColorsArgb: null,
                MaterialIndex: 0,
                Skinning: null)
        ];
        return new ImportedScene(
            Array.AsReadOnly(meshes),
            EmbeddedTextures: null,
            Array.AsReadOnly(materials));
    }

    private static BoundedHeadFixture BuildBoundedHeadFixture(
        SmoDocument target,
        TargetRigDefinition rig,
        TargetRigFittingPoseSnapshot pose)
    {
        Vector3[] deformBindPositions = rig.Joints
            .Where(joint => joint.IsDeformJoint)
            .Select(joint => Vector3.Transform(
                Vector3.Zero,
                joint.BindWorldMatrix))
            .ToArray();
        if (deformBindPositions.Length == 0)
            throw new InvalidDataException("Bounded head fixture found no deform joints.");
        float referenceHeight =
            deformBindPositions.Max(position => position.Y) -
            deformBindPositions.Min(position => position.Y);
        if (!float.IsFinite(referenceHeight) || referenceHeight <= 0.001f)
            throw new InvalidDataException("Bounded head fixture has no finite rig height.");

        Vector3 JointPosition(string name) => Vector3.Transform(
            Vector3.Zero,
            pose.WorldMatrices[rig.GetJointIndex(name)]);

        Vector3 neck = JointPosition("Neck");
        Vector3 head = JointPosition("Head");
        Vector3 axial = head - neck;
        if (!float.IsFinite(axial.LengthSquared()) ||
            axial.LengthSquared() <= 0.000000000001f)
        {
            throw new InvalidDataException(
                "Bounded head fixture has a degenerate Head/Neck axis.");
        }
        axial = Vector3.Normalize(axial);

        Vector3 lateral = JointPosition("L_Clavicle") -
                          JointPosition("R_Clavicle");
        lateral -= axial * Vector3.Dot(lateral, axial);
        if (!float.IsFinite(lateral.LengthSquared()) ||
            lateral.LengthSquared() <= 0.000000000001f)
        {
            lateral = Vector3.UnitX - axial * Vector3.Dot(Vector3.UnitX, axial);
        }
        if (!float.IsFinite(lateral.LengthSquared()) ||
            lateral.LengthSquared() <= 0.000000000001f)
        {
            lateral = Vector3.UnitZ - axial * Vector3.Dot(Vector3.UnitZ, axial);
        }
        if (!float.IsFinite(lateral.LengthSquared()) ||
            lateral.LengthSquared() <= 0.000000000001f)
        {
            throw new InvalidDataException(
                "Bounded head fixture could not build a lateral axis.");
        }
        lateral = Vector3.Normalize(lateral);
        Vector3 forward = Vector3.Normalize(Vector3.Cross(lateral, axial));

        const int ringSegments = 8;
        var bodyPositions = new List<Vector3>();
        var bodyTriangles = new List<uint>();

        int AddRing(Vector3 center, float lateralRadius, float forwardRadius)
        {
            int start = bodyPositions.Count;
            for (int segment = 0; segment < ringSegments; segment++)
            {
                float angle = MathF.Tau * segment / ringSegments;
                bodyPositions.Add(
                    center +
                    lateral * (MathF.Cos(angle) * lateralRadius) +
                    forward * (MathF.Sin(angle) * forwardRadius));
            }
            return start;
        }

        void AddTriangle(int first, int second, int third)
        {
            bodyTriangles.Add(checked((uint)first));
            bodyTriangles.Add(checked((uint)second));
            bodyTriangles.Add(checked((uint)third));
        }

        int bottom = bodyPositions.Count;
        bodyPositions.Add(neck - axial * (referenceHeight * 0.16f));
        float[] axialOffsets = [-0.14f, -0.045f, -0.01f, 0.015f];
        float[] lateralRadii = [0.015f, 0.010f, 0.010f, 0.012f];
        float[] forwardRadii = [0.014f, 0.009f, 0.009f, 0.011f];
        var ringStarts = new List<int>();
        for (int ring = 0; ring < axialOffsets.Length; ring++)
        {
            ringStarts.Add(AddRing(
                neck + axial * (referenceHeight * axialOffsets[ring]),
                referenceHeight * lateralRadii[ring],
                referenceHeight * forwardRadii[ring]));
        }
        ringStarts.Add(AddRing(
            head - axial * (referenceHeight * 0.002f),
            referenceHeight * 0.052f,
            referenceHeight * 0.072f));
        ringStarts.Add(AddRing(
            head - axial * (referenceHeight * 0.0014f),
            referenceHeight * 0.054f,
            referenceHeight * 0.075f));
        ringStarts.Add(AddRing(
            head + axial * (referenceHeight * 0.005f),
            referenceHeight * 0.055f,
            referenceHeight * 0.077f));
        ringStarts.Add(AddRing(
            head + axial * (referenceHeight * 0.062f),
            referenceHeight * 0.060f,
            referenceHeight * 0.085f));

        for (int segment = 0; segment < ringSegments; segment++)
        {
            int next = (segment + 1) % ringSegments;
            AddTriangle(bottom, ringStarts[0] + next, ringStarts[0] + segment);
        }
        for (int ring = 0; ring + 1 < ringStarts.Count; ring++)
        for (int segment = 0; segment < ringSegments; segment++)
        {
            int next = (segment + 1) % ringSegments;
            AddTriangle(
                ringStarts[ring] + segment,
                ringStarts[ring] + next,
                ringStarts[ring + 1] + segment);
            AddTriangle(
                ringStarts[ring] + next,
                ringStarts[ring + 1] + next,
                ringStarts[ring + 1] + segment);
        }
        int top = bodyPositions.Count;
        bodyPositions.Add(head + axial * (referenceHeight * 0.105f));
        for (int segment = 0; segment < ringSegments; segment++)
        {
            int next = (segment + 1) % ringSegments;
            AddTriangle(
                ringStarts[^1] + segment,
                ringStarts[^1] + next,
                top);
        }

        var faceGrid = new int[3, 3];
        var faceTipVertices = new List<int>();
        for (int row = 0; row < 3; row++)
        for (int column = 0; column < 3; column++)
        {
            int vertex = bodyPositions.Count;
            faceGrid[row, column] = vertex;
            faceTipVertices.Add(vertex);
            bodyPositions.Add(
                head +
                axial * (referenceHeight * (0.018f + row * 0.028f)) +
                lateral * (referenceHeight * ((column - 1) * 0.026f)) +
                forward * (referenceHeight * 0.094f));
        }
        for (int row = 0; row < 2; row++)
        for (int column = 0; column < 2; column++)
        {
            AddTriangle(
                faceGrid[row, column],
                faceGrid[row, column + 1],
                faceGrid[row + 1, column]);
            AddTriangle(
                faceGrid[row, column + 1],
                faceGrid[row + 1, column + 1],
                faceGrid[row + 1, column]);
        }
        int forwardRingVertex = ringStarts[^2] + ringSegments / 4;
        AddTriangle(
            forwardRingVertex,
            faceGrid[0, 0],
            faceGrid[0, 1]);
        AddTriangle(
            forwardRingVertex,
            faceGrid[0, 1],
            faceGrid[0, 2]);

        // These two symmetric face shells belong to the same indexed owner as
        // the primary head, but their only topological bridge reaches ring #3,
        // below the target-derived Head cut. Consequently the induced surface
        // above the cut has three seeded clusters even though the full mesh has
        // one connected component. This is the exact topology that used to
        // leave one half of a donor face on legacy weights.
        var secondaryFaceVertices = new List<int>();
        var secondaryFaceBoundaryVertices = new List<int>();
        foreach (int side in new[] { -1, 1 })
        {
            int lowerInner = bodyPositions.Count;
            bodyPositions.Add(
                head + axial * (referenceHeight * 0.054f) +
                lateral * (referenceHeight * side * 0.038f) +
                forward * (referenceHeight * 0.052f));
            int lowerOuter = bodyPositions.Count;
            bodyPositions.Add(
                head + axial * (referenceHeight * 0.054f) +
                lateral * (referenceHeight * side * 0.041f) +
                forward * (referenceHeight * 0.052f));
            int upperInner = bodyPositions.Count;
            bodyPositions.Add(
                head + axial * (referenceHeight * 0.060f) +
                lateral * (referenceHeight * side * 0.038f) +
                forward * (referenceHeight * 0.052f));
            int upperOuter = bodyPositions.Count;
            bodyPositions.Add(
                head + axial * (referenceHeight * 0.060f) +
                lateral * (referenceHeight * side * 0.041f) +
                forward * (referenceHeight * 0.052f));
            secondaryFaceVertices.AddRange(
                [lowerInner, lowerOuter, upperInner, upperOuter]);
            secondaryFaceBoundaryVertices.AddRange([lowerInner, lowerOuter]);

            AddTriangle(lowerInner, lowerOuter, upperInner);
            AddTriangle(lowerOuter, upperOuter, upperInner);
            int belowCutBridge = ringStarts[3] +
                                 (side < 0 ? ringSegments / 2 : 0);
            AddTriangle(belowCutBridge, lowerInner, lowerOuter);
        }

        var detailPositions = new List<Vector3>();
        var detailTriangles = new List<uint>();
        var detailVerticesByComponent =
            new Dictionary<int, IReadOnlyList<int>>();

        void AddDetailTriangle(int first, int second, int third)
        {
            detailTriangles.Add(checked((uint)first));
            detailTriangles.Add(checked((uint)second));
            detailTriangles.Add(checked((uint)third));
        }

        void AddTetrahedron(int componentIndex, Vector3 center)
        {
            int start = detailPositions.Count;
            float radius = referenceHeight * 0.0024f;
            detailPositions.Add(center + (lateral + axial + forward) * radius);
            detailPositions.Add(center + (-lateral - axial + forward) * radius);
            detailPositions.Add(center + (-lateral + axial - forward) * radius);
            detailPositions.Add(center + (lateral - axial - forward) * radius);
            AddDetailTriangle(start, start + 1, start + 2);
            AddDetailTriangle(start, start + 3, start + 1);
            AddDetailTriangle(start, start + 2, start + 3);
            AddDetailTriangle(start + 1, start + 3, start + 2);
            detailVerticesByComponent.Add(
                componentIndex,
                Array.AsReadOnly(Enumerable.Range(start, 4).ToArray()));
        }

        Vector3 companionOffset = forward * (referenceHeight * 0.004f);
        // Components #1/#2 are the two eyes, #3 is the teeth cluster and #4
        // is the compact forehead tuft. Names and materials remain anonymous;
        // only their proved geometric relationship to the smooth head matters.
        AddTetrahedron(1, bodyPositions[faceGrid[1, 0]] + companionOffset);
        AddTetrahedron(2, bodyPositions[faceGrid[1, 2]] + companionOffset);
        AddTetrahedron(3, bodyPositions[faceGrid[0, 1]] + companionOffset);
        AddTetrahedron(
            4,
            bodyPositions[faceGrid[2, 1]] + companionOffset +
            axial * (referenceHeight * 0.004f));

        const int wingComponentIndex = 5;
        int wingStart = detailPositions.Count;
        Vector3 wingCenter = neck -
                             axial * (referenceHeight * 0.10f) -
                             forward * (referenceHeight * 0.04f);
        detailPositions.Add(
            wingCenter - lateral * (referenceHeight * 0.23f) -
            axial * (referenceHeight * 0.018f));
        detailPositions.Add(
            wingCenter + lateral * (referenceHeight * 0.23f) -
            axial * (referenceHeight * 0.018f));
        detailPositions.Add(
            wingCenter + lateral * (referenceHeight * 0.23f) +
            axial * (referenceHeight * 0.018f));
        detailPositions.Add(
            wingCenter - lateral * (referenceHeight * 0.23f) +
            axial * (referenceHeight * 0.018f));
        AddDetailTriangle(wingStart, wingStart + 1, wingStart + 2);
        AddDetailTriangle(wingStart, wingStart + 2, wingStart + 3);
        detailVerticesByComponent.Add(
            wingComponentIndex,
            Array.AsReadOnly(Enumerable.Range(wingStart, 4).ToArray()));

        ImportedMaterial[] materials = [new("anonymous_bounded_material")];
        ImportedMesh[] meshes =
        [
            new ImportedMesh(
                "bounded_surface_q",
                bodyPositions.ToArray(),
                Enumerable.Repeat(Vector3.UnitY, bodyPositions.Count).ToArray(),
                Enumerable.Repeat(Vector2.Zero, bodyPositions.Count).ToArray(),
                bodyTriangles.ToArray(),
                DiffuseColorsArgb: null,
                MaterialIndex: 0,
                Skinning: null),
            new ImportedMesh(
                "bounded_surface_p",
                detailPositions.ToArray(),
                Enumerable.Repeat(Vector3.UnitY, detailPositions.Count).ToArray(),
                Enumerable.Repeat(Vector2.Zero, detailPositions.Count).ToArray(),
                detailTriangles.ToArray(),
                DiffuseColorsArgb: null,
                MaterialIndex: 0,
                Skinning: null)
        ];
        var donor = new ImportedScene(
            Array.AsReadOnly(meshes),
            EmbeddedTextures: null,
            Array.AsReadOnly(materials));

        Vector3 minimum = bodyPositions.Aggregate(Vector3.Min);
        Vector3 maximum = bodyPositions.Aggregate(Vector3.Max);
        double surfaceArea = 0;
        for (int index = 0; index < bodyTriangles.Count; index += 3)
        {
            Vector3 first = bodyPositions[checked((int)bodyTriangles[index])];
            Vector3 second = bodyPositions[checked((int)bodyTriangles[index + 1])];
            Vector3 third = bodyPositions[checked((int)bodyTriangles[index + 2])];
            surfaceArea += Vector3.Cross(second - first, third - first).Length() * 0.5;
        }
        var bodyMembership = new TargetRigBodyVertexMembership(
            0,
            meshes[0].Name,
            Array.AsReadOnly(Enumerable.Range(0, bodyPositions.Count).ToArray()));
        var selectedBody = new TargetRigSelectedBodyComponent(
            0,
            TargetRigBodyComponentRole.WholeBody,
            Array.AsReadOnly(new[] { bodyMembership }),
            bodyPositions.Distinct().Count(),
            bodyTriangles.Count / 3,
            checked((float)surfaceArea),
            minimum,
            maximum);
        var bodySelection = new TargetRigBodySelection(
            Array.AsReadOnly(new[] { selectedBody }),
            BoundedHeadExpectedComponents,
            BoundedHeadExpectedComponents - 1,
            TargetRigDefinition.ComputeSourceFingerprint(target),
            TargetRigAutomaticPoseFitter.ComputeDonorGeometryFingerprint(donor),
            ReplacementTransform.Identity);

        int[] protectedNeckBoundaryVertices = Enumerable.Range(
            ringStarts[4],
            ringSegments).ToArray();
        int[] torsoVertices = new[] { bottom }
            .Concat(Enumerable.Range(ringStarts[0], ringSegments))
            .ToArray();
        int[] expectedHeadVertices = ringStarts
            .Skip(4)
            .SelectMany(start => Enumerable.Range(start, ringSegments))
            .Concat(new[] { top })
            .Concat(faceTipVertices)
            .Concat(secondaryFaceVertices)
            .Distinct()
            .Order()
            .ToArray();
        int[] faceLandmarkVertices =
        [
            faceGrid[0, 0],
            faceGrid[0, 2],
            faceGrid[1, 1],
            faceGrid[2, 0],
            faceGrid[2, 2]
        ];
        return new BoundedHeadFixture(
            donor,
            bodySelection,
            0,
            1,
            Array.AsReadOnly(faceTipVertices.ToArray()),
            Array.AsReadOnly(protectedNeckBoundaryVertices),
            Array.AsReadOnly(torsoVertices),
            Array.AsReadOnly(expectedHeadVertices),
            Array.AsReadOnly(faceLandmarkVertices),
            Array.AsReadOnly(secondaryFaceVertices.ToArray()),
            Array.AsReadOnly(secondaryFaceBoundaryVertices.ToArray()),
            detailVerticesByComponent,
            wingComponentIndex,
            referenceHeight);
    }

    private static void AssertBoundedHeadFixtureBudget(ImportedScene donor)
    {
        int vertices = donor.Meshes.Sum(mesh => mesh.Positions.Length);
        int triangles = donor.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
        if (donor.Meshes.Count != 2 ||
            vertices > BoundedHeadMaximumVertices ||
            triangles > BoundedHeadMaximumTriangles ||
            donor.Meshes.Any(mesh => mesh.TriangleIndices.Length % 3 != 0))
        {
            throw new InvalidOperationException(
                $"Bounded head fixture exceeded its pre-Prepare budget: meshes=" +
                $"{donor.Meshes.Count}, vertices={vertices}/" +
                $"{BoundedHeadMaximumVertices}, triangles={triangles}/" +
                $"{BoundedHeadMaximumTriangles}.");
        }
        if (vertices != 103 || triangles != 162)
        {
            throw new InvalidOperationException(
                $"Bounded head fixture revision changed unexpectedly: " +
                $"vertices={vertices}, triangles={triangles}.");
        }

        var allPositions = donor.Meshes
            .SelectMany(mesh => mesh.Positions)
            .ToArray();
        if (allPositions.Distinct().Count() != allPositions.Length)
        {
            throw new InvalidOperationException(
                "Bounded head fixture contains an unintended exact-position seam.");
        }

        int[] offsets = new int[donor.Meshes.Count];
        int total = 0;
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            offsets[meshIndex] = total;
            total += donor.Meshes[meshIndex].Positions.Length;
        }
        int[] parent = Enumerable.Range(0, total).ToArray();
        bool[] referenced = new bool[total];

        int Find(int value)
        {
            int root = value;
            while (parent[root] != root)
                root = parent[root];
            while (parent[value] != value)
            {
                int next = parent[value];
                parent[value] = root;
                value = next;
            }
            return root;
        }

        void Union(int first, int second)
        {
            first = Find(first);
            second = Find(second);
            if (first != second)
                parent[second] = first;
        }

        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = donor.Meshes[meshIndex];
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                int first = checked((int)mesh.TriangleIndices[index]);
                int second = checked((int)mesh.TriangleIndices[index + 1]);
                int third = checked((int)mesh.TriangleIndices[index + 2]);
                if ((uint)first >= (uint)mesh.Positions.Length ||
                    (uint)second >= (uint)mesh.Positions.Length ||
                    (uint)third >= (uint)mesh.Positions.Length)
                {
                    throw new InvalidOperationException(
                        "Bounded head fixture contains an out-of-range index.");
                }
                first += offsets[meshIndex];
                second += offsets[meshIndex];
                third += offsets[meshIndex];
                referenced[first] = referenced[second] = referenced[third] = true;
                Union(first, second);
                Union(first, third);
            }
        }
        if (referenced.Any(value => !value))
        {
            throw new InvalidOperationException(
                "Bounded head fixture contains an unreferenced vertex.");
        }
        int components = Enumerable.Range(0, total)
            .Where(index => referenced[index])
            .Select(Find)
            .Distinct()
            .Count();
        if (components != BoundedHeadExpectedComponents)
        {
            throw new InvalidOperationException(
                $"Bounded head fixture has {components} indexed components; " +
                $"expected {BoundedHeadExpectedComponents}.");
        }
    }

    private static uint[] ReverseTriangleOrder(IReadOnlyList<uint> indices)
    {
        if (indices.Count % 3 != 0)
            throw new InvalidDataException("Synthetic source has incomplete triangles.");
        var result = new uint[indices.Count];
        int output = 0;
        for (int triangle = indices.Count / 3 - 1; triangle >= 0; triangle--)
        {
            result[output++] = indices[triangle * 3];
            result[output++] = indices[triangle * 3 + 1];
            result[output++] = indices[triangle * 3 + 2];
        }
        return result;
    }

    private static GeneratedSkinningPreparationResult PrepareAllDisabled(
        SmoDocument target,
        ImportedScene donor,
        TargetRigDefinition rig,
        TargetRigFittingPoseSnapshot pose,
        ReplacementTransform alignment,
        TargetRigBodySelection body,
        GeneratedSkinningRegionAnalysis analysis)
    {
        GeneratedSkinningRegionOverrides disabled = analysis.CreateOverrides(
            Enum.GetValues<GeneratedSkinningSemanticRegion>()
                .Select(region => new GeneratedSkinningRegionAdjustment(
                    region,
                    Enabled: false,
                    AxialOffset: 0,
                    AxialScale: 1,
                    RadialScale: 1))
                .ToArray());
        _ = rig; // Keeps call sites explicit about the target definition in use.
        return GeneratedSkinningPreparer.Prepare(
            target,
            donor,
            pose,
            alignment,
            body,
            disabled);
    }

    private static void AssertAllRegionsApplied(
        GeneratedSkinningRegionAnalysis analysis,
        string context)
    {
        GeneratedSkinningRegionResolution[] regions = analysis.Regions
            .OrderBy(value => value.Region)
            .ToArray();
        if (regions.Length != Enum.GetValues<GeneratedSkinningSemanticRegion>().Length ||
            regions.Any(region => region.Status != GeneratedSkinningRegionStatus.Applied ||
                                  !region.IsEnabled || !region.IsApplied ||
                                  region.AutomaticVolume is null ||
                                  region.ResolvedVolume is null ||
                                  Count(region.CoreVerticesByMesh) == 0 ||
                                  (region.Region == GeneratedSkinningSemanticRegion.Head &&
                                   Count(region.TransitionVerticesByMesh) == 0) ||
                                  (region.Region != GeneratedSkinningSemanticRegion.Head &&
                                   Count(region.TransitionVerticesByMesh) == 0 &&
                                   !region.Warnings.Any(message => message.Contains(
                                       "hard boundary",
                                       StringComparison.OrdinalIgnoreCase)))))
        {
            throw new InvalidOperationException(
                $"{context}: semantic auto calibration did not apply all regions: " +
                DescribeRegions(analysis) + "; " +
                string.Join(" | ", regions.SelectMany(region => region.Warnings)));
        }
        if (new[]
            {
                analysis.TargetRigFingerprint,
                analysis.DonorGeometryFingerprint,
                analysis.AlignmentFingerprint,
                analysis.FittingPoseFingerprint
            }.Any(string.IsNullOrWhiteSpace))
        {
            throw new InvalidOperationException($"{context}: analysis identity is incomplete.");
        }
    }

    private static void AssertHandsAppliedAndUnsafeHeadRejected(
        GeneratedSkinningRegionAnalysis analysis,
        string context)
    {
        GeneratedSkinningRegionResolution[] regions = analysis.Regions
            .OrderBy(value => value.Region)
            .ToArray();
        GeneratedSkinningRegionResolution head = Region(
            analysis,
            GeneratedSkinningSemanticRegion.Head);
        GeneratedSkinningRegionResolution[] hands = regions
            .Where(region => region.Region is
                GeneratedSkinningSemanticRegion.LeftHand or
                GeneratedSkinningSemanticRegion.RightHand)
            .ToArray();
        if (regions.Length != Enum.GetValues<GeneratedSkinningSemanticRegion>().Length ||
            head.Status != GeneratedSkinningRegionStatus.UnsafeCalibration ||
            head.IsApplied ||
            head.CoreVerticesByMesh.Count != 0 ||
            head.TransitionVerticesByMesh.Count != 0 ||
            !head.Warnings.Any(message => message.Contains(
                "bounded neck cut",
                StringComparison.OrdinalIgnoreCase)) ||
            hands.Length != 2 ||
            hands.Any(region =>
                region.Status != GeneratedSkinningRegionStatus.Applied ||
                !region.IsEnabled ||
                !region.IsApplied ||
                region.AutomaticVolume is null ||
                region.ResolvedVolume is null ||
                Count(region.CoreVerticesByMesh) == 0 ||
                Count(region.TransitionVerticesByMesh) == 0))
        {
            throw new InvalidOperationException(
                $"{context}: conservative semantic calibration contract changed: " +
                DescribeRegions(analysis) + "; " +
                string.Join(" | ", regions.SelectMany(region => region.Warnings)));
        }
        if (new[]
            {
                analysis.TargetRigFingerprint,
                analysis.DonorGeometryFingerprint,
                analysis.AlignmentFingerprint,
                analysis.FittingPoseFingerprint
            }.Any(string.IsNullOrWhiteSpace))
        {
            throw new InvalidOperationException($"{context}: analysis identity is incomplete.");
        }
    }

    private static void AssertSemanticWeightContract(
        GeneratedSkinningPreparationResult preparation,
        GeneratedSkinningRegionAnalysis analysis,
        string context)
    {
        foreach (GeneratedSkinningRegionResolution region in analysis.Regions)
        {
            if (!region.IsApplied)
                continue;
        foreach ((string sceneName, ImportedScene scene) in new[]
                 {
                     ("fitting", preparation.FittingPreviewScene),
                     ("canonical", preparation.PreparedScene)
                 })
        {
            ValidateCore(scene, region, context + " " + sceneName);
            ValidateTransition(scene, region, context + " " + sceneName);
        }
        }
    }

    private static void AssertBoundedHeadTopology(
        GeneratedSkinningPreparationResult preparation,
        BoundedHeadFixture fixture)
    {
        GeneratedSkinningRegionAnalysis regions = preparation.Analysis.SemanticRegions;
        GeneratedSkinningRegionResolution head = Region(
            regions,
            GeneratedSkinningSemanticRegion.Head);
        if (!head.IsApplied ||
            head.Status != GeneratedSkinningRegionStatus.Applied ||
            head.ResolvedVolume is null ||
            head.ResolvedVolume.ShapeExponent != 2 ||
            !head.Warnings.Any(message => message.Contains(
                "donor-topology",
                StringComparison.OrdinalIgnoreCase)) ||
            !head.Warnings.Any(message => message.Contains(
                "2 compact secondary face-shell cluster(s)",
                StringComparison.Ordinal)))
        {
            throw new InvalidOperationException(
                "Bounded head fixture did not apply a topology-refined p=2 Head region: " +
                string.Join(" | ", head.Warnings));
        }

        var core = head.CoreVerticesByMesh
            .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                (membership.MeshIndex, Vertex: vertex)))
            .ToHashSet();
        var transition = head.TransitionVerticesByMesh
            .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                (membership.MeshIndex, Vertex: vertex)))
            .ToHashSet();
        int[] exactCapturedHead = core
            .Where(value => value.MeshIndex == fixture.BodyMeshIndex)
            .Select(value => value.Vertex)
            .Distinct()
            .Order()
            .ToArray();
        if (!exactCapturedHead.SequenceEqual(fixture.ExpectedHeadVertices))
        {
            throw new InvalidOperationException(
                "Bounded Head did not capture exactly the complete expected donor " +
                $"lobe: actual={string.Join(",", exactCapturedHead)}; expected=" +
                string.Join(",", fixture.ExpectedHeadVertices));
        }
        int missingFaceCore = fixture.FaceTipVertices.Count(vertex =>
            !core.Contains((fixture.BodyMeshIndex, vertex)));
        int missingSecondaryFaceCore = fixture.SecondaryFaceVertices.Count(vertex =>
            !core.Contains((fixture.BodyMeshIndex, vertex)));
        int capturedTorso = fixture.TorsoVertices.Count(vertex =>
            core.Contains((fixture.BodyMeshIndex, vertex)) ||
            transition.Contains((fixture.BodyMeshIndex, vertex)));
        int missingProtectedNeck =
            fixture.ProtectedNeckBoundaryVertices.Count(vertex =>
                !core.Contains((fixture.BodyMeshIndex, vertex)));
        if (missingFaceCore != 0 ||
            missingSecondaryFaceCore != 0 ||
            capturedTorso != 0 ||
            missingProtectedNeck != 0 ||
            core.Any(value => value.MeshIndex != fixture.BodyMeshIndex) ||
            transition.Count == 0)
        {
            throw new InvalidOperationException(
                "Bounded Head membership did not keep the connected face rigid, " +
                "capture both same-owner secondary shells, keep the torso outside, " +
                "and retain a disjoint external Head+Neck collar: " +
                $"missing primary face core={missingFaceCore}, missing secondary " +
                $"face core={missingSecondaryFaceCore}, captured torso=" +
                $"{capturedTorso}, missing protected neck=" +
                $"{missingProtectedNeck}, total core={core.Count}, " +
                $"total transition={transition.Count}.");
        }

        ValidateCore(preparation.FittingPreviewScene, head, "bounded head fitting");
        ValidateCore(preparation.PreparedScene, head, "bounded head canonical");
        ValidateTransition(
            preparation.FittingPreviewScene,
            head,
            "bounded head fitting");
        ValidateTransition(
            preparation.PreparedScene,
            head,
            "bounded head canonical");
        AssertExactOneHot(
            preparation.FittingPreviewScene,
            fixture.BodyMeshIndex,
            fixture.ExpectedHeadVertices,
            head.AnchorSkeletonJointIndex,
            head.AnchorBoneName,
            "bounded protected Head fitting");
        AssertExactOneHot(
            preparation.PreparedScene,
            fixture.BodyMeshIndex,
            fixture.ExpectedHeadVertices,
            head.AnchorSkeletonJointIndex,
            head.AnchorBoneName,
            "bounded protected Head canonical");

        HashSet<int> protectedBodyVertices = fixture.ExpectedHeadVertices
            .ToHashSet();
        HashSet<int> transitionBodyVertices = transition
            .Where(value => value.MeshIndex == fixture.BodyMeshIndex)
            .Select(value => value.Vertex)
            .ToHashSet();
        if (transitionBodyVertices.Count != transition.Count ||
            transitionBodyVertices.Overlaps(protectedBodyVertices) ||
            fixture.FaceTipVertices.Any(transitionBodyVertices.Contains) ||
            fixture.SecondaryFaceVertices.Any(transitionBodyVertices.Contains) ||
            fixture.TorsoVertices.Any(transitionBodyVertices.Contains))
        {
            throw new InvalidOperationException(
                "Bounded Head transition is not a disjoint exterior neck collar.");
        }
        var bodyAdjacency = Enumerable.Range(
                0,
                fixture.Donor.Meshes[fixture.BodyMeshIndex].Positions.Length)
            .ToDictionary(vertex => vertex, _ => new HashSet<int>());
        IReadOnlyList<uint> bodyIndices =
            fixture.Donor.Meshes[fixture.BodyMeshIndex].TriangleIndices;
        for (int index = 0; index < bodyIndices.Count; index += 3)
        {
            int first = checked((int)bodyIndices[index]);
            int second = checked((int)bodyIndices[index + 1]);
            int third = checked((int)bodyIndices[index + 2]);
            bodyAdjacency[first].Add(second);
            bodyAdjacency[first].Add(third);
            bodyAdjacency[second].Add(first);
            bodyAdjacency[second].Add(third);
            bodyAdjacency[third].Add(first);
            bodyAdjacency[third].Add(second);
        }
        int[] protectedBoundary = protectedBodyVertices
            .Where(vertex => bodyAdjacency[vertex].Any(neighbour =>
                !protectedBodyVertices.Contains(neighbour)))
            .Order()
            .ToArray();
        int[] expectedProtectedBoundary = fixture.ProtectedNeckBoundaryVertices
            .Concat(fixture.SecondaryFaceBoundaryVertices)
            .Distinct()
            .Order()
            .ToArray();
        int[] transitionSeeds = transitionBodyVertices
            .Where(vertex => bodyAdjacency[vertex].Any(
                protectedBodyVertices.Contains))
            .Order()
            .ToArray();
        int[] unguardedProtectedBoundary = protectedBoundary
            .Where(vertex => !bodyAdjacency[vertex].Any(
                transitionBodyVertices.Contains))
            .Order()
            .ToArray();
        if (!protectedBoundary.SequenceEqual(expectedProtectedBoundary) ||
            transitionSeeds.Length == 0 ||
            unguardedProtectedBoundary.Length != 0)
        {
            throw new InvalidOperationException(
                "Bounded Head protected lobe is not separated from the exterior " +
                "capsule path by one topologically adjacent transition collar: " +
                $"protected boundary={string.Join(",", protectedBoundary)}, " +
                $"expected={string.Join(",", expectedProtectedBoundary)}, " +
                $"transition seeds={string.Join(",", transitionSeeds)}, " +
                $"unguarded={string.Join(",", unguardedProtectedBoundary)}.");
        }
        var unreachableTransition = transitionBodyVertices.ToHashSet();
        var transitionQueue = new Queue<int>();
        foreach (int seed in transitionSeeds)
        {
            if (unreachableTransition.Remove(seed))
                transitionQueue.Enqueue(seed);
        }
        while (transitionQueue.Count > 0)
        {
            int current = transitionQueue.Dequeue();
            foreach (int neighbour in bodyAdjacency[current])
            {
                if (unreachableTransition.Remove(neighbour))
                    transitionQueue.Enqueue(neighbour);
            }
        }
        if (unreachableTransition.Count != 0)
        {
            throw new InvalidOperationException(
                "Bounded Head transition contains vertices outside the exterior " +
                "collar connected to the protected lobe: " +
                string.Join(",", unreachableTransition.Order()) + ".");
        }

        int[] expectedCompanions = fixture.DetailVerticesByComponent.Keys
            .Where(component => component != fixture.WingComponentIndex)
            .Order()
            .ToArray();
        if (!head.RigidCompanionComponentIndices.SequenceEqual(expectedCompanions) ||
            preparation.Analysis.DonorComponentCount !=
            BoundedHeadExpectedComponents ||
            preparation.Analysis.Attachments.Count !=
            BoundedHeadExpectedComponents - 1)
        {
            throw new InvalidOperationException(
                "Bounded Head assembly did not resolve exactly four rigid companions " +
                "plus one unrelated attachment.");
        }

        Dictionary<int, GeneratedSkinningAttachment> attachments =
            preparation.Analysis.Attachments.ToDictionary(
                attachment => attachment.ComponentIndex);
        foreach (int componentIndex in expectedCompanions)
        {
            if (!attachments.TryGetValue(
                    componentIndex,
                    out GeneratedSkinningAttachment? attachment) ||
                attachment.SemanticAssignment != GeneratedSkinningSemanticRegion.Head ||
                attachment.ManualAssignment is not null ||
                !string.Equals(
                    attachment.TargetBoneName,
                    head.AnchorBoneName,
                    StringComparison.Ordinal) ||
                attachment.TargetSkeletonJointIndex !=
                head.AnchorSkeletonJointIndex)
            {
                throw new InvalidOperationException(
                    $"Detached face component #{componentIndex} was not proven as " +
                    "an automatic semantic Head companion.");
            }
            AssertExactAttachmentMembership(
                attachment,
                fixture.DetailMeshIndex,
                fixture.DetailVerticesByComponent[componentIndex]);
            AssertAttachmentOneHot(
                preparation.FittingPreviewScene,
                attachment,
                $"bounded companion #{componentIndex} fitting");
            AssertAttachmentOneHot(
                preparation.PreparedScene,
                attachment,
                $"bounded companion #{componentIndex} canonical");
        }

        if (!attachments.TryGetValue(
                fixture.WingComponentIndex,
                out GeneratedSkinningAttachment? wing) ||
            wing.SemanticAssignment is not null ||
            string.Equals(
                wing.TargetBoneName,
                head.AnchorBoneName,
                StringComparison.Ordinal))
        {
            throw new InvalidOperationException(
                "The unrelated long wing was swallowed by the semantic Head assembly.");
        }
        AssertExactAttachmentMembership(
            wing,
            fixture.DetailMeshIndex,
            fixture.DetailVerticesByComponent[fixture.WingComponentIndex]);

        GeneratedSkinningRegionResolution[] unexpectedlyAppliedHands = regions.Regions
            .Where(region => region.Region != GeneratedSkinningSemanticRegion.Head &&
                             region.IsApplied)
            .ToArray();
        int capturedSemanticVertices = regions.Regions.Sum(region =>
            Count(region.CoreVerticesByMesh) + Count(region.TransitionVerticesByMesh));
        if (unexpectedlyAppliedHands.Length != 0 ||
            preparation.Analysis.SemanticRegionAppliedVertexCount !=
            capturedSemanticVertices)
        {
            throw new InvalidOperationException(
                "Bounded head-only fixture unexpectedly applied a hand region or " +
                "reported inconsistent semantic vertex totals.");
        }

        float distanceTolerance = MathF.Max(
            0.00001f,
            fixture.ReferenceHeight * 0.00001f);
        AssertTopologyEdgeLengthsPreserved(
            preparation.FittingPreviewScene,
            preparation.PreparedScene,
            fixture.BodyMeshIndex,
            protectedBodyVertices,
            distanceTolerance,
            "bounded protected Head");
        HashSet<int> companionVertices = expectedCompanions
            .SelectMany(component =>
                fixture.DetailVerticesByComponent[component])
            .ToHashSet();
        AssertTopologyEdgeLengthsPreserved(
            preparation.FittingPreviewScene,
            preparation.PreparedScene,
            fixture.DetailMeshIndex,
            companionVertices,
            distanceTolerance,
            "bounded rigid Head companions");
        var rigidAssembly = protectedBodyVertices
            .Select(vertex => (fixture.BodyMeshIndex, Vertex: vertex))
            .Concat(companionVertices.Select(vertex =>
                (fixture.DetailMeshIndex, Vertex: vertex)))
            .OrderBy(value => value.Item1)
            .ThenBy(value => value.Vertex)
            .ToArray();
        AssertPairwiseDistancesPreserved(
            preparation.FittingPreviewScene,
            preparation.PreparedScene,
            rigidAssembly,
            distanceTolerance,
            "bounded complete rigid Head assembly");
        foreach (int componentIndex in expectedCompanions)
        {
            IReadOnlyList<int> vertices =
                fixture.DetailVerticesByComponent[componentIndex];
            Vector3 fittingCenter = AveragePositions(
                preparation.FittingPreviewScene.Meshes[fixture.DetailMeshIndex],
                vertices);
            Vector3 canonicalCenter = AveragePositions(
                preparation.PreparedScene.Meshes[fixture.DetailMeshIndex],
                vertices);
            foreach (int landmarkVertex in fixture.FaceLandmarkVertices)
            {
                Vector3 fittingFace = preparation.FittingPreviewScene
                    .Meshes[fixture.BodyMeshIndex]
                    .Positions[landmarkVertex];
                Vector3 canonicalFace = preparation.PreparedScene
                    .Meshes[fixture.BodyMeshIndex]
                    .Positions[landmarkVertex];
                float fittingDistance = Vector3.Distance(
                    fittingFace,
                    fittingCenter);
                float canonicalDistance = Vector3.Distance(
                    canonicalFace,
                    canonicalCenter);
                if (MathF.Abs(fittingDistance - canonicalDistance) >
                    distanceTolerance)
                {
                    throw new InvalidOperationException(
                        $"Detached face component #{componentIndex} drifted " +
                        $"relative to face landmark {landmarkVertex}: fitting=" +
                        $"{fittingDistance:G9}, canonical=" +
                        $"{canonicalDistance:G9}.");
                }
            }
        }
    }

    private static void AssertExactAttachmentMembership(
        GeneratedSkinningAttachment attachment,
        int expectedMesh,
        IReadOnlyList<int> expectedVertices)
    {
        if (attachment.VerticesByMesh.Count != 1 ||
            attachment.VerticesByMesh[0].MeshIndex != expectedMesh ||
            !attachment.VerticesByMesh[0].VertexIndices.SequenceEqual(
                expectedVertices.Order()))
        {
            throw new InvalidOperationException(
                $"Attachment #{attachment.ComponentIndex} changed exact donor membership.");
        }
    }

    private static void AssertExactOneHot(
        ImportedScene scene,
        int meshIndex,
        IReadOnlyList<int> vertices,
        int expectedJoint,
        string expectedBone,
        string context)
    {
        ImportedSkinning skinning = scene.Meshes[meshIndex].Skinning ??
            throw new InvalidOperationException(
                $"{context}: protected mesh has no skinning.");
        if (expectedJoint < 0 ||
            expectedJoint >= skinning.Skeleton.JointNames.Count ||
            !string.Equals(
                skinning.Skeleton.JointNames[expectedJoint],
                expectedBone,
                StringComparison.Ordinal))
        {
            throw new InvalidOperationException(
                $"{context}: joint {expectedJoint} is not '{expectedBone}'.");
        }
        foreach (int vertex in vertices)
        {
            ImportedJointIndices joints = skinning.JointIndices[vertex];
            Vector4 weights = skinning.Weights[vertex];
            if (weights != Vector4.UnitX ||
                joints.X != expectedJoint ||
                joints.Y != 0 || joints.Z != 0 || joints.W != 0)
            {
                throw new InvalidOperationException(
                    $"{context}: protected vertex {vertex} is not exact raw " +
                    $"one-hot {expectedBone}; a transition or legacy capsule " +
                    "influence leaked inside the rigid region.");
            }
        }
    }

    private static void AssertTopologyEdgeLengthsPreserved(
        ImportedScene fitting,
        ImportedScene canonical,
        int meshIndex,
        IReadOnlySet<int> vertices,
        float tolerance,
        string context)
    {
        ImportedMesh fittingMesh = fitting.Meshes[meshIndex];
        ImportedMesh canonicalMesh = canonical.Meshes[meshIndex];
        if (!fittingMesh.TriangleIndices.SequenceEqual(
                canonicalMesh.TriangleIndices))
        {
            throw new InvalidOperationException(
                $"{context}: fitting-to-canonical bake changed topology.");
        }

        var edges = new HashSet<(int First, int Second)>();
        for (int index = 0; index < fittingMesh.TriangleIndices.Length; index += 3)
        {
            int first = checked((int)fittingMesh.TriangleIndices[index]);
            int second = checked((int)fittingMesh.TriangleIndices[index + 1]);
            int third = checked((int)fittingMesh.TriangleIndices[index + 2]);
            Add(first, second);
            Add(second, third);
            Add(third, first);
        }
        if (edges.Count == 0)
        {
            throw new InvalidOperationException(
                $"{context}: protected topology has no internal edge witness.");
        }
        foreach ((int first, int second) in edges)
        {
            AssertDistancePreserved(
                fittingMesh.Positions[first],
                fittingMesh.Positions[second],
                canonicalMesh.Positions[first],
                canonicalMesh.Positions[second],
                tolerance,
                $"{context} edge {first}-{second}");
        }
        return;

        void Add(int first, int second)
        {
            if (!vertices.Contains(first) || !vertices.Contains(second) ||
                first == second)
            {
                return;
            }
            edges.Add(first < second ? (first, second) : (second, first));
        }
    }

    private static void AssertPairwiseDistancesPreserved(
        ImportedScene fitting,
        ImportedScene canonical,
        IReadOnlyList<(int MeshIndex, int Vertex)> vertices,
        float tolerance,
        string context)
    {
        if (vertices.Count < 2)
        {
            throw new InvalidOperationException(
                $"{context}: rigid assembly has no pairwise witness.");
        }
        for (int firstIndex = 0; firstIndex < vertices.Count; firstIndex++)
        for (int secondIndex = firstIndex + 1;
             secondIndex < vertices.Count;
             secondIndex++)
        {
            (int firstMesh, int firstVertex) = vertices[firstIndex];
            (int secondMesh, int secondVertex) = vertices[secondIndex];
            AssertDistancePreserved(
                fitting.Meshes[firstMesh].Positions[firstVertex],
                fitting.Meshes[secondMesh].Positions[secondVertex],
                canonical.Meshes[firstMesh].Positions[firstVertex],
                canonical.Meshes[secondMesh].Positions[secondVertex],
                tolerance,
                $"{context} pair m{firstMesh}:v{firstVertex}-" +
                $"m{secondMesh}:v{secondVertex}");
        }
    }

    private static void AssertDistancePreserved(
        Vector3 fittingFirst,
        Vector3 fittingSecond,
        Vector3 canonicalFirst,
        Vector3 canonicalSecond,
        float tolerance,
        string context)
    {
        float fittingDistance = Vector3.Distance(fittingFirst, fittingSecond);
        float canonicalDistance = Vector3.Distance(canonicalFirst, canonicalSecond);
        if (!float.IsFinite(fittingDistance) ||
            !float.IsFinite(canonicalDistance) ||
            MathF.Abs(fittingDistance - canonicalDistance) > tolerance)
        {
            throw new InvalidOperationException(
                $"{context}: fitting-to-canonical rigid distance changed from " +
                $"{fittingDistance:G9} to {canonicalDistance:G9} " +
                $"(tolerance {tolerance:G9}).");
        }
    }

    private static void AssertAttachmentOneHot(
        ImportedScene scene,
        GeneratedSkinningAttachment attachment,
        string context)
    {
        foreach ((ImportedMesh mesh, int vertex) in
                 Enumerate(scene, attachment.VerticesByMesh))
        {
            ImportedSkinning skinning = mesh.Skinning ??
                throw new InvalidOperationException(
                    $"{context}: attachment mesh has no skinning.");
            ImportedJointIndices joints = skinning.JointIndices[vertex];
            Vector4 weights = skinning.Weights[vertex];
            if (weights != Vector4.UnitX ||
                joints.X != attachment.TargetSkeletonJointIndex ||
                joints.Y != 0 || joints.Z != 0 || joints.W != 0)
            {
                throw new InvalidOperationException(
                    $"{context}: vertex {vertex} is not exact one-hot " +
                    $"{attachment.TargetBoneName}.");
            }
        }
    }

    private static Vector3 AveragePositions(
        ImportedMesh mesh,
        IReadOnlyList<int> vertices)
    {
        if (vertices.Count == 0)
            throw new InvalidOperationException("Cannot average an empty fixture component.");
        Vector3 sum = Vector3.Zero;
        foreach (int vertex in vertices)
            sum += mesh.Positions[vertex];
        return sum / vertices.Count;
    }

    private static void ValidateCore(
        ImportedScene scene,
        GeneratedSkinningRegionResolution region,
        string context)
    {
        foreach ((ImportedMesh mesh, int vertex) in Enumerate(scene, region.CoreVerticesByMesh))
        {
            ImportedSkinning skin = mesh.Skinning ?? throw new InvalidOperationException(
                $"{context}: mesh '{mesh.Name}' has no skinning.");
            ImportedJointIndices joints = skin.JointIndices[vertex];
            Vector4 weights = skin.Weights[vertex];
            bool articulatedHand =
                region.Region != GeneratedSkinningSemanticRegion.Head &&
                region.MotionBranchBoneNames.Count >= 2;
            if (!articulatedHand)
            {
                if (weights != Vector4.UnitX ||
                    joints.X != region.AnchorSkeletonJointIndex ||
                    joints.Y != 0 || joints.Z != 0 || joints.W != 0 ||
                    !string.Equals(
                        skin.Skeleton.JointNames[joints.X],
                        region.AnchorBoneName,
                        StringComparison.Ordinal))
                {
                    throw new InvalidOperationException(
                        $"{context}: {region.Region} core " +
                        $"[{mesh.Name}:{vertex}] is not exact one-hot " +
                        $"{region.AnchorBoneName}.");
                }
                continue;
            }

            (int Joint, float Weight)[] active = new[]
                {
                    (Joint: (int)joints.X, Weight: weights.X),
                    (Joint: (int)joints.Y, Weight: weights.Y),
                    (Joint: (int)joints.Z, Weight: weights.Z),
                    (Joint: (int)joints.W, Weight: weights.W)
                }
                .Where(value => value.Weight > 0.000001f)
                .ToArray();
            float total = active.Sum(value => value.Weight);
            IReadOnlySet<int> handSubtree = Descendants(
                skin.Skeleton, region.AnchorSkeletonJointIndex);
            IReadOnlyList<int> parents = skin.Skeleton.ParentJointIndices ??
                throw new InvalidOperationException(
                    $"{context}: generated Hand skeleton has no hierarchy.");
            bool adjacent = active.Length < 2 ||
                parents[active[0].Joint] == active[1].Joint ||
                parents[active[1].Joint] == active[0].Joint;
            if (active.Length is < 1 or > 2 ||
                !float.IsFinite(total) || MathF.Abs(total - 1) > 0.00001f ||
                active.Any(value => !float.IsFinite(value.Weight) ||
                    value.Weight < 0 ||
                    !handSubtree.Contains(value.Joint)) ||
                !adjacent)
            {
                throw new InvalidOperationException(
                    $"{context}: {region.Region} core [{mesh.Name}:{vertex}] " +
                    "is not a bounded adjacent Hand/finger-chain profile.");
            }
        }
    }

    private static void ValidateTransition(
        ImportedScene scene,
        GeneratedSkinningRegionResolution region,
        string context)
    {
        if (Count(region.TransitionVerticesByMesh) == 0)
        {
            if (region.Region == GeneratedSkinningSemanticRegion.Head ||
                !region.Warnings.Any(message => message.Contains(
                    "hard boundary",
                    StringComparison.OrdinalIgnoreCase)))
            {
                throw new InvalidOperationException(
                    $"{context}: {region.Region} has no diagnosed transition.");
            }
            return;
        }
        int mixed = 0;
        foreach ((ImportedMesh mesh, int vertex) in
                 Enumerate(scene, region.TransitionVerticesByMesh))
        {
            ImportedSkinning skin = mesh.Skinning ?? throw new InvalidOperationException(
                $"{context}: mesh '{mesh.Name}' has no skinning.");
            ImportedJointIndices joints = skin.JointIndices[vertex];
            Vector4 weights = skin.Weights[vertex];
            ushort[] indices = [joints.X, joints.Y, joints.Z, joints.W];
            float[] values = [weights.X, weights.Y, weights.Z, weights.W];
            float sum = 0;
            var active = new HashSet<int>();
            for (int slot = 0; slot < 4; slot++)
            {
                if (values[slot] <= 0)
                    continue;
                sum += values[slot];
                active.Add(indices[slot]);
            }
            if (active.Count == 2)
                mixed++;
            if (MathF.Abs(sum - 1) > 0.000001f ||
                active.Count == 0 || active.Count > 2 ||
                active.Any(index => index != region.AnchorSkeletonJointIndex &&
                                    index != region.ProximalSkeletonJointIndex))
            {
                throw new InvalidOperationException(
                    $"{context}: {region.Region} transition [{mesh.Name}:{vertex}] " +
                    "contains non-proximal weight mass.");
            }
        }
        if (mixed == 0)
        {
            throw new InvalidOperationException(
                $"{context}: {region.Region} transition has no genuine two-bone " +
                $"blend. Diagnostics: {string.Join(" | ", region.Warnings)}");
        }
    }

    private static void AssertOutsideBitIdentity(
        GeneratedSkinningPreparationResult automatic,
        GeneratedSkinningPreparationResult disabled,
        GeneratedSkinningRegionAnalysis analysis,
        string context)
    {
        HashSet<(int Mesh, int Vertex)> captured = analysis.Regions
            .SelectMany(region => region.CoreVerticesByMesh
                .Concat(region.TransitionVerticesByMesh))
            .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                (membership.MeshIndex, vertex)))
            .ToHashSet();
        captured.UnionWith(automatic.Analysis.Attachments
            .Where(attachment => attachment.SemanticAssignment is not null)
            .SelectMany(attachment => attachment.VerticesByMesh)
            .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                (membership.MeshIndex, vertex))));
        foreach ((string sceneName, ImportedScene actual, ImportedScene baseline) in
                 new[]
                 {
                     ("fitting", automatic.FittingPreviewScene, disabled.FittingPreviewScene),
                     ("canonical", automatic.PreparedScene, disabled.PreparedScene)
                 })
        {
            if (actual.Meshes.Count != baseline.Meshes.Count)
                throw new InvalidOperationException($"{context}: {sceneName} mesh count changed.");
            int compared = 0;
            for (int meshIndex = 0; meshIndex < actual.Meshes.Count; meshIndex++)
            {
                ImportedSkinning a = actual.Meshes[meshIndex].Skinning ??
                    throw new InvalidOperationException($"{context}: actual mesh is unskinned.");
                ImportedSkinning b = baseline.Meshes[meshIndex].Skinning ??
                    throw new InvalidOperationException($"{context}: baseline mesh is unskinned.");
                for (int vertex = 0; vertex < actual.Meshes[meshIndex].Positions.Length; vertex++)
                {
                    if (captured.Contains((meshIndex, vertex)))
                        continue;
                    compared++;
                    if (a.JointIndices[vertex] != b.JointIndices[vertex] ||
                        !SameBits(a.Weights[vertex], b.Weights[vertex]))
                    {
                        throw new InvalidOperationException(
                            $"{context}: outside vertex [{meshIndex}:{vertex}] changed " +
                            $"in the {sceneName} scene: auto=" +
                            DescribeInfluences(a, vertex) + "; disabled=" +
                            DescribeInfluences(b, vertex) + ".");
                    }
                }
            }
            if (compared == 0)
                throw new InvalidOperationException($"{context}: no outside vertices compared.");
        }
    }

    private static void AssertSeamConsistency(
        ImportedScene donor,
        TargetRigBodySelection body,
        GeneratedSkinningRegionAnalysis analysis)
    {
        var classification = new Dictionary<(int Mesh, int Vertex), string>();
        foreach (GeneratedSkinningRegionResolution region in analysis.Regions)
        {
            AddClassification(region.CoreVerticesByMesh, $"{region.Region}/core");
            AddClassification(region.TransitionVerticesByMesh, $"{region.Region}/transition");
        }

        void AddClassification(
            IReadOnlyList<TargetRigBodyVertexMembership> membership,
            string value)
        {
            foreach (TargetRigBodyVertexMembership mesh in membership)
            foreach (int vertex in mesh.VertexIndices)
                classification.Add((mesh.MeshIndex, vertex), value);
        }

        var selected = body.Components
            .SelectMany(component => component.VerticesByMesh)
            .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                (membership.MeshIndex, Vertex: vertex)))
            .ToArray();
        var seams = selected.GroupBy(value => donor.Meshes[value.MeshIndex].Positions[value.Vertex])
            .Where(group => group.Count() > 1)
            .ToArray();
        int semanticSeams = 0;
        foreach (IGrouping<Vector3, (int MeshIndex, int Vertex)> seam in seams)
        {
            string[] values = seam.Select(vertex => classification.TryGetValue(
                    (vertex.MeshIndex, vertex.Vertex), out string? zone)
                    ? zone
                    : "outside")
                .Distinct(StringComparer.Ordinal)
                .ToArray();
            if (values.Length != 1)
                throw new InvalidOperationException("Coincident seam vertices diverged by region.");
            if (values[0] != "outside")
                semanticSeams++;
        }
        if (seams.Length == 0 || semanticSeams == 0)
        {
            throw new InvalidOperationException(
                $"Synthetic seam oracle is under-sampled: all={seams.Length}, " +
                $"semantic={semanticSeams}.");
        }
    }

    private static void AssertMirroredAdjustments(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot pose,
        ReplacementTransform alignment,
        TargetRigBodySelection body,
        GeneratedSkinningPreparationResult automatic)
    {
        GeneratedSkinningRegionAnalysis baseline = automatic.Analysis.SemanticRegions;
        GeneratedSkinningRegionAdjustment[] adjustments = baseline.Regions.Select(region =>
        {
            if (region.Region == GeneratedSkinningSemanticRegion.Head)
                return new GeneratedSkinningRegionAdjustment(
                    region.Region, false, 0, 1, 1);
            float axialScale = Math.Clamp(
                1.05f,
                region.AdjustmentLimits.MinimumAxialScale,
                region.AdjustmentLimits.MaximumAxialScale);
            float radialScale = Math.Clamp(
                1.05f,
                region.AdjustmentLimits.MinimumRadialScale,
                region.AdjustmentLimits.MaximumRadialScale);
            return new GeneratedSkinningRegionAdjustment(
                region.Region, true, 0, axialScale, radialScale);
        }).ToArray();
        GeneratedSkinningPreparationResult adjusted = GeneratedSkinningPreparer.Prepare(
            target,
            donor,
            pose,
            alignment,
            body,
            baseline.CreateOverrides(adjustments));
        GeneratedSkinningRegionResolution left = Region(
            adjusted.Analysis.SemanticRegions,
            GeneratedSkinningSemanticRegion.LeftHand);
        GeneratedSkinningRegionResolution right = Region(
            adjusted.Analysis.SemanticRegions,
            GeneratedSkinningSemanticRegion.RightHand);
        if (!left.IsApplied || !right.IsApplied ||
            Region(adjusted.Analysis.SemanticRegions, GeneratedSkinningSemanticRegion.Head)
                .Status != GeneratedSkinningRegionStatus.Disabled)
        {
            throw new InvalidOperationException(
                "Mirrored hand edits did not remain independently applied.");
        }
        foreach (GeneratedSkinningRegionResolution value in new[] { left, right })
        {
            GeneratedSkinningRegionVolume auto = value.AutomaticVolume!;
            GeneratedSkinningRegionVolume resolved = value.ResolvedVolume!;
            if (!NearlyEqual(resolved.AxialRadius,
                    auto.AxialRadius * value.Adjustment.AxialScale) ||
                !NearlyEqual(resolved.LateralRadius,
                    auto.LateralRadius * value.Adjustment.RadialScale) ||
                !NearlyEqual(resolved.ForwardRadius,
                    auto.ForwardRadius * value.Adjustment.RadialScale))
            {
                throw new InvalidOperationException(
                    $"{value.Region} edit was not resolved in its bone-local frame.");
            }
        }
        GeneratedSkinningRegionVolume l = left.ResolvedVolume!;
        GeneratedSkinningRegionVolume r = right.ResolvedVolume!;
        if (MathF.Sign(l.Center.X) == MathF.Sign(r.Center.X) ||
            RelativeDifference(l.AxialRadius, r.AxialRadius) > 0.1f ||
            RelativeDifference(l.LateralRadius, r.LateralRadius) > 0.1f ||
            RelativeDifference(l.ForwardRadius, r.ForwardRadius) > 0.1f)
        {
            throw new InvalidOperationException(
                "Mirrored hand volumes lost target-rig bilateral symmetry.");
        }
        AssertSemanticWeightContract(adjusted, adjusted.Analysis.SemanticRegions, "mirror");
    }

    private static void AssertOverrideValidation(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot pose,
        ReplacementTransform alignment,
        TargetRigBodySelection body,
        GeneratedSkinningRegionAnalysis analysis)
    {
        GeneratedSkinningRegionOverrides valid = analysis.CreateOverrides([]);
        ExpectThrows<InvalidOperationException>(() => Prepare(valid with
        {
            TargetRigFingerprint = "stale-target"
        }), "stale target fingerprint");
        ExpectThrows<InvalidOperationException>(() => Prepare(valid with
        {
            DonorGeometryFingerprint = "stale-donor"
        }), "stale donor fingerprint");
        ExpectThrows<InvalidOperationException>(() => Prepare(valid with
        {
            AlignmentFingerprint = "stale-alignment"
        }), "stale alignment fingerprint");
        ExpectThrows<InvalidOperationException>(() => Prepare(valid with
        {
            FittingPoseFingerprint = "stale-pose"
        }), "stale fitting-pose fingerprint");

        GeneratedSkinningRegionAdjustment automatic =
            GeneratedSkinningRegionAdjustment.Automatic(
                GeneratedSkinningSemanticRegion.Head);
        ExpectThrows<InvalidDataException>(() => Prepare(analysis.CreateOverrides(
            [automatic, automatic])), "duplicate semantic edit");
        ExpectThrows<InvalidDataException>(() => Prepare(analysis.CreateOverrides(
            [automatic with { AxialOffset = float.NaN }])), "non-finite semantic edit");
        ExpectThrows<InvalidDataException>(() => Prepare(analysis.CreateOverrides(
            [automatic with { RadialScale = 0 }])), "non-positive semantic scale");

        void Prepare(GeneratedSkinningRegionOverrides value) =>
            _ = GeneratedSkinningPreparer.Prepare(
                target,
                donor,
                pose,
                alignment,
                body,
                value);
    }

    private static void AssertConservativeFallback(
        SmoDocument target,
        ImportedScene donor,
        TargetRigDefinition rig,
        TargetRigFittingPoseSnapshot pose,
        ReplacementTransform alignment,
        GeneratedSkinningPreparationResult reference)
    {
        GeneratedSkinningRegionResolution head = Region(
            reference.Analysis.SemanticRegions,
            GeneratedSkinningSemanticRegion.Head);
        ImportedScene? clipped = null;
        GeneratedSkinningPreparationResult? unsafeResult = null;
        TargetRigBodySelection? clippedBody = null;
        foreach (float expansion in new[] { 1.1f, 1.35f, 1.75f, 2.25f })
        {
            clipped = RemoveTrianglesInside(donor, head.ResolvedVolume!, expansion);
            try
            {
                clippedBody = TargetRigAutomaticPoseFitter.SelectBody(rig, clipped, alignment);
                unsafeResult = GeneratedSkinningPreparer.Prepare(
                    target,
                    clipped,
                    pose,
                    alignment,
                    clippedBody);
            }
            catch (InvalidDataException)
            {
                continue;
            }
            GeneratedSkinningRegionResolution resolution = Region(
                unsafeResult.Analysis.SemanticRegions,
                GeneratedSkinningSemanticRegion.Head);
            if (resolution.Status == GeneratedSkinningRegionStatus.UnsafeCalibration)
                break;
            unsafeResult = null;
        }
        if (clipped is null || clippedBody is null || unsafeResult is null)
        {
            throw new InvalidOperationException(
                "Synthetic empty-capture fixture did not reach conservative fallback.");
        }
        GeneratedSkinningRegionAnalysis unsafeAnalysis = unsafeResult.Analysis.SemanticRegions;
        GeneratedSkinningRegionResolution unsafeHead = Region(
            unsafeAnalysis,
            GeneratedSkinningSemanticRegion.Head);
        if (unsafeHead.IsApplied || unsafeHead.CoreVerticesByMesh.Count != 0 ||
            unsafeHead.TransitionVerticesByMesh.Count != 0 ||
            !unsafeHead.Warnings.Any(message => message.Contains(
                "capture", StringComparison.OrdinalIgnoreCase) ||
                message.Contains("contains no", StringComparison.OrdinalIgnoreCase)))
        {
            throw new InvalidOperationException("Unsafe empty capture was not diagnosed.");
        }
        GeneratedSkinningRegionOverrides onlyUnsafeHead = unsafeAnalysis.CreateOverrides(
        [
            new(GeneratedSkinningSemanticRegion.Head, true, 0, 1, 1),
            new(GeneratedSkinningSemanticRegion.LeftHand, false, 0, 1, 1),
            new(GeneratedSkinningSemanticRegion.RightHand, false, 0, 1, 1)
        ]);
        GeneratedSkinningPreparationResult unsafeOnly = GeneratedSkinningPreparer.Prepare(
            target, clipped, pose, alignment, clippedBody, onlyUnsafeHead);
        GeneratedSkinningPreparationResult allDisabled = PrepareAllDisabled(
            target, clipped, rig, pose, alignment, clippedBody, unsafeAnalysis);
        AssertScenesBitIdentical(
            unsafeOnly.PreparedScene,
            allDisabled.PreparedScene,
            "unsafe empty-capture fallback");
    }

    private static void AssertConservativeOverlap(
        SmoDocument target,
        ImportedScene donor,
        TargetRigDefinition rig,
        ReplacementTransform alignment)
    {
        TargetRigBodySelection body = TargetRigAutomaticPoseFitter.SelectBody(
            rig, donor, alignment);
        TargetRigBodyPoseParameters[] poses =
        [
            new(85, 0, 0, 0, 0, 0),
            new(70, 0, 145, 0, 0, 0),
            new(45, 75, 145, 0, 0, 0),
            new(85, 75, 145, 0, 0, 0)
        ];
        foreach (TargetRigBodyPoseParameters parameters in poses)
        {
            TargetRigFittingPoseSnapshot pose = TargetRigBodyPoseMapper.CreateSnapshot(
                rig, parameters);
            GeneratedSkinningPreparationResult initial;
            try
            {
                initial = GeneratedSkinningPreparer.Prepare(
                    target, donor, pose, alignment, body);
            }
            catch (InvalidDataException)
            {
                continue;
            }
            GeneratedSkinningRegionAdjustment[] maximum = initial.Analysis.SemanticRegions
                .Regions.Select(region => new GeneratedSkinningRegionAdjustment(
                    region.Region,
                    Enabled: true,
                    AxialOffset: 0,
                    AxialScale: region.AdjustmentLimits.MaximumAxialScale,
                    RadialScale: region.AdjustmentLimits.MaximumRadialScale))
                .ToArray();
            GeneratedSkinningPreparationResult enlarged;
            try
            {
                enlarged = GeneratedSkinningPreparer.Prepare(
                    target,
                    donor,
                    pose,
                    alignment,
                    body,
                    initial.Analysis.SemanticRegions.CreateOverrides(maximum));
            }
            catch (InvalidDataException)
            {
                continue;
            }
            GeneratedSkinningRegionResolution[] overlap = enlarged.Analysis.SemanticRegions
                .Regions.Where(region =>
                    region.Status == GeneratedSkinningRegionStatus.UnsafeCalibration &&
                    region.Warnings.Any(message => message.Contains(
                        "overlaps another semantic region",
                        StringComparison.OrdinalIgnoreCase)))
                .ToArray();
            if (overlap.Length < 2)
                continue;

            GeneratedSkinningRegionOverrides isolated = enlarged.Analysis.SemanticRegions
                .CreateOverrides(Enum.GetValues<GeneratedSkinningSemanticRegion>()
                    .Select(region => new GeneratedSkinningRegionAdjustment(
                        region,
                        Enabled: overlap.Any(value => value.Region == region),
                        AxialOffset: 0,
                        AxialScale: overlap.FirstOrDefault(value => value.Region == region)?
                            .Adjustment.AxialScale ?? 1,
                        RadialScale: overlap.FirstOrDefault(value => value.Region == region)?
                            .Adjustment.RadialScale ?? 1))
                    .ToArray());
            GeneratedSkinningPreparationResult unsafeOnly = GeneratedSkinningPreparer.Prepare(
                target, donor, pose, alignment, body, isolated);
            GeneratedSkinningPreparationResult disabled = PrepareAllDisabled(
                target,
                donor,
                rig,
                pose,
                alignment,
                body,
                enlarged.Analysis.SemanticRegions);
            AssertScenesBitIdentical(
                unsafeOnly.PreparedScene,
                disabled.PreparedScene,
                "overlapping-region fallback");
            return;
        }
        throw new InvalidOperationException(
            "Synthetic pose/volume matrix did not exercise overlap fallback.");
    }

    private static ImportedScene RemoveTrianglesInside(
        ImportedScene donor,
        GeneratedSkinningRegionVolume volume,
        float expansion)
    {
        ImportedMesh[] meshes = donor.Meshes.Select(mesh => mesh with
        {
            Positions = mesh.Positions.ToArray(),
            Normals = mesh.Normals.ToArray(),
            TextureCoordinates = mesh.TextureCoordinates.ToArray(),
            TriangleIndices = Enumerable.Range(0, mesh.TriangleIndices.Length / 3)
                .Where(triangle =>
                {
                    uint a = mesh.TriangleIndices[triangle * 3];
                    uint b = mesh.TriangleIndices[triangle * 3 + 1];
                    uint c = mesh.TriangleIndices[triangle * 3 + 2];
                    return Distance(mesh.Positions[checked((int)a)], volume) > expansion &&
                           Distance(mesh.Positions[checked((int)b)], volume) > expansion &&
                           Distance(mesh.Positions[checked((int)c)], volume) > expansion;
                })
                .SelectMany(triangle => new[]
                {
                    mesh.TriangleIndices[triangle * 3],
                    mesh.TriangleIndices[triangle * 3 + 1],
                    mesh.TriangleIndices[triangle * 3 + 2]
                })
                .ToArray(),
            DiffuseColorsArgb = mesh.DiffuseColorsArgb?.ToArray(),
            Skinning = null
        }).ToArray();
        return new ImportedScene(Array.AsReadOnly(meshes), donor.Textures, donor.Materials);
    }

    private static float Distance(Vector3 position, GeneratedSkinningRegionVolume volume)
    {
        Vector3 relative = position - volume.Center;
        float axial = Vector3.Dot(relative, volume.AxialAxis) / volume.AxialRadius;
        float lateral = Vector3.Dot(relative, volume.LateralAxis) / volume.LateralRadius;
        float forward = Vector3.Dot(relative, volume.ForwardAxis) / volume.ForwardRadius;
        float exponent = volume.ShapeExponent;
        float powered = MathF.Pow(MathF.Abs(axial), exponent) +
                        MathF.Pow(MathF.Abs(lateral), exponent) +
                        MathF.Pow(MathF.Abs(forward), exponent);
        return MathF.Pow(powered, 1 / exponent);
    }

    private static void AssertDaphneLegacyContamination(
        GeneratedSkinningPreparationResult disabled,
        GeneratedSkinningPreparationResult automatic,
        GeneratedSkinningRegionAnalysis regions)
    {
        GeneratedSkinningRegionResolution head = Region(
            regions, GeneratedSkinningSemanticRegion.Head);
        GeneratedSkinningRegionResolution left = Region(
            regions, GeneratedSkinningSemanticRegion.LeftHand);
        GeneratedSkinningRegionResolution right = Region(
            regions, GeneratedSkinningSemanticRegion.RightHand);
        float headContamination = Mass(
            disabled.FittingPreviewScene,
            head.CoreVerticesByMesh.Concat(head.TransitionVerticesByMesh),
            new HashSet<string>(
                ["Spine_01", "Spine_02", "Spine_03", "L_Bicep", "R_Bicep"],
                StringComparer.Ordinal));
        float disabledHandForbidden = ForbiddenAnchorMass(
            disabled.FittingPreviewScene,
            left) + ForbiddenAnchorMass(disabled.FittingPreviewScene, right);
        var contaminants = new HashSet<string>(
            ["L_Thigh", "R_Thigh", "Spine_01", "Spine_02", "Spine_03",
             "L_Bicep", "R_Bicep"],
            StringComparer.Ordinal);
        float postOverrideContamination = Mass(
            automatic.FittingPreviewScene,
            regions.Regions.SelectMany(region => region.CoreVerticesByMesh
                .Concat(region.TransitionVerticesByMesh)),
            contaminants);
        if (headContamination <= 0 || disabledHandForbidden <= 0 ||
            MathF.Abs(postOverrideContamination) > 0.000001f)
        {
            throw new InvalidOperationException(
                $"Daphne fixture no longer proves legacy contamination: head=" +
                $"{headContamination:G9}, hand forbidden=" +
                $"{disabledHandForbidden:G9}, post-override Thigh/Spine/Bicep=" +
                $"{postOverrideContamination:G9}.");
        }

        static float ForbiddenAnchorMass(
            ImportedScene scene,
            GeneratedSkinningRegionResolution region)
        {
            float result = 0;
            foreach ((ImportedMesh mesh, int vertex) in Enumerate(
                         scene,
                         region.CoreVerticesByMesh.Concat(region.TransitionVerticesByMesh)))
            {
                result += 1 - MassForJoint(
                    mesh.Skinning!, vertex, region.AnchorSkeletonJointIndex);
            }
            return result;
        }
    }

    private static void AssertDaphneHandCoverage(
        ImportedScene donor,
        TargetRigBodySelection body,
        GeneratedSkinningPreparationResult automatic,
        GeneratedSkinningPreparationResult disabled,
        GeneratedSkinningRegionAnalysis analysis)
    {
        HashSet<(int Mesh, int Vertex)> selected = body.Components
            .SelectMany(component => component.VerticesByMesh)
            .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                (membership.MeshIndex, vertex)))
            .ToHashSet();
        foreach (GeneratedSkinningSemanticRegion semantic in new[]
                 {
                     GeneratedSkinningSemanticRegion.LeftHand,
                     GeneratedSkinningSemanticRegion.RightHand
                 })
        {
            GeneratedSkinningRegionResolution region = Region(analysis, semantic);
            HashSet<(int Mesh, int Vertex)> captured = region.CoreVerticesByMesh
                .Concat(region.TransitionVerticesByMesh)
                .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                    (membership.MeshIndex, vertex)))
                .ToHashSet();
            var branch = new HashSet<(int Mesh, int Vertex)>();
            foreach ((int meshIndex, int vertex) in selected)
            {
                ImportedSkinning skin = disabled.FittingPreviewScene.Meshes[meshIndex]
                    .Skinning ?? throw new InvalidOperationException(
                        "Daphne disabled body mesh is unskinned.");
                IReadOnlySet<int> subtree = Descendants(
                    skin.Skeleton,
                    region.AnchorSkeletonJointIndex);
                if (MassForJoints(skin, vertex, subtree) > 0.000001f)
                    branch.Add((meshIndex, vertex));
            }
            int uniqueBranchPositions = branch.Select(value =>
                    donor.Meshes[value.Mesh].Positions[value.Vertex])
                .Distinct()
                .Count();
            (int Mesh, int Vertex)[] uncovered = branch.Except(captured).ToArray();
            int uniqueCapturedPositions = captured.Select(value =>
                    donor.Meshes[value.Mesh].Positions[value.Vertex])
                .Distinct()
                .Count();
            int uniqueUncoveredPositions = uncovered.Select(value =>
                    donor.Meshes[value.Mesh].Positions[value.Vertex])
                .Distinct()
                .Count();
            HashSet<(int Mesh, int Vertex)> core = region.CoreVerticesByMesh
                .SelectMany(membership => membership.VertexIndices.Select(vertex =>
                    (membership.MeshIndex, vertex)))
                .ToHashSet();
            float forbiddenMass = 0;
            float coarseMotionMass = 0;
            var usedFingerJoints = new HashSet<int>();
            foreach ((int meshIndex, int vertex) in captured)
            {
                ImportedSkinning skin = automatic.FittingPreviewScene
                    .Meshes[meshIndex].Skinning!;
                IReadOnlySet<int> handSubtree = Descendants(
                    skin.Skeleton, region.AnchorSkeletonJointIndex);
                IReadOnlySet<int> allowed = core.Contains((meshIndex, vertex))
                    ? handSubtree
                    : new HashSet<int>
                    {
                        region.AnchorSkeletonJointIndex,
                        region.ProximalSkeletonJointIndex
                    };
                forbiddenMass += 1 - MassForJoints(skin, vertex, allowed);
                if (core.Contains((meshIndex, vertex)))
                {
                    coarseMotionMass += MassForJoints(
                        skin,
                        vertex,
                        handSubtree.Where(joint =>
                            joint != region.AnchorSkeletonJointIndex).ToHashSet());
                    foreach ((int joint, float weight) in ActiveWeights(skin, vertex))
                    {
                        if (weight > 0.000001f &&
                            joint != region.AnchorSkeletonJointIndex &&
                            handSubtree.Contains(joint))
                        {
                            usedFingerJoints.Add(joint);
                        }
                    }
                }
            }
            ImportedSkeleton generatedSkeleton = automatic.FittingPreviewScene.Meshes
                .Select(mesh => mesh.Skinning?.Skeleton)
                .First(value => value is not null)!;
            int[] fingerDepths = usedFingerJoints
                .Select(joint => DepthBelow(
                    generatedSkeleton, joint, region.AnchorSkeletonJointIndex))
                .ToArray();
            int usedLaneCount = usedFingerJoints
                .Select(joint => DirectChildBelow(
                    generatedSkeleton, joint, region.AnchorSkeletonJointIndex))
                .Distinct()
                .Count();
            Console.WriteLine(
                $"  {semantic}: legacy-descendant records={branch.Count}, " +
                $"unique={uniqueBranchPositions}, captured={captured.Count}/" +
                $"unique{uniqueCapturedPositions}, uncovered={uncovered.Length}/" +
                $"unique{uniqueUncoveredPositions}, forbiddenMass={forbiddenMass:G9}, " +
                $"fingerMotionMass={coarseMotionMass:G9}, " +
                $"usedLanes={usedLaneCount}, maxDepth=" +
                $"{fingerDepths.DefaultIfEmpty().Max()}");
            if (uncovered.Length > 0)
            {
                float[] distances = uncovered.Select(value => Distance(
                        automatic.FittingPreviewScene.Meshes[value.Mesh]
                            .Positions[value.Vertex],
                        region.ResolvedVolume!))
                    .ToArray();
                GeneratedSkinningRegionVolume volume = region.ResolvedVolume!;
                (float Axial, float Radial)[] coordinates = uncovered.Select(value =>
                {
                    Vector3 position = automatic.FittingPreviewScene.Meshes[value.Mesh]
                        .Positions[value.Vertex];
                    Vector3 relative = position - volume.Center;
                    float axial = Vector3.Dot(relative, volume.AxialAxis) /
                                  volume.AxialRadius;
                    float lateral = Vector3.Dot(relative, volume.LateralAxis) /
                                    volume.LateralRadius;
                    float forward = Vector3.Dot(relative, volume.ForwardAxis) /
                                    volume.ForwardRadius;
                    return (axial, MathF.Sqrt(lateral * lateral + forward * forward));
                }).ToArray();
                string dominant = string.Join(", ", uncovered
                    .GroupBy(value =>
                    {
                        ImportedSkinning skin = automatic.FittingPreviewScene
                            .Meshes[value.Mesh].Skinning!;
                        return skin.Skeleton.JointNames[DominantJoint(skin, value.Vertex)];
                    })
                    .OrderByDescending(group => group.Count())
                    .Select(group => $"{group.Key}={group.Count()}"));
                throw new InvalidOperationException(
                    $"Daphne {semantic} leaves {uncovered.Length}/{branch.Count} " +
                    $"legacy hand-descendant vertices outside the complete " +
                    $"region; normalized ellipsoid distance range=" +
                    $"{distances.Min():G9}..{distances.Max():G9}, axial=" +
                    $"{coordinates.Min(value => value.Axial):G9}.." +
                    $"{coordinates.Max(value => value.Axial):G9}, radial=" +
                    $"{coordinates.Min(value => value.Radial):G9}.." +
                    $"{coordinates.Max(value => value.Radial):G9}; dominant=" +
                    dominant + ".");
            }
            if (branch.Count != 123 || uniqueBranchPositions != 87 ||
                region.MotionBranchBoneNames.Count != 5 ||
                usedLaneCount < 4 || fingerDepths.DefaultIfEmpty().Max() < 3 ||
                MathF.Abs(forbiddenMass) > 0.000001f ||
                coarseMotionMass <= 0.000001f)
            {
                throw new InvalidOperationException(
                    $"Daphne {semantic} semantic fixture changed: expected the exact " +
                    $"123-record/87-unique old Hand-descendant set, zero " +
                    $"post-override forbidden mass and nonzero approximate finger " +
                    "motion, got " +
                    $"{branch.Count}/{uniqueBranchPositions}, {forbiddenMass:G9}, " +
                    $"{coarseMotionMass:G9}.");
            }
        }
    }

    private static IReadOnlySet<int> Descendants(
        ImportedSkeleton skeleton,
        int anchor)
    {
        IReadOnlyList<int> parents = skeleton.ParentJointIndices ??
            throw new InvalidOperationException("Generated skeleton hierarchy is unavailable.");
        var result = new HashSet<int>();
        for (int joint = 0; joint < parents.Count; joint++)
        {
            int cursor = joint;
            var visited = new HashSet<int>();
            while (cursor >= 0 && visited.Add(cursor))
            {
                if (cursor == anchor)
                {
                    result.Add(joint);
                    break;
                }
                cursor = parents[cursor];
            }
        }
        return result;
    }

    private static int DepthBelow(
        ImportedSkeleton skeleton,
        int joint,
        int anchor)
    {
        IReadOnlyList<int> parents = skeleton.ParentJointIndices ??
            throw new InvalidOperationException("Generated skeleton hierarchy is unavailable.");
        int depth = 0;
        var visited = new HashSet<int>();
        for (int current = joint; current >= 0 && visited.Add(current);
             current = parents[current])
        {
            if (current == anchor)
                return depth;
            depth++;
        }
        throw new InvalidOperationException($"Joint {joint} is outside Hand {anchor}.");
    }

    private static int DirectChildBelow(
        ImportedSkeleton skeleton,
        int joint,
        int anchor)
    {
        IReadOnlyList<int> parents = skeleton.ParentJointIndices ??
            throw new InvalidOperationException("Generated skeleton hierarchy is unavailable.");
        int current = joint;
        var visited = new HashSet<int>();
        while (current >= 0 && visited.Add(current))
        {
            if (parents[current] == anchor)
                return current;
            current = parents[current];
        }
        throw new InvalidOperationException($"Joint {joint} is outside Hand {anchor}.");
    }

    private static IEnumerable<(int Joint, float Weight)> ActiveWeights(
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
        return values.Where(value => value.Weight > 0.000001f);
    }

    private static int DominantJoint(ImportedSkinning skin, int vertex)
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
        return values.OrderByDescending(value => value.Weight)
            .ThenBy(value => value.Joint)
            .First().Joint;
    }

    private static float MassForJoint(
        ImportedSkinning skin,
        int vertex,
        int joint)
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
        return values.Where(value => value.Joint == joint).Sum(value => value.Weight);
    }

    private static float MassForJoints(
        ImportedSkinning skin,
        int vertex,
        IReadOnlySet<int> jointsToKeep)
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
        return values.Where(value => jointsToKeep.Contains(value.Joint))
            .Sum(value => value.Weight);
    }

    private static void AssertDaphneCounts(GeneratedSkinningPreparationResult value)
    {
        int vertices = value.PreparedScene.Meshes.Sum(mesh => mesh.Positions.Length);
        int triangles = value.PreparedScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
        if (value.PreparedScene.Meshes.Count != 4 || vertices != 1701 || triangles != 2158 ||
            value.Analysis.PreparedVertexCount != 1701 ||
            value.Analysis.DonorComponentCount != 26 ||
            value.Analysis.Attachments.Count != 25)
        {
            throw new InvalidDataException(
                $"Daphne geometry changed: meshes={value.PreparedScene.Meshes.Count}, " +
                $"vertices={vertices}, triangles={triangles}, components=" +
                $"{value.Analysis.DonorComponentCount}, attachments=" +
                $"{value.Analysis.Attachments.Count}.");
        }
    }

    private static void AssertRigidCoreAnimationResidual(
        SmoDocument target,
        GeneratedSkinningPreparationResult preparation,
        GeneratedSkinningRegionAnalysis analysis,
        string animationDirectory)
    {
        if (!Directory.Exists(animationDirectory))
            throw new DirectoryNotFoundException(animationDirectory);
        string[] animationNames = ["blidme.san", "blwalk.san", "blru.san"];
        string[] animationPaths = animationNames
            .Select(name => Path.Combine(animationDirectory, name))
            .ToArray();
        string[] missing = animationPaths.Where(path => !File.Exists(path)).ToArray();
        if (missing.Length > 0)
            throw new FileNotFoundException("Missing SAN fixture: " + string.Join(", ", missing));

        // Exact one-hot Head core weights make full LBS algebraically identical
        // to the anchor-only transform for every animation matrix. Hands now use
        // deliberately non-rigid approximate finger chains and have their own
        // bounded topology/profile regression.
        SmoExportScene animated = SmoSceneBuilder.Build(
            target,
            new SmoExportOptions(
                ApplyWorldTransforms: true,
                AnimationPaths: animationPaths,
                Resources: SmoExportResourceTypes.Skeleton |
                           SmoExportResourceTypes.Animations |
                           SmoExportResourceTypes.ServiceNodes));
        if (animated.Animations.Count != 3 ||
            animationPaths.Any(path => !animated.Animations.Any(animation =>
                animation.Name.Contains(
                    Path.GetFileNameWithoutExtension(path),
                    StringComparison.OrdinalIgnoreCase))))
        {
            throw new InvalidDataException("SAN animation fixtures were not all decoded.");
        }
        float maximumResidual = 0;
        foreach (GeneratedSkinningRegionResolution region in analysis.Regions.Where(
                     value =>
                         value.Region == GeneratedSkinningSemanticRegion.Head))
        foreach ((ImportedMesh mesh, int vertex) in Enumerate(
                     preparation.PreparedScene,
                     region.CoreVerticesByMesh))
        {
            ImportedSkinning skin = mesh.Skinning!;
            ImportedJointIndices joints = skin.JointIndices[vertex];
            Vector4 weights = skin.Weights[vertex];
            float residual = MathF.Abs(1 - weights.X) +
                             MathF.Abs(weights.Y) +
                             MathF.Abs(weights.Z) +
                             MathF.Abs(weights.W) +
                             (joints.X == region.AnchorSkeletonJointIndex ? 0 : 1);
            maximumResidual = MathF.Max(maximumResidual, residual);
        }
        if (maximumResidual >= 0.0001f)
        {
            throw new InvalidOperationException(
                $"SAN Head-core one-hot residual is {maximumResidual:G9}.");
        }
    }

    private static float Mass(
        ImportedScene scene,
        IEnumerable<TargetRigBodyVertexMembership> memberships,
        IReadOnlySet<string> names)
    {
        float result = 0;
        foreach ((ImportedMesh mesh, int vertex) in Enumerate(scene, memberships))
        {
            ImportedSkinning skin = mesh.Skinning!;
            ImportedJointIndices joints = skin.JointIndices[vertex];
            Vector4 weights = skin.Weights[vertex];
            ushort[] indices = [joints.X, joints.Y, joints.Z, joints.W];
            float[] values = [weights.X, weights.Y, weights.Z, weights.W];
            for (int slot = 0; slot < 4; slot++)
            {
                if (values[slot] > 0 && names.Contains(skin.Skeleton.JointNames[indices[slot]]))
                    result += values[slot];
            }
        }
        return result;
    }

    private static IEnumerable<(ImportedMesh Mesh, int Vertex)> Enumerate(
        ImportedScene scene,
        IEnumerable<TargetRigBodyVertexMembership> memberships)
    {
        foreach (TargetRigBodyVertexMembership membership in memberships)
        foreach (int vertex in membership.VertexIndices)
            yield return (scene.Meshes[membership.MeshIndex], vertex);
    }

    private static void AssertScenesBitIdentical(
        ImportedScene actual,
        ImportedScene expected,
        string context)
    {
        if (actual.Meshes.Count != expected.Meshes.Count)
            throw new InvalidOperationException($"{context}: mesh count differs.");
        for (int meshIndex = 0; meshIndex < actual.Meshes.Count; meshIndex++)
        {
            ImportedMesh a = actual.Meshes[meshIndex];
            ImportedMesh b = expected.Meshes[meshIndex];
            if (!a.Positions.SequenceEqual(b.Positions) ||
                !a.TriangleIndices.SequenceEqual(b.TriangleIndices) ||
                a.Skinning is null || b.Skinning is null ||
                !a.Skinning.JointIndices.SequenceEqual(b.Skinning.JointIndices) ||
                a.Skinning.Weights.Length != b.Skinning.Weights.Length ||
                a.Skinning.Weights.Zip(b.Skinning.Weights)
                    .Any(pair => !SameBits(pair.First, pair.Second)))
            {
                throw new InvalidOperationException($"{context}: mesh {meshIndex} differs.");
            }
        }
    }

    private static bool SameBits(Vector4 left, Vector4 right) =>
        BitConverter.SingleToInt32Bits(left.X) == BitConverter.SingleToInt32Bits(right.X) &&
        BitConverter.SingleToInt32Bits(left.Y) == BitConverter.SingleToInt32Bits(right.Y) &&
        BitConverter.SingleToInt32Bits(left.Z) == BitConverter.SingleToInt32Bits(right.Z) &&
        BitConverter.SingleToInt32Bits(left.W) == BitConverter.SingleToInt32Bits(right.W);

    private static string DescribeInfluences(ImportedSkinning skin, int vertex)
    {
        ImportedJointIndices joints = skin.JointIndices[vertex];
        Vector4 weights = skin.Weights[vertex];
        ushort[] indices = [joints.X, joints.Y, joints.Z, joints.W];
        float[] values = [weights.X, weights.Y, weights.Z, weights.W];
        return string.Join(",", Enumerable.Range(0, 4).Select(slot =>
            $"{skin.Skeleton.JointNames[indices[slot]]}:{values[slot]:G9}"));
    }

    private static bool NearlyEqual(float left, float right) =>
        MathF.Abs(left - right) <= 0.00001f * MathF.Max(1, MathF.Max(MathF.Abs(left), MathF.Abs(right)));

    private static float RelativeDifference(float left, float right) =>
        MathF.Abs(left - right) / MathF.Max(0.000001f, MathF.Max(MathF.Abs(left), MathF.Abs(right)));

    private static int Count(IReadOnlyList<TargetRigBodyVertexMembership> value) =>
        value.Sum(membership => membership.VertexIndices.Count);

    private static GeneratedSkinningRegionResolution Region(
        GeneratedSkinningRegionAnalysis analysis,
        GeneratedSkinningSemanticRegion region) =>
        analysis.Regions.Single(value => value.Region == region);

    private static string DescribeRegions(GeneratedSkinningRegionAnalysis analysis) =>
        string.Join(", ", analysis.Regions.OrderBy(value => value.Region).Select(value =>
            $"{value.Region}={value.Status}/core{Count(value.CoreVerticesByMesh)}/" +
            $"transition{Count(value.TransitionVerticesByMesh)}"));

    private static void ExpectThrows<T>(Action action, string context)
        where T : Exception
    {
        try
        {
            action();
        }
        catch (T)
        {
            return;
        }
        throw new InvalidOperationException($"{context}: expected {typeof(T).Name}.");
    }

    private static string RequireInput(string argument, string extension, string label)
    {
        string path = Path.GetFullPath(argument);
        if (!File.Exists(path))
            throw new FileNotFoundException($"{label} was not found.", path);
        if (!Path.GetExtension(path).Equals(extension, StringComparison.OrdinalIgnoreCase))
            throw new ArgumentException($"{label} must be a {extension} file.");
        return path;
    }

    private static string FingerprintDonor(ImportedScene donor)
    {
        using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        foreach (ImportedMesh mesh in donor.Meshes)
        {
            hash.AppendData(System.Text.Encoding.UTF8.GetBytes(mesh.Name));
            foreach (Vector3 value in mesh.Positions)
            {
                Append(value.X); Append(value.Y); Append(value.Z);
            }
            foreach (uint value in mesh.TriangleIndices)
                hash.AppendData(BitConverter.GetBytes(value));
            hash.AppendData(BitConverter.GetBytes(mesh.MaterialIndex));
        }
        return Convert.ToHexString(hash.GetHashAndReset());

        void Append(float value) =>
            hash.AppendData(BitConverter.GetBytes(BitConverter.SingleToInt32Bits(value)));
    }
}
