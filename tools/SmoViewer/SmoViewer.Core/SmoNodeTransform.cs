using System.Numerics;
using System.Runtime.CompilerServices;

namespace SmoViewer.Core;

/// <summary>Confirmed base-node position, rotation and scale fields.</summary>
public sealed record SmoNodeTransform(
    int ObjectIndex,
    Vector3 Position,
    Quaternion Rotation,
    Vector3 Scale)
{
    public Matrix4x4 LocalMatrix => SmoViewer.Sparkplug.SparkplugNode.LocalMatrix(Position, Rotation, Scale);
}
public static class SmoNodeTransformDecoder
{
    private static readonly ConditionalWeakTable<SmoDocument, TransformContext>
        Contexts = new();

    private sealed record TransformContext(
        IReadOnlyDictionary<int, SmoObjectEntry> Entries,
        Lazy<IReadOnlyDictionary<int, Matrix4x4>> NodeWorlds);

    private static TransformContext CreateContext(SmoDocument document)
    {
        return new TransformContext(document.Objects.ToDictionary(entry => entry.Index),
            new Lazy<IReadOnlyDictionary<int,Matrix4x4>>(() => {
                var loaded = SmoLoadedResources.Get(document);
                if (loaded.LoadIssue is not null || loaded.SceneIssue is not null)
                    throw new InvalidDataException(loaded.LoadIssue ?? loaded.SceneIssue);
                return loaded.NodeWorlds;
            }));
    }

    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        out SmoNodeTransform? transform)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        transform = null;
        if (!SmoNodeDecoder.TryDecodeNodeSection(document, entry, out var node) || node is null)
            return false;
        transform = new SmoNodeTransform(entry.Index, node.Position, node.Rotation, node.Scale);
        return true;
    }

    public static Matrix4x4 ResolveModelWorldMatrix(
        SmoDocument document,
        SmoObjectEntry meshEntry)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(meshEntry);
        var context = Contexts.GetValue(document, CreateContext);
        SmoObjectEntry? model = meshEntry;
        var visited = new HashSet<int>();
        while (model is not null && model.TypeHash != SmoClassIds.Model && visited.Add(model.Index))
            model = model.ParentIndex is int parent && context.Entries.TryGetValue(parent, out var owner) ? owner : null;
        if (model is null) return Matrix4x4.Identity; // existing unplaced/skinned resource view
        if (TryResolvePlacement(document, context, model, out var world)) return world;
        throw new InvalidDataException($"MODEL_PLACEMENT_UNRESOLVED: [{model.Index}] has no unambiguous loaded render support; select an explicit container/member occurrence.");
    }

    /// <summary>Actual Node world or support matrix; physical selection is only a compatibility inspector view.</summary>
    public static bool TryResolveNodeWorldMatrix(SmoDocument document, SmoObjectEntry node, out Matrix4x4 world)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(node);
        return TryResolvePlacement(document, Contexts.GetValue(document, CreateContext), node, out world);
    }

    private static bool TryResolvePlacement(SmoDocument document, TransformContext context,
        SmoObjectEntry entry, out Matrix4x4 world)
    {
        var loaded = SmoLoadedResources.Get(document);
        if (loaded.LoadIssue is not null) throw new InvalidDataException(loaded.LoadIssue);
        var visited = new HashSet<int>();
        SmoObjectEntry? cursor = entry;
        while (cursor is not null && visited.Add(cursor.Index))
        {
            if (cursor.TypeHash == SmoClassIds.Model)
                return TryResolveStoredModelSupport(context, loaded, cursor, out world);
            if (loaded.RenderContainersByObjectIndex.TryGetValue(cursor.Index, out var container))
            {
                world = container.World.GetValueOrDefault();
                return container.World.HasValue;
            }
            if (context.NodeWorlds.Value.TryGetValue(cursor.Index, out world)) return true;
            cursor = cursor.ParentIndex is int parent && context.Entries.TryGetValue(parent, out var owner) ? owner : null;
        }
        world = Matrix4x4.Identity;
        return cursor is null; // unplaced resource preview; a metadata cycle is not a valid placement
    }

    private static bool TryResolveStoredModelSupport(TransformContext context, SmoLoadedResources loaded,
        SmoObjectEntry model, out Matrix4x4 world)
    {
        world = default;
        if (!loaded.RenderContainersByRenderable.TryGetValue(model.Index, out var containers)) return false;
        // This API returns one scalar compatibility view, not a draw list.
        // A storage ancestor is eligible only when its actual support contains
        // this Model. Partition membership therefore reaches the original
        // shared identity matrix, never an unrelated partition/zone center.
        int? index = model.ParentIndex;
        var visited = new HashSet<int>();
        while (index is int parent && visited.Add(parent) && context.Entries.TryGetValue(parent, out var entry))
        {
            var container = containers.FirstOrDefault(value => value.ObjectIndex == parent);
            if (container is not null)
            {
                world = container.World.GetValueOrDefault();
                return container.World.HasValue;
            }
            index = entry.ParentIndex;
        }
        if (containers.Count != 1 || containers[0].World is not Matrix4x4 single) return false;
        world = single;
        return true;
    }

    private static bool TryFindOwnTransformFields(
        SmoDocument document,
        SmoObjectEntry entry,
        out SmoObjectField? position,
        out SmoObjectField? rotation,
        out SmoObjectField? scale)
    {
        position = null;
        rotation = null;
        scale = null;
        if (!SmoObjectFieldReader.TryRead(document, entry, out var fields, out _) ||
            !SmoSerializedFieldRegistry.TryGetNodeSectionRange(entry.TypeHash, fields, out int first, out int terminal))
            return false;
        for (int index = first; index < terminal; index++)
        {
            var field = fields[index];
            switch (field.FieldType)
            {
                case 0 when field.PayloadSize == SmoNodeDecoder.VectorPayloadSize: position = field; break;
                case 1 when field.PayloadSize == SmoNodeDecoder.QuaternionPayloadSize: rotation = field; break;
                case 2 when field.PayloadSize == SmoNodeDecoder.VectorPayloadSize: scale = field; break;
            }
        }
        return true;
    }

    internal static bool TryFindOwnPositionPayloadOffset(
        SmoDocument document,
        SmoObjectEntry entry,
        out int absolutePayloadOffset)
    {
        bool found = TryFindOwnTransformPayloadOffsets(
            document,
            entry,
            out absolutePayloadOffset,
            out _,
            out _);
        return found;
    }

    public static bool TryFindOwnTransformPayloadOffsets(
        SmoDocument document,
        SmoObjectEntry entry,
        out int positionPayloadOffset,
        out int? rotationPayloadOffset,
        out int? scalePayloadOffset)
    {
        positionPayloadOffset = 0;
        rotationPayloadOffset = null;
        scalePayloadOffset = null;
        if (!entry.IsWithinDataSection || !entry.SignatureMatches ||
            entry.PhysicalOffset < 0 || entry.PhysicalOffset > int.MaxValue ||
            entry.SerializedSize > int.MaxValue ||
            entry.PhysicalEnd > document.Data.Length)
        {
            return false;
        }

        if (!TryFindOwnTransformFields(
                document, entry, out SmoObjectField? position,
                out SmoObjectField? rotation, out SmoObjectField? scale) ||
            position is null)
            return false;
        positionPayloadOffset = position.AbsolutePayloadOffset;
        rotationPayloadOffset = rotation?.AbsolutePayloadOffset;
        scalePayloadOffset = scale?.AbsolutePayloadOffset;
        return true;
    }

}
