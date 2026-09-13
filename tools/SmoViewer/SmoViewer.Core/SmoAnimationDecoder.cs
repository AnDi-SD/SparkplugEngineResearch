using System.Buffers.Binary;
using System.Numerics;
using System.Text;

namespace SmoViewer.Core;

public sealed record SmoAnimationKey<T>(float Time, T Value);

public sealed record SmoAnimationTrack(
    string NodeName,
    IReadOnlyList<SmoAnimationKey<Vector3>> Positions,
    IReadOnlyList<SmoAnimationKey<Quaternion>> Rotations,
    IReadOnlyList<SmoAnimationKey<Vector3>> Scales);

public sealed record SmoAnimationClip(
    string Path,
    float Duration,
    IReadOnlyList<SmoAnimationTrack> Tracks)
{
    public int FrameCount => Tracks.SelectMany(track => new[]
        { track.Positions.Count, track.Rotations.Count, track.Scales.Count }).DefaultIfEmpty().Max();
}

/// <summary>Decoder for the single-object PC SAN animation container.</summary>
public static class SmoAnimationDecoder
{
    public const uint AnimationClassHash = 0x56EE563A;

    public static bool TryDecode(string path, out SmoAnimationClip? clip, out string error)
    {
        clip = null;
        error = string.Empty;
        try
        {
            SmoDocument document = SmoDocument.Load(path);
            SmoObjectEntry? entry = document.Objects.FirstOrDefault(item =>
                item.TypeHash == AnimationClassHash);
            if (entry is null || !entry.IsWithinDataSection || entry.SerializedSize > int.MaxValue ||
                entry.PhysicalOffset < 0 || entry.PhysicalEnd > document.Data.Length)
            {
                error = "SAN does not contain a valid 0x56EE563A animation object.";
                return false;
            }

            ReadOnlySpan<byte> data = document.Data.Span.Slice(
                (int)entry.PhysicalOffset, (int)entry.SerializedSize);
            int offset = 8;
            if (!ReadHeader(data, ref offset, 0, out ReadOnlySpan<byte> durationBytes) ||
                durationBytes.Length != 4)
            {
                error = "SAN duration field is missing.";
                return false;
            }
            float duration = ReadSingle(durationBytes);
            List<SmoAnimationTrack> tracks = [];
            List<SmoAnimationKey<Vector3>> positions = [];
            List<SmoAnimationKey<Quaternion>> rotations = [];
            List<SmoAnimationKey<Vector3>> scales = [];

            while (offset < data.Length && SmoDataBlockReader.TryReadHeader(data, offset, out var header))
            {
                if (header.FieldType == 0 && header.PayloadSize == 0)
                    break;
                ReadOnlySpan<byte> payload = data.Slice(header.PayloadOffset, (int)header.PayloadSize);
                offset = (int)header.PayloadEnd;
                switch (header.FieldType)
                {
                    case 2: positions = ReadVectorCurve(payload); break;
                    case 3: rotations = ReadQuaternionCurve(payload); break;
                    case 4: scales = ReadVectorCurve(payload); break;
                    case 1:
                        string name = ReadName(payload);
                        if (name.Length > 0)
                            tracks.Add(new SmoAnimationTrack(name, positions, rotations, scales));
                        positions = []; rotations = []; scales = [];
                        break;
                }
            }
            if (tracks.Count == 0)
            {
                error = "SAN contains no named animation tracks.";
                return false;
            }
            clip = new SmoAnimationClip(path, duration, tracks);
            return true;
        }
        catch (Exception exception)
        {
            error = exception.Message;
            return false;
        }
    }

    private static List<SmoAnimationKey<Vector3>> ReadVectorCurve(ReadOnlySpan<byte> payload)
    {
        int count = ReadCurveCount(payload);
        if (count == 0 || payload.Length != 8 + count * 16) return [];
        List<SmoAnimationKey<Vector3>> result = new(count);
        int valuesOffset = 8 + count * 4;
        for (int i = 0; i < count; i++)
            result.Add(new(ReadSingle(payload[(8 + i * 4)..]), new Vector3(
                ReadSingle(payload[(valuesOffset + i * 12)..]),
                ReadSingle(payload[(valuesOffset + i * 12 + 4)..]),
                ReadSingle(payload[(valuesOffset + i * 12 + 8)..]))));
        return result;
    }

    private static List<SmoAnimationKey<Quaternion>> ReadQuaternionCurve(ReadOnlySpan<byte> payload)
    {
        int count = ReadCurveCount(payload);
        if (count == 0 || payload.Length != 8 + count * 20) return [];
        List<SmoAnimationKey<Quaternion>> result = new(count);
        int valuesOffset = 8 + count * 4;
        for (int i = 0; i < count; i++)
        {
            Quaternion value = new(
                ReadSingle(payload[(valuesOffset + i * 16)..]),
                ReadSingle(payload[(valuesOffset + i * 16 + 4)..]),
                ReadSingle(payload[(valuesOffset + i * 16 + 8)..]),
                ReadSingle(payload[(valuesOffset + i * 16 + 12)..]));
            result.Add(new(ReadSingle(payload[(8 + i * 4)..]), Quaternion.Normalize(value)));
        }
        return result;
    }

    private static int ReadCurveCount(ReadOnlySpan<byte> payload) => payload.Length >= 8
        ? checked((int)BinaryPrimitives.ReadUInt32LittleEndian(payload[4..])) : 0;

    private static string ReadName(ReadOnlySpan<byte> payload)
    {
        if (payload.Length < 2) return string.Empty;
        int length = BinaryPrimitives.ReadUInt16LittleEndian(payload);
        if (length <= 0 || length > payload.Length - 2) return string.Empty;
        return Encoding.Latin1.GetString(payload.Slice(2, length)).TrimEnd('\0');
    }

    private static bool ReadHeader(ReadOnlySpan<byte> data, ref int offset, int type,
        out ReadOnlySpan<byte> payload)
    {
        payload = default;
        if (!SmoDataBlockReader.TryReadHeader(data, offset, out var header) ||
            header.FieldType != type || header.PayloadSize > int.MaxValue) return false;
        payload = data.Slice(header.PayloadOffset, (int)header.PayloadSize);
        offset = (int)header.PayloadEnd;
        return true;
    }

    private static float ReadSingle(ReadOnlySpan<byte> data) =>
        BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(data));
}
