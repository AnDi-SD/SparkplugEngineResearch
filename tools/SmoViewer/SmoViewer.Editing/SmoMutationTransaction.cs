using System.Buffers.Binary;
using System.Collections.ObjectModel;

namespace SmoViewer.Core;

public enum SmoFieldInsertionKind
{
    BeforeTerminal,
    BeforeField,
    AfterField
}

/// <summary>Describes where a newly materialized direct field is inserted.</summary>
public readonly record struct SmoFieldInsertion(
    SmoFieldInsertionKind Kind,
    SmoFieldSelector? Anchor = null)
{
    public static SmoFieldInsertion BeforeTerminal =>
        new(SmoFieldInsertionKind.BeforeTerminal);

    public static SmoFieldInsertion Before(SmoFieldSelector anchor) =>
        new(SmoFieldInsertionKind.BeforeField, anchor);

    public static SmoFieldInsertion After(SmoFieldSelector anchor) =>
        new(SmoFieldInsertionKind.AfterField, anchor);
}

public enum SmoFieldMutationKind
{
    Set,
    Add,
    Remove
}

public sealed record SmoAppliedFieldMutation(
    SmoFieldMutationKind Kind,
    int ObjectIndex,
    int FieldType,
    int Occurrence,
    uint OldPayloadSize,
    uint NewPayloadSize);

public sealed record SmoMutationCommitResult(
    byte[] Data,
    IReadOnlyList<SmoAppliedFieldMutation> Mutations);

/// <summary>
/// The single low-level mutation path for direct SBOO fields. It can set,
/// materialize and remove every field number supported by the serializer,
/// while preserving the object directory, inline object sizes and enclosing
/// data-block sizes. Domain editors should expose schema-backed properties on
/// top of this API instead of exposing arbitrary field numbers to users.
/// </summary>
public sealed class SmoMutationTransaction
{
    private readonly SmoDocument _source;
    private readonly List<PendingMutation> _pending = [];

    public SmoMutationTransaction(SmoDocument source)
    {
        ArgumentNullException.ThrowIfNull(source);
        _source = source;
    }

    public SmoDocument Source => _source;

    public bool WillContainField(int objectIndex, SmoFieldSelector selector)
    {
        SmoObjectEntry entry = GetObject(_source, objectIndex);
        bool present = SmoObjectFieldReader.TryFind(_source, entry, selector, out _);
        foreach (PendingMutation mutation in _pending.Where(item =>
                     item.ObjectIndex == objectIndex &&
                     item.Selector.FieldType == selector.FieldType &&
                     item.Selector.Occurrence == selector.Occurrence))
        {
            switch (mutation.Kind)
            {
                case PendingMutationKind.Remove:
                    present = false;
                    break;
                case PendingMutationKind.Add:
                case PendingMutationKind.Upsert:
                    present = !selector.PayloadSize.HasValue ||
                        selector.PayloadSize.Value == mutation.Payload.Length;
                    break;
            }
        }
        return present;
    }

    public void SetFieldPayload(
        int objectIndex,
        SmoFieldSelector selector,
        ReadOnlyMemory<byte> payload) =>
        _pending.Add(new PendingMutation(
            PendingMutationKind.Set, objectIndex, selector, payload.ToArray(), 0, default));

    public void SetFieldPayloadSlice(
        int objectIndex,
        SmoFieldSelector selector,
        int payloadOffset,
        ReadOnlyMemory<byte> value)
    {
        if (payloadOffset < 0)
            throw new ArgumentOutOfRangeException(nameof(payloadOffset));
        _pending.Add(new PendingMutation(
            PendingMutationKind.SetSlice,
            objectIndex,
            selector,
            value.ToArray(),
            payloadOffset,
            default));
    }

    public void UpsertFieldPayload(
        int objectIndex,
        SmoFieldSelector selector,
        ReadOnlyMemory<byte> payload,
        SmoFieldInsertion insertion) =>
        _pending.Add(new PendingMutation(
            PendingMutationKind.Upsert,
            objectIndex,
            selector,
            payload.ToArray(),
            0,
            insertion));

