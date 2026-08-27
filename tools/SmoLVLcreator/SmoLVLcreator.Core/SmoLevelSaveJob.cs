using System.Numerics;
using System.Text.Json;
using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoLVLcreator.Core;

/// <summary>
/// Serializable, process-independent description of the editor state. The
/// worker always starts from BaseLevelPath, which is a byte-for-byte snapshot
/// of the document originally opened by the editor; it never reuses a file
/// that may already have been overwritten by an earlier save.
/// </summary>
public sealed class SmoLevelSaveJob
{
    public const int CurrentFormatVersion = 1;

    public int FormatVersion { get; init; } = CurrentFormatVersion;
    public required string OriginalSourcePath { get; init; }
    public required string BaseLevelPath { get; init; }
    public required string OutputPath { get; init; }
    public string? NativeFbxBridgePath { get; init; }
    public int TotalEntityCount { get; init; }
    public List<SmoSaveEntityTransform> EntityTransforms { get; init; } = [];
    public List<SmoSaveTextureReplacement> TextureReplacements { get; init; } = [];
    public List<SmoSaveModelReplacement> ModelReplacements { get; init; } = [];
    public List<SmoSaveSharedPlacementGroup> SharedPlacementGroups { get; init; } = [];
    public List<SmoSaveExternalModel> ExternalModels { get; init; } = [];
    public List<SmoSaveExternalPlacement> ExternalPlacements { get; init; } = [];
    public List<SmoSaveGeneratedCollision> GeneratedCollisions { get; init; } = [];
    public List<SmoSaveRemoval> Removals { get; init; } = [];

