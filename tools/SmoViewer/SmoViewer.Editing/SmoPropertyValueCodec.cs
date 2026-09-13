using System.Buffers.Binary;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Shared little-endian encoding for schema-backed property values.</summary>
public static class SmoPropertyValueCodec
{
    public static byte[] Encode(Vector3 value)
    {
        byte[] data = new byte[12];
        Write(data, value);
        return data;
    }

    public static byte[] Encode(Quaternion value)
    {
        byte[] data = new byte[16];
        Write(data, value);
        return data;
    }

    public static byte[] Encode(Matrix4x4 value)
    {
        byte[] data = new byte[64];
        Write(data, value);
        return data;
    }

    public static void Write(Span<byte> destination, Vector3 value)
    {
        Require(destination, 12);
        WriteSingle(destination, 0, value.X);
        WriteSingle(destination, 4, value.Y);
        WriteSingle(destination, 8, value.Z);
    }

    public static void Write(Span<byte> destination, Quaternion value)
    {
        Require(destination, 16);
        WriteSingle(destination, 0, value.X);
        WriteSingle(destination, 4, value.Y);
        WriteSingle(destination, 8, value.Z);
        WriteSingle(destination, 12, value.W);
    }

    public static void Write(Span<byte> destination, Matrix4x4 value)
    {
        Require(destination, 64);
        ReadOnlySpan<float> cells =
        [
            value.M11, value.M12, value.M13, value.M14,
            value.M21, value.M22, value.M23, value.M24,
            value.M31, value.M32, value.M33, value.M34,
            value.M41, value.M42, value.M43, value.M44
        ];
        for (int index = 0; index < cells.Length; index++)
            WriteSingle(destination, index * 4, cells[index]);
    }

    private static void WriteSingle(Span<byte> data, int offset, float value)
    {
        if (!float.IsFinite(value))
            throw new ArgumentException("Property values must be finite.", nameof(value));
        BinaryPrimitives.WriteInt32LittleEndian(
            data[offset..], BitConverter.SingleToInt32Bits(value));
    }

    private static void Require(ReadOnlySpan<byte> destination, int size)
    {
        if (destination.Length < size)
            throw new ArgumentException(
                $"Property destination needs at least {size} bytes.",
                nameof(destination));
    }
}
