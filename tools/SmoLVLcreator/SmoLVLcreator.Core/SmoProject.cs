using System.Buffers.Binary;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Numerics;
using SmoViewer.Core;

namespace SmoLVLcreator.Core;

/// <summary>
/// Project, editor-owned representation of an SMO container. Version 1
/// keeps the complete imported data section immutable while lifting the FFPS
/// directory, ownership intervals and direct fields into project metadata.
/// Schema-backed edits are stored separately and compiled into one copy-on-write
/// layout when an SMO is built.
/// </summary>
public sealed class SmoProject
{
    public const string ProjectFormat = "SmoLVLcreator.Project";
    public const int CurrentFormatVersion = 1;
    public const string ManifestEntryName = "project.json";
    public const string DataEntryName = "data.bin";
    public const string AssetEntryPrefix = "assets/";

    private readonly byte[] _dataSection;
    private readonly Dictionary<Guid, byte[]> _assetBlobs;

    internal SmoProject(
        SmoProjectManifest manifest,
        byte[] dataSection,
        IReadOnlyDictionary<Guid, byte[]>? assetBlobs = null)
    {
        Manifest = manifest;
        _dataSection = dataSection;
        _assetBlobs = assetBlobs?.ToDictionary(
            item => item.Key,
            item => item.Value) ?? [];
        Validate();
    }

    public SmoProjectManifest Manifest { get; }
    public ReadOnlyMemory<byte> DataSection => _dataSection;
    public long AssetDataLength =>
        _assetBlobs.Values.Sum(blob => (long)blob.Length);
    public IReadOnlyList<SmoProjectObject> Objects => Manifest.Objects;

    /// <summary>
    /// Replaces the complete serialized payload of one known object without
    /// modifying immutable data.bin or an immutable added-forest asset. The
    /// replacement must keep the object's size and SBOO identity; larger graph
    /// changes belong to forest additions/removals instead.
    /// </summary>
    public Guid ReplaceObjectData(uint objectId, ReadOnlySpan<byte> serializedData)
    {
        if (!TryGetKnownObjectType(objectId, out uint typeHash))
            throw new KeyNotFoundException($"Project object ID {objectId} is missing.");
        uint expectedSize = GetKnownObjectSize(objectId);
        if (serializedData.Length != checked((int)expectedSize))
        {
            throw new ArgumentException(
                $"Object {objectId} replacement has {serializedData.Length} bytes; " +
                $"exactly {expectedSize} are required.",
                nameof(serializedData));
        }
        if (serializedData.Length < 8 ||
            BinaryPrimitives.ReadUInt32LittleEndian(serializedData) != typeHash ||
            !serializedData[4..8].SequenceEqual("SBOO"u8))
        {
            throw new InvalidDataException(
                $"Object {objectId} replacement does not preserve its SBOO type signature.");
        }

        Guid blobId;
        do blobId = Guid.NewGuid();
        while (_assetBlobs.ContainsKey(blobId));
        byte[] blob = serializedData.ToArray();
        var replacement = new SmoProjectObjectDataReplacement
        {
            ObjectId = objectId,
            BlobId = blobId,
            BlobSha256 = Hash(blob)
        };
        SmoProjectObjectDataReplacement? previous =
            Manifest.ObjectDataReplacements.SingleOrDefault(item =>
                item.ObjectId == objectId);
        _assetBlobs.Add(blobId, blob);
        Manifest.ObjectDataReplacements.RemoveAll(item => item.ObjectId == objectId);
        Manifest.ObjectDataReplacements.Add(replacement);
        try
        {
            Validate();
            return blobId;
        }
        catch
        {
            Manifest.ObjectDataReplacements.Remove(replacement);
            if (previous is not null)
                Manifest.ObjectDataReplacements.Add(previous);
            _assetBlobs.Remove(blobId);
            throw;
        }
    }

    private uint GetKnownObjectSize(uint objectId)
    {
        SmoProjectObject? imported = Manifest.Objects.SingleOrDefault(item =>
            item.Id == objectId);
        if (imported is not null)
            return imported.SerializedSize;
        return Manifest.AddedForests
            .Where(forest => !Manifest.RemovedAddedForestIds.Contains(forest.BlobId))
            .SelectMany(forest => forest.Objects)
            .Single(item => item.Id == objectId)
            .SerializedSize;
    }

    internal ReadOnlyMemory<byte> GetAssetBlob(Guid blobId) =>
        _assetBlobs.TryGetValue(blobId, out byte[]? data)
            ? data
            : throw new InvalidDataException(
                $"Project asset {blobId:N} is missing.");

    private byte[] GetAssetBlobBytes(Guid blobId) =>
        _assetBlobs.TryGetValue(blobId, out byte[]? data)
            ? data
            : throw new InvalidDataException(
                $"Project asset {blobId:N} is missing.");

    internal void PruneUnreferencedAssetBlobs(IEnumerable<Guid>? historyAssetIds = null)
    {
        HashSet<Guid> retained = Manifest.AddedForests
            .Select(item => item.BlobId)
            .Concat(Manifest.ObjectDataReplacements.Select(item => item.BlobId))
            .ToHashSet();
        if (historyAssetIds is not null)
            retained.UnionWith(historyAssetIds);
        foreach (Guid blobId in _assetBlobs.Keys.Where(id => !retained.Contains(id)).ToArray())
            _assetBlobs.Remove(blobId);
    }

    internal static string GetAssetEntryName(Guid blobId) =>
        $"{AssetEntryPrefix}{blobId:N}.bin";

    public static SmoProject Import(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        if (document.HasErrors)
        {
            throw new InvalidDataException(
                "The project importer requires a structurally valid SMO.");
        }

        int dataStart = checked((int)document.Header.DataStart);
        int dataSize = checked((int)document.Header.DataSize);
        byte[] dataSection = document.Data.Span
            .Slice(dataStart, dataSize)
            .ToArray();
        var manifest = new SmoProjectManifest
        {
            Format = ProjectFormat,
            FormatVersion = CurrentFormatVersion,
            SourceFileName = string.IsNullOrWhiteSpace(document.SourcePath)
                ? null
                : Path.GetFileName(document.SourcePath),
            SourcePathHint = string.IsNullOrWhiteSpace(document.SourcePath)
                ? null
                : Path.GetFullPath(document.SourcePath),
            SourceLogicalPath = InferLogicalGameAssetPath(document.SourcePath),
            SourceSha256 = Hash(document.Data.Span),
            DataSha256 = Hash(dataSection),
            DataLength = dataSection.LongLength,
            Header = new SmoProjectHeader
            {
                SerializerVersion = document.Header.SerializerVersion,
                Unknown08 = document.Header.Unknown08,
                PlatformMask = document.Header.PlatformMask
            },
            Objects = document.Objects.Select(entry =>
            {
                bool decoded = SmoObjectFieldReader.TryRead(
                    document,
                    entry,
                    out IReadOnlyList<SmoObjectField> fields,
                    out string fieldError);
                return new SmoProjectObject
                {
                    Index = entry.Index,
                    Id = entry.Id,
                    RawName = entry.RawName.ToArray(),
                    TypeHash = entry.TypeHash,
                    LogicalOffset = entry.LogicalOffset,
                    SerializedSize = entry.SerializedSize,
                    ParentIndex = entry.ParentIndex,
                    FieldStreamDecoded = decoded,
                    FieldStreamError = decoded ? null : fieldError,
                    Fields = decoded
                        ? fields.Select(field => new SmoProjectField
                        {
                            FieldType = field.FieldType,
                            Occurrence = field.Occurrence,
                            RawHeader = field.RawHeader,
                            SizeKind = (byte)field.SizeKind,
                            HeaderSize = field.HeaderSize,
                            PayloadSize = field.PayloadSize,
                            RelativeHeaderOffset = field.RelativeHeaderOffset,
                            RelativePayloadOffset = field.RelativePayloadOffset
                        }).ToList()
                        : []
                };
            }).ToList()
        };
        return new SmoProject(manifest, dataSection);
    }

