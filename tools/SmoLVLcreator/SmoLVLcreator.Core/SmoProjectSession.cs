namespace SmoLVLcreator.Core;

/// <summary>
/// Transactional editor session over an project. History contains
/// only copies of the compact operation journal; immutable data.bin and the
/// imported object catalog are never duplicated for undo/redo.
/// </summary>
public sealed class SmoProjectSession
{
    private const int MaximumHistoryTransactions = 256;
    private readonly List<SmoProjectTransaction> _undo = [];
    private readonly List<SmoProjectTransaction> _redo = [];
    private SmoProjectOperationState _savedState;

    public SmoProjectSession(SmoProject project)
    {
        Project = project ?? throw new ArgumentNullException(nameof(project));
        Project.Validate();
        _savedState = Capture();
    }

    public SmoProject Project { get; }
    public bool CanUndo => _undo.Count > 0;
    public bool CanRedo => _redo.Count > 0;
    public string? UndoLabel => CanUndo ? _undo[^1].Label : null;
    public string? RedoLabel => CanRedo ? _redo[^1].Label : null;
    public int UndoCount => _undo.Count;
    public int RedoCount => _redo.Count;
    public bool IsModified => !Capture().IsEquivalentTo(_savedState);

    public bool Execute(string label, Action<SmoProject> edit)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(label);
        ArgumentNullException.ThrowIfNull(edit);
        SmoProjectOperationState before = Capture();
        try
        {
            edit(Project);
            Project.Validate();
            SmoProjectOperationState after = Capture();
            if (before.IsEquivalentTo(after))
                return false;
            _undo.Add(new SmoProjectTransaction(
                label.Trim(),
                before,
                after));
            _redo.Clear();
            if (_undo.Count > MaximumHistoryTransactions)
            {
                _undo.RemoveRange(
                    0,
                    _undo.Count - MaximumHistoryTransactions);
            }
            PruneUnreferencedHistoryAssets();
            return true;
        }
        catch
        {
            Restore(before);
            Project.Validate();
            throw;
        }
    }

    public bool Undo()
    {
        if (!CanUndo)
            return false;
        SmoProjectTransaction transaction = _undo[^1];
        Restore(transaction.Before);
        Project.Validate();
        _undo.RemoveAt(_undo.Count - 1);
        _redo.Add(transaction);
        return true;
    }

    public bool Redo()
    {
        if (!CanRedo)
            return false;
        SmoProjectTransaction transaction = _redo[^1];
        Restore(transaction.After);
        Project.Validate();
        _redo.RemoveAt(_redo.Count - 1);
        _undo.Add(transaction);
        return true;
    }

    /// <summary>
    /// Accepts the current project state as the new session baseline. This
    /// releases all history snapshots without modifying the project archive or
    /// its immutable data section.
    /// </summary>
    public void CompactHistory()
    {
        _undo.Clear();
        _redo.Clear();
        Project.PruneUnreferencedAssetBlobs();
    }

    public void MarkSaved() => _savedState = Capture();

    private void PruneUnreferencedHistoryAssets()
    {
        IEnumerable<Guid> historyIds = _undo
            .Concat(_redo)
            .SelectMany(transaction =>
                transaction.Before.AssetIds().Concat(transaction.After.AssetIds()));
        Project.PruneUnreferencedAssetBlobs(historyIds);
    }

    private SmoProjectOperationState Capture() => new(
        Project.Manifest.PropertyEdits.Select(Clone).ToList(),
        Project.Manifest.AddedObjectPropertyEdits.Select(Clone).ToList(),
        Project.Manifest.ResourceRelocations.Select(Clone).ToList(),
        Project.Manifest.ResourceRedirects.Select(Clone).ToList(),
        Project.Manifest.BranchRemovals.Select(Clone).ToList(),
        Project.Manifest.ReferencePlacements.Select(Clone).ToList(),
        Project.Manifest.CollisionLinkOverrides.Select(Clone).ToList(),
        Project.Manifest.AddedForests.Select(Clone).ToList(),
        Project.Manifest.RemovedAddedForestIds.ToList(),
        Project.Manifest.ObjectDataReplacements.Select(Clone).ToList());

    private void Restore(SmoProjectOperationState state)
    {
        Replace(Project.Manifest.PropertyEdits, state.PropertyEdits, Clone);
        Replace(
            Project.Manifest.AddedObjectPropertyEdits,
            state.AddedObjectPropertyEdits,
            Clone);
        Replace(Project.Manifest.ResourceRelocations, state.ResourceRelocations, Clone);
        Replace(Project.Manifest.ResourceRedirects, state.ResourceRedirects, Clone);
        Replace(Project.Manifest.BranchRemovals, state.BranchRemovals, Clone);
        Replace(Project.Manifest.ReferencePlacements, state.ReferencePlacements, Clone);
        Replace(
            Project.Manifest.CollisionLinkOverrides,
            state.CollisionLinkOverrides,
            Clone);
        Replace(Project.Manifest.AddedForests, state.AddedForests, Clone);
        Project.Manifest.RemovedAddedForestIds.Clear();
        Project.Manifest.RemovedAddedForestIds.AddRange(state.RemovedAddedForestIds);
        Replace(
            Project.Manifest.ObjectDataReplacements,
            state.ObjectDataReplacements,
            Clone);
    }

    private static void Replace<T>(
        List<T> destination,
        IReadOnlyList<T> source,
        Func<T, T> clone)
    {
        destination.Clear();
        destination.AddRange(source.Select(clone));
    }

    private static SmoProjectPropertyEdit Clone(
        SmoProjectPropertyEdit item) => new()
    {
        ObjectId = item.ObjectId,
        PropertyKey = item.PropertyKey,
        ValueKind = item.ValueKind,
        Value = item.Value.ToArray()
    };

    private static SmoProjectAddedObjectPropertyEdit Clone(
        SmoProjectAddedObjectPropertyEdit item) => new()
    {
        BlobId = item.BlobId,
        ObjectId = item.ObjectId,
        PropertyKey = item.PropertyKey,
        ValueKind = item.ValueKind,
        Value = item.Value.ToArray()
    };

    private static SmoProjectResourceRelocation Clone(
        SmoProjectResourceRelocation item) => new()
    {
        ObjectId = item.ObjectId,
        TargetOwnerId = item.TargetOwnerId,
        FieldType = item.FieldType,
        FieldOccurrence = item.FieldOccurrence
    };

    private static SmoProjectResourceRedirect Clone(
        SmoProjectResourceRedirect item) => new()
    {
        SourceObjectId = item.SourceObjectId,
        TargetObjectId = item.TargetObjectId
    };

    private static SmoProjectBranchRemoval Clone(
        SmoProjectBranchRemoval item) => new()
    {
        RootObjectId = item.RootObjectId
    };

    private static SmoProjectReferencePlacement Clone(
        SmoProjectReferencePlacement item) => new()
    {
        TemplateRootObjectId = item.TemplateRootObjectId,
        TargetOwnerId = item.TargetOwnerId,
        NewObjectIds = item.NewObjectIds.ToList(),
        RootRawName = item.RootRawName.ToArray(),
        WorldMatrix = item.WorldMatrix.ToArray(),
        InverseWorldMatrix = item.InverseWorldMatrix.ToArray(),
        ResourceObjectIdOverride = item.ResourceObjectIdOverride
    };

    private static SmoProjectCollisionLinkOverride Clone(
        SmoProjectCollisionLinkOverride item) => new()
    {
        VisualObjectId = item.VisualObjectId,
        CollisionObjectId = item.CollisionObjectId,
        Present = item.Present
    };

    private static SmoProjectAddedForest Clone(
        SmoProjectAddedForest item) => new()
    {
        BlobId = item.BlobId,
        BlobSha256 = item.BlobSha256,
        TargetOwnerId = item.TargetOwnerId,
        InsertionRelativeOffset = item.InsertionRelativeOffset,
        Objects = item.Objects.Select(Clone).ToList()
    };

    private static SmoProjectAddedObject Clone(
        SmoProjectAddedObject item) => new()
    {
        Id = item.Id,
        RawName = item.RawName.ToArray(),
        TypeHash = item.TypeHash,
        ObjectRelativeOffset = item.ObjectRelativeOffset,
        SerializedSize = item.SerializedSize,
        ParentObjectId = item.ParentObjectId
    };

    private static SmoProjectObjectDataReplacement Clone(
        SmoProjectObjectDataReplacement item) => new()
    {
        ObjectId = item.ObjectId,
        BlobId = item.BlobId,
        BlobSha256 = item.BlobSha256
    };
}

