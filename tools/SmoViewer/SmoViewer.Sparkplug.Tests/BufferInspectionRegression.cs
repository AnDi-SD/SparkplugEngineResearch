using System.Numerics;
using System.Runtime.InteropServices;
using SmoViewer.Sparkplug;

internal static class BufferInspectionRegression
{
    internal static unsafe int Run()
    {
        int checks = 0;
        void Check(bool value, string message) { ++checks; if (!value) throw new InvalidDataException(message); }
        Check(Marshal.SizeOf<NativeMethods.IndexBufferInfo>() == 24 &&
            Marshal.SizeOf<NativeMethods.VertexBufferInfo>() == 28, "CPU buffer C ABI layouts");
        byte[] indexBytes = Convert.FromHexString("02000000010000000000000000000100FFFF");
        byte[] snapshot = indexBytes.ToArray();
        IndexBufferHandle indices;
        fixed (byte* input = indexBytes)
            indices = new(NativeMethods.Check(NativeMethods.spv_index_buffer_read(input, (uint)indexBytes.Length)));
        using (indices)
        {
            Check(indexBytes.AsSpan().SequenceEqual(snapshot), "C ABI index read preserves input");
            Array.Clear(indexBytes);
            NativeMethods.Check(NativeMethods.spv_index_buffer_info(indices, out var info));
            Check(info.PrimitiveType == 2 && info.PrimitiveCount == 1 && info.IndexCount == 3 &&
                info.FormatFlags == 0 && info.ElementSize == 2 && info.FinalPosition == 18, "C ABI index metadata");
            uint[] values = new uint[3];
            fixed (uint* output = values)
            {
                Check(NativeMethods.spv_index_buffer_indices(indices, output, 2) == 0, "C ABI rejects short index output");
                NativeMethods.Check(NativeMethods.spv_index_buffer_indices(indices, output, 3));
            }
            Check(values.SequenceEqual(new uint[] { 0, 1, 65535 }), "C ABI owns decoded indices beyond input pin");
            Check(NativeMethods.spv_index_buffer_indices(indices, null, 3) == 0, "C ABI rejects null index output");
        }
        try { NativeMethods.spv_index_buffer_info(indices, out _); throw new InvalidDataException("Disposed index handle accepted"); }
        catch (ObjectDisposedException) { Check(true, "SafeHandle blocks disposed index owner"); }

        using var wire = new MemoryStream();
        using (var writer = new BinaryWriter(wire, System.Text.Encoding.UTF8, leaveOpen: true))
        {
            writer.Write(0x40u); writer.Write(2u); writer.Write(7u);
            foreach (float value in new float[] { 1, 2, 3, 4, 5, 6, 11, 12, 13, 14, 15, 16 }) writer.Write(value);
        }
        byte[] vertexBytes = wire.ToArray(); snapshot = vertexBytes.ToArray();
        VertexBufferHandle vertices;
        fixed (byte* input = vertexBytes)
            vertices = new(NativeMethods.Check(NativeMethods.spv_vertex_buffer_read(input, (uint)vertexBytes.Length)));
        using (vertices)
        {
            Check(vertexBytes.AsSpan().SequenceEqual(snapshot), "C ABI vertex read preserves input");
            Array.Clear(vertexBytes);
            NativeMethods.Check(NativeMethods.spv_vertex_buffer_info(vertices, out var info));
            Check(info.ComponentFlags == 0x40 && info.VertexCount == 2 && info.Flags == 7 &&
                info.Stride == 24 && info.ComponentCount == 6 && info.ByteCount == 48 && info.FinalPosition == 60,
                "C ABI preserves generic buffer layout and flags without occluder restrictions");
            Vector3[] positions = new Vector3[2];
            fixed (Vector3* output = positions)
            {
                Check(NativeMethods.spv_vertex_buffer_positions(vertices, output, 3) == 0, "C ABI rejects short position output");
                NativeMethods.Check(NativeMethods.spv_vertex_buffer_positions(vertices, output, 6));
            }
            Check(positions.SequenceEqual(new[] { new Vector3(1, 2, 3), new Vector3(11, 12, 13) }),
                "C ABI copies positions from owned buffer with native stride");
            Check(NativeMethods.spv_vertex_buffer_positions(vertices, null, 6) == 0, "C ABI rejects null position output");
        }
        try { NativeMethods.spv_vertex_buffer_info(vertices, out _); throw new InvalidDataException("Disposed vertex handle accepted"); }
        catch (ObjectDisposedException) { Check(true, "SafeHandle blocks disposed vertex owner"); }
        byte[] huge = Convert.FromHexString("00000000FFFFFFFF00000000");
        fixed (byte* input = huge)
        {
            Check(NativeMethods.spv_vertex_buffer_read(input, 12) == IntPtr.Zero, "Huge vertex count rejected before allocation");
            Check(NativeMethods.spv_index_buffer_read(input, 16 * 1024 * 1024 + 1) == IntPtr.Zero,
                "Input cap rejected before dereferencing claimed bytes");
        }
        Check(NativeMethods.spv_index_buffer_read(null, 12) == IntPtr.Zero &&
            NativeMethods.spv_vertex_buffer_read(null, 12) == IntPtr.Zero, "Null inputs return failure handles");
        Console.WriteLine($"PASS CPU buffer interop: {checks} checks");
        return 0;
    }
}
