using System.Collections.ObjectModel;
using System.Numerics;
using SmoViewer.Core;
using SmoViewer.Scene;

namespace SmoLVLcreator.Core;

/// <summary>
/// A non-destructive editor association between independently serialized visual
/// and collision entities. The source SMO hierarchy is not changed by a link.
/// </summary>
public sealed record SmoLevelCollisionLink(
    SmoLevelEntityId VisualEntityId,
    SmoLevelEntityId CollisionEntityId,
    float Confidence,
    SmoLevelBounds VisualBounds,
    SmoLevelBounds CollisionBounds);

public readonly record struct SmoLevelBounds(Vector3 Minimum, Vector3 Maximum)
{
    public Vector3 Size => Maximum - Minimum;
    public Vector3 Center => (Minimum + Maximum) * 0.5f;
    public float Volume
    {
        get
        {
            Vector3 size = Size;
            return MathF.Max(size.X, 0) *
                   MathF.Max(size.Y, 0) *
                   MathF.Max(size.Z, 0);
        }
    }
}

internal static class SmoLevelCollisionLinker
{
    private const float MinimumAxisRatio = 0.45f;
    private const float MinimumIntersectionOverUnion = 0.30f;
    private const float MinimumConfidence = 0.62f;

    public static IReadOnlyList<SmoLevelCollisionLink> Build(
        SmoLevelWorkspace workspace,
        IReadOnlyDictionary<SmoPlacementId, SmoEditablePlacement> placements,
        IReadOnlyList<SmoEditableCollision> collisions)
    {
        ArgumentNullException.ThrowIfNull(workspace);
        ArgumentNullException.ThrowIfNull(placements);
        ArgumentNullException.ThrowIfNull(collisions);

        Dictionary<SmoLevelEntityId, BoundsAccumulator> visualBounds = [];
        foreach (SmoSceneMesh sceneMesh in workspace.PreparedScene.Meshes)
        {
            var placementId = new SmoPlacementId(
                sceneMesh.Mesh.ObjectIndex,
                sceneMesh.SceneObjectIndex);
            if (!placements.TryGetValue(placementId, out SmoEditablePlacement? placement))
                continue;

            ref BoundsAccumulator bounds = ref CollectionsMarshalHelper.GetOrAdd(
                visualBounds, placement.Entity.Id);
            foreach (Vector3 position in sceneMesh.Mesh.Positions)
                bounds.Include(Vector3.Transform(position, sceneMesh.WorldTransform));
        }

        var visualCandidates = visualBounds
            .Where(pair => pair.Value.HasValue)
            .Select(pair => new Candidate(
                pair.Key,
                pair.Value.ToBounds(),
                FindPartitionAncestor(workspace.Document, pair.Key.SceneObjectIndex)))
            .Where(candidate => candidate.PartitionObjectIndex is not null)
            .ToArray();

        var links = new List<SmoLevelCollisionLink>();
        foreach (SmoEditableCollision collision in collisions)
        {
            if (!TryCalculateCollisionBounds(collision, out SmoLevelBounds collisionBounds))
                continue;
            int? partition = FindPartitionAncestor(
                workspace.Document,
                collision.Source.CollisionInfoObjectIndex);
            if (partition is null)
                continue;

            ScoredCandidate? best = null;
            foreach (Candidate visual in visualCandidates.Where(candidate =>
                         candidate.PartitionObjectIndex == partition))
            {
                float confidence = Score(visual.Bounds, collisionBounds);
                if (confidence < MinimumConfidence ||
                    (best is not null && confidence <= best.Confidence))
                {
                    continue;
                }
                best = new ScoredCandidate(visual, confidence);
            }

            if (best is not null)
            {
                links.Add(new SmoLevelCollisionLink(
                    best.Candidate.EntityId,
                    collision.Entity.Id,
                    best.Confidence,
                    best.Candidate.Bounds,
                    collisionBounds));
            }
        }

        return new ReadOnlyCollection<SmoLevelCollisionLink>(links);
    }