    public static string Write(
        SmoLevelDocument level,
        string outputPath,
        string jobDirectory,
        string? nativeFbxBridgePath = null)
    {
        ArgumentNullException.ThrowIfNull(level);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        ArgumentException.ThrowIfNullOrWhiteSpace(jobDirectory);
        if (level.HasActiveTransformSession)
            throw new InvalidOperationException(
                "Finish or cancel the active transform before saving.");

        string directory = Path.GetFullPath(jobDirectory);
        Directory.CreateDirectory(directory);
        string basePath = Path.Combine(directory, "base.smo");
        using (var stream = new FileStream(
                   basePath,
                   FileMode.CreateNew,
                   FileAccess.Write,
                   FileShare.Read,
                   bufferSize: 1024 * 1024,
                   FileOptions.SequentialScan))
        {
            stream.Write(level.Workspace.Document.Data.Span);
        }

        var job = new SmoLevelSaveJob
        {
            OriginalSourcePath = level.Workspace.SourcePath,
            BaseLevelPath = basePath,
            OutputPath = Path.GetFullPath(outputPath),
            TotalEntityCount = level.Entities.Count,
            NativeFbxBridgePath = string.IsNullOrWhiteSpace(nativeFbxBridgePath)
                ? null
                : Path.GetFullPath(nativeFbxBridgePath)
        };

        job.EntityTransforms.AddRange(level.Entities
            .Where(entity => entity.IsModified &&
                             entity.Id.SceneObjectIndex >= 0 &&
                             !level.RemovedEntityIds.Contains(entity.Id))
            .Select(entity => new SmoSaveEntityTransform(
                entity.Id.SceneObjectIndex,
                entity.Name,
                entity.Kind,
                SmoSaveMatrix.From(entity.OriginalWorldTransform),
                SmoSaveMatrix.From(entity.WorldTransform))));

        int textureOrdinal = 0;
        foreach (SmoLevelTextureReplacement replacement in
                 level.TextureReplacements.Values.OrderBy(item => item.TextureObjectIndex))
        {
            string fileName = $"texture-{textureOrdinal++:D4}.bin";
            string filePath = Path.Combine(directory, fileName);
            using (var stream = new FileStream(
                       filePath,
                       FileMode.CreateNew,
                       FileAccess.Write,
                       FileShare.Read,
                       bufferSize: 256 * 1024,
                       FileOptions.SequentialScan))
            {
                stream.Write(replacement.EncodedImage.Span);
            }
            job.TextureReplacements.Add(new SmoSaveTextureReplacement(
                replacement.TextureObjectIndex,
                filePath,
                replacement.SourcePath,
                replacement.ReplaceAlpha));
        }

        job.ModelReplacements.AddRange(level.ModelReplacements.Values
            .OrderBy(item => item.MeshObjectIndex)
            .Select(item => new SmoSaveModelReplacement(
                item.MeshObjectIndex,
                item.SourcePath,
                item.Transform.Scale,
                SmoSaveVector3.From(item.Transform.RotationDegrees),
                SmoSaveVector3.From(item.Transform.Translation),
                SmoSaveMatrix.From(item.ReferenceWorldTransform))));

        foreach (IGrouping<Guid, SmoLevelPlacementAddition> group in
                 level.PlacementAdditions.Values.GroupBy(item => item.GroupId))
        {
            job.SharedPlacementGroups.Add(new SmoSaveSharedPlacementGroup(
                group.Select(item => new SmoSaveSharedPlacement(
                    item.MeshObjectIndex,
                    SmoSaveMatrix.From(item.WorldTransform),
                    item.Name)).ToList()));
        }

        HashSet<Guid> placedExternalModelIds = level.ExternalPlacements.Values
            .Select(placement => placement.ModelId)
            .ToHashSet();
        job.ExternalModels.AddRange(level.ExternalModels.Values
            .Where(model => placedExternalModelIds.Contains(model.Id))
            .Select(model =>
            new SmoSaveExternalModel(
                model.Id,
                model.SourcePath,
                model.Name,
                model.TemplateMeshObjectIndex)));
        job.ExternalPlacements.AddRange(level.ExternalPlacements.Values.Select(placement =>
            new SmoSaveExternalPlacement(
                placement.Id,
                placement.ModelId,
                SmoSaveMatrix.From(placement.WorldTransform),
                placement.Name)));

        foreach (SmoLevelGeneratedCollision collision in
                 level.GeneratedCollisions.Values)
        {
            Matrix4x4 worldTransform = level.GetEntity(collision.EntityId).WorldTransform;
            job.GeneratedCollisions.Add(new SmoSaveGeneratedCollision(
                collision.Name,
                collision.Positions.Select(SmoSaveVector3.From).ToList(),
                collision.TriangleIndices.ToList(),
                SmoSaveMatrix.From(worldTransform)));
        }

        job.Removals.AddRange(level.RemovedEntityIds
            .Where(id => id.SceneObjectIndex >= 0)
            .Select(id => new SmoSaveRemoval(
                id.SceneObjectIndex,
                level.GetEntity(id).Kind))
            .OrderBy(item => item.EntityIndex));

        string jobPath = Path.Combine(directory, "save-job.json");
        File.WriteAllText(
            jobPath,
            JsonSerializer.Serialize(
                job,
                new JsonSerializerOptions { WriteIndented = false }));
        return jobPath;
    }

    public static SmoLevelSaveJob Read(string jobPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(jobPath);
        SmoLevelSaveJob job = JsonSerializer.Deserialize<SmoLevelSaveJob>(
            File.ReadAllText(Path.GetFullPath(jobPath))) ??
            throw new InvalidDataException("The isolated save job is empty.");
        if (job.FormatVersion != CurrentFormatVersion)
        {
            throw new InvalidDataException(
                $"Unsupported save-job version {job.FormatVersion}; " +
                $"expected {CurrentFormatVersion}.");
        }
        if (!File.Exists(job.BaseLevelPath))
            throw new FileNotFoundException("Save-job base level is missing.", job.BaseLevelPath);
        return job;
    }

