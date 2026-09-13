using System.Collections.ObjectModel;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>A requested world-transform change for one rendered scene object.</summary>
public sealed record SmoPlacementTransformEdit(
    int SceneObjectIndex,
    Matrix4x4 OriginalWorldTransform,
    Matrix4x4 WorldTransform);

/// <summary>The source container with only confirmed transform payloads patched.</summary>
public sealed record SmoPlacementTransformPatchResult(
    byte[] Data,
    int EditedSceneObjectCount,
    IReadOnlyList<int> StaticObjectIndices,
    IReadOnlyList<int> NodeObjectIndices,
    IReadOnlyList<int> CollisionMeshObjectIndices);

/// <summary>
/// Writes confirmed spStaticRenderObject matrices, schema-backed node
/// transforms, and collision vertex positions through <see
/// cref="SmoMutationTransaction"/>. Missing optional node rotation and scale
/// fields are materialized by the schema. Collision affine transforms are
/// baked into spMeshBV vertices while its independently serialized transform
/// remains unchanged.
/// </summary>
public static class SmoPlacementTransformWriter
{
    /// <summary>
    /// Existing editor authoring policy retained pending the LVLcreator review.
    /// This reproduces an observed file convention, not a recovered static-object
    /// inverse operation. The original reader stores both matrices independently.
    /// </summary>
    public static Matrix4x4 CreateLegacyStaticInverseTransform(Matrix4x4 transform) =>
        new(
            transform.M11,transform.M21,transform.M31,0,
            transform.M12,transform.M22,transform.M32,0,
            transform.M13,transform.M23,transform.M33,0,
            -(transform.M41 * transform.M11 +
              transform.M42 * transform.M12 +
              transform.M43 * transform.M13),
            -(transform.M41 * transform.M21 +
              transform.M42 * transform.M22 +
              transform.M43 * transform.M23),
            -(transform.M41 * transform.M31 +
              transform.M42 * transform.M32 +
              transform.M43 * transform.M33),
            1);

    private sealed record NodeTransformWrite(
        Vector3 Position,
        Quaternion Rotation,
        Vector3 Scale,
        Matrix4x4 LocalMatrix,
        Matrix4x4 WorldMatrix);

    /// <summary>
    /// Preflights one authored edit without rewriting the SMO container.
    /// Patch additionally verifies the actual Sparkplug world result; unsupported
    /// inverse-PRS cases may still fail that final check. Shear and singular
    /// matrices are rejected here before a gizmo edit is committed.
    /// </summary>
    public static void ValidateEdit(
        SmoDocument document,
        SmoPlacementTransformEdit edit)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(edit);
        ValidateMatrix(edit.OriginalWorldTransform, nameof(edit.OriginalWorldTransform));
        ValidateMatrix(edit.WorldTransform, nameof(edit.WorldTransform));
        SmoObjectEntry sceneObject = GetObject(document, edit.SceneObjectIndex);
        if (sceneObject.TypeHash == SmoClassIds.CollisionInfo)
        {
            ResolveCollisionVertices(
                document,
                sceneObject,
                edit,
                new Dictionary<int, Vector3[]>());
            return;
        }

        SmoObjectEntry? owner = FindStaticPlacementOwner(document.Objects, sceneObject);
        if (owner is null)
        {
            if (!TryResolveNodeTransform(
                    document,
                    sceneObject,
                    edit,
                    out _,
                    out NodeTransformWrite? desiredTransform) ||
                desiredTransform is null)
            {
                throw new NotSupportedException(
                    $"Scene object [{sceneObject.Index}] " +
                    $"\"{CleanName(sceneObject.Name)}\" has neither a confirmed " +
                    "spStaticRenderObject placement nor a writable node transform.");
            }
            return;
        }

