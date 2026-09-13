using System.Numerics;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Geometry-only MeshBV creation through the original owning classes and serializer.</summary>
public static class SmoMeshBoundingVolumeWriter
{
    public static unsafe byte[] CreateTriangleList(IReadOnlyList<Vector3> positions, IReadOnlyList<int> triangleIndices)
    {
        ArgumentNullException.ThrowIfNull(positions); ArgumentNullException.ThrowIfNull(triangleIndices);
        if (positions.Count is < 1 or > 65536 || triangleIndices.Count is < 1 or > 3000000)
            throw new ArgumentOutOfRangeException(nameof(positions), "Collision input exceeds the host writer budget.");
        var vertices = new NativeMethods.MeshVertex[positions.Count];
        for (int i = 0; i < positions.Count; i++) vertices[i].Position = positions[i];
        var indices = new uint[triangleIndices.Count];
        for (int i = 0; i < indices.Length; i++) indices[i] = checked((uint)triangleIndices[i]);
        SerializedBytesHandle owner;
        fixed (NativeMethods.MeshVertex* input = vertices)
        fixed (uint* inputIndices = indices)
            owner = new SerializedBytesHandle(NativeMethods.Check(NativeMethods.spv_mesh_bv_write_triangles(
                input, checked((uint)vertices.Length), inputIndices, checked((uint)indices.Length))));
        return SmoMeshDataWriter.CopyResult(owner);
    }
}
