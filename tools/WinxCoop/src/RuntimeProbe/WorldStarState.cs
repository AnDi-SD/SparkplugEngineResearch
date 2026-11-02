using System.Buffers.Binary;

namespace WinxCoop.RuntimeProbe;

// Copy-owned presentation data, distinct from Character ActorRecord and native
// Star logic. Category/slot select the original model pool; they are not reward
// amounts. Only the server's native activation supplies ActivationId.
internal sealed record WorldStarState(ulong ActivationId, uint Category, uint Slot,
    uint StateC8, uint Flags, float[] Position, float[] Scale, float[] Rotation)
{
    internal const float MaximumPositionMagnitude = 10000000;

    internal WorldStarState DeepCopy() => new(ActivationId, Category, Slot, StateC8, Flags,
        (float[])Position.Clone(), (float[])Scale.Clone(), (float[])Rotation.Clone());
    internal void Validate()
    {
        if (HasFinitePositionOutsidePresentationEnvelope())
            throw new InvalidDataException("Star presentation record is outside its bounded scalar contract.");
    }

    // Host capture alone may omit this one unsupported cosmetic condition.
    // Metadata, shape, nonfinite values, scale and rotation remain fatal even
    // for an activation whose presentation was omitted in an earlier capture.
    internal bool HasFinitePositionOutsidePresentationEnvelope()
    {
        if (ActivationId == 0 || Category > 3 || Slot >= (Category == 3 ? 100u : 50u) ||
            (Flags & ~17u) != 0 || (Flags & WorldStarContract.Present) == 0 ||
            !Finite(Position, 3, float.MaxValue) || !Finite(Scale, 3, 10000) || !Finite(Rotation, 9, 2))
            throw new InvalidDataException("Star presentation record is outside its bounded scalar contract.");
        // Zero/signed visual scales are preserved; no physics invertibility or
        // guessed native stateC8/amount domain is imposed on this presentation.
        return Position.Any(value => MathF.Abs(value) > MaximumPositionMagnitude);

        static bool Finite(float[]? values, int count, float maximum) => values is not null &&
            values.Length == count && values.All(value => float.IsFinite(value) && MathF.Abs(value) <= maximum);
    }
}

internal static class WorldStarContract
{
    internal const int MaximumStars = 16, MaxStars = MaximumStars, HeaderBytes = 16, RecordBytes = 88;
    internal const uint Present = 1, Visible = 16;
    internal static byte[] Pack(IReadOnlyList<WorldStarState> records)
    {
        Validate(records);
        byte[] result = new byte[HeaderBytes + records.Count * RecordBytes];
        W(0, 1); W(4, (uint)records.Count); W(8, RecordBytes);
        for (int index = 0; index < records.Count; ++index)
        {
            var value = records[index]; int at = HeaderBytes + index * RecordBytes;
            BinaryPrimitives.WriteUInt64LittleEndian(result.AsSpan(at, 8), value.ActivationId);
            W(at + 8, value.Category); W(at + 12, value.Slot); W(at + 16, value.StateC8); W(at + 20, value.Flags);
            F(at + 24, value.Position); F(at + 36, value.Scale); F(at + 48, value.Rotation);
        }
        return result;
        void W(int offset, uint value) => BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(offset, 4), value);
        void F(int offset, float[] values)
        {
            for (int index = 0; index < values.Length; ++index)
                BinaryPrimitives.WriteInt32LittleEndian(result.AsSpan(offset + index * 4, 4), BitConverter.SingleToInt32Bits(values[index]));
        }
    }

    internal static IReadOnlyList<WorldStarState> Unpack(byte[] payload)
    {
        if (payload.Length < HeaderBytes || U(0) != 1 || U(4) > MaximumStars || U(8) != RecordBytes || U(12) != 0 ||
            payload.Length != HeaderBytes + U(4) * RecordBytes)
            throw new InvalidDataException("Star presentation header/size/reserved value differs.");
        List<WorldStarState> result = [];
        for (int index = 0; index < U(4); ++index)
        {
            int at = HeaderBytes + index * RecordBytes;
            if (U(at + 84) != 0) throw new InvalidDataException("Star presentation record reserved value is nonzero.");
            result.Add(new(BinaryPrimitives.ReadUInt64LittleEndian(payload.AsSpan(at, 8)),
                U(at + 8), U(at + 12), U(at + 16), U(at + 20),
                F(at + 24, 3), F(at + 36, 3), F(at + 48, 9)));
        }
        Validate(result); return Array.AsReadOnly(result.ToArray());
        uint U(int offset) => BinaryPrimitives.ReadUInt32LittleEndian(payload.AsSpan(offset, 4));
        float[] F(int offset, int count) => Enumerable.Range(0, count).Select(index =>
            BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(payload.AsSpan(offset + index * 4, 4)))).ToArray();
    }

    internal static void Validate(IReadOnlyList<WorldStarState> records)
    {
        ArgumentNullException.ThrowIfNull(records);
        if (records.Count > MaximumStars) throw new InvalidDataException("Star presentation exceeds16 active records.");
        HashSet<ulong> ids = []; HashSet<(uint, uint)> slots = [];
        foreach (var record in records)
        {
            record.Validate();
            if (!ids.Add(record.ActivationId) || !slots.Add((record.Category, record.Slot)))
                throw new InvalidDataException("Star presentation repeats an activation or native pool slot.");
        }
    }
}
