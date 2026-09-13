using System.Numerics;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Borrowed slices of one owned managed copy of the actual writer output.</summary>
public readonly record struct SmoStaticMatrixPayloads(ReadOnlyMemory<byte> World, ReadOnlyMemory<byte> Inverse);

/// <summary>
/// Authoring adapter over spStaticRenderObjectSerializer's existing writer.
/// Both matrices are supplied independently. The editor's finite-input guard
/// does not imply an affine shape or any inverse calculation by the game.
/// </summary>
public static class SmoStaticMatrixWriter
{
    public static SmoStaticMatrixPayloads Encode(Matrix4x4 world, Matrix4x4 inverse)
    {
        var owner = new SerializedBytesHandle(NativeMethods.Check(
            NativeMethods.spv_static_write_matrix_fields(in world, in inverse)));
        byte[] fields = SmoMeshDataWriter.CopyResult(owner);
        if (!SmoDataBlockReader.TryReadHeader(fields, 0, out var worldField) ||
            worldField.FieldType != 1 || worldField.PayloadSize != 64 ||
            !SmoDataBlockReader.TryReadHeader(fields, checked((int)worldField.PayloadEnd), out var inverseField) ||
            inverseField.FieldType != 2 || inverseField.PayloadSize != 64 ||
            !SmoDataBlockReader.TryReadHeader(fields, checked((int)inverseField.PayloadEnd), out var terminal) ||
            terminal.SizeKind != SmoDataBlockSizeCode.Empty || terminal.PayloadEnd != fields.Length)
            throw new InvalidDataException("Shared StaticRenderObject writer returned an unexpected matrix-field extent.");
        return new(fields.AsMemory(worldField.PayloadOffset, 64), fields.AsMemory(inverseField.PayloadOffset, 64));
    }

    /// <summary>
    /// Copies the two encoded payloads into caller-selected, non-overlapping
    /// existing fields. Both ranges and both values are validated before writes.
    /// Headers, references and surrounding bytes remain the caller's responsibility.
    /// </summary>
    public static void PatchPayloads(Span<byte> destination, int worldPayloadOffset, int inversePayloadOffset,
        Matrix4x4 world, Matrix4x4 inverse)
    {
        if ((uint)worldPayloadOffset > (uint)destination.Length || destination.Length - worldPayloadOffset < 64)
            throw new ArgumentOutOfRangeException(nameof(worldPayloadOffset));
        if ((uint)inversePayloadOffset > (uint)destination.Length || destination.Length - inversePayloadOffset < 64)
            throw new ArgumentOutOfRangeException(nameof(inversePayloadOffset));
        if (worldPayloadOffset < inversePayloadOffset + 64 && inversePayloadOffset < worldPayloadOffset + 64)
            throw new ArgumentException("Static matrix payload destinations overlap.");
        SmoStaticMatrixPayloads encoded = Encode(world, inverse);
        encoded.World.Span.CopyTo(destination.Slice(worldPayloadOffset, 64));
        encoded.Inverse.Span.CopyTo(destination.Slice(inversePayloadOffset, 64));
    }
}