    public void AddField(
        int objectIndex,
        int fieldType,
        ReadOnlyMemory<byte> payload,
        SmoFieldInsertion insertion)
    {
        ValidateFieldType(fieldType);
        _pending.Add(new PendingMutation(
            PendingMutationKind.Add,
            objectIndex,
            new SmoFieldSelector(fieldType),
            payload.ToArray(),
            0,
            insertion));
    }

    public void RemoveField(int objectIndex, SmoFieldSelector selector) =>
        _pending.Add(new PendingMutation(
            PendingMutationKind.Remove,
            objectIndex,
            selector,
            Array.Empty<byte>(),
            0,
            default));

    public SmoMutationCommitResult Commit()
    {
        if (_pending.Count == 0)
            return new SmoMutationCommitResult(
                _source.Data.ToArray(),
                Array.Empty<SmoAppliedFieldMutation>());

        SmoDocument current = _source;
        var applied = new List<SmoAppliedFieldMutation>(_pending.Count);
        int mutationIndex = 0;
        while (mutationIndex < _pending.Count)
        {
            var batchChanges = new List<ByteChange>();
            var batchResults = new List<SmoAppliedFieldMutation>();
            var occupiedStarts = new HashSet<int>();
            int batchEnd = mutationIndex;
            while (batchEnd < _pending.Count &&
                   TryPrepareSameSize(
                       current,
                       _pending[batchEnd],
                       out ByteChange? change,
                       out SmoAppliedFieldMutation? result) &&
                   change is not null && result is not null &&
                   occupiedStarts.Add(change.Start))
            {
                batchChanges.Add(change);
                batchResults.Add(result);
                batchEnd++;
            }

            byte[] data;
            if (batchChanges.Count > 0)
            {
                data = RewriteSameSizeBatch(current, batchChanges);
                applied.AddRange(batchResults);
                mutationIndex = batchEnd;
            }
            else
            {
                data = ApplyOne(
                    current,
                    _pending[mutationIndex],
                    out SmoAppliedFieldMutation result);
                applied.Add(result);
                mutationIndex++;
            }
            current = SmoDocument.Parse(data, _source.SourcePath);
            VerifyObjectIdentity(_source, current);
        }

        return new SmoMutationCommitResult(
            current.Data.ToArray(),
            new ReadOnlyCollection<SmoAppliedFieldMutation>(applied));
    }

    private static bool TryPrepareSameSize(
        SmoDocument document,
        PendingMutation mutation,
        out ByteChange? change,
        out SmoAppliedFieldMutation? applied)
    {
        change = null;
        applied = null;
        if (mutation.Kind is PendingMutationKind.Add or PendingMutationKind.Remove)
            return false;
        SmoObjectEntry owner = GetObject(document, mutation.ObjectIndex);
        IReadOnlyList<SmoObjectField> fields = SmoObjectFieldReader.Read(document, owner);
        SmoObjectField? existing = fields.FirstOrDefault(mutation.Selector.Matches);
        if (existing is null)
            return false;

        byte[] payload;
        if (mutation.Kind == PendingMutationKind.SetSlice)
        {
            if (mutation.PayloadOffset > existing.Payload.Length - mutation.Payload.Length)
                throw new ArgumentOutOfRangeException(
                    nameof(mutation.PayloadOffset),
                    "The replacement slice is outside the field payload.");
            payload = existing.Payload.ToArray();
            mutation.Payload.CopyTo(payload, mutation.PayloadOffset);
        }
        else
        {
            payload = mutation.Payload;
        }
        byte[] replacement = BuildReplacement(existing, payload);
        if (replacement.Length != existing.EncodedSize)
            return false;

        change = new ByteChange(
            checked((int)owner.LogicalOffset + existing.RelativeHeaderOffset),
            existing.EncodedSize,
            replacement,
            owner.Index);
        applied = new SmoAppliedFieldMutation(
            SmoFieldMutationKind.Set,
            owner.Index,
            existing.FieldType,
            existing.Occurrence,
            existing.PayloadSize,
            checked((uint)payload.Length));
        return true;
    }

