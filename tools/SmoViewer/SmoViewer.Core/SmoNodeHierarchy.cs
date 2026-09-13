using System.Buffers.Binary;
using System.Collections.ObjectModel;

namespace SmoViewer.Core;

/// <summary>A confirmed <c>esfNodeChild</c> relation from an <c>spNode</c>.</summary>
public sealed record SmoNodeChildLink(
    int ParentObjectIndex,
    int ChildObjectIndex,
    uint ChildObjectId,
    uint InlineSerializedSize);

/// <summary>
/// Decodes the logical node graph. Directory containment describes serializer
/// ownership and must not be used as a skeleton hierarchy: a child can be
/// serialized inline or referenced by ID with a zero inline size.
/// </summary>
public sealed class SmoNodeHierarchy
{
    private const int ObjectSignatureSize = 8;
    private const int NodeChildFieldType = 5;
    private const int NodeChildPrefixSize = 2 * sizeof(uint);

    private SmoNodeHierarchy(
        IReadOnlyList<SmoNodeChildLink> links,
        IReadOnlyDictionary<int, IReadOnlyList<int>> childrenByParent,
        IReadOnlyDictionary<int, IReadOnlyList<int>> parentsByChild)
    {
        Links = links;
        ChildrenByParent = childrenByParent;
        ParentsByChild = parentsByChild;
    }

    public IReadOnlyList<SmoNodeChildLink> Links { get; }
    public IReadOnlyDictionary<int, IReadOnlyList<int>> ChildrenByParent { get; }
    public IReadOnlyDictionary<int, IReadOnlyList<int>> ParentsByChild { get; }

    public static SmoNodeHierarchy Decode(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        Dictionary<uint, SmoObjectEntry> entriesById = document.Objects
            .GroupBy(entry => entry.Id)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single());
        var links = new List<SmoNodeChildLink>();

        foreach (SmoObjectEntry parent in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Node &&
                     entry.IsWithinDataSection && entry.SignatureMatches &&
                     entry.PhysicalOffset >= 0 && entry.PhysicalOffset <= int.MaxValue &&
                     entry.SerializedSize <= int.MaxValue &&
                     entry.PhysicalEnd <= document.Data.Length))
        {
            ReadOnlySpan<byte> serialized = document.Data.Span.Slice(
                (int)parent.PhysicalOffset, (int)parent.SerializedSize);
            int offset = ObjectSignatureSize;
            while (offset < serialized.Length &&
                   SmoDataBlockReader.TryReadHeader(
                       serialized, offset, out SmoDataBlockHeader header))
            {
                if (header.FieldType == NodeChildFieldType &&
                    header.PayloadSize >= NodeChildPrefixSize)
                {
                    ReadOnlySpan<byte> payload = serialized.Slice(
                        header.PayloadOffset, checked((int)header.PayloadSize));
                    uint childId = BinaryPrimitives.ReadUInt32LittleEndian(payload);
                    uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(
                        payload.Slice(sizeof(uint)));
                    if (inlineSize == header.PayloadSize - NodeChildPrefixSize &&
                        entriesById.TryGetValue(childId, out SmoObjectEntry? child) &&
                        (inlineSize == 0 || inlineSize == child.SerializedSize))
                    {
                        links.Add(new SmoNodeChildLink(
                            parent.Index, child.Index, childId, inlineSize));
                    }
                }

                int next = checked((int)header.PayloadEnd);
                if (next <= offset)
                    break;
                offset = next;
            }
        }

        Dictionary<int, IReadOnlyList<int>> childrenByParent = links
            .GroupBy(link => link.ParentObjectIndex)
            .ToDictionary(
                group => group.Key,
                group => (IReadOnlyList<int>)group
                    .Select(link => link.ChildObjectIndex).Distinct().ToArray());
        Dictionary<int, IReadOnlyList<int>> parentsByChild = links
            .GroupBy(link => link.ChildObjectIndex)
            .ToDictionary(
                group => group.Key,
                group => (IReadOnlyList<int>)group
                    .Select(link => link.ParentObjectIndex).Distinct().ToArray());

        return new SmoNodeHierarchy(
            links.AsReadOnly(),
            new ReadOnlyDictionary<int, IReadOnlyList<int>>(childrenByParent),
            new ReadOnlyDictionary<int, IReadOnlyList<int>>(parentsByChild));
    }
}
