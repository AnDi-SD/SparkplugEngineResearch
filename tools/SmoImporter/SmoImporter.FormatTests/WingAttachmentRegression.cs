using System.Numerics;
using System.Security.Cryptography;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;
using SmoViewer.Sparkplug;

internal static class WingAttachmentRegression
{
    internal static int RunUntextured(string nativeWingPath, string winglessPath, string outputDirectory)
    {
        string[] inputs = [Path.GetFullPath(nativeWingPath), Path.GetFullPath(winglessPath)];
        string[] inputHashes = inputs.Select(Hash).ToArray();
        SmoDocument source = SmoDocument.Load(nativeWingPath);
        ImportedScene original = CreateDonor(source, SmoModelReader.Read(winglessPath));
        int[] rigid = Enumerable.Range(0, original.Meshes.Count).Where(index => original.Meshes[index].Skinning is null).ToArray();
        Require(rigid.Length == 2, "The untextured-wing fixture needs two original rigid wings.");
        Directory.CreateDirectory(outputDirectory);
        foreach (int missingCount in new[] { 1, 2 })
        {
            ImportedMaterial[] materials = original.Materials.ToArray();
            ImportedMesh[] meshes = original.Meshes.ToArray();
            foreach (int index in rigid.Take(missingCount))
            {
                ImportedMesh mesh = meshes[index];
                materials[mesh.MaterialIndex] = materials[mesh.MaterialIndex] with
                {
                    BaseColorTextureIndex = -1,
                    AlphaMode = missingCount == 2 ? ImportedMaterialAlphaMode.Blend : ImportedMaterialAlphaMode.Opaque
                };
                if (missingCount == 2) meshes[index] = mesh with
                    { DiffuseColorsArgb = Enumerable.Repeat(0x80FFFFFFu, mesh.Positions.Length).ToArray() };
            }
            ImportedScene donor = original with { Meshes = meshes, SourceMaterials = materials };
            GeneratedSkinningComponentInfo[] components = rigid.Select(index =>
            {
                ImportedMesh mesh = donor.Meshes[index];
                int[] vertices = mesh.TriangleIndices.Distinct().Select(vertex => checked((int)vertex)).ToArray();
                return new GeneratedSkinningComponentInfo(index, [index], [mesh.Name], vertices.Length,
                    mesh.TriangleIndices.Length / 3, Vector3.Zero, [new(index, mesh.Name, vertices)], default, null);
            }).ToArray();
            foreach (string input in inputs)
            {
                SmoDocument target = SmoDocument.Load(input);
                var pose = TargetRigDefinition.FromSmoDocument(target).CreateFittingPose().Capture();
                SmoWingFitResult fit = SmoWingFitter.Fit(target, pose, donor, components);
                SmoWingAttachmentDraft draft = SmoWingAttachmentDraft.Create(fit, pose, components);
                string output = Path.Combine(outputDirectory, Path.GetFileNameWithoutExtension(input) + $"-untextured-{missingCount}.smo");
                SmoSkinnedGlbReplacer.Replace(target, donor, ReplacementTransform.Identity, output,
                    SkinnedGeometryTransferMode.RetargetToGameBindPose, wingAttachments: draft);
                SmoDocument verified = SmoDocument.Load(output);
                SmoExportScene scene = SmoSceneBuilder.Build(verified);
                int[] wingNodes = scene.Nodes.Where(node => node.Name is "WingL" or "WingR").Select(node => node.ObjectIndex).ToArray();
                var wingMeshes = scene.MeshPlacements.Where(placement => wingNodes.Any(node => HasAncestor(verified, placement.ParentNodeObjectIndex, node)))
                    .Select(placement => scene.Meshes.Single(mesh => mesh.VariantKey == placement.EffectiveMeshKey)).ToArray();
                Require(wingMeshes.Length == 2 && wingMeshes.Count(mesh => mesh.Texture is null) == missingCount,
                    "A missing wing image was replaced by a catalog or neutral texture.");
                if (missingCount == 2)
                    Require(wingMeshes.All(mesh => mesh.LoadedMaterial?.VertexAlpha == 1 &&
                        mesh.LoadedMaterial.Passes.Single().Blend == 2 &&
                        SmoMeshDecoder.Decode(verified, verified.Objects[mesh.ObjectIndex]).DiffuseColorsArgb.All(color => color == 0x80FFFFFF)),
                        "Untextured wings lost their source vertex/material alpha.");
                Require(inputHashes.SequenceEqual(inputs.Select(Hash)), "An original wing fixture changed.");
                Console.WriteLine($"PASS {Path.GetFileName(output)} native readback: {missingCount} wing(s) without TextureData, original geometry/UV/body verified.");
            }
        }
        Console.WriteLine("UNTEXTURED WING REGRESSION PASS");
        return 0;
    }