    private static string? InferLogicalGameAssetPath(string? sourcePath)
    {
        if (string.IsNullOrWhiteSpace(sourcePath))
            return null;
        string fullPath = Path.GetFullPath(sourcePath);
        string marker = $"{Path.DirectorySeparatorChar}Media{Path.DirectorySeparatorChar}";
        int media = fullPath.LastIndexOf(marker, StringComparison.OrdinalIgnoreCase);
        return media >= 0
            ? fullPath[(media + marker.Length)..]
                .Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar)
            : Path.GetFileName(fullPath);
    }

    /// <summary>
    /// Reserves identities for an externally produced object forest. The
    /// caller writes these IDs into the forest's inline prefixes and reference
    /// fields before adding the immutable blob to the project.
    /// </summary>
    public uint[] AllocateObjectIds(int count)
    {
        if (count <= 0)
            throw new ArgumentOutOfRangeException(nameof(count));
        HashSet<uint> used = GetAllObjectIds();
        var result = new uint[count];
        uint candidate = used.Count == 0 || used.Max() == uint.MaxValue
            ? 1
            : used.Max() + 1;
        for (int index = 0; index < result.Length; index++)
        {
            uint first = candidate;
            while (candidate == 0 || !used.Add(candidate))
            {
                candidate++;
                if (candidate == first)
                    throw new InvalidOperationException("The SMO object ID space is exhausted.");
            }
            result[index] = candidate;
            if (index + 1 < result.Length)
                candidate++;
        }
        return result;
    }

    /// <summary>
    /// Reports whether a producer-selected set of stable IDs can be attached
    /// without colliding with imported or already generated project objects.
    /// </summary>
    public bool AreObjectIdsAvailable(IEnumerable<uint> objectIds)
    {
        ArgumentNullException.ThrowIfNull(objectIds);
        uint[] requested = objectIds.ToArray();
        if (requested.Length == 0 || requested.Any(id => id == 0) ||
            requested.Distinct().Count() != requested.Length)
            return false;
        HashSet<uint> used = GetAllObjectIds();
        return requested.All(id => !used.Contains(id));
    }

    /// <summary>
    /// Adds an already serialized field forest at a direct field boundary of
    /// an imported object. The binary stays in a separate immutable project
    /// asset and is only streamed into the data section when an SMO is built.
    /// This raw core primitive deliberately does not appear in the editor UI.
    /// </summary>
    public Guid AddInlineForest(
        int targetOwnerIndex,
        int insertionRelativeOffset,
        ReadOnlySpan<byte> data,
        IReadOnlyList<SmoProjectAddedObject> objects)
    {
        SmoProjectObject target = GetObject(targetOwnerIndex);
        ArgumentNullException.ThrowIfNull(objects);
        if (data.IsEmpty)
            throw new ArgumentException("An inline forest asset cannot be empty.", nameof(data));
        Guid blobId;
        do
        {
            blobId = Guid.NewGuid();
        }
        while (_assetBlobs.ContainsKey(blobId));
        byte[] blob = data.ToArray();
        var operation = new SmoProjectAddedForest
        {
            BlobId = blobId,
            BlobSha256 = Hash(blob),
            TargetOwnerId = target.Id,
            InsertionRelativeOffset = insertionRelativeOffset,
            Objects = objects.Select(item => new SmoProjectAddedObject
            {
                Id = item.Id,
                RawName = item.RawName.ToArray(),
                TypeHash = item.TypeHash,
                ObjectRelativeOffset = item.ObjectRelativeOffset,
                SerializedSize = item.SerializedSize,
                ParentObjectId = item.ParentObjectId
            }).ToList()
        };
        _assetBlobs.Add(blobId, blob);
        Manifest.AddedForests.Add(operation);
        try
        {
            Validate();
            return blobId;
        }
        catch
        {
            Manifest.AddedForests.Remove(operation);
            _assetBlobs.Remove(blobId);
            throw;
        }
    }

    internal void DiscardAddedForests(IReadOnlyCollection<Guid> blobIds)
    {
        ArgumentNullException.ThrowIfNull(blobIds);
        if (blobIds.Count == 0)
            return;
        HashSet<Guid> discarded = blobIds.ToHashSet();
        Manifest.AddedForests.RemoveAll(item => discarded.Contains(item.BlobId));
        Manifest.AddedObjectPropertyEdits.RemoveAll(item =>
            discarded.Contains(item.BlobId));
        foreach (Guid blobId in discarded)
            _assetBlobs.Remove(blobId);
        Validate();
    }

    /// <summary>
    /// Bakes one or more collision world-transform edits into the vertex
    /// payloads of their spMeshBV objects. The operation uses the same shared
    /// schema writer as the viewer/editor and records only exact-size object
    /// replacements in the project journal.
    /// </summary>
    public void SetCollisionTransforms(
        IReadOnlyCollection<SmoProjectCollisionTransformEdit> edits)
    {
        ArgumentNullException.ThrowIfNull(edits);
        SmoProjectCollisionTransformEdit[] requested = edits
            .GroupBy(item => item.CollisionInfoObjectId)
            .Select(group => group.Last())
            .ToArray();
        if (requested.Length == 0)
            return;

        SmoDocument current = SmoProjectSerializer.CreateCurrentDocument(this);
        var meshIds = new Dictionary<uint, uint>(requested.Length);
        var patches = new List<SmoPlacementTransformEdit>(requested.Length);
        foreach (SmoProjectCollisionTransformEdit edit in requested)
        {
            SmoObjectEntry collision = current.Objects.SingleOrDefault(item =>
                    item.Id == edit.CollisionInfoObjectId)
                ?? throw new KeyNotFoundException(
                    $"Collision object ID {edit.CollisionInfoObjectId} was not found.");
            if (collision.TypeHash != SmoClassIds.CollisionInfo)
            {
                throw new ArgumentException(
                    $"Project object {edit.CollisionInfoObjectId} is not an spCollisionInfo.",
                    nameof(edits));
            }
            SmoObjectEntry mesh = current.Objects.SingleOrDefault(item =>
                    item.ParentIndex == collision.Index &&
                    item.TypeHash == SmoClassIds.MeshBoundingVolume)
                ?? throw new InvalidDataException(
                    $"Collision {edit.CollisionInfoObjectId} has no inline spMeshBV.");
            meshIds.Add(edit.CollisionInfoObjectId, mesh.Id);
            patches.Add(new SmoPlacementTransformEdit(
                collision.Index,
                edit.OriginalWorldTransform,
                edit.WorldTransform));
        }

        SmoPlacementTransformPatchResult result =
            SmoPlacementTransformWriter.Patch(current, patches);
        SmoDocument patched = SmoDocument.ParseOwned(
            result.Data,
            current.SourcePath);
        foreach (uint meshId in meshIds.Values)
        {
            SmoObjectEntry mesh = patched.Objects.Single(item => item.Id == meshId);
            ReplaceObjectData(
                meshId,
                patched.Data.Span.Slice(
                    checked((int)mesh.PhysicalOffset),
                    checked((int)mesh.SerializedSize)));
        }
        Validate();
    }

    /// <summary>
    /// Removes imported, generated and asset-owned scene branches through one
    /// project operation. Asset bytes are retained for Undo/Redo and project
    /// history, but tombstoned forests are not emitted by preview or SMO build.
    /// </summary>
    public void RemoveSceneBranches(IReadOnlyCollection<uint> objectIds)
    {
        ArgumentNullException.ThrowIfNull(objectIds);
        uint[] requested = objectIds.Distinct().ToArray();
        if (requested.Length == 0)
            return;

        var importedIds = new List<uint>();
        var generatedRoots = new List<SmoProjectReferencePlacement>();
        var removedForestIds = new HashSet<Guid>();
        var removedAddedObjectIds = new HashSet<uint>();
        HashSet<uint> requestedIds = requested.ToHashSet();
        foreach (uint objectId in requested)
        {
            if (Manifest.Objects.Any(item => item.Id == objectId))
            {
                importedIds.Add(objectId);
                continue;
            }
            SmoProjectReferencePlacement? generated =
                Manifest.ReferencePlacements.SingleOrDefault(item =>
                    item.NewObjectIds.Count > 0 && item.NewObjectIds[0] == objectId);
            if (generated is not null)
            {
                generatedRoots.Add(generated);
                continue;
            }
            SmoProjectAddedForest? forest = Manifest.AddedForests
                .Where(item => !Manifest.RemovedAddedForestIds.Contains(item.BlobId))
                .SingleOrDefault(item => item.Objects.Any(entry =>
                    entry.Id == objectId &&
                    entry.ParentObjectId == item.TargetOwnerId));
            if (forest is null)
            {
                throw new KeyNotFoundException(
                    $"Scene branch object ID {objectId} was not found in the project.");
            }
            if (TryPromoteReferencePlacement(
                    forest,
                    objectId,
                    requestedIds))
            {
                continue;
            }
            removedForestIds.Add(forest.BlobId);
            removedAddedObjectIds.UnionWith(forest.Objects.Select(item => item.Id));
        }

        foreach (SmoProjectReferencePlacement generated in generatedRoots)
        {
            Manifest.ReferencePlacements.Remove(generated);
            removedAddedObjectIds.UnionWith(generated.NewObjectIds);
        }

        // Added forests can contain registry/reference fields which point at
        // another added scene branch (the physics registry -> collision pair
        // is the important example). Tombstone those dependent reference
        // forests as well so a delete can never leave a dangling object ID.
        bool expanded;
        do
        {
            expanded = false;
            foreach (SmoProjectAddedForest forest in Manifest.AddedForests.Where(item =>
                         !Manifest.RemovedAddedForestIds.Contains(item.BlobId) &&
                         !removedForestIds.Contains(item.BlobId)))
            {
                if (!AddedForestReferencesAny(forest, removedAddedObjectIds))
                    continue;
                removedForestIds.Add(forest.BlobId);
                removedAddedObjectIds.UnionWith(
                    forest.Objects.Select(item => item.Id));
                expanded = true;
            }
        }
        while (expanded);

        foreach (Guid blobId in removedForestIds)
        {
            if (!Manifest.RemovedAddedForestIds.Contains(blobId))
                Manifest.RemovedAddedForestIds.Add(blobId);
        }
        Manifest.AddedObjectPropertyEdits.RemoveAll(item =>
            removedForestIds.Contains(item.BlobId));
        Manifest.ObjectDataReplacements.RemoveAll(item =>
            removedAddedObjectIds.Contains(item.ObjectId));
        RemoveCollisionLinkOverridesForIds(removedAddedObjectIds);
        if (importedIds.Count > 0)
            RemoveInlineBranches(importedIds);
        Validate();
    }

    /// <summary>
    /// An imported external-model forest can own both its first placement and
    /// the mesh/material/texture resource used by lightweight copies. If that
    /// first placement is deleted while a copy survives, move the owning
    /// branch to one surviving copy and remove the lightweight shell instead.
    /// This preserves the shared resource without duplicating its bytes.
    /// </summary>
    private bool TryPromoteReferencePlacement(
        SmoProjectAddedForest forest,
        uint removedRootObjectId,
        IReadOnlySet<uint> requestedRemovalIds)
    {
        SmoProjectAddedObject? root = forest.Objects.SingleOrDefault(item =>
            item.Id == removedRootObjectId &&
            item.ParentObjectId == forest.TargetOwnerId &&
            item.TypeHash == SmoClassIds.StaticRenderObject);
        if (root is null)
            return false;
        HashSet<uint> meshIds = forest.Objects
            .Where(item => item.TypeHash == SmoClassIds.MeshData)
            .Select(item => item.Id)
            .ToHashSet();
        if (meshIds.Count == 0)
            return false;
        SmoProjectReferencePlacement? survivor = Manifest.ReferencePlacements
            .Where(item => item.NewObjectIds.Count > 0 &&
                           !requestedRemovalIds.Contains(item.NewObjectIds[0]) &&
                           item.ResourceObjectIdOverride is uint resourceId &&
                           meshIds.Contains(resourceId))
            .OrderBy(item => item.NewObjectIds[0])
            .FirstOrDefault();
        if (survivor is null)
            return false;

        Matrix4x4 world = DecodeMatrix(
            survivor.WorldMatrix,
            $"promoted placement {survivor.NewObjectIds[0]} world matrix");
        SetPlacementTransform(removedRootObjectId, world);
        RemapCollisionLinkObjectId(
            survivor.NewObjectIds[0],
            removedRootObjectId);
        RemoveCollisionLinkOverridesForIds(
            survivor.NewObjectIds.Skip(1));
        Manifest.ReferencePlacements.Remove(survivor);
        return true;
    }

    private bool AddedForestReferencesAny(
        SmoProjectAddedForest forest,
        IReadOnlySet<uint> objectIds)
    {
        if (objectIds.Count == 0)
            return false;
        ReadOnlySpan<byte> blob = GetAssetBlob(forest.BlobId).Span;
        if (DirectFieldStreamReferencesAny(blob, 0, blob.Length, objectIds))
            return true;
        foreach (SmoProjectAddedObject entry in forest.Objects)
        {
            int start = checked((int)entry.ObjectRelativeOffset + 8);
            int end = checked((int)entry.ObjectRelativeOffset +
                              (int)entry.SerializedSize);
            if (start <= end && end <= blob.Length &&
                DirectFieldStreamReferencesAny(blob, start, end, objectIds))
            {
                return true;
            }
        }
        return false;
    }

    private static bool DirectFieldStreamReferencesAny(
        ReadOnlySpan<byte> data,
        int start,
        int end,
        IReadOnlySet<uint> objectIds)
    {
        int offset = start;
        while (offset < end &&
               SmoDataBlockReader.TryReadHeader(data, offset, out SmoDataBlockHeader field) &&
               field.PayloadEnd <= end)
        {
            if (field.PayloadSize == 8 &&
                BinaryPrimitives.ReadUInt32LittleEndian(data[field.PayloadOffset..]) is uint id &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    data[(field.PayloadOffset + sizeof(uint))..]) == 0 &&
                objectIds.Contains(id))
            {
                return true;
            }
            offset = checked((int)field.PayloadEnd);
        }
        return false;
    }

    /// <summary>
    /// Replaces one imported inline resource by redirecting every reference to
    /// another known project resource. The imported bytes stay immutable in
    /// data.bin; the old branch is omitted only while the output SMO is built.
    /// Shared descendants are first moved to one of their surviving consumers.
    /// </summary>
    public void RedirectResource(uint sourceObjectId, uint targetObjectId)
    {
        if (sourceObjectId == targetObjectId)
            throw new ArgumentException("A resource cannot redirect to itself.");
        SmoProjectObject source = Manifest.Objects.SingleOrDefault(item =>
                item.Id == sourceObjectId)
            ?? throw new KeyNotFoundException(
                $"Redirect source object ID {sourceObjectId} was not found in data.bin.");
        if (!TryGetKnownObjectType(targetObjectId, out uint targetType))
        {
            throw new KeyNotFoundException(
                $"Redirect target object ID {targetObjectId} was not found in the project.");
        }
        if (source.TypeHash != targetType)
        {
            throw new InvalidOperationException(
                $"Resource redirect type mismatch: {sourceObjectId} is 0x{source.TypeHash:X8}, " +
                $"but {targetObjectId} is 0x{targetType:X8}.");
        }
        if (source.ParentIndex is not int parentIndex ||
            !TryFindExactInlineField(Manifest.Objects[parentIndex], source, out _))
        {
            throw new InvalidOperationException(
                $"Redirect source {sourceObjectId} is not an imported inline resource.");
        }

        int relocationCount = Manifest.ResourceRelocations.Count;
        SmoProjectResourceRedirect? previous =
            Manifest.ResourceRedirects.SingleOrDefault(item =>
                item.SourceObjectId == sourceObjectId);
        SmoProjectPropertyEdit[] previousEdits = Manifest.PropertyEdits
            .Where(edit => GetBranchObjects(source).Any(item => item.Id == edit.ObjectId))
            .ToArray();
        SmoProjectCollisionLinkOverride[] previousCollisionLinks =
            Manifest.CollisionLinkOverrides.Select(item =>
                new SmoProjectCollisionLinkOverride
                {
                    VisualObjectId = item.VisualObjectId,
                    CollisionObjectId = item.CollisionObjectId,
                    Present = item.Present
                }).ToArray();
        try
        {
            _ = RelocateSharedLeavesForBranch(source.Index);
            HashSet<uint> removedIds = GetEffectiveRemovedObjects(source)
                .Select(item => item.Id)
                .ToHashSet();
            Manifest.PropertyEdits.RemoveAll(edit => removedIds.Contains(edit.ObjectId));
            RemoveCollisionLinkOverridesForIds(
                GetBranchObjects(source).Select(item => item.Id));
            Manifest.ResourceRedirects.RemoveAll(item =>
                item.SourceObjectId == sourceObjectId);
            Manifest.ResourceRedirects.Add(new SmoProjectResourceRedirect
            {
                SourceObjectId = sourceObjectId,
                TargetObjectId = targetObjectId
            });
            Validate();
        }
        catch
        {
            Manifest.ResourceRedirects.RemoveAll(item =>
                item.SourceObjectId == sourceObjectId);
            if (previous is not null)
                Manifest.ResourceRedirects.Add(previous);
            Manifest.PropertyEdits.RemoveAll(edit =>
                previousEdits.Any(previousEdit =>
                    previousEdit.ObjectId == edit.ObjectId &&
                    previousEdit.PropertyKey == edit.PropertyKey));
            Manifest.PropertyEdits.AddRange(previousEdits);
            Manifest.ResourceRelocations.RemoveRange(
                relocationCount,
                Manifest.ResourceRelocations.Count - relocationCount);
            Manifest.CollisionLinkOverrides.Clear();
            Manifest.CollisionLinkOverrides.AddRange(previousCollisionLinks);
            throw;
        }
    }

    public void TranslateStaticPlacement(int objectIndex, Vector3 delta)
    {
        if (!IsFinite(delta))
            throw new ArgumentException("Translation delta must be finite.", nameof(delta));
        SmoProjectObject entry = GetObject(objectIndex);
        if (entry.TypeHash != SmoClassIds.StaticRenderObject)
        {
            throw new ArgumentException(
                $"Project object {objectIndex} is not a static placement.",
                nameof(objectIndex));
        }

        Matrix4x4 world = ReadMatrixProperty(entry, SmoPropertyKeys.WorldMatrix);
        world.M41 += delta.X;
        world.M42 += delta.Y;
        world.M43 += delta.Z;
        if (!Matrix4x4.Invert(world, out _))
            throw new InvalidOperationException("Translated placement matrix is singular.");
        Matrix4x4 inverse =
            SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(world);
        SetProperty(entry, SmoPropertyKeys.WorldMatrix, SmoPropertyValueKind.Matrix4x4,
            SmoPropertyValueCodec.Encode(world));
        SetProperty(entry, SmoPropertyKeys.InverseWorldMatrix,
            SmoPropertyValueKind.Matrix4x4,
            SmoPropertyValueCodec.Encode(inverse));
        Validate();
    }

    public void SetProperty(int objectIndex, string propertyKey, Vector3 value) =>
        SetProperty(
            GetObject(objectIndex),
            propertyKey,
            SmoPropertyValueKind.Vector3,
            SmoPropertyValueCodec.Encode(value));

    public void SetProperty(int objectIndex, string propertyKey, Quaternion value) =>
        SetProperty(
            GetObject(objectIndex),
            propertyKey,
            SmoPropertyValueKind.Quaternion,
            SmoPropertyValueCodec.Encode(value));

    public void SetProperty(int objectIndex, string propertyKey, Matrix4x4 value) =>
        SetProperty(
            GetObject(objectIndex),
            propertyKey,
            SmoPropertyValueKind.Matrix4x4,
            SmoPropertyValueCodec.Encode(value));

    public byte[] GetPropertyBytes(
        int objectIndex,
        string propertyKey,
        SmoPropertyValueKind valueKind) =>
        ReadProperty(GetObject(objectIndex), propertyKey, valueKind);

    public void SetPropertyBytes(
        int objectIndex,
        string propertyKey,
        SmoPropertyValueKind valueKind,
        ReadOnlySpan<byte> value) =>
        SetProperty(
            GetObject(objectIndex),
            propertyKey,
            valueKind,
            value.ToArray());

    public bool CanRemoveInlineBranch(int objectIndex, out string reason)
    {
        if ((uint)objectIndex >= (uint)Manifest.Objects.Count)
        {
            reason = $"Project object index {objectIndex} is outside the catalog.";
            return false;
        }

        SmoProjectObject root = Manifest.Objects[objectIndex];
        if (root.ParentIndex is not int parentIndex)
        {
            reason = $"Project object {objectIndex} is a root object and has no inline owner.";
            return false;
        }

        SmoProjectObject parent = Manifest.Objects[parentIndex];
        if (!TryFindExactInlineField(parent, root, out _))
        {
            reason = $"Project object {objectIndex} has no exact inline owner field.";
            return false;
        }

        HashSet<uint> branchIds = GetEffectiveRemovedObjects(root)
            .Select(item => item.Id)
            .ToHashSet();
        SmoProjectExternalReference? externalReference =
            FindExternalReferences(branchIds).FirstOrDefault(reference =>
                reference.ReferencedObjectId != root.Id &&
                !HasGeneratedOwnerCandidate(reference.ReferencedObjectId));
        if (externalReference is not null)
        {
            reason = $"Branch [{objectIndex}] owns object ID " +
                $"{externalReference.ReferencedObjectId}, referenced by external " +
                $"consumer [{externalReference.Owner.Index}]; relocate the shared " +
                "resource first.";
            return false;
        }

        SmoProjectBranchRemoval? containingRemoval =
            Manifest.BranchRemovals.FirstOrDefault(removal =>
            {
                SmoProjectObject removedRoot = Manifest.Objects.Single(item =>
                    item.Id == removal.RootObjectId);
                return Contains(removedRoot, root) || Contains(root, removedRoot);
            });
        if (containingRemoval is not null)
        {
            reason = $"Branch [{objectIndex}] overlaps pending removal " +
                $"{containingRemoval.RootObjectId}.";
            return false;
        }

        reason = string.Empty;
        return true;
    }

    public void RemoveInlineBranch(int objectIndex)
    {
        if (!CanRemoveInlineBranch(objectIndex, out string reason))
            throw new InvalidOperationException(reason);

        SmoProjectObject root = Manifest.Objects[objectIndex];
        HashSet<uint> generatedResourceIds = GetBranchObjects(root)
            .Where(item => HasGeneratedOwnerCandidate(item.Id))
            .Select(item => item.Id)
            .ToHashSet();
        HashSet<uint> branchIds = GetBranchObjects(root)
            .Select(item => item.Id)
            .Where(id => !generatedResourceIds.Contains(id))
            .ToHashSet();
        Manifest.PropertyEdits.RemoveAll(edit => branchIds.Contains(edit.ObjectId));
        Manifest.ObjectDataReplacements.RemoveAll(replacement =>
            branchIds.Contains(replacement.ObjectId));
        RemoveCollisionLinkOverridesForIds(
            GetBranchObjects(root).Select(item => item.Id));
        Manifest.BranchRemovals.Add(new SmoProjectBranchRemoval
        {
            RootObjectId = root.Id
        });
        Validate();
    }

    public bool CanAddReferencePlacement(int objectIndex, out string reason)
    {
        if ((uint)objectIndex >= (uint)Manifest.Objects.Count)
        {
            reason = $"Project object index {objectIndex} is outside the catalog.";
            return false;
        }

        SmoProjectObject root = Manifest.Objects[objectIndex];
        if (root.TypeHash != SmoClassIds.StaticRenderObject)
        {
            reason = $"Project object [{objectIndex}] is not a static placement.";
            return false;
        }
        if (root.ParentIndex is not int parentIndex ||
            !TryFindExactInlineField(Manifest.Objects[parentIndex], root, out _))
        {
            reason = $"Placement [{objectIndex}] has no exact inline owner field.";
            return false;
        }

        IReadOnlyList<SmoProjectObject> branch = GetBranchObjects(root);
        if (branch.Any(item => !item.FieldStreamDecoded))
        {
            reason = $"Placement [{objectIndex}] contains an undecoded object stream.";
            return false;
        }
        if (branch.Any(item => item.TypeHash is
                SmoClassIds.MeshData or SmoClassIds.TextureData))
        {
            reason = $"Placement [{objectIndex}] physically owns mesh or texture data; " +
                "the first reference-placement gate only accepts reference-only branches.";
            return false;
        }

        HashSet<uint> branchIds = branch.Select(item => item.Id).ToHashSet();
        bool referencesExternalMesh = branch.Any(owner => owner.Fields.Any(field =>
        {
            if (field.PayloadSize != 8)
                return false;
            int payload = checked((int)owner.LogicalOffset + field.RelativePayloadOffset);
            ReadOnlySpan<byte> bytes = _dataSection.AsSpan(payload, 8);
            uint referencedId = BinaryPrimitives.ReadUInt32LittleEndian(bytes);
            uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(bytes[4..]);
            return inlineSize == 0 && !branchIds.Contains(referencedId) &&
                   Manifest.Objects.Any(item =>
                       item.Id == referencedId && item.TypeHash == SmoClassIds.MeshData);
        }));
        if (!referencesExternalMesh)
        {
            reason = $"Placement [{objectIndex}] has no confirmed external mesh reference.";
            return false;
        }

        try
        {
            _ = ReadMatrixProperty(root, SmoPropertyKeys.WorldMatrix);
            _ = ReadMatrixProperty(root, SmoPropertyKeys.InverseWorldMatrix);
        }
        catch (Exception exception) when (exception is
                   InvalidDataException or InvalidOperationException or
                   NotSupportedException)
        {
            reason = $"Placement [{objectIndex}] has no complete transform pair: " +
                exception.Message;
            return false;
        }

        reason = string.Empty;
        return true;
    }

    public uint AddReferencePlacementTranslated(
        int templateObjectIndex,
        Vector3 delta,
        string? displayName = null)
    {
        if (!IsFinite(delta))
            throw new ArgumentException("Translation delta must be finite.", nameof(delta));
        if (!CanAddReferencePlacement(templateObjectIndex, out string reason))
            throw new InvalidOperationException(reason);

        SmoProjectObject template = Manifest.Objects[templateObjectIndex];
        Matrix4x4 world = ReadMatrixProperty(template, SmoPropertyKeys.WorldMatrix);
        return AddReferencePlacementAt(
            templateObjectIndex,
            new Vector3(
                world.M41 + delta.X,
                world.M42 + delta.Y,
                world.M43 + delta.Z),
            displayName);
    }

    public uint AddReferencePlacementAt(
        int templateObjectIndex,
        Vector3 worldPosition,
        string? displayName = null)
    {
        if (!IsFinite(worldPosition))
            throw new ArgumentException("World position must be finite.", nameof(worldPosition));
        if (!CanAddReferencePlacement(templateObjectIndex, out string reason))
            throw new InvalidOperationException(reason);

        SmoProjectObject template = Manifest.Objects[templateObjectIndex];
        Matrix4x4 world = ReadMatrixProperty(template, SmoPropertyKeys.WorldMatrix);
        world.M41 = worldPosition.X;
        world.M42 = worldPosition.Y;
        world.M43 = worldPosition.Z;
        return AddReferencePlacementCore(
            templateObjectIndex,
            world,
            displayName,
            resourceObjectIdOverride: null);
    }

    /// <summary>
    /// Creates a lightweight static placement for a mesh resource which may
    /// come from either imported data.bin or a newly added project forest.
    /// The placement shell is cloned from the nearest confirmed imported
    /// reference-only branch; geometry, material and texture bytes are reused.
    /// </summary>
    public uint AddReferencePlacementForResource(
        uint meshObjectId,
        Matrix4x4 world,
        string? displayName = null)
    {
        if (!IsFinite(world))
            throw new ArgumentException("Placement matrix must be finite.", nameof(world));
        if (!CanUseMeshResource(meshObjectId, out string resourceReason))
            throw new ArgumentException(resourceReason, nameof(meshObjectId));
        uint effectiveResourceId = ResolveResourceRedirect(meshObjectId);
        SmoProjectObject? importedResource = Manifest.Objects.SingleOrDefault(item =>
            item.Id == effectiveResourceId);
        SmoProjectObject? physicalTemplate =
            FindImportedStaticPlacementForResource(effectiveResourceId);
        SmoProjectObject? exactReferenceTemplate =
            FindImportedReferencePlacementForResource(effectiveResourceId);
        uint? resourceOwnerId = physicalTemplate?.ParentIndex is int physicalOwnerIndex
            ? Manifest.Objects[physicalOwnerIndex].Id
            : TryGetResourcePlacementOwnerId(meshObjectId);
        SmoProjectObject? template = null;
        if (physicalTemplate is not null &&
            CanCreateReferencePlacementShell(
                physicalTemplate,
                effectiveResourceId,
                out _))
        {
            template = physicalTemplate;
        }
        else if (exactReferenceTemplate is not null)
        {
            template = exactReferenceTemplate;
        }
        else if (importedResource is null)
        {
            template = Manifest.Objects
                .Where(item =>
                    (resourceOwnerId is null ||
                     item.ParentIndex is int parentIndex &&
                     Manifest.Objects[parentIndex].Id == resourceOwnerId.Value) &&
                    CanAddReferencePlacement(item.Index, out _))
                .Select(item => new
                {
                    Item = item,
                    World = ReadMatrixProperty(item, SmoPropertyKeys.WorldMatrix)
                })
                .OrderBy(item => Vector3.DistanceSquared(
                    item.World.Translation,
                    world.Translation))
                .ThenBy(item => item.Item.Index)
                .Select(item => item.Item)
                .FirstOrDefault();
        }
        if (template is null && importedResource is not null)
        {
            throw new NotSupportedException(
                $"Imported mesh resource {meshObjectId} is baked partition geometry " +
                "without a static placement or an existing reference shell. It cannot " +
                "be instantiated safely without copying and rebasing its vertices.");
        }
        if (template is null)
        {
            throw new NotSupportedException(
                resourceOwnerId is uint ownerId
                    ? $"Resource {meshObjectId} belongs to owner {ownerId}, but that " +
                      "owner has no confirmed reference-only placement shell."
                    : "The project base has no confirmed reference-only placement shell.");
        }
        return AddReferencePlacementCore(
            template.Index,
            world,
            displayName,
            meshObjectId,
            importedResource is null ? resourceOwnerId : null);
    }

    public bool CanAddReferencePlacementForResource(
        uint meshObjectId,
        out string reason)
    {
        if (!CanUseMeshResource(meshObjectId, out reason))
            return false;
        uint effectiveId = ResolveResourceRedirect(meshObjectId);
        SmoProjectObject? physical = FindImportedStaticPlacementForResource(effectiveId);
        if (physical is not null &&
            CanCreateReferencePlacementShell(physical, effectiveId, out _))
        {
            reason = string.Empty;
            return true;
        }
        if (FindImportedReferencePlacementForResource(effectiveId) is not null)
        {
            reason = string.Empty;
            return true;
        }
        if (Manifest.Objects.Any(item => item.Id == effectiveId))
        {
            reason =
                $"Mesh resource {meshObjectId} is baked partition geometry without " +
                "a reusable static placement shell.";
            return false;
        }

        uint? ownerId = TryGetResourcePlacementOwnerId(meshObjectId);
        bool hasShell = Manifest.Objects.Any(item =>
            (ownerId is null ||
             item.ParentIndex is int parentIndex &&
             Manifest.Objects[parentIndex].Id == ownerId.Value) &&
            CanAddReferencePlacement(item.Index, out _));
        reason = hasShell
            ? string.Empty
            : "The project has no confirmed reference-only placement shell for this resource.";
        return hasShell;
    }

    private uint? TryGetResourcePlacementOwnerId(uint meshObjectId)
    {
        uint effectiveId = ResolveResourceRedirect(meshObjectId);
        SmoProjectResourceRelocation? relocation =
            Manifest.ResourceRelocations.SingleOrDefault(item =>
                item.ObjectId == effectiveId);
        if (relocation is not null)
            return relocation.TargetOwnerId;

        SmoProjectAddedForest? forest = Manifest.AddedForests
            .Where(item => !Manifest.RemovedAddedForestIds.Contains(item.BlobId))
            .SingleOrDefault(item => item.Objects.Any(entry =>
                entry.Id == effectiveId));
        if (forest is not null)
            return forest.TargetOwnerId;

        SmoProjectObject? staticRoot =
            FindImportedStaticPlacementForResource(effectiveId);
        return staticRoot?.ParentIndex is int ownerIndex
            ? Manifest.Objects[ownerIndex].Id
            : null;
    }

    private SmoProjectObject? FindImportedStaticPlacementForResource(
        uint objectId)
    {
        SmoProjectObject? current = Manifest.Objects.SingleOrDefault(item =>
            item.Id == objectId);
        while (current is not null)
        {
            if (current.TypeHash == SmoClassIds.StaticRenderObject)
                return current;
            current = current.ParentIndex is int parentIndex
                ? Manifest.Objects[parentIndex]
                : null;
        }
        return null;
    }

    private SmoProjectObject? FindImportedReferencePlacementForResource(uint objectId) =>
        Manifest.Objects
            .Where(item => CanAddReferencePlacement(item.Index, out _))
            .Where(item => GetBranchObjects(item).Any(owner => owner.Fields.Any(field =>
            {
                if (field.PayloadSize != 8)
                    return false;
                int payloadOffset = checked(
                    (int)owner.LogicalOffset + field.RelativePayloadOffset);
                ReadOnlySpan<byte> payload = _dataSection.AsSpan(payloadOffset, 8);
                return BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]) == 0 &&
                       ResolveResourceRedirect(
                           BinaryPrimitives.ReadUInt32LittleEndian(payload)) == objectId;
            })))
            .OrderBy(item => item.Index)
            .FirstOrDefault();

    private bool CanCreateReferencePlacementShell(
        SmoProjectObject template,
        uint resourceObjectId,
        out string reason)
    {
        if (CanAddReferencePlacement(template.Index, out reason))
            return true;
        if (template.TypeHash != SmoClassIds.StaticRenderObject ||
            template.ParentIndex is not int ownerIndex ||
            !TryFindExactInlineField(Manifest.Objects[ownerIndex], template, out _))
        {
            reason = $"Placement [{template.Index}] has no exact static owner field.";
            return false;
        }

        SmoProjectObject? resource = Manifest.Objects.SingleOrDefault(item =>
            item.Id == resourceObjectId && item.TypeHash == SmoClassIds.MeshData);
        if (resource is null || !Contains(template, resource) ||
            resource.ParentIndex is not int resourceOwnerIndex ||
            !TryFindExactInlineField(
                Manifest.Objects[resourceOwnerIndex],
                resource,
                out _))
        {
            reason = $"Placement [{template.Index}] does not physically own " +
                     $"mesh resource {resourceObjectId}.";
            return false;
        }

        IReadOnlyList<SmoProjectObject> shell =
            GetReferencePlacementShellObjects(template, resourceObjectId);
        if (shell.Count == 0 || shell.Any(item => !item.FieldStreamDecoded))
        {
            reason = $"Placement [{template.Index}] has an undecoded reference shell.";
            return false;
        }
        if (shell.Any(item => item.TypeHash is
                SmoClassIds.MeshData or SmoClassIds.TextureData))
        {
            reason = $"Placement [{template.Index}] owns more than one embedded resource; " +
                     "a lightweight shell cannot be derived safely.";
            return false;
        }

        try
        {
            _ = ReadMatrixProperty(template, SmoPropertyKeys.WorldMatrix);
            _ = ReadMatrixProperty(template, SmoPropertyKeys.InverseWorldMatrix);
        }
        catch (Exception exception) when (exception is
                   InvalidDataException or InvalidOperationException or
                   NotSupportedException)
        {
            reason = $"Placement [{template.Index}] has no complete transform pair: " +
                     exception.Message;
            return false;
        }

        reason = string.Empty;
        return true;
    }

    private IReadOnlyList<SmoProjectObject> GetReferencePlacementShellObjects(
        SmoProjectObject template,
        uint? resourceObjectIdOverride)
    {
        IReadOnlyList<SmoProjectObject> branch = GetBranchObjects(template);
        if (resourceObjectIdOverride is not uint resourceId)
            return branch;
        uint effectiveId = ResolveResourceRedirect(resourceId);
        SmoProjectObject? resource = Manifest.Objects.SingleOrDefault(item =>
            item.Id == effectiveId && Contains(template, item));
        if (resource is null)
            return branch;
        HashSet<uint> resourceIds = GetBranchObjects(resource)
            .Select(item => item.Id)
            .ToHashSet();
        return branch.Where(item => !resourceIds.Contains(item.Id)).ToArray();
    }

    public bool CanUseMeshResource(uint meshObjectId, out string reason)
    {
        if (CanUseObject(meshObjectId, SmoClassIds.MeshData, out reason))
            return true;
        reason = $"Project object ID {meshObjectId} is not a usable mesh resource. {reason}";
        return false;
    }

    /// <summary>
    /// Stores an editor-level visual/collision association in the project.
    /// It controls linked transforms but intentionally emits no invented
    /// engine field into the rebuilt SMO.
    /// </summary>
    public void SetCollisionLinkOverride(
        uint visualObjectId,
        uint collisionObjectId,
        bool present)
    {
        if (visualObjectId == collisionObjectId)
            throw new ArgumentException("A collision link requires two distinct objects.");
        if (!IsActiveProjectObjectId(visualObjectId))
        {
            throw new KeyNotFoundException(
                $"Visual object ID {visualObjectId} is not active in the project graph.");
        }
        if (!IsActiveProjectObjectId(collisionObjectId))
        {
            throw new KeyNotFoundException(
                $"Collision object ID {collisionObjectId} is not active in the project graph.");
        }
        Manifest.CollisionLinkOverrides.RemoveAll(item =>
            item.VisualObjectId == visualObjectId &&
            item.CollisionObjectId == collisionObjectId);
        Manifest.CollisionLinkOverrides.Add(new SmoProjectCollisionLinkOverride
        {
            VisualObjectId = visualObjectId,
            CollisionObjectId = collisionObjectId,
            Present = present
        });
        Validate();
    }

    /// <summary>
    /// Reports whether an object of the requested engine type is part of the
    /// current project graph. Unlike the immutable imported directory, this
    /// view excludes imported branches and added forests pending removal.
    /// Resource editors use it to reject stale catalogue selections without
    /// materializing the complete SMO container.
    /// </summary>
    public bool CanUseObject(
        uint objectId,
        uint expectedTypeHash,
        out string reason)
    {
        if (!TryGetKnownObjectType(objectId, out uint typeHash))
        {
            reason = $"Project object ID {objectId} is missing from the current graph.";
            return false;
        }
        if (typeHash != expectedTypeHash)
        {
            reason =
                $"Project object ID {objectId} has type 0x{typeHash:X8}, " +
                $"not 0x{expectedTypeHash:X8}.";
            return false;
        }
        SmoProjectObject? imported = Manifest.Objects.SingleOrDefault(item =>
            item.Id == objectId);
        if (imported is not null && IsImportedObjectPendingRemoval(imported))
        {
            reason = $"Project object ID {objectId} belongs to a removed branch.";
            return false;
        }
        reason = string.Empty;
        return true;
    }

    private uint AddReferencePlacementCore(
        int templateObjectIndex,
        Matrix4x4 world,
        string? displayName,
        uint? resourceObjectIdOverride,
        uint? targetOwnerIdOverride = null)
    {
        if (!IsFinite(world))
            throw new ArgumentException("Placement matrix must be finite.", nameof(world));
        SmoProjectObject template = Manifest.Objects[templateObjectIndex];
        bool validShell = resourceObjectIdOverride is uint resourceId
            ? CanCreateReferencePlacementShell(
                template,
                ResolveResourceRedirect(resourceId),
                out string reason)
            : CanAddReferencePlacement(templateObjectIndex, out reason);
        if (!validShell)
            throw new InvalidOperationException(reason);
        if (!Matrix4x4.Invert(world, out _))
            throw new InvalidOperationException("New placement matrix is singular.");
        Matrix4x4 inverse =
            SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(world);

        IReadOnlyList<SmoProjectObject> branch = GetReferencePlacementShellObjects(
            template,
            resourceObjectIdOverride);
        HashSet<uint> usedIds = GetAllObjectIds();
        uint candidate = usedIds.Count == 0 ? 1 : checked(usedIds.Max() + 1);
        var newIds = new List<uint>(branch.Count);
        foreach (SmoProjectObject _ in branch)
        {
            while (!usedIds.Add(candidate))
                candidate = checked(candidate + 1);
            newIds.Add(candidate);
            if (newIds.Count < branch.Count)
                candidate = checked(candidate + 1);
        }

        string name = string.IsNullOrWhiteSpace(displayName)
            ? $"{template.DisplayName}_copy"
            : displayName.Trim();
        byte[] rawName = Encoding.UTF8.GetBytes(name + '\0');
        if (rawName.Length > ushort.MaxValue)
            throw new ArgumentException("Placement name is too long for FFPS.", nameof(displayName));

        Manifest.ReferencePlacements.Add(new SmoProjectReferencePlacement
        {
            TemplateRootObjectId = template.Id,
            TargetOwnerId = targetOwnerIdOverride ??
                Manifest.Objects[template.ParentIndex!.Value].Id,
            NewObjectIds = newIds,
            RootRawName = rawName,
            WorldMatrix = SmoPropertyValueCodec.Encode(world),
            InverseWorldMatrix = SmoPropertyValueCodec.Encode(inverse),
            ResourceObjectIdOverride = resourceObjectIdOverride
        });
        Validate();
        return newIds[0];
    }

    public void SetPlacementTransform(uint objectId, Matrix4x4 world)
    {
        byte[] worldBytes = SmoPropertyValueCodec.Encode(world);
        if (!Matrix4x4.Invert(world, out _))
            throw new InvalidOperationException("Placement matrix is singular.");
        Matrix4x4 inverse =
            SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(world);
        byte[] inverseBytes = SmoPropertyValueCodec.Encode(inverse);

        SmoProjectObject? imported = Manifest.Objects.SingleOrDefault(item =>
            item.Id == objectId);
        if (imported is not null)
        {
            if (imported.TypeHash != SmoClassIds.StaticRenderObject)
            {
                throw new ArgumentException(
                    $"Project object {objectId} is not a static placement.",
                    nameof(objectId));
            }
            SetProperty(
                imported,
                SmoPropertyKeys.WorldMatrix,
                SmoPropertyValueKind.Matrix4x4,
                worldBytes);
            SetProperty(
                imported,
                SmoPropertyKeys.InverseWorldMatrix,
                SmoPropertyValueKind.Matrix4x4,
                inverseBytes);
            Validate();
            return;
        }

        SmoProjectReferencePlacement? generated =
            Manifest.ReferencePlacements.SingleOrDefault(item =>
                item.NewObjectIds.Count > 0 && item.NewObjectIds[0] == objectId);
        if (generated is not null)
        {
            generated.WorldMatrix = worldBytes;
            generated.InverseWorldMatrix = inverseBytes;
            Validate();
            return;
        }

        SmoProjectAddedForest? forest = Manifest.AddedForests
            .SingleOrDefault(item => item.Objects.Any(entry => entry.Id == objectId));
        SmoProjectAddedObject? added = forest?.Objects
            .SingleOrDefault(entry => entry.Id == objectId);
        if (forest is null || added is null)
            throw new KeyNotFoundException($"Placement object ID {objectId} was not found.");
        if (added.TypeHash != SmoClassIds.StaticRenderObject)
        {
            throw new ArgumentException(
                $"Project object {objectId} is not a static placement.",
                nameof(objectId));
        }
        SetAddedObjectProperty(
            forest,
            added,
            SmoPropertyKeys.WorldMatrix,
            SmoPropertyValueKind.Matrix4x4,
            worldBytes);
        SetAddedObjectProperty(
            forest,
            added,
            SmoPropertyKeys.InverseWorldMatrix,
            SmoPropertyValueKind.Matrix4x4,
            inverseBytes);
        Validate();
    }

    public void RemovePlacement(uint objectId)
    {
        int generatedIndex = Manifest.ReferencePlacements.FindIndex(item =>
            item.NewObjectIds.Count > 0 && item.NewObjectIds[0] == objectId);
        if (generatedIndex >= 0)
        {
            RemoveCollisionLinkOverridesForIds(
                Manifest.ReferencePlacements[generatedIndex].NewObjectIds);
            Manifest.ReferencePlacements.RemoveAt(generatedIndex);
            Validate();
            return;
        }

        SmoProjectObject imported = Manifest.Objects.SingleOrDefault(item =>
                item.Id == objectId)
            ?? throw new KeyNotFoundException(
                $"Placement object ID {objectId} was not found.");
        if (imported.TypeHash != SmoClassIds.StaticRenderObject)
        {
            throw new ArgumentException(
                $"Project object {objectId} is not a static placement.",
                nameof(objectId));
        }

        int relocationCount = Manifest.ResourceRelocations.Count;
        int removalCount = Manifest.BranchRemovals.Count;
        try
        {
            _ = RelocateSharedLeavesForBranch(imported.Index);
            RemoveInlineBranch(imported.Index);
            Validate();
        }
        catch
        {
            Manifest.ResourceRelocations.RemoveRange(
                relocationCount,
                Manifest.ResourceRelocations.Count - relocationCount);
            Manifest.BranchRemovals.RemoveRange(
                removalCount,
                Manifest.BranchRemovals.Count - removalCount);
            throw;
        }
    }

    /// <summary>
    /// Removes several imported static placements as one reachability unit.
    /// Resources referenced only by another placement in the same removal set
    /// are pruned; resources still used outside the set are relocated once.
    /// </summary>
    public void RemovePlacements(IReadOnlyCollection<uint> objectIds)
    {
        ArgumentNullException.ThrowIfNull(objectIds);
        foreach (uint objectId in objectIds.Distinct())
        {
            SmoProjectObject root = Manifest.Objects.SingleOrDefault(item =>
                    item.Id == objectId)
                ?? throw new KeyNotFoundException(
                    $"Placement object ID {objectId} was not found in data.bin.");
            if (root.TypeHash != SmoClassIds.StaticRenderObject)
            {
                throw new ArgumentException(
                    $"Project object {objectId} is not a static placement.",
                    nameof(objectIds));
            }
        }
        RemoveInlineBranches(objectIds);
    }

    /// <summary>
    /// Removes an arbitrary set of imported inline branches as one
    /// reachability unit. Contained roots are folded into their outer root.
    /// This is the generic core operation used by complete-model replacement;
    /// the GUI still exposes deletion in terms of scene entities.
    /// </summary>
    public void RemoveInlineBranches(IReadOnlyCollection<uint> objectIds)
    {
        ArgumentNullException.ThrowIfNull(objectIds);
        uint[] distinctIds = objectIds.Distinct().ToArray();
        if (distinctIds.Length == 0)
            return;
        SmoProjectObject[] requestedRoots = distinctIds.Select(id =>
        {
            SmoProjectObject root = Manifest.Objects.SingleOrDefault(item =>
                    item.Id == id)
                ?? throw new KeyNotFoundException(
                    $"Inline branch object ID {id} was not found in data.bin.");
            if (root.ParentIndex is not int parentIndex ||
                !TryFindExactInlineField(Manifest.Objects[parentIndex], root, out _))
            {
                throw new InvalidOperationException(
                    $"Object {id} is not an imported inline branch.");
            }
            return root;
        }).ToArray();
        SmoProjectObject[] roots = requestedRoots
            .Where(candidate => !requestedRoots.Any(other =>
                other.Id != candidate.Id && Contains(other, candidate)))
            .ToArray();

        int relocationCount = Manifest.ResourceRelocations.Count;
        int removalCount = Manifest.BranchRemovals.Count;
        SmoProjectPropertyEdit[] priorEdits = Manifest.PropertyEdits
            .Where(edit => roots.Any(root =>
                GetBranchObjects(root).Any(item => item.Id == edit.ObjectId)))
            .ToArray();
        SmoProjectObjectDataReplacement[] priorReplacements =
            Manifest.ObjectDataReplacements
                .Where(replacement => roots.Any(root =>
                    GetBranchObjects(root).Any(item =>
                        item.Id == replacement.ObjectId)))
                .ToArray();
        SmoProjectCollisionLinkOverride[] priorCollisionLinks =
            Manifest.CollisionLinkOverrides.Select(item =>
                new SmoProjectCollisionLinkOverride
                {
                    VisualObjectId = item.VisualObjectId,
                    CollisionObjectId = item.CollisionObjectId,
                    Present = item.Present
                }).ToArray();
        try
        {
            HashSet<uint> removedIds = roots
                .SelectMany(GetEffectiveRemovedObjects)
                .Select(item => item.Id)
                .ToHashSet();
            foreach (SmoProjectObject root in roots)
                removedIds.Remove(root.Id);
            while (FindExternalReferences(removedIds).FirstOrDefault() is
                   SmoProjectExternalReference reference)
            {
                SmoProjectObject resource = Manifest.Objects.Single(item =>
                    item.Id == reference.ReferencedObjectId);
                if (HasGeneratedOwnerCandidate(resource.Id))
                {
                    removedIds.Remove(resource.Id);
                    continue;
                }
                if (!CanRelocateInlineLeaf(
                        resource.Index,
                        reference.Owner.Index,
                        existingRelocationObjectId: null,
                        reference.FieldType,
                        reference.FieldOccurrence,
                        out string reason))
                {
                    throw new InvalidOperationException(
                        $"Shared object {resource.Id} cannot be relocated: {reason}");
                }
                RelocateInlineLeaf(resource.Index, reference);
                removedIds.Remove(resource.Id);
            }

            HashSet<uint> generatedResourceIds = roots
                .SelectMany(GetBranchObjects)
                .Where(item => item.TypeHash == SmoClassIds.MeshData)
                .Select(item => item.Id)
                .Where(id => Manifest.ReferencePlacements.Any(placement =>
                    placement.ResourceObjectIdOverride is uint resourceId &&
                    ResolveResourceRedirect(resourceId) == id))
                .ToHashSet();
            HashSet<uint> fullRemovedIds = roots
                .SelectMany(GetBranchObjects)
                .Select(item => item.Id)
                .Where(id => !generatedResourceIds.Contains(id))
                .ToHashSet();
            Manifest.PropertyEdits.RemoveAll(edit =>
                fullRemovedIds.Contains(edit.ObjectId));
            Manifest.ObjectDataReplacements.RemoveAll(replacement =>
                fullRemovedIds.Contains(replacement.ObjectId));
            RemoveCollisionLinkOverridesForIds(
                roots.SelectMany(GetBranchObjects).Select(item => item.Id));
            Manifest.BranchRemovals.AddRange(roots.Select(root =>
                new SmoProjectBranchRemoval
                {
                    RootObjectId = root.Id
                }));
            Validate();
        }
        catch
        {
            Manifest.ResourceRelocations.RemoveRange(
                relocationCount,
                Manifest.ResourceRelocations.Count - relocationCount);
            Manifest.BranchRemovals.RemoveRange(
                removalCount,
                Manifest.BranchRemovals.Count - removalCount);
            Manifest.PropertyEdits.RemoveAll(edit =>
                priorEdits.Any(prior =>
                    prior.ObjectId == edit.ObjectId &&
                    prior.PropertyKey == edit.PropertyKey));
            Manifest.PropertyEdits.AddRange(priorEdits);
            Manifest.ObjectDataReplacements.RemoveAll(replacement =>
                priorReplacements.Any(prior =>
                    prior.ObjectId == replacement.ObjectId));
            Manifest.ObjectDataReplacements.AddRange(priorReplacements);
            Manifest.CollisionLinkOverrides.Clear();
            Manifest.CollisionLinkOverrides.AddRange(priorCollisionLinks);
            throw;
        }
    }

    public bool CanRelocateInlineLeaf(
        int objectIndex,
        int targetOwnerIndex,
        out string reason) => CanRelocateInlineLeaf(
            objectIndex,
            targetOwnerIndex,
            existingRelocationObjectId: null,
            targetFieldType: null,
            targetFieldOccurrence: null,
            out reason);

    private bool CanRelocateInlineLeaf(
        int objectIndex,
        int targetOwnerIndex,
        uint? existingRelocationObjectId,
        int? targetFieldType,
        int? targetFieldOccurrence,
        out string reason)
    {
        if ((uint)objectIndex >= (uint)Manifest.Objects.Count ||
            (uint)targetOwnerIndex >= (uint)Manifest.Objects.Count)
        {
            reason = "The resource or target owner index is outside the project catalog.";
            return false;
        }
        SmoProjectObject resource = Manifest.Objects[objectIndex];
        if (resource.ParentIndex is not int sourceOwnerIndex)
        {
            reason = $"Resource [{objectIndex}] has no inline owner.";
            return false;
        }
        if (GetBranchObjects(resource).Count != 1)
        {
            reason = $"Resource [{objectIndex}] is not an inline leaf.";
            return false;
        }
        SmoProjectObject sourceOwner = Manifest.Objects[sourceOwnerIndex];
        if (!TryFindExactInlineField(sourceOwner, resource, out SmoProjectField? sourceField))
        {
            reason = $"Resource [{objectIndex}] has no exact inline field.";
            return false;
        }
        if (sourceOwnerIndex == targetOwnerIndex)
        {
            reason = "The target owner is already the resource owner.";
            return false;
        }
        SmoProjectObject targetOwner = Manifest.Objects[targetOwnerIndex];
        SmoProjectField[] references = FindReferenceFields(
            targetOwner,
            resource.Id,
            sourceField!.FieldType);
        SmoProjectField? targetReference;
        if (targetFieldType is int selectedType &&
            targetFieldOccurrence is int selectedOccurrence)
        {
            targetReference = references.SingleOrDefault(field =>
                field.FieldType == selectedType &&
                field.Occurrence == selectedOccurrence);
            if (targetReference is null)
            {
                reason = $"Target owner [{targetOwnerIndex}] has no matching " +
                    $"reference field {selectedType}/{selectedOccurrence}.";
                return false;
            }
        }
        else if (references.Length != 1)
        {
            reason = $"Target owner [{targetOwnerIndex}] has {references.Length} " +
                $"matching reference fields; exactly one is required.";
            return false;
        }
        else
        {
            targetReference = references[0];
        }
        if (Manifest.ResourceRelocations.Any(item =>
                item.ObjectId != existingRelocationObjectId &&
                (item.ObjectId == resource.Id ||
                 item.TargetOwnerId == targetOwner.Id &&
                 item.FieldType == sourceField.FieldType &&
                 item.FieldOccurrence == targetReference.Occurrence)))
        {
            reason = "The resource or target reference already participates in a relocation.";
            return false;
        }
        if (Manifest.PropertyEdits.Any(edit => edit.ObjectId == resource.Id))
        {
            reason = "Relocation of a resource with pending property edits is not supported yet.";
            return false;
        }
        reason = string.Empty;
        return true;
    }

    public void RelocateInlineLeaf(int objectIndex, int targetOwnerIndex)
    {
        if (!CanRelocateInlineLeaf(objectIndex, targetOwnerIndex, out string reason))
            throw new InvalidOperationException(reason);
        SmoProjectObject resource = Manifest.Objects[objectIndex];
        SmoProjectObject sourceOwner =
            Manifest.Objects[resource.ParentIndex!.Value];
        SmoProjectField sourceField =
            FindExactInlineField(sourceOwner, resource);
        SmoProjectObject targetOwner = Manifest.Objects[targetOwnerIndex];
        SmoProjectField targetField = FindReferenceFields(
            targetOwner,
            resource.Id,
            sourceField.FieldType).Single();
        Manifest.ResourceRelocations.Add(new SmoProjectResourceRelocation
        {
            ObjectId = resource.Id,
            TargetOwnerId = targetOwner.Id,
            FieldType = targetField.FieldType,
            FieldOccurrence = targetField.Occurrence
        });
        Validate();
    }

    private void RelocateInlineLeaf(
        int objectIndex,
        SmoProjectExternalReference reference)
    {
        if (!CanRelocateInlineLeaf(
                objectIndex,
                reference.Owner.Index,
                existingRelocationObjectId: null,
                reference.FieldType,
                reference.FieldOccurrence,
                out string reason))
        {
            throw new InvalidOperationException(reason);
        }
        SmoProjectObject resource = Manifest.Objects[objectIndex];
        Manifest.ResourceRelocations.Add(new SmoProjectResourceRelocation
        {
            ObjectId = resource.Id,
            TargetOwnerId = reference.Owner.Id,
            FieldType = reference.FieldType,
            FieldOccurrence = reference.FieldOccurrence
        });
        Validate();
    }

    public IReadOnlyList<uint> RelocateSharedLeavesForBranch(int objectIndex)
    {
        SmoProjectObject root = GetObject(objectIndex);
        HashSet<uint> originalBranchIds = GetBranchObjects(root)
            .Select(item => item.Id)
            .ToHashSet();
        HashSet<uint> remainingRemovedIds = GetEffectiveRemovedObjects(root)
            .Select(item => item.Id)
            .ToHashSet();
        // References to the entity root are registry links that disappear with
        // the entity. Only externally used descendants need a new owner.
        remainingRemovedIds.Remove(root.Id);
        var planned = new List<(int ObjectIndex, SmoProjectExternalReference Reference)>();
        while (FindExternalReferences(remainingRemovedIds).FirstOrDefault() is
               SmoProjectExternalReference reference)
        {
            SmoProjectObject resource = Manifest.Objects.Single(item =>
                item.Id == reference.ReferencedObjectId);
            if (!originalBranchIds.Contains(resource.Id))
            {
                throw new InvalidOperationException(
                    $"Shared object {reference.ReferencedObjectId} is not inside " +
                    $"branch [{objectIndex}].");
            }
            if (HasGeneratedOwnerCandidate(resource.Id))
            {
                // A generated placement can retain the exact resource ID and
                // become its new physical owner. Relocating it into an
                // unrelated imported consumer would put the new placement
                // outside its declared owner interval during serialization.
                remainingRemovedIds.Remove(resource.Id);
                continue;
            }
            if (!CanRelocateInlineLeaf(
                    resource.Index,
                    reference.Owner.Index,
                    existingRelocationObjectId: null,
                    reference.FieldType,
                    reference.FieldOccurrence,
                    out string reason))
            {
                throw new InvalidOperationException(
                    $"Shared object {reference.ReferencedObjectId} cannot be relocated: " +
                    reason);
            }
            planned.Add((resource.Index, reference));
            remainingRemovedIds.Remove(resource.Id);
        }

        int originalCount = Manifest.ResourceRelocations.Count;
        try
        {
            foreach ((int resourceIndex, SmoProjectExternalReference reference) in planned)
                RelocateInlineLeaf(resourceIndex, reference);
            return planned.Select(item => Manifest.Objects[item.ObjectIndex].Id).ToArray();
        }
        catch
        {
            Manifest.ResourceRelocations.RemoveRange(
                originalCount,
                Manifest.ResourceRelocations.Count - originalCount);
            throw;
        }
    }

    public void Validate()
    {
        if (!string.Equals(Manifest.Format, ProjectFormat, StringComparison.Ordinal))
            throw new InvalidDataException(
                $"Unsupported project marker {Manifest.Format}.");
        if (Manifest.FormatVersion != CurrentFormatVersion)
        {
            throw new InvalidDataException(
                $"Unsupported project version {Manifest.FormatVersion}; " +
                $"expected {CurrentFormatVersion}.");
        }
        if (Manifest.Header is null)
            throw new InvalidDataException("The project has no FFPS header metadata.");
        if (Manifest.DataLength != _dataSection.LongLength)
        {
            throw new InvalidDataException(
                $"The project declares {Manifest.DataLength} data bytes but contains " +
                $"{_dataSection.LongLength}.");
        }
        if (!string.Equals(
                Manifest.DataSha256,
                Hash(_dataSection),
                StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException(
                "The project data section does not match its SHA-256 digest.");
        }

        HashSet<uint> ids = [];
        uint previousOffset = 0;
        for (int index = 0; index < Manifest.Objects.Count; index++)
        {
            SmoProjectObject entry = Manifest.Objects[index];
            if (entry.Index != index)
                throw new InvalidDataException(
                    $"Project object {index} declares non-canonical index {entry.Index}.");
            if (!ids.Add(entry.Id))
                throw new InvalidDataException(
                    $"Project object ID {entry.Id} is duplicated.");
            if (entry.RawName.Length > ushort.MaxValue)
                throw new InvalidDataException(
                    $"Project object {index} name is too long for FFPS.");
            if (entry.RawName.Length > 0 && entry.RawName[^1] != 0)
                throw new InvalidDataException(
                    $"Project object {index} name is not NUL terminated.");
            if (index > 0 && entry.LogicalOffset < previousOffset)
                throw new InvalidDataException(
                    "Project object offsets are not monotonic.");
            previousOffset = entry.LogicalOffset;

            ulong end = (ulong)entry.LogicalOffset + entry.SerializedSize;
            if (entry.SerializedSize < 8 || end > (ulong)_dataSection.LongLength)
            {
                throw new InvalidDataException(
                    $"Project object {index} interval is outside data.bin.");
            }
            if (entry.ParentIndex is int parent &&
                ((uint)parent >= (uint)index || parent < 0))
            {
                throw new InvalidDataException(
                    $"Project object {index} has invalid parent index {parent}.");
            }

            ReadOnlySpan<byte> objectStart = _dataSection.AsSpan(
                checked((int)entry.LogicalOffset), 8);
            uint storedType = BinaryPrimitives.ReadUInt32LittleEndian(objectStart);
            if (storedType != entry.TypeHash ||
                !objectStart[4..8].SequenceEqual("SBOO"u8))
            {
                throw new InvalidDataException(
                    $"Project object {index} does not point at its declared SBOO payload.");
            }
        }

        ValidateIntervalsAndParents();
        ValidateEdits();
        ValidatePlacementEditPairs();
        ValidateResourceRelocations();
        ValidateResourceRedirects();
        ValidateBranchRemovals();
        ValidateReferencePlacements();
        _ = PlanGeneratedResourcePromotions();
        ValidateCollisionLinkOverrides();
        ValidateAddedForests();
        ValidateRemovedAddedForests();
        ValidateAddedObjectEdits();
        ValidateObjectDataReplacements();
    }

    private HashSet<uint> GetAllObjectIds() => Manifest.Objects
        .Select(item => item.Id)
        .Concat(Manifest.ReferencePlacements.SelectMany(item => item.NewObjectIds))
        .Concat(Manifest.AddedForests.SelectMany(item =>
            item.Objects.Select(objectEntry => objectEntry.Id)))
        .ToHashSet();

    private bool IsActiveProjectObjectId(uint objectId)
    {
        if (Manifest.ReferencePlacements.Any(item =>
                item.NewObjectIds.Contains(objectId)))
        {
            return true;
        }
        if (Manifest.AddedForests.Any(forest =>
                !Manifest.RemovedAddedForestIds.Contains(forest.BlobId) &&
                forest.Objects.Any(item => item.Id == objectId)))
        {
            return true;
        }
        SmoProjectObject? imported = Manifest.Objects.SingleOrDefault(item =>
            item.Id == objectId);
        return imported is not null && !IsImportedObjectPendingRemoval(imported);
    }

    private void ValidateCollisionLinkOverrides()
    {
        var pairs = new HashSet<(uint VisualObjectId, uint CollisionObjectId)>();
        foreach (SmoProjectCollisionLinkOverride link in
                 Manifest.CollisionLinkOverrides)
        {
            if (link.VisualObjectId == link.CollisionObjectId)
                throw new InvalidDataException("A project collision link points to one object twice.");
            if (!pairs.Add((link.VisualObjectId, link.CollisionObjectId)))
            {
                throw new InvalidDataException(
                    $"Project collision link {link.VisualObjectId}/" +
                    $"{link.CollisionObjectId} is duplicated.");
            }
            if (!IsActiveProjectObjectId(link.VisualObjectId) ||
                !IsActiveProjectObjectId(link.CollisionObjectId))
            {
                throw new InvalidDataException(
                    $"Project collision link {link.VisualObjectId}/" +
                    $"{link.CollisionObjectId} points outside the active graph.");
            }
        }
    }

    private void RemoveCollisionLinkOverridesForIds(IEnumerable<uint> objectIds)
    {
        HashSet<uint> removed = objectIds.ToHashSet();
        if (removed.Count == 0)
            return;
        Manifest.CollisionLinkOverrides.RemoveAll(item =>
            removed.Contains(item.VisualObjectId) ||
            removed.Contains(item.CollisionObjectId));
    }

    private void RemapCollisionLinkObjectId(uint sourceObjectId, uint targetObjectId)
    {
        foreach (SmoProjectCollisionLinkOverride link in
                 Manifest.CollisionLinkOverrides)
        {
            if (link.VisualObjectId == sourceObjectId)
                link.VisualObjectId = targetObjectId;
            if (link.CollisionObjectId == sourceObjectId)
                link.CollisionObjectId = targetObjectId;
        }
        Manifest.CollisionLinkOverrides.RemoveAll(item =>
            item.VisualObjectId == item.CollisionObjectId);
        SmoProjectCollisionLinkOverride[] distinct = Manifest.CollisionLinkOverrides
            .GroupBy(item => (item.VisualObjectId, item.CollisionObjectId))
            .Select(group => group.Last())
            .ToArray();
        Manifest.CollisionLinkOverrides.Clear();
        Manifest.CollisionLinkOverrides.AddRange(distinct);
    }

    private bool TryGetKnownObjectType(uint objectId, out uint typeHash)
    {
        SmoProjectObject? imported = Manifest.Objects.SingleOrDefault(item =>
            item.Id == objectId);
        if (imported is not null)
        {
            typeHash = imported.TypeHash;
            return true;
        }
        SmoProjectAddedObject? added = Manifest.AddedForests
            .Where(forest => !Manifest.RemovedAddedForestIds.Contains(forest.BlobId))
            .SelectMany(item => item.Objects)
            .SingleOrDefault(item => item.Id == objectId);
        typeHash = added?.TypeHash ?? 0;
        return added is not null;
    }

    private void ValidateReferencePlacements()
    {
        HashSet<uint> allIds = Manifest.Objects.Select(item => item.Id).ToHashSet();
        foreach (SmoProjectReferencePlacement placement in
                 Manifest.ReferencePlacements)
        {
            SmoProjectObject template = Manifest.Objects.SingleOrDefault(item =>
                    item.Id == placement.TemplateRootObjectId)
                ?? throw new InvalidDataException(
                    $"Reference placement template {placement.TemplateRootObjectId} is missing.");
            SmoProjectObject targetOwner = Manifest.Objects.SingleOrDefault(item =>
                    item.Id == placement.TargetOwnerId)
                ?? throw new InvalidDataException(
                    $"Reference placement owner {placement.TargetOwnerId} is missing.");
            bool validShell = placement.ResourceObjectIdOverride is uint shellResourceId
                ? CanCreateReferencePlacementShell(
                    template,
                    ResolveResourceRedirect(shellResourceId),
                    out string reason)
                : CanAddReferencePlacement(template.Index, out reason);
            if (!validShell)
                throw new InvalidDataException(reason);
            bool usesTemplateOwner = template.ParentIndex is int parentIndex &&
                Manifest.Objects[parentIndex].Id == targetOwner.Id;
            bool usesProjectResourceOwner =
                placement.ResourceObjectIdOverride is uint ownedResourceId &&
                Manifest.AddedForests
                    .Where(item =>
                        !Manifest.RemovedAddedForestIds.Contains(item.BlobId))
                    .Any(item =>
                        item.TargetOwnerId == targetOwner.Id &&
                        item.Objects.Any(entry => entry.Id == ownedResourceId));
            if (!usesTemplateOwner && !usesProjectResourceOwner)
            {
                throw new InvalidDataException(
                    "Reference placement must use either the template owner or " +
                    "the active project forest which owns its overridden resource.");
            }
            IReadOnlyList<SmoProjectObject> branch = GetReferencePlacementShellObjects(
                template,
                placement.ResourceObjectIdOverride);
            if (placement.NewObjectIds.Count != branch.Count)
            {
                throw new InvalidDataException(
                    $"Reference placement {placement.TemplateRootObjectId} declares " +
                    $"{placement.NewObjectIds.Count} IDs for {branch.Count} branch objects.");
            }
            foreach (uint id in placement.NewObjectIds)
            {
                if (id == 0 || !allIds.Add(id))
                    throw new InvalidDataException(
                        $"Reference placement object ID {id} is zero or duplicated.");
            }
            if (placement.RootRawName.Length == 0 ||
                placement.RootRawName.Length > ushort.MaxValue ||
                placement.RootRawName[^1] != 0)
            {
                throw new InvalidDataException(
                    "Reference placement root name must be a non-empty NUL-terminated FFPS name.");
            }
            ValidatePlacementMatrixPair(
                placement.WorldMatrix,
                placement.InverseWorldMatrix,
                placement.NewObjectIds[0]);
            if (placement.ResourceObjectIdOverride is uint resourceId &&
                (!TryGetKnownObjectType(resourceId, out uint resourceType) ||
                 resourceType != SmoClassIds.MeshData))
            {
                throw new InvalidDataException(
                    $"Reference placement mesh override {resourceId} is missing or not spMeshData.");
            }

            bool ownerRemoved = Manifest.BranchRemovals.Any(removal =>
            {
                SmoProjectObject removed = Manifest.Objects.Single(item =>
                    item.Id == removal.RootObjectId);
                return Contains(removed, targetOwner);
            });
            if (ownerRemoved)
                throw new InvalidDataException(
                    $"Reference placement owner {targetOwner.Id} is pending removal.");
        }
    }

    /// <summary>
    /// Chooses a deterministic generated placement which takes ownership of an
    /// imported physical mesh when the original placement is removed. The mesh
    /// keeps its original object ID; only its inline owner moves. All remaining
    /// generated placements therefore continue to use the same reference.
    /// </summary>
    private Dictionary<uint, uint> PlanGeneratedResourcePromotions()
    {
        HashSet<uint> removedMeshIds = Manifest.BranchRemovals
            .Select(removal => Manifest.Objects.Single(item =>
                item.Id == removal.RootObjectId))
            // Include resources which an older project revision may already
            // have scheduled for relocation. A generated placement is the
            // safer owner when the original physical branch itself is gone:
            // it keeps placement bytes inside their declared target owner.
            .SelectMany(GetBranchObjects)
            .Where(item => item.TypeHash == SmoClassIds.MeshData)
            .Select(item => item.Id)
            .ToHashSet();
        var result = new Dictionary<uint, uint>();
        foreach (uint meshId in removedMeshIds.OrderBy(item => item))
        {
            SmoProjectReferencePlacement[] consumers = Manifest.ReferencePlacements
                .Where(placement =>
                    placement.ResourceObjectIdOverride is uint resourceId &&
                    ResolveResourceRedirect(resourceId) == meshId)
                .ToArray();
            if (consumers.Length == 0)
                continue;

            SmoProjectReferencePlacement? promoter = consumers.FirstOrDefault(placement =>
            {
                SmoProjectObject template = Manifest.Objects.Single(item =>
                    item.Id == placement.TemplateRootObjectId);
                return Manifest.Objects.Any(item =>
                    item.Id == meshId && Contains(template, item));
            });
            if (promoter is null)
            {
                throw new InvalidDataException(
                    $"Removed physical mesh {meshId} is still used by " +
                    $"{consumers.Length} generated placement(s), but none of their " +
                    "templates can become its inline owner.");
            }
            result.Add(promoter.NewObjectIds[0], meshId);
        }
        return result;
    }

    private bool HasGeneratedOwnerCandidate(uint resourceId) =>
        Manifest.ReferencePlacements.Any(placement =>
        {
            if (placement.ResourceObjectIdOverride is not uint overrideId ||
                ResolveResourceRedirect(overrideId) != resourceId)
            {
                return false;
            }
            SmoProjectObject template = Manifest.Objects.Single(item =>
                item.Id == placement.TemplateRootObjectId);
            return Manifest.Objects.Any(item =>
                item.Id == resourceId && Contains(template, item));
        });

    private void ValidateAddedForests()
    {
        HashSet<uint> allIds = Manifest.Objects.Select(item => item.Id)
            .Concat(Manifest.ReferencePlacements.SelectMany(item => item.NewObjectIds))
            .ToHashSet();
        HashSet<Guid> blobIds = [];
        foreach (SmoProjectAddedForest forest in Manifest.AddedForests)
        {
            if (forest.BlobId == Guid.Empty || !blobIds.Add(forest.BlobId))
            {
                throw new InvalidDataException(
                    $"Added forest asset {forest.BlobId:N} is empty or duplicated.");
            }
            if (!_assetBlobs.TryGetValue(forest.BlobId, out byte[]? blob))
            {
                throw new InvalidDataException(
                    $"Added forest asset {forest.BlobId:N} is missing.");
            }
            if (!string.Equals(forest.BlobSha256, Hash(blob),
                    StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException(
                    $"Added forest asset {forest.BlobId:N} failed its SHA-256 check.");
            }
            SmoProjectObject target = Manifest.Objects.SingleOrDefault(item =>
                    item.Id == forest.TargetOwnerId)
                ?? throw new InvalidDataException(
                    $"Added forest owner {forest.TargetOwnerId} is missing.");
            if (!target.FieldStreamDecoded)
            {
                throw new InvalidDataException(
                    $"Added forest owner {target.Index} has no decoded field stream.");
            }
            bool directFieldBoundary = target.Fields.Any(field =>
                field.RelativeHeaderOffset == forest.InsertionRelativeOffset);
            if (!directFieldBoundary)
            {
                throw new InvalidDataException(
                    $"Added forest for owner {target.Index} must be inserted at a " +
                    "direct field boundary.");
            }
            bool ownerRemoved = Manifest.BranchRemovals.Any(removal =>
            {
                SmoProjectObject removed = Manifest.Objects.Single(item =>
                    item.Id == removal.RootObjectId);
                return Contains(removed, target);
            });
            if (ownerRemoved)
            {
                throw new InvalidDataException(
                    $"Added forest owner {target.Id} is pending removal.");
            }
            var byId = new Dictionary<uint, SmoProjectAddedObject>();
            foreach (SmoProjectAddedObject entry in forest.Objects)
            {
                if (entry.Id == 0 || !allIds.Add(entry.Id) || !byId.TryAdd(entry.Id, entry))
                {
                    throw new InvalidDataException(
                        $"Added forest object ID {entry.Id} is zero or duplicated.");
                }
                if (entry.RawName.Length > ushort.MaxValue ||
                    entry.RawName.Length > 0 && entry.RawName[^1] != 0)
                {
                    throw new InvalidDataException(
                        $"Added forest object {entry.Id} has an invalid FFPS name.");
                }
                ulong start = entry.ObjectRelativeOffset;
                ulong end = start + entry.SerializedSize;
                if (entry.SerializedSize < 8 || start < 8 || end > (ulong)blob.LongLength)
                {
                    throw new InvalidDataException(
                        $"Added forest object {entry.Id} leaves its asset interval.");
                }
                int objectOffset = checked((int)entry.ObjectRelativeOffset);
                ReadOnlySpan<byte> prefix = blob.AsSpan(objectOffset - 8, 8);
                if (BinaryPrimitives.ReadUInt32LittleEndian(prefix) != entry.Id ||
                    BinaryPrimitives.ReadUInt32LittleEndian(prefix[4..]) !=
                        entry.SerializedSize)
                {
                    throw new InvalidDataException(
                        $"Added forest object {entry.Id} has no matching inline prefix.");
                }
                ReadOnlySpan<byte> objectStart = blob.AsSpan(objectOffset, 8);
                if (BinaryPrimitives.ReadUInt32LittleEndian(objectStart) != entry.TypeHash ||
                    !objectStart[4..8].SequenceEqual("SBOO"u8))
                {
                    throw new InvalidDataException(
                        $"Added forest object {entry.Id} does not point at its declared SBOO.");
                }
            }

            bool hasRoot = false;
            var ownershipStack = new List<SmoProjectAddedObject>();
            foreach (SmoProjectAddedObject entry in forest.Objects
                         .OrderBy(item => item.ObjectRelativeOffset)
                         .ThenByDescending(item => item.SerializedSize))
            {
                ulong start = entry.ObjectRelativeOffset;
                ulong end = start + entry.SerializedSize;
                while (ownershipStack.Count > 0 && start >=
                       (ulong)ownershipStack[^1].ObjectRelativeOffset +
                       ownershipStack[^1].SerializedSize)
                {
                    ownershipStack.RemoveAt(ownershipStack.Count - 1);
                }
                uint expectedParent = ownershipStack.Count == 0
                    ? target.Id
                    : ownershipStack[^1].Id;
                if (entry.ParentObjectId != expectedParent)
                {
                    throw new InvalidDataException(
                        $"Added forest object {entry.Id} declares parent " +
                        $"{entry.ParentObjectId?.ToString() ?? "NONE"}, but its " +
                        $"interval owner is {expectedParent}.");
                }
                if (ownershipStack.Count == 0)
                {
                    hasRoot = true;
                }
                else
                {
                    SmoProjectAddedObject parent = ownershipStack[^1];
                    ulong parentEnd = (ulong)parent.ObjectRelativeOffset +
                                      parent.SerializedSize;
                    if (end > parentEnd)
                    {
                        throw new InvalidDataException(
                            $"Added forest object {entry.Id} partially overlaps its owner.");
                    }
                }
                ownershipStack.Add(entry);
            }
            if (forest.Objects.Count > 0 && !hasRoot)
            {
                throw new InvalidDataException(
                    $"Added forest asset {forest.BlobId:N} has no root under its owner.");
            }
        }
    }

    private void ValidateAddedObjectEdits()
    {
        HashSet<(Guid BlobId, uint ObjectId, string PropertyKey)> unique = [];
        foreach (SmoProjectAddedObjectPropertyEdit edit in
                 Manifest.AddedObjectPropertyEdits)
        {
            if (!unique.Add((edit.BlobId, edit.ObjectId, edit.PropertyKey)))
            {
                throw new InvalidDataException(
                    $"Added-object property edit {edit.ObjectId}:{edit.PropertyKey} is duplicated.");
            }
            SmoProjectAddedForest forest = Manifest.AddedForests
                .SingleOrDefault(item => item.BlobId == edit.BlobId)
                ?? throw new InvalidDataException(
                    $"Added-object edit references missing asset {edit.BlobId:N}.");
            SmoProjectAddedObject entry = forest.Objects
                .SingleOrDefault(item => item.Id == edit.ObjectId)
                ?? throw new InvalidDataException(
                    $"Added-object edit references missing object ID {edit.ObjectId}.");
            SmoPropertyDescriptor descriptor = GetPropertyDescriptor(
                entry.TypeHash,
                edit.PropertyKey,
                edit.ValueKind,
                $"added object {entry.Id}");
            SmoObjectField? field = ReadAddedObjectFields(forest, entry)
                .SingleOrDefault(descriptor.Field.Matches);
            if (field is null)
            {
                throw new InvalidDataException(
                    $"Required property {entry.Id}:{edit.PropertyKey} is absent from its asset.");
            }
            if (edit.Value.Length != descriptor.ValueSize ||
                (long)descriptor.PayloadOffset + descriptor.ValueSize > field.PayloadSize)
            {
                throw new InvalidDataException(
                    $"Added-object property edit {entry.Id}:{edit.PropertyKey} has an invalid size.");
            }
        }
        ValidateAddedPlacementEditPairs();
    }

    private void ValidatePlacementEditPairs()
    {
        foreach (IGrouping<uint, SmoProjectPropertyEdit> group in
                 Manifest.PropertyEdits.GroupBy(item => item.ObjectId))
        {
            SmoProjectObject entry = Manifest.Objects.Single(item =>
                item.Id == group.Key);
            if (entry.TypeHash != SmoClassIds.StaticRenderObject)
                continue;
            SmoProjectPropertyEdit? world = group.SingleOrDefault(item =>
                item.PropertyKey == SmoPropertyKeys.WorldMatrix);
            SmoProjectPropertyEdit? inverse = group.SingleOrDefault(item =>
                item.PropertyKey == SmoPropertyKeys.InverseWorldMatrix);
            if (world is null && inverse is null)
                continue;
            if (world is null || inverse is null)
            {
                throw new InvalidDataException(
                    $"Static placement {entry.Id} must edit world and inverse " +
                    "matrices as one pair.");
            }
            ValidatePlacementMatrixPair(world.Value, inverse.Value, entry.Id);
        }
    }

    private void ValidateAddedPlacementEditPairs()
    {
        foreach (IGrouping<(Guid BlobId, uint ObjectId),
                     SmoProjectAddedObjectPropertyEdit> group in
                 Manifest.AddedObjectPropertyEdits.GroupBy(item =>
                     (item.BlobId, item.ObjectId)))
        {
            SmoProjectAddedForest forest = Manifest.AddedForests.Single(item =>
                item.BlobId == group.Key.BlobId);
            SmoProjectAddedObject entry = forest.Objects.Single(item =>
                item.Id == group.Key.ObjectId);
            if (entry.TypeHash != SmoClassIds.StaticRenderObject)
                continue;
            SmoProjectAddedObjectPropertyEdit? world = group.SingleOrDefault(item =>
                item.PropertyKey == SmoPropertyKeys.WorldMatrix);
            SmoProjectAddedObjectPropertyEdit? inverse = group.SingleOrDefault(item =>
                item.PropertyKey == SmoPropertyKeys.InverseWorldMatrix);
            if (world is null && inverse is null)
                continue;
            if (world is null || inverse is null)
            {
                throw new InvalidDataException(
                    $"Added static placement {entry.Id} must edit world and inverse " +
                    "matrices as one pair.");
            }
            ValidatePlacementMatrixPair(world.Value, inverse.Value, entry.Id);
        }
    }

    private void ValidateRemovedAddedForests()
    {
        if (Manifest.RemovedAddedForestIds.Count !=
            Manifest.RemovedAddedForestIds.Distinct().Count())
        {
            throw new InvalidDataException(
                "Removed added-forest IDs contain duplicates.");
        }
        HashSet<Guid> known = Manifest.AddedForests
            .Select(item => item.BlobId)
            .ToHashSet();
        Guid missing = Manifest.RemovedAddedForestIds.FirstOrDefault(id =>
            !known.Contains(id));
        if (missing != Guid.Empty)
        {
            throw new InvalidDataException(
                $"Removed added forest {missing:N} is missing from the project.");
        }
    }

    private void ValidateObjectDataReplacements()
    {
        HashSet<uint> unique = [];
        foreach (SmoProjectObjectDataReplacement replacement in
                 Manifest.ObjectDataReplacements)
        {
            if (!unique.Add(replacement.ObjectId))
            {
                throw new InvalidDataException(
                    $"Object data replacement {replacement.ObjectId} is duplicated.");
            }
            if (!TryGetKnownObjectType(replacement.ObjectId, out uint typeHash))
            {
                throw new InvalidDataException(
                    $"Object data replacement {replacement.ObjectId} targets a missing object.");
            }
            if (!_assetBlobs.TryGetValue(replacement.BlobId, out byte[]? blob))
            {
                throw new InvalidDataException(
                    $"Object data replacement asset {replacement.BlobId:N} is missing.");
            }
            if (!string.Equals(replacement.BlobSha256, Hash(blob),
                    StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException(
                    $"Object data replacement asset {replacement.BlobId:N} failed its SHA-256 check.");
            }
            if (blob.Length != checked((int)GetKnownObjectSize(replacement.ObjectId)) ||
                blob.Length < 8 ||
                BinaryPrimitives.ReadUInt32LittleEndian(blob) != typeHash ||
                !blob.AsSpan(4, 4).SequenceEqual("SBOO"u8))
            {
                throw new InvalidDataException(
                    $"Object data replacement {replacement.ObjectId} does not preserve its object layout.");
            }
            if (Manifest.PropertyEdits.Any(item => item.ObjectId == replacement.ObjectId) ||
                Manifest.AddedObjectPropertyEdits.Any(item =>
                    item.ObjectId == replacement.ObjectId))
            {
                throw new InvalidDataException(
                    $"Object {replacement.ObjectId} cannot have both a whole-data replacement " +
                    "and schema property edits in the same project state.");
            }
        }
    }

    private void SetAddedObjectProperty(
        SmoProjectAddedForest forest,
        SmoProjectAddedObject entry,
        string propertyKey,
        SmoPropertyValueKind valueKind,
        byte[] value)
    {
        SmoPropertyDescriptor descriptor = GetPropertyDescriptor(
            entry.TypeHash,
            propertyKey,
            valueKind,
            $"added object {entry.Id}");
        SmoObjectField? field = ReadAddedObjectFields(forest, entry)
            .SingleOrDefault(descriptor.Field.Matches);
        if (field is null ||
            (long)descriptor.PayloadOffset + descriptor.ValueSize > field.PayloadSize)
        {
            throw new InvalidDataException(
                $"Added object {entry.Id} has no writable {propertyKey} field.");
        }
        if (value.Length != descriptor.ValueSize)
            throw new ArgumentException("Encoded property size does not match its schema.");
        Manifest.AddedObjectPropertyEdits.RemoveAll(item =>
            item.BlobId == forest.BlobId &&
            item.ObjectId == entry.Id &&
            item.PropertyKey == propertyKey);
        Manifest.AddedObjectPropertyEdits.Add(
            new SmoProjectAddedObjectPropertyEdit
            {
                BlobId = forest.BlobId,
                ObjectId = entry.Id,
                PropertyKey = propertyKey,
                ValueKind = valueKind,
                Value = value.ToArray()
            });
    }

    private byte[] BuildAddedForestBlob(SmoProjectAddedForest forest)
    {
        byte[] source = GetAssetBlobBytes(forest.BlobId);
        SmoProjectAddedObjectPropertyEdit[] edits =
            Manifest.AddedObjectPropertyEdits
                .Where(item => item.BlobId == forest.BlobId)
                .ToArray();
        SmoProjectObjectDataReplacement[] replacements =
            Manifest.ObjectDataReplacements
                .Where(item => forest.Objects.Any(entry =>
                    entry.Id == item.ObjectId))
                .ToArray();
        if (edits.Length == 0 && replacements.Length == 0 &&
            Manifest.ResourceRedirects.Count == 0)
            return source;
        byte[] result = source.ToArray();
        foreach (SmoProjectObjectDataReplacement replacement in replacements)
        {
            SmoProjectAddedObject entry = forest.Objects.Single(item =>
                item.Id == replacement.ObjectId);
            GetAssetBlob(replacement.BlobId).Span.CopyTo(result.AsSpan(
                checked((int)entry.ObjectRelativeOffset),
                checked((int)entry.SerializedSize)));
        }
        foreach (SmoProjectAddedObjectPropertyEdit edit in edits)
        {
            SmoProjectAddedObject entry = forest.Objects.Single(item =>
                item.Id == edit.ObjectId);
            SmoPropertyDescriptor descriptor = GetPropertyDescriptor(
                entry.TypeHash,
                edit.PropertyKey,
                edit.ValueKind,
                $"added object {entry.Id}");
            SmoObjectField field = ReadAddedObjectFields(forest, entry)
                .Single(descriptor.Field.Matches);
            int offset = checked(
                (int)entry.ObjectRelativeOffset +
                field.RelativePayloadOffset +
                descriptor.PayloadOffset);
            edit.Value.CopyTo(result, offset);
        }
        foreach (SmoProjectAddedObject entry in forest.Objects)
        foreach (SmoObjectField field in ReadAddedObjectFields(forest, entry)
                     .Where(item => item.PayloadSize == 8))
        {
            int offset = checked(
                (int)entry.ObjectRelativeOffset + field.RelativePayloadOffset);
            uint referencedId = BinaryPrimitives.ReadUInt32LittleEndian(
                source.AsSpan(offset));
            uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(
                source.AsSpan(offset + sizeof(uint)));
            uint redirectedId = ResolveResourceRedirect(referencedId);
            if (inlineSize == 0 && redirectedId != referencedId)
            {
                BinaryPrimitives.WriteUInt32LittleEndian(
                    result.AsSpan(offset),
                    redirectedId);
            }
        }
        return result;
    }

    private IReadOnlyList<SmoObjectField> ReadAddedObjectFields(
        SmoProjectAddedForest forest,
        SmoProjectAddedObject entry)
    {
        byte[] blob = GetAssetBlobBytes(forest.BlobId);
        int objectOffset = checked((int)entry.ObjectRelativeOffset);
        int objectLength = checked((int)entry.SerializedSize);
        ReadOnlySpan<byte> serialized = blob.AsSpan(objectOffset, objectLength);
        var fields = new List<SmoObjectField>();
        var occurrences = new Dictionary<int, int>();
        int offset = 8;
        while (offset < serialized.Length)
        {
            if (!SmoDataBlockReader.TryReadHeader(
                    serialized,
                    offset,
                    out SmoDataBlockHeader header))
            {
                throw new InvalidDataException(
                    $"Added object {entry.Id} has an unreadable field at 0x{offset:X}.");
            }
            int occurrence = occurrences.GetValueOrDefault(header.FieldType);
            occurrences[header.FieldType] = occurrence + 1;
            fields.Add(new SmoObjectField(
                -1,
                entry.Id,
                header.FieldType,
                occurrence,
                header.RawHeader,
                header.SizeKind,
                header.HeaderSize,
                header.PayloadSize,
                header.Offset,
                header.PayloadOffset,
                checked(objectOffset + header.Offset),
                checked(objectOffset + header.PayloadOffset),
                blob.AsMemory(
                    checked(objectOffset + header.PayloadOffset),
                    checked((int)header.PayloadSize))));
            offset = checked((int)header.PayloadEnd);
        }
        if (offset != serialized.Length)
            throw new InvalidDataException($"Added object {entry.Id} field stream is incomplete.");
        return fields;
    }

    private void ValidateResourceRelocations()
    {
        HashSet<uint> resources = [];
        HashSet<(uint OwnerId, int FieldType, int Occurrence)> targets = [];
        foreach (SmoProjectResourceRelocation relocation in
                 Manifest.ResourceRelocations)
        {
            if (!resources.Add(relocation.ObjectId))
                throw new InvalidDataException(
                    $"Resource relocation {relocation.ObjectId} is duplicated.");
            if (!targets.Add((relocation.TargetOwnerId, relocation.FieldType,
                    relocation.FieldOccurrence)))
            {
                throw new InvalidDataException(
                    $"Several relocations target the same owner field " +
                    $"{relocation.TargetOwnerId}:{relocation.FieldType}/" +
                    $"{relocation.FieldOccurrence}.");
            }
            SmoProjectObject resource = Manifest.Objects.SingleOrDefault(item =>
                    item.Id == relocation.ObjectId)
                ?? throw new InvalidDataException(
                    $"Relocation references missing resource ID {relocation.ObjectId}.");
            SmoProjectObject target = Manifest.Objects.SingleOrDefault(item =>
                    item.Id == relocation.TargetOwnerId)
                ?? throw new InvalidDataException(
                    $"Relocation references missing target owner ID " +
                    $"{relocation.TargetOwnerId}.");
            if (!CanRelocateInlineLeaf(
                    resource.Index,
                    target.Index,
                    relocation.ObjectId,
                    relocation.FieldType,
                    relocation.FieldOccurrence,
                    out string reason))
                throw new InvalidDataException(reason);
            SmoProjectObject source = Manifest.Objects[resource.ParentIndex!.Value];
            SmoProjectField sourceField = FindExactInlineField(source, resource);
            SmoProjectField targetField = target.Fields.SingleOrDefault(field =>
                    field.FieldType == relocation.FieldType &&
                    field.Occurrence == relocation.FieldOccurrence)
                ?? throw new InvalidDataException(
                    $"Relocation target field is missing from owner {target.Index}.");
            if (sourceField.FieldType != relocation.FieldType ||
                !IsReferenceField(target, targetField, resource.Id))
            {
                throw new InvalidDataException(
                    $"Relocation {resource.Id} does not connect matching inline and " +
                    "reference fields.");
            }
        }
    }

    private void ValidateResourceRedirects()
    {
        var bySource = new Dictionary<uint, uint>();
        foreach (SmoProjectResourceRedirect redirect in
                 Manifest.ResourceRedirects)
        {
            if (!bySource.TryAdd(redirect.SourceObjectId, redirect.TargetObjectId))
            {
                throw new InvalidDataException(
                    $"Resource redirect {redirect.SourceObjectId} is duplicated.");
            }
            SmoProjectObject source = Manifest.Objects.SingleOrDefault(item =>
                    item.Id == redirect.SourceObjectId)
                ?? throw new InvalidDataException(
                    $"Redirect source {redirect.SourceObjectId} is not an imported object.");
            if (source.ParentIndex is not int parentIndex ||
                !TryFindExactInlineField(Manifest.Objects[parentIndex], source, out _))
            {
                throw new InvalidDataException(
                    $"Redirect source {redirect.SourceObjectId} is not inline-owned.");
            }
            if (!TryGetKnownObjectType(redirect.TargetObjectId, out uint targetType))
            {
                throw new InvalidDataException(
                    $"Redirect target {redirect.TargetObjectId} is missing.");
            }
            if (source.TypeHash != targetType)
            {
                throw new InvalidDataException(
                    $"Redirect {redirect.SourceObjectId}->{redirect.TargetObjectId} " +
                    "connects different object types.");
            }
            if (GetBranchObjects(source).Any(item => item.Id == redirect.TargetObjectId))
            {
                throw new InvalidDataException(
                    $"Redirect target {redirect.TargetObjectId} is inside its source branch.");
            }
            if (Manifest.BranchRemovals.Any(removal =>
            {
                SmoProjectObject removed = Manifest.Objects.Single(item =>
                    item.Id == removal.RootObjectId);
                return Contains(removed, source) || Contains(source, removed) ||
                       Manifest.Objects.Any(item =>
                           item.Id == redirect.TargetObjectId && Contains(removed, item));
            }))
            {
                throw new InvalidDataException(
                    $"Redirect {redirect.SourceObjectId}->{redirect.TargetObjectId} " +
                    "overlaps a pending branch removal.");
            }
            SmoProjectResourceRedirect? overlapping =
                Manifest.ResourceRedirects.FirstOrDefault(other =>
                {
                    if (ReferenceEquals(other, redirect))
                        return false;
                    SmoProjectObject otherSource = Manifest.Objects.Single(item =>
                        item.Id == other.SourceObjectId);
                    return Contains(source, otherSource) || Contains(otherSource, source);
                });
            if (overlapping is not null)
            {
                throw new InvalidDataException(
                    $"Redirect source branches {redirect.SourceObjectId} and " +
                    $"{overlapping.SourceObjectId} overlap.");
            }
            HashSet<uint> removedIds = GetEffectiveRemovedObjects(source)
                .Select(item => item.Id)
                .ToHashSet();
            removedIds.Remove(source.Id);
            SmoProjectExternalReference? descendantReference =
                FindExternalReferences(removedIds).FirstOrDefault();
            if (descendantReference is not null)
            {
                throw new InvalidDataException(
                    $"Redirect source {source.Id} still owns shared descendant " +
                    $"{descendantReference.ReferencedObjectId} used by " +
                    $"{descendantReference.Owner.Id}.");
            }
            if (Manifest.PropertyEdits.Any(edit =>
                    GetEffectiveRemovedObjects(source).Any(item => item.Id == edit.ObjectId)))
            {
                throw new InvalidDataException(
                    $"Redirect source {source.Id} still contains property edits.");
            }
        }

        foreach (uint sourceId in bySource.Keys)
        {
            var visited = new HashSet<uint>();
            uint current = sourceId;
            while (bySource.TryGetValue(current, out uint next))
            {
                if (!visited.Add(current))
                    throw new InvalidDataException("Resource redirects contain a cycle.");
                current = next;
            }
        }
    }

    private void ValidateBranchRemovals()
    {
        HashSet<uint> roots = [];
        var intervals = new List<SmoProjectObject>();
        foreach (SmoProjectBranchRemoval removal in Manifest.BranchRemovals)
        {
            if (!roots.Add(removal.RootObjectId))
                throw new InvalidDataException(
                    $"Project branch removal {removal.RootObjectId} is duplicated.");
            SmoProjectObject root = Manifest.Objects.SingleOrDefault(item =>
                    item.Id == removal.RootObjectId)
                ?? throw new InvalidDataException(
                    $"Branch removal references missing object ID {removal.RootObjectId}.");
            if (root.ParentIndex is not int parentIndex)
                throw new InvalidDataException(
                    $"Branch removal {removal.RootObjectId} targets a root object.");
            if (!TryFindExactInlineField(Manifest.Objects[parentIndex], root, out _))
                throw new InvalidDataException(
                    $"Branch removal {removal.RootObjectId} has no exact inline owner field.");
            if (intervals.Any(other => Contains(other, root) || Contains(root, other)))
                throw new InvalidDataException(
                    $"Branch removal {removal.RootObjectId} overlaps another removal.");
            intervals.Add(root);

            HashSet<uint> branchIds = GetEffectiveRemovedObjects(root)
                .Select(item => item.Id)
                .ToHashSet();
            SmoProjectExternalReference? externalReference =
                FindExternalReferences(branchIds).FirstOrDefault(reference =>
                    reference.ReferencedObjectId != root.Id &&
                    !HasGeneratedOwnerCandidate(reference.ReferencedObjectId) &&
                    !Manifest.BranchRemovals.Any(other =>
                    {
                        if (other.RootObjectId == root.Id)
                            return false;
                        SmoProjectObject otherRoot =
                            Manifest.Objects.Single(item =>
                                item.Id == other.RootObjectId);
                        return Contains(otherRoot, reference.Owner);
                    }));
            if (externalReference is not null)
            {
                throw new InvalidDataException(
                    $"Branch removal {removal.RootObjectId} would leave an external " +
                    $"reference in object {externalReference.Owner.Index}.");
            }
            if (Manifest.PropertyEdits.Any(edit =>
                    branchIds.Contains(edit.ObjectId) &&
                    !HasGeneratedOwnerCandidate(edit.ObjectId)))
                throw new InvalidDataException(
                    $"Branch removal {removal.RootObjectId} still contains property edits.");
        }
    }

    private void ValidateEdits()
    {
        HashSet<(uint ObjectId, string PropertyKey)> unique = [];
        foreach (SmoProjectPropertyEdit edit in Manifest.PropertyEdits)
        {
            if (!unique.Add((edit.ObjectId, edit.PropertyKey)))
                throw new InvalidDataException(
                    $"Project property edit {edit.ObjectId}:{edit.PropertyKey} is duplicated.");
            SmoProjectObject entry = Manifest.Objects.SingleOrDefault(
                    item => item.Id == edit.ObjectId)
                ?? throw new InvalidDataException(
                    $"Property edit references missing object ID {edit.ObjectId}.");
            SmoPropertyDescriptor descriptor = GetPropertyDescriptor(
                entry,
                edit.PropertyKey,
                edit.ValueKind);
            if (edit.Value.Length != descriptor.ValueSize)
            {
                throw new InvalidDataException(
                    $"Property edit {entry.Index}:{edit.PropertyKey} has " +
                    $"{edit.Value.Length} bytes instead of {descriptor.ValueSize}.");
            }
            SmoProjectField? field = entry.Fields.FirstOrDefault(item =>
                descriptor.Field.Matches(item.ToFieldView(entry)));
            if (descriptor.PayloadOffset < 0 || descriptor.ValueSize < 0)
                throw new InvalidDataException(
                    $"Property edit {entry.Index}:{edit.PropertyKey} has an invalid schema slice.");
            if (field is null)
            {
                if (!descriptor.CanMaterialize ||
                    (long)descriptor.PayloadOffset + descriptor.ValueSize >
                    descriptor.DefaultFieldPayload.Length)
                {
                    throw new InvalidDataException(
                        $"Required property {entry.Index}:{edit.PropertyKey} is absent " +
                        "and its confirmed schema cannot materialize it.");
                }
            }
            else if ((long)descriptor.PayloadOffset + descriptor.ValueSize >
                     field.PayloadSize)
            {
                throw new InvalidDataException(
                    $"Property edit {entry.Index}:{edit.PropertyKey} leaves its field payload.");
            }
        }
    }

    internal SmoProjectLayoutPlan BuildLayoutPlan()
    {
        ValidateEdits();
        ValidatePlacementEditPairs();
        ValidateResourceRelocations();
        ValidateResourceRedirects();
        ValidateBranchRemovals();
        ValidateReferencePlacements();
        Dictionary<uint, uint> generatedResourcePromotions =
            PlanGeneratedResourcePromotions();
        ValidateCollisionLinkOverrides();
        ValidateAddedForests();
        ValidateRemovedAddedForests();
        ValidateAddedObjectEdits();
        ValidateObjectDataReplacements();
        var changes = new List<SmoProjectDataChange>(
            Manifest.PropertyEdits.Count * 2 +
            Manifest.ResourceRelocations.Count * 2 +
            Manifest.ResourceRedirects.Count * 4 +
            Manifest.BranchRemovals.Count +
            Manifest.ReferencePlacements.Count +
            Manifest.AddedForests.Count);
        int sequence = 0;

        foreach (SmoProjectObjectDataReplacement replacement in
                 Manifest.ObjectDataReplacements)
        {
            SmoProjectObject? entry = Manifest.Objects.SingleOrDefault(item =>
                item.Id == replacement.ObjectId);
            if (entry is null || IsImportedObjectPendingRemoval(entry) ||
                Manifest.ResourceRelocations.Any(item =>
                    item.ObjectId == replacement.ObjectId))
            {
                continue;
            }
            changes.Add(new SmoProjectDataChange(
                checked((int)entry.LogicalOffset),
                checked((int)entry.SerializedSize),
                GetAssetBlobBytes(replacement.BlobId),
                entry.Index,
                $"replace-object-data:{entry.Index}/{entry.Id}",
                sequence++));
        }

        foreach (SmoProjectPropertyEdit edit in Manifest.PropertyEdits)
        {
            SmoProjectObject entry = Manifest.Objects.Single(item =>
                item.Id == edit.ObjectId);
            if (generatedResourcePromotions.Values.Contains(entry.Id))
                continue;
            SmoPropertyDescriptor descriptor = GetPropertyDescriptor(
                entry,
                edit.PropertyKey,
                edit.ValueKind);
            SmoProjectField? field = entry.Fields.FirstOrDefault(item =>
                descriptor.Field.Matches(item.ToFieldView(entry)));
            if (field is null)
                continue;
            int offset = checked(
                (int)entry.LogicalOffset +
                field.RelativePayloadOffset +
                descriptor.PayloadOffset);
            changes.Add(new SmoProjectDataChange(
                offset,
                descriptor.ValueSize,
                edit.Value,
                entry.Index,
                edit.PropertyKey,
                sequence++));
        }

        foreach (IGrouping<uint, SmoProjectPropertyEdit> group in
                 Manifest.PropertyEdits.GroupBy(edit => edit.ObjectId))
        {
            SmoProjectObject entry = Manifest.Objects.Single(item =>
                item.Id == group.Key);
            if (generatedResourcePromotions.Values.Contains(entry.Id))
                continue;
            if (!SmoSchemaRegistry.TryGet(
                    entry.TypeHash,
                    out SmoClassSchema? schema) ||
                schema is null)
            {
                continue;
            }
            var described = group.Select(edit => (
                Edit: edit,
                Descriptor: GetPropertyDescriptor(entry, edit.PropertyKey, edit.ValueKind)))
                .ToArray();
            var materialized = described
                .Where(item => !entry.Fields.Any(field =>
                    item.Descriptor.Field.Matches(field.ToFieldView(entry))))
                .ToArray();
            var emittedFields = new HashSet<SmoFieldSelector>();
            foreach (SmoPropertyDescriptor descriptor in schema.Properties)
            {
                (SmoProjectPropertyEdit Edit,
                    SmoPropertyDescriptor Descriptor)[] fieldEdits = materialized
                    .Where(item => item.Descriptor.Field == descriptor.Field)
                    .ToArray();
                if (fieldEdits.Length == 0 || !emittedFields.Add(descriptor.Field))
                    continue;

                byte[] payload = descriptor.DefaultFieldPayload.ToArray();
                foreach (var item in fieldEdits)
                {
                    item.Edit.Value.CopyTo(
                        payload,
                        item.Descriptor.PayloadOffset);
                }
                int insertion = ResolveInsertionOffset(
                    entry,
                    schema,
                    descriptor,
                    emittedFields);
                changes.Add(new SmoProjectDataChange(
                    checked((int)entry.LogicalOffset + insertion),
                    0,
                    SmoDataBlockWriter.BuildField(
                        descriptor.Field.FieldType,
                        payload),
                    entry.Index,
                    $"materialize:{descriptor.Key}",
                    sequence++));
            }
        }

        var relocatedObjects = new Dictionary<int, SmoProjectRelocationLayout>();
        foreach (SmoProjectResourceRelocation relocation in
                 Manifest.ResourceRelocations)
        {
            SmoProjectObject resource = Manifest.Objects.Single(item =>
                item.Id == relocation.ObjectId);
            if (generatedResourcePromotions.Values.Contains(resource.Id))
                continue;
            SmoProjectObject sourceOwner =
                Manifest.Objects[resource.ParentIndex!.Value];
            SmoProjectObject targetOwner = Manifest.Objects.Single(item =>
                item.Id == relocation.TargetOwnerId);
            SmoProjectField sourceField =
                FindExactInlineField(sourceOwner, resource);
            SmoProjectField targetField = targetOwner.Fields.Single(field =>
                field.FieldType == relocation.FieldType &&
                field.Occurrence == relocation.FieldOccurrence);
            byte[] referencePayload = new byte[8];
            BinaryPrimitives.WriteUInt32LittleEndian(referencePayload, resource.Id);
            byte[] referenceField = SmoDataBlockWriter.BuildField(
                sourceField.FieldType,
                referencePayload);
            byte[] inlinePayload = new byte[checked(8 + (int)resource.SerializedSize)];
            BinaryPrimitives.WriteUInt32LittleEndian(inlinePayload, resource.Id);
            BinaryPrimitives.WriteUInt32LittleEndian(
                inlinePayload.AsSpan(4),
                resource.SerializedSize);
            SmoProjectObjectDataReplacement? objectReplacement =
                Manifest.ObjectDataReplacements.SingleOrDefault(item =>
                    item.ObjectId == resource.Id);
            ReadOnlySpan<byte> relocatedData = objectReplacement is null
                ? _dataSection.AsSpan(
                    checked((int)resource.LogicalOffset),
                    checked((int)resource.SerializedSize))
                : GetAssetBlob(objectReplacement.BlobId).Span;
            relocatedData.CopyTo(inlinePayload.AsSpan(8));
            byte[] inlineField = SmoDataBlockWriter.BuildField(
                targetField.FieldType,
                inlinePayload);
            int targetFieldOffset = checked(
                (int)targetOwner.LogicalOffset + targetField.RelativeHeaderOffset);
            var targetChange = new SmoProjectDataChange(
                targetFieldOffset,
                checked(targetField.HeaderSize + (int)targetField.PayloadSize),
                inlineField,
                targetOwner.Index,
                $"relocate-target:{resource.Index}/{resource.Id}",
                sequence++);
            changes.Add(targetChange);
            relocatedObjects.Add(
                resource.Index,
                new SmoProjectRelocationLayout(
                    targetOwner.Index,
                    targetChange,
                    inlineField.Length - inlinePayload.Length + 8));

            bool sourceRemovedWithBranch = Manifest.BranchRemovals.Any(removal =>
            {
                SmoProjectObject removedRoot = Manifest.Objects.Single(item =>
                    item.Id == removal.RootObjectId);
                return Contains(removedRoot, resource) &&
                       !Contains(removedRoot, targetOwner);
            });
            if (!sourceRemovedWithBranch)
            {
                changes.Add(new SmoProjectDataChange(
                    checked((int)sourceOwner.LogicalOffset +
                            sourceField.RelativeHeaderOffset),
                    checked(sourceField.HeaderSize + (int)sourceField.PayloadSize),
                    referenceField,
                    sourceOwner.Index,
                    $"relocate-source:{resource.Index}/{resource.Id}",
                    sequence++));
            }
        }

        var addedObjects = new List<SmoProjectAddedObjectLayout>();
        SmoProjectAddedForest[] activeForests = Manifest.AddedForests
            .Where(item => !Manifest.RemovedAddedForestIds.Contains(item.BlobId))
            .ToArray();
        // Forests which own newly imported resources must be emitted before
        // lightweight placements which reference them. The game resolves
        // these IDs while walking the owner stream and does not safely defer a
        // forward reference to an object introduced later in the same stream.
        foreach (SmoProjectAddedForest forest in activeForests)
        {
            SmoProjectObject targetOwner = Manifest.Objects.Single(item =>
                item.Id == forest.TargetOwnerId);
            byte[] blob = BuildAddedForestBlob(forest);
            var change = new SmoProjectDataChange(
                checked((int)targetOwner.LogicalOffset +
                        forest.InsertionRelativeOffset),
                0,
                blob,
                targetOwner.Index,
                $"add-inline-forest:{forest.BlobId:N}",
                sequence++);
            changes.Add(change);
            addedObjects.AddRange(forest.Objects.Select(entry =>
                new SmoProjectAddedObjectLayout(
                    entry.Id,
                    entry.RawName,
                    entry.TypeHash,
                    entry.ObjectRelativeOffset,
                    entry.SerializedSize,
                    entry.ParentObjectId,
                    change)));
        }

        foreach (SmoProjectReferencePlacement placement in
                 Manifest.ReferencePlacements)
        {
            SmoProjectObject template = Manifest.Objects.Single(item =>
                item.Id == placement.TemplateRootObjectId);
            SmoProjectObject targetOwner = Manifest.Objects.Single(item =>
                item.Id == placement.TargetOwnerId);
            SmoProjectField templateField = FindExactInlineField(
                Manifest.Objects[template.ParentIndex!.Value],
                template);
            SmoProjectAddedForest? resourceForest =
                placement.ResourceObjectIdOverride is uint resourceId
                    ? activeForests.SingleOrDefault(item => item.Objects.Any(entry =>
                        entry.Id == ResolveResourceRedirect(resourceId)))
                    : null;
            uint? effectiveResourceId = placement.ResourceObjectIdOverride is uint overrideId
                ? ResolveResourceRedirect(overrideId)
                : null;
            SmoProjectObject? importedResource = effectiveResourceId is uint importedId
                ? Manifest.Objects.SingleOrDefault(item => item.Id == importedId)
                : null;
            SmoProjectObject? importedResourceRoot = effectiveResourceId is uint rootedId
                ? FindImportedStaticPlacementForResource(rootedId)
                : null;
            SmoProjectReferencePlacementData generated =
                BuildReferencePlacementData(
                    placement,
                    template,
                    targetOwner,
                    generatedResourcePromotions.TryGetValue(
                        placement.NewObjectIds[0],
                        out uint promotedResourceId)
                        ? promotedResourceId
                        : null);
            int insertionOffset = resourceForest is not null
                ? checked((int)targetOwner.LogicalOffset +
                          resourceForest.InsertionRelativeOffset)
                : importedResource is not null &&
                  relocatedObjects.TryGetValue(
                      importedResource.Index,
                      out SmoProjectRelocationLayout? relocation) &&
                  relocation.TargetOwnerIndex == targetOwner.Index
                    // The relocation replaces the imported reference field
                    // with the inline resource. Lightweight copies belong
                    // immediately after that complete replacement range; an
                    // insertion at its start would overlap the relocation and
                    // could also be visited before the resource definition.
                    ? checked(relocation.TargetChange.Offset +
                              relocation.TargetChange.OldLength)
                : importedResourceRoot?.ParentIndex is int resourceOwnerIndex &&
                  Manifest.Objects[resourceOwnerIndex].Id == targetOwner.Id
                    ? GetInlineFieldEndOffset(
                        Manifest.Objects[resourceOwnerIndex],
                        importedResourceRoot)
                : checked(
                    (int)Manifest.Objects[template.ParentIndex.Value].LogicalOffset +
                    templateField.RelativeHeaderOffset +
                    templateField.HeaderSize +
                    (int)templateField.PayloadSize);
            var change = new SmoProjectDataChange(
                insertionOffset,
                0,
                generated.Data,
                targetOwner.Index,
                $"add-reference-placement:{template.Index}/{placement.NewObjectIds[0]}",
                sequence++);
            changes.Add(change);
            addedObjects.AddRange(generated.Objects.Select(item => item with
            {
                Change = change
            }));
        }

        HashSet<int> removedObjectIndices = [];
        foreach (SmoProjectResourceRedirect redirect in
                 Manifest.ResourceRedirects)
        {
            SmoProjectObject source = Manifest.Objects.Single(item =>
                item.Id == redirect.SourceObjectId);
            SmoProjectObject parent =
                Manifest.Objects[source.ParentIndex!.Value];
            SmoProjectField ownerField = FindExactInlineField(parent, source);
            uint targetId = ResolveResourceRedirect(redirect.TargetObjectId);
            byte[] referencePayload = new byte[8];
            BinaryPrimitives.WriteUInt32LittleEndian(referencePayload, targetId);
            byte[] referenceField = SmoDataBlockWriter.BuildField(
                ownerField.FieldType,
                referencePayload);
            changes.Add(new SmoProjectDataChange(
                checked((int)parent.LogicalOffset + ownerField.RelativeHeaderOffset),
                checked(ownerField.HeaderSize + (int)ownerField.PayloadSize),
                referenceField,
                parent.Index,
                $"redirect-owner:{source.Index}/{source.Id}->{targetId}",
                sequence++));
            foreach (SmoProjectObject removed in
                     GetEffectiveRemovedObjects(source))
            {
                removedObjectIndices.Add(removed.Index);
            }

            HashSet<uint> fullBranchIds = GetBranchObjects(source)
                .Select(item => item.Id)
                .ToHashSet();
            foreach (SmoProjectExternalReference reference in
                     FindExternalReferences(new HashSet<uint> { source.Id })
                         .Where(item => !fullBranchIds.Contains(item.Owner.Id) &&
                                        !IsImportedObjectPendingRemoval(item.Owner)))
            {
                SmoProjectField field = reference.Owner.Fields.Single(item =>
                    item.FieldType == reference.FieldType &&
                    item.Occurrence == reference.FieldOccurrence);
                byte[] id = new byte[sizeof(uint)];
                BinaryPrimitives.WriteUInt32LittleEndian(id, targetId);
                changes.Add(new SmoProjectDataChange(
                    checked((int)reference.Owner.LogicalOffset +
                            field.RelativePayloadOffset),
                    sizeof(uint),
                    id,
                    reference.Owner.Index,
                    $"redirect-reference:{source.Id}/{reference.Owner.Id}/" +
                    $"{field.FieldType}/{field.Occurrence}->{targetId}",
                    sequence++));
            }
        }
        foreach (SmoProjectBranchRemoval removal in Manifest.BranchRemovals)
        {
            SmoProjectObject root = Manifest.Objects.Single(item =>
                item.Id == removal.RootObjectId);
            SmoProjectObject parent = Manifest.Objects[root.ParentIndex!.Value];
            SmoProjectField ownerField = FindExactInlineField(parent, root);
            foreach (SmoProjectObject removed in GetEffectiveRemovedObjects(root))
                removedObjectIndices.Add(removed.Index);
            foreach (uint promotedResourceId in generatedResourcePromotions.Values)
            {
                SmoProjectObject promotedResource = Manifest.Objects.Single(item =>
                    item.Id == promotedResourceId);
                if (Contains(root, promotedResource))
                    removedObjectIndices.Add(promotedResource.Index);
            }
            changes.Add(new SmoProjectDataChange(
                checked((int)parent.LogicalOffset + ownerField.RelativeHeaderOffset),
                checked(ownerField.HeaderSize + (int)ownerField.PayloadSize),
                [],
                parent.Index,
                $"remove-branch:{root.Index}/{root.Id}",
                sequence++));

            HashSet<uint> fullBranchIds = GetBranchObjects(root)
                .Select(item => item.Id)
                .ToHashSet();
            foreach (SmoProjectExternalReference reference in
                     FindExternalReferences(new HashSet<uint> { root.Id })
                         .Where(item => !fullBranchIds.Contains(item.Owner.Id) &&
                                        !IsImportedObjectPendingRemoval(item.Owner)))
            {
                SmoProjectField field = reference.Owner.Fields.Single(item =>
                    item.FieldType == reference.FieldType &&
                    item.Occurrence == reference.FieldOccurrence);
                changes.Add(new SmoProjectDataChange(
                    checked((int)reference.Owner.LogicalOffset +
                            field.RelativeHeaderOffset),
                    checked(field.HeaderSize + (int)field.PayloadSize),
                    [],
                    reference.Owner.Index,
                    $"remove-root-reference:{root.Index}/{reference.Owner.Index}/" +
                    $"{field.FieldType}/{field.Occurrence}",
                    sequence++));
            }
        }

        var objectDeltas = new int[Manifest.Objects.Count];
        foreach (SmoProjectDataChange change in changes)
            objectDeltas[change.OwnerIndex] = checked(
                objectDeltas[change.OwnerIndex] + change.Delta);
        var fieldPayloadDeltas = new Dictionary<(int ObjectIndex, int HeaderOffset), int>();
        for (int index = Manifest.Objects.Count - 1; index >= 0; index--)
        {
            SmoProjectObject entry = Manifest.Objects[index];
            foreach (((int ObjectIndex, int HeaderOffset) fieldKey, int payloadDelta) in
                     fieldPayloadDeltas.Where(item => item.Key.ObjectIndex == index).ToArray())
            {
                SmoProjectField field = entry.Fields.Single(item =>
                    item.RelativeHeaderOffset == fieldKey.HeaderOffset);
                uint newPayloadSize = checked((uint)(field.PayloadSize + payloadDelta));
                var preferred = new SmoDataBlockHeader(
                    field.RelativeHeaderOffset,
                    field.RawHeader,
                    field.FieldType,
                    field.SizeKind,
                    field.HeaderSize,
                    field.PayloadSize);
                byte[] header = SmoDataBlockWriter.BuildHeader(
                    field.FieldType,
                    newPayloadSize,
                    preferred);
                changes.Add(new SmoProjectDataChange(
                    checked((int)entry.LogicalOffset + field.RelativeHeaderOffset),
                    field.HeaderSize,
                    header,
                    entry.Index,
                    $"resize-field:{field.FieldType}/{field.Occurrence}",
                    sequence++));
                objectDeltas[index] = checked(
                    objectDeltas[index] + header.Length - field.HeaderSize);
            }

            if (objectDeltas[index] == 0 || entry.ParentIndex is not int parentIndex)
                continue;
            SmoProjectObject parent = Manifest.Objects[parentIndex];
            SmoProjectField containing = FindContainingField(parent, entry);
            var containingKey = (parentIndex, containing.RelativeHeaderOffset);
            fieldPayloadDeltas.TryGetValue(containingKey, out int priorDelta);
            fieldPayloadDeltas[containingKey] = checked(
                priorDelta + objectDeltas[index]);
            objectDeltas[parentIndex] = checked(
                objectDeltas[parentIndex] + objectDeltas[index]);
        }

        SmoProjectObject[] survivingObjects = Manifest.Objects
            .Where(entry => !removedObjectIndices.Contains(entry.Index))
            .ToArray();
        var drafts = new List<SmoProjectDirectoryDraft>(
            survivingObjects.Length + addedObjects.Count);
        drafts.AddRange(survivingObjects.Select(entry =>
        {
            if (relocatedObjects.TryGetValue(
                    entry.Index,
                    out SmoProjectRelocationLayout? relocation))
            {
                return new SmoProjectDirectoryDraft(
                    entry.Id,
                    entry.RawName,
                    entry.TypeHash,
                    checked((uint)(MapOffset(
                        relocation.TargetChange.Offset,
                        changes) + relocation.ObjectRelativeOffset)),
                    checked((uint)(entry.SerializedSize + objectDeltas[entry.Index])),
                    Manifest.Objects[relocation.TargetOwnerIndex].Id,
                    entry.Index);
            }
            return new SmoProjectDirectoryDraft(
                entry.Id,
                entry.RawName,
                entry.TypeHash,
                checked((uint)MapOffset(entry.LogicalOffset, changes)),
                checked((uint)(entry.SerializedSize + objectDeltas[entry.Index])),
                entry.ParentIndex is int parentIndex
                    ? Manifest.Objects[parentIndex].Id
                    : null,
                entry.Index);
        }));
        drafts.AddRange(addedObjects.Select((entry, addedIndex) =>
            new SmoProjectDirectoryDraft(
                entry.Id,
                entry.RawName,
                entry.TypeHash,
                checked((uint)(MapChangeStart(entry.Change, changes) +
                    entry.ObjectRelativeOffset)),
                entry.SerializedSize,
                entry.ParentObjectId,
                checked(Manifest.Objects.Count + addedIndex))));
        SmoProjectDirectoryDraft[] orderedDrafts = drafts
            .OrderBy(item => item.LogicalOffset)
            .ThenByDescending(item => item.SerializedSize)
            .ThenBy(item => item.SortKey)
            .ToArray();
        Dictionary<uint, int> rebuiltIndices = orderedDrafts
            .Select((draft, index) => (draft.Id, RebuiltIndex: index))
            .ToDictionary(item => item.Id, item => item.RebuiltIndex);
        SmoProjectLayoutEntry[] directory = orderedDrafts.Select(draft =>
            new SmoProjectLayoutEntry(
                draft.Id,
                draft.RawName,
                draft.TypeHash,
                draft.LogicalOffset,
                draft.SerializedSize,
                draft.ParentObjectId is uint parentId
                    ? rebuiltIndices[parentId]
                    : null)).ToArray();

        for (int index = 0; index < Manifest.Objects.Count; index++)
        {
            SmoProjectObject entry = Manifest.Objects[index];
            if (removedObjectIndices.Contains(index) ||
                entry.ParentIndex is null || objectDeltas[index] == 0)
                continue;
            int prefix = checked((int)entry.LogicalOffset - 8);
            if (prefix < 0 || prefix > _dataSection.Length - 8 ||
                BinaryPrimitives.ReadUInt32LittleEndian(_dataSection.AsSpan(prefix)) !=
                    entry.Id ||
                BinaryPrimitives.ReadUInt32LittleEndian(
                    _dataSection.AsSpan(prefix + 4)) != entry.SerializedSize)
            {
                throw new InvalidDataException(
                    $"Inline object {entry.Index} has no confirmed ID/size prefix.");
            }
            byte[] size = new byte[sizeof(uint)];
            SmoProjectLayoutEntry rebuiltEntry = directory.Single(item =>
                item.Id == entry.Id);
            BinaryPrimitives.WriteUInt32LittleEndian(
                size,
                rebuiltEntry.SerializedSize);
            changes.Add(new SmoProjectDataChange(
                prefix + 4,
                sizeof(uint),
                size,
                entry.ParentIndex.Value,
                $"inline-size:{entry.Index}",
                sequence++));
        }

        changes.Sort(SmoProjectDataChange.Compare);
        ValidateChanges(changes);
        int dataLength = checked(
            _dataSection.Length + changes.Sum(change => change.Delta));
        return new SmoProjectLayoutPlan(directory, changes, dataLength);
    }

    private void SetProperty(
        SmoProjectObject entry,
        string propertyKey,
        SmoPropertyValueKind valueKind,
        byte[] value)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(propertyKey);
        SmoPropertyDescriptor descriptor = GetPropertyDescriptor(
            entry,
            propertyKey,
            valueKind);
        if (value.Length != descriptor.ValueSize)
            throw new ArgumentException("Encoded property size does not match its schema.");
        if (!entry.Fields.Any(field =>
                descriptor.Field.Matches(field.ToFieldView(entry))))
        {
            if (!descriptor.CanMaterialize)
            {
                throw new InvalidDataException(
                    $"Required property {entry.Index}:{propertyKey} is absent.");
            }
            EnsureInsertionAnchorEdit(entry, descriptor.Insertion);
        }
        Manifest.PropertyEdits.RemoveAll(edit =>
            edit.ObjectId == entry.Id && edit.PropertyKey == propertyKey);
        Manifest.PropertyEdits.Add(new SmoProjectPropertyEdit
        {
            ObjectId = entry.Id,
            PropertyKey = propertyKey,
            ValueKind = valueKind,
            Value = value
        });
        ValidateEdits();
    }

    private void EnsureInsertionAnchorEdit(
        SmoProjectObject entry,
        SmoFieldInsertion insertion)
    {
        if (insertion.Kind == SmoFieldInsertionKind.BeforeTerminal ||
            insertion.Anchor is not SmoFieldSelector anchor ||
            entry.Fields.Any(field => anchor.Matches(field.ToFieldView(entry))))
        {
            return;
        }
        if (!SmoSchemaRegistry.TryGet(entry.TypeHash, out SmoClassSchema? schema) ||
            schema is null)
        {
            throw new InvalidDataException(
                $"Object {entry.Index} has no schema for insertion anchors.");
        }
        SmoPropertyDescriptor descriptor = schema.Properties.FirstOrDefault(item =>
                item.Field == anchor && item.CanMaterialize)
            ?? throw new InvalidDataException(
                $"Insertion anchor field {anchor.FieldType} is absent from " +
                $"object {entry.Index} and cannot be materialized.");
        bool alreadyPending = Manifest.PropertyEdits.Any(edit =>
        {
            if (edit.ObjectId != entry.Id)
                return false;
            SmoPropertyDescriptor pending = GetPropertyDescriptor(
                entry,
                edit.PropertyKey,
                edit.ValueKind);
            return pending.Field == descriptor.Field;
        });
        if (alreadyPending)
            return;

        EnsureInsertionAnchorEdit(entry, descriptor.Insertion);
        Manifest.PropertyEdits.Add(new SmoProjectPropertyEdit
        {
            ObjectId = entry.Id,
            PropertyKey = descriptor.Key,
            ValueKind = descriptor.ValueKind,
            Value = descriptor.DefaultFieldPayload.Span
                .Slice(descriptor.PayloadOffset, descriptor.ValueSize)
                .ToArray()
        });
    }

    private static int ResolveInsertionOffset(
        SmoProjectObject entry,
        SmoClassSchema schema,
        SmoPropertyDescriptor descriptor,
        IReadOnlySet<SmoFieldSelector> alreadyEmitted)
    {
        SmoFieldInsertion insertion = descriptor.Insertion;
        if (insertion.Kind == SmoFieldInsertionKind.BeforeTerminal)
        {
            SmoProjectField? terminal = entry.Fields.LastOrDefault(field =>
                field.PayloadSize == 0 &&
                field.RelativeHeaderOffset + field.HeaderSize == entry.SerializedSize);
            return terminal?.RelativeHeaderOffset ?? checked((int)entry.SerializedSize);
        }
        if (insertion.Anchor is not SmoFieldSelector anchor)
            throw new InvalidDataException("A schema field insertion anchor is missing.");
        SmoProjectField? original = entry.Fields.FirstOrDefault(field =>
            anchor.Matches(field.ToFieldView(entry)));
        if (original is not null)
        {
            return insertion.Kind == SmoFieldInsertionKind.BeforeField
                ? original.RelativeHeaderOffset
                : checked(original.RelativePayloadOffset + (int)original.PayloadSize);
        }
        SmoPropertyDescriptor anchorDescriptor = schema.Properties.FirstOrDefault(item =>
                item.Field == anchor && alreadyEmitted.Contains(item.Field))
            ?? throw new InvalidDataException(
                $"Materialized property {descriptor.Key} has no available anchor field.");
        return ResolveInsertionOffset(
            entry,
            schema,
            anchorDescriptor,
            alreadyEmitted);
    }

    private SmoProjectReferencePlacementData BuildReferencePlacementData(
        SmoProjectReferencePlacement placement,
        SmoProjectObject template,
        SmoProjectObject targetOwner,
        uint? promotedResourceId)
    {
        if (placement.ResourceObjectIdOverride is uint physicalResourceId)
        {
            uint effectiveId = ResolveResourceRedirect(physicalResourceId);
            SmoProjectObject? physicalResource = Manifest.Objects.SingleOrDefault(item =>
                item.Id == effectiveId && Contains(template, item));
            if (physicalResource is not null)
            {
                return BuildPhysicalReferencePlacementData(
                    placement,
                    template,
                    targetOwner,
                    physicalResource,
                    promotedResourceId == physicalResource.Id);
            }
        }

        SmoProjectObject sourceOwner =
            Manifest.Objects[template.ParentIndex!.Value];
        SmoProjectField templateField =
            FindExactInlineField(sourceOwner, template);
        int fieldStart = checked(
            (int)sourceOwner.LogicalOffset + templateField.RelativeHeaderOffset);
        int fieldLength = checked(
            templateField.HeaderSize + (int)templateField.PayloadSize);
        byte[] data = _dataSection.AsSpan(fieldStart, fieldLength).ToArray();
        IReadOnlyList<SmoProjectObject> branch = GetBranchObjects(template);
        Dictionary<uint, uint> remappedIds = branch
            .Select((source, index) => (source.Id, NewId: placement.NewObjectIds[index]))
            .ToDictionary(item => item.Id, item => item.NewId);
        int resourceOverrideCount = 0;

        foreach (SmoProjectObject source in branch)
        {
            int relativeObjectOffset = checked((int)source.LogicalOffset - fieldStart);
            int prefixOffset = checked(relativeObjectOffset - 8);
            if (prefixOffset < 0 || prefixOffset > data.Length - 8 ||
                BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(prefixOffset)) != source.Id ||
                BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(prefixOffset + 4)) !=
                    source.SerializedSize)
            {
                throw new InvalidDataException(
                    $"Reference placement source [{source.Index}] has no confirmed prefix.");
            }
            BinaryPrimitives.WriteUInt32LittleEndian(
                data.AsSpan(prefixOffset),
                remappedIds[source.Id]);

            foreach (SmoProjectField field in source.Fields.Where(item =>
                         item.PayloadSize == 8))
            {
                int originalPayload = checked(
                    (int)source.LogicalOffset + field.RelativePayloadOffset);
                ReadOnlySpan<byte> original = _dataSection.AsSpan(originalPayload, 8);
                uint referencedId = BinaryPrimitives.ReadUInt32LittleEndian(original);
                uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(original[4..]);
                if (inlineSize != 0 || !remappedIds.TryGetValue(referencedId, out uint newId))
                {
                    if (inlineSize == 0 &&
                        placement.ResourceObjectIdOverride is uint resourceOverride &&
                        Manifest.Objects.Any(item =>
                            item.Id == referencedId &&
                            item.TypeHash == SmoClassIds.MeshData))
                    {
                        int overriddenPayload = checked(
                            relativeObjectOffset + field.RelativePayloadOffset);
                        BinaryPrimitives.WriteUInt32LittleEndian(
                            data.AsSpan(overriddenPayload),
                            ResolveResourceRedirect(resourceOverride));
                        resourceOverrideCount++;
                    }
                    else if (inlineSize == 0)
                    {
                        uint redirectedId = ResolveResourceRedirect(referencedId);
                        if (redirectedId != referencedId)
                        {
                            int redirectedPayload = checked(
                                relativeObjectOffset + field.RelativePayloadOffset);
                            BinaryPrimitives.WriteUInt32LittleEndian(
                                data.AsSpan(redirectedPayload),
                                redirectedId);
                        }
                    }
                    continue;
                }
                int clonedPayload = checked(relativeObjectOffset + field.RelativePayloadOffset);
                BinaryPrimitives.WriteUInt32LittleEndian(data.AsSpan(clonedPayload), newId);
            }
        }
        if (placement.ResourceObjectIdOverride is not null && resourceOverrideCount == 0)
        {
            throw new InvalidDataException(
                $"Reference placement template {template.Id} has no mesh reference to override.");
        }

        PatchClonedMatrix(
            data,
            fieldStart,
            template,
            SmoPropertyKeys.WorldMatrix,
            placement.WorldMatrix);
        PatchClonedMatrix(
            data,
            fieldStart,
            template,
            SmoPropertyKeys.InverseWorldMatrix,
            placement.InverseWorldMatrix);

        var objects = new List<SmoProjectAddedObjectLayout>(branch.Count);
        for (int index = 0; index < branch.Count; index++)
        {
            SmoProjectObject source = branch[index];
            uint? parentId = source.Id == template.Id
                ? targetOwner.Id
                : source.ParentIndex is int parentIndex
                    ? remappedIds[Manifest.Objects[parentIndex].Id]
                    : throw new InvalidDataException(
                        $"Cloned descendant [{source.Index}] has no parent.");
            objects.Add(new SmoProjectAddedObjectLayout(
                placement.NewObjectIds[index],
                source.Id == template.Id ? placement.RootRawName : source.RawName,
                source.TypeHash,
                checked((uint)((int)source.LogicalOffset - fieldStart)),
                source.SerializedSize,
                parentId,
                null!));
        }
        return new SmoProjectReferencePlacementData(data, objects);
    }

    private SmoProjectReferencePlacementData BuildPhysicalReferencePlacementData(
        SmoProjectReferencePlacement placement,
        SmoProjectObject template,
        SmoProjectObject targetOwner,
        SmoProjectObject physicalResource,
        bool promoteResource)
    {
        SmoProjectObject sourceOwner = Manifest.Objects[template.ParentIndex!.Value];
        SmoProjectField templateField = FindExactInlineField(sourceOwner, template);
        IReadOnlyList<SmoProjectObject> shell = GetReferencePlacementShellObjects(
            template,
            placement.ResourceObjectIdOverride);
        Dictionary<uint, uint> remappedIds = shell
            .Select((source, index) => (source.Id, NewId: placement.NewObjectIds[index]))
            .ToDictionary(item => item.Id, item => item.NewId);
        HashSet<uint> shellIds = shell.Select(item => item.Id).ToHashSet();
        uint resourceId = ResolveResourceRedirect(
            placement.ResourceObjectIdOverride!.Value);
        int resourceOverrideCount = 0;
        SmoPropertyDescriptor worldDescriptor = GetPropertyDescriptor(
            template,
            SmoPropertyKeys.WorldMatrix,
            SmoPropertyValueKind.Matrix4x4);
        SmoPropertyDescriptor inverseDescriptor = GetPropertyDescriptor(
            template,
            SmoPropertyKeys.InverseWorldMatrix,
            SmoPropertyValueKind.Matrix4x4);

        BuiltReferenceObject BuildObject(SmoProjectObject source, uint parentId)
        {
            ReadOnlySpan<byte> original = _dataSection.AsSpan(
                checked((int)source.LogicalOffset),
                checked((int)source.SerializedSize));
            using var stream = new MemoryStream(checked((int)source.SerializedSize));
            int cursor = 0;
            var descendants = new List<BuiltReferenceObjectLayout>();
            foreach (SmoProjectField field in source.Fields
                         .OrderBy(item => item.RelativeHeaderOffset))
            {
                if (field.RelativeHeaderOffset < cursor)
                {
                    throw new InvalidDataException(
                        $"Reference shell object {source.Id} has overlapping fields.");
                }
                stream.Write(original.Slice(
                    cursor,
                    field.RelativeHeaderOffset - cursor));

                SmoProjectObject? inlineChild = Manifest.Objects
                    .Where(item => item.ParentIndex == source.Index)
                    .SingleOrDefault(item =>
                        field.PayloadSize == item.SerializedSize + 8UL &&
                        (long)source.LogicalOffset + field.RelativePayloadOffset + 8L ==
                        item.LogicalOffset);
                byte[] fieldBytes;
                BuiltReferenceObject? builtChild = null;
                if (inlineChild?.Id == physicalResource.Id)
                {
                    if (promoteResource)
                    {
                        if (GetBranchObjects(physicalResource).Count != 1)
                        {
                            throw new InvalidDataException(
                                $"Physical mesh {physicalResource.Id} has inline " +
                                "descendants and cannot be promoted as one resource.");
                        }
                        byte[] resourceData = BuildPromotedResourceData(
                            physicalResource);
                        byte[] payload = new byte[checked(8 + resourceData.Length)];
                        BinaryPrimitives.WriteUInt32LittleEndian(
                            payload,
                            physicalResource.Id);
                        BinaryPrimitives.WriteUInt32LittleEndian(
                            payload.AsSpan(4),
                            checked((uint)resourceData.Length));
                        resourceData.CopyTo(payload, 8);
                        fieldBytes = SmoDataBlockWriter.BuildField(
                            field.FieldType,
                            payload);
                    }
                    else
                    {
                        byte[] payload = new byte[8];
                        BinaryPrimitives.WriteUInt32LittleEndian(payload, resourceId);
                        fieldBytes = SmoDataBlockWriter.BuildField(field.FieldType, payload);
                    }
                    resourceOverrideCount++;
                }
                else if (inlineChild is not null && shellIds.Contains(inlineChild.Id))
                {
                    builtChild = BuildObject(inlineChild, remappedIds[source.Id]);
                    byte[] payload = new byte[checked(8 + builtChild.Data.Length)];
                    BinaryPrimitives.WriteUInt32LittleEndian(
                        payload,
                        remappedIds[inlineChild.Id]);
                    BinaryPrimitives.WriteUInt32LittleEndian(
                        payload.AsSpan(4),
                        checked((uint)builtChild.Data.Length));
                    builtChild.Data.CopyTo(payload, 8);
                    fieldBytes = SmoDataBlockWriter.BuildField(field.FieldType, payload);
                }
                else if (inlineChild is not null)
                {
                    throw new InvalidDataException(
                        $"Reference shell excluded unexpected inline object {inlineChild.Id}.");
                }
                else
                {
                    int fieldLength = checked(field.HeaderSize + (int)field.PayloadSize);
                    fieldBytes = original.Slice(field.RelativeHeaderOffset, fieldLength)
                        .ToArray();
                    if (field.PayloadSize == 8)
                    {
                        uint referencedId = BinaryPrimitives.ReadUInt32LittleEndian(
                            fieldBytes.AsSpan(field.HeaderSize));
                        uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(
                            fieldBytes.AsSpan(field.HeaderSize + 4));
                        if (inlineSize == 0)
                        {
                            uint patchedId = remappedIds.GetValueOrDefault(
                                referencedId,
                                ResolveResourceRedirect(referencedId));
                            if (referencedId == physicalResource.Id)
                            {
                                patchedId = resourceId;
                                resourceOverrideCount++;
                            }
                            BinaryPrimitives.WriteUInt32LittleEndian(
                                fieldBytes.AsSpan(field.HeaderSize),
                                patchedId);
                        }
                    }
                }

                if (source.Id == template.Id)
                {
                    if (worldDescriptor.Field.Matches(field.ToFieldView(source)))
                    {
                        placement.WorldMatrix.CopyTo(
                            fieldBytes,
                            fieldBytes.Length - (int)field.PayloadSize +
                            worldDescriptor.PayloadOffset);
                    }
                    else if (inverseDescriptor.Field.Matches(field.ToFieldView(source)))
                    {
                        placement.InverseWorldMatrix.CopyTo(
                            fieldBytes,
                            fieldBytes.Length - (int)field.PayloadSize +
                            inverseDescriptor.PayloadOffset);
                    }
                }

                int fieldStart = checked((int)stream.Position);
                stream.Write(fieldBytes);
                if (inlineChild?.Id == physicalResource.Id && promoteResource)
                {
                    // The field may have been rebuilt with a different mesh size.
                    // Its inline object always starts after the generated field
                    // header and the eight-byte ID/size prefix.
                    if (!SmoDataBlockReader.TryReadHeader(
                            fieldBytes,
                            0,
                            out SmoDataBlockHeader promotedHeader))
                    {
                        throw new InvalidDataException(
                            $"Promoted mesh field for {physicalResource.Id} is invalid.");
                    }
                    int resourceOffset = checked(
                        fieldStart + promotedHeader.PayloadOffset + 8);
                    descendants.Add(new BuiltReferenceObjectLayout(
                        physicalResource,
                        resourceOffset,
                        checked((uint)(fieldBytes.Length -
                                       promotedHeader.PayloadOffset - 8)),
                        remappedIds[source.Id]));
                }
                if (builtChild is not null)
                {
                    int childOffset = checked(
                        fieldStart + fieldBytes.Length - builtChild.Data.Length);
                    descendants.AddRange(builtChild.Layouts.Select(item => item with
                    {
                        RelativeOffset = checked(item.RelativeOffset + childOffset)
                    }));
                }
                cursor = checked(
                    field.RelativeHeaderOffset + field.HeaderSize +
                    (int)field.PayloadSize);
            }
            if (cursor < original.Length)
                stream.Write(original[cursor..]);
            byte[] data = stream.ToArray();
            var layouts = new List<BuiltReferenceObjectLayout>(1 + descendants.Count)
            {
                new(
                    source,
                    0,
                    checked((uint)data.Length),
                    parentId)
            };
            layouts.AddRange(descendants);
            return new BuiltReferenceObject(data, layouts);
        }

        BuiltReferenceObject root = BuildObject(template, targetOwner.Id);
        if (resourceOverrideCount == 0)
        {
            throw new InvalidDataException(
                $"Physical reference shell {template.Id} does not expose mesh " +
                $"resource {physicalResource.Id}.");
        }
        byte[] outerPayload = new byte[checked(8 + root.Data.Length)];
        BinaryPrimitives.WriteUInt32LittleEndian(
            outerPayload,
            remappedIds[template.Id]);
        BinaryPrimitives.WriteUInt32LittleEndian(
            outerPayload.AsSpan(4),
            checked((uint)root.Data.Length));
        root.Data.CopyTo(outerPayload, 8);
        byte[] data = SmoDataBlockWriter.BuildField(
            templateField.FieldType,
            outerPayload);
        int rootOffset = checked(data.Length - root.Data.Length);
        SmoProjectAddedObjectLayout[] objects = root.Layouts.Select(item =>
            new SmoProjectAddedObjectLayout(
                remappedIds.TryGetValue(item.Source.Id, out uint remappedId)
                    ? remappedId
                    : item.Source.Id,
                item.Source.Id == template.Id
                    ? placement.RootRawName
                    : item.Source.RawName,
                item.Source.TypeHash,
                checked((uint)(rootOffset + item.RelativeOffset)),
                item.SerializedSize,
                item.ParentId,
                null!))
            .ToArray();
        return new SmoProjectReferencePlacementData(data, objects);
    }

    private byte[] BuildPromotedResourceData(SmoProjectObject resource)
    {
        SmoProjectObjectDataReplacement? replacement =
            Manifest.ObjectDataReplacements.SingleOrDefault(item =>
                item.ObjectId == resource.Id);
        byte[] data = replacement is null
            ? _dataSection.AsSpan(
                checked((int)resource.LogicalOffset),
                checked((int)resource.SerializedSize)).ToArray()
            : GetAssetBlobBytes(replacement.BlobId);

        SmoProjectPropertyEdit[] edits = Manifest.PropertyEdits
            .Where(item => item.ObjectId == resource.Id)
            .ToArray();
        if (replacement is not null && edits.Length > 0)
        {
            throw new InvalidDataException(
                $"Promoted resource {resource.Id} has both a complete data " +
                "replacement and property edits; their layouts cannot be merged safely.");
        }
        foreach (SmoProjectPropertyEdit edit in edits)
        {
            SmoPropertyDescriptor descriptor = GetPropertyDescriptor(
                resource,
                edit.PropertyKey,
                edit.ValueKind);
            SmoProjectField field = resource.Fields.Single(item =>
                descriptor.Field.Matches(item.ToFieldView(resource)));
            int offset = checked(
                field.RelativePayloadOffset + descriptor.PayloadOffset);
            if (offset < 0 || offset > data.Length - edit.Value.Length)
            {
                throw new InvalidDataException(
                    $"Promoted resource edit {resource.Id}:{edit.PropertyKey} " +
                    "leaves the serialized object.");
            }
            edit.Value.CopyTo(data, offset);
        }

        // References contained by the moved object no longer occupy their old
        // data.bin offsets, so apply pending resource redirects to the copied
        // field stream as part of promotion.
        byte[] redirected = data.ToArray();
        int cursor = 8;
        bool decoded = redirected.Length >= 8;
        while (decoded && cursor < redirected.Length)
        {
            if (!SmoDataBlockReader.TryReadHeader(
                    redirected,
                    cursor,
                    out SmoDataBlockHeader field) ||
                field.PayloadEnd > redirected.Length)
            {
                decoded = false;
                break;
            }
            if (field.PayloadSize == 8 &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    redirected.AsSpan(field.PayloadOffset + 4)) == 0)
            {
                uint referencedId = BinaryPrimitives.ReadUInt32LittleEndian(
                    redirected.AsSpan(field.PayloadOffset));
                uint targetId = ResolveResourceRedirect(referencedId);
                if (targetId != referencedId)
                {
                    BinaryPrimitives.WriteUInt32LittleEndian(
                        redirected.AsSpan(field.PayloadOffset),
                        targetId);
                }
            }
            cursor = checked((int)field.PayloadEnd);
        }
        return decoded && cursor == redirected.Length ? redirected : data;
    }

    private void PatchClonedMatrix(
        byte[] clone,
        int fieldStart,
        SmoProjectObject template,
        string propertyKey,
        byte[] value)
    {
        SmoPropertyDescriptor descriptor = GetPropertyDescriptor(
            template,
            propertyKey,
            SmoPropertyValueKind.Matrix4x4);
        SmoProjectField field = template.Fields.Single(item =>
            descriptor.Field.Matches(item.ToFieldView(template)));
        int relative = checked(
            (int)template.LogicalOffset - fieldStart +
            field.RelativePayloadOffset + descriptor.PayloadOffset);
        value.CopyTo(clone, relative);
    }

    private static void ValidatePlacementMatrixPair(
        byte[] worldBytes,
        byte[] inverseBytes,
        uint objectId)
    {
        Matrix4x4 world = DecodeMatrix(worldBytes, $"placement {objectId} world matrix");
        Matrix4x4 inverse = DecodeMatrix(
            inverseBytes,
            $"placement {objectId} inverse matrix");
        Matrix4x4 expectedInverse =
            SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(world);
        if (!Matrix4x4.Invert(world, out _) ||
            !MatrixNearlyEquals(expectedInverse, inverse, 0.001f))
        {
            throw new InvalidDataException(
                $"Reference placement {objectId} has a singular or mismatched matrix pair.");
        }
    }

    private static Matrix4x4 DecodeMatrix(byte[] bytes, string label)
    {
        if (bytes.Length != 64)
            throw new InvalidDataException($"The {label} must contain 64 bytes.");
        Span<float> cells = stackalloc float[16];
        for (int index = 0; index < cells.Length; index++)
        {
            cells[index] = BitConverter.Int32BitsToSingle(
                BinaryPrimitives.ReadInt32LittleEndian(bytes.AsSpan(index * 4)));
            if (!float.IsFinite(cells[index]))
                throw new InvalidDataException($"The {label} contains a non-finite cell.");
        }
        return new Matrix4x4(
            cells[0], cells[1], cells[2], cells[3],
            cells[4], cells[5], cells[6], cells[7],
            cells[8], cells[9], cells[10], cells[11],
            cells[12], cells[13], cells[14], cells[15]);
    }

    private static bool MatrixNearlyEquals(Matrix4x4 left, Matrix4x4 right, float epsilon)
    {
        Span<float> leftCells = stackalloc float[16]
        {
            left.M11, left.M12, left.M13, left.M14,
            left.M21, left.M22, left.M23, left.M24,
            left.M31, left.M32, left.M33, left.M34,
            left.M41, left.M42, left.M43, left.M44
        };
        Span<float> rightCells = stackalloc float[16]
        {
            right.M11, right.M12, right.M13, right.M14,
            right.M21, right.M22, right.M23, right.M24,
            right.M31, right.M32, right.M33, right.M34,
            right.M41, right.M42, right.M43, right.M44
        };
        for (int index = 0; index < leftCells.Length; index++)
        {
            if (MathF.Abs(leftCells[index] - rightCells[index]) > epsilon)
                return false;
        }
        return true;
    }

    private static SmoProjectField FindContainingField(
        SmoProjectObject parent,
        SmoProjectObject child)
    {
        long childStart = child.LogicalOffset;
        long childEnd = childStart + child.SerializedSize;
        return parent.Fields
            .Where(field =>
            {
                long payloadStart = parent.LogicalOffset + field.RelativePayloadOffset;
                long payloadEnd = payloadStart + field.PayloadSize;
                return payloadStart <= childStart && childEnd <= payloadEnd;
            })
            .OrderBy(field => field.PayloadSize)
            .FirstOrDefault()
            ?? throw new InvalidDataException(
                $"Parent object {parent.Index} has no direct field containing " +
                $"child object {child.Index}.");
    }

    private static SmoProjectField FindExactInlineField(
        SmoProjectObject parent,
        SmoProjectObject child) =>
        TryFindExactInlineField(parent, child, out SmoProjectField? field)
            ? field!
            : throw new InvalidDataException(
                $"Parent object {parent.Index} has no exact inline field for " +
                $"child object {child.Index}.");

    private static int GetInlineFieldEndOffset(
        SmoProjectObject parent,
        SmoProjectObject child)
    {
        SmoProjectField field = FindExactInlineField(parent, child);
        return checked(
            (int)parent.LogicalOffset +
            field.RelativeHeaderOffset +
            field.HeaderSize +
            (int)field.PayloadSize);
    }

    private static bool TryFindExactInlineField(
        SmoProjectObject parent,
        SmoProjectObject child,
        out SmoProjectField? result)
    {
        long expectedPayload = (long)child.LogicalOffset - parent.LogicalOffset - 8;
        result = parent.Fields.FirstOrDefault(field =>
            field.RelativePayloadOffset == expectedPayload &&
            field.PayloadSize == child.SerializedSize + 8UL);
        return result is not null;
    }

    private IReadOnlyList<SmoProjectObject> GetBranchObjects(
        SmoProjectObject root)
    {
        ulong start = root.LogicalOffset;
        ulong end = start + root.SerializedSize;
        return Manifest.Objects.Where(item =>
        {
            ulong itemStart = item.LogicalOffset;
            ulong itemEnd = itemStart + item.SerializedSize;
            return start <= itemStart && itemEnd <= end;
        }).ToArray();
    }

    private IReadOnlyList<SmoProjectObject> GetEffectiveRemovedObjects(
        SmoProjectObject root)
    {
        HashSet<uint> relocatedOut = Manifest.ResourceRelocations
            .Where(relocation =>
            {
                SmoProjectObject resource = Manifest.Objects.Single(item =>
                    item.Id == relocation.ObjectId);
                SmoProjectObject target = Manifest.Objects.Single(item =>
                    item.Id == relocation.TargetOwnerId);
                return Contains(root, resource) && !Contains(root, target);
            })
            .Select(item => item.ObjectId)
            .ToHashSet();
        return GetBranchObjects(root)
            .Where(item => !relocatedOut.Contains(item.Id))
            .ToArray();
    }

    private uint ResolveResourceRedirect(uint objectId)
    {
        var visited = new HashSet<uint>();
        uint current = objectId;
        while (Manifest.ResourceRedirects.FirstOrDefault(item =>
                   item.SourceObjectId == current) is
               SmoProjectResourceRedirect redirect)
        {
            if (!visited.Add(current))
                throw new InvalidDataException("Resource redirects contain a cycle.");
            current = redirect.TargetObjectId;
        }
        return current;
    }

    private bool IsImportedObjectPendingRemoval(SmoProjectObject entry) =>
        Manifest.BranchRemovals.Any(removal =>
        {
            SmoProjectObject root = Manifest.Objects.Single(item =>
                item.Id == removal.RootObjectId);
            return Contains(root, entry);
        }) ||
        Manifest.ResourceRedirects.Any(redirect =>
        {
            SmoProjectObject root = Manifest.Objects.Single(item =>
                item.Id == redirect.SourceObjectId);
            return Contains(root, entry) &&
                   GetEffectiveRemovedObjects(root).Any(item => item.Id == entry.Id);
        });

    private IReadOnlyList<SmoProjectExternalReference> FindExternalReferences(
        IReadOnlySet<uint> branchIds)
    {
        var result = new List<SmoProjectExternalReference>();
        foreach (SmoProjectObject owner in Manifest.Objects.Where(item =>
                     !branchIds.Contains(item.Id)))
        {
            foreach (SmoProjectField field in owner.Fields.Where(item =>
                         item.PayloadSize == 8))
            {
                int payload = checked(
                    (int)owner.LogicalOffset + field.RelativePayloadOffset);
                ReadOnlySpan<byte> bytes = _dataSection.AsSpan(payload, 8);
                uint referencedId = BinaryPrimitives.ReadUInt32LittleEndian(bytes);
                uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(bytes[4..]);
                if (inlineSize == 0 && branchIds.Contains(referencedId))
                    result.Add(new SmoProjectExternalReference(
                        owner,
                        referencedId,
                        field.FieldType,
                        field.Occurrence));
            }
        }
        return result;
    }

    private SmoProjectField[] FindReferenceFields(
        SmoProjectObject owner,
        uint referencedObjectId,
        int fieldType) => owner.Fields
        .Where(field => field.FieldType == fieldType &&
                        IsReferenceField(owner, field, referencedObjectId))
        .ToArray();

    private bool IsReferenceField(
        SmoProjectObject owner,
        SmoProjectField field,
        uint referencedObjectId)
    {
        if (field.PayloadSize != 8)
            return false;
        int payload = checked(
            (int)owner.LogicalOffset + field.RelativePayloadOffset);
        ReadOnlySpan<byte> bytes = _dataSection.AsSpan(payload, 8);
        return BinaryPrimitives.ReadUInt32LittleEndian(bytes) == referencedObjectId &&
               BinaryPrimitives.ReadUInt32LittleEndian(bytes[4..]) == 0;
    }

    private static bool Contains(
        SmoProjectObject container,
        SmoProjectObject item)
    {
        ulong containerStart = container.LogicalOffset;
        ulong containerEnd = containerStart + container.SerializedSize;
        ulong itemStart = item.LogicalOffset;
        ulong itemEnd = itemStart + item.SerializedSize;
        return containerStart <= itemStart && itemEnd <= containerEnd;
    }

    private static long MapOffset(
        long oldOffset,
        IEnumerable<SmoProjectDataChange> changes)
    {
        long mapped = oldOffset;
        foreach (SmoProjectDataChange change in changes)
        {
            int oldEnd = checked(change.Offset + change.OldLength);
            if (change.OldLength == 0
                    ? oldOffset >= change.Offset
                    : oldOffset >= oldEnd)
            {
                mapped += change.Delta;
            }
            else if (oldOffset > change.Offset && oldOffset < oldEnd)
            {
                throw new InvalidDataException(
                    "An object offset points inside a rewritten data-block header.");
            }
        }
        return mapped;
    }

    private static long MapChangeStart(
        SmoProjectDataChange target,
        IEnumerable<SmoProjectDataChange> changes)
    {
        long mapped = target.Offset;
        foreach (SmoProjectDataChange change in changes
                     .OrderBy(item => item.Offset)
                     .ThenBy(item => item.Sequence))
        {
            if (ReferenceEquals(change, target))
                break;
            int oldEnd = checked(change.Offset + change.OldLength);
            if (change.OldLength == 0
                    ? target.Offset >= change.Offset
                    : target.Offset >= oldEnd)
            {
                mapped += change.Delta;
            }
            else if (target.Offset > change.Offset && target.Offset < oldEnd)
            {
                throw new InvalidDataException(
                    "An inserted placement starts inside another rewritten field.");
            }
        }
        return mapped;
    }

    private void ValidateChanges(IReadOnlyList<SmoProjectDataChange> changes)
    {
        int sourceCursor = 0;
        foreach (SmoProjectDataChange change in changes)
        {
            if (change.Offset < sourceCursor ||
                change.Offset < 0 ||
                change.OldLength < 0 ||
                change.Offset > _dataSection.Length - change.OldLength)
            {
                throw new InvalidDataException(
                    $"Project layout changes overlap or leave data.bin at " +
                    $"{change.Label}.");
            }
            sourceCursor = checked(change.Offset + change.OldLength);
        }
    }

    private Matrix4x4 ReadMatrixProperty(
        SmoProjectObject entry,
        string propertyKey)
    {
        byte[] bytes = ReadProperty(
            entry,
            propertyKey,
            SmoPropertyValueKind.Matrix4x4);
        float[] cells = new float[16];
        for (int index = 0; index < cells.Length; index++)
        {
            cells[index] = BitConverter.Int32BitsToSingle(
                BinaryPrimitives.ReadInt32LittleEndian(bytes.AsSpan(index * 4)));
        }
        return new Matrix4x4(
            cells[0], cells[1], cells[2], cells[3],
            cells[4], cells[5], cells[6], cells[7],
            cells[8], cells[9], cells[10], cells[11],
            cells[12], cells[13], cells[14], cells[15]);
    }

    private byte[] ReadProperty(
        SmoProjectObject entry,
        string propertyKey,
        SmoPropertyValueKind valueKind)
    {
        SmoPropertyDescriptor descriptor = GetPropertyDescriptor(
            entry,
            propertyKey,
            valueKind);
        SmoProjectPropertyEdit? edited = Manifest.PropertyEdits
            .LastOrDefault(item => item.ObjectId == entry.Id &&
                                   item.PropertyKey == propertyKey);
        if (edited is not null)
            return edited.Value.ToArray();
        SmoProjectField field = entry.Fields.Single(item =>
            descriptor.Field.Matches(item.ToFieldView(entry)));
        int offset = checked(
            (int)entry.LogicalOffset +
            field.RelativePayloadOffset +
            descriptor.PayloadOffset);
        return _dataSection.AsSpan(offset, descriptor.ValueSize).ToArray();
    }

    private static SmoPropertyDescriptor GetPropertyDescriptor(
        SmoProjectObject entry,
        string propertyKey,
        SmoPropertyValueKind valueKind)
        => GetPropertyDescriptor(
            entry.TypeHash,
            propertyKey,
            valueKind,
            $"object {entry.Index}");

    private static SmoPropertyDescriptor GetPropertyDescriptor(
        uint typeHash,
        string propertyKey,
        SmoPropertyValueKind valueKind,
        string objectLabel)
    {
        if (!SmoSchemaRegistry.TryGet(typeHash, out SmoClassSchema? schema) ||
            schema is null)
        {
            throw new NotSupportedException(
                $"Project {objectLabel} has no confirmed property schema.");
        }
        SmoPropertyDescriptor descriptor = schema.Properties.SingleOrDefault(item =>
                item.Key == propertyKey)
            ?? throw new NotSupportedException(
                $"Property {propertyKey} is not confirmed for {objectLabel}.");
        if (descriptor.ValueKind != valueKind)
        {
            throw new ArgumentException(
                $"Property {propertyKey} expects {descriptor.ValueKind}, not {valueKind}.");
        }
        return descriptor;
    }

    private SmoProjectObject GetObject(int objectIndex)
    {
        if ((uint)objectIndex >= (uint)Manifest.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(objectIndex));
        return Manifest.Objects[objectIndex];
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);

    private static bool IsFinite(Matrix4x4 value) =>
        float.IsFinite(value.M11) && float.IsFinite(value.M12) &&
        float.IsFinite(value.M13) && float.IsFinite(value.M14) &&
        float.IsFinite(value.M21) && float.IsFinite(value.M22) &&
        float.IsFinite(value.M23) && float.IsFinite(value.M24) &&
        float.IsFinite(value.M31) && float.IsFinite(value.M32) &&
        float.IsFinite(value.M33) && float.IsFinite(value.M34) &&
        float.IsFinite(value.M41) && float.IsFinite(value.M42) &&
        float.IsFinite(value.M43) && float.IsFinite(value.M44);

    private void ValidateIntervalsAndParents()
    {
        var stack = new List<SmoProjectObject>();
        foreach (SmoProjectObject entry in Manifest.Objects
                     .OrderBy(item => item.LogicalOffset)
                     .ThenByDescending(item => (ulong)item.LogicalOffset + item.SerializedSize)
                     .ThenBy(item => item.Index))
        {
            ulong start = entry.LogicalOffset;
            ulong end = start + entry.SerializedSize;
            while (stack.Count > 0 && start >=
                   (ulong)stack[^1].LogicalOffset + stack[^1].SerializedSize)
            {
                stack.RemoveAt(stack.Count - 1);
            }
            if (stack.Count > 0)
            {
                SmoProjectObject parent = stack[^1];
                ulong parentEnd = (ulong)parent.LogicalOffset + parent.SerializedSize;
                if (end > parentEnd)
                    throw new InvalidDataException("Project object intervals partially overlap.");
                if (entry.ParentIndex != parent.Index)
                {
                    throw new InvalidDataException(
                        $"Project object {entry.Index} declares parent " +
                        $"{entry.ParentIndex?.ToString() ?? "NONE"}, but its interval owner is " +
                        $"{parent.Index}.");
                }
            }
            else if (entry.ParentIndex is not null)
            {
                throw new InvalidDataException(
                    $"Root project object {entry.Index} declares a parent.");
            }
            stack.Add(entry);
        }
    }

    internal static string Hash(ReadOnlySpan<byte> data) =>
        Convert.ToHexString(SHA256.HashData(data));
}