        if (!SmoStaticRenderObjectTransformDecoder.TryDecode(
                document, owner, out Matrix4x4 originalAuthored) ||
            !Matrix4x4.Invert(originalAuthored, out Matrix4x4 inverseAuthored))
        {
            throw new InvalidDataException(
                $"Placement [{owner.Index}] has an invalid or singular authored transform.");
        }
        Matrix4x4 prefix = edit.OriginalWorldTransform * inverseAuthored;
        if (!Matrix4x4.Invert(prefix, out Matrix4x4 inversePrefix))
            throw new InvalidDataException("The scene transform prefix is singular.");
        Matrix4x4 desiredAuthored = inversePrefix * edit.WorldTransform;
        ValidateMatrix(desiredAuthored, nameof(edit.WorldTransform));
        if (!Matrix4x4.Invert(desiredAuthored, out _))
            throw new InvalidDataException("The authored placement transform is singular.");
    }

    public static SmoPlacementTransformPatchResult Patch(
        SmoDocument document,
        IEnumerable<SmoPlacementTransformEdit> edits)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(edits);

        SmoPlacementTransformEdit[] requested = edits.ToArray();
        var matricesByOwner = new Dictionary<int, Matrix4x4>();
        var transformsByNode = new Dictionary<int, NodeTransformWrite>();
        var verticesByCollisionMesh = new Dictionary<int, Vector3[]>();
        foreach (SmoPlacementTransformEdit edit in requested)
        {
            ValidateMatrix(edit.OriginalWorldTransform, nameof(edit.OriginalWorldTransform));
            ValidateMatrix(edit.WorldTransform, nameof(edit.WorldTransform));
            SmoObjectEntry sceneObject = GetObject(document, edit.SceneObjectIndex);
            if (sceneObject.TypeHash == SmoClassIds.CollisionInfo)
            {
                ResolveCollisionVertices(
                    document,
                    sceneObject,
                    edit,
                    verticesByCollisionMesh);
                continue;
            }
            SmoObjectEntry? owner = FindStaticPlacementOwner(
                document.Objects, sceneObject);
            if (owner is null)
            {
                if (!TryResolveNodeTransform(
                        document,
                        sceneObject,
                        edit,
                        out int nodeIndex,
                        out NodeTransformWrite? desiredTransform) ||
                    desiredTransform is null)
                {
                    throw new NotSupportedException(
                        $"Scene object [{sceneObject.Index}] " +
                        $"\"{CleanName(sceneObject.Name)}\" has neither a confirmed " +
                        "spStaticRenderObject placement nor a writable node transform. " +
                        "Its transform was not written.");
                }
                if (transformsByNode.TryGetValue(
                        nodeIndex, out NodeTransformWrite? existingTransform) &&
                    MatrixDifference(
                        existingTransform.LocalMatrix,
                        desiredTransform.LocalMatrix) > 0.0001f)
                {
                    throw new InvalidOperationException(
                        $"Node [{nodeIndex}] received conflicting transforms.");
                }
                transformsByNode[nodeIndex] = desiredTransform;
                continue;
            }

            if (!SmoStaticRenderObjectTransformDecoder.TryDecode(
                    document, owner, out Matrix4x4 originalAuthored) ||
                !Matrix4x4.Invert(originalAuthored, out Matrix4x4 inverseAuthored))
            {
                throw new InvalidDataException(
                    $"Placement [{owner.Index}] \"{CleanName(owner.Name)}\" has an " +
                    "invalid or singular authored transform.");
            }

            // ResolveModelWorldMatrix uses row vectors and composes the child/model
            // prefix before the static object's authored world matrix.
            Matrix4x4 prefix = edit.OriginalWorldTransform * inverseAuthored;
            if (!Matrix4x4.Invert(prefix, out Matrix4x4 inversePrefix))
            {
                throw new InvalidDataException(
                    $"Scene object [{sceneObject.Index}] has a singular transform prefix.");
            }

            Matrix4x4 desiredAuthored = inversePrefix * edit.WorldTransform;
            ValidateMatrix(desiredAuthored, nameof(edit.WorldTransform));
            if (!Matrix4x4.Invert(desiredAuthored, out _))
            {
                throw new InvalidDataException(
                    $"The new transform for placement [{owner.Index}] is singular and " +
                    "cannot produce the required inverse matrix.");
            }

            if (matricesByOwner.TryGetValue(owner.Index, out Matrix4x4 existing) &&
                MatrixDifference(existing, desiredAuthored) > 0.00001f)
            {
                throw new InvalidOperationException(
                    $"Several edited scene objects resolve to placement [{owner.Index}] " +
                    "but request different transforms. The file was not changed.");
            }
            matricesByOwner[owner.Index] = desiredAuthored;
        }

        var transaction = new SmoMutationTransaction(document);
        foreach ((int ownerIndex, Matrix4x4 authored) in matricesByOwner)
        {
            Matrix4x4 inverse =
                SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(authored);
            SmoStaticMatrixPayloads encoded = SmoStaticMatrixWriter.Encode(authored, inverse);
            SmoPropertyMutation.Set(
                transaction,
                ownerIndex,
                SmoPropertyKeys.WorldMatrix,
                encoded.World);
            SmoPropertyMutation.Set(
                transaction,
                ownerIndex,
                SmoPropertyKeys.InverseWorldMatrix,
                encoded.Inverse);
        }
        foreach ((int nodeIndex, NodeTransformWrite transform) in transformsByNode)
        {
            SmoPropertyMutation.Set(
                transaction,
                nodeIndex,
                SmoPropertyKeys.Position,
                transform.Position);
            SmoPropertyMutation.Set(
                transaction,
                nodeIndex,
                SmoPropertyKeys.Rotation,
                transform.Rotation);
            SmoPropertyMutation.Set(
                transaction,
                nodeIndex,
                SmoPropertyKeys.Scale,
                transform.Scale);
        }
        foreach ((int meshIndex, Vector3[] positions) in verticesByCollisionMesh)
        {
            SmoObjectEntry mesh = document.Objects[meshIndex];
            if (!SmoCollisionMeshDecoder.TryFindVertexPayloadOffset(
                    document, mesh, out int vertexOffset, out int vertexCount) ||
                vertexCount != positions.Length)
            {
                throw new InvalidDataException(
                    $"Collision mesh [{meshIndex}] lost its confirmed vertex payload.");
            }
            SmoObjectField geometry = SmoObjectFieldReader.Read(document, mesh)
                .FirstOrDefault(field =>
                    field.FieldType == 0 &&
                    vertexOffset >= field.AbsolutePayloadOffset &&
                    vertexOffset + positions.Length * 3 * sizeof(float) <= field.AbsoluteEnd)
                ?? throw new InvalidDataException(
                    $"Collision mesh [{meshIndex}] has no owning geometry field.");
            byte[] payload = geometry.Payload.ToArray();
            int relativeVertexOffset = vertexOffset - geometry.AbsolutePayloadOffset;
            for (int index = 0; index < positions.Length; index++)
            {
                SmoPropertyValueCodec.Write(
                    payload.AsSpan(relativeVertexOffset + index * 3 * sizeof(float),
                        3 * sizeof(float)),
                    positions[index]);
            }
            transaction.SetFieldPayload(meshIndex, geometry.Selector, payload);
        }

        byte[] output = transaction.Commit().Data;

        VerifyPatchedContainer(
            document,
            output,
            matricesByOwner,
            transformsByNode,
            verticesByCollisionMesh);
        return new SmoPlacementTransformPatchResult(
            output,
            requested.Length,
            new ReadOnlyCollection<int>(matricesByOwner.Keys.Order().ToArray()),
            new ReadOnlyCollection<int>(transformsByNode.Keys.Order().ToArray()),
            new ReadOnlyCollection<int>(verticesByCollisionMesh.Keys.Order().ToArray()));
    }

    public static int? FindStaticPlacementOwnerIndex(
        SmoDocument document,
        int sceneObjectIndex)
    {
        ArgumentNullException.ThrowIfNull(document);
        SmoObjectEntry sceneObject = GetObject(document, sceneObjectIndex);
        return FindStaticPlacementOwner(document.Objects, sceneObject)?.Index;
    }

    public static bool CanWriteNodeTransform(
        SmoDocument document,
        int sceneObjectIndex)
    {
        ArgumentNullException.ThrowIfNull(document);
        SmoObjectEntry sceneObject = GetObject(document, sceneObjectIndex);
        return FindNodeTransformOwner(document, sceneObject) is not null;
    }

    public static int? FindNodeTransformOwnerIndex(
        SmoDocument document,
        int sceneObjectIndex)
    {
        ArgumentNullException.ThrowIfNull(document);
        SmoObjectEntry sceneObject = GetObject(document, sceneObjectIndex);
        return FindNodeTransformOwner(document, sceneObject)?.Index;
    }

    public static bool CanWriteCollisionTransform(
        SmoDocument document,
        int collisionInfoObjectIndex)
    {
        ArgumentNullException.ThrowIfNull(document);
        SmoObjectEntry collisionInfo = GetObject(document, collisionInfoObjectIndex);
        SmoObjectEntry? mesh = FindCollisionMesh(document, collisionInfo);
        return collisionInfo.TypeHash == SmoClassIds.CollisionInfo &&
            mesh is not null &&
            SmoCollisionMeshDecoder.TryFindVertexPayloadOffset(
                document, mesh, out _, out _);
    }

    private static void ResolveCollisionVertices(
        SmoDocument document,
        SmoObjectEntry collisionInfo,
        SmoPlacementTransformEdit edit,
        Dictionary<int, Vector3[]> verticesByCollisionMesh)
    {
        SmoObjectEntry? mesh = FindCollisionMesh(document, collisionInfo);
        if (mesh is null ||
            !SmoCollisionMeshDecoder.TryDecodeShape(
                document, mesh, out Vector3[] positions, out _))
        {
            throw new InvalidDataException(
                $"Collision [{collisionInfo.Index}] has no confirmed spMeshBV geometry.");
        }
        if (!Matrix4x4.Invert(edit.OriginalWorldTransform, out Matrix4x4 inverseOriginal))
        {
            throw new InvalidDataException(
                $"Collision [{collisionInfo.Index}] has a singular original transform.");
        }
        Matrix4x4 bakeTransform = edit.WorldTransform * inverseOriginal;
        Vector3[] desired = positions
            .Select(position => Vector3.Transform(position, bakeTransform))
            .ToArray();
        if (desired.Any(position =>
                !float.IsFinite(position.X) ||
                !float.IsFinite(position.Y) ||
                !float.IsFinite(position.Z)))
        {
            throw new InvalidDataException(
                $"Collision [{collisionInfo.Index}] produced non-finite vertices.");
        }
        if (verticesByCollisionMesh.TryGetValue(mesh.Index, out Vector3[]? existing) &&
            (existing.Length != desired.Length || existing.Where((position, index) =>
                Vector3.Distance(position, desired[index]) > 0.00001f).Any()))
        {
            throw new InvalidOperationException(
                $"Collision mesh [{mesh.Index}] received conflicting transforms.");
        }
        verticesByCollisionMesh[mesh.Index] = desired;
    }

    private static SmoObjectEntry? FindCollisionMesh(
        SmoDocument document,
        SmoObjectEntry collisionInfo) =>
        collisionInfo.TypeHash == SmoClassIds.CollisionInfo
            ? document.Objects.FirstOrDefault(entry =>
                entry.ParentIndex == collisionInfo.Index &&
                entry.TypeHash == SmoClassIds.MeshBoundingVolume)
            : null;

    private static bool TryResolveNodeTransform(
        SmoDocument document,
        SmoObjectEntry sceneObject,
        SmoPlacementTransformEdit edit,
        out int nodeIndex,
        out NodeTransformWrite? desiredTransform)
    {
        nodeIndex = -1;
        desiredTransform = null;
        SmoObjectEntry? owner = FindNodeTransformOwner(document, sceneObject);
        if (owner is null ||
            !SmoNodeTransformDecoder.TryDecode(
                document, owner, out SmoNodeTransform? transform) ||
            transform is null)
        {
            return false;
        }

        Matrix4x4 originalLocal = transform.LocalMatrix;
        if (!Matrix4x4.Invert(originalLocal, out Matrix4x4 inverseLocal) ||
            !SmoNodeTransformDecoder.TryResolveNodeWorldMatrix(
                document, owner, out Matrix4x4 originalOwnerWorld) ||
            !Matrix4x4.Invert(originalOwnerWorld, out Matrix4x4 inverseOwnerWorld))
            return false;
        Matrix4x4 childPrefix = edit.OriginalWorldTransform * inverseOwnerWorld;
        if (!Matrix4x4.Invert(childPrefix, out Matrix4x4 inverseChildPrefix))
            return false;
        Matrix4x4 parentWorld = inverseLocal * originalOwnerWorld;
        if (!Matrix4x4.Invert(parentWorld, out Matrix4x4 inverseParentWorld))
            return false;
        Matrix4x4 desiredOwnerWorld = inverseChildPrefix * edit.WorldTransform;
        Matrix4x4 desiredLocal = desiredOwnerWorld * inverseParentWorld;
        if (!Matrix4x4.Decompose(
                desiredLocal,
                out Vector3 desiredScale,
                out Quaternion desiredRotation,
                out Vector3 desiredPosition) ||
            desiredRotation.LengthSquared() < 0.000001f)
        {
            throw new NotSupportedException(
                $"Node [{owner.Index}] produced a non-decomposable local transform.");
        }
        desiredRotation = Quaternion.Normalize(desiredRotation);
        Matrix4x4 reconstructed =
            Matrix4x4.CreateScale(desiredScale) *
            Matrix4x4.CreateFromQuaternion(desiredRotation) *
            Matrix4x4.CreateTranslation(desiredPosition);
        if (MatrixDifference(reconstructed, desiredLocal) > 0.001f)
        {
            throw new NotSupportedException(
                $"Node [{owner.Index}] produced a sheared local transform.");
        }

        nodeIndex = owner.Index;
        desiredTransform = new NodeTransformWrite(
            desiredPosition,
            desiredRotation,
            desiredScale,
            reconstructed,
            desiredOwnerWorld);
        return true;
    }

    private static SmoObjectEntry GetObject(SmoDocument document, int index) =>
        (uint)index < (uint)document.Objects.Count
            ? document.Objects[index]
            : throw new ArgumentOutOfRangeException(
                nameof(index), index, "Scene object index is outside the SMO table.");

    private static SmoObjectEntry? FindStaticPlacementOwner(
        IReadOnlyList<SmoObjectEntry> objects,
        SmoObjectEntry sceneObject)
    {
        SmoObjectEntry? cursor = sceneObject;
        var visited = new HashSet<int>();
        while (cursor is not null && visited.Add(cursor.Index))
        {
            if (cursor.TypeHash == SmoClassIds.StaticRenderObject)
                return cursor;
            cursor = cursor.ParentIndex is int parentIndex &&
                     (uint)parentIndex < (uint)objects.Count
                ? objects[parentIndex]
                : null;
        }
        return null;
    }

    private static SmoObjectEntry? FindNodeTransformOwner(
        SmoDocument document,
        SmoObjectEntry sceneObject)
    {
        SmoObjectEntry? cursor = sceneObject;
        var visited = new HashSet<int>();
        while (cursor is not null && visited.Add(cursor.Index))
        {
            if (cursor.TypeHash is SmoClassIds.Node or
                    SmoClassIds.RenderNode or SmoClassIds.Model &&
                SmoNodeTransformDecoder.TryDecode(
                    document, cursor, out SmoNodeTransform? transform) &&
                transform is not null &&
                SmoNodeTransformDecoder.TryFindOwnPositionPayloadOffset(
                    document, cursor, out _))
            {
                return cursor;
            }
            cursor = cursor.ParentIndex is int parentIndex &&
                     (uint)parentIndex < (uint)document.Objects.Count
                ? document.Objects[parentIndex]
                : null;
        }
        return null;
    }

    private static void VerifyPatchedContainer(
        SmoDocument source,
        byte[] output,
        IReadOnlyDictionary<int, Matrix4x4> expectedMatrices,
        IReadOnlyDictionary<int, NodeTransformWrite> expectedNodeTransforms,
        IReadOnlyDictionary<int, Vector3[]> expectedCollisionVertices)
    {
        SmoDocument verified = SmoDocument.Parse(output);
        if (verified.Objects.Count != source.Objects.Count ||
            verified.Header.DataStart != source.Header.DataStart)
        {
            throw new InvalidDataException(
                "Transform mutation changed the identity of the SMO object table.");
        }

        foreach ((int ownerIndex, Matrix4x4 expectedMatrix) in expectedMatrices)
        {
            if (!SmoStaticRenderObjectTransformDecoder.TryDecode(
                    verified, verified.Objects[ownerIndex], out Matrix4x4 actual) ||
                MatrixDifference(expectedMatrix, actual) > 0.00001f)
            {
                throw new InvalidDataException(
                    $"Placement [{ownerIndex}] failed post-write verification.");
            }
        }
        foreach ((int nodeIndex, NodeTransformWrite expected) in expectedNodeTransforms)
        {
            if (!SmoNodeTransformDecoder.TryDecode(
                    verified,
                    verified.Objects[nodeIndex],
                    out SmoNodeTransform? actual) ||
                actual is null ||
                MatrixDifference(expected.LocalMatrix, actual.LocalMatrix) > 0.001f)
            {
                throw new InvalidDataException(
                    $"Node [{nodeIndex}] failed post-write verification.");
            }
            if (!SmoNodeTransformDecoder.TryResolveNodeWorldMatrix(verified, verified.Objects[nodeIndex], out var world) ||
                MatrixDifference(expected.WorldMatrix, world) > 0.001f)
            {
                throw new NotSupportedException(
                    $"NODE_WORLD_WRITE_UNSUPPORTED: Node [{nodeIndex}] does not reproduce the requested world transform " +
                    "under Sparkplug PRS inheritance. The editor inverse path requires further work; no output is returned.");
            }
        }
        foreach ((int meshIndex, Vector3[] expected) in expectedCollisionVertices)
        {
            if (!SmoCollisionMeshDecoder.TryDecodeShape(
                    verified,
                    verified.Objects[meshIndex],
                    out Vector3[] actual,
                    out _) ||
                actual.Length != expected.Length ||
                actual.Where((position, index) =>
                    Vector3.Distance(position, expected[index]) > 0.00001f).Any())
            {
                throw new InvalidDataException(
                    $"Collision mesh [{meshIndex}] failed post-write verification.");
            }
        }
    }

    private static void ValidateMatrix(Matrix4x4 value, string parameterName)
    {
        if (!AllCells(value).All(float.IsFinite))
            throw new ArgumentException("Transform matrix must be finite.", parameterName);
    }

    private static IEnumerable<float> AllCells(Matrix4x4 value)
    {
        yield return value.M11; yield return value.M12;
        yield return value.M13; yield return value.M14;
        yield return value.M21; yield return value.M22;
        yield return value.M23; yield return value.M24;
        yield return value.M31; yield return value.M32;
        yield return value.M33; yield return value.M34;
        yield return value.M41; yield return value.M42;
        yield return value.M43; yield return value.M44;
    }

    private static float MatrixDifference(Matrix4x4 left, Matrix4x4 right) =>
        AllCells(left).Zip(AllCells(right), (a, b) => MathF.Abs(a - b)).Max();

    private static string CleanName(string name) => name.TrimEnd('\0');
}