    public SmoLevelSaveResult Execute(
        IProgress<SmoLevelSaveProgress>? progress = null,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var preparation = new SmoLevelSaveProgress(
            SmoLevelSaveStage.Preparing,
            0,
            1,
            "Preparing isolated save workspace",
            new FileInfo(BaseLevelPath).Length);
        progress?.Report(preparation);

        SmoDocument source = SmoDocument.Load(BaseLevelPath);
        var importedScenes = new Dictionary<string, ImportedScene>(
            StringComparer.OrdinalIgnoreCase);

        ImportedScene LoadImported(string path)
        {
            string fullPath = Path.GetFullPath(path);
            if (!File.Exists(fullPath))
                throw new FileNotFoundException(
                    "An imported model used by this save is no longer available.",
                    fullPath);
            if (!importedScenes.TryGetValue(fullPath, out ImportedScene? scene))
            {
                scene = ImportedModelReader.Read(
                    fullPath,
                    NativeFbxBridgePath,
                    cancellationToken);
                importedScenes.Add(fullPath, scene);
            }
            return scene;
        }

        SmoWorkerHostInfo workerHost = SmoWorkerHost.ResolveCurrent();
        var state = new SmoLevelSaveState
        {
            Source = source,
            SourcePath = OriginalSourcePath,
            TotalEntityCount = TotalEntityCount,
            BatchWorkerExecutable = workerHost.ExecutablePath,
            BatchWorkerAssembly = workerHost.ManagedEntryAssemblyPath,
            NativeFbxBridgePath = NativeFbxBridgePath,
            IsModified = EntityTransforms.Count > 0 ||
                         ModelReplacements.Count > 0 ||
                         TextureReplacements.Count > 0 ||
                         SharedPlacementGroups.Count > 0 ||
                         ExternalPlacements.Count > 0 ||
                         GeneratedCollisions.Count > 0 ||
                         Removals.Count > 0,
            Transforms = EntityTransforms.Select(edit =>
                new SmoLevelSaveTransform(
                    edit.EntityIndex,
                    edit.Name,
                    edit.Kind,
                    edit.OriginalWorldTransform.ToMatrix(),
                    edit.WorldTransform.ToMatrix(),
                    string.Empty,
                    string.Empty)).ToArray(),
            ModelReplacements = ModelReplacements.Select(replacement =>
                new SmoLevelModelReplacement(
                    replacement.MeshObjectIndex,
                    LoadImported(replacement.SourcePath),
                    new ReplacementTransform(
                        replacement.Scale,
                        replacement.RotationDegrees.ToVector3(),
                        replacement.Translation.ToVector3()),
                    replacement.ReferenceWorldTransform.ToMatrix(),
                    replacement.SourcePath)).ToArray(),
            TextureReplacements = TextureReplacements.Select(replacement =>
                new SmoLevelTextureReplacement(
                    replacement.TextureObjectIndex,
                    File.ReadAllBytes(replacement.EncodedImagePath),
                    replacement.SourcePath,
                    replacement.ReplaceAlpha)).ToArray(),
            PlacementAdditions = SharedPlacementGroups.SelectMany((group, groupIndex) =>
            {
                Guid groupId = CreateStableGuid(groupIndex, 0x31);
                return group.Placements.Select((item, itemIndex) =>
                    new SmoLevelPlacementAddition(
                        CreateStableGuid(groupIndex, itemIndex + 1),
                        groupId,
                        item.MeshObjectIndex,
                        item.WorldTransform.ToMatrix(),
                        item.Name));
            }).ToArray(),
            ExternalModels = ExternalModels.ToDictionary(
                model => model.Id,
                model => new SmoLevelExternalModel(
                    model.Id,
                    LoadImported(model.SourcePath),
                    model.SourcePath,
                    model.Name,
                    model.TemplateMeshObjectIndex)),
            ExternalPlacements = ExternalPlacements.Select(placement =>
                new SmoLevelExternalPlacement(
                    placement.Id,
                    placement.ModelId,
                    placement.WorldTransform.ToMatrix(),
                    placement.Name)).ToArray(),
            Removals = Removals.Select(item =>
                new SmoLevelSaveRemoval(item.EntityIndex, item.Kind)).ToArray(),
            GeneratedCollisions = GeneratedCollisions.Select((collision, index) =>
                new SmoLevelSaveCollision(
                    -1 - index,
                    collision.Name,
                    collision.Positions.Select(item => item.ToVector3()).ToArray(),
                    collision.TriangleIndices,
                    collision.WorldTransform.ToMatrix())).ToArray()
        };

        cancellationToken.ThrowIfCancellationRequested();
        return SmoLevelSaveService.SaveStateWithoutMemoryGuard(
            state,
            OutputPath,
            progress,
            cancellationToken);
    }

