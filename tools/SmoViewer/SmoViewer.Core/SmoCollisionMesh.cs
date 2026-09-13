using SmoViewer.Sparkplug;
using System.Collections.ObjectModel;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Confirmed triangle collision geometry stored by spMeshBV.</summary>
public sealed record SmoCollisionMesh(
    int NodeObjectIndex,
    int CollisionInfoObjectIndex,
    int MeshBoundingVolumeObjectIndex,
    string Name,
    IReadOnlyList<Vector3> Positions,
    IReadOnlyList<int> TriangleIndices,
    Matrix4x4 WorldTransform);

/// <summary>
/// One sparse wxFaceData record associated with a collision triangle.
/// Zero is the serializer default for every member and is therefore omitted
/// from the field stream.
/// </summary>
public sealed record SmoMeshBoundingVolumeFaceData(
    byte SurfaceType,
    ushort Flags,
    byte SurfaceId,
    byte SerializedFieldMask);

/// <summary>Complete confirmed spMeshBV payload shared by PC and PS2.</summary>
public sealed record SmoMeshBoundingVolumeData(
    uint Version,
    IReadOnlyList<Vector3> Positions,
    IReadOnlyList<int> TriangleIndices,
    IReadOnlyList<SmoMeshBoundingVolumeFaceData>? FaceData,
    byte SerializedFieldMask)
{
    /// <summary>Original index-buffer primitive type. Version is a legacy DTO name.</summary>
    public uint PrimitiveType => Version;
    /// <summary>Observed vertex-array position relative to the object field stream.</summary>
    public int VertexPayloadOffset { get; internal init; }
    public int TriangleCount => TriangleIndices.Count / 3;
    public int VertexCount => Positions.Count;
}

public static class SmoMeshBoundingVolumeDecoder
{
    public const uint TriangleListPrimitiveType = 2;
    // Kept for source compatibility with editing clients; this word is not a version.
    public const uint CurrentVersion = TriangleListPrimitiveType;
    public const uint FaceDataClassId = 0x313C4C17;

    // Inspector labels for original numeric surface values; no gameplay behavior.
    public static string GetSurfaceTypeName(byte value) => value switch
    {
        0 => "unspecified", 1 => "stone", 2 => "dirt", 3 => "grass",
        4 => "water", 5 => "snow", 6 => "swamp", 7 => "mud",
        8 => "deepwater", 9 => "carpet", _ => "unknown"
    };

    public static bool TryDecode(SmoDocument document, SmoObjectEntry entry,
        out SmoMeshBoundingVolumeData? data, out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        data = null;
        error = string.Empty;
        if (entry.TypeHash != SmoClassIds.MeshBoundingVolume ||
            !entry.IsWithinDataSection || !entry.SignatureMatches ||
            entry.PhysicalOffset < 0 || entry.SerializedSize < 8 ||
            entry.PhysicalEnd > document.Data.Length)
        {
            error = "Object is not a bounded spMeshBV resource.";
            return false;
        }
        try
        {
            using var native = new NativeView(document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset + 8), checked((int)entry.SerializedSize - 8)), 0);
            native.CopyGeometry(out Vector3[] positions, out int[] indices);
            var faces = native.Info.HasFaces != 0 ? native.CopyFaces() : null;
            data = new SmoMeshBoundingVolumeData(native.Info.PrimitiveType,
                Array.AsReadOnly(positions), Array.AsReadOnly(indices),
                faces is null ? null : Array.AsReadOnly(faces), checked((byte)native.Info.FieldMask))
            { VertexPayloadOffset = checked((int)native.Info.VertexPayloadOffset) };
            return true;
        }
        catch (InvalidDataException exception) { error = exception.Message; return false; }
    }

    public static bool TryDecodeGeometry(ReadOnlySpan<byte> payload,
        out uint version, out Vector3[] positions, out int[] triangleIndices,
        out int vertexPayloadOffset, out string error)
    {
        version = 0; positions = []; triangleIndices = []; vertexPayloadOffset = 0;
        error = string.Empty;
        try
        {
            using var native = new NativeView(payload, 1);
            native.CopyGeometry(out positions, out triangleIndices);
            version = native.Info.PrimitiveType;
            vertexPayloadOffset = checked((int)native.Info.VertexPayloadOffset);
            return true;
        }
        catch (InvalidDataException exception) { error = exception.Message; return false; }
    }

    public static bool TryDecodeFaceData(ReadOnlySpan<byte> payload, int expectedFaceCount,
        out SmoMeshBoundingVolumeFaceData[] faces, out string error)
    {
        // Compatibility argument only. Original spFaceDataContainer reads its
        // own count and does not compare it with the mesh's triangle count.
        _ = expectedFaceCount;
        faces = []; error = string.Empty;
        try
        {
            using var native = new NativeView(payload, 2);
            faces = native.CopyFaces();
            return true;
        }
        catch (InvalidDataException exception) { error = exception.Message; return false; }
    }

    private sealed unsafe class NativeView : IDisposable
    {
        private readonly MeshBVHandle handle;
        internal NativeMethods.MeshBVInfo Info { get; }
        internal NativeView(ReadOnlySpan<byte> payload, uint kind)
        {
            fixed (byte* input = payload)
                handle = new MeshBVHandle(NativeMethods.Check(
                    NativeMethods.spv_mesh_bv_read(input, checked((uint)payload.Length), kind)));
            try
            {
                NativeMethods.Check(NativeMethods.spv_mesh_bv_info(handle, out var info));
                Info = info;
            }
            catch { handle.Dispose(); throw; }
        }
        internal void CopyGeometry(out Vector3[] positions, out int[] indices)
        {
            positions = new Vector3[checked((int)Info.Vertices)];
            indices = new int[checked((int)Info.Indices)];
            fixed (Vector3* vertices = positions)
            fixed (int* triangles = indices)
                NativeMethods.Check(NativeMethods.spv_mesh_bv_geometry(
                    handle, vertices, checked(Info.Vertices * 3), triangles, Info.Indices));
        }
        internal SmoMeshBoundingVolumeFaceData[] CopyFaces()
        {
            var values = new NativeMethods.FaceData[checked((int)Info.Faces)];
            fixed (NativeMethods.FaceData* output = values)
                NativeMethods.Check(NativeMethods.spv_mesh_bv_faces(handle, output, Info.Faces));
            return values.Select(face => new SmoMeshBoundingVolumeFaceData(
                checked((byte)face.SurfaceType), checked((ushort)face.Flags),
                checked((byte)face.SurfaceId), checked((byte)face.FieldMask))).ToArray();
        }
        public void Dispose() => handle.Dispose();
    }
}