    internal static int Run(string nativeWingPath, string winglessPath, string outputDirectory)
    {
        string[] inputs = [Path.GetFullPath(nativeWingPath), Path.GetFullPath(winglessPath)];
        string[] inputHashes = inputs.Select(Hash).ToArray();
        SmoDocument source = SmoDocument.Load(nativeWingPath);
        SmoExportScene referenceScene = SmoSceneBuilder.Build(source);
        ImportedScene donor = CreateDonor(source, SmoModelReader.Read(winglessPath));
        int[] rigidIndices = Enumerable.Range(0, donor.Meshes.Count).Where(index => donor.Meshes[index].Skinning is null).ToArray();
        Require(rigidIndices.Length == 2, "The fixture must have exactly two rigid wing surfaces.");
        GeneratedSkinningComponentInfo[] components = rigidIndices.Select(index =>
        {
            ImportedMesh mesh = donor.Meshes[index];
            int[] vertices = mesh.TriangleIndices.Distinct().Select(vertex => checked((int)vertex)).ToArray();
            return new GeneratedSkinningComponentInfo(index, [index], [mesh.Name], vertices.Length,
                mesh.TriangleIndices.Length / 3, Vector3.Zero, [new(index, mesh.Name, vertices)], default, null);
        }).ToArray();
        Directory.CreateDirectory(outputDirectory);
        using var clip = SparkplugAnimationClip.Load(Path.Combine(Path.GetDirectoryName(nativeWingPath)!, "blbcadf.san"));
        using var sourceRuntime = new SparkplugSceneRuntime(source, new Dictionary<int, SmoSkin>());
        sourceRuntime.Bind(clip);
        foreach (string input in inputs)
        {
            SmoDocument target = SmoDocument.Load(input);
            var pose = TargetRigDefinition.FromSmoDocument(target).CreateFittingPose().Capture();
            SmoWingFitResult fit = SmoWingFitter.Fit(target, pose, donor, components);
            VerifyHeightAdjustment(target, pose, donor, components, fit, outputDirectory, input);
            VerifyRotationAdjustment(target, pose, donor, components, fit, clip, outputDirectory, input);
            SmoWingAttachmentDraft draft = SmoWingAttachmentDraft.Create(fit, pose, components);
            GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(target, donor, wingAttachments: draft);
            Require(plan.CanReplace, "Mixed analysis rejected the original fixture. " + string.Join(" ", plan.Messages));
            string output = Path.Combine(outputDirectory, Path.GetFileNameWithoutExtension(input) + "-wings.smo");
            GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(target, donor, ReplacementTransform.Identity,
                output, SkinnedGeometryTransferMode.RetargetToGameBindPose, wingAttachments: draft);
            SmoDocument readback = SmoDocument.Load(result.OutputPath);
            SmoExportScene scene = SmoSceneBuilder.Build(readback);
            Require(!readback.HasErrors, "The mixed character failed strict SMO parsing.");
            Require(result.TriangleCount == donor.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3), "The mixed character lost or duplicated triangles.");
            int[] wingNodes = scene.Nodes.Where(node => node.Name is "WingL" or "WingR").Select(node => node.ObjectIndex).ToArray();
            uint[] wingTextureIds = scene.MeshPlacements.Where(placement => wingNodes.Any(node =>
                    HasAncestor(readback, placement.ParentNodeObjectIndex, node)))
                .Select(placement => scene.Meshes.Single(mesh => mesh.VariantKey == placement.EffectiveMeshKey)
                    .LoadedMaterial!.Passes.Single().Layers.Single().Texture!.ObjectId).Distinct().ToArray();
            Require(wingTextureIds.Length == 1, "The two wings duplicated their identical source image instead of sharing one TextureData.");
            using var runtime = new SparkplugSceneRuntime(readback, new Dictionary<int, SmoSkin>());
            runtime.Bind(clip);
            foreach (string nodeName in new[] { "WingL", "WingR" })
            {
                var node = scene.Nodes.Single(node => node.Name == nodeName);
                var originalNode = source.Objects.Single(entry => entry.Name == nodeName &&
                    entry.TypeHash is SmoClassIds.Node or SmoClassIds.RenderNode);
                if (input == inputs[0])
                {
                    SmoObjectEntry restored = readback.Objects[node.ObjectIndex];
                    Require(restored.Id == originalNode.Id && restored.TypeHash == originalNode.TypeHash &&
                        restored.RawName.Span.SequenceEqual(originalNode.RawName.Span),
                        "Replacing native wings changed their class, ID or serialized name.");
                }
                var meshes = scene.MeshPlacements.Where(placement => HasAncestor(readback, placement.ParentNodeObjectIndex, node.ObjectIndex)).ToArray();
                Require(meshes.Length > 0 && meshes.All(placement => scene.Meshes.Single(mesh => mesh.VariantKey == placement.EffectiveMeshKey).SkinObjectIndex is null),
                    "A selected wing was not attached as a rigid animated visual.");
                Matrix4x4 first = default;
                Vector3[]? firstPositions = null;
                bool geometryMoved = false;
                var referencePlacements = referenceScene.MeshPlacements.Where(placement =>
                    HasAncestor(source, placement.ParentNodeObjectIndex, originalNode.Index)).ToArray();
                for (int sample = 0; sample < 6; sample++)
                {
                    float time = clip.Duration * sample / 6;
                    sourceRuntime.Sample(time);
                    runtime.Sample(time);
                    Matrix4x4 actual = runtime.Worlds[node.ObjectIndex];
                    Matrix4x4 expected = sourceRuntime.Worlds[originalNode.Index];
                    Require(MatrixNear(actual, expected), "The attached wing does not follow the original SAN transform.");
                    Vector3[] positions = ReadAnimatedWingPositions(scene, meshes, runtime);
                    Vector3[] reference = ReadAnimatedWingPositions(referenceScene, referencePlacements, sourceRuntime);
                    Require(positions.Length == reference.Length && positions.All(point =>
                        reference.Any(original => Vector3.Distance(point, original) < 0.002f)),
                        "The serialized rigid wing vertices diverge from the original animated geometry.");
                    if (firstPositions is not null)
                        geometryMoved |= positions.Where((point, index) =>
                            Vector3.Distance(point, firstPositions[index]) > 0.01f).Any();
                    firstPositions ??= positions;
                    if (sample == 0)
                        first = actual;
                }
                Require(Vector3.Distance(Vector3.TransformNormal(Vector3.UnitX, first),
                    Vector3.TransformNormal(Vector3.UnitX, runtime.Worlds[node.ObjectIndex])) > 0.01f,
                    "The attached wing node did not animate.");
                Require(geometryMoved, "The serialized rigid wing geometry did not move during SAN playback.");
            }
            if (string.Equals(input, inputs[1], StringComparison.OrdinalIgnoreCase))
                VerifyBodyOnlyClip(readback, draft, Path.Combine(Path.GetDirectoryName(input)!, "blwalk.san"));
            Require(inputHashes.SequenceEqual(inputs.Select(Hash)), "An original fixture changed.");
            Console.WriteLine($"PASS {Path.GetFileName(input)} mixed rigid wings, complete body, textures and actual wing vertices across six original SAN samples");
        }
        Console.WriteLine("WING ATTACHMENT REGRESSION PASS");
        return 0;
    }

    private static void VerifyHeightAdjustment(SmoDocument target, TargetRigFittingPoseSnapshot pose,
        ImportedScene donor, IReadOnlyList<GeneratedSkinningComponentInfo> components, SmoWingFitResult automatic,
        string outputDirectory, string input)
    {
        const float percent = 10;
        SmoWingFitResult zero = SmoWingFitter.Fit(target, pose, donor, components, 0);
        SmoWingFitResult up = SmoWingFitter.Fit(target, pose, donor, components, percent);
        SmoWingFitResult down = SmoWingFitter.Fit(target, pose, donor, components, -percent);
        var selected = components.SelectMany(component => component.VerticesByMesh)
            .SelectMany(member => member.VertexIndices.Select(vertex => (Mesh: member.MeshIndex, Vertex: vertex))).ToHashSet();
        int spine = pose.Definition.GetJointIndex("Spine_03");
        int lowerSpine = pose.Definition.GetJointIndex("Spine_02");
        int leftClavicle = pose.Definition.GetJointIndex("L_Clavicle");
        int rightClavicle = pose.Definition.GetJointIndex("R_Clavicle");
        Vector3 lateral = Vector3.Normalize(pose.WorldMatrices[leftClavicle].Translation - pose.WorldMatrices[rightClavicle].Translation);
        Vector3 spineUp = pose.WorldMatrices[spine].Translation - pose.WorldMatrices[lowerSpine].Translation;
        // Height is measured along the torso's orthogonal up direction, rather than the slightly oblique spine segment.
        Vector3 bodyUp = Vector3.Normalize(spineUp - Vector3.Dot(spineUp, lateral) * lateral);
        float[] bodyHeights = donor.Meshes.SelectMany((mesh, index) => mesh.TriangleIndices.Distinct()
            .Where(vertex => !selected.Contains((index, (int)vertex)))
            .Select(vertex => Vector3.Dot(mesh.Positions[vertex], bodyUp))).ToArray();
        float bodyHeight = bodyHeights.Max() - bodyHeights.Min();
        float expectedDistance = bodyHeight * percent / 100;
        float tolerance = Math.Max(0.0001f, bodyHeight * 0.00001f);
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            Require(zero.Scene.Meshes[meshIndex].Positions.SequenceEqual(automatic.Scene.Meshes[meshIndex].Positions),
                "An explicit zero height correction changed the automatic wing fit.");
            for (int vertex = 0; vertex < donor.Meshes[meshIndex].Positions.Length; vertex++)
            {
                if (selected.Contains((meshIndex, vertex)))
                    continue;
                Require(up.Scene.Meshes[meshIndex].Positions[vertex] == donor.Meshes[meshIndex].Positions[vertex] &&
                    down.Scene.Meshes[meshIndex].Positions[vertex] == donor.Meshes[meshIndex].Positions[vertex],
                    "A wing height correction moved a body or head vertex.");
            }
            Require(up.Scene.Meshes[meshIndex].Normals.SequenceEqual(automatic.Scene.Meshes[meshIndex].Normals) &&
                down.Scene.Meshes[meshIndex].Normals.SequenceEqual(automatic.Scene.Meshes[meshIndex].Normals),
                "A wing height translation changed a normal.");
        }
        foreach (var (baselineSide, upSide, downSide) in new[]
        {
            (automatic.Left, up.Left, down.Left), (automatic.Right, up.Right, down.Right)
        })
        {
            Vector3 shift = upSide.Transform.Translation - baselineSide.Transform.Translation;
            Vector3 reverse = downSide.Transform.Translation - baselineSide.Transform.Translation;
            Require(Math.Abs(shift.Length() - expectedDistance) <= tolerance && (shift + reverse).Length() <= tolerance,
                "Wing height is not a symmetric percentage of the non-wing body height.");
            Require(Math.Abs(Vector3.Dot(shift, baselineSide.Reference.PlaneNormal)) <= tolerance &&
                Vector3.Dot(shift, bodyUp) > 0,
                "Wing height did not move upward within its fitted plane.");
            Require(upSide.Reference.NodeLocalToSpine == baselineSide.Reference.NodeLocalToSpine &&
                downSide.Reference.NodeLocalToSpine == baselineSide.Reference.NodeLocalToSpine,
                "Wing height changed the native animation pivot.");
        }

        ImportedScene enlargedWings = ScaleDonor(donor, selected, wingsOnly: true, scale: 4);
        SmoWingFitResult enlargedBase = SmoWingFitter.Fit(target, pose, enlargedWings, components);
        SmoWingFitResult enlargedUp = SmoWingFitter.Fit(target, pose, enlargedWings, components, percent);
        Require(Vector3.Distance(enlargedUp.Left.Transform.Translation - enlargedBase.Left.Transform.Translation,
                up.Left.Transform.Translation - automatic.Left.Transform.Translation) <= tolerance &&
            Vector3.Distance(enlargedUp.Right.Transform.Translation - enlargedBase.Right.Transform.Translation,
                up.Right.Transform.Translation - automatic.Right.Transform.Translation) <= tolerance,
            "The height percentage changed when only the selected wing extents grew.");
        const float scale = 2.75f;
        ImportedScene scaled = ScaleDonor(donor, selected, wingsOnly: false, scale);
        SmoWingFitResult scaledBase = SmoWingFitter.Fit(target, pose, scaled, components);
        SmoWingFitResult scaledUp = SmoWingFitter.Fit(target, pose, scaled, components, percent);
        Require(Math.Abs((scaledUp.Left.Transform.Translation - scaledBase.Left.Transform.Translation).Length() -
                expectedDistance * scale) <= tolerance * scale &&
            Math.Abs((scaledUp.Right.Transform.Translation - scaledBase.Right.Transform.Translation).Length() -
                expectedDistance * scale) <= tolerance * scale,
            "The height percentage did not follow the donor body's scale.");
        foreach (float invalid in new[] { float.NaN, float.PositiveInfinity, float.NegativeInfinity })
        {
            bool rejected = false;
            try { SmoWingFitter.Fit(target, pose, donor, components, invalid); }
            catch (ArgumentOutOfRangeException) { rejected = true; }
            Require(rejected, "A non-finite wing height correction was accepted.");
        }
        ImportedScene collapsedBody = donor with
        {
            Meshes = donor.Meshes.Select((mesh, meshIndex) => mesh with
            {
                Positions = mesh.Positions.Select((point, vertex) => selected.Contains((meshIndex, vertex)) ? point :
                    Vector3.Zero).ToArray()
            }).ToArray()
        };
        bool collapsedRejected = false;
        try { SmoWingFitter.Fit(target, pose, collapsedBody, components, percent); }
        catch (InvalidDataException) { collapsedRejected = true; }
        Require(collapsedRejected, "A wing height correction accepted a body with no measurable height.");

        SmoWingAttachmentDraft draft = SmoWingAttachmentDraft.Create(up, pose, components);
        string output = Path.Combine(outputDirectory, Path.GetFileNameWithoutExtension(input) + "-wings-height.smo");
        GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(target, donor, ReplacementTransform.Identity,
            output, SkinnedGeometryTransferMode.RetargetToGameBindPose, wingAttachments: draft);
        SmoDocument readback = SmoDocument.Load(result.OutputPath);
        Require(!readback.HasErrors, "The height-adjusted character failed strict SMO parsing.");
        SmoExportScene scene = SmoSceneBuilder.Build(readback);
        foreach (SmoWingSideFit side in new[] { up.Left, up.Right })
        {
            var wingNode = scene.Nodes.Single(node => node.Name == side.Reference.NodeName);
            int node = wingNode.ObjectIndex;
            Require(MatrixNear(wingNode.BindLocalMatrix, side.Reference.NodeLocalToSpine),
                "Saving a height correction changed the wing's spine-relative animation pivot.");
            Vector3[] actual = scene.MeshPlacements.Where(placement => HasAncestor(readback, placement.ParentNodeObjectIndex, node))
                .SelectMany(placement => scene.Meshes.Single(mesh => mesh.VariantKey == placement.EffectiveMeshKey).Positions
                    .Select(point => Vector3.Transform(point, placement.WorldMatrix))).ToArray();
            Vector3[] expected = side.Components.SelectMany(component => components.Single(item => item.ComponentIndex == component).VerticesByMesh)
                .SelectMany(member => member.VertexIndices.Select(vertex => up.Scene.Meshes[member.MeshIndex].Positions[vertex])).ToArray();
            Require(actual.Length == expected.Length && actual.All(point => expected.Any(vertex => Vector3.Distance(point, vertex) <= tolerance)),
                "The saved rigid wing lost its fitting-space height correction.");
        }
        Console.WriteLine($"PASS {Path.GetFileName(input)} wing height +/-{percent}%: in-plane, body unchanged, wing extent independent, scale relative, draft and strict save/readback");
    }

    private static void VerifyRotationAdjustment(SmoDocument target, TargetRigFittingPoseSnapshot pose,
        ImportedScene donor, IReadOnlyList<GeneratedSkinningComponentInfo> components, SmoWingFitResult automatic,
        SparkplugAnimationClip clip, string outputDirectory, string input)
    {
        const float angle = 20, height = -5;
        Vector3[][] originalPositions = donor.Meshes.Select(mesh => mesh.Positions.ToArray()).ToArray();
        SmoWingFitResult zero = SmoWingFitter.Fit(target, pose, donor, components, 0, 0);
        SmoWingFitResult raised = SmoWingFitter.Fit(target, pose, donor, components, rotationDegrees: angle);
        SmoWingFitResult lowered = SmoWingFitter.Fit(target, pose, donor, components, rotationDegrees: -angle);
        SmoWingFitResult shifted = SmoWingFitter.Fit(target, pose, donor, components, height);
        SmoWingFitResult combined = SmoWingFitter.Fit(target, pose, donor, components, height, -angle);
        SmoWingFitResult repeated = SmoWingFitter.Fit(target, pose, donor, components, height, -angle);
        var selected = components.SelectMany(component => component.VerticesByMesh)
            .SelectMany(member => member.VertexIndices.Select(vertex => (Mesh: member.MeshIndex, Vertex: vertex))).ToHashSet();
        int spine = pose.Definition.GetJointIndex("Spine_03");
        int lowerSpine = pose.Definition.GetJointIndex("Spine_02");
        int leftClavicle = pose.Definition.GetJointIndex("L_Clavicle");
        int rightClavicle = pose.Definition.GetJointIndex("R_Clavicle");
        Vector3 lateral = Vector3.Normalize(pose.WorldMatrices[leftClavicle].Translation - pose.WorldMatrices[rightClavicle].Translation);
        Vector3 spineUp = pose.WorldMatrices[spine].Translation - pose.WorldMatrices[lowerSpine].Translation;
        Vector3 bodyUp = Vector3.Normalize(spineUp - Vector3.Dot(spineUp, lateral) * lateral);
        const float tolerance = 0.001f;
        foreach (SmoWingFitResult fitted in new[] { zero, raised, lowered, shifted, combined, repeated })
        {
            Require(ReferenceEquals(fitted.Scene.Textures, donor.Textures) &&
                ReferenceEquals(fitted.Scene.SourceMaterials, donor.SourceMaterials), "Wing rotation replaced a material or texture.");
            for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
            {
                ImportedMesh original = donor.Meshes[meshIndex], fittedMesh = fitted.Scene.Meshes[meshIndex];
                Require(ReferenceEquals(original.TextureCoordinates, fittedMesh.TextureCoordinates) &&
                    ReferenceEquals(original.TriangleIndices, fittedMesh.TriangleIndices), "Wing rotation changed UVs or triangle membership.");
                for (int vertex = 0; vertex < original.Positions.Length; vertex++)
                {
                    if (selected.Contains((meshIndex, vertex))) continue;
                    Require(fittedMesh.Positions[vertex] == original.Positions[vertex] &&
                        (original.Normals.Length == 0 || fittedMesh.Normals[vertex] == original.Normals[vertex]),
                        "Wing rotation moved a body/head vertex or normal.");
                }
                Require(zero.Scene.Meshes[meshIndex].Positions.SequenceEqual(automatic.Scene.Meshes[meshIndex].Positions) &&
                    zero.Scene.Meshes[meshIndex].Normals.SequenceEqual(automatic.Scene.Meshes[meshIndex].Normals),
                    "Explicit zero rotation changed the original fit.");
                Require(repeated.Scene.Meshes[meshIndex].Positions.SequenceEqual(combined.Scene.Meshes[meshIndex].Positions),
                    "Repeated rotation accumulated a previous correction.");
            }
        }
        foreach (var (baselineSide, upSide, downSide, shiftedSide, combinedSide) in new[]
        {
            (automatic.Left, raised.Left, lowered.Left, shifted.Left, combined.Left),
            (automatic.Right, raised.Right, lowered.Right, shifted.Right, combined.Right)
        })
        {
            Vector3 root = baselineSide.Reference.Pivot;
            Vector3 shiftedRoot = Vector3.Transform(shiftedSide.PlaneBeforeFit.Translation, shiftedSide.Transform);
            Require(Vector3.Distance(Vector3.Transform(upSide.PlaneBeforeFit.Translation, upSide.Transform), root) <= tolerance &&
                Vector3.Distance(Vector3.Transform(downSide.PlaneBeforeFit.Translation, downSide.Transform), root) <= tolerance &&
                Vector3.Distance(Vector3.Transform(combinedSide.PlaneBeforeFit.Translation, combinedSide.Transform), shiftedRoot) <= tolerance,
                "Wing rotation did not keep the back-side root fixed after the height adjustment.");
            foreach (SmoWingSideFit side in new[] { upSide, downSide, shiftedSide, combinedSide })
                Require(side.Reference.NodeName == baselineSide.Reference.NodeName &&
                    side.Reference.NodeObjectIndex == baselineSide.Reference.NodeObjectIndex &&
                    side.Reference.MeshObjectIndices.SequenceEqual(baselineSide.Reference.MeshObjectIndices) &&
                    side.Reference.NodeLocalToSpine == baselineSide.Reference.NodeLocalToSpine &&
                    side.Reference.PlaneLocalToSpine == baselineSide.Reference.PlaneLocalToSpine &&
                    side.Reference.PlaneInFittingSpace == baselineSide.Reference.PlaneInFittingSpace,
                    "Wing rotation changed a native animation pivot/reference.");
            Vector3 normal = baselineSide.Reference.PlaneNormal;
            var members = baselineSide.Components.SelectMany(component => components.Single(item => item.ComponentIndex == component).VerticesByMesh).ToArray();
            float rise = 0;
            int count = 0;
            foreach (TargetRigBodyVertexMembership member in members)
            foreach (int vertex in member.VertexIndices)
            {
                Vector3 original = automatic.Scene.Meshes[member.MeshIndex].Positions[vertex];
                Vector3 up = raised.Scene.Meshes[member.MeshIndex].Positions[vertex];
                Vector3 down = lowered.Scene.Meshes[member.MeshIndex].Positions[vertex];
                Vector3 shiftedPoint = shifted.Scene.Meshes[member.MeshIndex].Positions[vertex];
                Vector3 combinedPoint = combined.Scene.Meshes[member.MeshIndex].Positions[vertex];
                Require(Math.Abs(Vector3.Distance(up, root) - Vector3.Distance(original, root)) <= tolerance &&
                    Math.Abs(Vector3.Distance(down, root) - Vector3.Distance(original, root)) <= tolerance &&
                    Math.Abs(Vector3.Distance(combinedPoint, shiftedRoot) - Vector3.Distance(shiftedPoint, shiftedRoot)) <= tolerance,
                    "Wing rotation changed the distance from a vertex to its root.");
                Require(Math.Abs(Vector3.Dot(up - original, normal)) <= tolerance &&
                    Math.Abs(Vector3.Dot(down - original, normal)) <= tolerance &&
                    Math.Abs(Vector3.Dot(combinedPoint - shiftedPoint, normal)) <= tolerance,
                    "Wing rotation moved a vertex out of its fitting plane.");
                Require(Vector3.Distance((up + down) * 0.5f - root,
                    (original - root) * MathF.Cos(angle * MathF.PI / 180) +
                    normal * Vector3.Dot(original - root, normal) * (1 - MathF.Cos(angle * MathF.PI / 180))) <= tolerance,
                    "Positive and negative wing angles are not symmetric rigid rotations.");
                ImportedMesh originalMesh = automatic.Scene.Meshes[member.MeshIndex];
                if (originalMesh.Normals.Length != 0)
                {
                    Vector3 before = originalMesh.Normals[vertex], after = raised.Scene.Meshes[member.MeshIndex].Normals[vertex];
                    Require(Math.Abs(before.Length() - after.Length()) <= tolerance &&
                        Math.Abs(Vector3.Dot(before, normal) - Vector3.Dot(after, normal)) <= tolerance,
                        "Wing rotation did not rotate normals rigidly about the plane normal.");
                }
                rise += Vector3.Dot(up - down, bodyUp); count++;
            }
            Require(count > 0 && rise / count > tolerance, "One wing angle raised one side while lowering the other.");
        }
        foreach (float invalid in new[] { float.NaN, float.PositiveInfinity, float.NegativeInfinity })
        {
            bool rejected = false;
            try { SmoWingFitter.Fit(target, pose, donor, components, rotationDegrees: invalid); }
            catch (ArgumentOutOfRangeException) { rejected = true; }
            Require(rejected, "A non-finite wing rotation was accepted.");
        }

        SmoWingAttachmentDraft draft = SmoWingAttachmentDraft.Create(combined, pose, components);
        string output = Path.Combine(outputDirectory, Path.GetFileNameWithoutExtension(input) + "-wings-rotation.smo");
        GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(target, donor, ReplacementTransform.Identity,
            output, SkinnedGeometryTransferMode.RetargetToGameBindPose, wingAttachments: draft);
        SmoDocument readback = SmoDocument.Load(result.OutputPath);
        Require(!readback.HasErrors, "The rotated character failed strict SMO parsing.");
        Require(result.TriangleCount == donor.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3),
            "Rotated-wing serialization lost or duplicated geometry.");
        SmoExportScene scene = SmoSceneBuilder.Build(readback);
        using var runtime = new SparkplugSceneRuntime(readback, new Dictionary<int, SmoSkin>());
        using var targetRuntime = new SparkplugSceneRuntime(target, new Dictionary<int, SmoSkin>());
        runtime.Bind(clip); targetRuntime.Bind(clip);
        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        foreach (var (side, attachment) in new[] { (combined.Left, draft.Left), (combined.Right, draft.Right) })
        {
            var node = scene.Nodes.Single(item => item.Name == side.Reference.NodeName);
            Require(MatrixNear(node.BindLocalMatrix, side.Reference.NodeLocalToSpine), "Saved rotation changed the animation pivot.");
            var placements = scene.MeshPlacements.Where(placement => HasAncestor(readback, placement.ParentNodeObjectIndex, node.ObjectIndex)).ToArray();
            Vector3[] actual = placements.SelectMany(placement => scene.Meshes.Single(mesh => mesh.VariantKey == placement.EffectiveMeshKey).Positions
                .Select(point => Vector3.Transform(point, placement.WorldMatrix))).ToArray();
            Vector3[] expected = side.Components.SelectMany(component => components.Single(item => item.ComponentIndex == component).VerticesByMesh)
                .SelectMany(member => member.VertexIndices.Select(vertex => combined.Scene.Meshes[member.MeshIndex].Positions[vertex])).ToArray();
            Require(actual.Length == expected.Length && actual.All(point => expected.Any(vertex => Vector3.Distance(point, vertex) <= tolerance)),
                "The saved wing lost its combined angle and height correction.");
            Vector3[]? first = null;
            bool moved = false;
            for (int sample = 0; sample < 6; sample++)
            {
                float time = clip.Duration * sample / 6;
                runtime.Sample(time); targetRuntime.Sample(time);
                Vector3[] animated = ReadAnimatedWingPositions(scene, placements, runtime);
                Vector3[] localExpected = attachment.LocalScene.Meshes.SelectMany(mesh => mesh.Positions.Select(point =>
                    Vector3.Transform(Vector3.Transform(point, reflection), runtime.Worlds[node.ObjectIndex]))).ToArray();
                Require(animated.Length == localExpected.Length && animated.All(point => localExpected.Any(vertex => Vector3.Distance(point, vertex) <= tolerance)),
                    "Rotated geometry diverged from its serialized local wing attachment during SAN playback.");
                if (side.Reference.NodeObjectIndex is int targetNode)
                    Require(MatrixNear(runtime.Worlds[node.ObjectIndex], targetRuntime.Worlds[targetNode]),
                        "Wing correction changed the original SAN node motion.");
                if (first is not null) moved |= animated.Where((point, index) => Vector3.Distance(point, first[index]) > 0.01f).Any();
                first ??= animated;
            }
            Require(moved, "Corrected wing geometry did not move during the original SAN clip.");
        }
        if (combined.Left.Reference.UsesBloomXProfile &&
            File.Exists(Path.Combine(Path.GetDirectoryName(input)!, "blwalk.san")))
            VerifyBodyOnlyClip(readback, draft, Path.Combine(Path.GetDirectoryName(input)!, "blwalk.san"));
        for (int index = 0; index < donor.Meshes.Count; index++)
            Require(donor.Meshes[index].Positions.SequenceEqual(originalPositions[index]), "Wing rotation mutated the input scene.");
        Console.WriteLine($"PASS {Path.GetFileName(input)} wing rotation +/-{angle}deg, shifted root, mirrored direction, unchanged body/normals/materials, strict save/readback and six SAN samples");
    }

    private static ImportedScene ScaleDonor(ImportedScene scene, IReadOnlySet<(int Mesh, int Vertex)> selected,
        bool wingsOnly, float scale) => scene with
    {
        Meshes = scene.Meshes.Select((mesh, meshIndex) => mesh with
        {
            Positions = mesh.Positions.Select((point, vertex) => !wingsOnly || selected.Contains((meshIndex, vertex))
                ? point * scale : point).ToArray()
        }).ToArray()
    };

    private static bool MatrixNear(Matrix4x4 first, Matrix4x4 second) =>
        Vector3.Distance(first.Translation, second.Translation) < 0.001f &&
        Vector3.Distance(Vector3.TransformNormal(Vector3.UnitX, first), Vector3.TransformNormal(Vector3.UnitX, second)) < 0.001f &&
        Vector3.Distance(Vector3.TransformNormal(Vector3.UnitY, first), Vector3.TransformNormal(Vector3.UnitY, second)) < 0.001f &&
        Vector3.Distance(Vector3.TransformNormal(Vector3.UnitZ, first), Vector3.TransformNormal(Vector3.UnitZ, second)) < 0.001f;

    private static Vector3[] ReadAnimatedWingPositions(SmoExportScene scene,
        IReadOnlyList<SmoExportMeshPlacement> placements, SparkplugSceneRuntime runtime)
    {
        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        return placements.SelectMany(placement =>
        {
            SmoExportMesh mesh = scene.Meshes.Single(mesh => mesh.VariantKey == placement.EffectiveMeshKey);
            int parent = placement.ParentNodeObjectIndex ?? throw new InvalidDataException("The wing has no animated node owner.");
            return mesh.Positions.Select(point => Vector3.Transform(Vector3.Transform(point, reflection), runtime.Worlds[parent]));
        }).ToArray();
    }

    private static string Hash(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));

    private static void VerifyBodyOnlyClip(SmoDocument output, SmoWingAttachmentDraft draft, string path)
    {
        using var clip = SparkplugAnimationClip.Load(path);
        Require(!clip.Tracks.Any(track => track.NodeName is "WingL" or "WingR"), "The body-only animation fixture contains wing tracks.");
        using var runtime = new SparkplugSceneRuntime(output, new Dictionary<int, SmoSkin>());
        runtime.Bind(clip);
        int spine = output.Objects.Single(entry => entry.TypeHash == SmoClassIds.Node && entry.Name == "Spine_03").Index;
        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        for (int sample = 0; sample < 6; sample++)
        {
            runtime.Sample(clip.Duration * sample / 6);
            Require(Matrix4x4.Invert(runtime.Worlds[spine], out Matrix4x4 inverseSpine), "The animated spine is singular.");
            foreach (SmoWingRigidAttachment side in new[] { draft.Left, draft.Right })
            {
                int node = output.Objects.Single(entry => entry.TypeHash == SmoClassIds.Node && entry.Name == side.Reference.NodeName).Index;
                Matrix4x4 local = runtime.Worlds[node] * inverseSpine;
                Require(MatrixNear(local, reflection * side.Reference.NodeLocalToSpine * reflection),
                    "A body-only animation moved an untracked wing relative to its spine attachment.");
            }
        }
        Console.WriteLine("PASS original blwalk.san has no wing tracks and retains both exact spine-relative wing attachments");
    }

    private static ImportedScene CreateDonor(SmoDocument originalWings, ImportedScene body)
    {
        SmoExportScene source = SmoSceneBuilder.Build(originalWings);
        var meshes = body.Meshes.ToList();
        var textures = body.Textures.ToList();
        var materials = body.Materials.ToList();
        foreach (string name in new[] { "WingL", "WingR" })
        {
            int node = source.Nodes.Single(node => node.Name == name).ObjectIndex;
            var placement = source.MeshPlacements.Single(placement => placement.ParentNodeObjectIndex == node);
            var mesh = source.Meshes.Single(mesh => mesh.VariantKey == placement.EffectiveMeshKey);
            SmoExportTexture texture = mesh.Texture ?? throw new InvalidDataException("The original wing has no texture.");
            int textureIndex = textures.Count;
            textures.Add(new ImportedTexture(texture.Name, "image/png", texture.Width, texture.Height, texture.PngBytes));
            int materialIndex = materials.Count;
            materials.Add(new ImportedMaterial(name + " material", texture.Name, textureIndex,
                mesh.UsesAlphaBlend ? ImportedMaterialAlphaMode.Blend : ImportedMaterialAlphaMode.Opaque));
            Require(Matrix4x4.Invert(placement.WorldMatrix, out Matrix4x4 inverse), "The original wing has an invalid placement.");
            Matrix4x4 normalTransform = Matrix4x4.Transpose(inverse);
            meshes.Add(new ImportedMesh(name + " donor",
                mesh.Positions.Select(position => Vector3.Transform(position, placement.WorldMatrix)).ToArray(),
                mesh.Normals.Select(normal => Vector3.Normalize(Vector3.TransformNormal(normal, normalTransform))).ToArray(),
                mesh.TextureCoordinates0, mesh.TriangleIndices, MaterialIndex: materialIndex)
            {
                SecondaryTextureCoordinates = mesh.TextureCoordinates1
            });
        }
        return new ImportedScene(meshes.AsReadOnly(), textures.AsReadOnly(), materials.AsReadOnly());
    }

    private static bool HasAncestor(SmoDocument document, int? index, int ancestor)
    {
        while (index is int current)
        {
            if (current == ancestor)
                return true;
            index = document.Objects[current].ParentIndex;
        }
        return false;
    }

    private static void Require(bool condition, string issue)
    {
        if (!condition)
            throw new InvalidDataException(issue);
    }
}