public sealed class SmoProjectManifest
{
    public required string Format { get; init; }
    public int FormatVersion { get; init; }
    public string? SourceFileName { get; init; }
    public string? SourcePathHint { get; init; }
    public string? SourceLogicalPath { get; init; }
    public required string SourceSha256 { get; init; }
    public required string DataSha256 { get; init; }
    public long DataLength { get; init; }
    public required SmoProjectHeader Header { get; init; }
    public List<SmoProjectObject> Objects { get; init; } = [];
    public List<SmoProjectPropertyEdit> PropertyEdits { get; init; } = [];
    public List<SmoProjectResourceRelocation> ResourceRelocations { get; init; } = [];
    public List<SmoProjectResourceRedirect> ResourceRedirects { get; init; } = [];
    public List<SmoProjectBranchRemoval> BranchRemovals { get; init; } = [];
    public List<SmoProjectReferencePlacement> ReferencePlacements { get; init; } = [];
    public List<SmoProjectCollisionLinkOverride> CollisionLinkOverrides
        { get; init; } = [];
    public List<SmoProjectAddedForest> AddedForests { get; init; } = [];
    public List<Guid> RemovedAddedForestIds { get; init; } = [];
    public List<SmoProjectAddedObjectPropertyEdit> AddedObjectPropertyEdits
        { get; init; } = [];
    public List<SmoProjectObjectDataReplacement> ObjectDataReplacements
        { get; init; } = [];
}