public static class SmoCollisionMeshDecoder
{
    public static IReadOnlyList<SmoCollisionMesh> DecodeAll(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        var result = new List<SmoCollisionMesh>();
        foreach (SmoObjectEntry collisionInfo in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.CollisionInfo))
        {
            if (collisionInfo.ParentIndex is not int nodeIndex ||
                (uint)nodeIndex >= (uint)document.Objects.Count)
            {
                continue;
            }
            SmoObjectEntry node = document.Objects[nodeIndex];
            SmoObjectEntry? shape = document.Objects.FirstOrDefault(entry =>
                entry.ParentIndex == collisionInfo.Index &&
                entry.TypeHash == SmoClassIds.MeshBoundingVolume);
            if (shape is null ||
                !TryDecodeShape(document, shape, out Vector3[] positions,
                    out int[] indices))
            {
                continue;
            }

            Matrix4x4 worldTransform;
            if (SmoCollisionInfoTransformDecoder.TryDecode(
                    document, collisionInfo,
                    out SmoCollisionInfoTransform? collisionTransform) &&
                collisionTransform is not null)
            {
                worldTransform = collisionTransform.WorldMatrix;
            }
            else if (!SmoNodeTransformDecoder.TryResolveNodeWorldMatrix(
                         document, node, out worldTransform))
            {
                continue;
            }

            result.Add(new SmoCollisionMesh(
                node.Index,
                collisionInfo.Index,
                shape.Index,
                string.IsNullOrWhiteSpace(node.Name.TrimEnd('\0'))
                    ? $"Collision_{collisionInfo.Index}"
                    : node.Name.TrimEnd('\0'),
                new ReadOnlyCollection<Vector3>(positions),
                new ReadOnlyCollection<int>(indices),
                worldTransform));
        }
        return new ReadOnlyCollection<SmoCollisionMesh>(result);
    }

    public static bool TryDecodeShape(
        SmoDocument document,
        SmoObjectEntry entry,
        out Vector3[] positions,
        out int[] triangleIndices)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        positions = [];
        triangleIndices = [];
        if (!SmoMeshBoundingVolumeDecoder.TryDecode(
                document,entry,out SmoMeshBoundingVolumeData? data,out _) ||
            data is null)
            return false;
        positions = data.Positions.ToArray();
        triangleIndices = data.TriangleIndices.ToArray();
        return true;
    }

    internal static bool TryFindVertexPayloadOffset(
        SmoDocument document,
        SmoObjectEntry entry,
        out int absoluteOffset,
        out int vertexCount)
    {
        absoluteOffset = 0;
        vertexCount = 0;
        if (!SmoMeshBoundingVolumeDecoder.TryDecode(
                document,entry,out SmoMeshBoundingVolumeData? data,out _) ||
            data is null)
            return false;
        absoluteOffset = checked((int)entry.PhysicalOffset + 8 + data.VertexPayloadOffset);
        vertexCount = data.VertexCount;
        return true;
    }
}