    private static byte[] RewriteSameSizeBatch(
        SmoDocument document,
        List<ByteChange> changes)
    {
        changes.Sort((left, right) => left.Start.CompareTo(right.Start));
        ValidateChanges(changes);
        ReadOnlySpan<byte> oldSection = document.Data.Span.Slice(
            checked((int)document.Header.DataStart),
            checked((int)document.Header.DataSize));
        byte[] newSection = ApplyChanges(oldSection, changes);
        DirectoryEntry[] directory = document.Objects.Select(entry =>
            new DirectoryEntry(
                entry.Id,
                entry.RawName.ToArray(),
                entry.TypeHash,
                entry.LogicalOffset,
                entry.SerializedSize)).ToArray();
        return BuildContainer(document, newSection, directory);
    }

    private static byte[] ApplyOne(
        SmoDocument document,
        PendingMutation mutation,
        out SmoAppliedFieldMutation applied)
    {
        SmoObjectEntry owner = GetObject(document, mutation.ObjectIndex);
        IReadOnlyList<SmoObjectField> fields = SmoObjectFieldReader.Read(document, owner);
        SmoObjectField? existing = fields.FirstOrDefault(mutation.Selector.Matches);

        if (mutation.Kind is PendingMutationKind.Set or PendingMutationKind.SetSlice &&
            existing is null)
            throw MissingField(owner, mutation.Selector);
        if (mutation.Kind == PendingMutationKind.Remove && existing is null)
            throw MissingField(owner, mutation.Selector);

        ByteChange target;
        SmoFieldMutationKind publicKind;
        uint oldPayloadSize;
        uint newPayloadSize;
        if (existing is not null && mutation.Kind != PendingMutationKind.Add)
        {
            oldPayloadSize = existing.PayloadSize;
            publicKind = mutation.Kind == PendingMutationKind.Remove
                ? SmoFieldMutationKind.Remove
                : SmoFieldMutationKind.Set;
            byte[] replacement;
            if (mutation.Kind == PendingMutationKind.Remove)
            {
                replacement = Array.Empty<byte>();
            }
            else if (mutation.Kind == PendingMutationKind.SetSlice)
            {
                if (mutation.PayloadOffset > existing.Payload.Length -
                    mutation.Payload.Length)
                {
                    throw new ArgumentOutOfRangeException(
                        nameof(mutation.PayloadOffset),
                        "The replacement slice is outside the field payload.");
                }
                byte[] updatedPayload = existing.Payload.ToArray();
                mutation.Payload.CopyTo(updatedPayload, mutation.PayloadOffset);
                replacement = BuildReplacement(existing, updatedPayload);
            }
            else
            {
                replacement = BuildReplacement(existing, mutation.Payload);
            }
            int logicalHeader = checked((int)owner.LogicalOffset +
                existing.RelativeHeaderOffset);
            RejectUnsafeNestedResize(
                document, owner, existing, replacement.Length - existing.EncodedSize);
            target = new ByteChange(
                logicalHeader,
                existing.EncodedSize,
                replacement,
                owner.Index);
            newPayloadSize = mutation.Kind == PendingMutationKind.Remove
                ? 0
                : mutation.Kind == PendingMutationKind.SetSlice
                    ? existing.PayloadSize
                    : checked((uint)mutation.Payload.Length);
        }
        else
        {
            int insertion = ResolveInsertionOffset(fields, owner, mutation.Insertion);
            byte[] field = SmoDataBlockWriter.BuildField(
                mutation.Selector.FieldType, mutation.Payload);
            target = new ByteChange(
                checked((int)owner.LogicalOffset + insertion),
                0,
                field,
                owner.Index);
            publicKind = SmoFieldMutationKind.Add;
            oldPayloadSize = 0;
            newPayloadSize = checked((uint)mutation.Payload.Length);
        }

        byte[] output = Rewrite(document, target);
        applied = new SmoAppliedFieldMutation(
            publicKind,
            owner.Index,
            mutation.Selector.FieldType,
            existing?.Occurrence ?? CountFields(fields, mutation.Selector.FieldType),
            oldPayloadSize,
            newPayloadSize);
        return output;
    }

