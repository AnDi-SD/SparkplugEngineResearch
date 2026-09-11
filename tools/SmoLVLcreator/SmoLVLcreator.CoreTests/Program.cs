using SmoLVLcreator.Core;
using SmoImporter.Core;
using SmoExporter.Core;
using SmoViewer.Core;
using SmoViewer.Scene;
using System.Buffers.Binary;
using System.Diagnostics;
using System.IO.Compression;
using System.Numerics;
using System.Reflection;
using System.Runtime;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;

namespace SmoLVLcreator.CoreTests;

internal static class Program
{
    private static int _assertions;

    public static int Main(string[] args)
    {
        try
        {
            if (args.Length == 3 && args[0] == "--container-envelope")
                return SmoContainerEnvelopeRegression.Run(args[1], args[2]);
            if (args.Length == 3 && args[0] == "--leaf-envelope")
                return SmoContainerEnvelopeRegression.Run(args[1], args[2], leafOnly: true);
            if (args.Length == 1 && args[0] == "--container-envelope-boundaries")
                return SmoContainerEnvelopeRegression.RunBoundaries();
            if (args.Length == 3 && args[0] == "--render-occurrence-workspace")
                return SmoOccurrenceWorkspaceRegression.Run(args[1], args[2]);
            if (args.Length == 3 && args[0] == "--render-occurrence-edit")
                return SmoOccurrenceWorkspaceRegression.RunEdit(args[1], args[2]);
            if (args.Length >= 2 && args[0] == "--editable-workspaces")
            {
                foreach(string path in args.Skip(1))ValidateWorkspace(path);
                Console.WriteLine($"PASS: {_assertions} existing workspace/editor assertions");return 0;
            }
            if (args.Length == 2 && args[0].Equals(
                    "--isolated-external-model-batch",
                    StringComparison.OrdinalIgnoreCase))
            {
                return SmoExternalModelBatchJob.Run(args[1]);
            }
            if (args.Length is 2 or 3 && args[0].Equals(
                    "--validate-project-collision",
                    StringComparison.OrdinalIgnoreCase))
            {
                ValidateProjectCollisionProject(
                    Path.GetFullPath(args[1]),
                    args.Length == 3 ? Path.GetFullPath(args[2]) : null);
                Console.WriteLine($"PASS: {_assertions} assertions");
                return 0;
            }
            if (args.Length is 3 or 4 && args[0].Equals(
                    "--validate-project-external-model",
                    StringComparison.OrdinalIgnoreCase))
            {
                ValidateProjectExternalProject(
                    Path.GetFullPath(args[1]),
                    Path.GetFullPath(args[2]),
                    args.Length == 4 ? Path.GetFullPath(args[3]) : null);
                Console.WriteLine($"PASS: {_assertions} assertions");
                return 0;
            }
            if (args.Length == 5 && args[0].Equals(
                    "--prepare-isolated-save-job",
                    StringComparison.OrdinalIgnoreCase))
            {
                string sourcePath = Path.GetFullPath(args[1]);
                string modelPath = Path.GetFullPath(args[2]);
                string outputPath = Path.GetFullPath(args[3]);
                string jobDirectory = Path.GetFullPath(args[4]);
                Directory.CreateDirectory(jobDirectory);
                SmoLevelWorkspace workspace = SmoLevelWorkspace.Load(sourcePath);
                var level = new SmoLevelDocument(workspace);
                ImportedScene imported = ImportedModelReader.Read(modelPath);
                Guid modelId = level.AddExternalModel(
                    imported,
                    modelPath,
                    "IsolatedSaveRegression");
                level.AddExternalPlacement(
                    modelId,
                    Matrix4x4.CreateTranslation(-4300, 0, -600));
                string jobPath = SmoLevelSaveJob.Write(
                    level,
                    outputPath,
                    jobDirectory);
                Console.WriteLine(
                    $"JOB={jobPath}; meshes={imported.Meshes.Count}; " +
                    $"vertices={imported.Meshes.Sum(mesh => mesh.Positions.Length)}; " +
                    $"textures={imported.Textures.Count}; " +
                    $"textureBytes={imported.Textures.Sum(texture => (long)texture.Data.Length)}; " +
                    $"textureSizes={string.Join(',', imported.Textures.Select(texture => $"{texture.Width}x{texture.Height}"))}");
                return 0;
            }
            if (args.Length >= 4 && args[0].Equals(
                    "--stress-random-save",
                    StringComparison.OrdinalIgnoreCase))
            {
                return SmoLevelRandomStressRunner.RunSupervised(args);
            }
            if (args.Length >= 4 && args[0].Equals(
                    "--stress-project-save",
                    StringComparison.OrdinalIgnoreCase))
            {
                return SmoProjectRandomStressRunner.RunSupervised(args);
            }
            if (args.Length == 6 && args[0].Equals(
                    "--stress-random-save-child",
                    StringComparison.OrdinalIgnoreCase))
            {
                return SmoLevelRandomStressRunner.RunChild(args);
            }
            if (args.Length == 6 && args[0].Equals(
                    "--stress-project-save-child",
                    StringComparison.OrdinalIgnoreCase))
            {
                return SmoProjectRandomStressRunner.RunChild(args);
            }
            if (args.Length == 3 && args[0].Equals(
                    "--stress-external-append",
                    StringComparison.OrdinalIgnoreCase))
            {
                SmoLevelWorkspace workspace = SmoLevelWorkspace.Load(args[1]);
                ImportedScene imported = ObjModelReader.Read(args[2]);
                int templateIndex = SmoExternalLevelModelAppender
                    .FindTemplateMeshObjectIndex(workspace.Document, requireMaterial: true);
                GC.Collect(
                    GC.MaxGeneration,
                    GCCollectionMode.Forced,
                    blocking: true,
                    compacting: true);
                using Process process = Process.GetCurrentProcess();
                process.Refresh();
                long baselineWorkingSet = process.WorkingSet64;
                long baselinePrivate = process.PrivateMemorySize64;
                var timer = Stopwatch.StartNew();
                SmoExternalLevelModelAppendResult appended =
                    SmoExternalLevelModelAppender.AppendPartRange(
                        workspace.Document,
                        templateIndex,
                        imported,
                        [Matrix4x4.Identity],
                        "MemoryRegression",
                        firstPartIndex: 0,
                        partCount: imported.Meshes.Count);
                SmoDocument verified = SmoDocument.Parse(
                    appended.Data,
                    workspace.SourcePath);
                timer.Stop();
                long managedBeforeFinalCollection = GC.GetTotalMemory(
                    forceFullCollection: false);
                GCSettings.LargeObjectHeapCompactionMode =
                    GCLargeObjectHeapCompactionMode.CompactOnce;
                GC.Collect(
                    GC.MaxGeneration,
                    GCCollectionMode.Forced,
                    blocking: true,
                    compacting: true);
                process.Refresh();
                Console.WriteLine(
                    $"PASS: source={workspace.Document.Data.Length}; " +
                    $"output={appended.Data.Length}; parts={imported.Meshes.Count}; " +
                    $"objects={verified.Objects.Count}; elapsed={timer.Elapsed}; " +
                    $"baselineWorkingSet={baselineWorkingSet}; " +
                    $"baselinePrivate={baselinePrivate}; " +
                    $"managedBeforeFinalCollection={managedBeforeFinalCollection}; " +
                    $"managedAfterFinalCollection={GC.GetTotalMemory(false)}; " +
                    $"finalWorkingSet={process.WorkingSet64}; " +
                    $"finalPrivate={process.PrivateMemorySize64}; " +
                    $"peakWorkingSet={process.PeakWorkingSet64}");
                return 0;
            }
            if (args.Length == 3 && args[0].Equals(
                    "--stress-external-save",
                    StringComparison.OrdinalIgnoreCase))
            {
                string directory = Path.Combine(
                    Path.GetTempPath(),
                    $"SmoLVLcreator-save-memory-{Guid.NewGuid():N}");
                Directory.CreateDirectory(directory);
                try
                {
                    string output = Path.Combine(directory, "memory-regression.smo");
                    SmoLevelWorkspace workspace = SmoLevelWorkspace.Load(args[1]);
                    var level = new SmoLevelDocument(workspace);
                    ImportedScene imported = ObjModelReader.Read(args[2]);
                    Guid modelId = level.AddExternalModel(
                        imported,
                        args[2],
                        "MemoryRegression");
                    level.AddExternalPlacement(
                        modelId,
                        Matrix4x4.CreateTranslation(100, 200, 300));
                    using Process process = Process.GetCurrentProcess();
                    process.Refresh();
                    long baselineWorkingSet = process.WorkingSet64;
                    var timer = Stopwatch.StartNew();
                    SmoLevelSaveResult saved =
                        SmoLevelSaveService.Save(level, output);
                    timer.Stop();
                    SmoDocument verified = SmoDocument.Load(output);
                    process.Refresh();
                    Console.WriteLine(
                        $"PASS: bytes={verified.Data.Length}; " +
                        $"objects={verified.Objects.Count}; elapsed={timer.Elapsed}; " +
                        $"baselineWorkingSet={baselineWorkingSet}; " +
                        $"finalWorkingSet={process.WorkingSet64}; " +
                        $"peakWorkingSet={process.PeakWorkingSet64}; " +
                        $"log={saved.LogPath}");
                    return 0;
                }
                finally
                {
                    Directory.Delete(directory, recursive: true);
                }
            }
            if (args.Length == 3 && args[0].Equals(
                    "--repair-collision-registry",
                    StringComparison.OrdinalIgnoreCase))
            {
                string output = Path.GetFullPath(args[2]);
                Directory.CreateDirectory(Path.GetDirectoryName(output)!);
                SmoLevelWorkspace workspace = SmoLevelWorkspace.Load(args[1]);
                var level = new SmoLevelDocument(workspace);
                int registrationRepairCount = SmoCollisionBranchAppender
                    .FindUnregisteredCollisionInfoObjectIndices(workspace.Document)
                    .Count;
                int meshRepairCount = SmoCollisionBranchAppender
                    .FindUnterminatedCollisionMeshObjectIndices(workspace.Document)
                    .Count;
                SmoLevelSaveResult saved = SmoLevelSaveService.Save(level, output);
                SmoDocument verified = SmoDocument.Load(output);
                Console.WriteLine(
                    $"PASS: registry-repaired={registrationRepairCount}; " +
                    $"mesh-rebuilt={meshRepairCount}; " +
                    $"objects={verified.Objects.Count}; bytes={verified.Data.Length}; " +
                    $"log={saved.LogPath}; output={output}");
                return 0;
            }
            ValidatePickingMath();
            ValidateEulerAngles();
            ValidateImportContract();
            ValidateObjAdjacentTextureImport();
            ValidateCollisionHullGenerator();
            ValidateSaveDiagnostics();
            string repository = Path.GetFullPath(Path.Combine(
                AppContext.BaseDirectory, "..", "..", "..", "..", "..", ".."));
            string assets = Path.Combine(
                repository, "tools", "SmoViewer", "SmoViewer", "Assets");

            ValidateWorkspace(Path.Combine(assets, "fish.smo"));
            ValidateWorkspace(Path.Combine(assets, "bloom_ball.smo"));
            ValidateProjectRoundTrip(Path.Combine(assets, "fish.smo"));
            ValidateProjectRoundTrip(Path.Combine(assets, "bloom_ball.smo"));
            ValidateProjectCollisionLinkMetadata();
            ValidateProjectTextureReplacement(
                Path.Combine(assets, "fish.smo"));
            ValidateProjectJournalStress(Path.Combine(assets, "fish.smo"));
            ValidateProjectPropertyEdit();
            ValidateProjectSession();
            ValidateProjectFieldMaterialization(
                Path.Combine(assets, "fish.smo"));
            ValidateProjectResourceRelocation();
            ValidateProjectReferencePlacement();
            ValidateProjectAddedForest();
            ValidateProjectBranchRemoval(
                Path.Combine(assets, "fish.smo"));
            ValidateCancelledSavePreservesDestination(
                Path.Combine(assets, "fish.smo"));
            foreach (string path in args)
            {
                ValidateCancelledSavePreservesDestination(path);
                ValidateCollisionSave(path);
            }

            Console.WriteLine($"PASS: {_assertions} assertions");
            return 0;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine($"FAIL: {exception.Message}");
            Console.Error.WriteLine(exception);
            return 1;
        }
    }

    private static void ValidateEulerAngles()
    {
        Vector3[] samples =
        [
            Vector3.Zero,
            new(20, 35, -15),
            new(-45, 120, 70),
            new(89, -30, 5)
        ];
        foreach (Vector3 sample in samples)
        {
            Quaternion source = SmoEulerAngles.FromDegrees(sample);
            Vector3 decoded = SmoEulerAngles.ToDegrees(source);
            Quaternion roundTrip = SmoEulerAngles.FromDegrees(decoded);
            True(MathF.Abs(Quaternion.Dot(source, roundTrip)) > 0.99999f,
                $"Euler editor conversion round-trips {sample}");
        }

        Quaternion delta = SmoEulerAngles.FromDegrees(new Vector3(0, 30, 0));
        True(SmoEulerAngles.TryGetAxisAngle(delta, out Vector3 axis, out float angle),
            "rotation delta exposes an axis-angle");
        True(Vector3.Dot(axis, Vector3.UnitY) > 0.9999f &&
             MathF.Abs(angle - MathF.PI / 6) < 0.0001f,
            "axis-angle preserves a 30 degree Y rotation");
    }