public sealed class SmoProjectObjectDataReplacement
{
    public uint ObjectId { get; init; }
    public Guid BlobId { get; init; }
    public string BlobSha256 { get; init; } = string.Empty;
}

public sealed class SmoProjectHeader
{
    public uint SerializerVersion { get; init; }
    public uint Unknown08 { get; init; }
    public uint PlatformMask { get; init; }
}

public sealed class SmoProjectObject
{
    public int Index { get; init; }
    public uint Id { get; init; }
    public byte[] RawName { get; init; } = [];
    public uint TypeHash { get; init; }
    public uint LogicalOffset { get; init; }
    public uint SerializedSize { get; init; }
    public int? ParentIndex { get; init; }
    public bool FieldStreamDecoded { get; init; }
    public string? FieldStreamError { get; init; }
    public List<SmoProjectField> Fields { get; init; } = [];

    public string DisplayName
    {
        get
        {
            ReadOnlySpan<byte> bytes = RawName;
            if (!bytes.IsEmpty && bytes[^1] == 0)
                bytes = bytes[..^1];
            return Encoding.UTF8.GetString(bytes);
        }
    }
}

public sealed class SmoProjectField
{
    public int FieldType { get; init; }
    public int Occurrence { get; init; }
    public byte RawHeader { get; init; }
    public byte SizeKind { get; init; }
    public int HeaderSize { get; init; }
    public uint PayloadSize { get; init; }
    public int RelativeHeaderOffset { get; init; }
    public int RelativePayloadOffset { get; init; }

