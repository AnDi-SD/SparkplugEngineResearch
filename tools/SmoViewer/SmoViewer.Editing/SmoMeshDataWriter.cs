using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Typed editing input for the original CPU mesh/buffer writers.</summary>
public static class SmoMeshDataWriter
{
    public static unsafe byte[] CreateTriangleList(SmoMesh mesh)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        if (mesh.PrimitiveType != SmoMeshDecoder.TriangleListPrimitive ||
            mesh.Marker is not (SmoMeshDecoder.E0Marker or SmoMeshDecoder.E1Marker))
            throw new NotSupportedException("Mesh writer requires portable or PC UInt16 triangle input.");
        int count = mesh.VertexCount;
        foreach (int length in new[] { mesh.Normals.Length, mesh.TextureCoordinates.Length,
            mesh.TextureCoordinates1.Length, mesh.DiffuseColorsArgb.Length, mesh.BlendWeights.Length, mesh.BlendIndices.Length })
            if (length != 0 && length != count) throw new ArgumentException("Mesh attribute count differs from positions.", nameof(mesh));
        var vertices = new NativeMethods.MeshVertex[count];
        for (int i = 0; i < count; i++)
        {
            ref var vertex = ref vertices[i]; vertex.Position = mesh.Positions[i];
            if (mesh.Normals.Length != 0) vertex.Normal = mesh.Normals[i];
            if (mesh.TextureCoordinates.Length != 0) vertex.Uv0 = mesh.TextureCoordinates[i];
            if (mesh.TextureCoordinates1.Length != 0) vertex.Uv1 = mesh.TextureCoordinates1[i];
            if (mesh.DiffuseColorsArgb.Length != 0) vertex.Color = mesh.DiffuseColorsArgb[i];
            if (mesh.BlendWeights.Length != 0) vertex.Weights = mesh.BlendWeights[i];
            if (mesh.BlendIndices.Length != 0)
            {
                var b = mesh.BlendIndices[i];
                vertex.Bones = (uint)b.X | (uint)b.Y << 8 | (uint)b.Z << 16 | (uint)b.W << 24;
            }
        }
        SerializedBytesHandle owner;
        fixed (NativeMethods.MeshVertex* input = vertices)
        fixed (uint* indices = mesh.TriangleIndices)
            owner = new SerializedBytesHandle(NativeMethods.Check(NativeMethods.spv_mesh_write_triangles(
                input, checked((uint)count), indices, checked((uint)mesh.TriangleIndices.Length),
                mesh.VertexFormat, mesh.Marker == SmoMeshDecoder.E0Marker ? 0u : 1u)));
        return CopyResult(owner);
    }

    internal static unsafe byte[] CopyResult(SerializedBytesHandle owner)
    {
        using (owner)
        {
            NativeMethods.Check(NativeMethods.spv_serialized_bytes_size(owner, out uint size));
            byte[] output = new byte[checked((int)size)];
            fixed (byte* bytes = output) NativeMethods.Check(NativeMethods.spv_serialized_bytes_copy(owner, bytes, size));
            return output;
        }
    }
}
