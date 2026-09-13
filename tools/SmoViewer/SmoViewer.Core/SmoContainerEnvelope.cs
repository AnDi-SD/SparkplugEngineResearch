using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>An already selected/relocated entry; names retain their exact wire bytes.</summary>
public readonly record struct SmoContainerEntry(
    uint Id, ReadOnlyMemory<byte> RawName, uint TypeHash, uint LogicalOffset, uint SerializedSize);

/// <summary>
/// Thin adapter to the shared HOST FFPS envelope encoder. This is not an
/// original whole-file writer and does not interpret or relocate object data.
/// Only the existing inline FAT and empty external-file table are supported.
/// </summary>
public sealed unsafe class SmoContainerEnvelope
{
    private readonly NativeMethods.EnvelopeHeader _header;
    private readonly NativeMethods.EnvelopeEntry[] _entries;
    private readonly byte[] _names;
    private readonly uint _dataSize;

    public int DataStart { get; }
    public int FileSize { get; }

    public SmoContainerEnvelope(SmoHeader header, IReadOnlyList<SmoContainerEntry> entries,
        int dataSize) : this(header.SerializerVersion, header.Unknown08, header.PlatformMask,
            entries, dataSize) { }

    public SmoContainerEnvelope(uint version, uint exportTag, uint platformMask,
        IReadOnlyList<SmoContainerEntry> entries, int dataSize)
    {
        ArgumentNullException.ThrowIfNull(entries);
        ArgumentOutOfRangeException.ThrowIfNegative(dataSize);
        _header = new() { Signature = 0x53504646, Version = version,
            ExportTag = exportTag, PlatformMask = platformMask };
        _dataSize = checked((uint)dataSize);
        _entries = new NativeMethods.EnvelopeEntry[entries.Count];
        int nameBytes = 0;
        for (int i = 0; i < entries.Count; i++)
        {
            // These are marshaling buffer offsets, not FAT size calculations.
            SmoContainerEntry entry = entries[i];
            _entries[i] = new() { Id = entry.Id, ClassId = entry.TypeHash,
                Offset = entry.LogicalOffset, Size = entry.SerializedSize,
                NameOffset = checked((uint)nameBytes), NameSize = checked((uint)entry.RawName.Length) };
            nameBytes = checked(nameBytes + entry.RawName.Length);
        }
        _names = new byte[nameBytes];
        for (int i = 0; i < entries.Count; i++)
            entries[i].RawName.Span.CopyTo(_names.AsSpan(checked((int)_entries[i].NameOffset)));
        fixed (NativeMethods.EnvelopeEntry* index = _entries)
        fixed (byte* names = _names)
        {
            NativeMethods.Check(NativeMethods.spv_envelope_measure(index, checked((uint)_entries.Length),
                names, checked((uint)_names.Length), _dataSize, out var layout));
            DataStart = checked((int)layout.DataOffset);
            FileSize = checked((int)layout.FileSize);
        }
    }

    public byte[] AllocateContainer()
    {
        byte[] result = new byte[FileSize];
        WritePrefix(result.AsSpan(0, DataStart));
        return result;
    }

    public void WritePrefix(Span<byte> destination)
    {
        if (destination.Length < DataStart)
            throw new ArgumentException("Envelope output is too short.", nameof(destination));
        fixed (NativeMethods.EnvelopeEntry* index = _entries)
        fixed (byte* names = _names)
        fixed (byte* output = destination)
            NativeMethods.Check(NativeMethods.spv_envelope_write(in _header,
                index, checked((uint)_entries.Length), names, checked((uint)_names.Length),
                _dataSize, output, checked((uint)destination.Length)));
    }

    public void WritePrefix(Stream output)
    {
        ArgumentNullException.ThrowIfNull(output);
        if (!output.CanWrite) throw new ArgumentException("Stream is not writable.", nameof(output));
        byte[] prefix = new byte[DataStart];
        WritePrefix(prefix.AsSpan());
        output.Write(prefix);
    }
}