    private static byte[] BuildReplacement(
        SmoObjectField existing,
        ReadOnlySpan<byte> payload)
    {
        var preferred = new SmoDataBlockHeader(
            existing.RelativeHeaderOffset,
            existing.RawHeader,
            existing.FieldType,
            (byte)existing.SizeKind,
            existing.HeaderSize,
            existing.PayloadSize);
        byte[] header = SmoDataBlockWriter.BuildHeader(
            existing.FieldType, checked((uint)payload.Length), preferred);
        byte[] result = new byte[checked(header.Length + payload.Length)];
        header.CopyTo(result, 0);
        payload.CopyTo(result.AsSpan(header.Length));
        return result;
    }

    private static int ResolveInsertionOffset(
        IReadOnlyList<SmoObjectField> fields,
        SmoObjectEntry owner,
        SmoFieldInsertion insertion)
    {
        switch (insertion.Kind)
        {
            case SmoFieldInsertionKind.BeforeField:
            case SmoFieldInsertionKind.AfterField:
                if (insertion.Anchor is not SmoFieldSelector anchor)
                    throw new ArgumentException("A field insertion anchor is required.");
                SmoObjectField? anchored = fields.FirstOrDefault(anchor.Matches);
                if (anchored is null)
                    throw MissingField(owner, anchor);
                return insertion.Kind == SmoFieldInsertionKind.BeforeField
                    ? anchored.RelativeHeaderOffset
                    : anchored.RelativeEnd;

            case SmoFieldInsertionKind.BeforeTerminal:
                SmoObjectField? terminal = fields.LastOrDefault(field =>
                    field.PayloadSize == 0 && field.RelativeEnd == owner.SerializedSize);
                return terminal?.RelativeHeaderOffset ?? checked((int)owner.SerializedSize);

            default:
                throw new ArgumentOutOfRangeException(nameof(insertion));
        }
    }

    private static byte[] Rewrite(SmoDocument document, ByteChange target)
    {
        var changes = new List<ByteChange> { target };
        int accumulatedDelta = target.Delta;
        int currentStart = target.Start;
        int currentEnd = checked(target.Start + target.OldLength);
        int? parentIndex = document.Objects[target.OwnerIndex].ParentIndex;
        while (parentIndex is int index)
        {
            SmoObjectEntry parent = document.Objects[index];
            SmoObjectField containing = FindContainingField(
                document, parent, currentStart, currentEnd, target.OldLength == 0);
            uint resizedPayload = checked((uint)(containing.PayloadSize + accumulatedDelta));
            var original = new SmoDataBlockHeader(
                containing.RelativeHeaderOffset,
                containing.RawHeader,
                containing.FieldType,
                (byte)containing.SizeKind,
                containing.HeaderSize,
                containing.PayloadSize);
            byte[] header = SmoDataBlockWriter.BuildHeader(
                containing.FieldType, resizedPayload, original);
            int headerLogical = checked((int)parent.LogicalOffset +
                containing.RelativeHeaderOffset);
            var headerChange = new ByteChange(
                headerLogical,
                containing.HeaderSize,
                header,
                parent.Index);
            changes.Add(headerChange);
            accumulatedDelta = checked(accumulatedDelta + headerChange.Delta);
            currentStart = headerLogical;
            currentEnd = checked(headerLogical + containing.HeaderSize);
            parentIndex = parent.ParentIndex;
        }

        changes.Sort((left, right) => left.Start.CompareTo(right.Start));
        ValidateChanges(changes);
        ReadOnlySpan<byte> oldSection = document.Data.Span.Slice(
            checked((int)document.Header.DataStart),
            checked((int)document.Header.DataSize));
        byte[] newSection = ApplyChanges(oldSection, changes);
        IReadOnlyList<DirectoryEntry> directory = BuildDirectory(document, changes);
        UpdateInlineSizePrefixes(document, newSection, changes, directory);
        return BuildContainer(document, newSection, directory);
    }