    internal SmoObjectField ToFieldView(SmoProjectObject owner) => new(
        owner.Index,
        owner.Id,
        FieldType,
        Occurrence,
        RawHeader,
        (SmoDataBlockSizeCode)SizeKind,
        HeaderSize,
        PayloadSize,
        RelativeHeaderOffset,
        RelativePayloadOffset,
        0,
        0,
        ReadOnlyMemory<byte>.Empty);
}

public sealed class SmoProjectPropertyEdit
{
    public uint ObjectId { get; init; }
    public required string PropertyKey { get; init; }
    public SmoPropertyValueKind ValueKind { get; init; }
    public byte[] Value { get; set; } = [];
}

public sealed class SmoProjectAddedObjectPropertyEdit
{
    public Guid BlobId { get; init; }
    public uint ObjectId { get; init; }
    public required string PropertyKey { get; init; }
    public SmoPropertyValueKind ValueKind { get; init; }
    public byte[] Value { get; set; } = [];
}

public sealed class SmoProjectBranchRemoval
{
    public uint RootObjectId { get; init; }
}

public sealed class SmoProjectResourceRelocation
{
    public uint ObjectId { get; init; }
    public uint TargetOwnerId { get; init; }
    public int FieldType { get; init; }
    public int FieldOccurrence { get; init; }
}

