using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using SmoImporter.Core;
using SmoViewer.Core;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;

internal static class AlphaBranchRegression
{
    private const string OpaqueOverlayMaterialPrefix = "imp_o_m_";
    private const string OpaqueOverlaySkinPrefix = "imp_o_s_";
    private const string OpaqueOverlayMeshPrefix = "imp_o_x_";
    private const string AlphaMaterialPrefix = "imp_a_m_";
    private const string AlphaSkinPrefix = "imp_a_s_";
    private const string AlphaMeshPrefix = "imp_a_x_";
    private const float WeightEpsilon = 0.000001f;
    private const uint OpaqueOverlayAlphaSortEnable = 0;
    private const uint SkinnedTransparentSurfaceAlphaSortEnable = 1;
    private const uint RenderPriority = 1;
    private const uint OpaqueOverlayVertexDiffuse = 0xFFFFFFFF;
    private const uint SkinnedTransparentSurfaceVertexDiffuse = 0xFF000000;

    private static readonly uint[] SkinnedTransparentSurfaceMaterialRenderStates =
        [0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6];

    private static readonly uint[] OpaqueOverlayMaterialRenderStates =
        [0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6];

    private static readonly uint[] SkinnedTransparentSurfaceLightingTextureStates =
        [0, 3, 3, 0, 0, 0xFF000000, 2, 0, 0];

    private static readonly IReadOnlyDictionary<string, (int Triangles, bool Alpha)>
        ExpectedLaylaGroups = new Dictionary<string, (int, bool)>(
            StringComparer.OrdinalIgnoreCase)
        {
            ["mat1.png"] = (1108, false),
            ["mat2.png"] = (1606, false),
            ["mat3.png"] = (72, true),
            ["mat4.png"] = (52, true),
            ["mat5.png"] = (34, true),
            ["mat6.png"] = (2, true),
            ["mat7.png"] = (34, true)
        };

    private static readonly string[] ExpectedAlphaRenderableOrder =
        ["mat3.png", "mat4.png", "mat5.png", "mat7.png", "mat6.png"];

    private static readonly string[] ExpectedOpaqueOverlayOrder =
        ["mat3.png", "mat4.png"];

    private static readonly string[] ExpectedTransparentSurfaceOrder =
        ["mat5.png", "mat7.png", "mat6.png"];

    public static SkinnedRenderableMaterialProfile
        CreateLaylaFaceOverlayMaterialProfile(ImportedScene preparedScene)
    {
        ArgumentNullException.ThrowIfNull(preparedScene);
        SourceFixture fixture = InspectSourceFixture(
            preparedScene, "Layla face-overlay material profile");
        AlphaRenderableFixture[] face = fixture.AlphaRenderables
            .Where(item => ExpectedOpaqueOverlayOrder.Contains(
                item.TextureName, StringComparer.OrdinalIgnoreCase))
            .ToArray();
        if (!face.Select(item => item.SourceMeshKey).SequenceEqual([1, 2]) ||
            !face.Select(item => preparedScene.Meshes[item.SourceMeshKey].Name)
                .SequenceEqual(["Model_mat3.png", "Model_mat4.png"],
                    StringComparer.Ordinal) ||
            !face.Select(item =>
                    preparedScene.Meshes[item.SourceMeshKey].MaterialIndex)
                .SequenceEqual([1, 2]) ||
            !face.Select(item => preparedScene.Materials[
                    preparedScene.Meshes[item.SourceMeshKey].MaterialIndex].Name)
                .SequenceEqual(["mat3.png", "mat4.png"],
                    StringComparer.Ordinal))
        {
            throw new InvalidOperationException(
                "Layla face-overlay source mesh/material identity changed; refusing " +
                "to apply a key-only regression override.");
        }
        return new SkinnedRenderableMaterialProfile(
            preparedScene,
            face.Select(item => new SkinnedRenderableMaterialOverride(
                item.SourceMeshKey,
                SkinnedRenderableMaterialMode.OpaqueOverlay)));
    }

    public static GeneratedSkinningComponentOverrides CreateLaylaManualOverrides(
        GeneratedSkinningPreparationResult preparation)
    {
        ArgumentNullException.ThrowIfNull(preparation);
        int[] headAndFaceMeshes = [0, 1, 2, 3];
        const int wingMesh = 4;
        GeneratedSkinningAttachment[] headAndFace = preparation.Analysis.Attachments
            .Where(attachment => attachment.MeshIndices.Any(headAndFaceMeshes.Contains))
            .OrderBy(attachment => attachment.ComponentIndex)
            .ToArray();
        GeneratedSkinningAttachment[] wings = preparation.Analysis.Attachments
            .Where(attachment => attachment.MeshIndices.Contains(wingMesh))
            .OrderBy(attachment => attachment.ComponentIndex)
            .ToArray();
        if (headAndFace.Length == 0 || wings.Length != 8 ||
            headAndFace.Select(attachment => attachment.ComponentIndex)
                .Intersect(wings.Select(attachment => attachment.ComponentIndex)).Any())
        {
            throw new InvalidOperationException(
                $"Layla attachment fixture changed: head/face={headAndFace.Length}, " +
                $"wings={wings.Length}.");
        }
        foreach (int meshIndex in headAndFaceMeshes)
        {
            if (!headAndFace.Any(attachment => attachment.MeshIndices.Contains(meshIndex)))
            {
                throw new InvalidOperationException(
                    $"Layla head/face mesh {meshIndex} has no detached component override.");
            }
        }

        GeneratedSkinningComponentOverride[] components = headAndFace
            .Select(attachment => new GeneratedSkinningComponentOverride(
                attachment.ComponentIndex,
                GeneratedSkinningComponentAttachmentTarget.Head,
                attachment.VerticesByMesh))
            .Concat(wings.Select(attachment => new GeneratedSkinningComponentOverride(
                attachment.ComponentIndex,
                GeneratedSkinningComponentAttachmentTarget.UpperBack,
                attachment.VerticesByMesh)))
            .OrderBy(component => component.ComponentIndex)
            .ToArray();
        return new GeneratedSkinningComponentOverrides(
            Array.AsReadOnly(components),
            preparation.Analysis.DonorComponentCount,
            preparation.Analysis.TargetRigFingerprint,
            preparation.Analysis.DonorGeometryFingerprint);
    }

    public static void Run(
        SmoDocument target,
        ImportedScene preparedScene,
        string outputPath,
        string context)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(preparedScene);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        SourceFixture fixture = InspectSourceFixture(preparedScene, context);
        byte[] targetBefore = target.Data.ToArray();
        string donorFingerprintBefore = FingerprintImportedScene(preparedScene);

        if (File.Exists(outputPath))
            throw new InvalidOperationException(
                $"{context}: alpha regression output already exists: {outputPath}.");