internal sealed record SmoProjectTransaction(
    string Label,
    SmoProjectOperationState Before,
    SmoProjectOperationState After);

internal sealed record SmoProjectOperationState(
    IReadOnlyList<SmoProjectPropertyEdit> PropertyEdits,
    IReadOnlyList<SmoProjectAddedObjectPropertyEdit> AddedObjectPropertyEdits,
    IReadOnlyList<SmoProjectResourceRelocation> ResourceRelocations,
    IReadOnlyList<SmoProjectResourceRedirect> ResourceRedirects,
    IReadOnlyList<SmoProjectBranchRemoval> BranchRemovals,
    IReadOnlyList<SmoProjectReferencePlacement> ReferencePlacements,
    IReadOnlyList<SmoProjectCollisionLinkOverride> CollisionLinkOverrides,
    IReadOnlyList<SmoProjectAddedForest> AddedForests,
    IReadOnlyList<Guid> RemovedAddedForestIds,
    IReadOnlyList<SmoProjectObjectDataReplacement> ObjectDataReplacements)
{
    public IEnumerable<Guid> AssetIds() =>
        AddedForests.Select(item => item.BlobId)
            .Concat(ObjectDataReplacements.Select(item => item.BlobId));

    public bool IsEquivalentTo(SmoProjectOperationState other) =>
        PropertyEdits.SequenceEqual(other.PropertyEdits, PropertyEditComparer.Instance) &&
        AddedObjectPropertyEdits.SequenceEqual(
            other.AddedObjectPropertyEdits,
            AddedObjectPropertyEditComparer.Instance) &&
        ResourceRelocations.SequenceEqual(
            other.ResourceRelocations,
            ResourceRelocationComparer.Instance) &&
        ResourceRedirects.SequenceEqual(
            other.ResourceRedirects,
            ResourceRedirectComparer.Instance) &&
        BranchRemovals.SequenceEqual(
            other.BranchRemovals,
            BranchRemovalComparer.Instance) &&
        ReferencePlacements.SequenceEqual(
            other.ReferencePlacements,
            ReferencePlacementComparer.Instance) &&
        CollisionLinkOverrides.SequenceEqual(
            other.CollisionLinkOverrides,
            CollisionLinkOverrideComparer.Instance) &&
        AddedForests.SequenceEqual(
            other.AddedForests,
            AddedForestComparer.Instance) &&
        RemovedAddedForestIds.SequenceEqual(other.RemovedAddedForestIds) &&
        ObjectDataReplacements.SequenceEqual(
            other.ObjectDataReplacements,
            ObjectDataReplacementComparer.Instance);

    private sealed class ObjectDataReplacementComparer :
        IEqualityComparer<SmoProjectObjectDataReplacement>
    {
        public static ObjectDataReplacementComparer Instance { get; } = new();

        public bool Equals(
            SmoProjectObjectDataReplacement? left,
            SmoProjectObjectDataReplacement? right) =>
            ReferenceEquals(left, right) ||
            left is not null && right is not null &&
            left.ObjectId == right.ObjectId &&
            left.BlobId == right.BlobId &&
            left.BlobSha256 == right.BlobSha256;

        public int GetHashCode(SmoProjectObjectDataReplacement item) =>
            HashCode.Combine(item.ObjectId, item.BlobId, item.BlobSha256);
    }

    private sealed class PropertyEditComparer :
        IEqualityComparer<SmoProjectPropertyEdit>
    {
        public static PropertyEditComparer Instance { get; } = new();

        public bool Equals(
            SmoProjectPropertyEdit? left,
            SmoProjectPropertyEdit? right) =>
            ReferenceEquals(left, right) ||
            left is not null && right is not null &&
            left.ObjectId == right.ObjectId &&
            left.PropertyKey == right.PropertyKey &&
            left.ValueKind == right.ValueKind &&
            left.Value.AsSpan().SequenceEqual(right.Value);

        public int GetHashCode(SmoProjectPropertyEdit item) =>
            HashCode.Combine(item.ObjectId, item.PropertyKey, item.ValueKind);
    }

    private sealed class ResourceRelocationComparer :
        IEqualityComparer<SmoProjectResourceRelocation>
    {
        public static ResourceRelocationComparer Instance { get; } = new();

        public bool Equals(
            SmoProjectResourceRelocation? left,
            SmoProjectResourceRelocation? right) =>
            ReferenceEquals(left, right) ||
            left is not null && right is not null &&
            left.ObjectId == right.ObjectId &&
            left.TargetOwnerId == right.TargetOwnerId &&
            left.FieldType == right.FieldType &&
            left.FieldOccurrence == right.FieldOccurrence;

        public int GetHashCode(SmoProjectResourceRelocation item) =>
            HashCode.Combine(
                item.ObjectId,
                item.TargetOwnerId,
                item.FieldType,
                item.FieldOccurrence);
    }

    private sealed class AddedObjectPropertyEditComparer :
        IEqualityComparer<SmoProjectAddedObjectPropertyEdit>
    {
        public static AddedObjectPropertyEditComparer Instance { get; } = new();

        public bool Equals(
            SmoProjectAddedObjectPropertyEdit? left,
            SmoProjectAddedObjectPropertyEdit? right) =>
            ReferenceEquals(left, right) ||
            left is not null && right is not null &&
            left.BlobId == right.BlobId &&
            left.ObjectId == right.ObjectId &&
            left.PropertyKey == right.PropertyKey &&
            left.ValueKind == right.ValueKind &&
            left.Value.AsSpan().SequenceEqual(right.Value);

        public int GetHashCode(SmoProjectAddedObjectPropertyEdit item) =>
            HashCode.Combine(
                item.BlobId,
                item.ObjectId,
                item.PropertyKey,
                item.ValueKind);
    }

    private sealed class ResourceRedirectComparer :
        IEqualityComparer<SmoProjectResourceRedirect>
    {
        public static ResourceRedirectComparer Instance { get; } = new();

        public bool Equals(
            SmoProjectResourceRedirect? left,
            SmoProjectResourceRedirect? right) =>
            ReferenceEquals(left, right) ||
            left is not null && right is not null &&
            left.SourceObjectId == right.SourceObjectId &&
            left.TargetObjectId == right.TargetObjectId;

        public int GetHashCode(SmoProjectResourceRedirect item) =>
            HashCode.Combine(item.SourceObjectId, item.TargetObjectId);
    }

    private sealed class BranchRemovalComparer :
        IEqualityComparer<SmoProjectBranchRemoval>
    {
        public static BranchRemovalComparer Instance { get; } = new();

        public bool Equals(
            SmoProjectBranchRemoval? left,
            SmoProjectBranchRemoval? right) =>
            ReferenceEquals(left, right) ||
            left is not null && right is not null &&
            left.RootObjectId == right.RootObjectId;

        public int GetHashCode(SmoProjectBranchRemoval item) =>
            item.RootObjectId.GetHashCode();
    }

    private sealed class ReferencePlacementComparer :
        IEqualityComparer<SmoProjectReferencePlacement>
    {
        public static ReferencePlacementComparer Instance { get; } = new();

        public bool Equals(
            SmoProjectReferencePlacement? left,
            SmoProjectReferencePlacement? right) =>
            ReferenceEquals(left, right) ||
            left is not null && right is not null &&
            left.TemplateRootObjectId == right.TemplateRootObjectId &&
            left.TargetOwnerId == right.TargetOwnerId &&
            left.ResourceObjectIdOverride == right.ResourceObjectIdOverride &&
            left.NewObjectIds.SequenceEqual(right.NewObjectIds) &&
            left.RootRawName.AsSpan().SequenceEqual(right.RootRawName) &&
            left.WorldMatrix.AsSpan().SequenceEqual(right.WorldMatrix) &&
            left.InverseWorldMatrix.AsSpan().SequenceEqual(right.InverseWorldMatrix);

        public int GetHashCode(SmoProjectReferencePlacement item) =>
            HashCode.Combine(
                item.TemplateRootObjectId,
                item.TargetOwnerId,
                item.ResourceObjectIdOverride);
    }

    private sealed class CollisionLinkOverrideComparer :
        IEqualityComparer<SmoProjectCollisionLinkOverride>
    {
        public static CollisionLinkOverrideComparer Instance { get; } = new();

        public bool Equals(
            SmoProjectCollisionLinkOverride? left,
            SmoProjectCollisionLinkOverride? right) =>
            ReferenceEquals(left, right) ||
            left is not null && right is not null &&
            left.VisualObjectId == right.VisualObjectId &&
            left.CollisionObjectId == right.CollisionObjectId &&
            left.Present == right.Present;

        public int GetHashCode(SmoProjectCollisionLinkOverride item) =>
            HashCode.Combine(
                item.VisualObjectId,
                item.CollisionObjectId,
                item.Present);
    }

    private sealed class AddedForestComparer :
        IEqualityComparer<SmoProjectAddedForest>
    {
        public static AddedForestComparer Instance { get; } = new();

        public bool Equals(
            SmoProjectAddedForest? left,
            SmoProjectAddedForest? right) =>
            ReferenceEquals(left, right) ||
            left is not null && right is not null &&
            left.BlobId == right.BlobId &&
            left.BlobSha256 == right.BlobSha256 &&
            left.TargetOwnerId == right.TargetOwnerId &&
            left.InsertionRelativeOffset == right.InsertionRelativeOffset &&
            left.Objects.SequenceEqual(right.Objects, AddedObjectComparer.Instance);

        public int GetHashCode(SmoProjectAddedForest item) =>
            HashCode.Combine(item.BlobId, item.TargetOwnerId,
                item.InsertionRelativeOffset);
    }

    private sealed class AddedObjectComparer :
        IEqualityComparer<SmoProjectAddedObject>
    {
        public static AddedObjectComparer Instance { get; } = new();

        public bool Equals(
            SmoProjectAddedObject? left,
            SmoProjectAddedObject? right) =>
            ReferenceEquals(left, right) ||
            left is not null && right is not null &&
            left.Id == right.Id &&
            left.RawName.AsSpan().SequenceEqual(right.RawName) &&
            left.TypeHash == right.TypeHash &&
            left.ObjectRelativeOffset == right.ObjectRelativeOffset &&
            left.SerializedSize == right.SerializedSize &&
            left.ParentObjectId == right.ParentObjectId;

        public int GetHashCode(SmoProjectAddedObject item) =>
            HashCode.Combine(item.Id, item.TypeHash, item.ObjectRelativeOffset,
                item.SerializedSize, item.ParentObjectId);
    }
}