    private static float Score(SmoLevelBounds visual, SmoLevelBounds collision)
    {
        Vector3 visualSize = visual.Size;
        Vector3 collisionSize = collision.Size;
        if (!HasVolume(visualSize) || !HasVolume(collisionSize))
            return 0;

        Vector3 axisRatios = new(
            Ratio(visualSize.X, collisionSize.X),
            Ratio(visualSize.Y, collisionSize.Y),
            Ratio(visualSize.Z, collisionSize.Z));
        float minimumAxisRatio = MathF.Min(
            axisRatios.X,
            MathF.Min(axisRatios.Y, axisRatios.Z));
        if (minimumAxisRatio < MinimumAxisRatio)
            return 0;

        Vector3 intersectionSize = Vector3.Max(
            Vector3.Zero,
            Vector3.Min(visual.Maximum, collision.Maximum) -
            Vector3.Max(visual.Minimum, collision.Minimum));
        float intersectionVolume =
            intersectionSize.X * intersectionSize.Y * intersectionSize.Z;
        float unionVolume = visual.Volume + collision.Volume - intersectionVolume;
        float intersectionOverUnion = unionVolume > 0
            ? intersectionVolume / unionVolume
            : 0;
        if (intersectionOverUnion < MinimumIntersectionOverUnion)
            return 0;

        Vector3 comparisonSize = Vector3.Max(visualSize, collisionSize);
        Vector3 centerDelta = Vector3.Abs(visual.Center - collision.Center);
        float centerDistance = new Vector3(
            centerDelta.X / comparisonSize.X,
            centerDelta.Y / comparisonSize.Y,
            centerDelta.Z / comparisonSize.Z).Length();
        if (centerDistance > 0.30f)
            return 0;

        float meanAxisRatio =
            (axisRatios.X + axisRatios.Y + axisRatios.Z) / 3f;
        float centerScore = 1f - Math.Clamp(centerDistance / 0.30f, 0, 1);
        return 0.55f * intersectionOverUnion +
               0.30f * meanAxisRatio +
               0.15f * centerScore;
    }

    private static bool TryCalculateCollisionBounds(
        SmoEditableCollision collision,
        out SmoLevelBounds bounds)
    {
        var accumulator = new BoundsAccumulator();
        foreach (Vector3 position in collision.Source.Positions)
        {
            accumulator.Include(Vector3.Transform(
                position,
                collision.OriginalWorldTransform));
        }
        bounds = accumulator.HasValue ? accumulator.ToBounds() : default;
        return accumulator.HasValue;
    }

    private static int? FindPartitionAncestor(SmoDocument document, int objectIndex)
    {
        int? cursor = objectIndex;
        var visited = new HashSet<int>();
        while (cursor is int index &&
               (uint)index < (uint)document.Objects.Count &&
               visited.Add(index))
        {
            SmoObjectEntry entry = document.Objects[index];
            if (entry.TypeHash == SmoClassIds.PartitionNode)
                return index;
            cursor = entry.ParentIndex;
        }
        return null;
    }

    private static bool HasVolume(Vector3 size) =>
        size.X > 1e-4f && size.Y > 1e-4f && size.Z > 1e-4f;

    private static float Ratio(float left, float right) =>
        MathF.Min(left, right) / MathF.Max(left, right);

    private sealed record Candidate(
        SmoLevelEntityId EntityId,
        SmoLevelBounds Bounds,
        int? PartitionObjectIndex);

    private sealed record ScoredCandidate(Candidate Candidate, float Confidence);

    private struct BoundsAccumulator
    {
        public Vector3 Minimum;
        public Vector3 Maximum;
        public bool HasValue;

        public void Include(Vector3 point)
        {
            if (!HasValue)
            {
                Minimum = point;
                Maximum = point;
                HasValue = true;
                return;
            }
            Minimum = Vector3.Min(Minimum, point);
            Maximum = Vector3.Max(Maximum, point);
        }

        public readonly SmoLevelBounds ToBounds() => new(Minimum, Maximum);
    }

    private static class CollectionsMarshalHelper
    {
        public static ref BoundsAccumulator GetOrAdd(
            Dictionary<SmoLevelEntityId, BoundsAccumulator> dictionary,
            SmoLevelEntityId key)
        {
            return ref System.Runtime.InteropServices.CollectionsMarshal
                .GetValueRefOrAddDefault(dictionary, key, out _);
        }
    }
}