        try
        {
            GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(
                target,
                preparedScene,
                SkinnedTextureTransferMode.ImportDonor);
            if (!plan.CanReplace || plan.MaterialGroupCount != 1)
            {
                throw new InvalidOperationException(
                    $"{context}: mixed opaque/alpha atlas is still blocked: " +
                    string.Join(" | ", plan.Messages));
            }
            if (!plan.Messages.Any(message =>
                    message.Contains("separate opaque and alpha", StringComparison.OrdinalIgnoreCase) ||
                    message.Contains("separate material/spSkin branches", StringComparison.OrdinalIgnoreCase)))
            {
                throw new InvalidOperationException(
                    $"{context}: analysis did not report the required native branch split: " +
                    string.Join(" | ", plan.Messages));
            }

            GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
                target,
                preparedScene,
                ReplacementTransform.Identity,
                outputPath,
                SkinnedGeometryTransferMode.PreservePreparedGeometry,
                texture: null,
                textureMode: SkinnedTextureTransferMode.ImportDonor);
            VerifySavedCandidate(target, preparedScene, outputPath, context, fixture);
            SmoDocument output = SmoDocument.Load(outputPath);
            if (result.TriangleCount != fixture.TotalTriangles ||
                result.Sha256 != Convert.ToHexString(SHA256.HashData(output.Data.Span)))
            {
                throw new InvalidOperationException(
                    $"{context}: public writer result does not describe the verified file.");
            }
        }
        finally
        {
            if (File.Exists(outputPath))
                File.Delete(outputPath);
        }

        if (!target.Data.Span.SequenceEqual(targetBefore) ||
            FingerprintImportedScene(preparedScene) != donorFingerprintBefore)
        {
            throw new InvalidOperationException(
                $"{context}: mixed-alpha writer mutated its target or prepared donor input.");
        }
    }

    public static void RunFaceOverlay(
        SmoDocument target,
        ImportedScene preparedScene,
        string outputPath,
        string context)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(preparedScene);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        SourceFixture fixture = InspectSourceFixture(preparedScene, context);
        SkinnedRenderableMaterialProfile profile =
            CreateLaylaFaceOverlayMaterialProfile(preparedScene);
        byte[] targetBefore = target.Data.ToArray();
        string donorFingerprintBefore = FingerprintImportedScene(preparedScene);

        ImportedScene staleScene = preparedScene with
        {
            Meshes = preparedScene.Meshes.Select((mesh, index) => index == 0
                    ? mesh with { Name = mesh.Name + "_stale" }
                    : mesh)
                .ToArray()
        };
        GlbSkinTransferPlan stalePlan = SmoSkinnedGlbReplacer.Analyze(
            target,
            staleScene,
            SkinnedTextureTransferMode.ImportDonor,
            profile);
        if (stalePlan.CanReplace || !stalePlan.Messages.Any(message =>
                message.Contains("different donor scene", StringComparison.OrdinalIgnoreCase)))
        {
            throw new InvalidOperationException(
                $"{context}: stale material profile was not rejected by donor provenance.");
        }
        try
        {
            _ = new SkinnedRenderableMaterialProfile(
                preparedScene,
                [
                    new(1, SkinnedRenderableMaterialMode.OpaqueOverlay),
                    new(1, SkinnedRenderableMaterialMode.TransparentSurface)
                ]);
            throw new InvalidOperationException(
                $"{context}: duplicate material override was accepted.");
        }
        catch (ArgumentException)
        {
            // Expected: one source mesh cannot have two native material contracts.
        }
        SkinnedRenderableMaterialProfile outOfRange = new(
            preparedScene,
            [new(preparedScene.Meshes.Count, SkinnedRenderableMaterialMode.OpaqueOverlay)]);
        GlbSkinTransferPlan outOfRangePlan = SmoSkinnedGlbReplacer.Analyze(
            target,
            preparedScene,
            SkinnedTextureTransferMode.ImportDonor,
            outOfRange);
        if (outOfRangePlan.CanReplace || !outOfRangePlan.Messages.Any(message =>
                message.Contains("outside the donor mesh catalog",
                    StringComparison.OrdinalIgnoreCase)))
        {
            throw new InvalidOperationException(
                $"{context}: out-of-range material override was not rejected.");
        }

        ImportedMesh[] mixedMeshes = preparedScene.Meshes.ToArray();
        mixedMeshes[0] = mixedMeshes[0] with
        {
            MaterialIndex = preparedScene.Meshes[1].MaterialIndex
        };
        ImportedScene mixedScene = preparedScene with { Meshes = mixedMeshes };
        SkinnedRenderableMaterialProfile mixedProfile = new(
            mixedScene,
            [new(1, SkinnedRenderableMaterialMode.OpaqueOverlay)]);
        GlbSkinTransferPlan mixedPlan = SmoSkinnedGlbReplacer.Analyze(
            target,
            mixedScene,
            SkinnedTextureTransferMode.ImportDonor,
            mixedProfile);
        if (mixedPlan.CanReplace || !mixedPlan.Messages.Any(message =>
                message.Contains("mixes source mesh keys",
                    StringComparison.OrdinalIgnoreCase)))
        {
            throw new InvalidOperationException(
                $"{context}: mixed shared-texture override was not rejected.");
        }
        if (File.Exists(outputPath))
            throw new InvalidOperationException(
                $"{context}: face-overlay regression output already exists: {outputPath}.");

        try
        {
            GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(
                target,
                preparedScene,
                SkinnedTextureTransferMode.ImportDonor,
                profile);
            if (!plan.CanReplace || plan.MaterialGroupCount != 1 ||
                !plan.Messages.Any(message => message.Contains(
                    "explicit renderable material", StringComparison.OrdinalIgnoreCase)))
            {
                throw new InvalidOperationException(
                    $"{context}: explicit face-overlay plan is blocked or unreported: " +
                    string.Join(" | ", plan.Messages));
            }

            ImportedScene autoPreview = SmoSkinnedGlbReplacer.PrepareGeometryPreview(
                target,
                preparedScene,
                ReplacementTransform.Identity,
                SkinnedGeometryTransferMode.PreservePreparedGeometry,
                SkinnedTextureTransferMode.ImportDonor,
                SkinnedRenderableMaterialProfile.Default);
            ImportedScene preview = SmoSkinnedGlbReplacer.PrepareGeometryPreview(
                target,
                preparedScene,
                ReplacementTransform.Identity,
                SkinnedGeometryTransferMode.PreservePreparedGeometry,
                SkinnedTextureTransferMode.ImportDonor,
                profile);
            if (preview.Meshes.Count != preparedScene.Meshes.Count ||
                preview.Textures.Count != 1 ||
                !TextureContainsTransparency(preview.Textures[0]))
            {
                throw new InvalidOperationException(
                    $"{context}: preview did not apply the same explicit atlas contract.");
            }
            VerifyPreviewMaterialIsolation(
                preparedScene, autoPreview, preview, fixture, context);
            VerifyPreviewFormerTransparentRgb(
                preparedScene, preview, context);

            GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
                target,
                preparedScene,
                ReplacementTransform.Identity,
                outputPath,
                SkinnedGeometryTransferMode.PreservePreparedGeometry,
                texture: null,
                textureMode: SkinnedTextureTransferMode.ImportDonor,
                materialProfile: profile);
            VerifySavedFaceOverlayCandidate(
                target, preparedScene, outputPath, context, fixture);
            VerifySavedAtlasMatchesPreview(outputPath, preview, context);
            SmoDocument output = SmoDocument.Load(outputPath);
            if (result.TriangleCount != fixture.TotalTriangles ||
                result.Sha256 != Convert.ToHexString(SHA256.HashData(output.Data.Span)))
            {
                throw new InvalidOperationException(
                    $"{context}: face-overlay writer result does not describe the file.");
            }
        }
        finally
        {
            if (File.Exists(outputPath))
                File.Delete(outputPath);
        }

        if (!target.Data.Span.SequenceEqual(targetBefore) ||
            FingerprintImportedScene(preparedScene) != donorFingerprintBefore)
        {
            throw new InvalidOperationException(
                $"{context}: face-overlay writer mutated its target or donor input.");
        }
    }

    public static void VerifySavedCandidate(
        SmoDocument target,
        ImportedScene preparedScene,
        string outputPath,
        string context)
    {
        VerifySavedCandidate(
            target,
            preparedScene,
            outputPath,
            context,
            InspectSourceFixture(preparedScene, context));
    }

    public static void VerifySavedFaceOverlayCandidate(
        SmoDocument target,
        ImportedScene preparedScene,
        string outputPath,
        string context)
    {
        VerifySavedFaceOverlayCandidate(
            target,
            preparedScene,
            outputPath,
            context,
            InspectSourceFixture(preparedScene, context));
    }

    private static void VerifySavedFaceOverlayCandidate(
        SmoDocument target,
        ImportedScene preparedScene,
        string outputPath,
        string context,
        SourceFixture fixture)
    {
        SmoDocument output = SmoDocument.Load(outputPath);
        if (output.HasErrors)
            throw new InvalidOperationException(
                $"{context}: face-overlay output failed strict parsing.");
        Dictionary<uint, SmoObjectEntry> outputById = output.Objects
            .ToDictionary(entry => entry.Id);
        VerifyRetainedTargetGraph(target, output, outputById, context);
        VerifyRetainedNativeStates(target, output, outputById, context);

        AlphaRenderableFixture[] opaqueOverlayFixtures = fixture.AlphaRenderables
            .Where(item => ExpectedOpaqueOverlayOrder.Contains(
                item.TextureName, StringComparer.OrdinalIgnoreCase))
            .ToArray();
        AlphaRenderableFixture[] transparentFixtures = fixture.AlphaRenderables
            .Where(item => ExpectedTransparentSurfaceOrder.Contains(
                item.TextureName, StringComparer.OrdinalIgnoreCase))
            .ToArray();
        if (!opaqueOverlayFixtures.Select(item => item.TextureName).SequenceEqual(
                ExpectedOpaqueOverlayOrder, StringComparer.OrdinalIgnoreCase) ||
            !transparentFixtures.Select(item => item.TextureName).SequenceEqual(
                ExpectedTransparentSurfaceOrder, StringComparer.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                $"{context}: face-overlay fixture material order changed.");
        }

        GeneratedRunSet opaqueOverlay = FindGeneratedRuns(
            output,
            OpaqueOverlayMaterialPrefix,
            OpaqueOverlaySkinPrefix,
            OpaqueOverlayMeshPrefix);
        GeneratedRunSet transparent = FindGeneratedRuns(
            output,
            AlphaMaterialPrefix,
            AlphaSkinPrefix,
            AlphaMeshPrefix);
        uint opaqueTextureId = VerifyGeneratedMaterialRuns(
            target,
            output,
            opaqueOverlay,
            opaqueOverlayFixtures,
            0,
            OpaqueOverlayMaterialRenderStates,
            SkinnedTransparentSurfaceLightingTextureStates,
            OpaqueOverlayAlphaSortEnable,
            OpaqueOverlayVertexDiffuse,
            context,
            "opaque face overlay");
        uint alphaTextureId = VerifyGeneratedMaterialRuns(
            target,
            output,
            transparent,
            transparentFixtures,
            2,
            SkinnedTransparentSurfaceMaterialRenderStates,
            SkinnedTransparentSurfaceLightingTextureStates,
            SkinnedTransparentSurfaceAlphaSortEnable,
            SkinnedTransparentSurfaceVertexDiffuse,
            context,
            "transparent surface");
        if (opaqueTextureId != alphaTextureId)
        {
            throw new InvalidOperationException(
                $"{context}: opaque face and transparent surfaces do not share the atlas.");
        }

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(output);
        foreach (SmoObjectEntry meshEntry in opaqueOverlay.Meshes)
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(output, meshEntry);
            if (!bindings.TryGetValue(meshEntry.Index, out SmoTextureBinding? binding) ||
                binding.Texture is null)
            {
                throw new InvalidOperationException(
                    $"{context}: opaque overlay mesh has no atlas binding.");
            }
            SmoTextureUvAlphaCoverage coverage =
                SmoTextureUvAlphaAnalyzer.Analyze(mesh, binding.Texture);
            if (!coverage.IsReliable || coverage.SampledTexelCount == 0 ||
                coverage.FullyTransparentTexelCount != 0 ||
                coverage.PartialAlphaTexelCount != 0 ||
                coverage.OpaqueTexelCount != coverage.SampledTexelCount)
            {
                throw new InvalidOperationException(
                    $"{context}: opaque overlay alpha was not forced before atlas resize: " +
                    coverage + ".");
            }
        }

        SmoTexture sharedTexture = bindings[opaqueOverlay.Meshes[0].Index].Texture!;
        ReadOnlySpan<byte> sharedPixels = sharedTexture.Bgra32Pixels.Span;
        bool hasTransparent = false;
        bool hasOpaque = false;
        for (int pixel = 3; pixel < sharedPixels.Length; pixel += 4)
        {
            hasTransparent |= sharedPixels[pixel] < byte.MaxValue;
            hasOpaque |= sharedPixels[pixel] == byte.MaxValue;
        }
        if (!hasTransparent || !hasOpaque)
        {
            throw new InvalidOperationException(
                $"{context}: forcing face alpha destroyed the real transparent atlas regions.");
        }

        HashSet<uint> targetMeshIds = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .Select(entry => entry.Id)
            .ToHashSet();
        SmoObjectEntry[] retainedMeshes = output.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData &&
                            targetMeshIds.Contains(entry.Id))
            .ToArray();
        TriangleMultiset bodyTriangles = BuildOutputTriangles(output, retainedMeshes);
        TriangleMultiset faceTriangles = BuildOutputTriangles(output, opaqueOverlay.Meshes);
        TriangleMultiset alphaTriangles = BuildOutputTriangles(output, transparent.Meshes);
        if (bodyTriangles.Count != 2714 || faceTriangles.Count != 124 ||
            alphaTriangles.Count != 70 ||
            bodyTriangles.Count + faceTriangles.Count + alphaTriangles.Count != 2908 ||
            !alphaTriangles.Contains(fixture.PendantTriangles))
        {
            throw new InvalidOperationException(
                $"{context}: expected body/face/alpha/total 2714/124/70/2908, got " +
                $"{bodyTriangles.Count}/{faceTriangles.Count}/{alphaTriangles.Count}/" +
                $"{bodyTriangles.Count + faceTriangles.Count + alphaTriangles.Count}.");
        }
        fixture.OpaqueTriangles.AssertEqual(bodyTriangles, context + " body branch");
        VerifyGeneratedDrawOrder(
            target,
            output,
            opaqueOverlay.Skins.Concat(transparent.Skins),
            context);

        Console.WriteLine(
            $"  {context} face-overlay PASS: body={bodyTriangles.Count}; " +
            $"opaque-face={faceTriangles.Count}; alpha={alphaTriangles.Count}; " +
            "face op0/MRS/A0/P1/white; alpha op2/MRS-RS5=1/A1/P1/black; " +
            "face UV coverage A255; " +
            "shared atlas and post-body draw order verified.");
    }

    private static void VerifySavedCandidate(
        SmoDocument target,
        ImportedScene preparedScene,
        string outputPath,
        string context,
        SourceFixture fixture)
    {
        SmoDocument output = SmoDocument.Load(outputPath);
        if (output.HasErrors)
            throw new InvalidOperationException(
                $"{context}: mixed-alpha output failed strict parsing.");

        Dictionary<uint, SmoObjectEntry> outputById = output.Objects
            .ToDictionary(entry => entry.Id);
        VerifyRetainedTargetGraph(target, output, outputById, context);
        VerifyRetainedNativeStates(target, output, outputById, context);

        SmoObjectEntry[] materials = output.Objects.Where(entry =>
                entry.TypeHash == SmoClassIds.MaterialData &&
                entry.Name.StartsWith(AlphaMaterialPrefix, StringComparison.Ordinal))
            .OrderBy(entry => entry.Name, StringComparer.Ordinal)
            .ToArray();
        if (materials.Length != fixture.AlphaRenderables.Count)
        {
            throw new InvalidOperationException(
                $"{context}: expected {fixture.AlphaRenderables.Count} independent " +
                $"alpha material runs, got {materials.Length}.");
        }
        foreach (SmoObjectEntry material in materials)
        {
            ExactMaterialState alphaState = ReadMaterialState(output, material, context);
            if (alphaState.FinalBlendOperation != 2 ||
                !alphaState.MaterialRenderStates.SequenceEqual(
                    SkinnedTransparentSurfaceMaterialRenderStates) ||
                !alphaState.LightingTextureStates.SequenceEqual(
                    SkinnedTransparentSurfaceLightingTextureStates))
            {
                throw new InvalidOperationException(
                    $"{context}: alpha material '{material.Name}' is not the " +
                    "exact shipped skinned-transparent-surface op2 state.");
            }
            if (alphaState.FinalBlendOperation is 4 or 6)
                throw new InvalidOperationException(
                    $"{context}: generated branch regressed to FinalBlendOp " +
                    $"{alphaState.FinalBlendOperation}.");
        }

        SmoObjectEntry[] alphaSkins = output.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin &&
                            entry.Name.StartsWith(AlphaSkinPrefix, StringComparison.Ordinal))
            .OrderBy(entry => entry.Name, StringComparer.Ordinal)
            .ToArray();
        SmoObjectEntry[] alphaMeshes = output.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData &&
                            entry.Name.StartsWith(AlphaMeshPrefix, StringComparison.Ordinal))
            .OrderBy(entry => entry.Name, StringComparer.Ordinal)
            .ToArray();
        if (alphaSkins.Length != fixture.AlphaRenderables.Count ||
            alphaMeshes.Length != alphaSkins.Length)
            throw new InvalidOperationException(
                $"{context}: generated alpha skin/mesh runs must preserve all " +
                $"{fixture.AlphaRenderables.Count} source renderables independently; " +
                $"got skins={alphaSkins.Length}, meshes={alphaMeshes.Length}.");

        HashSet<uint> targetObjectIds = target.Objects.Select(entry => entry.Id).ToHashSet();
        if (alphaSkins.Concat(alphaMeshes).Concat(materials)
            .Any(entry => targetObjectIds.Contains(entry.Id)))
        {
            throw new InvalidOperationException(
                $"{context}: generated alpha branch reused a target object ID.");
        }

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(output);
        HashSet<uint> targetTextureIds = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
            .Select(entry => entry.Id)
            .ToHashSet();
        uint? sharedTextureId = null;
        Dictionary<string, Matrix4x4> expectedInverseBind =
            BuildExpectedTargetInverseBind(target);
        HashSet<uint> targetNodeIds = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Node)
            .Select(entry => entry.Id)
            .ToHashSet();

        for (int index = 0; index < alphaSkins.Length; index++)
        {
            SmoObjectEntry skinEntry = alphaSkins[index];
            if (!SmoSkinDecoder.TryDecode(
                    output, skinEntry, out SmoSkin? skin, out string skinError) || skin is null)
            {
                throw new InvalidOperationException(
                    $"{context}: generated skin '{skinEntry.Name}' is invalid: {skinError}");
            }
            if (skin.AlphaSortEnable != SkinnedTransparentSurfaceAlphaSortEnable ||
                skin.Priority != RenderPriority || skin.Bones.Count != 16)
            {
                throw new InvalidOperationException(
                    $"{context}: generated skin '{skinEntry.Name}' changed " +
                    "skinned-transparent AlphaSort=1/Priority=1/16-slot palette.");
            }
            if (skin.Bones.Select(bone => bone.NodeObjectId).Distinct().Count() > 16)
                throw new InvalidOperationException(
                    $"{context}: generated skin '{skinEntry.Name}' exceeds 16 unique bones.");
            foreach (SmoSkinBone bone in skin.Bones)
            {
                SmoObjectEntry node = output.Objects[bone.NodeObjectIndex];
                if (bone.InlineSerializedSize != 0 ||
                    !targetNodeIds.Contains(bone.NodeObjectId) ||
                    !expectedInverseBind.TryGetValue(node.Name, out Matrix4x4 expected) ||
                    !MatrixApproximatelyEqual(bone.InverseBindMatrix, expected, 0.0001f))
                {
                    throw new InvalidOperationException(
                        $"{context}: generated skin '{skinEntry.Name}' has a non-target palette/IBM.");
                }
            }

            SmoObjectEntry[] directMaterials = output.Objects.Where(entry =>
                entry.ParentIndex == skinEntry.Index &&
                entry.TypeHash == SmoClassIds.MaterialData).ToArray();
            if (directMaterials.Length != 1 ||
                directMaterials[0].Id != materials[index].Id)
            {
                throw new InvalidOperationException(
                    $"{context}: alpha skin '{skinEntry.Name}' did not start its " +
                    "own source-renderable material run.");
            }
            SmoObjectEntry meshEntry = output.Objects.SingleOrDefault(entry =>
                entry.ParentIndex == skinEntry.Index &&
                entry.TypeHash == SmoClassIds.MeshData) ??
                throw new InvalidOperationException(
                    $"{context}: generated skin '{skinEntry.Name}' has no unique mesh.");
            if (!alphaMeshes.Any(entry => entry.Id == meshEntry.Id))
                throw new InvalidOperationException(
                    $"{context}: generated mesh is outside the alpha run.");
            SmoMesh mesh = SmoMeshDecoder.Decode(output, meshEntry);
            ValidateGeneratedMeshWeights(mesh, skin, context);
            if (!mesh.HasDiffuseColors ||
                mesh.DiffuseColorsArgb.Any(color =>
                    color != SkinnedTransparentSurfaceVertexDiffuse))
            {
                throw new InvalidOperationException(
                    $"{context}: generated alpha vertices are not uniform FF000000.");
            }
            if (!bindings.TryGetValue(meshEntry.Index, out SmoTextureBinding? binding) ||
                binding.Issue is not null || binding.Texture is null ||
                binding.MaterialRenderState is null ||
                binding.MaterialRenderState.FinalBlendOperation != 2 ||
                !binding.MaterialRenderState.MaterialRenderStates
                    .SequenceEqual(SkinnedTransparentSurfaceMaterialRenderStates))
            {
                throw new InvalidOperationException(
                    $"{context}: generated alpha mesh has no exact shipped " +
                    "skinned-transparent-surface material binding.");
            }
            uint textureId = output.Objects[binding.Texture.ObjectIndex].Id;
            if (!targetTextureIds.Contains(textureId) ||
                sharedTextureId.HasValue && sharedTextureId.Value != textureId)
            {
                throw new InvalidOperationException(
                    $"{context}: generated alpha skins do not share one retained target texture.");
            }
            sharedTextureId = textureId;
            ReadOnlySpan<byte> pixels = binding.Texture.Bgra32Pixels.Span;
            bool hasAlpha = false;
            bool hasOpaque = false;
            for (int pixel = 3; pixel < pixels.Length; pixel += 4)
            {
                hasAlpha |= pixels[pixel] < byte.MaxValue;
                hasOpaque |= pixels[pixel] == byte.MaxValue;
            }
            if (!hasAlpha || !hasOpaque)
                throw new InvalidOperationException(
                    $"{context}: shared atlas must retain both transparent and opaque texels.");
        }

        HashSet<uint> targetMeshIds = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .Select(entry => entry.Id)
            .ToHashSet();
        SmoObjectEntry[] retainedMeshes = output.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData &&
                            targetMeshIds.Contains(entry.Id))
            .ToArray();
        TriangleMultiset actualOpaque = BuildOutputTriangles(output, retainedMeshes);
        TriangleMultiset actualAlpha = BuildOutputTriangles(output, alphaMeshes);
        bool pendantInAlpha = actualAlpha.Contains(fixture.PendantTriangles);
        if (actualOpaque.Count != 2714 || actualAlpha.Count != 194 ||
            actualOpaque.Count + actualAlpha.Count != 2908 || !pendantInAlpha)
        {
            throw new InvalidOperationException(
                $"{context}: expected opaque/alpha/total triangles 2714/194/2908 " +
                $"with mat6 pendant in alpha, got {actualOpaque.Count}/" +
                $"{actualAlpha.Count}/{actualOpaque.Count + actualAlpha.Count}; " +
                $"pendantInAlpha={pendantInAlpha}.");
        }
        fixture.OpaqueTriangles.AssertEqual(actualOpaque, context + " opaque branch");
        fixture.AlphaTriangles.AssertEqual(actualAlpha, context + " alpha branch");
        for (int index = 0; index < fixture.AlphaRenderables.Count; index++)
        {
            AlphaRenderableFixture expected = fixture.AlphaRenderables[index];
            TriangleMultiset actual = BuildOutputTriangles(output, [alphaMeshes[index]]);
            expected.Triangles.AssertEqual(
                actual,
                $"{context} alpha run {index} ({expected.TextureName})");
            if (actual.Count != expected.Triangles.Count)
            {
                throw new InvalidOperationException(
                    $"{context}: alpha run {index} ({expected.TextureName}) " +
                    $"expected {expected.Triangles.Count} triangles, got {actual.Count}.");
            }
        }

        if (!sharedTextureId.HasValue || !retainedMeshes.Any(entry =>
                bindings.TryGetValue(entry.Index, out SmoTextureBinding? binding) &&
                binding.Texture is not null &&
                output.Objects[binding.Texture.ObjectIndex].Id == sharedTextureId.Value &&
                binding.MaterialRenderState?.FinalBlendOperation is 0 or 2))
        {
            throw new InvalidOperationException(
                $"{context}: opaque and alpha branches do not reference the shared atlas independently.");
        }

        SmoObjectEntry[] unexpectedMeshes = output.Objects.Where(entry =>
            entry.TypeHash == SmoClassIds.MeshData &&
            !targetMeshIds.Contains(entry.Id) &&
            !alphaMeshes.Any(alpha => alpha.Id == entry.Id)).ToArray();
        if (unexpectedMeshes.Any(entry =>
                CountNonDegenerateTriangles(SmoMeshDecoder.Decode(output, entry)) != 0))
        {
            throw new InvalidOperationException(
                $"{context}: non-alpha geometry leaked into an unexpected added mesh.");
        }

        Console.WriteLine(
            $"  {context} alpha split PASS: opaque={actualOpaque.Count}; " +
            $"alpha={actualAlpha.Count}; pendant={fixture.PendantTriangles.Count}; " +
            $"runs={alphaSkins.Length} [" +
            string.Join(",", fixture.AlphaRenderables.Select(item =>
                $"{item.TextureName}:{item.Triangles.Count}")) +
            "]; independent material starts; " +
            "op2 skinned-transparent MRS-RS5=1/A1/P1/black; shared atlas; " +
            "target IBM/weights verified; no op4/op6.");
    }

    private static GeneratedRunSet FindGeneratedRuns(
        SmoDocument output,
        string materialPrefix,
        string skinPrefix,
        string meshPrefix) => new(
        output.Objects.Where(entry =>
                entry.TypeHash == SmoClassIds.MaterialData &&
                entry.Name.StartsWith(materialPrefix, StringComparison.Ordinal))
            .OrderBy(entry => entry.Name, StringComparer.Ordinal)
            .ToArray(),
        output.Objects.Where(entry =>
                entry.TypeHash == SmoClassIds.Skin &&
                entry.Name.StartsWith(skinPrefix, StringComparison.Ordinal))
            .OrderBy(entry => entry.Name, StringComparer.Ordinal)
            .ToArray(),
        output.Objects.Where(entry =>
                entry.TypeHash == SmoClassIds.MeshData &&
                entry.Name.StartsWith(meshPrefix, StringComparison.Ordinal))
            .OrderBy(entry => entry.Name, StringComparer.Ordinal)
            .ToArray());

    private static uint VerifyGeneratedMaterialRuns(
        SmoDocument target,
        SmoDocument output,
        GeneratedRunSet runs,
        IReadOnlyList<AlphaRenderableFixture> expected,
        uint expectedFinalBlend,
        IReadOnlyList<uint> expectedMaterialRenderStates,
        IReadOnlyList<uint> expectedLightingTextureStates,
        uint expectedAlphaSortEnable,
        uint expectedVertexDiffuse,
        string context,
        string label)
    {
        if (runs.Materials.Length != expected.Count ||
            runs.Skins.Length != expected.Count ||
            runs.Meshes.Length != expected.Count)
        {
            throw new InvalidOperationException(
                $"{context}: {label} expected {expected.Count} independent material/skin/mesh " +
                $"runs, got {runs.Materials.Length}/{runs.Skins.Length}/{runs.Meshes.Length}.");
        }
        HashSet<uint> targetObjectIds = target.Objects.Select(entry => entry.Id).ToHashSet();
        if (runs.Materials.Concat(runs.Skins).Concat(runs.Meshes)
            .Any(entry => targetObjectIds.Contains(entry.Id)))
        {
            throw new InvalidOperationException(
                $"{context}: {label} reused a target object ID.");
        }

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(output);
        HashSet<uint> targetTextureIds = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
            .Select(entry => entry.Id)
            .ToHashSet();
        Dictionary<string, Matrix4x4> expectedInverseBind =
            BuildExpectedTargetInverseBind(target);
        HashSet<uint> targetNodeIds = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Node)
            .Select(entry => entry.Id)
            .ToHashSet();
        uint? sharedTextureId = null;

        for (int index = 0; index < expected.Count; index++)
        {
            SmoObjectEntry materialEntry = runs.Materials[index];
            ExactMaterialState material = ReadMaterialState(output, materialEntry, context);
            if (material.FinalBlendOperation != expectedFinalBlend ||
                !material.MaterialRenderStates.SequenceEqual(
                    expectedMaterialRenderStates) ||
                !material.LightingTextureStates.SequenceEqual(
                    expectedLightingTextureStates))
            {
                throw new InvalidOperationException(
                    $"{context}: {label} material '{materialEntry.Name}' has the wrong " +
                    "FinalBlend/MRS/LTS tuple.");
            }

            SmoObjectEntry skinEntry = runs.Skins[index];
            if (!SmoSkinDecoder.TryDecode(
                    output, skinEntry, out SmoSkin? skin, out string skinError) || skin is null)
            {
                throw new InvalidOperationException(
                    $"{context}: {label} skin '{skinEntry.Name}' is invalid: {skinError}");
            }
            if (skin.AlphaSortEnable != expectedAlphaSortEnable ||
                skin.Priority != RenderPriority ||
                skin.Bones.Count != 16)
            {
                throw new InvalidOperationException(
                    $"{context}: {label} skin '{skinEntry.Name}' changed " +
                    $"AlphaSort={expectedAlphaSortEnable}/Priority=1/16-slot palette.");
            }
            foreach (SmoSkinBone bone in skin.Bones)
            {
                SmoObjectEntry node = output.Objects[bone.NodeObjectIndex];
                if (bone.InlineSerializedSize != 0 ||
                    !targetNodeIds.Contains(bone.NodeObjectId) ||
                    !expectedInverseBind.TryGetValue(node.Name, out Matrix4x4 expectedIbm) ||
                    !MatrixApproximatelyEqual(bone.InverseBindMatrix, expectedIbm, 0.0001f))
                {
                    throw new InvalidOperationException(
                        $"{context}: {label} skin '{skinEntry.Name}' has a non-target palette/IBM.");
                }
            }

            SmoObjectEntry[] directMaterials = output.Objects.Where(entry =>
                entry.ParentIndex == skinEntry.Index &&
                entry.TypeHash == SmoClassIds.MaterialData).ToArray();
            SmoObjectEntry[] directMeshes = output.Objects.Where(entry =>
                entry.ParentIndex == skinEntry.Index &&
                entry.TypeHash == SmoClassIds.MeshData).ToArray();
            if (directMaterials.Length != 1 || directMeshes.Length != 1 ||
                directMaterials[0].Id != materialEntry.Id ||
                directMeshes[0].Id != runs.Meshes[index].Id)
            {
                throw new InvalidOperationException(
                    $"{context}: {label} run {index} is not an independent source-renderable run.");
            }

            SmoObjectEntry meshEntry = runs.Meshes[index];
            SmoMesh mesh = SmoMeshDecoder.Decode(output, meshEntry);
            ValidateGeneratedMeshWeights(mesh, skin, context);
            if (!mesh.HasDiffuseColors ||
                mesh.DiffuseColorsArgb.Any(color =>
                    color != expectedVertexDiffuse))
            {
                throw new InvalidOperationException(
                    $"{context}: {label} vertices are not uniform " +
                    $"{expectedVertexDiffuse:X8}.");
            }
            if (!bindings.TryGetValue(meshEntry.Index, out SmoTextureBinding? binding) ||
                binding.Issue is not null || binding.Texture is null ||
                binding.MaterialRenderState is null ||
                binding.MaterialRenderState.FinalBlendOperation != expectedFinalBlend ||
                !binding.MaterialRenderState.MaterialRenderStates.SequenceEqual(
                    expectedMaterialRenderStates))
            {
                throw new InvalidOperationException(
                    $"{context}: {label} mesh has no exact native material binding.");
            }
            uint textureId = output.Objects[binding.Texture.ObjectIndex].Id;
            if (!targetTextureIds.Contains(textureId) ||
                sharedTextureId.HasValue && sharedTextureId.Value != textureId)
            {
                throw new InvalidOperationException(
                    $"{context}: {label} does not share one retained target texture.");
            }
            sharedTextureId = textureId;

            TriangleMultiset actual = BuildOutputTriangles(output, [meshEntry]);
            expected[index].Triangles.AssertEqual(
                actual,
                $"{context} {label} run {index} ({expected[index].TextureName})");
        }
        return sharedTextureId ?? throw new InvalidOperationException(
            $"{context}: {label} has no shared texture.");
    }

    private static void VerifyGeneratedDrawOrder(
        SmoDocument target,
        SmoDocument output,
        IEnumerable<SmoObjectEntry> generatedSkins,
        string context)
    {
        Dictionary<uint, SmoObjectEntry> outputById = output.Objects
            .ToDictionary(entry => entry.Id);
        foreach (IGrouping<uint, SmoObjectEntry> group in generatedSkins.GroupBy(entry =>
                     output.Objects[entry.ParentIndex ?? throw new InvalidOperationException(
                         $"{context}: generated skin has no render parent.")].Id))
        {
            int[] retainedSiblingIndices = target.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.Skin &&
                    entry.ParentIndex is int parentIndex &&
                    target.Objects[parentIndex].Id == group.Key)
                .Select(entry => outputById[entry.Id].Index)
                .ToArray();
            if (retainedSiblingIndices.Length == 0 ||
                group.Any(entry => entry.Index <= retainedSiblingIndices.Max()))
            {
                throw new InvalidOperationException(
                    $"{context}: generated material runs are not serialized after the " +
                    "retained body skins in their render branch.");
            }
        }
    }

    private static SourceFixture InspectSourceFixture(ImportedScene scene, string context)
    {
        var groups = new Dictionary<string, SourceGroup>(StringComparer.OrdinalIgnoreCase);
        for (int meshIndex = 0; meshIndex < scene.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = scene.Meshes[meshIndex];
            if ((uint)mesh.MaterialIndex >= (uint)scene.Materials.Count)
                throw new InvalidOperationException(
                    $"{context}: mesh [{meshIndex}] has no source material.");
            ImportedMaterial material = scene.Materials[mesh.MaterialIndex];
            if ((uint)material.BaseColorTextureIndex >= (uint)scene.Textures.Count)
                throw new InvalidOperationException(
                    $"{context}: material '{material.Name}' has no resolved texture.");
            ImportedTexture texture = scene.Textures[material.BaseColorTextureIndex];
            string name = TextureFileName(texture);
            if (!groups.TryGetValue(name, out SourceGroup? group))
            {
                group = new SourceGroup(texture, []);
                groups.Add(name, group);
            }
            group.Meshes.Add(mesh);
        }
        if (!groups.Keys.ToHashSet(StringComparer.OrdinalIgnoreCase)
            .SetEquals(ExpectedLaylaGroups.Keys))
        {
            throw new InvalidOperationException(
                $"{context}: exact Layla texture groups changed: " +
                string.Join(", ", groups.Keys.Order(StringComparer.OrdinalIgnoreCase)) + ".");
        }

        var opaque = new TriangleMultiset();
        var alpha = new TriangleMultiset();
        var pendant = new TriangleMultiset();
        foreach ((string name, SourceGroup group) in groups)
        {
            int triangles = group.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
            bool usesAlpha = TextureContainsTransparency(group.Texture);
            (int expectedTriangles, bool expectedAlpha) = ExpectedLaylaGroups[name];
            if (triangles != expectedTriangles || usesAlpha != expectedAlpha)
            {
                throw new InvalidOperationException(
                    $"{context}: {name} expected triangles/alpha " +
                    $"{expectedTriangles}/{expectedAlpha}, got {triangles}/{usesAlpha}.");
            }
            TriangleMultiset destination = usesAlpha ? alpha : opaque;
            foreach (ImportedMesh mesh in group.Meshes)
                AddImportedTriangles(destination, mesh);
            if (string.Equals(name, "mat6.png", StringComparison.OrdinalIgnoreCase))
                foreach (ImportedMesh mesh in group.Meshes)
                    AddImportedTriangles(pendant, mesh);
        }
        if (opaque.Count != 2714 || alpha.Count != 194 || pendant.Count != 2)
            throw new InvalidOperationException(
                $"{context}: Layla opacity fixture totals changed.");

        var alphaRenderables = new List<AlphaRenderableFixture>();
        for (int meshIndex = 0; meshIndex < scene.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = scene.Meshes[meshIndex];
            ImportedMaterial material = scene.Materials[mesh.MaterialIndex];
            ImportedTexture texture = scene.Textures[material.BaseColorTextureIndex];
            string name = TextureFileName(texture);
            if (!ExpectedLaylaGroups[name].Alpha)
                continue;
            var triangles = new TriangleMultiset();
            AddImportedTriangles(triangles, mesh);
            alphaRenderables.Add(new AlphaRenderableFixture(
                meshIndex, name, triangles));
        }
        if (!alphaRenderables.Select(item => item.TextureName)
                .SequenceEqual(ExpectedAlphaRenderableOrder, StringComparer.OrdinalIgnoreCase) ||
            !alphaRenderables.Select(item => item.Triangles.Count)
                .SequenceEqual([72, 52, 34, 34, 2]))
        {
            throw new InvalidOperationException(
                $"{context}: Layla source alpha renderable order changed: " +
                string.Join(", ", alphaRenderables.Select(item =>
                    $"key{item.SourceMeshKey}:{item.TextureName}:{item.Triangles.Count}")) + ".");
        }
        return new SourceFixture(
            opaque,
            alpha,
            pendant,
            Array.AsReadOnly(alphaRenderables.ToArray()));
    }

    private static void VerifyRetainedTargetGraph(
        SmoDocument target,
        SmoDocument output,
        IReadOnlyDictionary<uint, SmoObjectEntry> outputById,
        string context)
    {
        foreach (SmoObjectEntry source in target.Objects)
        {
            if (!outputById.TryGetValue(source.Id, out SmoObjectEntry? retained) ||
                retained.TypeHash != source.TypeHash || retained.Name != source.Name)
            {
                throw new InvalidOperationException(
                    $"{context}: target object ID {source.Id} identity changed.");
            }
            uint? sourceParent = source.ParentIndex is int sourceParentIndex
                ? target.Objects[sourceParentIndex].Id
                : null;
            uint? outputParent = retained.ParentIndex is int outputParentIndex
                ? output.Objects[outputParentIndex].Id
                : null;
            if (sourceParent != outputParent)
                throw new InvalidOperationException(
                    $"{context}: target object ID {source.Id} parent changed.");
        }
        if (output.Objects.Select(entry => entry.Id).Distinct().Count() != output.Objects.Count)
            throw new InvalidOperationException(
                $"{context}: output contains duplicate object IDs.");
    }

    private static void VerifyRetainedNativeStates(
        SmoDocument target,
        SmoDocument output,
        IReadOnlyDictionary<uint, SmoObjectEntry> outputById,
        string context)
    {
        foreach (SmoObjectEntry material in target.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MaterialData))
        {
            ExactMaterialState before = ReadMaterialState(target, material, context);
            ExactMaterialState after = ReadMaterialState(
                output, outputById[material.Id], context);
            if (before.FinalBlendOperation != after.FinalBlendOperation ||
                !before.MaterialRenderStates.SequenceEqual(after.MaterialRenderStates) ||
                !before.LightingTextureStates.SequenceEqual(after.LightingTextureStates))
            {
                throw new InvalidOperationException(
                    $"{context}: retained material '{material.Name}' changed its " +
                    "FinalBlend/depth/lighting tuple.");
            }
        }
        foreach (SmoObjectEntry skinEntry in target.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            string afterError = string.Empty;
            if (!SmoSkinDecoder.TryDecode(
                    target, skinEntry, out SmoSkin? before, out string beforeError) ||
                before is null ||
                !SmoSkinDecoder.TryDecode(
                    output, outputById[skinEntry.Id], out SmoSkin? after,
                    out afterError) || after is null)
            {
                throw new InvalidOperationException(
                    $"{context}: retained skin state cannot be decoded: " +
                    $"{beforeError} {afterError}");
            }
            if (before.AlphaSortEnable != after.AlphaSortEnable ||
                before.Priority != after.Priority)
            {
                throw new InvalidOperationException(
                    $"{context}: retained skin '{skinEntry.Name}' changed sort/priority.");
            }
        }
    }

    private static ExactMaterialState ReadMaterialState(
        SmoDocument document,
        SmoObjectEntry material,
        string context)
    {
        if (!SmoMaterialRenderState.TryDecode(
                document, material, out SmoMaterialRenderStateInfo? decoded) ||
            decoded is null)
        {
            throw new InvalidOperationException(
                $"{context}: material [{material.Index}] '{material.Name}' has no render state.");
        }
        ReadOnlySpan<byte> data = document.Data.Span.Slice(
            checked((int)material.PhysicalOffset),
            checked((int)material.SerializedSize));
        uint[]? lighting = null;
        int offset = 8;
        while (offset < data.Length &&
               SmoDataBlockReader.TryReadHeader(data, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == 17 && field.PayloadSize == 9 * sizeof(uint))
            {
                lighting = new uint[9];
                for (int index = 0; index < lighting.Length; index++)
                {
                    lighting[index] = BinaryPrimitives.ReadUInt32LittleEndian(
                        data.Slice(
                            field.PayloadOffset + index * sizeof(uint),
                            sizeof(uint)));
                }
                break;
            }
            int next = checked((int)field.PayloadEnd);
            if (next <= offset)
                break;
            offset = next;
        }
        return new ExactMaterialState(
            decoded.FinalBlendOperation,
            decoded.MaterialRenderStates.ToArray(),
            lighting ?? []);
    }

    private static Dictionary<string, Matrix4x4> BuildExpectedTargetInverseBind(
        SmoDocument target)
    {
        IReadOnlyDictionary<int, Matrix4x4> bind =
            SmoSkinBindingResolver.ResolveBindWorldMatrices(target);
        var result = new Dictionary<string, Matrix4x4>(StringComparer.Ordinal);
        foreach ((int objectIndex, Matrix4x4 world) in bind)
        {
            SmoObjectEntry entry = target.Objects[objectIndex];
            if (entry.TypeHash != SmoClassIds.Node ||
                !Matrix4x4.Invert(world, out Matrix4x4 inverse))
                continue;
            result[entry.Name] = inverse;
        }
        return result;
    }

    private static void ValidateGeneratedMeshWeights(
        SmoMesh mesh,
        SmoSkin skin,
        string context)
    {
        if (!mesh.HasSkinningData)
            throw new InvalidOperationException(
                $"{context}: generated mesh '{mesh.Name}' has no skinning data.");
        for (int vertex = 0; vertex < mesh.VertexCount; vertex++)
        {
            Vector4 weights = mesh.BlendWeights[vertex];
            SmoBlendIndices joints = mesh.BlendIndices[vertex];
            float[] values = [weights.X, weights.Y, weights.Z, weights.W];
            byte[] indices = [joints.X, joints.Y, joints.Z, joints.W];
            float sum = 0;
            for (int influence = 0; influence < 4; influence++)
            {
                float weight = values[influence];
                if (!float.IsFinite(weight) || weight < 0 ||
                    weight > WeightEpsilon && indices[influence] >= skin.Bones.Count)
                {
                    throw new InvalidOperationException(
                        $"{context}: generated mesh '{mesh.Name}' vertex {vertex} " +
                        "has an invalid weight/palette index.");
                }
                if (weight > WeightEpsilon)
                    sum += weight;
            }
            if (MathF.Abs(sum - 1) > 0.0001f)
                throw new InvalidOperationException(
                    $"{context}: generated mesh '{mesh.Name}' vertex {vertex} " +
                    $"weights sum to {sum:G9}.");
        }
    }

    private static TriangleMultiset BuildOutputTriangles(
        SmoDocument document,
        IEnumerable<SmoObjectEntry> entries)
    {
        var result = new TriangleMultiset();
        foreach (SmoObjectEntry entry in entries)
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(document, entry);
            for (int triangle = 0; triangle < mesh.TriangleIndices.Length / 3; triangle++)
            {
                int offset = triangle * 3;
                uint first = mesh.TriangleIndices[offset];
                uint second = mesh.TriangleIndices[offset + 1];
                uint third = mesh.TriangleIndices[offset + 2];
                if (first == second || second == third || first == third)
                    continue;
                result.Add(TriangleKey.Create(
                    mesh.Positions[checked((int)first)],
                    mesh.Positions[checked((int)second)],
                    mesh.Positions[checked((int)third)]));
            }
        }
        return result;
    }

    private static void AddImportedTriangles(TriangleMultiset destination, ImportedMesh mesh)
    {
        for (int triangle = 0; triangle < mesh.TriangleIndices.Length / 3; triangle++)
        {
            int offset = triangle * 3;
            Vector3 first = ToSmoSpace(mesh.Positions[checked((int)mesh.TriangleIndices[offset])]);
            Vector3 second = ToSmoSpace(mesh.Positions[checked((int)mesh.TriangleIndices[offset + 1])]);
            Vector3 third = ToSmoSpace(mesh.Positions[checked((int)mesh.TriangleIndices[offset + 2])]);
            destination.Add(TriangleKey.Create(first, second, third));
        }
    }

    private static Vector3 ToSmoSpace(Vector3 value) =>
        new(value.X, value.Y, -value.Z);

    private static bool TextureContainsTransparency(ImportedTexture texture)
    {
        using Image<Rgba32> image = Image.Load<Rgba32>(texture.Data);
        if (image.Width != texture.Width || image.Height != texture.Height)
            throw new InvalidDataException(
                $"Texture {texture.Name} declares the wrong dimensions.");
        bool result = false;
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < accessor.Height && !result; y++)
                foreach (Rgba32 pixel in accessor.GetRowSpan(y))
                    if (pixel.A < byte.MaxValue)
                    {
                        result = true;
                        break;
                    }
        });
        return result;
    }

    private static void VerifyPreviewMaterialIsolation(
        ImportedScene source,
        ImportedScene autoPreview,
        ImportedScene explicitPreview,
        SourceFixture fixture,
        string context)
    {
        const int wrappedGutter = 4;
        if (autoPreview.Meshes.Count != explicitPreview.Meshes.Count ||
            autoPreview.Meshes.Count != source.Meshes.Count ||
            autoPreview.Textures.Count != 1 || explicitPreview.Textures.Count != 1)
        {
            throw new InvalidOperationException(
                $"{context}: Auto/explicit previews do not share one mesh/atlas layout.");
        }
        for (int meshIndex = 0; meshIndex < source.Meshes.Count; meshIndex++)
        {
            ImportedMesh autoMesh = autoPreview.Meshes[meshIndex];
            ImportedMesh explicitMesh = explicitPreview.Meshes[meshIndex];
            if (autoMesh.MaterialIndex != explicitMesh.MaterialIndex ||
                !autoMesh.TextureCoordinates.SequenceEqual(
                    explicitMesh.TextureCoordinates))
            {
                throw new InvalidOperationException(
                    $"{context}: material override changed atlas UV/layout of mesh " +
                    $"[{meshIndex}] '{source.Meshes[meshIndex].Name}'.");
            }
        }

        using Image<Rgba32> autoAtlas =
            Image.Load<Rgba32>(autoPreview.Textures[0].Data);
        using Image<Rgba32> explicitAtlas =
            Image.Load<Rgba32>(explicitPreview.Textures[0].Data);
        if (autoAtlas.Width != explicitAtlas.Width ||
            autoAtlas.Height != explicitAtlas.Height)
        {
            throw new InvalidOperationException(
                $"{context}: material override changed atlas dimensions.");
        }

        bool[] faceCellPixels = new bool[checked(autoAtlas.Width * autoAtlas.Height)];
        int exactFacePixels = 0;
        foreach (AlphaRenderableFixture item in fixture.AlphaRenderables.Where(item =>
                     ExpectedOpaqueOverlayOrder.Contains(
                         item.TextureName, StringComparer.OrdinalIgnoreCase)))
        {
            ImportedMesh sourceMesh = source.Meshes[item.SourceMeshKey];
            AtlasCell cell = DeriveAtlasCell(
                sourceMesh,
                explicitPreview.Meshes[item.SourceMeshKey],
                explicitAtlas.Width,
                explicitAtlas.Height,
                wrappedGutter,
                context);
            using Image<Rgba32> sourceImage = Image.Load<Rgba32>(
                source.Textures[source.Materials[sourceMesh.MaterialIndex]
                    .BaseColorTextureIndex].Data);
            using Image<Rgba32> alphaOnly = sourceImage.Clone();
            alphaOnly.ProcessPixelRows(accessor =>
            {
                for (int y = 0; y < accessor.Height; y++)
                {
                    Span<Rgba32> row = accessor.GetRowSpan(y);
                    for (int x = 0; x < row.Length; x++)
                    {
                        Rgba32 before = row[x];
                        row[x] = new Rgba32(before.R, before.G, before.B, byte.MaxValue);
                    }
                }
            });
            using Image<Rgba32> expected = alphaOnly.Clone(operation => operation.Resize(
                new ResizeOptions
                {
                    Size = new Size(cell.ContentWidth, cell.ContentHeight),
                    Mode = ResizeMode.Stretch,
                    Sampler = KnownResamplers.Bicubic,
                    PremultiplyAlpha = true
                }));
            for (int y = cell.Top; y < cell.Bottom; y++)
            for (int x = cell.Left; x < cell.Right; x++)
            {
                int pixelIndex = checked(y * explicitAtlas.Width + x);
                if (faceCellPixels[pixelIndex])
                    throw new InvalidOperationException(
                        $"{context}: opaque face atlas cells overlap.");
                faceCellPixels[pixelIndex] = true;
                Rgba32 expectedPixel = expected[
                    Mod(x - cell.ContentLeft, cell.ContentWidth),
                    Mod(y - cell.ContentTop, cell.ContentHeight)];
                Rgba32 actualPixel = explicitAtlas[x, y];
                if (!actualPixel.Equals(expectedPixel) ||
                    actualPixel.A != byte.MaxValue)
                {
                    throw new InvalidOperationException(
                        $"{context}: {item.TextureName} atlas cell differs from an " +
                        "alpha-only source edit before premultiplied resize.");
                }
                exactFacePixels++;
            }
        }

        int exactTransparentPixels = 0;
        foreach (AlphaRenderableFixture item in fixture.AlphaRenderables.Where(item =>
                     ExpectedTransparentSurfaceOrder.Contains(
                         item.TextureName, StringComparer.OrdinalIgnoreCase)))
        {
            ImportedMesh sourceMesh = source.Meshes[item.SourceMeshKey];
            AtlasCell autoCell = DeriveAtlasCell(
                sourceMesh,
                autoPreview.Meshes[item.SourceMeshKey],
                autoAtlas.Width,
                autoAtlas.Height,
                wrappedGutter,
                context);
            AtlasCell explicitCell = DeriveAtlasCell(
                sourceMesh,
                explicitPreview.Meshes[item.SourceMeshKey],
                explicitAtlas.Width,
                explicitAtlas.Height,
                wrappedGutter,
                context);
            if (autoCell != explicitCell)
                throw new InvalidOperationException(
                    $"{context}: {item.TextureName} atlas cell moved relative to Auto.");
            for (int y = autoCell.Top; y < autoCell.Bottom; y++)
            for (int x = autoCell.Left; x < autoCell.Right; x++)
            {
                if (!autoAtlas[x, y].Equals(explicitAtlas[x, y]))
                    throw new InvalidOperationException(
                        $"{context}: {item.TextureName} RGBA changed relative to Auto.");
                exactTransparentPixels++;
            }
        }

        int exactNonFacePixels = 0;
        for (int y = 0; y < autoAtlas.Height; y++)
        for (int x = 0; x < autoAtlas.Width; x++)
        {
            if (faceCellPixels[checked(y * autoAtlas.Width + x)])
                continue;
            if (!autoAtlas[x, y].Equals(explicitAtlas[x, y]))
                throw new InvalidOperationException(
                    $"{context}: atlas pixel ({x}, {y}) outside mat3/mat4 changed " +
                    "relative to Auto.");
            exactNonFacePixels++;
        }
        if (exactFacePixels == 0 || exactTransparentPixels == 0 ||
            exactNonFacePixels == 0)
        {
            throw new InvalidOperationException(
                $"{context}: material-isolation regression sampled no atlas pixels.");
        }
        Console.WriteLine(
            $"  {context} atlas isolation: mat3/mat4 {exactFacePixels} pixels " +
            "match alpha-only source edits; " +
            $"mat5/mat7/mat6 {exactTransparentPixels} and all " +
            $"{exactNonFacePixels} non-face pixels are RGBA-identical to Auto; " +
            "all mesh UVs identical.");
    }

    private static AtlasCell DeriveAtlasCell(
        ImportedMesh source,
        ImportedMesh preview,
        int atlasWidth,
        int atlasHeight,
        int wrappedGutter,
        string context)
    {
        (float scaleX, float offsetX) = FitUvAxis(
            source.TextureCoordinates,
            preview.TextureCoordinates,
            value => value.X,
            context);
        (float scaleY, float offsetY) = FitUvAxis(
            source.TextureCoordinates,
            preview.TextureCoordinates,
            value => value.Y,
            context);
        int contentLeft = ExactAtlasInteger(offsetX * atlasWidth, context);
        int contentTop = ExactAtlasInteger(offsetY * atlasHeight, context);
        int contentWidth = ExactAtlasInteger(scaleX * atlasWidth, context);
        int contentHeight = ExactAtlasInteger(scaleY * atlasHeight, context);
        var result = new AtlasCell(
            contentLeft - wrappedGutter,
            contentTop - wrappedGutter,
            contentLeft + contentWidth + wrappedGutter,
            contentTop + contentHeight + wrappedGutter,
            contentLeft,
            contentTop,
            contentWidth,
            contentHeight);
        if (contentWidth <= 0 || contentHeight <= 0 || result.Left < 0 ||
            result.Top < 0 || result.Right > atlasWidth || result.Bottom > atlasHeight)
        {
            throw new InvalidOperationException(
                $"{context}: derived atlas cell is outside the texture.");
        }
        return result;
    }

    private static int ExactAtlasInteger(float value, string context)
    {
        int result = checked((int)MathF.Round(value));
        if (MathF.Abs(value - result) > 0.0001f)
            throw new InvalidOperationException(
                $"{context}: atlas placement {value:G9} is not an integer pixel.");
        return result;
    }

    private static int Mod(int value, int modulus)
    {
        int result = value % modulus;
        return result < 0 ? result + modulus : result;
    }

    private static void VerifySavedAtlasMatchesPreview(
        string outputPath,
        ImportedScene preview,
        string context)
    {
        SmoDocument output = SmoDocument.Load(outputPath);
        SmoObjectEntry meshEntry = output.Objects.First(entry =>
            entry.TypeHash == SmoClassIds.MeshData &&
            entry.Name.StartsWith(OpaqueOverlayMeshPrefix, StringComparison.Ordinal));
        SmoTextureBinding binding = SmoTextureBindingResolver.ResolveAll(output)[meshEntry.Index];
        SmoTexture written = binding.Texture ?? throw new InvalidOperationException(
            $"{context}: written face overlay has no atlas texture.");
        using Image<Rgba32> expected = Image.Load<Rgba32>(preview.Textures.Single().Data);
        if (written.Width != expected.Width || written.Height != expected.Height ||
            written.Bgra32Pixels.Length != checked(expected.Width * expected.Height * 4))
        {
            throw new InvalidOperationException(
                $"{context}: written atlas dimensions differ from final preview.");
        }
        ReadOnlySpan<byte> actual = written.Bgra32Pixels.Span;
        int offset = 0;
        for (int y = 0; y < expected.Height; y++)
        for (int x = 0; x < expected.Width; x++, offset += 4)
        {
            Rgba32 pixel = expected[x, y];
            if (actual[offset] != pixel.B || actual[offset + 1] != pixel.G ||
                actual[offset + 2] != pixel.R || actual[offset + 3] != pixel.A)
            {
                throw new InvalidOperationException(
                    $"{context}: written atlas pixel ({x}, {y}) differs from final preview.");
            }
        }
    }

    private static void VerifyPreviewFormerTransparentRgb(
        ImportedScene source,
        ImportedScene preview,
        string context)
    {
        if (preview.Textures.Count != 1)
            throw new InvalidOperationException(
                $"{context}: RGB preservation requires the one-texture preview atlas.");
        using Image<Rgba32> atlas = Image.Load<Rgba32>(preview.Textures[0].Data);
        int representative = 0;
        int preserved = 0;
        foreach (int meshKey in new[] { 1, 2 })
        {
            ImportedMesh sourceMesh = source.Meshes[meshKey];
            ImportedMesh previewMesh = preview.Meshes[meshKey];
            ImportedMaterial material = source.Materials[sourceMesh.MaterialIndex];
            ImportedTexture texture = source.Textures[material.BaseColorTextureIndex];
            using Image<Rgba32> sourceImage = Image.Load<Rgba32>(texture.Data);
            (float scaleX, float offsetX) = FitUvAxis(
                sourceMesh.TextureCoordinates,
                previewMesh.TextureCoordinates,
                value => value.X,
                context);
            (float scaleY, float offsetY) = FitUvAxis(
                sourceMesh.TextureCoordinates,
                previewMesh.TextureCoordinates,
                value => value.Y,
                context);
            if (scaleX <= 0 || scaleY <= 0)
                throw new InvalidOperationException(
                    $"{context}: atlas face UV mapping is mirrored or degenerate.");

            float minU = previewMesh.TextureCoordinates.Min(value => value.X);
            float maxU = previewMesh.TextureCoordinates.Max(value => value.X);
            float minV = previewMesh.TextureCoordinates.Min(value => value.Y);
            float maxV = previewMesh.TextureCoordinates.Max(value => value.Y);
            int minX = Math.Clamp(
                (int)MathF.Ceiling(minU * atlas.Width - 0.5f), 0, atlas.Width - 1);
            int maxX = Math.Clamp(
                (int)MathF.Floor(maxU * atlas.Width - 0.5f), 0, atlas.Width - 1);
            int minY = Math.Clamp(
                (int)MathF.Ceiling(minV * atlas.Height - 0.5f), 0, atlas.Height - 1);
            int maxY = Math.Clamp(
                (int)MathF.Floor(maxV * atlas.Height - 0.5f), 0, atlas.Height - 1);
            for (int y = minY; y <= maxY; y++)
            for (int x = minX; x <= maxX; x++)
            {
                Vector2 atlasUv = new(
                    (x + 0.5f) / atlas.Width,
                    (y + 0.5f) / atlas.Height);
                Vector2 sourceUv = new(
                    (atlasUv.X - offsetX) / scaleX,
                    (atlasUv.Y - offsetY) / scaleY);
                if (sourceUv.X <= 0 || sourceUv.X >= 1 ||
                    sourceUv.Y <= 0 || sourceUv.Y >= 1 ||
                    !MeshUvContains(sourceMesh, sourceUv))
                    continue;
                int sourceX = Math.Clamp(
                    (int)(sourceUv.X * sourceImage.Width), 0, sourceImage.Width - 1);
                int sourceY = Math.Clamp(
                    (int)(sourceUv.Y * sourceImage.Height), 0, sourceImage.Height - 1);
                if (sourceX <= 0 || sourceX >= sourceImage.Width - 1 ||
                    sourceY <= 0 || sourceY >= sourceImage.Height - 1 ||
                    !NeighborhoodWasHidden(sourceImage, sourceX, sourceY))
                    continue;
                Rgba32 expected = sourceImage[sourceX, sourceY];
                if (expected.R + expected.G + expected.B < 120)
                    continue;
                representative++;
                Rgba32 actual = atlas[x, y];
                int difference = Math.Abs(actual.R - expected.R) +
                                 Math.Abs(actual.G - expected.G) +
                                 Math.Abs(actual.B - expected.B);
                if (actual.A == byte.MaxValue &&
                    actual.R + actual.G + actual.B >= 80 &&
                    difference <= 180)
                    preserved++;
            }
        }
        if (representative < 8 || preserved * 4 < representative * 3)
        {
            throw new InvalidOperationException(
                $"{context}: RGB under former A0 face texels did not survive the " +
                $"premultiplied atlas resize; preserved={preserved}/{representative}.");
        }
        Console.WriteLine(
            $"  {context} former-A0 face RGB preserved: " +
            $"{preserved}/{representative} representative atlas texels.");

        static bool NeighborhoodWasHidden(Image<Rgba32> image, int x, int y)
        {
            for (int offsetY = -1; offsetY <= 1; offsetY++)
            for (int offsetX = -1; offsetX <= 1; offsetX++)
                if (image[x + offsetX, y + offsetY].A != 0)
                    return false;
            return true;
        }
    }

    private static (float Scale, float Offset) FitUvAxis(
        IReadOnlyList<Vector2> source,
        IReadOnlyList<Vector2> destination,
        Func<Vector2, float> select,
        string context)
    {
        if (source.Count != destination.Count || source.Count == 0)
            throw new InvalidOperationException(
                $"{context}: source/preview UV identity changed before atlas fitting.");
        float sourceMean = source.Average(select);
        float destinationMean = destination.Average(select);
        float covariance = 0;
        float variance = 0;
        for (int index = 0; index < source.Count; index++)
        {
            float centered = select(source[index]) - sourceMean;
            covariance += centered * (select(destination[index]) - destinationMean);
            variance += centered * centered;
        }
        if (variance <= 0.0000001f)
            throw new InvalidOperationException(
                $"{context}: source face UV axis is degenerate.");
        float scale = covariance / variance;
        float offset = destinationMean - scale * sourceMean;
        float maximumResidual = source.Select((value, index) => MathF.Abs(
                select(destination[index]) - (select(value) * scale + offset)))
            .Max();
        if (maximumResidual > 0.00001f)
            throw new InvalidOperationException(
                $"{context}: atlas UV mapping is not an affine cell transform " +
                $"(residual {maximumResidual:G9}).");
        return (scale, offset);
    }

    private static bool MeshUvContains(ImportedMesh mesh, Vector2 point)
    {
        for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
        {
            Vector2 a = mesh.TextureCoordinates[
                checked((int)mesh.TriangleIndices[index])];
            Vector2 b = mesh.TextureCoordinates[
                checked((int)mesh.TriangleIndices[index + 1])];
            Vector2 c = mesh.TextureCoordinates[
                checked((int)mesh.TriangleIndices[index + 2])];
            float edge0 = CrossUv(b - a, point - a);
            float edge1 = CrossUv(c - b, point - b);
            float edge2 = CrossUv(a - c, point - c);
            bool negative = edge0 < -0.0000001f || edge1 < -0.0000001f ||
                            edge2 < -0.0000001f;
            bool positive = edge0 > 0.0000001f || edge1 > 0.0000001f ||
                            edge2 > 0.0000001f;
            if (!(negative && positive))
                return true;
        }
        return false;
    }

    private static float CrossUv(Vector2 left, Vector2 right) =>
        left.X * right.Y - left.Y * right.X;

    private static string TextureFileName(ImportedTexture texture)
    {
        string source = string.IsNullOrWhiteSpace(texture.SourcePath)
            ? texture.Name
            : texture.SourcePath;
        string name = Path.GetFileName(source);
        return string.IsNullOrWhiteSpace(name) ? source : name;
    }

    private static int CountNonDegenerateTriangles(SmoMesh mesh) =>
        Enumerable.Range(0, mesh.TriangleIndices.Length / 3).Count(triangle =>
        {
            uint first = mesh.TriangleIndices[triangle * 3];
            uint second = mesh.TriangleIndices[triangle * 3 + 1];
            uint third = mesh.TriangleIndices[triangle * 3 + 2];
            return first != second && second != third && first != third;
        });

    private static bool MatrixApproximatelyEqual(
        Matrix4x4 left,
        Matrix4x4 right,
        float epsilon)
    {
        float[] differences =
        [
            left.M11 - right.M11, left.M12 - right.M12,
            left.M13 - right.M13, left.M14 - right.M14,
            left.M21 - right.M21, left.M22 - right.M22,
            left.M23 - right.M23, left.M24 - right.M24,
            left.M31 - right.M31, left.M32 - right.M32,
            left.M33 - right.M33, left.M34 - right.M34,
            left.M41 - right.M41, left.M42 - right.M42,
            left.M43 - right.M43, left.M44 - right.M44
        ];
        return differences.All(value => MathF.Abs(value) <= epsilon);
    }

    private static string FingerprintImportedScene(ImportedScene scene)
    {
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        foreach (ImportedMesh mesh in scene.Meshes)
        {
            AppendString(mesh.Name);
            AppendInt(mesh.MaterialIndex);
            foreach (Vector3 value in mesh.Positions)
            {
                AppendFloat(value.X); AppendFloat(value.Y); AppendFloat(value.Z);
            }
            foreach (Vector3 value in mesh.Normals)
            {
                AppendFloat(value.X); AppendFloat(value.Y); AppendFloat(value.Z);
            }
            foreach (Vector2 value in mesh.TextureCoordinates)
            {
                AppendFloat(value.X); AppendFloat(value.Y);
            }
            foreach (uint value in mesh.TriangleIndices)
                AppendUInt(value);
            if (mesh.Skinning is not null)
            {
                foreach (ImportedJointIndices value in mesh.Skinning.JointIndices)
                {
                    AppendUShort(value.X); AppendUShort(value.Y);
                    AppendUShort(value.Z); AppendUShort(value.W);
                }
                foreach (Vector4 value in mesh.Skinning.Weights)
                {
                    AppendFloat(value.X); AppendFloat(value.Y);
                    AppendFloat(value.Z); AppendFloat(value.W);
                }
            }
        }
        foreach (ImportedTexture texture in scene.Textures)
        {
            AppendString(texture.Name);
            hash.AppendData(texture.Data);
        }
        return Convert.ToHexString(hash.GetHashAndReset());

        void AppendString(string value) =>
            hash.AppendData(System.Text.Encoding.UTF8.GetBytes(value));
        void AppendInt(int value) => AppendUInt(unchecked((uint)value));
        void AppendUInt(uint value)
        {
            Span<byte> bytes = stackalloc byte[4];
            BinaryPrimitives.WriteUInt32LittleEndian(bytes, value);
            hash.AppendData(bytes);
        }
        void AppendUShort(ushort value)
        {
            Span<byte> bytes = stackalloc byte[2];
            BinaryPrimitives.WriteUInt16LittleEndian(bytes, value);
            hash.AppendData(bytes);
        }
        void AppendFloat(float value) =>
            AppendUInt(unchecked((uint)BitConverter.SingleToInt32Bits(value)));
    }

    private sealed record SourceFixture(
        TriangleMultiset OpaqueTriangles,
        TriangleMultiset AlphaTriangles,
        TriangleMultiset PendantTriangles,
        IReadOnlyList<AlphaRenderableFixture> AlphaRenderables)
    {
        public int TotalTriangles => OpaqueTriangles.Count + AlphaTriangles.Count;
    }

    private sealed record AlphaRenderableFixture(
        int SourceMeshKey,
        string TextureName,
        TriangleMultiset Triangles);

    private sealed record ExactMaterialState(
        uint FinalBlendOperation,
        uint[] MaterialRenderStates,
        uint[] LightingTextureStates);

    private sealed record GeneratedRunSet(
        SmoObjectEntry[] Materials,
        SmoObjectEntry[] Skins,
        SmoObjectEntry[] Meshes);

    private readonly record struct AtlasCell(
        int Left,
        int Top,
        int Right,
        int Bottom,
        int ContentLeft,
        int ContentTop,
        int ContentWidth,
        int ContentHeight);

    private sealed record SourceGroup(ImportedTexture Texture, List<ImportedMesh> Meshes);

    private sealed class TriangleMultiset
    {
        private readonly Dictionary<TriangleKey, int> _counts = [];

        public int Count { get; private set; }

        public void Add(TriangleKey key)
        {
            _counts[key] = _counts.GetValueOrDefault(key) + 1;
            Count++;
        }

        public bool Contains(TriangleMultiset required) =>
            required._counts.All(pair => _counts.GetValueOrDefault(pair.Key) >= pair.Value);

        public void AssertEqual(TriangleMultiset actual, string context)
        {
            if (Count == actual.Count &&
                _counts.Count == actual._counts.Count &&
                _counts.All(pair => actual._counts.GetValueOrDefault(pair.Key) == pair.Value))
                return;
            TriangleKey? missing = _counts.FirstOrDefault(pair =>
                actual._counts.GetValueOrDefault(pair.Key) != pair.Value).Key;
            TriangleKey? unexpected = actual._counts.FirstOrDefault(pair =>
                _counts.GetValueOrDefault(pair.Key) != pair.Value).Key;
            throw new InvalidOperationException(
                $"{context}: triangle geometry multiset changed; expected/actual " +
                $"{Count}/{actual.Count}; missing={missing}; unexpected={unexpected}.");
        }
    }

    private readonly record struct TriangleKey(VertexKey A, VertexKey B, VertexKey C)
    {
        public static TriangleKey Create(Vector3 first, Vector3 second, Vector3 third)
        {
            VertexKey[] vertices =
                [VertexKey.Create(first), VertexKey.Create(second), VertexKey.Create(third)];
            Array.Sort(vertices);
            return new TriangleKey(vertices[0], vertices[1], vertices[2]);
        }
    }

    private readonly record struct VertexKey(int X, int Y, int Z) : IComparable<VertexKey>
    {
        public static VertexKey Create(Vector3 value) => new(
            BitConverter.SingleToInt32Bits(value.X),
            BitConverter.SingleToInt32Bits(value.Y),
            BitConverter.SingleToInt32Bits(value.Z));

        public int CompareTo(VertexKey other)
        {
            int result = X.CompareTo(other.X);
            if (result != 0) return result;
            result = Y.CompareTo(other.Y);
            return result != 0 ? result : Z.CompareTo(other.Z);
        }
    }
}