public sealed class SmoProjectResourceRedirect
{
    public uint SourceObjectId { get; init; }
    public uint TargetObjectId { get; init; }
}

public sealed class SmoProjectCollisionLinkOverride
{
    public uint VisualObjectId { get; set; }
    public uint CollisionObjectId { get; set; }
    public bool Present { get; init; }
}

public sealed class SmoProjectReferencePlacement
{
    public uint TemplateRootObjectId { get; init; }
    public uint TargetOwnerId { get; init; }
    public List<uint> NewObjectIds { get; init; } = [];
    public byte[] RootRawName { get; init; } = [];
    public byte[] WorldMatrix { get; set; } = [];
    public byte[] InverseWorldMatrix { get; set; } = [];
    public uint? ResourceObjectIdOverride { get; init; }
}

public sealed class SmoProjectAddedForest
{
    public Guid BlobId { get; init; }
    public required string BlobSha256 { get; init; }
    public uint TargetOwnerId { get; init; }
    public int InsertionRelativeOffset { get; init; }
    public List<SmoProjectAddedObject> Objects { get; init; } = [];
}

public sealed class SmoProjectAddedObject
{
    public uint Id { get; init; }
    public byte[] RawName { get; init; } = [];
    public uint TypeHash { get; init; }
    public uint ObjectRelativeOffset { get; init; }
    public uint SerializedSize { get; init; }
    public uint? ParentObjectId { get; init; }
}