    private static void ValidateProjectRoundTrip(string sourcePath)
    {
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-roundtrip-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string projectPath = Path.Combine(directory, "test.smolvlproj");
            string rebuiltPath = Path.Combine(directory, "rebuilt.smo");
            SmoDocument source = SmoDocument.Load(sourcePath);
            SmoProject imported =
                SmoProject.Import(source);
            SmoProjectArchive.Save(imported, projectPath);
            SmoProject reopened =
                SmoProjectArchive.Load(projectPath);
            SmoProjectBuildResult result =
                SmoProjectSerializer.Build(reopened, rebuiltPath);
            SmoDocument rebuilt = SmoDocument.Load(rebuiltPath);

            True(result.IsByteIdenticalToImportedSource,
                $"project rebuild is byte-identical for {Path.GetFileName(sourcePath)}");
            True(source.Data.Span.SequenceEqual(rebuilt.Data.Span),
                $"project preserves every SMO byte for {Path.GetFileName(sourcePath)}");
            True(source.Objects.Count == reopened.Objects.Count &&
                 rebuilt.Objects.Count == source.Objects.Count,
                $"project preserves the object catalog for {Path.GetFileName(sourcePath)}");
            True(reopened.Manifest.Header.SerializerVersion == source.Header.SerializerVersion &&
                 reopened.Manifest.Header.Unknown08 == source.Header.Unknown08 &&
                 reopened.Manifest.Header.PlatformMask == source.Header.PlatformMask,
                $"project preserves unknown FFPS header words for {Path.GetFileName(sourcePath)}");
            True(string.Equals(
                     reopened.Manifest.SourcePathHint,
                     Path.GetFullPath(sourcePath),
                     StringComparison.OrdinalIgnoreCase) &&
                 reopened.Manifest.SourceLogicalPath == Path.GetFileName(sourcePath),
                $"project preserves native-validation source hints for {Path.GetFileName(sourcePath)}");

            SmoProjectBuildResult replacement =
                SmoProjectSerializer.Build(reopened, rebuiltPath);
            True(replacement.BackupPath is not null &&
                 File.Exists(replacement.BackupPath),
                $"project rebuild creates a backup for {Path.GetFileName(sourcePath)}");
            True(File.ReadAllBytes(replacement.BackupPath!).AsSpan()
                    .SequenceEqual(source.Data.Span),
                $"project rebuild backup preserves the previous output for {Path.GetFileName(sourcePath)}");
            True(replacement.Sha256 == result.Sha256 &&
                 File.ReadAllBytes(rebuiltPath).AsSpan().SequenceEqual(source.Data.Span),
                $"project replacement remains deterministic for {Path.GetFileName(sourcePath)}");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateProjectCollisionLinkMetadata()
    {
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-links-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            SmoDocument source = SmoDocument.Parse(
                BuildReferencePlacementSample(Matrix4x4.Identity),
                "synthetic-project-links.smo");
            SmoProject project = SmoProject.Import(source);
            var session = new SmoProjectSession(project);
            uint visualId = source.Objects.Single(item =>
                item.TypeHash == SmoClassIds.StaticRenderObject).Id;
            uint collisionId = source.Objects.Single(item =>
                item.TypeHash != SmoClassIds.StaticRenderObject &&
                item.TypeHash != SmoClassIds.MeshData).Id;

            True(session.Execute(
                    "link visual and collision",
                    current => current.SetCollisionLinkOverride(
                        visualId,
                        collisionId,
                        present: true)),
                "project collision link creates one journal transaction");
            True(project.Manifest.CollisionLinkOverrides is
                    [{ Present: true }],
                "project collision link is represented as editor metadata");
            True(session.Undo() &&
                 project.Manifest.CollisionLinkOverrides.Count == 0,
                "project collision link supports undo");
            True(session.Redo() &&
                 project.Manifest.CollisionLinkOverrides.Count == 1,
                "project collision link supports redo");

            string projectPath = Path.Combine(directory, "links.smolvlproj");
            string outputPath = Path.Combine(directory, "links.smo");
            SmoProjectArchive.Save(project, projectPath);
            SmoProject reopened = SmoProjectArchive.Load(projectPath);
            SmoProjectBuildResult built = SmoProjectSerializer.Build(
                reopened,
                outputPath);
            True(reopened.Manifest.CollisionLinkOverrides.Single() is
                    { VisualObjectId: var reopenedVisual,
                      CollisionObjectId: var reopenedCollision,
                      Present: true } &&
                 reopenedVisual == visualId &&
                 reopenedCollision == collisionId,
                "manual collision link survives project archive reopen");
            True(built.IsByteIdenticalToImportedSource &&
                 File.ReadAllBytes(outputPath).AsSpan().SequenceEqual(source.Data.Span),
                "editor collision metadata does not invent fields in the SMO build");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateProjectTextureReplacement(string sourcePath)
    {
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-texture-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            SmoDocument source = SmoDocument.Load(sourcePath);
            SmoObjectEntry textureEntry = source.Objects.First(entry =>
                entry.TypeHash == SmoClassIds.TextureData);
            SmoProject project = SmoProject.Import(source);
            var session = new SmoProjectSession(project);
            byte[] encoded;
            using (var image = new Image<Rgba32>(
                       4,
                       4,
                       new Rgba32(23, 101, 207, 37)))
            using (var stream = new MemoryStream())
            {
                image.SaveAsPng(stream);
                encoded = stream.ToArray();
            }

            True(session.Execute(
                    "replace texture",
                    current => SmoProjectTextureReplacement.Replace(
                        current,
                        textureEntry.Id,
                        encoded,
                        replaceAlpha: true)),
                "project texture replacement creates one journal transaction");
            True(project.Manifest.ObjectDataReplacements.Count == 1,
                "project texture replacement stores one compact object operation");
            SmoDocument replaced = SmoProjectSerializer.CreateCurrentDocument(project);
            SmoObjectEntry replacedEntry = replaced.Objects.Single(entry =>
                entry.Id == textureEntry.Id);
            True(SmoTextureDecoder.TryDecode(
                    replaced,
                    replacedEntry,
                    out SmoTexture? replacedTexture,
                    out _) &&
                 replacedTexture is not null &&
                 replacedTexture.Bgra32Pixels.Span[0] == 207 &&
                 replacedTexture.Bgra32Pixels.Span[1] == 101 &&
                 replacedTexture.Bgra32Pixels.Span[2] == 23 &&
                 replacedTexture.Bgra32Pixels.Span[3] == 37,
                "project texture replacement writes RGB and alpha into TextureData");

            True(session.Undo(), "project texture replacement supports undo");
            True(SmoProjectSerializer.CreateCurrentDocument(project).Data.Span
                    .SequenceEqual(source.Data.Span),
                "texture undo restores the byte-identical source container");
            True(session.Redo(), "project texture replacement supports redo");

            string projectPath = Path.Combine(directory, "texture.smolvlproj");
            string outputPath = Path.Combine(directory, "texture.smo");
            SmoProjectArchive.Save(project, projectPath);
            SmoProject reopened =
                SmoProjectArchive.Load(projectPath);
            _ = SmoProjectSerializer.Build(reopened, outputPath);
            SmoDocument rebuilt = SmoDocument.Load(outputPath);
            True(reopened.Manifest.ObjectDataReplacements.Count == 1 &&
                 rebuilt.Objects.Any(entry => entry.Id == textureEntry.Id),
                "texture replacement asset survives project archive reopen and SMO build");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateProjectJournalStress(string sourcePath)
    {
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-stress-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            SmoDocument source = SmoDocument.Load(sourcePath);
            (uint ObjectId, Matrix4x4 World)[] placements = source.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.StaticRenderObject)
                .Select(entry =>
                {
                    bool decoded = SmoStaticRenderObjectTransformDecoder.TryDecode(
                        source,
                        entry,
                        out Matrix4x4 world);
                    return (entry.Id, World: world, Decoded: decoded);
                })
                .Where(item => item.Decoded)
                .Select(item => (item.Id, item.World))
                .Take(32)
                .ToArray();
            if (placements.Length == 0)
            {
                source = SmoDocument.Parse(
                    BuildStaticPlacementSample(Matrix4x4.Identity),
                    "synthetic-project-stress.smo");
                placements = [(source.Objects[0].Id, Matrix4x4.Identity)];
            }
            True(placements.Length > 0,
                "project stress fixture exposes editable static placements");
            SmoProject project = SmoProject.Import(source);
            byte[] immutable = project.DataSection.ToArray();
            var session = new SmoProjectSession(project);
            var random = new Random(0x534D4F);
            const int iterations = 1500;
            for (int iteration = 0; iteration < iterations; iteration++)
            {
                (uint ObjectId, Matrix4x4 World) placement =
                    placements[random.Next(placements.Length)];
                Matrix4x4 transform =
                    Matrix4x4.CreateScale(
                        0.75f + (float)random.NextDouble() * 0.75f) *
                    Matrix4x4.CreateRotationY(
                        -MathF.PI + (float)random.NextDouble() * MathF.Tau) *
                    Matrix4x4.CreateTranslation(
                        placement.World.Translation +
                        new Vector3(
                            random.Next(-250, 251),
                            random.Next(-100, 101),
                            random.Next(-250, 251)));
                True(session.Execute(
                        $"stress transform {iteration}",
                        current => current.SetPlacementTransform(
                            placement.ObjectId,
                            transform)),
                    $"project stress mutation {iteration} commits");
                if ((iteration + 1) % 250 == 0)
                    session.CompactHistory();
            }
            True(project.DataSection.Span.SequenceEqual(immutable),
                "1500 project mutations keep data.bin immutable");
            True(project.Manifest.PropertyEdits.Count <= placements.Length * 2,
                "repeated project mutations coalesce by stable object/property key");

            string projectPath = Path.Combine(directory, "stress.smolvlproj");
            string outputPath = Path.Combine(directory, "stress.smo");
            SmoProjectArchive.Save(project, projectPath);
            SmoProject reopened = SmoProjectArchive.Load(projectPath);
            SmoProjectBuildResult result = SmoProjectSerializer.Build(
                reopened,
                outputPath);
            SmoDocument rebuilt = SmoDocument.Load(outputPath);
            True(!rebuilt.HasErrors && rebuilt.Objects.Count == source.Objects.Count &&
                 result.ObjectCount == source.Objects.Count,
                "stressed project reopens and builds one structurally valid SMO");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateProjectPropertyEdit()
    {
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-edit-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string projectPath = Path.Combine(directory, "moved.smolvlproj");
            string outputPath = Path.Combine(directory, "moved.smo");
            Matrix4x4 originalWorld =
                Matrix4x4.CreateScale(1.25f, 0.75f, 1.5f) *
                Matrix4x4.CreateRotationY(0.4f) *
                Matrix4x4.CreateTranslation(10, 20, 30);
            SmoDocument source = SmoDocument.Parse(
                BuildStaticPlacementSample(originalWorld),
                "synthetic-static-placement.smo");
            True(!source.HasErrors && source.Objects.Count == 1,
                "synthetic project placement is structurally valid");

            SmoProject project = SmoProject.Import(source);
            byte[] immutableData = project.DataSection.ToArray();
            Vector3 delta = new(1.25f, -2.5f, 3.75f);
            Matrix4x4 desiredWorld = originalWorld;
            desiredWorld.M41 += delta.X;
            desiredWorld.M42 += delta.Y;
            desiredWorld.M43 += delta.Z;
            project.SetPlacementTransform(project.Objects[0].Id, desiredWorld);
            SmoProjectArchive.Save(project, projectPath);
            SmoProject reopened =
                SmoProjectArchive.Load(projectPath);
            SmoProjectBuildResult result =
                SmoProjectSerializer.Build(reopened, outputPath);
            SmoDocument moved = SmoDocument.Load(outputPath);

            True(reopened.Manifest.PropertyEdits.Count == 2 &&
                 reopened.DataSection.Span.SequenceEqual(immutableData),
                "project persists two matrix edits without changing immutable data.bin");
            True(!result.IsByteIdenticalToImportedSource &&
                 moved.Data.Length == source.Data.Length &&
                 moved.Objects.Count == source.Objects.Count,
                "fixed-size project edit preserves file and catalog layout");
            IReadOnlyList<SmoObjectField> movedFields =
                SmoObjectFieldReader.Read(moved, moved.Objects[0]);
            Matrix4x4 movedWorld = ReadMatrix(movedFields.Single(field =>
                field.FieldType == 1 && field.PayloadSize == 64).Payload.Span);
            True(Vector3.Distance(
                    Translation(movedWorld),
                    Translation(originalWorld) + delta) < 0.0001f,
                "project translation changes the authored world matrix");

            byte[] inverseBytes = reopened.GetPropertyBytes(
                0,
                SmoPropertyKeys.InverseWorldMatrix,
                SmoPropertyValueKind.Matrix4x4);
            Matrix4x4 inverse = ReadMatrix(inverseBytes);
            Matrix4x4 expectedEngineInverse =
                SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(movedWorld);
            True(MatrixDistance(inverse, expectedEngineInverse) < 0.0001f,
                "project transform regenerates Sparkplug's transpose-basis inverse");
            Matrix4x4 builtInverse = ReadMatrix(movedFields.Single(field =>
                field.FieldType == 2 && field.PayloadSize == 64).Payload.Span);
            True(MatrixDistance(builtInverse, expectedEngineInverse) < 0.0001f,
                "built SMO stores the project transpose-basis inverse");
            True(MatrixDistance(movedWorld * inverse, Matrix4x4.Identity) > 0.1f,
                "scaled project placement deliberately avoids a mathematical inverse");

            SmoProjectObject objectMetadata = reopened.Objects[0];
            (int Start, int End)[] writableRanges = objectMetadata.Fields
                .Where(field => field.FieldType is 1 or 2 && field.PayloadSize == 64)
                .Select(field =>
                {
                    int start = checked(
                        (int)source.Header.DataStart +
                        (int)objectMetadata.LogicalOffset +
                        field.RelativePayloadOffset);
                    return (start, start + 64);
                })
                .ToArray();
            int[] differences = Enumerable.Range(0, source.Data.Length)
                .Where(index => source.Data.Span[index] != moved.Data.Span[index])
                .ToArray();
            True(differences.Length > 0 && differences.All(index =>
                    writableRanges.Any(range =>
                        index >= range.Start && index < range.End)),
                "project serializer changes bytes only inside declared property slices");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateProjectSession()
    {
        Matrix4x4 originalWorld = Matrix4x4.CreateTranslation(3, 4, 5);
        SmoDocument source = SmoDocument.Parse(
            BuildStaticPlacementSample(originalWorld),
            "synthetic-transaction-placement.smo");
        SmoProject project = SmoProject.Import(source);
        byte[] immutableData = project.DataSection.ToArray();
        var session = new SmoProjectSession(project);
        Vector3 delta = new(7, 8, 9);

        True(session.Execute(
                "move placement",
                current => current.TranslateStaticPlacement(0, delta)) &&
             session.CanUndo && !session.CanRedo &&
             session.UndoLabel == "move placement" &&
             project.Manifest.PropertyEdits.Count == 2 && session.IsModified,
            "transaction records a compact placement edit");
        True(session.Undo() &&
             project.Manifest.PropertyEdits.Count == 0 &&
             session.CanRedo && session.RedoLabel == "move placement" &&
             !session.IsModified,
            "transaction undo restores the previous operation journal");
        True(session.Redo() &&
             project.Manifest.PropertyEdits.Count == 2 &&
             project.DataSection.Span.SequenceEqual(immutableData),
            "transaction redo restores edits without cloning immutable data.bin");

        int undoCount = session.UndoCount;
        try
        {
            session.Execute("failing transaction", current =>
            {
                current.Manifest.PropertyEdits.Clear();
                throw new InvalidOperationException("synthetic rollback");
            });
            throw new InvalidOperationException("Expected transaction failure.");
        }
        catch (InvalidOperationException exception) when (
            exception.Message == "synthetic rollback")
        {
            // Expected.
        }
        True(project.Manifest.PropertyEdits.Count == 2 &&
             session.UndoCount == undoCount && !session.CanRedo,
            "failed transaction rolls back atomically without touching history");

        True(!session.Execute(
                "no-op",
                current => current.TranslateStaticPlacement(0, Vector3.Zero)) &&
             session.UndoCount == undoCount,
            "equivalent operation state does not create an undo record");
        session.CompactHistory();
        True(!session.CanUndo && !session.CanRedo &&
             project.Manifest.PropertyEdits.Count == 2 && session.IsModified,
            "history compaction keeps the accepted project state");
        session.MarkSaved();
        True(!session.IsModified,
            "session saved baseline is independent from undo history");

        using var output = new MemoryStream();
        SmoProjectSerializer.Write(project, output);
        SmoDocument rebuilt = SmoDocument.Parse(
            output.ToArray(),
            "synthetic-transaction-result.smo");
        True(TryReadSyntheticStaticWorld(
                rebuilt,
                rebuilt.Objects[0],
                out Matrix4x4 moved) &&
             Vector3.Distance(
                 Translation(moved),
                 Translation(originalWorld) + delta) < 0.0001f,
            "compacted transaction state builds the requested transform");
    }

    private static void ValidateProjectFieldMaterialization(
        string sourcePath)
    {
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-materialize-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            SmoDocument source = SmoDocument.Load(sourcePath);
            SmoObjectEntry candidate = source.Objects.First(entry =>
            {
                if (entry.ParentIndex is null ||
                    entry.TypeHash is not (SmoClassIds.Node or
                        SmoClassIds.RenderNode or SmoClassIds.Model) ||
                    !SmoObjectFieldReader.TryRead(
                        source,
                        entry,
                        out IReadOnlyList<SmoObjectField> fields,
                        out _))
                {
                    return false;
                }
                return fields.Any(new SmoFieldSelector(0, 0, 12).Matches) &&
                       !fields.Any(new SmoFieldSelector(1, 0, 16).Matches) &&
                       !fields.Any(new SmoFieldSelector(2, 0, 12).Matches);
            });
            var desiredScale = new Vector3(1.25f, 0.75f, 1.5f);
            SmoProject project = SmoProject.Import(source);
            project.SetProperty(
                candidate.Index,
                SmoPropertyKeys.Scale,
                desiredScale);
            string projectPath = Path.Combine(directory, "scaled.smolvlproj");
            string outputPath = Path.Combine(directory, "scaled.smo");
            SmoProjectArchive.Save(project, projectPath);
            SmoProject reopened =
                SmoProjectArchive.Load(projectPath);
            SmoProjectBuildResult result =
                SmoProjectSerializer.Build(reopened, outputPath);
            SmoDocument scaled = SmoDocument.Load(outputPath);

            True(reopened.Manifest.PropertyEdits.Count == 2,
                "materializing scale also records its missing identity-rotation anchor");
            True(!scaled.HasErrors && scaled.Objects.Count == source.Objects.Count &&
                 result.FileSize > source.Data.Length,
                "materialized fields produce a larger structurally valid container");
            True(SmoNodeTransformDecoder.TryDecode(
                    scaled,
                    scaled.Objects[candidate.Index],
                    out SmoNodeTransform? transform) &&
                 transform is not null &&
                 Vector3.Distance(transform.Scale, desiredScale) < 0.0001f &&
                 MathF.Abs(Quaternion.Dot(
                    transform.Rotation,
                    Quaternion.Identity)) > 0.99999f,
                "materialized node fields decode as the requested scale and identity rotation");

            int? ancestorIndex = candidate.ParentIndex;
            bool allAncestorsGrew = true;
            while (ancestorIndex is int index)
            {
                allAncestorsGrew &= scaled.Objects[index].SerializedSize >
                    source.Objects[index].SerializedSize;
                ancestorIndex = source.Objects[index].ParentIndex;
            }
            True(allAncestorsGrew,
                "field materialization propagates size changes through every ancestor");
            True(source.Objects.Zip(scaled.Objects).All(pair =>
                    pair.First.Id == pair.Second.Id &&
                    pair.First.TypeHash == pair.Second.TypeHash &&
                    pair.First.RawName.Span.SequenceEqual(pair.Second.RawName.Span)),
                "field materialization preserves every catalog identity");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateProjectBranchRemoval(
        string sourcePath)
    {
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-remove-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            SmoDocument source = SmoDocument.Load(sourcePath);
            SmoProject project = SmoProject.Import(source);
            SmoProjectObject root = project.Objects
                .Where(item => item.ParentIndex is not null)
                .Where(item => project.CanRemoveInlineBranch(item.Index, out _))
                .OrderByDescending(item => project.Objects.Count(candidate =>
                    item.LogicalOffset <= candidate.LogicalOffset &&
                    (ulong)candidate.LogicalOffset + candidate.SerializedSize <=
                    (ulong)item.LogicalOffset + item.SerializedSize))
                .First();
            uint[] removedIds = project.Objects.Where(item =>
                    root.LogicalOffset <= item.LogicalOffset &&
                    (ulong)item.LogicalOffset + item.SerializedSize <=
                    (ulong)root.LogicalOffset + root.SerializedSize)
                .Select(item => item.Id)
                .ToArray();
            byte[] immutableData = project.DataSection.ToArray();

            project.RemoveInlineBranch(root.Index);
            string projectPath = Path.Combine(directory, "removed.smolvlproj");
            string outputPath = Path.Combine(directory, "removed.smo");
            SmoProjectArchive.Save(project, projectPath);
            SmoProject reopened =
                SmoProjectArchive.Load(projectPath);
            SmoProjectBuildResult result =
                SmoProjectSerializer.Build(reopened, outputPath);
            SmoDocument rebuilt = SmoDocument.Load(outputPath);

            True(reopened.Manifest.BranchRemovals.Count == 1 &&
                 reopened.DataSection.Span.SequenceEqual(immutableData),
                "project persists branch removal without changing immutable data.bin");
            True(!rebuilt.HasErrors &&
                 rebuilt.Objects.Count == source.Objects.Count - removedIds.Length &&
                 result.ObjectCount == rebuilt.Objects.Count,
                "branch removal rebuilds a structurally valid smaller object catalog");
            True(removedIds.All(id => rebuilt.Objects.All(item => item.Id != id)),
                "branch removal omits the complete inline subtree from the new catalog");
            True(rebuilt.Objects.All(item =>
            {
                SmoObjectEntry original = source.Objects.Single(candidate =>
                    candidate.Id == item.Id);
                return original.TypeHash == item.TypeHash &&
                       original.RawName.Span.SequenceEqual(item.RawName.Span);
            }),
                "branch removal preserves every surviving catalog identity");
            True(result.FileSize < source.Data.Length,
                "branch removal emits a smaller SMO without an intermediate source rewrite");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateProjectResourceRelocation()
    {
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-relocate-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            SmoDocument source = SmoDocument.Parse(
                BuildRelocatableResourceSample(),
                "synthetic-relocatable-resource.smo");
            True(!source.HasErrors && source.Objects.Count == 3,
                "synthetic relocation graph is structurally valid");
            SmoProject project = SmoProject.Import(source);
            SmoProjectObject resource = project.Objects[1];
            SmoProjectObject target = project.Objects[2];
            True(project.CanRelocateInlineLeaf(resource.Index, target.Index, out _),
                "synthetic inline resource can move to its reference consumer");
            uint resourceId = resource.Id;
            uint sourceOwnerId = project.Objects[
                resource.ParentIndex!.Value].Id;
            uint targetOwnerId = target.Id;
            byte[] immutableData = project.DataSection.ToArray();

            project.RelocateInlineLeaf(
                resource.Index,
                target.Index);
            string projectPath = Path.Combine(directory, "relocated.smolvlproj");
            string outputPath = Path.Combine(directory, "relocated.smo");
            SmoProjectArchive.Save(project, projectPath);
            SmoProject reopened =
                SmoProjectArchive.Load(projectPath);
            SmoProjectBuildResult result =
                SmoProjectSerializer.Build(reopened, outputPath);
            SmoDocument rebuilt = SmoDocument.Load(outputPath);
            SmoObjectEntry relocated = rebuilt.Objects.Single(item =>
                item.Id == resourceId);
            SmoObjectEntry targetOwner = rebuilt.Objects.Single(item =>
                item.Id == targetOwnerId);
            SmoObjectEntry sourceOwner = rebuilt.Objects.Single(item =>
                item.Id == sourceOwnerId);

            True(reopened.Manifest.ResourceRelocations.Count == 1 &&
                 reopened.DataSection.Span.SequenceEqual(immutableData),
                "project persists owner relocation without changing immutable data.bin");
            True(!rebuilt.HasErrors &&
                 rebuilt.Objects.Count == source.Objects.Count &&
                 result.ObjectCount == source.Objects.Count,
                "owner relocation preserves a structurally valid complete catalog");
            True(relocated.ParentIndex == targetOwner.Index,
                "relocated resource is physically owned by the selected consumer");
            True(sourceOwner.Index != relocated.ParentIndex &&
                 sourceOwner.SerializedSize < source.Objects.Single(item =>
                     item.Id == sourceOwnerId).SerializedSize &&
                 targetOwner.SerializedSize > source.Objects.Single(item =>
                     item.Id == targetOwnerId).SerializedSize,
                "relocation shrinks the old owner and grows the new owner");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateProjectReferencePlacement()
    {
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-reference-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            Matrix4x4 originalWorld = Matrix4x4.CreateTranslation(10, 20, 30);
            SmoDocument source = SmoDocument.Parse(
                BuildReferencePlacementSample(originalWorld),
                "synthetic-reference-placement.smo");
            True(!source.HasErrors && source.Objects.Count == 3,
                "synthetic reference placement graph is structurally valid");
            SmoProject project = SmoProject.Import(source);
            SmoProjectObject template = project.Objects.Single(item =>
                item.TypeHash == SmoClassIds.StaticRenderObject);
            True(project.CanAddReferencePlacement(template.Index, out _),
                "reference-only static branch passes the placement gate");
            byte[] immutableData = project.DataSection.ToArray();
            Vector3 delta = new(100, -5, 250);
            uint newId = project.AddReferencePlacementTranslated(
                template.Index,
                delta,
                "placement-copy");
            Vector3 secondPosition = new(-25, 15, 40);
            uint secondId = project.AddReferencePlacementAt(
                template.Index,
                secondPosition,
                "placement-copy-2");
            Vector3 updatedSecondPosition = new(-50, 35, 80);
            project.SetPlacementTransform(
                secondId,
                Matrix4x4.CreateTranslation(updatedSecondPosition));
            uint discardedId = project.AddReferencePlacementAt(
                template.Index,
                new Vector3(1, 2, 3),
                "placement-discarded");
            project.RemovePlacement(discardedId);
            True(project.Manifest.ReferencePlacements.Count == 2 &&
                 project.Manifest.ReferencePlacements.All(item =>
                     !item.NewObjectIds.Contains(discardedId)),
                "generated reference placement can be removed from the journal");
            string projectPath = Path.Combine(directory, "reference.smolvlproj");
            string outputPath = Path.Combine(directory, "reference.smo");
            SmoProjectArchive.Save(project, projectPath);
            SmoProject reopened =
                SmoProjectArchive.Load(projectPath);
            SmoProjectBuildResult result =
                SmoProjectSerializer.Build(reopened, outputPath);
            SmoDocument rebuilt = SmoDocument.Load(outputPath);
            SmoObjectEntry added = rebuilt.Objects.Single(item => item.Id == newId);
            SmoObjectEntry original = rebuilt.Objects.Single(item => item.Id == template.Id);

            True(reopened.Manifest.ReferencePlacements.Count == 2 &&
                 reopened.DataSection.Span.SequenceEqual(immutableData),
                "reference placement persists as an operation over immutable data.bin");
            True(!rebuilt.HasErrors && rebuilt.Objects.Count == source.Objects.Count + 2 &&
                 result.ObjectCount == source.Objects.Count + 2,
                "reference placements add valid catalog branches at one insertion point");
            True(rebuilt.Objects.Count(item => item.TypeHash == SmoClassIds.MeshData) == 1,
                "reference placement does not duplicate mesh geometry");
            True(added.ParentIndex == original.ParentIndex &&
                 added.RawName.Span.SequenceEqual("placement-copy\0"u8),
                "reference placement receives a new identity under the original owner");
            True(TryReadSyntheticStaticWorld(
                    rebuilt,
                    added,
                    out Matrix4x4 addedWorld) &&
                 Vector3.Distance(
                    Translation(addedWorld),
                    Translation(originalWorld) + delta) < 0.0001f,
                "reference placement stores its independent world transform");
            SmoObjectEntry second = rebuilt.Objects.Single(item => item.Id == secondId);
            True(TryReadSyntheticStaticWorld(
                    rebuilt,
                    second,
                    out Matrix4x4 secondWorld) &&
                 Vector3.Distance(
                    Translation(secondWorld),
                    updatedSecondPosition) < 0.0001f,
                "generated reference placement accepts a later transform edit");

            reopened.RemoveInlineBranch(template.Index);
            string replacedProjectPath = Path.Combine(directory, "reference-replaced.smolvlproj");
            string replacedOutputPath = Path.Combine(directory, "reference-replaced.smo");
            SmoProjectArchive.Save(reopened, replacedProjectPath);
            _ = SmoProjectSerializer.Build(reopened, replacedOutputPath);
            SmoDocument replaced = SmoDocument.Load(replacedOutputPath);
            True(!replaced.HasErrors &&
                 replaced.Objects.Count == source.Objects.Count + 1 &&
                 replaced.Objects.All(item => item.Id != template.Id) &&
                 replaced.Objects.Any(item => item.Id == newId) &&
                 replaced.Objects.Any(item => item.Id == secondId),
                "new reference placements survive removal of their template branch");
            True(replaced.Objects.Count(item => item.TypeHash == SmoClassIds.MeshData) == 1,
                "template removal keeps the resource shared by the new placement");

            SmoDocument physicalSource = SmoDocument.Parse(
                BuildPhysicalReferencePlacementSample(originalWorld),
                "synthetic-physical-reference-placement.smo");
            SmoProject physicalProject = SmoProject.Import(physicalSource);
            SmoProjectObject physicalMesh = physicalProject.Objects.Single(item =>
                item.TypeHash == SmoClassIds.MeshData);
            uint physicalCopyId = physicalProject.AddReferencePlacementForResource(
                physicalMesh.Id,
                Matrix4x4.CreateTranslation(70, 80, 90),
                "physical-placement-copy");
            string physicalOutputPath = Path.Combine(directory, "physical-reference.smo");
            _ = SmoProjectSerializer.Build(physicalProject, physicalOutputPath);
            SmoDocument physicalRebuilt = SmoDocument.Load(physicalOutputPath);
            SmoObjectEntry physicalCopy = physicalRebuilt.Objects.Single(item =>
                item.Id == physicalCopyId);
            bool physicalUsesReference = SmoObjectFieldReader
                .Read(physicalRebuilt, physicalCopy)
                .Any(field =>
                    field.PayloadSize == 8 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span) ==
                    physicalMesh.Id &&
                    BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span[4..]) == 0);
            True(!physicalRebuilt.HasErrors &&
                 physicalRebuilt.Objects.Count == physicalSource.Objects.Count + 1 &&
                 physicalRebuilt.Objects.Count(item =>
                     item.TypeHash == SmoClassIds.MeshData) == 1,
                "a physical mesh owner produces one lightweight placement without duplication");
            True(physicalUsesReference &&
                 physicalRebuilt.Objects.Single(item => item.Id == physicalMesh.Id).Index <
                 physicalCopy.Index,
                "derived placement uses its own shell and is emitted after the physical resource");

            SmoProject promotionProject = SmoProject.Import(physicalSource);
            SmoProjectObject promotionMesh = promotionProject.Objects.Single(item =>
                item.TypeHash == SmoClassIds.MeshData);
            SmoProjectObject promotionSource = promotionProject.Objects.Single(item =>
                item.TypeHash == SmoClassIds.StaticRenderObject);
            uint promotedCopyId = promotionProject.AddReferencePlacementForResource(
                promotionMesh.Id,
                Matrix4x4.CreateTranslation(100, 200, 300),
                "promoted-physical-copy");
            uint remainingCopyId = promotionProject.AddReferencePlacementForResource(
                promotionMesh.Id,
                Matrix4x4.CreateTranslation(400, 500, 600),
                "remaining-reference-copy");
            promotionProject.RemovePlacement(promotionSource.Id);
            string promotedOutputPath = Path.Combine(
                directory,
                "physical-reference-promoted.smo");
            _ = SmoProjectSerializer.Build(promotionProject, promotedOutputPath);
            SmoDocument promoted = SmoDocument.Load(promotedOutputPath);
            SmoObjectEntry promotedRoot = promoted.Objects.Single(item =>
                item.Id == promotedCopyId);
            SmoObjectEntry remainingRoot = promoted.Objects.Single(item =>
                item.Id == remainingCopyId);
            SmoObjectEntry promotedMesh = promoted.Objects.Single(item =>
                item.Id == promotionMesh.Id);
            bool promoterOwnsMesh = promotedMesh.ParentIndex == promotedRoot.Index &&
                SmoObjectFieldReader.Read(promoted, promotedRoot).Any(field =>
                    field.PayloadSize > 8 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span) ==
                    promotionMesh.Id);
            bool survivorReferencesMesh = SmoObjectFieldReader.Read(promoted, remainingRoot)
                .Any(field =>
                    field.PayloadSize == 8 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span) ==
                    promotionMesh.Id &&
                    BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span[4..]) == 0);
            True(!promoted.HasErrors &&
                 promoted.Objects.All(item => item.Id != promotionSource.Id) &&
                 promoted.Objects.Count(item =>
                     item.TypeHash == SmoClassIds.MeshData) == 1,
                "removing a physical source keeps exactly one shared mesh resource");
            True(promoterOwnsMesh && survivorReferencesMesh &&
                 promotedMesh.Index < remainingRoot.Index,
                "first surviving generated copy becomes the physical owner and later copies keep references");

            SmoDocument relocationSource = SmoDocument.Parse(
                BuildPhysicalReferencePlacementSample(
                    originalWorld,
                    includeReferenceConsumer: true),
                "synthetic-physical-reference-relocation.smo");
            SmoProject relocationProject = SmoProject.Import(relocationSource);
            SmoProjectObject relocationMesh = relocationProject.Objects.Single(item =>
                item.TypeHash == SmoClassIds.MeshData);
            SmoProjectObject relocationOwner = relocationProject.Objects.Single(item =>
                item.TypeHash == SmoClassIds.StaticRenderObject &&
                item.Index == relocationMesh.ParentIndex);
            SmoProjectObject relocationConsumer = relocationProject.Objects.Single(item =>
                item.TypeHash == SmoClassIds.StaticRenderObject &&
                item.Index != relocationOwner.Index);
            uint relocatedCopyId = relocationProject.AddReferencePlacementForResource(
                relocationMesh.Id,
                Matrix4x4.CreateTranslation(700, 800, 900),
                "relocated-resource-copy");
            relocationProject.RemovePlacement(relocationOwner.Id);
            string relocationOutputPath = Path.Combine(
                directory,
                "physical-reference-relocated.smo");
            _ = SmoProjectSerializer.Build(relocationProject, relocationOutputPath);
            SmoDocument relocationRebuilt = SmoDocument.Load(relocationOutputPath);
            SmoObjectEntry relocatedMesh = relocationRebuilt.Objects.Single(item =>
                item.Id == relocationMesh.Id);
            SmoObjectEntry relocatedConsumer = relocationRebuilt.Objects.Single(item =>
                item.Id == relocationConsumer.Id);
            SmoObjectEntry relocatedCopy = relocationRebuilt.Objects.Single(item =>
                item.Id == relocatedCopyId);
            bool relocatedCopyOwnsMesh = relocatedMesh.ParentIndex == relocatedCopy.Index &&
                SmoObjectFieldReader.Read(relocationRebuilt, relocatedCopy)
                .Any(field =>
                    field.PayloadSize > 8 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span) ==
                    relocationMesh.Id);
            bool importedConsumerReferencesMesh = SmoObjectFieldReader
                .Read(relocationRebuilt, relocatedConsumer)
                .Any(field =>
                    field.PayloadSize == 8 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span) ==
                    relocationMesh.Id &&
                    BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span[4..]) == 0);
            True(!relocationRebuilt.HasErrors &&
                 relocationRebuilt.Objects.All(item => item.Id != relocationOwner.Id) &&
                 relocationRebuilt.Objects.Count(item =>
                     item.TypeHash == SmoClassIds.MeshData) == 1,
                "owner removal preserves a shared mesh and keeps the generated placement valid");
            True(relocatedCopyOwnsMesh &&
                 importedConsumerReferencesMesh &&
                 relocatedMesh.Index < relocatedConsumer.Index,
                "generated owner serializes the resource before imported references without overlapping edits");

