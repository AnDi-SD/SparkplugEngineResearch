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
                     (entry.TypeHash is SmoClassIds.Node or SmoClassIds.RenderNode) &&
                     entry.IsWithinDataSection && entry.SignatureMatches))
        {
            if (!SmoObjectFieldReader.TryRead(
                    document,parent,out IReadOnlyList<SmoObjectField>? fields,out _))
            {
                continue;
            }
            for (int index = 0; index < fields.Count; index++)
            {
                SmoObjectField field = fields[index];
                if (!SmoSerializedFieldRegistry.TryDescribeField(
                        parent.TypeHash,fields,index,
                        out SmoSerializedFieldDescriptor? descriptor) ||
                    descriptor?.Key != "node.child" ||
                    !SmoNodeDecoder.TryDecodeRelationship(
                        field.Payload.Span,out uint childId,out uint inlineSize,out _) ||
                    !entriesById.TryGetValue(childId,out SmoObjectEntry? child) ||
                    (inlineSize != 0 && inlineSize != child.SerializedSize))
                {
                    continue;
                }
                links.Add(new SmoNodeChildLink(
                    parent.Index,child.Index,childId,inlineSize));
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
