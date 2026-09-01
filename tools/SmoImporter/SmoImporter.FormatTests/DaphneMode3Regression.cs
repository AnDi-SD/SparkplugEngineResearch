using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class DaphneMode3Regression
{
    private const float FixtureScale = 84.5f;
    private const int ExpectedMeshCount = 4;
    private const int ExpectedVertexCount = 1701;
    private const int ExpectedTriangleCount = 2158;
    private const string OpaqueBodyMeshPrefix = "imp_b_x_";
    private const string AlphaMeshPrefix = "imp_a_x_";
    private const string OpaqueOverlayMeshPrefix = "imp_o_x_";

    private static readonly Vector3 FixtureTranslation = new(0, 12, -3);

    public static void Run(string targetArgument, string donorArgument, string outputArgument)
    {
        string targetPath = RequireInput(targetArgument, ".smo", "target SMO");
        string donorPath = RequireInput(donorArgument, ".obj", "Daphne OBJ donor");
        string outputPath = Path.GetFullPath(outputArgument);
        if (!Path.GetExtension(outputPath).Equals(
                ".smo", StringComparison.OrdinalIgnoreCase))
        {
            throw new ArgumentException("Daphne regression output must be a .smo file.");
        }
        EnsureSeparateOutput(targetPath, donorPath, outputPath);
        if (File.Exists(outputPath))
        {
            throw new InvalidOperationException(
                "Daphne regression output already exists; choose a new output path.");
        }

        byte[] targetBytesBefore = File.ReadAllBytes(targetPath);
        IReadOnlyDictionary<string, string> donorFilesBefore = SnapshotDonorDirectory(
            donorPath, outputPath);
        SmoDocument target = SmoDocument.Load(targetPath);
        if (target.HasErrors)
            throw new InvalidDataException("Daphne regression target failed strict parsing.");

        OldVisualSnapshot oldVisualsBefore = CaptureOldVisuals(target);
        SmoObjectEntry[] targetTextureEntries = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
            .ToArray();
        if (targetTextureEntries.Length != 2)
        {
            throw new InvalidDataException(
                $"Daphne fixture target must have exactly two TextureData objects; " +
                $"got {targetTextureEntries.Length}.");
        }
        ImportedScene rawDonor = ImportedModelReader.ReadGeometryOnly(donorPath);
        ImportedTexture[] externalTextures = ReadDonorDirectoryTextures(
            donorPath, rawDonor);
        ImportedTextureCatalogResult catalog = ImportedTextureCatalog.ResolveExternalOverrides(
            rawDonor, Array.AsReadOnly(externalTextures));
        ImportedScene donor = catalog.EffectiveScene;
        VerifyDaphneDonorFixture(donor, externalTextures, catalog);
        string donorFingerprintBefore = FingerprintImportedScene(donor);

        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
        SmoExportScene targetScene = SmoSceneBuilder.Build(
            target,
            new SmoExportOptions(
                ApplyWorldTransforms: true,
                AnimationPaths: null,
                Resources: SmoExportResourceTypes.Meshes |
                           SmoExportResourceTypes.Skeleton));
        var alignment = new ReplacementTransform(
            FixtureScale,
            Vector3.Zero,
            FixtureTranslation);
        TargetRigAutomaticPoseFitResult fit = TargetRigAutomaticPoseFitter.Fit(
            rig,
            targetScene,
            donor,
            alignment);
        if (fit.BodySelection.TotalComponentCount != 26 ||
            fit.BodySelection.Components.Count == 0 ||
            fit.BodySelection.ExcludedComponentCount !=
                fit.BodySelection.TotalComponentCount - fit.BodySelection.Components.Count ||
            !Equals(fit.BodySelection.DonorAlignment, alignment))
        {
            throw new InvalidDataException(
                "Daphne human-mode body selection is incomplete or internally inconsistent: " +
                $"selected {fit.BodySelection.Components.Count}/" +
                $"{fit.BodySelection.TotalComponentCount}.");
        }

        GeneratedSkinningPreparationResult preparation =
            GeneratedSkinningPreparer.Prepare(
                target,
                donor,
                fit.Pose,
                alignment,
                fit.BodySelection);
        HashSet<int> automaticRigidIndices = preparation.Analysis.Attachments
            .Select(attachment => attachment.ComponentIndex)
            .ToHashSet();
        TargetRigSelectedBodyComponent fittingOnlyProbe = fit.BodySelection.Components
            .FirstOrDefault(component =>
                component.Role == TargetRigBodyComponentRole.SupplementalBody &&
                !automaticRigidIndices.Contains(component.ComponentIndex)) ??
            throw new InvalidDataException(
                "Daphne fixture has no smooth supplemental component for the " +
                "fitting-selection isolation regression.");
        TargetRigSelectedBodyComponent[] reducedFittingComponents = fit.BodySelection
            .Components
            .Where(component =>
                component.ComponentIndex != fittingOnlyProbe.ComponentIndex)
            .ToArray();
        TargetRigBodySelection reducedFittingSelection = fit.BodySelection with
        {
            Components = Array.AsReadOnly(reducedFittingComponents),
            ExcludedComponentCount = fit.BodySelection.TotalComponentCount -
                                     reducedFittingComponents.Length
        };
        GeneratedSkinningPreparationResult reducedFittingPreparation =
            GeneratedSkinningPreparer.Prepare(
                target,
                donor,
                fit.Pose,
                alignment,
                reducedFittingSelection);
        if (reducedFittingPreparation.Analysis.Attachments.Any(attachment =>
                attachment.ComponentIndex == fittingOnlyProbe.ComponentIndex))
        {
            throw new InvalidDataException(
                $"Fitting-only component #{fittingOnlyProbe.ComponentIndex} became " +
                "rigid merely because it was removed from TargetRigBodySelection.");
        }
        Matrix4x4 lieDown = Matrix4x4.CreateRotationZ(MathF.PI / 2);
        ImportedScene lyingDonor = donor with
        {
            Meshes = donor.Meshes
                .Select(mesh => mesh with
                {
                    Positions = mesh.Positions
                        .Select(position => Vector3.Transform(position, lieDown))
                        .ToArray(),
                    Normals = mesh.Normals
                        .Select(normal => Vector3.TransformNormal(normal, lieDown))
                        .ToArray()
                })
                .ToArray()
        };
        var rotatedAlignment = alignment with
        {
            RotationDegrees = new Vector3(0, 0, -90)
        };
        TargetRigBodySelection rotatedBody = TargetRigAutomaticPoseFitter.SelectBody(
            rig,
            lyingDonor,
            rotatedAlignment);
        GeneratedSkinningPreparationResult rotatedPreparation =
            GeneratedSkinningPreparer.Prepare(
                target,
                lyingDonor,
                fit.Pose,
                rotatedAlignment,
                rotatedBody);
        VerifyRotatedAlignment(lyingDonor, rotatedPreparation, rotatedAlignment);
        if (preparation.Analysis.PreparedVertexCount != ExpectedVertexCount ||
            preparation.Analysis.DonorComponentCount != 26 ||
            preparation.Analysis.Attachments.Count >=
                preparation.Analysis.DonorComponentCount ||
            preparation.PreparedScene.Meshes.Count != ExpectedMeshCount)
        {
            throw new InvalidDataException(
                "Daphne generated-skinning fixture changed: expected " +
                $"{ExpectedVertexCount} vertices, 26 components, fewer than 26 attachments and " +
                $"{ExpectedMeshCount} meshes; got " +
                $"{preparation.Analysis.PreparedVertexCount}, " +
                $"{preparation.Analysis.DonorComponentCount}, " +
                $"{preparation.Analysis.Attachments.Count}, " +
                $"{preparation.PreparedScene.Meshes.Count}.");
        }
        if (preparation.Analysis.Attachments.Any(attachment =>
                attachment.ManualAssignment is not null ||
                attachment.PlaneAssignment !=
                    GeneratedSkinningComponentAttachmentTarget.UpperBack))
        {
            throw new InvalidDataException(
                "Daphne automatic preparation produced a rigid detail which was " +
                "not extracted by the strict-majority Back rule.");
        }
        string preparedFingerprintBefore = FingerprintImportedScene(
            preparation.PreparedScene);

        GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(
            target,
            preparation.PreparedScene);
        if (!plan.CanReplace || plan.MaterialGroupCount != 2 ||
            plan.MeshCount != ExpectedMeshCount)
        {
            throw new InvalidOperationException(
                "Daphne ImportDonor analysis did not retain both texture groups: " +
                string.Join(" | ", plan.Messages));
        }
        if (!plan.Messages.Any(message =>
                message.Contains("independent native TextureData", StringComparison.Ordinal)))
        {
            throw new InvalidOperationException(
                "Daphne analysis succeeded without selecting independent native textures: " +
                string.Join(" | ", plan.Messages));
        }

        ImportedScene preview = SmoSkinnedGlbReplacer.PrepareGeometryPreview(
            target,
            preparation.PreparedScene,
            ReplacementTransform.Identity,
            SkinnedGeometryTransferMode.PreservePreparedGeometry);
        VerifyPreview(preparation.PreparedScene, preview);
        (int OpaqueBody, int OpaqueOverlay, int Alpha) opacity = ClassifyPreviewOpacity(
            preparation.PreparedScene, preview);
        VerifyOpacityCounts(opacity, "Daphne final preview");

        bool outputVerified = false;
        try
        {
            GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
                target,
                preparation.PreparedScene,
                ReplacementTransform.Identity,
                outputPath,
                SkinnedGeometryTransferMode.PreservePreparedGeometry);
            SmoDocument output = SmoDocument.Load(outputPath);
            if (output.HasErrors)
                throw new InvalidDataException("Daphne output failed strict parsing.");
            VerifyWriterResult(target, output, result, preview, opacity);
            VerifyOldVisualsRemoved(output, oldVisualsBefore);

            if (!target.Data.Span.SequenceEqual(targetBytesBefore) ||
                FingerprintImportedScene(donor) != donorFingerprintBefore ||
                FingerprintImportedScene(preparation.PreparedScene) !=
                    preparedFingerprintBefore)
            {
                throw new InvalidOperationException(
                    "Daphne Analyze/preview/write mutated an in-memory source input.");
            }
            VerifyDonorFilesUnchanged(donorFilesBefore);
            if (!File.ReadAllBytes(targetPath).SequenceEqual(targetBytesBefore))
                throw new InvalidOperationException("Daphne regression modified the target file.");

            outputVerified = true;
            HashSet<int> selectedForFit = fit.BodySelection.Components
                .Select(component => component.ComponentIndex)
                .ToHashSet();
            HashSet<int> rigidBack = preparation.Analysis.Attachments
                .Select(attachment => attachment.ComponentIndex)
                .ToHashSet();
            int fittingExcludedNowDeform = Enumerable
                .Range(0, fit.BodySelection.TotalComponentCount)
                .Count(component =>
                    !selectedForFit.Contains(component) &&
                    !rigidBack.Contains(component));
            Console.WriteLine(
                "DAPHNE MODE 3 REGRESSION PASS: " +
                $"alignment={FixtureScale:G9}/(0,12,-3); " +
                $"body={fit.BodySelection.Components.Count}/" +
                $"{fit.BodySelection.TotalComponentCount}; " +
                $"rigidBack={rigidBack.Count}; " +
                $"logicalDeform={fit.BodySelection.TotalComponentCount - rigidBack.Count}; " +
                $"fitExcludedNowDeform={fittingExcludedNowDeform}; " +
                $"fittingIsolationProbe=#{fittingOnlyProbe.ComponentIndex}; " +
                $"meshes={ExpectedMeshCount}; vertices={ExpectedVertexCount}; " +
                $"triangles={result.TriangleCount}; groups={plan.MaterialGroupCount}; " +
                $"opaque/overlay/alpha={opacity.OpaqueBody}/" +
                $"{opacity.OpaqueOverlay}/{opacity.Alpha}; " +
                "textures=2 native branches; source UV exact; old meshes/materials/textures absent; " +
                $"strict reload; SHA-256={result.Sha256}; output={result.OutputPath}");
        }
        finally
        {
            if (!outputVerified && File.Exists(outputPath))
                File.Delete(outputPath);
        }
    }

    private static void VerifyRotatedAlignment(
        ImportedScene donor,
        GeneratedSkinningPreparationResult preparation,
        ReplacementTransform alignment)
    {
        if (preparation.Analysis.Alignment.RotationDegrees !=
            alignment.RotationDegrees)
        {
            throw new InvalidDataException(
                "Daphne rotated-alignment analysis lost the explicit rotation.");
        }
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            ImportedMesh source = donor.Meshes[meshIndex];
            ImportedMesh prepared = preparation.FittingPreviewScene.Meshes[meshIndex];
            for (int vertex = 0; vertex < source.Positions.Length; vertex++)
            {
                Vector3 expected = Vector3.Transform(
                    source.Positions[vertex], alignment.Matrix);
                Vector3 actual = prepared.Positions[vertex];
                float tolerance = 0.00001f * MathF.Max(1, expected.Length());
                if (Vector3.DistanceSquared(expected, actual) >
                    tolerance * tolerance)
                {
                    throw new InvalidDataException(
                        "Daphne mode-3 rotation was not baked into every position.");
                }
            }
            for (int normal = 0; normal < source.Normals.Length; normal++)
            {
                Vector3 transformed = Vector3.TransformNormal(
                    source.Normals[normal], alignment.Matrix);
                Vector3 expected = transformed.LengthSquared() <= 0.000000000001f
                    ? Vector3.Zero
                    : Vector3.Normalize(transformed);
                if (Vector3.DistanceSquared(expected, prepared.Normals[normal]) >
                    0.00000001f)
                {
                    throw new InvalidDataException(
                        "Daphne mode-3 rotation was not baked into every normal.");
                }
            }
        }
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

    private static void EnsureSeparateOutput(
        string targetPath,
        string donorPath,
        string outputPath)
    {
        if (string.Equals(outputPath, targetPath, StringComparison.OrdinalIgnoreCase) ||
            string.Equals(outputPath, donorPath, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                "Daphne regression output must be separate from both source files.");
        }
    }

    private static IReadOnlyDictionary<string, string> SnapshotDonorDirectory(
        string donorPath,
        string outputPath)
    {
        string directory = Path.GetDirectoryName(donorPath) ??
            throw new InvalidOperationException("Daphne donor directory is unavailable.");
        return Directory.EnumerateFiles(directory, "*", SearchOption.TopDirectoryOnly)
            .Select(Path.GetFullPath)
            .Where(path => !string.Equals(path, outputPath, StringComparison.OrdinalIgnoreCase))
            .ToDictionary(
                path => path,
                path => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))),
                StringComparer.OrdinalIgnoreCase);
    }

    private static void VerifyDonorFilesUnchanged(
        IReadOnlyDictionary<string, string> before)
    {
        foreach ((string path, string hash) in before)
        {
            if (!File.Exists(path) ||
                Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))) != hash)
            {
                throw new InvalidOperationException(
                    $"Daphne regression modified donor-side file {path}.");
            }
        }
    }

    private static ImportedTexture[] ReadDonorDirectoryTextures(
        string donorPath,
        ImportedScene donor)
    {
        string? directory = Path.GetDirectoryName(donorPath);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
            return [];
        string[] files = Directory.EnumerateFiles(
                directory, "*", SearchOption.TopDirectoryOnly)
            .Where(path => Path.GetExtension(path).ToLowerInvariant() is
                ".png" or ".jpg" or ".jpeg" or ".bmp" or ".tga")
            .OrderBy(path => path, StringComparer.OrdinalIgnoreCase)
            .ToArray();
        var exactFileNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var bareStems = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        void AddReference(string? reference)
        {
            if (string.IsNullOrWhiteSpace(reference))
                return;
            string fileName = Path.GetFileName(reference.Trim());
            string fileExtension = Path.GetExtension(fileName).ToLowerInvariant();
            if (fileExtension is ".png" or ".jpg" or ".jpeg" or ".bmp" or ".tga")
                exactFileNames.Add(fileName);
            else
                bareStems.Add(fileName);
        }

        foreach (ImportedMaterial material in donor.Materials)
        {
            AddReference(material.BaseColorTextureName);
            AddReference(material.Name);
        }
        foreach (ImportedTexture texture in donor.Textures)
        {
            AddReference(texture.SourcePath);
            AddReference(texture.Name);
        }
        HashSet<string> exactStems = exactFileNames
            .Select(fileName => Path.GetFileNameWithoutExtension(fileName) ?? fileName)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        return files.Where(path =>
            {
                string fileName = Path.GetFileName(path);
                string stem = Path.GetFileNameWithoutExtension(fileName);
                return exactFileNames.Contains(fileName) ||
                       !exactStems.Contains(stem) && bareStems.Contains(stem);
            })
            .Select(ImportedTextureFileReader.Read)
            .ToArray();
    }

    private static void VerifyDaphneDonorFixture(
        ImportedScene donor,
        IReadOnlyList<ImportedTexture> externalTextures,
        ImportedTextureCatalogResult catalog)
    {
        int vertices = donor.Meshes.Sum(mesh => mesh.Positions.Length);
        int triangles = donor.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
        int[] usedTextures = donor.Meshes
            .Select(mesh => donor.Materials[mesh.MaterialIndex].BaseColorTextureIndex)
            .Distinct()
            .Order()
            .ToArray();
        if (donor.Meshes.Count != ExpectedMeshCount ||
            vertices != ExpectedVertexCount ||
            triangles != ExpectedTriangleCount ||
            donor.Materials.Count != 2 ||
            externalTextures.Count != 2 ||
            usedTextures.Length != 2 ||
            usedTextures.Any(index => index < 0 || index >= donor.Textures.Count) ||
            catalog.UnusedExternalTextures.Count != 0)
        {
            throw new InvalidDataException(
                "Daphne donor fixture changed: expected 4 meshes, 1701 vertices, " +
                "2158 triangles, two used materials/textures and no unused external " +
                $"images; got {donor.Meshes.Count}, {vertices}, {triangles}, " +
                $"{donor.Materials.Count}, {externalTextures.Count}, " +
                $"{usedTextures.Length}, {catalog.UnusedExternalTextures.Count}.");
        }
    }

    private static void VerifyPreview(ImportedScene prepared, ImportedScene preview)
    {
        int vertices = preview.Meshes.Sum(mesh => mesh.Positions.Length);
        int triangles = preview.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
        if (preview.Meshes.Count != ExpectedMeshCount ||
            vertices != ExpectedVertexCount ||
            triangles != ExpectedTriangleCount ||
            preview.Textures.Count != 2 ||
            preview.Meshes.Any(mesh => mesh.Skinning is not null) ||
            preview.Meshes.Any(mesh =>
                mesh.MaterialIndex < 0 || mesh.MaterialIndex >= preview.Materials.Count ||
                preview.Materials[mesh.MaterialIndex].BaseColorTextureIndex < 0 ||
                preview.Materials[mesh.MaterialIndex].BaseColorTextureIndex >=
                    preview.Textures.Count))
        {
            throw new InvalidDataException(
                "Daphne final preview is not the expected unskinned 4-mesh, " +
                "1701-vertex, 2158-triangle two-texture scene: " +
                $"meshes={preview.Meshes.Count}, vertices={vertices}, " +
                $"triangles={triangles}, textures={preview.Textures.Count}, " +
                $"textureSize={string.Join(",", preview.Textures.Select(texture => $"{texture.Width}x{texture.Height}"))}, " +
                $"skinned={preview.Meshes.Count(mesh => mesh.Skinning is not null)}, " +
                $"materialIndices={string.Join(",", preview.Meshes.Select(mesh => mesh.MaterialIndex).Distinct().Order())}.");
        }
        for (int index = 0; index < preview.Meshes.Count; index++)
        {
            if (preview.Meshes[index].Name != prepared.Meshes[index].Name ||
                preview.Meshes[index].TriangleIndices.Length !=
                    prepared.Meshes[index].TriangleIndices.Length ||
                !preview.Meshes[index].TextureCoordinates.SequenceEqual(
                    prepared.Meshes[index].TextureCoordinates) ||
                prepared.Meshes[index].Skinning is null)
            {
                throw new InvalidDataException(
                    $"Daphne preview mesh [{index}] no longer corresponds to prepared input.");
            }
        }
        if (preview.Textures.Where((texture, index) =>
                index >= prepared.Textures.Count ||
                texture.Width != prepared.Textures[index].Width ||
                texture.Height != prepared.Textures[index].Height ||
                !texture.Data.SequenceEqual(prepared.Textures[index].Data)).Any())
        {
            throw new InvalidDataException(
                "Daphne preview combined, resized or reordered the two source textures.");
        }
    }

    private static (int OpaqueBody, int OpaqueOverlay, int Alpha) ClassifyPreviewOpacity(
        ImportedScene prepared,
        ImportedScene preview)
    {
        int opaqueBody = 0;
        int opaqueOverlay = 0;
        int alpha = 0;
        foreach (IGrouping<int, (ImportedMesh Mesh, int Index)> group in preview.Meshes
                     .Select((mesh, index) => (Mesh: mesh, Index: index))
                     .GroupBy(item => preview.Materials[item.Mesh.MaterialIndex]
                         .BaseColorTextureIndex))
        {
            SmoSkinnedBranchSourceMesh[] meshes = group.Select(item =>
                new SmoSkinnedBranchSourceMesh(
                    item.Index,
                    item.Mesh.Name,
                    item.Mesh.Positions,
                    item.Mesh.Normals,
                    item.Mesh.TextureCoordinates,
                    item.Mesh.DiffuseColors,
                    item.Mesh.TriangleIndices,
                    prepared.Meshes[item.Index].Skinning ?? throw new InvalidDataException(
                        $"Daphne prepared mesh [{item.Index}] has no generated skinning.")))
                .ToArray();
            SmoSkinnedRenderableOpacityPlan result =
                SmoSkinnedBranchSplitBuilder.ClassifyRenderables(
                    meshes, preview.Textures[group.Key]);
            opaqueBody += result.OpaqueBodyTriangleCount;
            opaqueOverlay += result.OpaqueOverlayTriangleCount;
            alpha += result.AlphaTriangleCount;
        }
        return (opaqueBody, opaqueOverlay, alpha);
    }

    private static void VerifyOpacityCounts(
        (int OpaqueBody, int OpaqueOverlay, int Alpha) opacity,
        string context)
    {
        if (opacity.OpaqueBody <= 0 ||
            opacity.Alpha <= 0 ||
            opacity.OpaqueBody + opacity.OpaqueOverlay + opacity.Alpha != ExpectedTriangleCount)
        {
            throw new InvalidDataException(
                $"{context} opacity must retain both opaque and alpha geometry and " +
                $"cover {ExpectedTriangleCount} triangles; got {opacity.OpaqueBody}/" +
                $"{opacity.OpaqueOverlay}/{opacity.Alpha}.");
        }
    }

    private static void VerifyWriterResult(
        SmoDocument target,
        SmoDocument output,
        GlbSkinTransferResult result,
        ImportedScene preview,
        (int OpaqueBody, int OpaqueOverlay, int Alpha) expectedOpacity)
    {
        HashSet<uint> targetMeshIds = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .Select(entry => entry.Id)
            .ToHashSet();
        SmoObjectEntry[] outputMeshes = output.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .ToArray();
        if (outputMeshes.Any(entry => targetMeshIds.Contains(entry.Id)))
        {
            throw new InvalidDataException(
                "Daphne clean rebuild retained one or more old MeshData object IDs.");
        }
        int retained = outputMeshes
            .Where(entry => targetMeshIds.Contains(entry.Id))
            .Sum(entry => CountNonDegenerateTriangles(SmoMeshDecoder.Decode(output, entry)));
        int opaqueBody = outputMeshes
            .Where(entry => entry.Name.StartsWith(
                OpaqueBodyMeshPrefix, StringComparison.Ordinal))
            .Sum(entry => CountNonDegenerateTriangles(SmoMeshDecoder.Decode(output, entry)));
        int alpha = outputMeshes
            .Where(entry => entry.Name.StartsWith(
                AlphaMeshPrefix, StringComparison.Ordinal))
            .Sum(entry => CountNonDegenerateTriangles(SmoMeshDecoder.Decode(output, entry)));
        int overlay = outputMeshes
            .Where(entry => entry.Name.StartsWith(
                OpaqueOverlayMeshPrefix, StringComparison.Ordinal))
            .Sum(entry => CountNonDegenerateTriangles(SmoMeshDecoder.Decode(output, entry)));
        int unexpectedAdded = outputMeshes
            .Where(entry => !targetMeshIds.Contains(entry.Id) &&
                            !entry.Name.StartsWith(AlphaMeshPrefix, StringComparison.Ordinal) &&
                            !entry.Name.StartsWith(OpaqueBodyMeshPrefix, StringComparison.Ordinal) &&
                            !entry.Name.StartsWith(
                                OpaqueOverlayMeshPrefix, StringComparison.Ordinal))
            .Sum(entry => CountNonDegenerateTriangles(SmoMeshDecoder.Decode(output, entry)));
        if (retained != 0 ||
            opaqueBody != expectedOpacity.OpaqueBody ||
            overlay != expectedOpacity.OpaqueOverlay ||
            alpha != expectedOpacity.Alpha ||
            unexpectedAdded != 0 ||
            result.TriangleCount != ExpectedTriangleCount ||
            result.MeshSlotCount != outputMeshes.Length ||
            result.FileSize != output.Data.Length ||
            result.Sha256 != Convert.ToHexString(SHA256.HashData(output.Data.Span)))
        {
            throw new InvalidDataException(
                "Daphne writer result changed: expected retained/body/overlay/alpha/total " +
                $"0/{expectedOpacity.OpaqueBody}/{expectedOpacity.OpaqueOverlay}/" +
                $"{expectedOpacity.Alpha}/{ExpectedTriangleCount}, got " +
                $"{retained}/{opaqueBody}/{overlay}/{alpha}/{result.TriangleCount}; " +
                $"unexpected added triangles={unexpectedAdded}.");
        }

        HashSet<uint> targetTextureIds = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
            .Select(entry => entry.Id)
            .ToHashSet();
        if (output.Objects.Any(entry =>
                entry.TypeHash == SmoClassIds.TextureData &&
                targetTextureIds.Contains(entry.Id)))
        {
            throw new InvalidDataException(
                "Daphne clean rebuild retained one or more old TextureData object IDs.");
        }
        HashSet<uint> targetMaterialIds = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MaterialData)
            .Select(entry => entry.Id)
            .ToHashSet();
        if (output.Objects.Any(entry =>
                entry.TypeHash == SmoClassIds.MaterialData &&
                targetMaterialIds.Contains(entry.Id)))
        {
            throw new InvalidDataException(
                "Daphne clean rebuild retained one or more old spMaterialData object IDs.");
        }
        SmoTexture[] addedTextures = output.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData &&
                            !targetTextureIds.Contains(entry.Id))
            .Select(entry =>
            {
                if (!SmoTextureDecoder.TryDecode(
                        output, entry, out SmoTexture? texture, out string error) ||
                    texture is null)
                {
                    throw new InvalidDataException(
                        $"Daphne generated texture [{entry.Index}] could not be decoded: {error}");
                }
                return texture;
            })
            .ToArray();
        if (addedTextures.Length != preview.Textures.Count)
        {
            throw new InvalidDataException(
                $"Daphne writer added {addedTextures.Length} textures, expected " +
                $"{preview.Textures.Count} independent source textures.");
        }
        var unmatched = addedTextures.ToList();
        foreach (ImportedTexture expected in preview.Textures)
        {
            int match = unmatched.FindIndex(actual =>
                actual.Width == expected.Width &&
                actual.Height == expected.Height &&
                ImportedTextureImageTools.SerializedBgraMatches(
                    expected.Data,
                    actual.Width,
                    actual.Height,
                    actual.Bgra32Pixels.Span,
                    out _));
            if (match < 0)
            {
                throw new InvalidDataException(
                    $"Daphne source texture {expected.Name} has no exact independent " +
                    "TextureData match in the written SMO.");
            }
            unmatched.RemoveAt(match);
        }
    }

    private static OldVisualSnapshot CaptureOldVisuals(SmoDocument document)
    {
        return new OldVisualSnapshot(
            document.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
                .Select(entry => entry.Id)
                .ToHashSet(),
            document.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.MaterialData)
                .Select(entry => entry.Id)
                .ToHashSet(),
            document.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
                .Select(entry => entry.Id)
                .ToHashSet());
    }

    private static void VerifyOldVisualsRemoved(
        SmoDocument output,
        OldVisualSnapshot expected)
    {
        uint[] leaked = output.Objects
            .Where(entry =>
                (entry.TypeHash == SmoClassIds.MeshData &&
                 expected.MeshObjectIds.Contains(entry.Id)) ||
                (entry.TypeHash == SmoClassIds.MaterialData &&
                 expected.MaterialObjectIds.Contains(entry.Id)) ||
                (entry.TypeHash == SmoClassIds.TextureData &&
                 expected.TextureObjectIds.Contains(entry.Id)))
            .Select(entry => entry.Id)
            .ToArray();
        if (leaked.Length != 0)
        {
            throw new InvalidDataException(
                "Daphne clean rebuild leaked old visual object IDs: " +
                string.Join(", ", leaked) + ".");
        }
    }

    private static SmoObjectEntry FindAncestorSkin(
        SmoDocument document,
        SmoObjectEntry mesh)
    {
        int? parent = mesh.ParentIndex;
        while (parent.HasValue)
        {
            SmoObjectEntry entry = document.Objects[parent.Value];
            if (entry.TypeHash == SmoClassIds.Skin)
                return entry;
            parent = entry.ParentIndex;
        }
        throw new InvalidDataException(
            $"Daphne eye mesh [{mesh.Index}] has no ancestor spSkin.");
    }

    private static uint? GetParentObjectId(
        SmoDocument document,
        SmoObjectEntry entry) =>
        entry.ParentIndex is int parentIndex
            ? document.Objects[parentIndex].Id
            : null;

    private static byte[] ObjectBytes(SmoDocument document, SmoObjectEntry entry) =>
        document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset),
            checked((int)entry.SerializedSize)).ToArray();

    private static int CountNonDegenerateTriangles(SmoMesh mesh) =>
        Enumerable.Range(0, mesh.TriangleIndices.Length / 3).Count(triangle =>
        {
            uint first = mesh.TriangleIndices[triangle * 3];
            uint second = mesh.TriangleIndices[triangle * 3 + 1];
            uint third = mesh.TriangleIndices[triangle * 3 + 2];
            return first != second && second != third && first != third;
        });

    private static string FingerprintImportedScene(ImportedScene scene)
    {
        using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        AppendInt(hash, scene.Meshes.Count);
        foreach (ImportedMesh mesh in scene.Meshes)
        {
            AppendString(hash, mesh.Name);
            AppendInt(hash, mesh.MaterialIndex);
            AppendInt(hash, mesh.Positions.Length);
            foreach (Vector3 value in mesh.Positions)
                AppendVector3(hash, value);
            AppendInt(hash, mesh.Normals.Length);
            foreach (Vector3 value in mesh.Normals)
                AppendVector3(hash, value);
            AppendInt(hash, mesh.TextureCoordinates.Length);
            foreach (Vector2 value in mesh.TextureCoordinates)
            {
                AppendFloat(hash, value.X);
                AppendFloat(hash, value.Y);
            }
            AppendInt(hash, mesh.TriangleIndices.Length);
            foreach (uint value in mesh.TriangleIndices)
                AppendUInt(hash, value);
            AppendInt(hash, mesh.DiffuseColors.Length);
            foreach (uint value in mesh.DiffuseColors)
                AppendUInt(hash, value);
            AppendInt(hash, mesh.Skinning is null ? 0 : 1);
            if (mesh.Skinning is null)
                continue;
            ImportedSkinning skinning = mesh.Skinning;
            AppendString(hash, skinning.Skeleton.Name);
            AppendInt(hash, skinning.Skeleton.JointNames.Count);
            foreach (string name in skinning.Skeleton.JointNames)
                AppendString(hash, name);
            foreach (Matrix4x4 matrix in skinning.Skeleton.InverseBindMatrices)
                AppendMatrix(hash, matrix);
            AppendInt(hash, skinning.JointIndices.Length);
            foreach (ImportedJointIndices joints in skinning.JointIndices)
            {
                AppendInt(hash, joints.X);
                AppendInt(hash, joints.Y);
                AppendInt(hash, joints.Z);
                AppendInt(hash, joints.W);
            }
            AppendInt(hash, skinning.Weights.Length);
            foreach (Vector4 weights in skinning.Weights)
            {
                AppendFloat(hash, weights.X);
                AppendFloat(hash, weights.Y);
                AppendFloat(hash, weights.Z);
                AppendFloat(hash, weights.W);
            }
        }
        AppendInt(hash, scene.Materials.Count);
        foreach (ImportedMaterial material in scene.Materials)
        {
            AppendString(hash, material.Name);
            AppendString(hash, material.BaseColorTextureName ?? string.Empty);
            AppendInt(hash, material.BaseColorTextureIndex);
        }
        AppendInt(hash, scene.Textures.Count);
        foreach (ImportedTexture texture in scene.Textures)
        {
            AppendString(hash, texture.Name);
            AppendString(hash, texture.MimeType);
            AppendString(hash, texture.SourcePath ?? string.Empty);
            AppendInt(hash, texture.Width);
            AppendInt(hash, texture.Height);
            AppendInt(hash, texture.Data.Length);
            hash.AppendData(texture.Data);
        }
        return Convert.ToHexString(hash.GetHashAndReset());
    }

    private static void AppendMatrix(IncrementalHash hash, Matrix4x4 value)
    {
        AppendFloat(hash, value.M11); AppendFloat(hash, value.M12);
        AppendFloat(hash, value.M13); AppendFloat(hash, value.M14);
        AppendFloat(hash, value.M21); AppendFloat(hash, value.M22);
        AppendFloat(hash, value.M23); AppendFloat(hash, value.M24);
        AppendFloat(hash, value.M31); AppendFloat(hash, value.M32);
        AppendFloat(hash, value.M33); AppendFloat(hash, value.M34);
        AppendFloat(hash, value.M41); AppendFloat(hash, value.M42);
        AppendFloat(hash, value.M43); AppendFloat(hash, value.M44);
    }

    private static void AppendVector3(IncrementalHash hash, Vector3 value)
    {
        AppendFloat(hash, value.X);
        AppendFloat(hash, value.Y);
        AppendFloat(hash, value.Z);
    }

    private static void AppendFloat(IncrementalHash hash, float value) =>
        AppendInt(hash, BitConverter.SingleToInt32Bits(value));

    private static void AppendString(IncrementalHash hash, string value)
    {
        byte[] bytes = System.Text.Encoding.UTF8.GetBytes(value);
        AppendInt(hash, bytes.Length);
        hash.AppendData(bytes);
    }

    private static void AppendInt(IncrementalHash hash, int value)
    {
        Span<byte> bytes = stackalloc byte[sizeof(int)];
        BinaryPrimitives.WriteInt32LittleEndian(bytes, value);
        hash.AppendData(bytes);
    }

    private static void AppendUInt(IncrementalHash hash, uint value)
    {
        Span<byte> bytes = stackalloc byte[sizeof(uint)];
        BinaryPrimitives.WriteUInt32LittleEndian(bytes, value);
        hash.AppendData(bytes);
    }

    private sealed record OldVisualSnapshot(
        IReadOnlySet<uint> MeshObjectIds,
        IReadOnlySet<uint> MaterialObjectIds,
        IReadOnlySet<uint> TextureObjectIds);
}