    private static SmoObjectField FindContainingField(
        SmoDocument document,
        SmoObjectEntry parent,
        int rangeStart,
        int rangeEnd,
        bool insertion)
    {
        SmoObjectField[] candidates = SmoObjectFieldReader.Read(document, parent)
            .Where(field =>
            {
                long payloadStart = parent.LogicalOffset + field.RelativePayloadOffset;
                long payloadEnd = payloadStart + field.PayloadSize;
                return insertion
                    ? payloadStart <= rangeStart && rangeStart < payloadEnd
                    : payloadStart <= rangeStart && rangeEnd <= payloadEnd;
            })
            .OrderBy(field => field.PayloadSize)
            .ToArray();
        if (candidates.Length == 0)
        {
            throw new InvalidDataException(
                $"Object [{parent.Index}] contains the edited object in its directory " +
                "interval, but no direct field owns that serialized range.");
        }
        return candidates[0];
    }

    private static byte[] ApplyChanges(
        ReadOnlySpan<byte> source,
        IReadOnlyList<ByteChange> changes)
    {
        int length = checked(source.Length + changes.Sum(change => change.Delta));
        byte[] result = new byte[length];
        int sourceCursor = 0;
        int destinationCursor = 0;
        foreach (ByteChange change in changes)
        {
            source[sourceCursor..change.Start].CopyTo(result.AsSpan(destinationCursor));
            destinationCursor += change.Start - sourceCursor;
            change.Replacement.CopyTo(result, destinationCursor);
            destinationCursor += change.Replacement.Length;
            sourceCursor = checked(change.Start + change.OldLength);
        }
        source[sourceCursor..].CopyTo(result.AsSpan(destinationCursor));
        return result;
    }

    private static IReadOnlyList<DirectoryEntry> BuildDirectory(
        SmoDocument document,
        IReadOnlyList<ByteChange> changes)
    {
        var deltasByObject = new int[document.Objects.Count];
        foreach (ByteChange change in changes)
        {
            int? index = change.OwnerIndex;
            while (index is int ownerIndex)
            {
                deltasByObject[ownerIndex] = checked(
                    deltasByObject[ownerIndex] + change.Delta);
                index = document.Objects[ownerIndex].ParentIndex;
            }
        }

        return document.Objects.Select(entry => new DirectoryEntry(
            entry.Id,
            entry.RawName.ToArray(),
            entry.TypeHash,
            checked((uint)MapOffset(entry.LogicalOffset, changes)),
            checked((uint)(entry.SerializedSize + deltasByObject[entry.Index]))))
            .ToArray();
    }

    private static void UpdateInlineSizePrefixes(
        SmoDocument document,
        Span<byte> newSection,
        IReadOnlyList<ByteChange> changes,
        IReadOnlyList<DirectoryEntry> directory)
    {
        for (int index = 0; index < document.Objects.Count; index++)
        {
            SmoObjectEntry oldEntry = document.Objects[index];
            DirectoryEntry newEntry = directory[index];
            if (oldEntry.ParentIndex is null ||
                newEntry.SerializedSize == oldEntry.SerializedSize)
            {
                continue;
            }

            long oldPrefix = (long)oldEntry.LogicalOffset - 8;
            if (oldPrefix < 0 || oldPrefix > document.Header.DataSize - 8)
                throw new InvalidDataException(
                    $"Inline object [{index}] has no readable reference prefix.");
            ReadOnlySpan<byte> oldSection = document.Data.Span.Slice(
                checked((int)document.Header.DataStart),
                checked((int)document.Header.DataSize));
            uint prefixId = BinaryPrimitives.ReadUInt32LittleEndian(
                oldSection.Slice(checked((int)oldPrefix), 4));
            uint prefixSize = BinaryPrimitives.ReadUInt32LittleEndian(
                oldSection.Slice(checked((int)oldPrefix + 4), 4));
            if (prefixId != oldEntry.Id || prefixSize != oldEntry.SerializedSize)
            {
                throw new InvalidDataException(
                    $"Inline object [{index}] does not have a confirmed ID/size prefix.");
            }
            int mappedPrefix = checked((int)MapOffset(oldPrefix, changes));
            BinaryPrimitives.WriteUInt32LittleEndian(
                newSection.Slice(mappedPrefix + 4, 4), newEntry.SerializedSize);
        }
    }