            SmoDocument duplicateReferenceSource = SmoDocument.Parse(
                BuildPhysicalReferencePlacementSample(
                    originalWorld,
                    includeReferenceConsumer: true,
                    duplicateReferenceConsumerField: true),
                "synthetic-duplicate-reference-relocation.smo");
            SmoProject duplicateReferenceProject = SmoProject.Import(
                duplicateReferenceSource);
            SmoProjectObject duplicateReferenceMesh = duplicateReferenceProject.Objects
                .Single(item => item.TypeHash == SmoClassIds.MeshData);
            SmoProjectObject duplicateReferenceOwner = duplicateReferenceProject.Objects
                .Single(item =>
                    item.TypeHash == SmoClassIds.StaticRenderObject &&
                    item.Index == duplicateReferenceMesh.ParentIndex);
            uint duplicateReferenceConsumerId = duplicateReferenceProject.Objects
                .Single(item =>
                    item.TypeHash == SmoClassIds.StaticRenderObject &&
                    item.Index != duplicateReferenceOwner.Index)
                .Id;
            duplicateReferenceProject.RemovePlacement(duplicateReferenceOwner.Id);
            string duplicateReferenceOutputPath = Path.Combine(
                directory,
                "duplicate-reference-relocated.smo");
            _ = SmoProjectSerializer.Build(
                duplicateReferenceProject,
                duplicateReferenceOutputPath);
            SmoDocument duplicateReferenceRebuilt = SmoDocument.Load(
                duplicateReferenceOutputPath);
            SmoObjectEntry duplicateReferenceConsumer = duplicateReferenceRebuilt.Objects
                .Single(item => item.Id == duplicateReferenceConsumerId);
            SmoObjectEntry duplicateReferenceRelocatedMesh =
                duplicateReferenceRebuilt.Objects.Single(item =>
                    item.Id == duplicateReferenceMesh.Id);
            SmoObjectField[] duplicateReferenceFields = SmoObjectFieldReader
                .Read(duplicateReferenceRebuilt, duplicateReferenceConsumer)
                .Where(field =>
                    field.PayloadSize >= sizeof(uint) &&
                    BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span) ==
                    duplicateReferenceMesh.Id)
                .ToArray();
            True(!duplicateReferenceRebuilt.HasErrors &&
                 duplicateReferenceRelocatedMesh.ParentIndex ==
                 duplicateReferenceConsumer.Index,
                "shared mesh relocation selects an exact field when one owner has duplicate references");
            True(duplicateReferenceFields.Count(field => field.PayloadSize > 8) == 1 &&
                 duplicateReferenceFields.Count(field => field.PayloadSize == 8) == 1,
                "exact duplicate-reference relocation promotes one field and preserves the other reference");

            SmoProject bakedProject = SmoProject.Import(SmoDocument.Parse(
                BuildPhysicalReferencePlacementSample(
                    originalWorld,
                    useStaticPlacement: false),
                "synthetic-baked-partition-resource.smo"));
            uint bakedMeshId = bakedProject.Objects.Single(item =>
                item.TypeHash == SmoClassIds.MeshData).Id;
            True(!bakedProject.CanAddReferencePlacementForResource(
                     bakedMeshId,
                     out string bakedReason) &&
                 bakedReason.Contains("baked partition", StringComparison.OrdinalIgnoreCase),
                "baked partition geometry is rejected instead of receiving an unsafe foreign shell");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateProjectAddedForest()
    {
        const uint childType = 0xA0030001;
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-forest-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            SmoDocument source = SmoDocument.Parse(
                BuildStaticPlacementSample(Matrix4x4.Identity),
                "synthetic-added-forest.smo");
            SmoProject project = SmoProject.Import(source);
            SmoProjectObject owner = project.Objects[0];
            SmoProjectField terminal = owner.Fields.Single(field =>
                field.PayloadSize == 0 &&
                field.RelativeHeaderOffset + field.HeaderSize == owner.SerializedSize);
            uint childId = project.AllocateObjectIds(1)[0];

            byte[] childTerminal = SmoDataBlockWriter.BuildField(0, []);
            byte[] child = new byte[checked(8 + childTerminal.Length)];
            BinaryPrimitives.WriteUInt32LittleEndian(child, childType);
            "SBOO"u8.CopyTo(child.AsSpan(4));
            childTerminal.CopyTo(child, 8);
            byte[] payload = new byte[checked(8 + child.Length)];
            BinaryPrimitives.WriteUInt32LittleEndian(payload, childId);
            BinaryPrimitives.WriteUInt32LittleEndian(
                payload.AsSpan(4),
                checked((uint)child.Length));
            child.CopyTo(payload, 8);
            byte[] forestData = SmoDataBlockWriter.BuildField(23, payload);
            int fieldHeaderSize = forestData.Length - payload.Length;
            var descriptor = new SmoProjectAddedObject
            {
                Id = childId,
                RawName = "added-child\0"u8.ToArray(),
                TypeHash = childType,
                ObjectRelativeOffset = checked((uint)(fieldHeaderSize + 8)),
                SerializedSize = checked((uint)child.Length),
                ParentObjectId = owner.Id
            };
            byte[] immutableData = project.DataSection.ToArray();
            var session = new SmoProjectSession(project);
            Guid blobId = Guid.Empty;
            True(session.Execute(
                    "add opaque forest",
                    current => blobId = current.AddInlineForest(
                        owner.Index,
                        terminal.RelativeHeaderOffset,
                        forestData,
                        [descriptor])) &&
                 project.Manifest.AddedForests.Count == 1 &&
                 project.DataSection.Span.SequenceEqual(immutableData),
                "added forest records metadata and leaves imported data.bin immutable");
            True(session.Undo() && project.Manifest.AddedForests.Count == 0 &&
                 session.Redo() && project.Manifest.AddedForests.Count == 1,
                "added forest participates in compact undo and redo history");

            string projectPath = Path.Combine(directory, "forest.smolvlproj");
            string outputPath = Path.Combine(directory, "forest.smo");
            SmoProjectArchive.Save(project, projectPath);
            using (ZipArchive archive = ZipFile.OpenRead(projectPath))
            {
                ZipArchiveEntry? asset = archive.GetEntry(
                    $"{SmoProject.AssetEntryPrefix}{blobId:N}.bin");
                True(asset is not null && asset.Length == forestData.Length &&
                     archive.GetEntry(SmoProject.DataEntryName)?.Length ==
                         immutableData.Length,
                    "project archive stores added binary separately from immutable data.bin");
            }

            SmoProject reopened =
                SmoProjectArchive.Load(projectPath);
            SmoProjectBuildResult result =
                SmoProjectSerializer.Build(reopened, outputPath);
            SmoDocument rebuilt = SmoDocument.Load(outputPath);
            SmoObjectEntry rebuiltOwner = rebuilt.Objects.Single(item => item.Id == owner.Id);
            SmoObjectEntry rebuiltChild = rebuilt.Objects.Single(item => item.Id == childId);
            True(reopened.Manifest.AddedForests.Count == 1 &&
                 reopened.DataSection.Span.SequenceEqual(immutableData) &&
                 result.ObjectCount == source.Objects.Count + 1,
                "added forest survives project archive round-trip and enters the new catalog");
            True(!rebuilt.HasErrors && rebuiltChild.ParentIndex == rebuiltOwner.Index &&
                 rebuiltChild.TypeHash == childType &&
                 rebuiltOwner.SerializedSize == owner.SerializedSize + forestData.Length,
                "rebuilt SMO contains the opaque child under a correctly resized owner");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateProjectCollisionProject(
        string sourcePath,
        string? retainedOutputDirectory)
    {
        string directory = retainedOutputDirectory ?? Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-collision-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            SmoDocument source = SmoDocument.Load(sourcePath);
            SmoCollisionMesh template = SmoCollisionMeshDecoder.DecodeAll(source)
                .First(collision => collision.Positions.Count >= 4 &&
                                    collision.TriangleIndices.Count >= 12 &&
                                    DecodeMeshBoundingVolume(
                                        source,
                                        collision.MeshBoundingVolumeObjectIndex)
                                        .FaceData is { Count: > 0 });
            Vector3[] worldPositions = template.Positions
                .Select(position => Vector3.Transform(position, template.WorldTransform))
                .ToArray();
            int[] indices = template.TriangleIndices.ToArray();

            uint importedCollisionId =
                source.Objects[template.CollisionInfoObjectIndex].Id;
            byte[]? importedFaceData = ReadOptionalFieldPayload(
                source,
                template.MeshBoundingVolumeObjectIndex,
                fieldType: 1);
            SmoProject importedCollisionProject = SmoProject.Import(source);
            var importedCollisionSession = new SmoProjectSession(
                importedCollisionProject);
            Matrix4x4 importedMovedWorld = template.WorldTransform;
            importedMovedWorld.M41 += 3.5f;
            importedMovedWorld.M42 -= 2.25f;
            importedMovedWorld.M43 += 8.75f;
            True(importedCollisionSession.Execute(
                    "move imported collision",
                    current => current.SetCollisionTransforms(
                    [
                        new SmoProjectCollisionTransformEdit(
                            importedCollisionId,
                            template.WorldTransform,
                            importedMovedWorld)
                    ])),
                "an imported collision uses the same project transform operation");
            string importedMovedPath = Path.Combine(
                directory,
                "imported-collision-moved.smo");
            _ = SmoProjectSerializer.Build(
                importedCollisionProject,
                importedMovedPath);
            SmoDocument importedMoved = SmoDocument.Load(importedMovedPath);
            SmoCollisionMesh movedImportedCollision = SmoCollisionMeshDecoder
                .DecodeAll(importedMoved)
                .Single(candidate =>
                    importedMoved.Objects[candidate.CollisionInfoObjectIndex].Id ==
                    importedCollisionId);
            byte[]? movedImportedFaceData = ReadOptionalFieldPayload(
                importedMoved,
                movedImportedCollision.MeshBoundingVolumeObjectIndex,
                fieldType: 1);
            True(movedImportedCollision.TriangleIndices.SequenceEqual(indices),
                "moving an imported collision preserves exact triangle order");
            True(OptionalBytesEqual(importedFaceData, movedImportedFaceData),
                "moving an imported collision preserves wxFaceData byte-for-byte");
            True(importedCollisionSession.Execute(
                    "delete imported collision",
                    current => current.RemoveSceneBranches([importedCollisionId])) &&
                 importedCollisionProject.Manifest.ObjectDataReplacements.Count == 0,
                "deleting a moved imported collision also removes its pending mesh replacement");
            string importedDeletedPath = Path.Combine(
                directory,
                "imported-collision-deleted.smo");
            _ = SmoProjectSerializer.Build(
                importedCollisionProject,
                importedDeletedPath);
            SmoCollisionBranchRemovalResult legacyImportedRemoval =
                SmoCollisionBranchRemover.Remove(
                    source,
                    template.CollisionInfoObjectIndex);
            SmoDocument importedDeleted = SmoDocument.Load(importedDeletedPath);
            True(importedDeleted.Data.Span.SequenceEqual(
                    legacyImportedRemoval.Data) &&
                 importedDeleted.Objects.All(item => item.Id != importedCollisionId),
                "project deletion of an imported collision matches the proven immediate writer");

            SmoProject project = SmoProject.Import(source);
            byte[] immutableData = project.DataSection.ToArray();
            var session = new SmoProjectSession(project);
            SmoProjectCollisionAddition? addition = null;
            True(session.Execute(
                    "add planned collision",
                    current => addition = SmoProjectImporterBridge.AddCollision(
                        current,
                        source,
                        worldPositions,
                        indices,
                        "ProjectCollision")) &&
                 addition is not null &&
                 addition.AssetIds.Count == 2 &&
                 project.Manifest.AddedForests.Count == 2 &&
                 project.DataSection.Span.SequenceEqual(immutableData),
                "collision importer emits two project assets without rewriting data.bin");
            SmoProjectCollisionAddition addedCollision = addition ??
                throw new InvalidOperationException("Project collision addition was not captured.");
            var sourceLevel = new SmoLevelDocument(SmoLevelWorkspace.Load(sourcePath));
            SmoLevelEntity linkedVisual = sourceLevel.Entities.First(entity =>
                entity.Kind == SmoLevelEntityKind.Visual);
            uint linkedVisualObjectId = source.Objects[
                linkedVisual.Id.SceneObjectIndex].Id;
            True(session.Execute(
                    "link planned collision",
                    current => current.SetCollisionLinkOverride(
                        linkedVisualObjectId,
                        addedCollision.CollisionInfoObjectId,
                        present: true)),
                "project collision can be linked to a real visual transactionally");

            string projectPath = Path.Combine(directory, "collision.smolvlproj");
            string outputPath = Path.Combine(directory, "collision.smo");
            SmoProjectArchive.Save(project, projectPath);
            SmoProject reopened =
                SmoProjectArchive.Load(projectPath);
            _ = SmoProjectSerializer.Build(reopened, outputPath);
            SmoDocument rebuilt = SmoDocument.Load(outputPath);
            SmoObjectEntry collisionEntry = rebuilt.Objects.Single(entry =>
                entry.Id == addedCollision.CollisionInfoObjectId);
            SmoCollisionMesh generated = SmoCollisionMeshDecoder.DecodeAll(rebuilt)
                .Single(collision =>
                    collision.CollisionInfoObjectIndex == collisionEntry.Index);
            SmoCollisionInfoData generatedInfo = DecodeCollisionInfo(
                rebuilt,
                collisionEntry.Index);
            SmoMeshBoundingVolumeData generatedMeshData = DecodeMeshBoundingVolume(
                rebuilt,
                generated.MeshBoundingVolumeObjectIndex);
            True(!rebuilt.HasErrors &&
                 rebuilt.Objects.Count == source.Objects.Count + 2 &&
                 generated.Positions.Count == worldPositions.Length &&
                 generated.TriangleIndices.SequenceEqual(indices),
                "project build contains the complete generated collision branch");
            True(generatedInfo.CollisionGroup ==
                    SmoCollisionBranchAppender.ProductionDefaultCollisionGroup,
                "generated collision serializes the selected production Group 2 default");
            True(generatedMeshData.FaceData is null,
                "generated collision uses the native unspecified per-face default without invented wxFaceData");
            True(reopened.Manifest.CollisionLinkOverrides.Single() is
                    { Present: true,
                      VisualObjectId: var reopenedVisualId,
                      CollisionObjectId: var reopenedCollisionId } &&
                 reopenedVisualId == linkedVisualObjectId &&
                 reopenedCollisionId == addedCollision.CollisionInfoObjectId,
                "real visual/collision link survives project archive reopen as editor metadata");
            True(!SmoCollisionBranchAppender
                    .FindUnregisteredCollisionInfoObjectIndices(rebuilt)
                    .Contains(collisionEntry.Index),
                "project collision is registered in the native physics registry");

            SmoCollisionBranchAppendResult legacy = SmoCollisionBranchAppender.Append(
                source,
                worldPositions,
                indices,
                "ProjectCollision");
            True(rebuilt.Data.Span.SequenceEqual(legacy.Data),
                "project build is byte-identical to the immediate importer writer");

            Matrix4x4 originalCollisionWorld = generated.WorldTransform;
            Matrix4x4 movedCollisionWorld = originalCollisionWorld;
            var collisionDelta = new Vector3(17.5f, 6.25f, -9.75f);
            movedCollisionWorld.M41 += collisionDelta.X;
            movedCollisionWorld.M42 += collisionDelta.Y;
            movedCollisionWorld.M43 += collisionDelta.Z;
            var editSession = new SmoProjectSession(reopened);
            True(editSession.Execute(
                    "move project collision",
                    current => current.SetCollisionTransforms(
                    [
                        new SmoProjectCollisionTransformEdit(
                            addedCollision.CollisionInfoObjectId,
                            originalCollisionWorld,
                            movedCollisionWorld)
                    ])),
                "project collision transform is recorded transactionally");
            string movedPath = Path.Combine(directory, "collision-moved.smo");
            _ = SmoProjectSerializer.Build(reopened, movedPath);
            SmoDocument movedDocument = SmoDocument.Load(movedPath);
            SmoCollisionMesh movedCollision = SmoCollisionMeshDecoder.DecodeAll(movedDocument)
                .Single(collision =>
                    movedDocument.Objects[collision.CollisionInfoObjectIndex].Id ==
                    addedCollision.CollisionInfoObjectId);
            Vector3[] movedWorldPositions = movedCollision.Positions
                .Select(position => Vector3.Transform(position, movedCollision.WorldTransform))
                .ToArray();
            True(movedWorldPositions.Zip(worldPositions).All(pair =>
                    Vector3.Distance(pair.First, pair.Second + collisionDelta) < 0.001f),
                "project collision transform persists by baking the shared writer output");
            True(movedCollision.TriangleIndices.SequenceEqual(indices) &&
                 DecodeCollisionInfo(
                     movedDocument,
                     movedCollision.CollisionInfoObjectIndex).CollisionGroup ==
                    SmoCollisionBranchAppender.ProductionDefaultCollisionGroup,
                "moving a generated project collision preserves triangle order and Group 2");
            True(editSession.Execute(
                    "unlink project collision",
                    current => current.SetCollisionLinkOverride(
                        linkedVisualObjectId,
                        addedCollision.CollisionInfoObjectId,
                        present: false)) &&
                 reopened.Manifest.CollisionLinkOverrides.Single().Present == false,
                "project collision can be unlinked transactionally");
            True(editSession.Undo() &&
                 reopened.Manifest.CollisionLinkOverrides.Single().Present &&
                 editSession.Redo() &&
                 reopened.Manifest.CollisionLinkOverrides.Single().Present == false,
                "project collision unlink participates in Undo/Redo");

            True(editSession.Execute(
                    "delete project collision",
                    current => current.RemoveSceneBranches(
                        [addedCollision.CollisionInfoObjectId])) &&
                 reopened.Manifest.RemovedAddedForestIds.Count == 2 &&
                 reopened.Manifest.ObjectDataReplacements.Count == 0,
                "project collision delete tombstones geometry and its physics registry reference");
            string deletedPath = Path.Combine(directory, "collision-deleted.smo");
            _ = SmoProjectSerializer.Build(reopened, deletedPath);
            SmoDocument deleted = SmoDocument.Load(deletedPath);
            True(deleted.Data.Span.SequenceEqual(source.Data.Span) &&
                 deleted.Objects.All(entry =>
                     entry.Id != addedCollision.CollisionInfoObjectId &&
                     entry.Id != addedCollision.MeshBoundingVolumeObjectId),
                "deleting a newly added collision restores the imported SMO byte-for-byte");
            True(reopened.Manifest.CollisionLinkOverrides.Count == 0,
                "deleting a collision also removes its editor link metadata");
            True(editSession.Undo() && editSession.Undo() && editSession.Undo() &&
                 editSession.Redo() && editSession.Redo() && editSession.Redo(),
                "project collision move/unlink/delete all participate in Undo/Redo");
            if (retainedOutputDirectory is not null)
            {
                Console.WriteLine($"PROJECT={projectPath}");
                Console.WriteLine($"SMO={outputPath}");
                Console.WriteLine($"IMPORTED_MOVED_SMO={importedMovedPath}");
                Console.WriteLine($"MOVED_SMO={movedPath}");
                Console.WriteLine($"DELETED_SMO={deletedPath}");
            }
        }
        finally
        {
            if (retainedOutputDirectory is null)
                Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateProjectExternalProject(
        string sourcePath,
        string modelPath,
        string? retainedOutputDirectory)
    {
        string directory = retainedOutputDirectory ?? Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-project-external-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            SmoDocument source = SmoDocument.Load(sourcePath);
            ImportedScene imported = SmoLevelRigidImportPreparer.Prepare(
                ImportedModelReader.Read(modelPath));
            Matrix4x4 transform = Matrix4x4.CreateScale(100f) *
                Matrix4x4.CreateTranslation(-4300, 0, -600);
            SmoProject project = SmoProject.Import(source);
            byte[] immutableData = project.DataSection.ToArray();
            var session = new SmoProjectSession(project);
            SmoProjectExternalModelAddition? addition = null;
            string workerExecutable = Environment.ProcessPath ??
                throw new InvalidOperationException("Could not resolve test worker executable.");
            string? workerAssembly = Path.GetFileNameWithoutExtension(workerExecutable).Equals(
                "dotnet",
                StringComparison.OrdinalIgnoreCase)
                    ? Assembly.GetEntryAssembly()?.Location
                    : null;
            True(session.Execute(
                    "add external model",
                    current => addition = SmoProjectImporterBridge.AddExternalModelBatched(
                        current,
                        imported,
                        [transform],
                        modelPath,
                        Path.GetFileNameWithoutExtension(modelPath),
                        workerExecutable,
                        workerAssembly)) &&
                 addition is not null &&
                 addition.MeshObjectIds.Count == imported.Meshes.Count &&
                 addition.AssetIds.Count > 0,
                "real external importer result becomes one transactional asset group");
            True(project.DataSection.Span.SequenceEqual(immutableData),
                "real external import leaves project data.bin byte-identical");

            string projectPath = Path.Combine(directory, "external.smolvlproj");
            string outputPath = Path.Combine(directory, "external.smo");
            SmoProjectArchive.Save(project, projectPath);
            SmoProject reopened =
                SmoProjectArchive.Load(projectPath);
            _ = SmoProjectSerializer.Build(reopened, outputPath);
            SmoDocument rebuilt = SmoDocument.Load(outputPath);
            True(!rebuilt.HasErrors,
                "project external-model build remains structurally valid after archive reopen");
            True(addition!.MeshObjectIds.All(id =>
                    SmoMeshDecoder.Decode(
                        rebuilt,
                        rebuilt.Objects.Single(entry => entry.Id == id)).TriangleCount > 0),
                "every project-imported mesh remains decodable after archive reopen");
            True(addition.ImportedTextureObjectIds.Values.All(id =>
                    rebuilt.Objects.Any(entry =>
                        entry.Id == id && entry.TypeHash == SmoClassIds.TextureData)),
                "project archive preserves every imported texture object");
            SmoPreparedScene preparedScene =
                SmoViewer.Scene.SmoSceneBuilder.Build(rebuilt);
            for (int meshIndex = 0; meshIndex < imported.Meshes.Count; meshIndex++)
            {
                ImportedMesh sourceMesh = imported.Meshes[meshIndex];
                int objectIndex = rebuilt.Objects.Single(entry =>
                    entry.Id == addition.MeshObjectIds[meshIndex]).Index;
                SmoSceneMesh occurrence = preparedScene.Meshes.Single(mesh =>
                    mesh.Mesh.ObjectIndex == objectIndex &&
                    mesh.SharedInstance is null);
                bool expectsTextureAlpha = sourceMesh.MaterialIndex >= 0 &&
                    sourceMesh.MaterialIndex < imported.Materials.Count &&
                    imported.Materials[sourceMesh.MaterialIndex].UsesTextureAlpha;
                SmoMaterialRenderStateInfo? state = occurrence.MaterialRenderState;
                bool hasSerializedRigidAlphaContract =
                    state?.HasRigidTextureAlphaSurfaceContext == true;
                bool uvConfirmsPartialAlpha =
                    state?.TextureUvAlphaCoverage.HasPartialAlpha == true;
                True(expectsTextureAlpha
                        ? hasSerializedRigidAlphaContract &&
                          (!uvConfirmsPartialAlpha ||
                           occurrence.UsesAlphaBlend &&
                           occurrence.RequiresTransparentOrdering &&
                           state?.BlendMode == SmoMaterialBlendMode
                               .RigidTextureAlphaSurfaceFinalBlend2)
                        : !occurrence.UsesAlphaBlend,
                    $"mesh {meshIndex} retains its imported opaque/alpha material state " +
                    $"(expectedAlpha={expectsTextureAlpha}; usesAlpha=" +
                    $"{occurrence.UsesAlphaBlend}; transparentOrder=" +
                    $"{occurrence.RequiresTransparentOrdering}; blendMode=" +
                    $"{state?.BlendMode}; finalBlend={state?.FinalBlendOperation}; " +
                    $"uvCoverage={state?.TextureUvAlphaCoverage})");
            }
            foreach ((int textureIndex, uint objectId) in
                     addition.ImportedTextureObjectIds.OrderBy(pair => pair.Key))
            {
                ImportedTexture sourceTexture = imported.Textures[textureIndex];
                SmoObjectEntry textureEntry = rebuilt.Objects.Single(entry =>
                    entry.Id == objectId);
                True(SmoTextureDecoder.TryDecode(
                        rebuilt,
                        textureEntry,
                        out SmoTexture? decodedTexture,
                        out _) &&
                     decodedTexture.Width == sourceTexture.Width &&
                     decodedTexture.Height == sourceTexture.Height,
                    $"texture {textureIndex} retains its imported dimensions");
                using Image<Rgba32> sourceImage = Image.Load<Rgba32>(sourceTexture.Data);
                byte[] rgba = new byte[checked(sourceImage.Width * sourceImage.Height * 4)];
                sourceImage.CopyPixelDataTo(rgba);
                ReadOnlySpan<byte> bgra = decodedTexture!.Bgra32Pixels.Span;
                bool pixelsEqual = bgra.Length == rgba.Length;
                for (int offset = 0; pixelsEqual && offset < rgba.Length; offset += 4)
                {
                    pixelsEqual = bgra[offset] == rgba[offset + 2] &&
                                  bgra[offset + 1] == rgba[offset + 1] &&
                                  bgra[offset + 2] == rgba[offset] &&
                                  bgra[offset + 3] == rgba[offset + 3];
                }
                True(pixelsEqual,
                    $"texture {textureIndex} retains exact BGRA color and alpha bytes");
            }
            True(addition.PlacementRootObjectIds.Count == imported.Meshes.Count,
                "external import reports every editable initial placement root");

            uint initialRootId = addition.PlacementRootObjectIds[0];
            Matrix4x4 movedTransform = Matrix4x4.CreateScale(100f) *
                Matrix4x4.CreateTranslation(-4100, 75, -450);
            var movedSession = new SmoProjectSession(reopened);
            True(movedSession.Execute(
                    "move initial asset placement",
                    current => current.SetPlacementTransform(
                        initialRootId,
                        movedTransform)) &&
                 reopened.Manifest.AddedObjectPropertyEdits.Count == 2 &&
                 reopened.DataSection.Span.SequenceEqual(immutableData),
                "initial external placement stores matrix overlays without mutating data.bin");
            string movedProjectPath = Path.Combine(directory, "external-moved.smolvlproj");
            string movedOutputPath = Path.Combine(directory, "external-moved.smo");
            SmoProjectArchive.Save(reopened, movedProjectPath);
            SmoProject movedProject =
                SmoProjectArchive.Load(movedProjectPath);
            _ = SmoProjectSerializer.Build(movedProject, movedOutputPath);
            SmoDocument moved = SmoDocument.Load(movedOutputPath);
            SmoObjectEntry movedRoot = moved.Objects.Single(entry => entry.Id == initialRootId);
            SmoPropertyDescriptor movedWorldDescriptor =
                SmoSchemaRegistry.Describe(moved, movedRoot).Properties
                    .Single(property =>
                        property.Descriptor.Key == SmoPropertyKeys.WorldMatrix)
                    .Descriptor;
            SmoObjectField movedWorldField = SmoObjectFieldReader.Read(moved, movedRoot)
                .Single(movedWorldDescriptor.Field.Matches);
            True(movedWorldField.Payload.Span.Slice(
                     movedWorldDescriptor.PayloadOffset,
                     movedWorldDescriptor.ValueSize).SequenceEqual(
                     SmoPropertyValueCodec.Encode(movedTransform)),
                "archive reopen applies the edited initial external placement transform");
            int assetCount = project.Manifest.AddedForests.Count;
            uint copyRootId = 0;
            Matrix4x4 copyTransform = Matrix4x4.CreateScale(100f) *
                Matrix4x4.CreateTranslation(-4000, 50, -300);
            True(session.Execute(
                    "place imported resource again",
                    current => copyRootId = current.AddReferencePlacementForResource(
                        addition.MeshObjectIds[0],
                        copyTransform,
                        "shrek_reference_copy")) &&
                 project.Manifest.ReferencePlacements.Count == 1,
                "new project mesh resource accepts a reference-only placement");
            string copyProjectPath = Path.Combine(directory, "external-copy.smolvlproj");
            string copyOutputPath = Path.Combine(directory, "external-copy.smo");
            SmoProjectArchive.Save(project, copyProjectPath);
            SmoProject copyProject =
                SmoProjectArchive.Load(copyProjectPath);
            _ = SmoProjectSerializer.Build(copyProject, copyOutputPath);
            SmoDocument copied = SmoDocument.Load(copyOutputPath);
            SmoObjectEntry copyRoot = copied.Objects.Single(entry => entry.Id == copyRootId);
            SmoSharedMeshInstanceInfo copyInstance =
                SmoSharedMeshInstanceResolver.ResolveAll(copied).Single(instance =>
                    instance.StaticObjectIndex == copyRoot.Index);
            True(copyInstance.SourceMeshObjectId == addition.MeshObjectIds[0] &&
                 Vector3.DistanceSquared(
                     copyInstance.WorldTransform.Translation,
                     copyTransform.Translation) < 0.01f &&
                 copied.Objects.Count > rebuilt.Objects.Count,
                "reference-only copy resolves the asset mesh and requested transform");
            True(copied.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) ==
                 rebuilt.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) &&
                 copied.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData) ==
                 rebuilt.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData),
                "copying an imported resource duplicates neither mesh nor texture objects");
            True(copied.Objects.Single(entry =>
                     entry.Id == addition.MeshObjectIds[0]).Index < copyRoot.Index,
                "project-owned mesh is emitted before a lightweight copy which references it");

            SmoProject promotionProject =
                SmoProjectArchive.Load(copyProjectPath);
            var promotionSession = new SmoProjectSession(promotionProject);
            True(promotionSession.Execute(
                    "delete owning placement while a reference survives",
                    current => current.RemoveSceneBranches([initialRootId])) &&
                 promotionProject.Manifest.ReferencePlacements.Count == 0 &&
                 promotionProject.Manifest.RemovedAddedForestIds.Count == 0,
                "deleting an asset-owning placement promotes a surviving reference");
            string promotionOutputPath = Path.Combine(
                directory,
                "external-promoted-reference.smo");
            _ = SmoProjectSerializer.Build(
                promotionProject,
                promotionOutputPath);
            SmoDocument promoted = SmoDocument.Load(promotionOutputPath);
            SmoObjectEntry promotedRoot = promoted.Objects.Single(entry =>
                entry.Id == initialRootId);
            True(SmoStaticRenderObjectTransformDecoder.TryDecode(
                    promoted,
                    promotedRoot,
                    out Matrix4x4 promotedTransform),
                "promoted asset owner keeps a decodable static transform");
            True(promoted.Objects.All(entry => entry.Id != copyRootId) &&
                 promoted.Objects.Any(entry => entry.Id == addition.MeshObjectIds[0]) &&
                 Vector3.DistanceSquared(
                     promotedTransform.Translation,
                     copyTransform.Translation) < 0.01f,
                "promotion keeps one visible copy and the shared mesh at the copy transform");
            True(promotionSession.Undo() && promotionSession.Redo(),
                "asset-owner promotion is one reversible project operation");

            uint sharedSourceMeshId = SmoSharedMeshInstanceResolver.ResolveAll(source)
                .GroupBy(instance => instance.SourceMeshObjectId)
                .Where(group => group.Count() > 1)
                .Select(group => group.Key)
                .First();
            SmoObjectEntry redirectSource = source.Objects.Single(entry =>
                entry.Id == sharedSourceMeshId);
            True(redirectSource.TypeHash == SmoClassIds.MeshData,
                "real replacement fixture selects a shared imported mesh resource");
            uint redirectTargetId = addition.MeshObjectIds[0];
            var redirectSession = new SmoProjectSession(copyProject);
            True(redirectSession.Execute(
                    "redirect imported mesh to project asset",
                    current => current.RedirectResource(
                        redirectSource.Id,
                        redirectTargetId)) &&
                 copyProject.Manifest.ResourceRedirects.Count == 1 &&
                 redirectSession.Undo() &&
                 copyProject.Manifest.ResourceRedirects.Count == 0 &&
                 redirectSession.Redo(),
                "resource redirect is one undoable project operation");
            string redirectProjectPath = Path.Combine(directory, "external-redirect.smolvlproj");
            string redirectOutputPath = Path.Combine(directory, "external-redirect.smo");
            SmoProjectArchive.Save(copyProject, redirectProjectPath);
            SmoProject redirectProject =
                SmoProjectArchive.Load(redirectProjectPath);
            _ = SmoProjectSerializer.Build(redirectProject, redirectOutputPath);
            SmoDocument redirected = SmoDocument.Load(redirectOutputPath);
            bool HasReference(SmoObjectEntry owner, uint objectId) =>
                SmoObjectFieldReader.Read(redirected, owner).Any(field =>
                    field.PayloadSize == 8 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span) == objectId &&
                    BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span[4..]) == 0);
            True(!redirected.Objects.Any(entry => entry.Id == redirectSource.Id) &&
                 redirected.Objects.Any(entry => entry.Id == redirectTargetId) &&
                 redirected.Objects.All(entry => !HasReference(entry, redirectSource.Id)),
                "build-time reachability omits the old mesh and leaves no stale references");
            SmoSharedMeshInstanceInfo[] redirectedInstances =
                SmoSharedMeshInstanceResolver.ResolveAll(redirected)
                    .Where(instance => instance.SourceMeshObjectId == redirectTargetId)
                    .ToArray();
            int redirectedCopyRootIndex = redirected.Objects.Single(entry =>
                entry.Id == copyRootId).Index;
            True(redirectedInstances.Length >= 2 &&
                 redirectedInstances.Any(instance =>
                     instance.StaticObjectIndex == redirectedCopyRootIndex),
                "existing and generated placements resolve through the new mesh resource");

            SmoProject removalProject =
                SmoProjectArchive.Load(projectPath);
            SmoSharedMeshInstanceInfo[] sharedSourceInstances =
                SmoSharedMeshInstanceResolver.ResolveAll(source)
                    .Where(instance => instance.SourceMeshObjectId == sharedSourceMeshId)
                    .ToArray();
            uint[] groupedRemovalIds = sharedSourceInstances
                .Select(instance => source.Objects[instance.StaticObjectIndex].Id)
                .Distinct()
                .ToArray();
            var removalSession = new SmoProjectSession(removalProject);
            True(groupedRemovalIds.Length > 1 && removalSession.Execute(
                    "remove complete placement group",
                    current => current.RemovePlacements(groupedRemovalIds)),
                "complete replacement can prune all old placements as one operation");
            string removalOutputPath = Path.Combine(directory, "external-group-removal.smo");
            _ = SmoProjectSerializer.Build(removalProject, removalOutputPath);
            SmoDocument groupRemoved = SmoDocument.Load(removalOutputPath);
            True(groupedRemovalIds.All(id =>
                     groupRemoved.Objects.All(entry => entry.Id != id)) &&
                 addition.MeshObjectIds.All(id =>
                     groupRemoved.Objects.Any(entry => entry.Id == id)),
                "group removal keeps the newly imported resource graph and omits old roots");

            SmoProject assetRemovalProject =
                SmoProjectArchive.Load(projectPath);
            var assetRemovalSession = new SmoProjectSession(
                assetRemovalProject);
            True(assetRemovalSession.Execute(
                    "remove project-owned model roots",
                    current => current.RemoveSceneBranches(
                        addition.PlacementRootObjectIds.ToArray())) &&
                 assetRemovalProject.Manifest.RemovedAddedForestIds.Count > 0 &&
                 assetRemovalSession.Undo() &&
                 assetRemovalProject.Manifest.RemovedAddedForestIds.Count == 0 &&
                 assetRemovalSession.Redo(),
                "asset-owned roots use reversible forest tombstones");
            string assetRemovalOutputPath = Path.Combine(
                directory,
                "external-asset-removal.smo");
            _ = SmoProjectSerializer.Build(
                assetRemovalProject,
                assetRemovalOutputPath);
            SmoDocument assetRemoved = SmoDocument.Load(assetRemovalOutputPath);
            True(addition.MeshObjectIds.All(id =>
                     assetRemoved.Objects.All(entry => entry.Id != id)) &&
                 addition.PlacementRootObjectIds.All(id =>
                     assetRemoved.Objects.All(entry => entry.Id != id)),
                "tombstoned model assets are absent from the final SMO");
            True(session.Undo() && project.Manifest.ReferencePlacements.Count == 0 &&
                 session.Undo() && project.Manifest.AddedForests.Count == 0 &&
                 session.Redo() && project.Manifest.AddedForests.Count == assetCount &&
                 session.Redo() && project.Manifest.ReferencePlacements.Count == 1,
                "external import and later placement retain independent transactional history");
            if (retainedOutputDirectory is not null)
            {
                Console.WriteLine($"PROJECT={projectPath}");
                Console.WriteLine($"SMO={outputPath}");
                Console.WriteLine($"COPY_PROJECT={copyProjectPath}");
                Console.WriteLine($"COPY_SMO={copyOutputPath}");
                Console.WriteLine($"MOVED_PROJECT={movedProjectPath}");
                Console.WriteLine($"MOVED_SMO={movedOutputPath}");
                Console.WriteLine($"REDIRECT_PROJECT={redirectProjectPath}");
                Console.WriteLine($"REDIRECT_SMO={redirectOutputPath}");
            }
        }
        finally
        {
            if (retainedOutputDirectory is null)
                Directory.Delete(directory, recursive: true);
        }
    }

    private static byte[] BuildStaticPlacementSample(Matrix4x4 world)
    {
        if (!Matrix4x4.Invert(world, out _))
            throw new InvalidOperationException("Synthetic placement is singular.");
        Matrix4x4 inverse =
            SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(world);
        byte[] forwardField = SmoDataBlockWriter.BuildField(
            1,
            SmoPropertyValueCodec.Encode(world));
        byte[] inverseField = SmoDataBlockWriter.BuildField(
            2,
            SmoPropertyValueCodec.Encode(inverse));
        byte[] terminator = SmoDataBlockWriter.BuildField(0, []);
        byte[] data = new byte[checked(
            8 + forwardField.Length + inverseField.Length + terminator.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(
            data,
            SmoClassIds.StaticRenderObject);
        "SBOO"u8.CopyTo(data.AsSpan(4));
        int cursor = 8;
        forwardField.CopyTo(data, cursor);
        cursor += forwardField.Length;
        inverseField.CopyTo(data, cursor);
        cursor += inverseField.Length;
        terminator.CopyTo(data, cursor);

        byte[] name = "placement\0"u8.ToArray();
        int tableSize = checked(18 + name.Length);
        int dataStart = checked(SmoHeader.Size + tableSize + sizeof(uint));
        byte[] result = new byte[checked(dataStart + data.Length)];
        "FFPS"u8.CopyTo(result);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x04), 1);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x08), 0);
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x0C),
            checked((uint)result.Length));
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x10), 2);
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x14),
            checked((uint)dataStart));
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x18),
            checked((uint)data.Length));
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x1C), 1);

        cursor = SmoHeader.Size;
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(cursor), 1);
        cursor += sizeof(uint);
        BinaryPrimitives.WriteUInt16LittleEndian(
            result.AsSpan(cursor),
            checked((ushort)name.Length));
        cursor += sizeof(ushort);
        name.CopyTo(result, cursor);
        cursor += name.Length;
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(cursor),
            SmoClassIds.StaticRenderObject);
        cursor += sizeof(uint);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(cursor), 0);
        cursor += sizeof(uint);
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(cursor),
            checked((uint)data.Length));
        data.CopyTo(result, dataStart);
        return result;
    }

    private static byte[] BuildRelocatableResourceSample()
    {
        const uint ownerType = 0xA0010001;
        const uint resourceType = 0xA0010002;
        const uint sourceOwnerId = 1;
        const uint resourceId = 2;
        const uint targetOwnerId = 3;

        byte[] terminal = SmoDataBlockWriter.BuildField(0, []);
        byte[] resource = new byte[checked(8 + terminal.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(resource, resourceType);
        "SBOO"u8.CopyTo(resource.AsSpan(4));
        terminal.CopyTo(resource, 8);

        byte[] inlinePayload = new byte[checked(8 + resource.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(inlinePayload, resourceId);
        BinaryPrimitives.WriteUInt32LittleEndian(
            inlinePayload.AsSpan(4),
            checked((uint)resource.Length));
        resource.CopyTo(inlinePayload, 8);
        byte[] inlineField = SmoDataBlockWriter.BuildField(7, inlinePayload);
        byte[] sourceOwner = new byte[checked(8 + inlineField.Length + terminal.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(sourceOwner, ownerType);
        "SBOO"u8.CopyTo(sourceOwner.AsSpan(4));
        inlineField.CopyTo(sourceOwner, 8);
        terminal.CopyTo(sourceOwner, 8 + inlineField.Length);

        byte[] referencePayload = new byte[8];
        BinaryPrimitives.WriteUInt32LittleEndian(referencePayload, resourceId);
        byte[] referenceField = SmoDataBlockWriter.BuildField(7, referencePayload);
        byte[] targetOwner = new byte[checked(8 + referenceField.Length + terminal.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(targetOwner, ownerType);
        "SBOO"u8.CopyTo(targetOwner.AsSpan(4));
        referenceField.CopyTo(targetOwner, 8);
        terminal.CopyTo(targetOwner, 8 + referenceField.Length);

        int inlineHeaderSize = inlineField.Length - inlinePayload.Length;
        uint resourceOffset = checked((uint)(8 + inlineHeaderSize + 8));
        byte[] sourceName = "source-owner\0"u8.ToArray();
        byte[] resourceName = "shared-resource\0"u8.ToArray();
        byte[] targetName = "target-owner\0"u8.ToArray();
        (uint Id, byte[] Name, uint Type, uint Offset, uint Size)[] entries =
        [
            (sourceOwnerId, sourceName, ownerType, 0, checked((uint)sourceOwner.Length)),
            (resourceId, resourceName, resourceType, resourceOffset,
                checked((uint)resource.Length)),
            (targetOwnerId, targetName, ownerType, checked((uint)sourceOwner.Length),
                checked((uint)targetOwner.Length))
        ];
        int tableSize = entries.Sum(entry => 18 + entry.Name.Length);
        int dataStart = checked(SmoHeader.Size + tableSize + sizeof(uint));
        int dataLength = checked(sourceOwner.Length + targetOwner.Length);
        byte[] result = new byte[checked(dataStart + dataLength)];
        "FFPS"u8.CopyTo(result);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x04), 1);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x08), 0);
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x0C),
            checked((uint)result.Length));
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x10), 2);
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x14),
            checked((uint)dataStart));
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x18),
            checked((uint)dataLength));
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x1C), 3);

        int cursor = SmoHeader.Size;
        foreach ((uint id, byte[] name, uint type, uint offset, uint size) in entries)
        {
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(cursor), id);
            BinaryPrimitives.WriteUInt16LittleEndian(
                result.AsSpan(cursor + 4),
                checked((ushort)name.Length));
            name.CopyTo(result, cursor + 6);
            int suffix = cursor + 6 + name.Length;
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(suffix), type);
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(suffix + 4), offset);
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(suffix + 8), size);
            cursor += 18 + name.Length;
        }
        sourceOwner.CopyTo(result, dataStart);
        targetOwner.CopyTo(result, dataStart + sourceOwner.Length);
        return result;
    }

    private static byte[] BuildReferencePlacementSample(Matrix4x4 world)
    {
        if (!Matrix4x4.Invert(world, out _))
            throw new InvalidOperationException("Synthetic placement is singular.");
        Matrix4x4 inverse =
            SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(world);
        const uint ownerType = 0xA0020001;
        const uint ownerId = 1;
        const uint meshId = 2;
        const uint placementId = 3;
        byte[] terminal = SmoDataBlockWriter.BuildField(0, []);

        byte[] mesh = new byte[checked(8 + terminal.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(mesh, SmoClassIds.MeshData);
        "SBOO"u8.CopyTo(mesh.AsSpan(4));
        terminal.CopyTo(mesh, 8);

        byte[] meshReference = new byte[8];
        BinaryPrimitives.WriteUInt32LittleEndian(meshReference, meshId);
        byte[] placementFields =
        [
            .. SmoDataBlockWriter.BuildField(
                1,
                SmoPropertyValueCodec.Encode(world)),
            .. SmoDataBlockWriter.BuildField(
                2,
                SmoPropertyValueCodec.Encode(inverse)),
            .. SmoDataBlockWriter.BuildField(7, meshReference),
            .. terminal
        ];
        byte[] placement = new byte[checked(8 + placementFields.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(
            placement,
            SmoClassIds.StaticRenderObject);
        "SBOO"u8.CopyTo(placement.AsSpan(4));
        placementFields.CopyTo(placement, 8);

        byte[] meshPayload = new byte[checked(8 + mesh.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(meshPayload, meshId);
        BinaryPrimitives.WriteUInt32LittleEndian(
            meshPayload.AsSpan(4),
            checked((uint)mesh.Length));
        mesh.CopyTo(meshPayload, 8);
        byte[] meshField = SmoDataBlockWriter.BuildField(5, meshPayload);
        byte[] placementPayload = new byte[checked(8 + placement.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(placementPayload, placementId);
        BinaryPrimitives.WriteUInt32LittleEndian(
            placementPayload.AsSpan(4),
            checked((uint)placement.Length));
        placement.CopyTo(placementPayload, 8);
        byte[] placementField = SmoDataBlockWriter.BuildField(6, placementPayload);

        byte[] owner = new byte[checked(
            8 + meshField.Length + placementField.Length + terminal.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(owner, ownerType);
        "SBOO"u8.CopyTo(owner.AsSpan(4));
        int ownerCursor = 8;
        meshField.CopyTo(owner, ownerCursor);
        ownerCursor += meshField.Length;
        placementField.CopyTo(owner, ownerCursor);
        ownerCursor += placementField.Length;
        terminal.CopyTo(owner, ownerCursor);

        int meshHeaderSize = meshField.Length - meshPayload.Length;
        int placementHeaderSize = placementField.Length - placementPayload.Length;
        uint meshOffset = checked((uint)(8 + meshHeaderSize + 8));
        uint placementOffset = checked((uint)(
            8 + meshField.Length + placementHeaderSize + 8));
        (uint Id, byte[] Name, uint Type, uint Offset, uint Size)[] entries =
        [
            (ownerId, "owner\0"u8.ToArray(), ownerType, 0, checked((uint)owner.Length)),
            (meshId, "mesh\0"u8.ToArray(), SmoClassIds.MeshData,
                meshOffset, checked((uint)mesh.Length)),
            (placementId, "placement\0"u8.ToArray(),
                SmoClassIds.StaticRenderObject,
                placementOffset, checked((uint)placement.Length))
        ];
        int tableSize = entries.Sum(entry => 18 + entry.Name.Length);
        int dataStart = checked(SmoHeader.Size + tableSize + sizeof(uint));
        byte[] result = new byte[checked(dataStart + owner.Length)];
        "FFPS"u8.CopyTo(result);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x04), 1);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x08), 0);
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x0C),
            checked((uint)result.Length));
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x10), 2);
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x14),
            checked((uint)dataStart));
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x18),
            checked((uint)owner.Length));
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x1C), 3);

        int cursor = SmoHeader.Size;
        foreach ((uint id, byte[] name, uint type, uint offset, uint size) in entries)
        {
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(cursor), id);
            BinaryPrimitives.WriteUInt16LittleEndian(
                result.AsSpan(cursor + 4),
                checked((ushort)name.Length));
            name.CopyTo(result, cursor + 6);
            int suffix = cursor + 6 + name.Length;
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(suffix), type);
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(suffix + 4), offset);
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(suffix + 8), size);
            cursor += 18 + name.Length;
        }
        owner.CopyTo(result, dataStart);
        return result;
    }

    private static byte[] BuildPhysicalReferencePlacementSample(
        Matrix4x4 world,
        bool useStaticPlacement = true,
        bool includeReferenceConsumer = false,
        bool duplicateReferenceConsumerField = false)
    {
        if (!Matrix4x4.Invert(world, out _))
            throw new InvalidOperationException("Synthetic placement is singular.");
        Matrix4x4 inverse =
            SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(world);
        const uint ownerType = 0xA0021001;
        const uint ownerId = 1;
        const uint placementId = 2;
        const uint meshId = 3;
        uint placementType = useStaticPlacement
            ? SmoClassIds.StaticRenderObject
            : SmoClassIds.PartitionRenderable;
        byte[] terminal = SmoDataBlockWriter.BuildField(0, []);

        byte[] mesh = new byte[checked(8 + terminal.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(mesh, SmoClassIds.MeshData);
        "SBOO"u8.CopyTo(mesh.AsSpan(4));
        terminal.CopyTo(mesh, 8);
        byte[] meshPayload = new byte[checked(8 + mesh.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(meshPayload, meshId);
        BinaryPrimitives.WriteUInt32LittleEndian(
            meshPayload.AsSpan(4),
            checked((uint)mesh.Length));
        mesh.CopyTo(meshPayload, 8);
        byte[] meshField = SmoDataBlockWriter.BuildField(7, meshPayload);

        byte[] placementFields =
        [
            .. SmoDataBlockWriter.BuildField(
                1,
                SmoPropertyValueCodec.Encode(world)),
            .. SmoDataBlockWriter.BuildField(
                2,
                SmoPropertyValueCodec.Encode(inverse)),
            .. meshField,
            .. terminal
        ];
        byte[] placement = new byte[checked(8 + placementFields.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(
            placement,
            placementType);
        "SBOO"u8.CopyTo(placement.AsSpan(4));
        placementFields.CopyTo(placement, 8);

        byte[] placementPayload = new byte[checked(8 + placement.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(placementPayload, placementId);
        BinaryPrimitives.WriteUInt32LittleEndian(
            placementPayload.AsSpan(4),
            checked((uint)placement.Length));
        placement.CopyTo(placementPayload, 8);
        byte[] placementField = SmoDataBlockWriter.BuildField(6, placementPayload);
        byte[] owner = new byte[checked(8 + placementField.Length + terminal.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(owner, ownerType);
        "SBOO"u8.CopyTo(owner.AsSpan(4));
        placementField.CopyTo(owner, 8);
        terminal.CopyTo(owner, 8 + placementField.Length);

        const uint referenceConsumerId = 4;
        byte[] referenceConsumer = [];
        if (includeReferenceConsumer)
        {
            byte[] meshReference = new byte[8];
            BinaryPrimitives.WriteUInt32LittleEndian(meshReference, meshId);
            byte[] referenceFields =
            [
                .. SmoDataBlockWriter.BuildField(
                    1,
                    SmoPropertyValueCodec.Encode(world)),
                .. SmoDataBlockWriter.BuildField(
                    2,
                    SmoPropertyValueCodec.Encode(inverse)),
                .. SmoDataBlockWriter.BuildField(7, meshReference),
                .. (duplicateReferenceConsumerField
                    ? SmoDataBlockWriter.BuildField(7, meshReference)
                    : []),
                .. terminal
            ];
            referenceConsumer = new byte[checked(8 + referenceFields.Length)];
            BinaryPrimitives.WriteUInt32LittleEndian(
                referenceConsumer,
                SmoClassIds.StaticRenderObject);
            "SBOO"u8.CopyTo(referenceConsumer.AsSpan(4));
            referenceFields.CopyTo(referenceConsumer, 8);
        }

        int placementHeaderSize = placementField.Length - placementPayload.Length;
        int meshHeaderSize = meshField.Length - meshPayload.Length;
        uint placementOffset = checked((uint)(8 + placementHeaderSize + 8));
        uint meshOffset = checked((uint)(
            placementOffset + 8 +
            SmoDataBlockWriter.BuildField(
                1,
                SmoPropertyValueCodec.Encode(world)).Length +
            SmoDataBlockWriter.BuildField(
                2,
                SmoPropertyValueCodec.Encode(inverse)).Length +
            meshHeaderSize + 8));
        var entries = new List<(uint Id, byte[] Name, uint Type, uint Offset, uint Size)>
        {
            (ownerId, "owner\0"u8.ToArray(), ownerType, 0,
                checked((uint)owner.Length)),
            (placementId, "physical-placement\0"u8.ToArray(),
                placementType,
                placementOffset,
                checked((uint)placement.Length)),
            (meshId, "physical-mesh\0"u8.ToArray(),
                SmoClassIds.MeshData,
                meshOffset,
                checked((uint)mesh.Length))
        };
        if (includeReferenceConsumer)
        {
            entries.Add((
                referenceConsumerId,
                "reference-consumer\0"u8.ToArray(),
                SmoClassIds.StaticRenderObject,
                checked((uint)owner.Length),
                checked((uint)referenceConsumer.Length)));
        }
        int tableSize = entries.Sum(entry => 18 + entry.Name.Length);
        int dataStart = checked(SmoHeader.Size + tableSize + sizeof(uint));
        int dataLength = checked(owner.Length + referenceConsumer.Length);
        byte[] result = new byte[checked(dataStart + dataLength)];
        "FFPS"u8.CopyTo(result);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x04), 1);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x08), 0);
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x0C),
            checked((uint)result.Length));
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(0x10), 2);
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x14),
            checked((uint)dataStart));
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x18),
            checked((uint)dataLength));
        BinaryPrimitives.WriteUInt32LittleEndian(
            result.AsSpan(0x1C),
            checked((uint)entries.Count));

        int cursor = SmoHeader.Size;
        foreach ((uint id, byte[] name, uint type, uint offset, uint size) in entries)
        {
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(cursor), id);
            BinaryPrimitives.WriteUInt16LittleEndian(
                result.AsSpan(cursor + 4),
                checked((ushort)name.Length));
            name.CopyTo(result, cursor + 6);
            int suffix = cursor + 6 + name.Length;
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(suffix), type);
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(suffix + 4), offset);
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(suffix + 8), size);
            cursor += 18 + name.Length;
        }
        owner.CopyTo(result, dataStart);
        referenceConsumer.CopyTo(result, dataStart + owner.Length);
        return result;
    }

    private static Matrix4x4 ReadMatrix(ReadOnlySpan<byte> data)
    {
        if (data.Length != 64)
            throw new ArgumentException("A serialized matrix must contain 64 bytes.");
        Span<float> cells = stackalloc float[16];
        for (int index = 0; index < cells.Length; index++)
        {
            cells[index] = BitConverter.Int32BitsToSingle(
                BinaryPrimitives.ReadInt32LittleEndian(data[(index * 4)..]));
        }
        return new Matrix4x4(
            cells[0], cells[1], cells[2], cells[3],
            cells[4], cells[5], cells[6], cells[7],
            cells[8], cells[9], cells[10], cells[11],
            cells[12], cells[13], cells[14], cells[15]);
    }

    private static bool TryReadSyntheticStaticWorld(
        SmoDocument document,
        SmoObjectEntry entry,
        out Matrix4x4 world)
    {
        world = Matrix4x4.Identity;
        if (entry.TypeHash != SmoClassIds.StaticRenderObject ||
            !SmoObjectFieldReader.TryRead(document, entry, out var fields, out _))
        {
            return false;
        }
        SmoObjectField? transform = fields.SingleOrDefault(field =>
            field.FieldType == 1 && field.PayloadSize == 64);
        if (transform is null)
            return false;
        world = ReadMatrix(transform.Payload.Span);
        return true;
    }

    private static void ValidateImportContract()
    {
        var mesh = new ImportedMesh(
            "triangle",
            [Vector3.Zero, Vector3.UnitX, Vector3.UnitY],
            [],
            [],
            [0u, 1u, 2u]);
        var valid = new ImportedScene([mesh]);
        SmoLevelImportValidationReport report = SmoLevelImportValidator.Validate(
            valid,
            SmoLevelImportPurpose.AddExternalModel);
        True(report.CanImport,
            "rigid triangle satisfies the shared level import contract");

        report = SmoLevelImportValidator.Validate(
            valid,
            SmoLevelImportPurpose.ReplaceModelResource,
            expectedMeshCount: 2);
        True(!report.CanImport && report.Errors.Any(error =>
                error.Contains("2", StringComparison.Ordinal)),
            "resource replacement rejects a mismatched mesh-part count");

        var invalidIndices = valid with
        {
            Meshes =
            [
                mesh with { TriangleIndices = [0u, 1u, 7u] }
            ]
        };
        report = SmoLevelImportValidator.Validate(
            invalidIndices,
            SmoLevelImportPurpose.AddExternalModel);
        True(!report.CanImport,
            "level import rejects triangle indices outside the vertex buffer");

        var masked = new ImportedScene(
            [mesh with { MaterialIndex = 0 }],
            [new ImportedTexture("alpha", "image/png", 1, 1, [1])],
            [new ImportedMaterial(
                "masked",
                "alpha",
                0,
                ImportedMaterialAlphaMode.Mask)])
        {
            ImportWarnings = ["decoder repair"]
        };
        report = SmoLevelImportValidator.Validate(
            masked,
            SmoLevelImportPurpose.AddExternalModel);
        True(report.CanImport && report.Warnings.Count >= 2,
            "level import preserves decoder warnings and reports MASK conversion");
    }

    private static void ValidateObjAdjacentTextureImport()
    {
        string root = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-obj-textures-{Guid.NewGuid():N}");
        Directory.CreateDirectory(root);
        try
        {
            string mtlDirectory = Path.Combine(root, "with-mtl");
            Directory.CreateDirectory(mtlDirectory);
            string mtlObj = Path.Combine(mtlDirectory, "model.obj");
            File.WriteAllText(
                mtlObj,
                TriangleObj("mtllib \"library with spaces.mtl\"", "usemtl body_mat"));
            File.WriteAllText(
                Path.Combine(mtlDirectory, "library with spaces.mtl"),
                "newmtl body_mat\nmap_Kd -s 1 1 1 \"body texture.png\"\n");
            WriteTexture(
                Path.Combine(mtlDirectory, "body texture.png"),
                transparent: true);
            ImportedScene withMtl = ObjModelReader.Read(mtlObj);
            True(withMtl.Textures.Count == 1 &&
                 withMtl.Materials.Count == 1 &&
                 withMtl.Meshes[0].MaterialIndex == 0 &&
                 withMtl.Materials[0].BaseColorTextureIndex == 0,
                "OBJ imports an adjacent MTL base-color texture without GUI input");
            True(withMtl.Materials[0].AlphaMode == ImportedMaterialAlphaMode.Blend,
                "OBJ adjacent PNG alpha selects the alpha material path");

            string namedDirectory = Path.Combine(root, "without-mtl-named");
            Directory.CreateDirectory(namedDirectory);
            string namedObj = Path.Combine(namedDirectory, "named.obj");
            File.WriteAllText(namedObj, TriangleObj(null, "usemtl skin"));
            WriteTexture(Path.Combine(namedDirectory, "skin.png"), transparent: false);
            ImportedScene named = ObjModelReader.Read(namedObj);
            True(named.Textures.Count == 1 &&
                 named.Materials[named.Meshes[0].MaterialIndex].BaseColorTextureIndex == 0,
                "OBJ without MTL matches an adjacent texture by material name");

            string fallbackDirectory = Path.Combine(root, "without-mtl-fallback");
            Directory.CreateDirectory(fallbackDirectory);
            string fallbackObj = Path.Combine(fallbackDirectory, "fallback.obj");
            File.WriteAllText(fallbackObj, TriangleObj(null, null));
            WriteTexture(Path.Combine(fallbackDirectory, "only-image.png"), transparent: false);
            WriteTexture(
                Path.Combine(fallbackDirectory, "only-image_NOMR.png"),
                transparent: false);
            ImportedScene fallback = ObjModelReader.Read(fallbackObj);
            True(fallback.Textures.Count == 1 &&
                 fallback.Meshes[0].MaterialIndex >= 0 &&
                 fallback.ImportWarnings.Any(warning => warning.Contains(
                     "fallback",
                     StringComparison.OrdinalIgnoreCase)),
                "OBJ without MTL uses the sole adjacent texture as an unambiguous fallback");

            string ambiguousDirectory = Path.Combine(root, "ambiguous");
            Directory.CreateDirectory(ambiguousDirectory);
            string ambiguousObj = Path.Combine(ambiguousDirectory, "ambiguous.obj");
            File.WriteAllText(ambiguousObj, TriangleObj(null, null));
            WriteTexture(Path.Combine(ambiguousDirectory, "first.png"), transparent: false);
            WriteTexture(Path.Combine(ambiguousDirectory, "second.png"), transparent: false);
            ImportedScene ambiguous = ObjModelReader.Read(ambiguousObj);
            True(ambiguous.Textures.Count == 0 &&
                 ambiguous.Meshes[0].MaterialIndex == -1 &&
                 ambiguous.ImportWarnings.Count > 0,
                "OBJ adjacent texture discovery does not guess between ambiguous images");
        }
        finally
        {
            Directory.Delete(root, recursive: true);
        }

        static string TriangleObj(string? library, string? material)
        {
            string[] lines =
            [
                library ?? string.Empty,
                "o sample",
                "v 0 0 0",
                "v 1 0 0",
                "v 0 1 0",
                "vt 0 0",
                "vt 1 0",
                "vt 0 1",
                material ?? string.Empty,
                "f 1/1 2/2 3/3"
            ];
            return string.Join('\n', lines);
        }

        static void WriteTexture(string path, bool transparent)
        {
            using var image = new Image<Rgba32>(2, 2, new Rgba32(80, 140, 220, 255));
            if (transparent)
                image[0, 0] = new Rgba32(80, 140, 220, 0);
            image.SaveAsPng(path);
        }
    }

    private static void ValidateCollisionSave(string path)
    {
        True(File.Exists(path), $"collision fixture exists: {path}");
        SmoLevelWorkspace workspace = SmoLevelWorkspace.Load(path);
        True(workspace.Collisions.Count > 0, "workspace exposes collision meshes");
        True(workspace.Textures.Count > 0,
            "real level exposes decoded textures through the workspace catalog");
        var document = new SmoLevelDocument(workspace);
        True(document.Collisions.Count == workspace.Collisions.Count,
            "every collision mesh has editable state");
        True(document.Entities.Count(entity =>
                entity.Kind == SmoLevelEntityKind.Collision) == workspace.Collisions.Count,
            "every spCollisionInfo is an independent editor entity");

        if (Path.GetFileName(path).Equals(
                "Alfea02_old.smo",
                StringComparison.OrdinalIgnoreCase))
        {
            ValidateAlfeaTableCollisionLink(document, path);
            ValidateLinkedAffineSave(path, rotate: true);
            ValidateLinkedAffineSave(path, rotate: false);
            ValidateMultiSelectionExport(path);
            ValidateAlfeaCompositeModels(document);
            ValidateTextureReplacement(document);
            ValidateModelReplacement(document);
            ValidateCompoundModelReplacement(document.Workspace.Document);
            ValidateSharedPlacementClone(document.Workspace.Document);
            ValidateExternalModelAppend(document.Workspace.Document);
            ValidateCompositeResourceReplacement(path);
            ValidateNodeOwnedPlacementTransform(path);
            ValidatePlacementRemoval(document.Workspace.Document);
            ValidateGeneratedCollisionSave(path);
            ValidateCollisionRemoval(path);
        }
        if (Path.GetFileName(path).Equals(
                "Alfea02.smo",
                StringComparison.OrdinalIgnoreCase))
        {
            ValidateAlfeaCompositeModels(document);
            ValidateTextureReplacement(document);
            ValidateModelReplacement(document);
            ValidateCompoundModelReplacement(document.Workspace.Document);
            ValidateSharedPlacementClone(document.Workspace.Document);
        }
        if (Path.GetFileName(path).Equals(
                "Domino01.smo",
                StringComparison.OrdinalIgnoreCase))
        {
            ValidateDominoCompositeModels(document);
        }

        SmoEditableCollision collision = document.Collisions[0];
        Matrix4x4 desired = collision.WorldTransform;
        Vector3 delta = new(23.5f, -6.25f, 11.75f);
        desired.M41 += delta.X;
        desired.M42 += delta.Y;
        desired.M43 += delta.Z;
        True(document.SetEntityTransform(
                collision.Entity.Id,
                desired,
                "Move collision integration test"),
            "collision entity accepts a transform command");

        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-collision-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, Path.GetFileName(path));
            SmoLevelSaveResult result = SmoLevelSaveService.Save(document, output);
            True(result.PatchedTransformCount == 1,
                "collision save reports one patched transform");
            SmoLevelWorkspace saved = SmoLevelWorkspace.Load(output);
            SmoCollisionMesh moved = saved.Collisions.Single(candidate =>
                candidate.CollisionInfoObjectIndex ==
                collision.Source.CollisionInfoObjectIndex);
            Vector3 beforeWorld = Vector3.Transform(
                collision.Source.Positions[0],
                collision.OriginalWorldTransform);
            Vector3 afterWorld = Vector3.Transform(
                moved.Positions[0],
                moved.WorldTransform);
            True(Vector3.Distance(afterWorld, beforeWorld + delta) < 0.001f,
                "saved collision vertex moved by the requested world delta");
            True(new FileInfo(output).Length == new FileInfo(path).Length,
                "collision save preserves container byte length");
            string log = File.ReadAllText(result.LogPath);
            True(log.Contains("kind=Collision", StringComparison.Ordinal),
                "save log identifies collision entity edits");
            True(log.Contains("collisionMeshes=", StringComparison.Ordinal),
                "save log records patched collision mesh indices");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateCollisionHullGenerator()
    {
        Vector3[] source =
        [
            new(-4, -1, -2), new(4, -1, -2), new(4, 1, -2), new(-4, 1, -2),
            new(-4, -1, 2), new(4, -1, 2), new(4, 1, 2), new(-4, 1, 2),
            new(0, 2.5f, 0), new(0, -2.5f, 0)
        ];
        SmoGeneratedCollisionMesh collision = SmoCollisionHullGenerator.Generate(
            source,
            triangleBudget: 48);
        True(collision.TriangleCount <= 48,
            "generated collision respects its hard triangle budget");
        True(collision.Positions.Count > 8,
            "generated collision is a shaped hull rather than an axis-aligned box");
        True(collision.TriangleIndices.All(index =>
                (uint)index < (uint)collision.Positions.Count),
            "generated collision indices reference valid vertices");
        SmoGeneratedCollisionMesh padded = SmoCollisionHullGenerator.Generate(
            source,
            triangleBudget: 48,
            padding: 1);
        True(padded.Positions.Max(point => point.X) > 4.9f,
            "explicit collision padding expands the generated hull");
    }

    private static void ValidateGeneratedCollisionSave(string path)
    {
        SmoLevelWorkspace workspace = SmoLevelWorkspace.Load(path);
        var document = new SmoLevelDocument(workspace);
        SmoLevelEntity visual = document.Entities.First(entity =>
            entity.Kind == SmoLevelEntityKind.Visual && entity.Parts.Count > 0);
        Vector3[] sourceWorld = visual.Parts.SelectMany(part =>
        {
            SmoSceneMesh sourceMesh = workspace.PreparedScene.Meshes.First(mesh =>
                mesh.Mesh.ObjectIndex == part.Asset.ObjectIndex &&
                mesh.SceneObjectIndex == part.Source.SceneObjectIndex);
            return sourceMesh.Mesh.Positions.Select(position =>
                Vector3.Transform(position, part.WorldTransform));
        }).ToArray();
        SmoGeneratedCollisionMesh generated = SmoCollisionHullGenerator.Generate(
            sourceWorld,
            triangleBudget: 48);
        SmoLevelEntityId generatedId = document.AddGeneratedCollision(
            "Collision_GeneratedTest",
            generated.Positions,
            generated.TriangleIndices,
            visual.Id);
        True(document.GeneratedCollisions.ContainsKey(generatedId),
            "generated collision enters pending document state");
        True(document.Collisions.Count == workspace.Collisions.Count + 1,
            "generated collision is immediately visible to the editor");
        True(document.Undo(), "generated collision creation can be undone");
        True(document.Collisions.Count == workspace.Collisions.Count,
            "undo removes generated collision preview");
        True(document.Redo(), "generated collision creation can be redone");
        True(document.RemoveGeneratedCollision(generatedId) &&
             !document.GeneratedCollisions.ContainsKey(generatedId),
            "generated collision can be deleted before serialization");
        True(document.Undo() && document.GeneratedCollisions.ContainsKey(generatedId),
            "generated collision deletion can be undone");

        SmoGeneratedCollisionMesh regenerated = SmoCollisionHullGenerator.Generate(
            sourceWorld,
            triangleBudget: 32,
            padding: 0.25f);
        True(document.ReplaceGeneratedCollision(
                generatedId,
                regenerated.Positions,
                regenerated.TriangleIndices),
            "generated collision can be regenerated in place");
        True(document.GeneratedCollisions[generatedId].TriangleIndices.Count ==
             regenerated.TriangleIndices.Count,
            "regeneration keeps the entity ID and replaces geometry");
        True(document.Undo(), "collision regeneration can be undone");
        True(document.Redo(), "collision regeneration can be redone");

        True(document.GetCollisionLinks(visual.Id).Any(link =>
                link.CollisionEntityId == generatedId),
            "generated collision records its source visual link");
        True(document.RemoveCollisionLink(visual.Id, generatedId),
            "manual visual/collision link can be removed");
        True(document.Undo() && document.GetCollisionLinks(visual.Id).Any(link =>
                link.CollisionEntityId == generatedId),
            "manual unlink can be undone");

        var placementDelta = new Vector3(4.5f, -1.25f, 7.75f);
        using (SmoTransformSession transform = document.BeginTransform(
                   document.ExpandLinkedEntities([visual.Id])))
        {
            transform.PreviewTranslation(placementDelta);
            True(transform.Commit("Move generated hull with source visual"),
                "linked visual and generated hull move as one editor command");
        }
        True(Vector3.Distance(
                Translation(document.GetEntity(generatedId).WorldTransform),
                placementDelta) < 0.0001f,
            "generated hull receives the exact linked placement delta");

        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-generated-collision-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, Path.GetFileName(path));
            SmoLevelSaveResult result = SmoLevelSaveService.Save(document, output);
            SmoLevelWorkspace saved = SmoLevelWorkspace.Load(output);
            True(saved.Collisions.Count == workspace.Collisions.Count + 1,
                "saved level decodes one additional native collision branch");
            SmoObjectEntry savedCollision = saved.Document.Objects.Single(entry =>
                entry.Name.Equals(
                    "Collision_GeneratedTest",
                    StringComparison.Ordinal));
            SmoCollisionMesh savedGenerated = SmoCollisionMeshDecoder.DecodeAll(
                    saved.Document)
                .Single(collision =>
                    collision.CollisionInfoObjectIndex == savedCollision.Index);
            Vector3[] savedGeneratedWorld = savedGenerated.Positions
                .Select(position => Vector3.Transform(
                    position,
                    savedGenerated.WorldTransform))
                .ToArray();
            Vector3[] expectedGeneratedWorld = regenerated.Positions
                .Select(position => position + placementDelta)
                .ToArray();
            True(savedGenerated.TriangleIndices.SequenceEqual(
                    regenerated.TriangleIndices) &&
                 savedGeneratedWorld.Zip(expectedGeneratedWorld).All(pair =>
                    Vector3.Distance(pair.First, pair.Second) < 0.001f),
                "saved generated hull preserves exact triangle order and linked placement");
            (Vector3 SourceMin, Vector3 SourceMax) = CalculateBounds(
                sourceWorld.Select(position => position + placementDelta));
            (Vector3 HullMin, Vector3 HullMax) = CalculateBounds(savedGeneratedWorld);
            True(HullMin.X <= SourceMin.X && HullMin.Y <= SourceMin.Y &&
                 HullMin.Z <= SourceMin.Z && HullMax.X >= SourceMax.X &&
                 HullMax.Y >= SourceMax.Y && HullMax.Z >= SourceMax.Z,
                "rebuilt collision hull still encloses the moved visual geometry");
            var reopenedDocument = new SmoLevelDocument(saved);
            True(MatrixDistance(
                    reopenedDocument.GetEntity(visual.Id).WorldTransform,
                    visual.WorldTransform) < 0.002f,
                "reopened visual keeps the placement used by its generated hull");
            True(DecodeCollisionInfo(saved.Document, savedCollision.Index)
                    .CollisionGroup ==
                    SmoCollisionBranchAppender.ProductionDefaultCollisionGroup,
                "direct save serializes generated collision with production Group 2");
            True(CountReferenceFields(saved.Document, savedCollision.Id, 7) == 1,
                "generated collision is registered once in spPartitionSystem");
            SmoObjectEntry savedMesh = saved.Document.Objects.Single(entry =>
                entry.ParentIndex == savedCollision.Index &&
                entry.TypeHash == SmoClassIds.MeshBoundingVolume);
            ReadOnlySpan<byte> savedMeshBytes = saved.Document.Data.Span.Slice(
                checked((int)savedMesh.PhysicalOffset),
                checked((int)savedMesh.SerializedSize));
            True(savedMeshBytes[^1] == 0,
                "generated spMeshBV has the native terminal empty field");
            True(DecodeMeshBoundingVolume(saved.Document, savedMesh.Index)
                    .FaceData is null,
                "generated spMeshBV leaves wxFaceData absent instead of inventing surface metadata");
            True(File.ReadAllText(result.LogPath).Contains(
                    "COLLISION_ADD",
                    StringComparison.Ordinal),
                "save log records generated collision serialization");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateCollisionRemoval(string path)
    {
        SmoLevelWorkspace workspace = SmoLevelWorkspace.Load(path);
        var document = new SmoLevelDocument(workspace);
        SmoEditableCollision collision = document.Collisions[0];
        uint collisionId = workspace.Document.Objects[
            collision.Source.CollisionInfoObjectIndex].Id;
        uint meshId = workspace.Document.Objects[
            collision.Source.MeshBoundingVolumeObjectIndex].Id;

        True(document.RemoveEntities([collision.Entity.Id]),
            "native collision enters the shared pending-removal state");
        True(document.RemovedEntityIds.Contains(collision.Entity.Id),
            "native collision is hidden before saving");
        True(document.Undo(), "native collision removal can be undone");
        True(!document.RemovedEntityIds.Contains(collision.Entity.Id),
            "undo restores the native collision");
        True(document.Redo(), "native collision removal can be redone");

        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-remove-collision-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, Path.GetFileName(path));
            SmoLevelSaveResult result = SmoLevelSaveService.Save(document, output);
            SmoLevelWorkspace saved = SmoLevelWorkspace.Load(output);
            True(saved.Collisions.Count == workspace.Collisions.Count - 1,
                "saved level contains one fewer native collision");
            True(saved.Document.Objects.All(entry =>
                    entry.Id != collisionId && entry.Id != meshId),
                "saved level removes spCollisionInfo and spMeshBV objects");
            True(File.ReadAllText(result.LogPath).Contains(
                    "COLLISION_REMOVE",
                    StringComparison.Ordinal),
                "save log records native collision removal");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static int CountReferenceFields(
        SmoDocument document,
        uint targetId,
        int fieldType)
    {
        int count = 0;
        foreach (SmoObjectEntry owner in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.PartitionSystem))
        {
            ReadOnlySpan<byte> bytes = document.Data.Span.Slice(
                checked((int)owner.PhysicalOffset),
                checked((int)owner.SerializedSize));
            int offset = 8;
            while (offset < bytes.Length &&
                   SmoDataBlockReader.TryReadHeader(
                       bytes, offset, out SmoDataBlockHeader field))
            {
                if (field.FieldType == fieldType && field.PayloadSize == 8 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(
                        bytes[field.PayloadOffset..]) == targetId &&
                    BinaryPrimitives.ReadUInt32LittleEndian(
                        bytes[(field.PayloadOffset + 4)..]) == 0)
                {
                    count++;
                }
                offset = checked((int)field.PayloadEnd);
            }
        }
        return count;
    }

    private static SmoCollisionInfoData DecodeCollisionInfo(
        SmoDocument document,
        int objectIndex)
    {
        SmoObjectEntry entry = document.Objects[objectIndex];
        if (!SmoCollisionInfoDecoder.TryDecode(
                document,
                entry,
                out SmoCollisionInfoData? data,
                out string error) ||
            data is null)
        {
            throw new InvalidDataException(
                $"Could not decode spCollisionInfo [{objectIndex}]: {error}");
        }
        return data;
    }

    private static SmoMeshBoundingVolumeData DecodeMeshBoundingVolume(
        SmoDocument document,
        int objectIndex)
    {
        SmoObjectEntry entry = document.Objects[objectIndex];
        if (!SmoMeshBoundingVolumeDecoder.TryDecode(
                document,
                entry,
                out SmoMeshBoundingVolumeData? data,
                out string error) ||
            data is null)
        {
            throw new InvalidDataException(
                $"Could not decode spMeshBV [{objectIndex}]: {error}");
        }
        return data;
    }

    private static byte[]? ReadOptionalFieldPayload(
        SmoDocument document,
        int objectIndex,
        int fieldType)
    {
        SmoObjectField? field = SmoObjectFieldReader.Read(document, document.Objects[objectIndex])
            .FirstOrDefault(candidate =>
                candidate.FieldType == fieldType && candidate.PayloadSize > 0);
        return field?.Payload.ToArray();
    }

    private static bool OptionalBytesEqual(byte[]? left, byte[]? right) =>
        left is null
            ? right is null
            : right is not null && left.AsSpan().SequenceEqual(right);

    private static (Vector3 Minimum, Vector3 Maximum) CalculateBounds(
        IEnumerable<Vector3> positions)
    {
        using IEnumerator<Vector3> iterator = positions.GetEnumerator();
        if (!iterator.MoveNext())
            throw new ArgumentException("Cannot calculate bounds of an empty point set.");
        Vector3 minimum = iterator.Current;
        Vector3 maximum = iterator.Current;
        while (iterator.MoveNext())
        {
            minimum = Vector3.Min(minimum, iterator.Current);
            maximum = Vector3.Max(maximum, iterator.Current);
        }
        return (minimum, maximum);
    }

    private static void ValidateAlfeaCompositeModels(SmoLevelDocument document)
    {
        SmoLevelEntity vase = document.Entities.Single(entity =>
            entity.Name.Equals("vase09-000", StringComparison.OrdinalIgnoreCase));
        True(document.CanPersistTransform(vase.Id),
            "node-owned vase placement resolves to a writable transform owner");
        True(vase.Id.SceneObjectIndex ==
             SmoPlacementTransformWriter.FindNodeTransformOwnerIndex(
                 document.Workspace.Document,
                 vase.Parts[0].Source.SceneObjectIndex),
            "node-owned vase entity is grouped by its spRenderNode owner");

        SmoLevelAsset tecnaBakedPart = document.Workspace.Assets.Single(asset =>
            asset.ObjectIndex == 1678);
        True(
            tecnaBakedPart.DisplayName.Equals(
                "Tcn_BNOSH-000",
                StringComparison.OrdinalIgnoreCase),
            "unnamed mesh 1678 inherits its authored spModel name");
        SmoCompositeModel tecnaBed = document.CompositeModels.Single(model =>
            model.Parts.Select(part => part.Asset.ObjectIndex)
                .Order()
                .SequenceEqual([245, 1678]));
        SmoLevelEntity tecnaBakedEntity = tecnaBed.Entities.Single(entity =>
            entity.Parts.Any(part => part.Asset.ObjectIndex == 1678));
        True(
            tecnaBed.Entities.Count == 2 &&
            tecnaBed.Entities.SelectMany(entity => entity.Parts)
                .Select(part => part.Asset.ObjectIndex)
                .Order()
                .SequenceEqual([245, 1678]),
            "placed Tecna bed top and baked NOSH body form one model");
        True(
            document.ExpandCompositeEntities([tecnaBakedEntity.Id])
                .Select(id => id.SceneObjectIndex).Order()
                .SequenceEqual(tecnaBed.Entities.Select(entity =>
                    entity.Id.SceneObjectIndex).Order()),
            "selecting previously anonymous mesh 1678 selects the whole Tecna bed");

        SmoCompositeModel bed = document.CompositeModels.Single(model =>
            model.Name.Equals("Muza_bed", StringComparison.OrdinalIgnoreCase) &&
            model.ParentObjectIndex == 1662);
        True(
            bed.Parts.Select(part => part.Asset.ObjectIndex)
                .SequenceEqual([1691, 1694, 1697, 1706, 1709]),
            "baked Muza bed combines frame, linen, pillow and roof meshes");
        True(
            document.ExpandCompositeEntities([new SmoLevelEntityId(1706)])
                .Select(id => id.SceneObjectIndex).Order()
                .SequenceEqual([1691, 1694, 1697, 1706, 1709]),
            "selecting mesh 1706 expands to the complete baked Muza bed");

        SmoCompositeModel tree = document.CompositeModels.Single(model =>
            model.Name.Equals("tree01", StringComparison.OrdinalIgnoreCase) &&
            model.ParentObjectIndex == 2090);
        True(
            tree.Entities.Select(entity => entity.Id.SceneObjectIndex)
                .SequenceEqual([2122, 2126, 2130]),
            "Alfea tree01 numbered static objects form one inferred model");
        True(
            tree.Parts.Select(part => part.Asset.ObjectIndex)
                .SequenceEqual([2125, 2129, 2133]),
            "Alfea tree01 composite keeps all three physical meshes");

        IReadOnlyList<SmoLevelEntityId> expanded =
            document.ExpandCompositeEntities([new SmoLevelEntityId(2126)]);
        True(
            expanded.Select(id => id.SceneObjectIndex).Order()
                .SequenceEqual([2122, 2126, 2130]),
            "selecting one tree01 part expands to the complete model");

        Matrix4x4[] groupBefore = expanded
            .Select(id => document.GetEntity(id).WorldTransform)
            .ToArray();
        var groupDelta = new Vector3(3, -2, 5);
        using (SmoTransformSession session = document.BeginTransform(expanded))
        {
            session.PreviewTranslation(groupDelta);
            True(expanded.Select((id, index) => Vector3.Distance(
                        Translation(document.GetEntity(id).WorldTransform),
                        Translation(groupBefore[index]) + groupDelta) < 0.0001f)
                    .All(moved => moved),
                "one composite transform preview moves every tree01 mesh part");
            session.Cancel();
        }

        Matrix4x4 untouched = document.GetEntity(new SmoLevelEntityId(2126))
            .WorldTransform;
        Matrix4x4 moved = document.GetEntity(new SmoLevelEntityId(2122))
            .WorldTransform;
        moved.M41 += 1;
        True(document.SetEntityTransform(
                new SmoLevelEntityId(2122),
                moved,
                "Move raw composite part"),
            "a raw composite entity can still be moved independently");
        True(document.GetEntity(new SmoLevelEntityId(2126)).WorldTransform.Equals(untouched),
            "independent raw-part movement leaves sibling parts unchanged");
        True(document.Undo(), "raw composite-part movement is undoable");
    }

    private static void ValidateDominoCompositeModels(SmoLevelDocument document)
    {
        SmoCompositeModel ground = document.CompositeModels.Single(model =>
            model.Parts.Select(part => part.Asset.ObjectIndex)
                .Order()
                .SequenceEqual([11, 15]));
        True(
            ground.Name.StartsWith(
                "domino_part1_ground",
                StringComparison.OrdinalIgnoreCase),
            "normal and no-shadow Domino ground representations form one model");
    }

    private static void ValidateTextureReplacement(SmoLevelDocument document)
    {
        string repository = Path.GetFullPath(Path.Combine(
            AppContext.BaseDirectory, "..", "..", "..", "..", "..", ".."));
        string imagePath = Path.Combine(
            repository,
            "tools",
            "SmoLVLcreator",
            "ui-preview.png");
        True(File.Exists(imagePath), "texture replacement fixture exists");
        SmoLevelTexture sourceTexture = document.Workspace.Textures[0];
        byte[] sourcePixels = sourceTexture.Bgra32Pixels.ToArray();
        True(document.ReplaceTexture(
                sourceTexture.ObjectIndex,
                File.ReadAllBytes(imagePath),
                imagePath),
            "texture replacement enters editor history");
        True(document.TextureReplacements.ContainsKey(sourceTexture.ObjectIndex),
            "pending texture replacement is exposed to the save pipeline");
        True(document.Undo() && document.TextureReplacements.Count == 0,
            "texture replacement is undoable");
        True(document.Redo() &&
             document.TextureReplacements.ContainsKey(sourceTexture.ObjectIndex),
            "texture replacement is redoable");

        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-texture-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, "Alfea02-texture.smo");
            SmoLevelSaveResult result = SmoLevelSaveService.Save(document, output);
            True(result.ResourceEditCount == 1 &&
                 result.EditedEntityCount == 0 &&
                 result.PatchedTransformCount == 0,
                "save reports one resource edit without fake transform edits");
            SmoLevelWorkspace saved = SmoLevelWorkspace.Load(output);
            SmoLevelTexture replaced = saved.Textures.Single(texture =>
                texture.ObjectIndex == sourceTexture.ObjectIndex);
            True(replaced.Width == sourceTexture.Width &&
                 replaced.Height == sourceTexture.Height,
                "native texture replacement preserves SMO dimensions");
            True(!replaced.Bgra32Pixels.Span.SequenceEqual(sourcePixels),
                "saved texture contains replacement RGB pixels");
            True(File.ReadAllText(result.LogPath).Contains(
                    "RESOURCE_TEXTURE",
                    StringComparison.Ordinal),
                "save log records the texture resource edit");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
        True(document.Undo() && document.TextureReplacements.Count == 0,
            "texture integration fixture restores the level document");
    }

    private static void ValidateModelReplacement(SmoLevelDocument document)
    {
        const int targetMeshIndex = 347;
        SmoObjectEntry oldMeshEntry = document.Workspace.Document.Objects[
            targetMeshIndex];
        SmoSceneMesh sourceMesh = document.Workspace.PreparedScene.Meshes
            .First(mesh => mesh.Mesh.ObjectIndex == targetMeshIndex);
        int placementCount = document.Workspace.PreparedScene.Meshes.Count(mesh =>
            mesh.Mesh.ObjectIndex == targetMeshIndex);
        var importedMesh = new ImportedMesh(
            "level-editor-test",
            [new Vector3(-1, 0, 0), new Vector3(1, 0, 0), new Vector3(0, 2, 0)],
            [Vector3.UnitZ, Vector3.UnitZ, Vector3.UnitZ],
            [new Vector2(0, 0), new Vector2(1, 0), new Vector2(0.5f, 1)],
            [0, 1, 2],
            [0xFFFF0000, 0xFF00FF00, 0xFF0000FF]);
        var importedScene = new ImportedScene([importedMesh]);
        SmoMesh memoryPreview = SmoMeshResourceReplacer.CreatePreviewMesh(
            document.Workspace.Document,
            targetMeshIndex,
            importedScene,
            ReplacementTransform.Identity,
            referenceWorldTransform: sourceMesh.WorldTransform);
        True(memoryPreview.VertexCount == 3 && memoryPreview.TriangleCount == 1,
            "model replacement builds a renderable in-memory mesh without repacking SMO");
        const int transientPreviewIndex = -1_234_567_890;
        SmoMesh isolatedPreview = SmoMeshResourceReplacer.CreatePreviewMesh(
            document.Workspace.Document,
            targetMeshIndex,
            importedScene,
            ReplacementTransform.Identity,
            referenceWorldTransform: sourceMesh.WorldTransform,
            transientObjectIndex: transientPreviewIndex);
        True(isolatedPreview.ObjectIndex == transientPreviewIndex,
            "transient model preview can use an isolated GPU geometry identity");
        var previewAdjustment = new ReplacementTransform(
            1.7f,
            new Vector3(12, -31, 8),
            new Vector3(4, -2, 7));
        Matrix4x4 previewModel =
            SmoMeshResourceReplacer.CreatePreviewModelTransform(
                sourceMesh.WorldTransform,
                previewAdjustment);
        Matrix4x4 rendererReflection = Matrix4x4.CreateScale(1, 1, -1);
        Vector3 previewPosition = Vector3.Transform(
            memoryPreview.Positions[0],
            previewModel * rendererReflection);
        Vector3 savedVisiblePosition = Vector3.Transform(
            importedMesh.Positions[0],
            previewAdjustment.Matrix);
        True(Vector3.Distance(previewPosition, savedVisiblePosition) < 0.001f,
            "GPU fitting matrix matches the geometry that save will pack");
        True(document.ReplaceModel(
                targetMeshIndex,
                importedScene,
                ReplacementTransform.Identity,
                sourceMesh.WorldTransform,
                "level-editor-test.obj"),
            "model replacement enters editor history");
        True(document.ModelReplacements.ContainsKey(targetMeshIndex),
            "pending model replacement is exposed to the save pipeline");
        True(document.Undo() && document.ModelReplacements.Count == 0,
            "model replacement is undoable");
        True(document.Redo() &&
             document.ModelReplacements.ContainsKey(targetMeshIndex),
            "model replacement is redoable");

        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-model-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, "Alfea02-model.smo");
            SmoLevelSaveResult result = SmoLevelSaveService.Save(document, output);
            True(result.ResourceEditCount == 1 &&
                 result.EditedEntityCount == 0 &&
                 result.PatchedTransformCount == 0,
                "save reports one model resource edit without fake placement edits");
            SmoLevelWorkspace saved = SmoLevelWorkspace.Load(output);
            True(!saved.Document.Objects.Any(entry => entry.Id == oldMeshEntry.Id),
                "unused old mesh resource is omitted from the saved graph");
            SmoObjectEntry newMeshEntry = saved.Document.Objects.Single(entry =>
                entry.TypeHash == SmoClassIds.MeshData &&
                entry.Name.TrimEnd('\0') == "level-editor-test_lvl");
            SmoLevelAsset replaced = saved.Assets.Single(asset =>
                asset.ObjectIndex == newMeshEntry.Index);
            True(replaced.VertexCount == 3 && replaced.TriangleCount == 1,
                "saved model points to the newly added imported geometry");
            SmoMesh savedMesh = saved.PreparedScene.Meshes.First(mesh =>
                mesh.Mesh.ObjectIndex == newMeshEntry.Index).Mesh;
            True(savedMesh.Positions.SequenceEqual(memoryPreview.Positions) &&
                 savedMesh.TriangleIndices.SequenceEqual(
                     memoryPreview.TriangleIndices),
                "in-memory replacement exactly matches the geometry packed on save");
            True(replaced.Placements.Count == placementCount,
                "all existing placements were redirected to the new resource");
            True(File.ReadAllText(result.LogPath).Contains(
                    "RESOURCE_MODEL",
                    StringComparison.Ordinal),
                "save log records the model resource edit");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
        True(document.Undo() && document.ModelReplacements.Count == 0,
            "model integration fixture restores the level document");
    }

    private static void ValidateSharedPlacementClone(SmoDocument source)
    {
        const int sourceMeshIndex = 347;
        SmoSharedMeshInstanceInfo template =
            SmoSharedMeshInstanceResolver.ResolveAll(source).First(instance =>
                instance.SourceMeshObjectIndex == sourceMeshIndex);
        Matrix4x4 desired =
            Matrix4x4.CreateScale(1.25f, 0.75f, 1.5f) *
            Matrix4x4.CreateFromYawPitchRoll(0.31f, -0.17f, 0.43f) *
            Matrix4x4.CreateTranslation(
                template.WorldTransform.Translation + new Vector3(137, 19, -43));
        SmoSharedPlacementCloneResult result = SmoSharedPlacementCloner.Clone(
            source,
            sourceMeshIndex,
            desired,
            "SmoLVLcreator_clone_test");
        SmoDocument cloned = SmoDocument.Parse(result.Data, source.SourcePath);
        True(result.AddedObjectCount >= 2 &&
             cloned.Objects.Count == source.Objects.Count + result.AddedObjectCount,
            "drag placement clone adds a lightweight object branch");
        True(cloned.Objects[sourceMeshIndex].Id == source.Objects[sourceMeshIndex].Id,
            "appended placement keeps every existing object index stable");
        SmoSharedMeshInstanceInfo added =
            SmoSharedMeshInstanceResolver.ResolveAll(cloned).Single(instance =>
                instance.StaticObjectIndex == result.StaticObjectIndex);
        True(added.SourceMeshObjectIndex == sourceMeshIndex,
            "new placement references the original physical mesh resource");
        True(MatrixDistance(added.WorldTransform, desired) < 0.001f,
            "new shared placement stores requested XYZ rotation and nonuniform scale");
        SmoObjectEntry addedStatic = cloned.Objects[added.StaticObjectIndex];
        True(SmoStaticRenderObjectDecoder.TryDecode(
                 cloned,
                 addedStatic,
                 out SmoStaticRenderObjectData? addedStaticData,
                 out _) &&
             addedStaticData is not null &&
             MatrixDistance(
                 addedStaticData.EngineInverseTransform,
                 SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(desired)) <
             0.001f,
            "new shared placement writes the scaled Sparkplug inverse convention");
        SmoSharedMeshInstanceInfo nearestTemplate =
            SmoSharedMeshInstanceResolver.ResolveAll(source)
                .OrderBy(instance => Vector3.DistanceSquared(
                    instance.WorldTransform.Translation,
                    desired.Translation))
                .ThenBy(instance => instance.StaticObjectIndex)
                .First();
        uint expectedPartitionOwnerId = source.Objects[
            source.Objects[nearestTemplate.StaticObjectIndex].ParentIndex!.Value].Id;
        uint actualPartitionOwnerId = cloned.Objects[
            cloned.Objects[added.StaticObjectIndex].ParentIndex!.Value].Id;
        True(actualPartitionOwnerId == expectedPartitionOwnerId,
            "new placement is attached to the nearest spatial partition owner");

        var level = new SmoLevelDocument(
            SmoLevelWorkspace.Load(source.SourcePath!));
        level.AddSharedPlacement(
            sourceMeshIndex,
            desired,
            "SmoLVLcreator_drag_test");
        True(level.PlacementAdditions.Count == 1,
            "catalog drag enters editor history as a pending placement");
        True(level.Undo() && level.PlacementAdditions.Count == 0,
            "catalog drag placement is undoable");
        True(level.Redo() && level.PlacementAdditions.Count == 1,
            "catalog drag placement is redoable");
        SmoLevelPlacementAddition pending = level.PlacementAdditions.Values.Single();
        Vector3 editDelta = new(12, -7, 31);
        using (SmoPendingPlacementTransformSession session =
               level.BeginPendingPlacementTransform(
                   pending.GroupId,
                   external: false))
        {
            session.PreviewTranslation(editDelta);
            Matrix4x4 preview = level.PlacementAdditions[pending.Id].WorldTransform;
            True(Vector3.Distance(
                     new Vector3(preview.M41, preview.M42, preview.M43),
                     new Vector3(desired.M41, desired.M42, desired.M43) + editDelta) <
                 0.001f,
                "new shared placement supports live gizmo preview");
            True(session.Commit("Move pending placement"),
                "new shared placement transform commits to history");
        }
        True(level.Undo() &&
             MatrixDistance(
                 level.PlacementAdditions[pending.Id].WorldTransform,
                 desired) < 0.001f,
            "pending placement gizmo transform is undoable");
        True(level.Redo() &&
             Vector3.Distance(
                 new Vector3(
                     level.PlacementAdditions[pending.Id].WorldTransform.M41,
                     level.PlacementAdditions[pending.Id].WorldTransform.M42,
                     level.PlacementAdditions[pending.Id].WorldTransform.M43),
                 new Vector3(desired.M41, desired.M42, desired.M43) + editDelta) <
             0.001f,
            "pending placement gizmo transform is redoable");
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-placement-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, "Alfea02-placement.smo");
            int beforeCount = SmoSharedMeshInstanceResolver.ResolveAll(source)
                .Count(instance => instance.SourceMeshObjectIndex == sourceMeshIndex);
            SmoLevelSaveResult save = SmoLevelSaveService.Save(level, output);
            SmoDocument saved = SmoDocument.Load(output);
            int afterCount = SmoSharedMeshInstanceResolver.ResolveAll(saved)
                .Count(instance => instance.SourceMeshObjectIndex == sourceMeshIndex);
            True(save.AddedPlacementCount == 1 && afterCount == beforeCount + 1,
                "save installs one lightweight reference placement");
            True(File.ReadAllText(save.LogPath).Contains(
                    "PLACEMENT_ADD",
                    StringComparison.Ordinal),
                "save log records the added placement branch");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }

        HashSet<int> sharedMeshIndices = SmoSharedMeshInstanceResolver
            .ResolveAll(source)
            .Select(instance => instance.SourceMeshObjectIndex)
            .ToHashSet();
        SmoObjectEntry? embeddedMesh = source.Objects.FirstOrDefault(entry =>
            entry.TypeHash == SmoClassIds.MeshData &&
            !sharedMeshIndices.Contains(entry.Index) &&
            SmoSharedPlacementCloner.CanClone(source, entry.Index));
        if (embeddedMesh is not null)
        {
            SmoSharedPlacementCloneResult embeddedClone =
                SmoSharedPlacementCloner.Clone(
                    source,
                    embeddedMesh.Index,
                    Matrix4x4.CreateTranslation(321, 654, 987),
                    "embedded_clone_test");
            SmoDocument embeddedResult = SmoDocument.Parse(
                embeddedClone.Data,
                source.SourcePath);
            True(!embeddedResult.HasErrors && embeddedClone.AddedObjectCount > 0,
                "single-use embedded model can be placed as a shared reference");
            True(SmoSharedMeshInstanceResolver.ResolveAll(embeddedResult).Any(instance =>
                    instance.StaticObjectIndex == embeddedClone.StaticObjectIndex &&
                    instance.SourceMeshObjectIndex == embeddedMesh.Index),
                "embedded placement clone references the original mesh resource");
        }

        const int nodeAuthoredMeshIndex = 70;
        if (source.Objects[nodeAuthoredMeshIndex].TypeHash == SmoClassIds.MeshData)
        {
            SmoSharedPlacementCloneResult nodeClone = SmoSharedPlacementCloner.Clone(
                source,
                nodeAuthoredMeshIndex,
                Matrix4x4.CreateTranslation(111, 222, 333),
                "node_model_clone_test");
            SmoDocument nodeResult = SmoDocument.Parse(
                nodeClone.Data,
                source.SourcePath);
            True(!nodeResult.HasErrors && nodeClone.AddedObjectCount > 0,
                "node-authored model receives a static placement shell");
            True(SmoSharedMeshInstanceResolver.ResolveAll(nodeResult).Any(instance =>
                    instance.StaticObjectIndex == nodeClone.StaticObjectIndex &&
                    instance.SourceMeshObjectIndex == nodeAuthoredMeshIndex),
                "node-authored placement shell shares the original mesh");
            SmoSharedMeshInstanceInfo nodeAdded =
                SmoSharedMeshInstanceResolver.ResolveAll(nodeResult).Single(instance =>
                    instance.StaticObjectIndex == nodeClone.StaticObjectIndex);
            Matrix4x4 nodeDesired = Matrix4x4.CreateTranslation(111, 222, 333);
            SmoSharedMeshInstanceInfo nodeNearest =
                SmoSharedMeshInstanceResolver.ResolveAll(source)
                    .OrderBy(instance => Vector3.DistanceSquared(
                        instance.WorldTransform.Translation,
                        nodeDesired.Translation))
                    .ThenBy(instance => instance.StaticObjectIndex)
                    .First();
            True(nodeResult.Objects[
                     nodeResult.Objects[nodeAdded.StaticObjectIndex].ParentIndex!.Value].Id ==
                 source.Objects[
                     source.Objects[nodeNearest.StaticObjectIndex].ParentIndex!.Value].Id,
                "node-authored model uses a shell in the nearest spatial partition");
        }

        const int partitionNodeMeshIndex = 3373;
        if (source.Objects.Count > partitionNodeMeshIndex &&
            source.Objects[partitionNodeMeshIndex].TypeHash == SmoClassIds.MeshData)
        {
            SmoSharedPlacementCloneResult partitionClone =
                SmoSharedPlacementCloner.Clone(
                    source,
                    partitionNodeMeshIndex,
                    Matrix4x4.CreateTranslation(-3200, 125, -2100),
                    "partition_node_clone_test");
            SmoDocument partitionResult = SmoDocument.Parse(
                partitionClone.Data,
                source.SourcePath);
            True(SmoSharedMeshInstanceResolver.ResolveAll(partitionResult).Any(instance =>
                    instance.StaticObjectIndex == partitionClone.StaticObjectIndex &&
                    instance.SourceMeshObjectId ==
                        source.Objects[partitionNodeMeshIndex].Id),
                "partition-authored model receives a resolvable shared placement shell");
        }
    }

    private static void ValidateExternalModelAppend(SmoDocument source)
    {
        ImportedMesh Part(
            string name,
            float offset,
            int materialIndex = -1,
            bool includeNormals = true) => new(
            name,
            [new Vector3(offset, 0, 0), new Vector3(offset + 20, 0, 0),
             new Vector3(offset, 20, 0)],
            includeNormals ? [Vector3.UnitZ, Vector3.UnitZ, Vector3.UnitZ] : [],
            [Vector2.Zero, Vector2.UnitX, Vector2.UnitY],
            [0, 1, 2],
            [0xFFFF8040, 0xFF40FF80, 0xFF4080FF],
            materialIndex);
        byte[] sharedPng;
        using (var image = new Image<Rgba32>(8, 8, new Rgba32(30, 90, 180, 220)))
        using (var stream = new MemoryStream())
        {
            image.SaveAsPng(stream);
            sharedPng = stream.ToArray();
        }
        var imported = new ImportedScene(
            [Part("part_a", 0, 0, includeNormals: false), Part("part_b", 25, 0)],
            [new ImportedTexture("shared", "image/png", 8, 8, sharedPng)],
            [new ImportedMaterial("shared", "shared", 0)]);
        int templateIndex = SmoExternalLevelModelAppender.FindTemplateMeshObjectIndex(
            source,
            requireMaterial: true);
        SmoObjectEntry? uv1Template = source.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .FirstOrDefault(entry =>
            {
                try
                {
                    SmoMesh mesh = SmoMeshDecoder.Decode(source, entry);
                    return mesh.Marker == SmoMeshDecoder.E1Marker &&
                           SmoVertexLayoutRegistry.TryGet(
                               mesh.VertexFormat,
                               out SmoVertexLayout? layout) &&
                           layout?.TextureCoordinate1Offset is not null;
                }
                catch
                {
                    return false;
                }
            });
        True(uv1Template is not null,
            "real Alfea fixture exposes a writable two-UV E1 layout");
        var uv1Part = Part("uv1_writer", 0) with
        {
            SecondaryTextureCoordinates =
                [new Vector2(0.2f, 0.3f), new Vector2(0.4f, 0.5f),
                 new Vector2(0.6f, 0.7f)]
        };
        SmoMeshResourceReplacement uv1Replacement =
            SmoMeshResourceReplacer.Replace(
                source,
                uv1Template!.Index,
                new ImportedScene([uv1Part]),
                ReplacementTransform.Identity);
        SmoDocument uv1Document = SmoDocument.ParseOwned(
            uv1Replacement.Data,
            source.SourcePath);
        SmoMesh uv1Decoded = SmoMeshDecoder.Decode(
            uv1Document,
            uv1Document.Objects[uv1Template.Index]);
        True(uv1Decoded.TextureCoordinates.SequenceEqual(
                 uv1Part.TextureCoordinates) &&
             uv1Decoded.TextureCoordinates1.SequenceEqual(
                 uv1Part.SecondaryTextureCoordinates),
            "two-UV writer preserves distinct UV0 and UV1 channels");
        const int oversizedVertexCount = 65_538;
        var oversizedPositions = new Vector3[oversizedVertexCount];
        var oversizedIndices = new uint[oversizedVertexCount];
        for (int vertex = 0; vertex < oversizedVertexCount; vertex++)
        {
            oversizedPositions[vertex] = new Vector3(
                vertex,
                vertex % 5,
                vertex % 7);
            oversizedIndices[vertex] = checked((uint)vertex);
        }
        var oversizedScene = new ImportedScene([
            new ImportedMesh(
                "oversized_batch_range",
                oversizedPositions,
                [],
                [],
                oversizedIndices)
        ]);
        SmoExternalLevelModelAppendResult oversizedRange =
            SmoExternalLevelModelAppender.AppendPartRange(
                source,
                templateIndex,
                oversizedScene,
                [Matrix4x4.Identity],
                "oversized_batch_range",
                firstPartIndex: 1,
                partCount: 1);
        SmoDocument oversizedDocument = SmoDocument.ParseOwned(
            oversizedRange.Data,
            source.SourcePath);
        SmoMesh oversizedTail = SmoMeshDecoder.Decode(
            oversizedDocument,
            oversizedDocument.Objects.Single(entry =>
                entry.Id == oversizedRange.MeshObjectIds.Single()));
        True(oversizedTail.VertexCount == 3 && oversizedTail.TriangleCount == 1,
            "batch range indices are validated after deterministic UInt16 splitting");
        Matrix4x4 transformA = Matrix4x4.CreateTranslation(100, 200, 300);
        Matrix4x4 transformB = Matrix4x4.CreateTranslation(400, 500, 600);
        SmoExternalLevelModelAppendResult result =
            SmoExternalLevelModelAppender.AppendPartRange(
                source,
                templateIndex,
                imported,
                [transformA, transformB],
                "external_test",
                firstPartIndex: 0,
                partCount: imported.Meshes.Count);
        SmoDocument appended = SmoDocument.Parse(result.Data, source.SourcePath);
        True(!appended.HasErrors && result.MeshObjectIds.Count == 2,
            "external catalog model appends every mesh part as a valid resource");
        True(result.PlacementCount == 2 && result.AddedObjectCount > 0,
            "external model appender creates multiple placements in one resource pass");
        foreach (uint meshId in result.MeshObjectIds)
        {
            SmoMesh decoded = SmoMeshDecoder.Decode(
                appended,
                appended.Objects.Single(entry => entry.Id == meshId));
            True(decoded.TriangleCount == 1,
                "external mesh resource remains decodable after packing");
            True(decoded.TriangleIndices.SequenceEqual(new uint[] { 0, 2, 1 }),
                "external mesh reflection reverses triangle winding for engine handedness");
            True(decoded.HasNormals && decoded.Normals.All(normal =>
                    float.IsFinite(normal.X) &&
                    float.IsFinite(normal.Y) &&
                    float.IsFinite(normal.Z) &&
                    MathF.Abs(normal.Length() - 1f) < 0.001f),
                "external mesh writer preserves or reconstructs finite unit normals");
        }
        True(result.ImportedTextureObjectIds.Count == 1,
            "multi-part external model embeds one shared imported texture only once");
        HashSet<uint> appendedMeshIds = result.MeshObjectIds.ToHashSet();
        int[] textureObjectIndices = SmoViewer.Scene.SmoSceneBuilder.Build(appended).Meshes
            .Where(mesh => appendedMeshIds.Contains(
                appended.Objects[mesh.Mesh.ObjectIndex].Id))
            .Select(mesh => mesh.Texture?.ObjectIndex ?? -1)
            .Distinct()
            .ToArray();
        True(textureObjectIndices.Length == 1 && textureObjectIndices[0] >= 0,
            "every part and placement reuses the same embedded texture resource");

        string glbPath = Path.GetFullPath(Path.Combine(
            "local-data", "Модели", "Текна", "Беливикс DS", "Текна изм.glb"));
        if (File.Exists(glbPath))
        {
            ImportedScene textured = ImportedModelReader.Read(glbPath);
            SmoExternalLevelModelAppendResult texturedResult =
                SmoExternalLevelModelAppender.AppendPartRange(
                    source,
                    SmoExternalLevelModelAppender.FindTemplateMeshObjectIndex(
                        source,
                        requireMaterial: true),
                    textured,
                    [transformA],
                    "external_alpha_test",
                    firstPartIndex: 0,
                    partCount: textured.Meshes.Count);
            SmoDocument texturedDocument = SmoDocument.Parse(
                texturedResult.Data,
                source.SourcePath);
            True(!texturedDocument.HasErrors &&
                 texturedResult.MeshObjectIds.Count == textured.Meshes.Count,
                "real textured GLB is packable as a new level resource");
            True(textured.Materials.Any(material => material.UsesTextureAlpha),
                "real external fixture exercises alpha material serialization");

            string directory = Path.Combine(
                Path.GetTempPath(),
                $"SmoLVLcreator-external-{Guid.NewGuid():N}");
            Directory.CreateDirectory(directory);
            try
            {
                var level = new SmoLevelDocument(
                    SmoLevelWorkspace.Load(source.SourcePath!));
                Guid modelId = level.AddExternalModel(textured, glbPath, "Techna_test");
                True(level.ExternalModels[modelId].SuggestedPlacementScale == 100f,
                    "GLB catalog models convert metre units to the level centimetre scale");
                Guid externalPlacementId = level.AddExternalPlacement(
                    modelId,
                    transformA);
                using (SmoPendingPlacementTransformSession session =
                       level.BeginPendingPlacementTransform(
                           externalPlacementId,
                           external: true))
                {
                    session.PreviewTranslation(new Vector3(7, 8, 9));
                    True(session.Commit("Move external placement") &&
                         level.ExternalPlacements[externalPlacementId]
                             .WorldTransform.M41 == transformA.M41 + 7,
                        "external catalog placement supports gizmo transforms");
                }
                string output = Path.Combine(directory, "external-placement.smo");
                SmoLevelSaveResult save = SmoLevelSaveService.Save(level, output);
                SmoDocument saved = SmoDocument.Load(output);
                True(!saved.HasErrors && save.AddedPlacementCount == 1,
                    "level save packs a pending external catalog placement");
                True(File.ReadAllText(save.LogPath).Contains(
                        "EXTERNAL_MODEL_ADD",
                        StringComparison.Ordinal),
                    "save log records external resource packing");
            }
            finally
            {
                Directory.Delete(directory, recursive: true);
            }
        }
    }

    private static void ValidateCompositeResourceReplacement(string sourcePath)
    {
        var level = new SmoLevelDocument(SmoLevelWorkspace.Load(sourcePath));
        SmoCompositeModel target = level.CompositeModels.Single(model =>
            model.Name.Equals("Muza_smallSpk02", StringComparison.OrdinalIgnoreCase));
        SmoCompositeModel[] instances = level.CompositeModels
            .Where(model => model.Parts.Select(part => part.Asset.ObjectIndex)
                .Order()
                .SequenceEqual(target.Parts.Select(part => part.Asset.ObjectIndex)
                    .Order()))
            .ToArray();

        ImportedMesh Part(string name, float offset) => new(
            name,
            [new Vector3(offset, 0, 0), new Vector3(offset + 25, 0, 0),
             new Vector3(offset, 25, 0)],
            [Vector3.UnitZ, Vector3.UnitZ, Vector3.UnitZ],
            [Vector2.Zero, Vector2.UnitX, Vector2.UnitY],
            [0, 1, 2],
            [0xFFFFA040, 0xFF40FFA0, 0xFF4080FF]);
        var imported = new ImportedScene([
            Part("composite_new_a", 0),
            Part("composite_new_b", 35),
            Part("composite_new_c", 70)
        ]);
        Matrix4x4 reference = target.Entities[0].WorldTransform;
        var fit = new ReplacementTransform(
            1,
            Vector3.Zero,
            new Vector3(reference.M41, reference.M42, -reference.M43));
        Guid modelId = level.ReplaceCompositeModels(
            instances,
            imported,
            fit,
            reference,
            "composite-replacement-test.obj",
            "Muza_smallSpk02_replaced");

        True(level.ExternalModels.ContainsKey(modelId) &&
             level.ExternalPlacements.Values.Count(item => item.ModelId == modelId) ==
                 instances.Length,
            "composite replacement creates one new resource and one reference per placement");
        True(target.Parts.Count != imported.Meshes.Count &&
             target.Entities.All(entity => level.RemovedEntityIds.Contains(entity.Id)),
            "composite replacement accepts a different mesh count and removes old parts");
        int referenceInstanceIndex = Array.IndexOf(instances, target);
        Matrix4x4 placed = level.ExternalPlacements.Values.Single(item =>
            item.ModelId == modelId &&
            item.Name.EndsWith(
                $"_{referenceInstanceIndex + 1}",
                StringComparison.Ordinal)).WorldTransform;
        True(Vector3.Distance(placed.Translation, reference.Translation) < 0.005f,
            $"fitted composite replacement keeps the reference placement position " +
            $"(expected {reference.Translation}, actual {placed.Translation}, " +
            $"reference instance {referenceInstanceIndex})");
        True(level.Undo() && level.ExternalModels.Count == 0 &&
             target.Entities.All(entity => !level.RemovedEntityIds.Contains(entity.Id)),
            "composite replacement undo restores every old part atomically");
        True(level.Redo() && level.ExternalModels.ContainsKey(modelId),
            "composite replacement redo restores the new shared resource atomically");

        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-composite-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, "composite-replacement.smo");
            SmoLevelSaveResult saved = SmoLevelSaveService.Save(level, output);
            SmoLevelWorkspace reopened = SmoLevelWorkspace.Load(output);
            True(!reopened.Document.HasErrors &&
                 saved.AddedPlacementCount == instances.Length,
                "composite replacement survives strict save and reopen");
            True(imported.Meshes.All(part => reopened.Document.Objects.Any(entry =>
                    entry.TypeHash == SmoClassIds.MeshData &&
                    entry.Name.TrimEnd('\0').StartsWith(
                        part.Name,
                        StringComparison.OrdinalIgnoreCase))),
                "saved composite replacement contains every imported mesh resource");
            uint[] removedObjectIds = target.Entities.Select(entity =>
                level.Workspace.Document.Objects[entity.Id.SceneObjectIndex].Id).ToArray();
            True(removedObjectIds.All(id =>
                    reopened.Document.Objects.All(entry => entry.Id != id)),
                "saved composite replacement omits every old placement branch");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateNodeOwnedPlacementTransform(string sourcePath)
    {
        var level = new SmoLevelDocument(SmoLevelWorkspace.Load(sourcePath));
        SmoLevelEntity vase = level.Entities.Single(entity =>
            entity.Name.Equals("vase09-000", StringComparison.OrdinalIgnoreCase));
        Matrix4x4 original = vase.WorldTransform;
        Vector3 position = original.Translation + new Vector3(17, 3, -11);
        Matrix4x4 desired =
            Matrix4x4.CreateScale(1.15f) *
            Matrix4x4.CreateRotationZ(MathF.PI / 9) *
            Matrix4x4.CreateTranslation(position);
        True(level.SetEntityTransform(vase.Id, desired),
            "node-owned vase accepts translation, rotation and scale editing");

        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-node-transform-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, "node-transform.smo");
            SmoLevelSaveResult saved = SmoLevelSaveService.Save(level, output);
            var reopened = new SmoLevelDocument(SmoLevelWorkspace.Load(output));
            SmoLevelEntity reopenedVase = reopened.Entities.Single(entity =>
                entity.Name.Equals("vase09-000", StringComparison.OrdinalIgnoreCase));
            True(saved.EditedEntityCount == 1 &&
                 !reopened.Workspace.Document.HasErrors,
                "node-owned vase transform survives strict save and reopen");
            True(MatrixDistance(
                    desired,
                    reopenedVase.WorldTransform) < 0.01f,
                "node-owned vase preserves translation, rotation and scale");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidatePlacementRemoval(SmoDocument source)
    {
        SmoSharedMeshInstanceInfo placement =
            SmoSharedMeshInstanceResolver.ResolveAll(source).First();
        SmoLevelPlacementRemovalResult removed = SmoLevelPlacementRemover.Remove(
            source,
            placement.StaticObjectIndex);
        SmoDocument result = SmoDocument.Parse(removed.Data, source.SourcePath);
        True(!result.HasErrors && removed.RemovedObjectCount > 0,
            "deleting a visual placement removes a structurally complete branch");
        True(result.Objects.Count == source.Objects.Count - removed.RemovedObjectCount,
            "placement removal reports the exact catalog delta");

        SmoSharedMeshInstanceInfo[] instances =
            SmoSharedMeshInstanceResolver.ResolveAll(source).ToArray();
        SmoObjectEntry? resourceHost = source.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.StaticRenderObject)
            .FirstOrDefault(staticEntry => source.Objects.Any(mesh =>
                mesh.TypeHash == SmoClassIds.MeshData &&
                mesh.PhysicalOffset >= staticEntry.PhysicalOffset &&
                mesh.PhysicalEnd <= staticEntry.PhysicalEnd &&
                instances.Any(instance =>
                    instance.SourceMeshObjectIndex == mesh.Index &&
                    instance.StaticObjectIndex != staticEntry.Index)));
        if (resourceHost is not null)
        {
            SmoLevelPlacementRemovalResult hostedRemoval =
                SmoLevelPlacementRemover.Remove(source, resourceHost.Index);
            SmoDocument hostedResult = SmoDocument.Parse(
                hostedRemoval.Data,
                source.SourcePath);
            True(!hostedResult.HasErrors &&
                 hostedRemoval.RelocatedResourceIds.Count > 0,
                "deleting a resource-owning placement relocates shared resources");
            True(hostedRemoval.RelocatedResourceIds.All(id =>
                    hostedResult.Objects.Any(entry => entry.Id == id)),
                "every relocated shared resource survives placement deletion");
        }


        SmoObjectEntry? embeddedMesh = source.Objects
            .Where(entry =>
                entry.TypeHash == SmoClassIds.MeshData &&
                SmoPlacementTransformWriter.FindStaticPlacementOwnerIndex(
                    source, entry.Index) is null &&
                entry.ParentIndex is int parentIndex &&
                source.Objects[parentIndex].TypeHash == SmoClassIds.Model)
            .OrderByDescending(entry => entry.Name.StartsWith(
                "cadreZ03_2_", StringComparison.OrdinalIgnoreCase))
            .FirstOrDefault();
        if (embeddedMesh is not null)
        {
            SmoObjectEntry owningModel = source.Objects[embeddedMesh.ParentIndex!.Value];
            SmoLevelPlacementRemovalResult embeddedRemoval =
                SmoLevelPlacementRemover.Remove(source, embeddedMesh.Index);
            SmoDocument embeddedResult = SmoDocument.Parse(
                embeddedRemoval.Data,
                source.SourcePath);
            True(!embeddedResult.HasErrors &&
                 embeddedResult.Objects.All(entry => entry.Id != owningModel.Id),
                "deleting an embedded mesh removes its complete spModel branch");
            True(embeddedResult.Objects.All(entry => entry.Id != embeddedMesh.Id),
                "embedded-model deletion does not leave the selected mesh behind");
        }
    }

    private static void ValidateCompoundModelReplacement(SmoDocument source)
    {
        const int selectedMeshIndex = 94;
        SmoLevelModelGraphPlan plan = SmoLevelModelGraphReplacer.ResolvePlan(
            source,
            selectedMeshIndex);
        True(plan.Components.Count == 2,
            "Muza_smallSpk resolves as one complete two-mesh model graph");

        ImportedMesh Part(string name, float offset, int material) => new(
            name,
            [
                new Vector3(offset - 1, 0, 0),
                new Vector3(offset + 1, 0, 0),
                new Vector3(offset, 2, 0)
            ],
            [Vector3.UnitZ, Vector3.UnitZ, Vector3.UnitZ],
            [new Vector2(0, 0), new Vector2(1, 0), new Vector2(0.5f, 1)],
            [0, 1, 2],
            [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF],
            material);
        byte[] png;
        using (var textureImage = new Image<Rgba32>(
                   8, 8, new Rgba32(40, 170, 230, 128)))
        using (var textureStream = new MemoryStream())
        {
            textureImage.SaveAsPng(textureStream);
            png = textureStream.ToArray();
        }
        var imported = new ImportedScene(
            [Part("compound-a", -2, 0), Part("compound-b", 2, 1)],
            [new ImportedTexture("compound-texture", "image/png", 8, 8, png)],
            [
                new ImportedMaterial(
                    "compound-mat-a",
                    "compound-texture",
                    0,
                    ImportedMaterialAlphaMode.Blend),
                new ImportedMaterial(
                    "compound-mat-b",
                    "compound-texture",
                    0,
                    ImportedMaterialAlphaMode.Mask)
            ]);

        HashSet<uint> replacedMeshIds = plan.Components
            .Select(component => component.MeshObjectId)
            .ToHashSet();
        SmoObjectEntry sharedOldTexture = source.Objects.Single(entry => entry.Id == 98);
        byte[] sharedOldTextureBytes = source.Data.Span.Slice(
            checked((int)sharedOldTexture.PhysicalOffset),
            checked((int)sharedOldTexture.SerializedSize)).ToArray();
        SmoLevelModelGraphReplacementResult result =
            SmoLevelModelGraphReplacer.Replace(
                source,
                selectedMeshIndex,
                imported,
                ReplacementTransform.Identity,
                Matrix4x4.Identity);
        SmoDocument replaced = SmoDocument.Parse(result.Data, source.SourcePath);
        True(!replaced.HasErrors &&
             result.MeshObjectIds.Count == 2 &&
             result.TextureObjectIds.Values.Distinct().Count() == 1 &&
             replaced.Objects.Count == source.Objects.Count + result.AddedObjectCount,
            "compound replacement adds a valid new resource graph and primary placements");
        True(replaced.Objects.All(entry => !replacedMeshIds.Contains(entry.Id)),
            "compound replacement omits both unused old meshes");
        SmoObjectEntry retainedTexture = replaced.Objects.Single(entry => entry.Id == 98);
        True(replaced.Data.Span.Slice(
                checked((int)retainedTexture.PhysicalOffset),
                checked((int)retainedTexture.SerializedSize))
                .SequenceEqual(sharedOldTextureBytes),
            "compound replacement retains the byte-identical old texture used outside the model");
        SmoPreparedScene scene = SmoViewer.Scene.SmoSceneBuilder.Build(replaced);
        HashSet<uint> oldMeshIds = result.MeshObjectIds.Keys.ToHashSet();
        HashSet<uint> newMeshIds = result.MeshObjectIds.Values.ToHashSet();
        SmoSceneMesh[] newOccurrences = scene.Meshes.Where(mesh =>
            newMeshIds.Contains(replaced.Objects[mesh.Mesh.ObjectIndex].Id)).ToArray();
        SmoSceneMesh[] oldOccurrences = scene.Meshes.Where(mesh =>
            oldMeshIds.Contains(replaced.Objects[mesh.Mesh.ObjectIndex].Id)).ToArray();
        True(oldOccurrences.Length == 0 &&
             newOccurrences.Length == 6,
            "primary model and all four references use only the new meshes");
        True(newOccurrences.All(mesh => mesh.Texture?.Name == "compound-texture_lvl"),
            "all new compound-model parts use the newly added shared texture");
        True(newOccurrences.All(mesh =>
                mesh.MaterialRenderState?.BlendMode ==
                    SmoMaterialBlendMode.RigidTextureAlphaSurfaceFinalBlend2 &&
                mesh.RequiresTransparentOrdering),
            "GLB BLEND/MASK materials keep texture alpha on every replacement placement");
    }

    private static void ValidateMultiSelectionExport(string sourcePath)
    {
        var document = new SmoLevelDocument(SmoLevelWorkspace.Load(sourcePath));
        SmoLevelEntity[] selected = document.Entities
            .Where(entity => entity.Kind == SmoLevelEntityKind.Visual)
            .Take(3)
            .ToArray();
        True(selected.Length == 3, "Alfea exposes three visual export entities");

        var delta = new Vector3(17, -4, 9);
        using (SmoTransformSession session = document.BeginTransform(
                   selected.Select(entity => entity.Id)))
        {
            session.PreviewTranslation(delta);
            True(session.Commit("Move export selection"),
                "multi-selection export fixture keeps unsaved editor transforms");
        }

        SmoExportScene scene = SmoLevelExportService.BuildSelectionScene(
            document,
            selected.Select(entity => entity.Id));
        int expectedParts = selected.Sum(entity => entity.Parts.Count);
        True(scene.MeshPlacements.Count == expectedParts,
            "one export scene contains every part of all selected entities");
        True(scene.MeshPlacements.Select(placement => placement.SceneObjectIndex)
                .Distinct().Count() == expectedParts,
            "multi-selection placements keep independent scene identities");
        True(scene.Nodes.Count == 0 && scene.Skins.Count == 0,
            "rigid level selection excludes unrelated scene and skin nodes");

        SmoEditablePlacement firstPart = selected[0].Parts[0];
        SmoExportMeshPlacement exportedPart = scene.MeshPlacements.Single(placement =>
            placement.MeshObjectIndex == firstPart.Asset.ObjectIndex &&
            placement.SceneObjectIndex == firstPart.Source.SceneObjectIndex);
        True(MatrixDistance(
                exportedPart.WorldMatrix,
                SmoExportCoordinateSystem.ToExportMatrix(firstPart.WorldTransform)) < 0.0001f,
            "selection export uses the current unsaved editor transform");

        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-export-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, "three-selected.glb");
            SmoLevelExportResult result = SmoLevelExportService.Export(
                document,
                selected.Select(entity => entity.Id),
                output,
                SmoLevelExportFormat.Glb);
            True(result.EntityCount == 3 &&
                 result.PlacementCount == expectedParts,
                "GLB export reports the complete editor selection");
            True(File.Exists(output) && new FileInfo(output).Length > 20,
                "selected entities are written to one non-empty GLB");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateLinkedAffineSave(string sourcePath, bool rotate)
    {
        var document = new SmoLevelDocument(SmoLevelWorkspace.Load(sourcePath));
        var visualId = new SmoLevelEntityId(2722);
        var collisionId = new SmoLevelEntityId(1328);
        SmoLevelCollisionLink link = document.CollisionLinks.Single(candidate =>
            candidate.VisualEntityId == visualId &&
            candidate.CollisionEntityId == collisionId);
        SmoLevelEntity visual = document.GetEntity(visualId);
        SmoEditableCollision collision = document.GetEntity(collisionId)
            .Collisions.Single();
        Vector3 pivot = link.VisualBounds.Center;
        using (SmoTransformSession session = document.BeginTransform(
                   document.ExpandLinkedEntities([visualId])))
        {
            if (rotate)
            {
                session.PreviewRotation(Vector3.UnitY, MathF.PI / 6, pivot);
            }
            else
            {
                session.PreviewScale(
                    new Vector3(1.2f, 0.85f, 1.1f),
                    Quaternion.Identity,
                    pivot);
            }
            True(session.Commit(rotate
                    ? "Rotate linked Alfea table"
                    : "Scale linked Alfea table"),
                $"linked {(rotate ? "rotation" : "scale")} commits");
        }

        Matrix4x4 desiredVisual = visual.WorldTransform;
        Vector3 desiredCollisionVertex = Vector3.Transform(
            collision.Source.Positions[0],
            collision.WorldTransform);
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-affine-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, Path.GetFileName(sourcePath));
            SmoLevelSaveResult result = SmoLevelSaveService.Save(document, output);
            True(result.EditedEntityCount == 2 && result.PatchedTransformCount == 2,
                $"linked {(rotate ? "rotation" : "scale")} patches both entities");
            var saved = new SmoLevelDocument(SmoLevelWorkspace.Load(output));
            True(MatrixDistance(
                    saved.GetEntity(visualId).WorldTransform,
                    desiredVisual) < 0.002f,
                $"reopened visual keeps linked {(rotate ? "rotation" : "scale")}");
            SmoEditableCollision savedCollision = saved.GetEntity(collisionId)
                .Collisions.Single();
            Vector3 savedCollisionVertex = Vector3.Transform(
                savedCollision.Source.Positions[0],
                savedCollision.WorldTransform);
            True(Vector3.Distance(savedCollisionVertex, desiredCollisionVertex) < 0.002f,
                $"reopened collider keeps linked {(rotate ? "rotation" : "scale")}");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateAlfeaTableCollisionLink(
        SmoLevelDocument document,
        string sourcePath)
    {
        var visualId = new SmoLevelEntityId(2722);
        var collisionId = new SmoLevelEntityId(1328);
        SmoLevelCollisionLink? link = document.CollisionLinks.SingleOrDefault(candidate =>
            candidate.VisualEntityId == visualId &&
            candidate.CollisionEntityId == collisionId);
        True(link is not null,
            "Alfea table visual [2722] is linked to collider [1328]");
        True(link!.Confidence >= 0.62f,
            "Alfea table collision link has confirmed geometric confidence");

        IReadOnlyList<SmoLevelEntityId> expanded =
            document.ExpandLinkedEntities([visualId]);
        True(expanded.Contains(visualId) && expanded.Contains(collisionId),
            "linked movement expands the table selection to its collider");
        True(document.ExpandLinkedEntities([collisionId]).Contains(visualId),
            "linked movement is symmetric when the collider is selected");

        SmoLevelEntity visual = document.GetEntity(visualId);
        SmoLevelEntity collision = document.GetEntity(collisionId);
        Matrix4x4 visualBefore = visual.WorldTransform;
        Matrix4x4 collisionBefore = collision.WorldTransform;
        var delta = new Vector3(7, -3, 5);
        using (SmoTransformSession session = document.BeginTransform(expanded))
        {
            session.PreviewTranslation(delta);
            True(Translation(visual.WorldTransform) ==
                 Translation(visualBefore) + delta,
                "linked preview moves the table visual");
            True(Translation(collision.WorldTransform) ==
                 Translation(collisionBefore) + delta,
                "linked preview moves the table collider by the same delta");
            True(session.Commit("Move linked Alfea table"),
                "linked table movement commits as one editor command");
        }

        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-linked-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, Path.GetFileName(sourcePath));
            SmoLevelSaveResult result = SmoLevelSaveService.Save(document, output);
            True(result.EditedEntityCount == 2,
                "linked save contains the visual and collision entities");
            True(result.PatchedTransformCount == 2,
                "linked save patches one render transform and one collision mesh");

            var saved = new SmoLevelDocument(SmoLevelWorkspace.Load(output));
            True(saved.CollisionLinks.Any(candidate =>
                    candidate.VisualEntityId == visualId &&
                    candidate.CollisionEntityId == collisionId),
                "reopened SMO rediscovers the moved table/collision link");
            True(Translation(saved.GetEntity(visualId).WorldTransform) ==
                 Translation(visualBefore) + delta,
                "reopened SMO keeps the linked table translation");
            SmoEditableCollision savedCollision = saved.GetEntity(collisionId)
                .Collisions.Single();
            Vector3 collisionVertexBefore = Vector3.Transform(
                collision.Collisions.Single().Source.Positions[0],
                collisionBefore);
            Vector3 collisionVertexAfter = Vector3.Transform(
                savedCollision.Source.Positions[0],
                savedCollision.WorldTransform);
            True(Vector3.Distance(
                    collisionVertexAfter,
                    collisionVertexBefore + delta) < 0.001f,
                "reopened SMO keeps the linked collision translation");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }

        True(document.Undo(), "linked table movement is one undoable command");
        True(visual.WorldTransform.Equals(visualBefore) &&
             collision.WorldTransform.Equals(collisionBefore),
            "one undo restores both sides of the linked pair");

        using (SmoTransformSession session = document.BeginTransform([collisionId]))
        {
            session.PreviewTranslation(delta);
            True(visual.WorldTransform.Equals(visualBefore),
                "independent collision movement leaves the visual untouched");
            True(Translation(collision.WorldTransform) ==
                 Translation(collisionBefore) + delta,
                "independent collision movement remains available");
            session.Cancel();
        }
    }

    private static void ValidatePickingMath()
    {
        var ray = new SmoPickRay(
            new Vector3(0.25f, 0.25f, 2),
            -Vector3.UnitZ);
        True(
            SmoPickingMath.TryIntersectTriangle(
                ray,
                Vector3.Zero,
                Vector3.UnitX,
                Vector3.UnitY,
                out float distance,
                out Vector3 barycentric),
            "center-facing ray intersects a triangle");
        True(MathF.Abs(distance - 2) < 1e-5f, "picking distance is exact");
        True(
            MathF.Abs(barycentric.X + barycentric.Y + barycentric.Z - 1) < 1e-5f,
            "picking barycentric coordinates are normalized");
        True(
            !SmoPickingMath.TryIntersectTriangle(
                new SmoPickRay(new Vector3(2, 2, 2), -Vector3.UnitZ),
                Vector3.Zero,
                Vector3.UnitX,
                Vector3.UnitY,
                out _,
                out _),
            "ray outside a triangle misses");
    }

    private static void ValidateSaveDiagnostics()
    {
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-tests-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, "level_edited.smo");
            string logPath = SmoLevelSaveService.GetLogPath(output);
            True(
                logPath == output + ".SmoLVLcreator.log",
                "save diagnostics are placed beside the output SMO");
            True(
                SmoLevelSaveService.TryAppendDiagnostic(
                    logPath,
                    "ERROR",
                    "TEST_FAILURE",
                    "synthetic stack trace"),
                "save diagnostics can append an external GUI failure");
            string log = File.ReadAllText(logPath);
            True(log.Contains("[ERROR] [TEST_FAILURE]", StringComparison.Ordinal),
                "save log keeps severity and category");
            True(log.Contains("synthetic stack trace", StringComparison.Ordinal),
                "save log keeps diagnostic details");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void ValidateWorkspace(string path)
    {
        True(File.Exists(path), $"fixture exists: {path}");
        SmoLevelWorkspace workspace = SmoLevelWorkspace.Load(path);
        True(workspace.ObjectCount > 0, "workspace has objects");
        True(workspace.Assets.Count > 0, "workspace has decoded mesh assets");
        True(
            ReferenceEquals(workspace.Document, workspace.PreparedScene.Document),
            "workspace and render scene share one parsed document");
        True(
            workspace.PreparedScene.Meshes.Count == workspace.PlacementCount,
            "catalog placements come from the authoritative render scene");
        // This workspace test has no GPU context. Since the shared GPU-skinning
        // migration, Skin picking explicitly requires backend position output.
        var pickIndex = new SmoScenePickIndex(workspace.PreparedScene.Meshes);
        True(pickIndex.Count + pickIndex.Issues.Count == workspace.PreparedScene.Meshes.Count,
            "every prepared occurrence is pickable or has an explicit missing-backend diagnostic");
        True(pickIndex.Issues.All(issue => issue.Contains("SKIN_PICKING_POSITIONS_UNAVAILABLE",StringComparison.Ordinal)),
            "headless workspace does not synthesize skinned picking positions");
        ValidateEditableDocument(workspace);
        True(
            workspace.PlacementCount >= workspace.Assets.Count,
            "each decoded asset has at least its embedded placement");
        True(
            workspace.Assets.All(asset => asset.Placements.Count > 0),
            "asset placement lists are non-empty");
        True(
            workspace.Assets.All(asset => !string.IsNullOrWhiteSpace(asset.FullPath)),
            "asset paths are populated from the shared object hierarchy");
        True(
            workspace.Textures.Count <= workspace.TextureCount,
            "texture catalog contains only decoded spTextureData objects");
        True(
            workspace.Textures.Select(texture => texture.ObjectIndex).Distinct().Count() ==
            workspace.Textures.Count,
            "texture catalog contains unique object entries");
        True(
            workspace.Textures.All(texture =>
                texture.Bgra32Pixels.Length == texture.Width * texture.Height * 4),
            "texture catalog retains exact BGRA32 preview pixels");
        True(
            workspace.Assets.Where(asset => asset.Texture is not null).All(asset =>
                workspace.Textures.Any(texture =>
                    texture.ObjectIndex == asset.Texture!.ObjectIndex)),
            "every bound asset texture is exposed by the workspace catalog");
    }

    private static void ValidateEditableDocument(SmoLevelWorkspace workspace)
    {
        var document = new SmoLevelDocument(workspace);
        True(document.Entities.Count > 0, "document exposes level entities");
        True(
            document.Entities.Count <= document.Placements.Count,
            "an entity can group several physical mesh placements");
        True(
            document.Placements.All(placement =>
                placement.Entity.Parts.Contains(placement)),
            "every physical placement belongs to its level entity");
        SmoEditablePlacement? placement = document.Placements.FirstOrDefault(candidate =>
            document.CanPersistTransform(candidate.Entity.Id));
        if (placement is null)
            return;
        Matrix4x4 before = placement.WorldTransform;
        Matrix4x4 after = before * Matrix4x4.CreateTranslation(1, 2, 3);

        True(
            document.SetPlacementTransform(
                placement.Id,
                after,
                "Move test placement"),
            "document accepts a placement transform command");
        True(document.IsModified, "transform command marks document modified");
        True(document.CanUndo && !document.CanRedo, "new command is undoable");
        True(
            document.UndoDescription == "Move test placement",
            "undo exposes the command description");
        True(document.Undo(), "transform command can be undone");
        True(
            placement.WorldTransform.Equals(before),
            "undo restores the exact source transform");
        True(document.Redo(), "transform command can be redone");
        True(
            placement.WorldTransform.Equals(after),
            "redo restores the edited transform");
        document.MarkSaved();
        True(!document.IsModified, "saved history position clears dirty state");
        True(document.Undo(), "saved command can still be undone");
        True(document.IsModified, "undo away from saved state is dirty");
        True(
            document.SetPlacementTransform(
                placement.Id,
                before * Matrix4x4.CreateTranslation(-1, 0, 0)),
            "new command can branch after undo");
        True(!document.CanRedo, "branching edit discards redo history");

        var sessionDocument = new SmoLevelDocument(workspace);
        SmoLevelEntity[] movingEntities = sessionDocument.Entities
            .Where(entity => sessionDocument.CanPersistTransform(entity.Id))
            .Take(2)
            .ToArray();
        if (movingEntities.Length == 0)
            return;
        Matrix4x4[] originals = movingEntities
            .Select(entity => entity.WorldTransform)
            .ToArray();
        var delta = new Vector3(4, -2, 1);
        using (SmoTransformSession session = sessionDocument.BeginTransform(
                   movingEntities.Select(entity => entity.Id)))
        {
            session.PreviewTranslation(delta);
            True(sessionDocument.HasActiveTransformSession,
                "preview marks a transform session active");
            True(sessionDocument.IsModified,
                "live transform preview marks the document dirty");
            True(!sessionDocument.CanUndo,
                "history cannot run during a live transform");
            True(
                movingEntities.All(entity =>
                    MathF.Abs(entity.WorldTransform.M41 -
                        (originals[Array.IndexOf(movingEntities, entity)].M41 + delta.X)) <
                    1e-5f),
                "preview translates every selected entity");
            session.Cancel();
        }
        True(
            movingEntities.Select(entity => entity.WorldTransform)
                .SequenceEqual(originals),
            "cancel restores every entity exactly");
        True(!sessionDocument.CanUndo,
            "cancelled preview does not create a history command");

        using (SmoTransformSession session = sessionDocument.BeginTransform(
                   movingEntities.Select(entity => entity.Id)))
        {
            session.PreviewTranslation(delta);
            True(session.Commit("Move selected entities"),
                "changed transform session commits");
        }
        True(sessionDocument.CanUndo,
            "a committed multi-entity drag creates one undo command");
        True(sessionDocument.Undo(), "multi-entity drag can be undone");
        True(
            movingEntities.Select(entity => entity.WorldTransform)
                .SequenceEqual(originals),
            "one undo restores the complete multi-selection");

        SmoLevelEntity affineEntity = movingEntities[0];
        Matrix4x4 affineOriginal = affineEntity.WorldTransform;
        Vector3 pivot = Translation(affineOriginal);
        using (SmoTransformSession session = sessionDocument.BeginTransform(
                   [affineEntity.Id]))
        {
            session.PreviewRotation(Vector3.UnitY, MathF.PI / 2, pivot);
            True(Vector3.Distance(
                    Translation(affineEntity.WorldTransform), pivot) < 0.0001f,
                "rotation around the entity pivot preserves its position");
            True(session.RotationAxis == Vector3.UnitY &&
                 MathF.Abs(session.RotationRadians - MathF.PI / 2) < 0.0001f,
                "rotation preview exposes its axis and angle");
            session.Cancel();
        }
        True(affineEntity.WorldTransform.Equals(affineOriginal),
            "cancel restores a rotation preview exactly");

        using (SmoTransformSession session = sessionDocument.BeginTransform(
                   [affineEntity.Id]))
        {
            var factors = new Vector3(1.25f, 0.75f, 1.5f);
            session.PreviewScale(factors, Quaternion.Identity, pivot);
            True(Vector3.Distance(
                    Translation(affineEntity.WorldTransform), pivot) < 0.0001f,
                "scale around the entity pivot preserves its position");
            True(session.ScaleFactors == factors,
                "scale preview exposes its three factors");
            session.Cancel();
        }
        True(affineEntity.WorldTransform.Equals(affineOriginal),
            "cancel restores a scale preview exactly");
    }

    private static void True(bool condition, string message)
    {
        _assertions++;
        if (!condition)
            throw new InvalidOperationException(message);
    }

    private static void ValidateCancelledSavePreservesDestination(
        string sourcePath)
    {
        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoLVLcreator-cancel-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, "existing.smo");
            File.Copy(sourcePath, output);
            byte[] original = File.ReadAllBytes(output);
            SmoLevelWorkspace workspace = SmoLevelWorkspace.Load(sourcePath);
            var level = new SmoLevelDocument(workspace);
            using var cancellation = new CancellationTokenSource();
            var progress = new SynchronousProgress<SmoLevelSaveProgress>(item =>
            {
                if (item.Stage == SmoLevelSaveStage.RepairingCollisions)
                    cancellation.Cancel();
            });
            bool cancelled = false;
            try
            {
                SmoLevelSaveService.Save(
                    level,
                    output,
                    progress,
                    cancellation.Token);
            }
            catch (OperationCanceledException)
            {
                cancelled = true;
            }

            True(cancelled,
                "cancelled save reports OperationCanceledException");
            True(File.ReadAllBytes(output).AsSpan().SequenceEqual(original),
                "cancelled save preserves the existing destination byte-for-byte");
            True(!Directory.EnumerateFiles(directory, "*.tmp").Any(),
                "cancelled save leaves no temporary output");
            True(!Directory.EnumerateFiles(directory, "*.bak").Any(),
                "cancelled save does not install a backup");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private sealed class SynchronousProgress<T>(Action<T> handler) : IProgress<T>
    {
        public void Report(T value) => handler(value);
    }

    private static Vector3 Translation(Matrix4x4 transform) =>
        new(transform.M41, transform.M42, transform.M43);

    private static float MatrixDistance(Matrix4x4 left, Matrix4x4 right)
    {
        ReadOnlySpan<float> leftValues =
        [
            left.M11, left.M12, left.M13, left.M14,
            left.M21, left.M22, left.M23, left.M24,
            left.M31, left.M32, left.M33, left.M34,
            left.M41, left.M42, left.M43, left.M44
        ];
        ReadOnlySpan<float> rightValues =
        [
            right.M11, right.M12, right.M13, right.M14,
            right.M21, right.M22, right.M23, right.M24,
            right.M31, right.M32, right.M33, right.M34,
            right.M41, right.M42, right.M43, right.M44
        ];
        float maximum = 0;
        for (int index = 0; index < leftValues.Length; index++)
            maximum = MathF.Max(maximum, MathF.Abs(leftValues[index] - rightValues[index]));
        return maximum;
    }
}