    private static Guid CreateStableGuid(int first, int second)
    {
        Span<byte> bytes = stackalloc byte[16];
        BitConverter.TryWriteBytes(bytes, first);
        BitConverter.TryWriteBytes(bytes[4..], second);
        bytes[8] = 0x53;
        bytes[9] = 0x4D;
        bytes[10] = 0x4F;
        bytes[11] = 0x4C;
        return new Guid(bytes);
    }
}

public sealed record SmoSaveEntityTransform(
    int EntityIndex,
    string Name,
    SmoLevelEntityKind Kind,
    SmoSaveMatrix OriginalWorldTransform,
    SmoSaveMatrix WorldTransform);

public sealed record SmoSaveTextureReplacement(
    int TextureObjectIndex,
    string EncodedImagePath,
    string SourcePath,
    bool ReplaceAlpha);

public sealed record SmoSaveModelReplacement(
    int MeshObjectIndex,
    string SourcePath,
    float Scale,
    SmoSaveVector3 RotationDegrees,
    SmoSaveVector3 Translation,
    SmoSaveMatrix ReferenceWorldTransform);

public sealed record SmoSaveSharedPlacementGroup(
    List<SmoSaveSharedPlacement> Placements);

public sealed record SmoSaveSharedPlacement(
    int MeshObjectIndex,
    SmoSaveMatrix WorldTransform,
    string Name);

public sealed record SmoSaveExternalModel(
    Guid Id,
    string SourcePath,
    string Name,
    int TemplateMeshObjectIndex);

public sealed record SmoSaveExternalPlacement(
    Guid Id,
    Guid ModelId,
    SmoSaveMatrix WorldTransform,
    string Name);

public sealed record SmoSaveGeneratedCollision(
    string Name,
    List<SmoSaveVector3> Positions,
    List<int> TriangleIndices,
    SmoSaveMatrix WorldTransform);

public sealed record SmoSaveRemoval(
    int EntityIndex,
    SmoLevelEntityKind Kind);

public sealed record SmoSaveVector3(float X, float Y, float Z)
{
    public static SmoSaveVector3 From(Vector3 value) =>
        new(value.X, value.Y, value.Z);

    public Vector3 ToVector3() => new(X, Y, Z);
}

public sealed record SmoSaveMatrix(
    float M11, float M12, float M13, float M14,
    float M21, float M22, float M23, float M24,
    float M31, float M32, float M33, float M34,
    float M41, float M42, float M43, float M44)
{
    public static SmoSaveMatrix From(Matrix4x4 value) => new(
        value.M11, value.M12, value.M13, value.M14,
        value.M21, value.M22, value.M23, value.M24,
        value.M31, value.M32, value.M33, value.M34,
        value.M41, value.M42, value.M43, value.M44);

    public Matrix4x4 ToMatrix() => new(
        M11, M12, M13, M14,
        M21, M22, M23, M24,
        M31, M32, M33, M34,
        M41, M42, M43, M44);
}

public sealed record SmoLevelSaveWorkerResult(
    bool Success,
    SmoLevelSaveResult? SaveResult,
    string? Error,
    string? Details)
{
    public static SmoLevelSaveWorkerResult FromSuccess(SmoLevelSaveResult result) =>
        new(true, result, null, null);

    public static SmoLevelSaveWorkerResult FromException(Exception exception) =>
        new(false, null, exception.Message, exception.ToString());
}