    private static byte[] BuildContainer(
        SmoDocument document,
        ReadOnlySpan<byte> dataSection,
        IReadOnlyList<DirectoryEntry> entries)
    {
        var envelope = new SmoContainerEnvelope(document.Header,
            entries.Select(entry => new SmoContainerEntry(entry.Id, entry.RawName,
                entry.TypeHash, entry.LogicalOffset, entry.SerializedSize)).ToArray(), dataSection.Length);
        byte[] result = envelope.AllocateContainer();
        dataSection.CopyTo(result.AsSpan(envelope.DataStart));
        return result;
    }

    private static long MapOffset(
        long oldOffset,
        IReadOnlyList<ByteChange> changes)
    {
        long mapped = oldOffset;
        foreach (ByteChange change in changes)
        {
            int oldEnd = checked(change.Start + change.OldLength);
            if (change.OldLength == 0 ? oldOffset >= change.Start : oldOffset >= oldEnd)
                mapped += change.Delta;
            else if (oldOffset > change.Start && oldOffset < oldEnd)
                throw new InvalidDataException(
                    "An object directory offset points inside a replaced field header.");
        }
        return mapped;
    }

    private static void RejectUnsafeNestedResize(
        SmoDocument document,
        SmoObjectEntry owner,
        SmoObjectField field,
        int delta)
    {
        if (delta == 0)
            return;
        long start = owner.LogicalOffset + field.RelativePayloadOffset;
        long end = start + field.PayloadSize;
        if (document.Objects.Any(entry =>
                entry.Index != owner.Index &&
                entry.LogicalOffset >= start &&
                entry.LogicalEnd <= (ulong)end))
        {
            throw new NotSupportedException(
                "A size-changing raw replacement cannot rewrite a field that owns " +
                "cataloged inline objects. Use a branch mutation API for that field.");
        }
    }

    private static void ValidateChanges(IReadOnlyList<ByteChange> changes)
    {
        int previousEnd = 0;
        foreach (ByteChange change in changes)
        {
            if (change.Start < previousEnd)
                throw new InvalidDataException("Generated SMO byte changes overlap.");
            previousEnd = checked(change.Start + change.OldLength);
        }
    }

    private static void VerifyObjectIdentity(SmoDocument source, SmoDocument result)
    {
        if (result.HasErrors || result.Objects.Count != source.Objects.Count)
            throw new InvalidDataException(
                "The mutated SMO container failed structural verification.");
        for (int index = 0; index < source.Objects.Count; index++)
        {
            SmoObjectEntry before = source.Objects[index];
            SmoObjectEntry after = result.Objects[index];
            if (before.Id != after.Id || before.TypeHash != after.TypeHash ||
                !before.RawName.Span.SequenceEqual(after.RawName.Span))
            {
                throw new InvalidDataException(
                    $"Object directory identity changed at index {index}.");
            }
        }
    }

    private static SmoObjectEntry GetObject(SmoDocument document, int index)
    {
        if ((uint)index >= (uint)document.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(index));
        return document.Objects[index];
    }

    private static InvalidOperationException MissingField(
        SmoObjectEntry owner,
        SmoFieldSelector selector) => new(
            $"Object [{owner.Index}] \"{owner.Name.TrimEnd('\0')}\" has no direct " +
            $"field {selector.FieldType}, occurrence {selector.Occurrence}.");

    private static int CountFields(
        IEnumerable<SmoObjectField> fields,
        int fieldType) => fields.Count(field => field.FieldType == fieldType);

    private static void ValidateFieldType(int fieldType)
    {
        if (fieldType is < 0 or > byte.MaxValue)
            throw new ArgumentOutOfRangeException(nameof(fieldType));
    }

    private enum PendingMutationKind
    {
        Set,
        SetSlice,
        Upsert,
        Add,
        Remove
    }

    private sealed record PendingMutation(
        PendingMutationKind Kind,
        int ObjectIndex,
        SmoFieldSelector Selector,
        byte[] Payload,
        int PayloadOffset,
        SmoFieldInsertion Insertion);

    private sealed record ByteChange(
        int Start,
        int OldLength,
        byte[] Replacement,
        int OwnerIndex)
    {
        public int Delta => checked(Replacement.Length - OldLength);
    }

    private sealed record DirectoryEntry(
        uint Id,
        byte[] RawName,
        uint TypeHash,
        uint LogicalOffset,
        uint SerializedSize);

}