internal sealed record SmoProjectDataChange(
    int Offset,
    int OldLength,
    byte[] Replacement,
    int ObjectIndex,
    string Label,
    int Sequence)
{
    public int OwnerIndex => ObjectIndex;
    public int Delta => checked(Replacement.Length - OldLength);

    public static int Compare(
        SmoProjectDataChange left,
        SmoProjectDataChange right)
    {
        int offset = left.Offset.CompareTo(right.Offset);
        return offset != 0 ? offset : left.Sequence.CompareTo(right.Sequence);
    }
}

internal sealed record SmoProjectLayoutEntry(
    uint Id,
    byte[] RawName,
    uint TypeHash,
    uint LogicalOffset,
    uint SerializedSize,
    int? ParentIndex);

internal sealed record SmoProjectDirectoryDraft(
    uint Id,
    byte[] RawName,
    uint TypeHash,
    uint LogicalOffset,
    uint SerializedSize,
    uint? ParentObjectId,
    int SortKey);

internal sealed record SmoProjectRelocationLayout(
    int TargetOwnerIndex,
    SmoProjectDataChange TargetChange,
    int ObjectRelativeOffset);

internal sealed record SmoProjectAddedObjectLayout(
    uint Id,
    byte[] RawName,
    uint TypeHash,
    uint ObjectRelativeOffset,
    uint SerializedSize,
    uint? ParentObjectId,
    SmoProjectDataChange Change);

internal sealed record SmoProjectReferencePlacementData(
    byte[] Data,
    IReadOnlyList<SmoProjectAddedObjectLayout> Objects);

internal sealed record BuiltReferenceObject(
    byte[] Data,
    IReadOnlyList<BuiltReferenceObjectLayout> Layouts);

internal sealed record BuiltReferenceObjectLayout(
    SmoProjectObject Source,
    int RelativeOffset,
    uint SerializedSize,
    uint ParentId);

internal sealed record SmoProjectExternalReference(
    SmoProjectObject Owner,
    uint ReferencedObjectId,
    int FieldType,
    int FieldOccurrence);

internal sealed record SmoProjectLayoutPlan(
    IReadOnlyList<SmoProjectLayoutEntry> Entries,
    IReadOnlyList<SmoProjectDataChange> Changes,
    int DataLength);

public static class SmoProjectArchive
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        Converters = { new JsonStringEnumConverter() }
    };

    public static void Save(SmoProject project, string projectPath)
    {
        ArgumentNullException.ThrowIfNull(project);
        ArgumentException.ThrowIfNullOrWhiteSpace(projectPath);
        project.Validate();
        string output = Path.GetFullPath(projectPath);
        string? directory = Path.GetDirectoryName(output);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
            throw new DirectoryNotFoundException(directory);
        string temporary = Path.Combine(
            directory,
            $".{Path.GetFileName(output)}.{Guid.NewGuid():N}.tmp");
        try
        {
            SmoProjectLayoutPlan plan = project.BuildLayoutPlan();
            using (var stream = new FileStream(
                       temporary,
                       FileMode.CreateNew,
                       FileAccess.ReadWrite,
                       FileShare.None,
                       1024 * 1024,
                       FileOptions.SequentialScan))
            using (var archive = new ZipArchive(
                       stream,
                       ZipArchiveMode.Create,
                       leaveOpen: false,
                       entryNameEncoding: Encoding.UTF8))
            {
                ZipArchiveEntry manifestEntry = archive.CreateEntry(
                    SmoProject.ManifestEntryName,
                    CompressionLevel.Fastest);
                using (Stream manifestStream = manifestEntry.Open())
                {
                    JsonSerializer.Serialize(
                        manifestStream,
                        project.Manifest,
                        JsonOptions);
                }

                ZipArchiveEntry dataEntry = archive.CreateEntry(
                    SmoProject.DataEntryName,
                    CompressionLevel.NoCompression);
                using (Stream dataStream = dataEntry.Open())
                    dataStream.Write(project.DataSection.Span);

                foreach (SmoProjectAddedForest forest in
                         project.Manifest.AddedForests)
                {
                    ZipArchiveEntry assetEntry = archive.CreateEntry(
                        SmoProject.GetAssetEntryName(forest.BlobId),
                        CompressionLevel.NoCompression);
                    using Stream assetStream = assetEntry.Open();
                    assetStream.Write(project.GetAssetBlob(forest.BlobId).Span);
                }
                foreach (SmoProjectObjectDataReplacement replacement in
                         project.Manifest.ObjectDataReplacements)
                {
                    ZipArchiveEntry assetEntry = archive.CreateEntry(
                        SmoProject.GetAssetEntryName(replacement.BlobId),
                        CompressionLevel.NoCompression);
                    using Stream assetStream = assetEntry.Open();
                    assetStream.Write(project.GetAssetBlob(replacement.BlobId).Span);
                }
            }

            _ = Load(temporary);
            File.Move(temporary, output, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary))
                File.Delete(temporary);
        }
    }

    public static SmoProject Load(string projectPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(projectPath);
        using var stream = new FileStream(
            Path.GetFullPath(projectPath),
            FileMode.Open,
            FileAccess.Read,
            FileShare.Read,
            1024 * 1024,
            FileOptions.SequentialScan);
        using var archive = new ZipArchive(
            stream,
            ZipArchiveMode.Read,
            leaveOpen: false,
            entryNameEncoding: Encoding.UTF8);
        ZipArchiveEntry manifestEntry = archive.GetEntry(
            SmoProject.ManifestEntryName) ??
            throw new InvalidDataException("The project has no project.json entry.");
        SmoProjectManifest manifest;
        using (Stream manifestStream = manifestEntry.Open())
        {
            manifest = JsonSerializer.Deserialize<SmoProjectManifest>(
                manifestStream,
                JsonOptions) ??
                throw new InvalidDataException("The project manifest is empty.");
        }

        ZipArchiveEntry dataEntry = archive.GetEntry(
            SmoProject.DataEntryName) ??
            throw new InvalidDataException("The project has no data.bin entry.");
        if (dataEntry.Length > int.MaxValue)
            throw new NotSupportedException("The project data section exceeds 2 GiB.");
        byte[] data = new byte[checked((int)dataEntry.Length)];
        using (Stream dataStream = dataEntry.Open())
            dataStream.ReadExactly(data);
        var assets = new Dictionary<Guid, byte[]>();
        foreach (SmoProjectAddedForest forest in manifest.AddedForests)
        {
            string entryName = SmoProject.GetAssetEntryName(forest.BlobId);
            ZipArchiveEntry assetEntry = archive.GetEntry(entryName) ??
                throw new InvalidDataException(
                    $"The project has no {entryName} entry.");
            if (assetEntry.Length > int.MaxValue)
                throw new NotSupportedException(
                    $"Project asset {forest.BlobId:N} exceeds 2 GiB.");
            if (assets.ContainsKey(forest.BlobId))
                throw new InvalidDataException(
                    $"Project asset {forest.BlobId:N} is referenced more than once.");
            byte[] asset = new byte[checked((int)assetEntry.Length)];
            using (Stream assetStream = assetEntry.Open())
                assetStream.ReadExactly(asset);
            assets.Add(forest.BlobId, asset);
        }
        foreach (SmoProjectObjectDataReplacement replacement in
                 manifest.ObjectDataReplacements)
        {
            string entryName = SmoProject.GetAssetEntryName(replacement.BlobId);
            ZipArchiveEntry assetEntry = archive.GetEntry(entryName) ??
                throw new InvalidDataException($"The project has no {entryName} entry.");
            if (assetEntry.Length > int.MaxValue)
                throw new NotSupportedException(
                    $"Project asset {replacement.BlobId:N} exceeds 2 GiB.");
            if (assets.ContainsKey(replacement.BlobId))
                throw new InvalidDataException(
                    $"Project asset {replacement.BlobId:N} is referenced more than once.");
            byte[] asset = new byte[checked((int)assetEntry.Length)];
            using (Stream assetStream = assetEntry.Open())
                assetStream.ReadExactly(asset);
            assets.Add(replacement.BlobId, asset);
        }
        return new SmoProject(manifest, data, assets);
    }
}

public sealed record SmoProjectCollisionTransformEdit(
    uint CollisionInfoObjectId,
    Matrix4x4 OriginalWorldTransform,
    Matrix4x4 WorldTransform);

public sealed record SmoProjectBuildResult(
    string OutputPath,
    long FileSize,
    int ObjectCount,
    string Sha256,
    bool IsByteIdenticalToImportedSource,
    string? BackupPath = null);

public static class SmoProjectSerializer
{
    /// <summary>
    /// Materializes the current journal state in memory for preview and
    /// importer adapters. Final saves still stream directly to disk.
    /// </summary>
    public static SmoDocument CreateCurrentDocument(SmoProject project)
    {
        ArgumentNullException.ThrowIfNull(project);
        project.Validate();
        SmoProjectLayoutPlan plan = project.BuildLayoutPlan();
        byte[] container = CreateContainer(project, plan);
        SmoDocument current = SmoDocument.ParseOwned(
            container,
            project.Manifest.SourcePathHint ?? project.Manifest.SourceFileName);
        Verify(project, plan, current);
        return current;
    }

    public static SmoDocument CreateImportedSourceDocument(
        SmoProject project)
    {
        ArgumentNullException.ThrowIfNull(project);
        project.Validate();
        SmoProjectLayoutEntry[] entries = project.Objects.Select(entry =>
            new SmoProjectLayoutEntry(
                entry.Id,
                entry.RawName,
                entry.TypeHash,
                entry.LogicalOffset,
                entry.SerializedSize,
                entry.ParentIndex)).ToArray();
        var plan = new SmoProjectLayoutPlan(
            entries,
            [],
            project.DataSection.Length);
        byte[] container = CreateContainer(project, plan);
        SmoDocument source = SmoDocument.ParseOwned(
            container,
            project.Manifest.SourcePathHint ?? project.Manifest.SourceFileName);
        Verify(project, plan, source);
        if (!string.Equals(
                SmoProject.Hash(source.Data.Span),
                project.Manifest.SourceSha256,
                StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException(
                "The reconstructed project base does not match its source SHA-256.");
        }
        return source;
    }

    public static SmoProjectBuildResult Build(
        SmoProject project,
        string outputPath)
    {
        ArgumentNullException.ThrowIfNull(project);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        project.Validate();
        string output = Path.GetFullPath(outputPath);
        string? directory = Path.GetDirectoryName(output);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
            throw new DirectoryNotFoundException(directory);
        string temporary = Path.Combine(
            directory,
            $".{Path.GetFileName(output)}.{Guid.NewGuid():N}.tmp");
        try
        {
            SmoProjectLayoutPlan plan = project.BuildLayoutPlan();
            using (var stream = new FileStream(
                       temporary,
                       FileMode.CreateNew,
                       FileAccess.Write,
                       FileShare.None,
                       1024 * 1024,
                       FileOptions.SequentialScan))
            {
                Write(project, plan, stream);
                stream.Flush(flushToDisk: true);
            }

            SmoDocument verified = SmoDocument.Load(temporary);
            Verify(project, plan, verified);
            string hash = SmoProject.Hash(verified.Data.Span);
            long fileSize = verified.Data.Length;
            int objectCount = verified.Objects.Count;
            verified = null!;
            string? backupPath = null;
            if (File.Exists(output))
            {
                backupPath = output +
                    $".{DateTime.Now:yyyyMMdd-HHmmss-fff}.bak";
                File.Replace(
                    temporary,
                    output,
                    backupPath,
                    ignoreMetadataErrors: true);
            }
            else
            {
                File.Move(temporary, output);
            }
            using (var finalStream = new FileStream(
                       output,
                       FileMode.Open,
                       FileAccess.Read,
                       FileShare.Read,
                       1024 * 1024,
                       FileOptions.SequentialScan))
            {
                string installedHash = Convert.ToHexString(
                    System.Security.Cryptography.SHA256.HashData(finalStream));
                if (!string.Equals(
                        installedHash,
                        hash,
                        StringComparison.OrdinalIgnoreCase))
                {
                    throw new InvalidDataException(
                        "Installed project SMO differs from the verified temporary file.");
                }
            }
            return new SmoProjectBuildResult(
                output,
                fileSize,
                objectCount,
                hash,
                string.Equals(
                    hash,
                    project.Manifest.SourceSha256,
                    StringComparison.OrdinalIgnoreCase),
                backupPath);
        }
        finally
        {
            if (File.Exists(temporary))
                File.Delete(temporary);
        }
    }

    public static void Write(SmoProject project, Stream output)
    {
        ArgumentNullException.ThrowIfNull(project);
        ArgumentNullException.ThrowIfNull(output);
        if (!output.CanWrite)
            throw new ArgumentException("The output stream is not writable.", nameof(output));
        project.Validate();
        Write(project, project.BuildLayoutPlan(), output);
    }

    private static void Write(
        SmoProject project,
        SmoProjectLayoutPlan plan,
        Stream output)
    {
        CreateEnvelope(project, plan).WritePrefix(output);
        WriteDataSection(project, plan, output);
    }

    private static SmoContainerEnvelope CreateEnvelope(SmoProject project, SmoProjectLayoutPlan plan) =>
        new(project.Manifest.Header.SerializerVersion, project.Manifest.Header.Unknown08,
            project.Manifest.Header.PlatformMask,
            plan.Entries.Select(entry => new SmoContainerEntry(entry.Id, entry.RawName,
                entry.TypeHash, entry.LogicalOffset, entry.SerializedSize)).ToArray(), plan.DataLength);

    private static byte[] CreateContainer(SmoProject project, SmoProjectLayoutPlan plan)
    {
        SmoContainerEnvelope envelope = CreateEnvelope(project, plan);
        byte[] container = envelope.AllocateContainer();
        using var stream = new MemoryStream(container, writable: true);
        stream.Position = envelope.DataStart;
        WriteDataSection(project, plan, stream);
        return container;
    }

    private static void Verify(
        SmoProject project,
        SmoProjectLayoutPlan plan,
        SmoDocument rebuilt)
    {
        if (rebuilt.HasErrors)
            throw new InvalidDataException(
                "The project serializer produced an invalid SMO container.");
        if (rebuilt.Objects.Count != plan.Entries.Count)
            throw new InvalidDataException(
                "The project serializer changed the object count.");
        for (int index = 0; index < plan.Entries.Count; index++)
        {
            SmoProjectLayoutEntry expected = plan.Entries[index];
            SmoObjectEntry actual = rebuilt.Objects[index];
            if (actual.Id != expected.Id ||
                actual.TypeHash != expected.TypeHash ||
                actual.LogicalOffset != expected.LogicalOffset ||
                actual.SerializedSize != expected.SerializedSize ||
                actual.ParentIndex != expected.ParentIndex ||
                !actual.RawName.Span.SequenceEqual(expected.RawName))
            {
                throw new InvalidDataException(
                    $"Project round-trip changed object-directory entry {index}: " +
                    $"expected id={expected.Id}, type=0x{expected.TypeHash:X8}, " +
                    $"offset={expected.LogicalOffset}, size={expected.SerializedSize}, " +
                    $"parent={expected.ParentIndex?.ToString() ?? "root"}; " +
                    $"actual id={actual.Id}, type=0x{actual.TypeHash:X8}, " +
                    $"offset={actual.LogicalOffset}, size={actual.SerializedSize}, " +
                    $"parent={actual.ParentIndex?.ToString() ?? "root"}; " +
                    $"expected-parent-entry=" +
                    (expected.ParentIndex is int expectedParent
                        ? $"{plan.Entries[expectedParent].Id}/" +
                          $"{plan.Entries[expectedParent].LogicalOffset}/" +
                          $"{plan.Entries[expectedParent].SerializedSize}"
                        : "none") + "; actual-parent-entry=" +
                    (actual.ParentIndex is int actualParent
                        ? $"{rebuilt.Objects[actualParent].Id}/" +
                          $"{rebuilt.Objects[actualParent].LogicalOffset}/" +
                          $"{rebuilt.Objects[actualParent].SerializedSize}"
                        : "none") + ".");
            }
        }
        int dataStart = checked((int)rebuilt.Header.DataStart);
        ReadOnlySpan<byte> actualData = rebuilt.Data.Span[dataStart..];
        if (actualData.Length != plan.DataLength)
            throw new InvalidDataException(
                "Project build produced an unexpected data-section size.");
        int sourceCursor = 0;
        int destinationCursor = 0;
        foreach (SmoProjectDataChange change in plan.Changes)
        {
            int unchangedLength = change.Offset - sourceCursor;
            if (!actualData.Slice(destinationCursor, unchangedLength).SequenceEqual(
                    project.DataSection.Span.Slice(sourceCursor, unchangedLength)))
            {
                throw new InvalidDataException(
                    $"Project layout changed bytes before {change.Label}.");
            }
            destinationCursor += unchangedLength;
            if (!actualData.Slice(
                    destinationCursor,
                    change.Replacement.Length).SequenceEqual(change.Replacement))
            {
                throw new InvalidDataException(
                    $"Project change {change.ObjectIndex}:{change.Label} " +
                    "failed verification.");
            }
            destinationCursor += change.Replacement.Length;
            sourceCursor = checked(change.Offset + change.OldLength);
        }
        if (!actualData[destinationCursor..].SequenceEqual(
                project.DataSection.Span[sourceCursor..]))
            throw new InvalidDataException(
                "Project build changed bytes after its final layout operation.");
    }

    private static void WriteDataSection(
        SmoProject project,
        SmoProjectLayoutPlan plan,
        Stream output)
    {
        int cursor = 0;
        foreach (SmoProjectDataChange change in plan.Changes)
        {
            output.Write(project.DataSection.Span[cursor..change.Offset]);
            output.Write(change.Replacement);
            cursor = checked(change.Offset + change.OldLength);
        }
        output.Write(project.DataSection.Span[cursor..]);
    }

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);
}
