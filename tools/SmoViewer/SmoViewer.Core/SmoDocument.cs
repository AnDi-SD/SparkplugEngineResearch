using System.Buffers.Binary;
using System.Collections.ObjectModel;
using System.Text;

namespace SmoViewer.Core;

/// <summary>
/// A parsed FFPS/SMO container. Parsing preserves the complete source bytes and
/// keeps recoverable consistency problems in <see cref="Diagnostics"/>.
/// </summary>
public sealed class SmoDocument
{
    private static ReadOnlySpan<byte> FileSignature => "FFPS"u8;
    private static ReadOnlySpan<byte> ObjectTag => "SBOO"u8;
    private static readonly UTF8Encoding StrictUtf8 = new(false, true);

    private SmoDocument(
        string? sourcePath,
        ReadOnlyMemory<byte> data,
        SmoHeader header,
        IReadOnlyList<SmoObjectEntry> objects,
        IReadOnlyList<SmoDiagnostic> diagnostics)
    {
        SourcePath = sourcePath;
        Data = data;
        Header = header;
        Objects = objects;
        Diagnostics = diagnostics;
    }

    public string? SourcePath { get; }
    public ReadOnlyMemory<byte> Data { get; }
    public SmoHeader Header { get; }
    public IReadOnlyList<SmoObjectEntry> Objects { get; }
    public IReadOnlyList<SmoDiagnostic> Diagnostics { get; }
    public bool HasErrors => Diagnostics.Any(
        diagnostic => diagnostic.Severity == SmoDiagnosticSeverity.Error);

    public static SmoDocument Load(string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        return ParseOwned(File.ReadAllBytes(path), path);
    }

    public static SmoDocument Parse(
        ReadOnlyMemory<byte> data,
        string? sourcePath = null) =>
        ParseOwned(data.ToArray(), sourcePath);

    private static SmoDocument ParseOwned(byte[] data, string? sourcePath)
    {
        ReadOnlyMemory<byte> memory = data;
        ReadOnlySpan<byte> span = data;
        if (span.Length < SmoHeader.Size)
        {
            throw new SmoFormatException(
                $"The file is {span.Length} bytes long; an FFPS header needs " +
                $"{SmoHeader.Size} bytes.");
        }

        if (!span[..FileSignature.Length].SequenceEqual(FileSignature))
            throw new SmoFormatException("The file does not start with the FFPS signature.");

        var header = new SmoHeader(
            Encoding.ASCII.GetString(span[..4]),
            ReadUInt32(span, 0x04),
            ReadUInt32(span, 0x08),
            ReadUInt32(span, 0x0C),
            ReadUInt32(span, 0x10),
            ReadUInt32(span, 0x14),
            ReadUInt32(span, 0x18),
            ReadUInt32(span, 0x1C));

        var diagnostics = new List<SmoDiagnostic>();
        ValidateDeclaredSizes(header, span.Length, diagnostics);

        if (header.DataStart < SmoHeader.Size + sizeof(uint))
        {
            throw new SmoFormatException(
                $"DataStart 0x{header.DataStart:X8} leaves no room for the " +
                "object table and its four-byte terminator.");
        }

        if (header.DataStart > span.Length)
        {
            throw new SmoFormatException(
                $"DataStart 0x{header.DataStart:X8} is outside the " +
                $"{span.Length}-byte file.");
        }

        int dataStart = checked((int)header.DataStart);
        int terminatorOffset = dataStart - sizeof(uint);
        int tableCapacity = terminatorOffset - SmoHeader.ObjectTableOffset;
        uint maximumObjectCount = (uint)(tableCapacity / MinimumObjectEntrySize);
        if (header.ObjectCount > maximumObjectCount)
        {
            throw new SmoFormatException(
                $"The table region can contain at most {maximumObjectCount} " +
                $"minimal entries, but the header declares {header.ObjectCount}.");
        }

        var objects = new List<SmoObjectEntry>(checked((int)header.ObjectCount));
        int offset = SmoHeader.ObjectTableOffset;
        for (int index = 0; index < header.ObjectCount; index++)
        {
            int entryOffset = offset;
            uint id = ReadTableUInt32(span, ref offset, terminatorOffset, index, "object ID");
            ushort nameLength = ReadTableUInt16(
                span, ref offset, terminatorOffset, index, "name length");
            int nameOffset = offset;
            RequireTableBytes(
                offset, nameLength, terminatorOffset, index, "object name");
            ReadOnlySpan<byte> rawName = span.Slice(offset, nameLength);
            offset += nameLength;
            string name = DecodeName(rawName, nameOffset, index, diagnostics);

            uint typeHash = ReadTableUInt32(
                span, ref offset, terminatorOffset, index, "class ID");
            uint logicalOffset = ReadTableUInt32(
                span, ref offset, terminatorOffset, index, "logical offset");
            uint serializedSize = ReadTableUInt32(
                span, ref offset, terminatorOffset, index, "serialized size");
            long physicalOffset = (long)header.DataStart + logicalOffset;

            objects.Add(new SmoObjectEntry(
                index,
                entryOffset,
                id,
                nameLength,
                name,
                memory.Slice(nameOffset, nameLength),
                typeHash,
                logicalOffset,
                serializedSize,
                physicalOffset));
        }

        if (offset != terminatorOffset)
        {
            throw new SmoFormatException(
                $"The object table ends at 0x{offset:X}, but the four-byte " +
                $"terminator starts at 0x{terminatorOffset:X}.");
        }

        if (!span.Slice(terminatorOffset, sizeof(uint)).SequenceEqual(stackalloc byte[4]))
        {
            throw new SmoFormatException(
                $"The object table terminator at 0x{terminatorOffset:X} is not zero.");
        }

        header.ObjectTableEnd = offset;
        header.TerminatorOffset = terminatorOffset;

        ValidateObjectEntries(span, header, objects, diagnostics);
        ValidateNestedIntervals(objects, diagnostics);

        return new SmoDocument(
            sourcePath,
            memory,
            header,
            new ReadOnlyCollection<SmoObjectEntry>(objects),
            new ReadOnlyCollection<SmoDiagnostic>(diagnostics));
    }

    private const int MinimumObjectEntrySize =
        sizeof(uint) + sizeof(ushort) + sizeof(uint) * 3;

    private static void ValidateDeclaredSizes(
        SmoHeader header,
        int actualLength,
        ICollection<SmoDiagnostic> diagnostics)
    {
        if (header.FileSize != actualLength)
        {
            diagnostics.Add(new SmoDiagnostic(
                SmoDiagnosticSeverity.Error,
                "FILE_SIZE_MISMATCH",
                $"The header declares {header.FileSize} bytes, but the file " +
                $"contains {actualLength} bytes.",
                0x0C));
        }

        ulong declaredDataEnd = header.DeclaredDataEnd;
        if (declaredDataEnd != header.FileSize)
        {
            diagnostics.Add(new SmoDiagnostic(
                SmoDiagnosticSeverity.Error,
                "DECLARED_DATA_RANGE_MISMATCH",
                $"DataStart + DataSize is {declaredDataEnd}, while FileSize is " +
                $"{header.FileSize}.",
                0x18));
        }

        if (declaredDataEnd != (ulong)actualLength)
        {
            diagnostics.Add(new SmoDiagnostic(
                SmoDiagnosticSeverity.Error,
                "DATA_SIZE_MISMATCH",
                $"The declared data section ends at 0x{declaredDataEnd:X}, " +
                $"while the file ends at 0x{actualLength:X}.",
                0x18));
        }
    }

    private static void ValidateObjectEntries(
        ReadOnlySpan<byte> data,
        SmoHeader header,
        IReadOnlyList<SmoObjectEntry> objects,
        ICollection<SmoDiagnostic> diagnostics)
    {
        uint previousLogicalOffset = 0;
        for (int index = 0; index < objects.Count; index++)
        {
            SmoObjectEntry entry = objects[index];
            if (index > 0 && entry.LogicalOffset < previousLogicalOffset)
            {
                diagnostics.Add(new SmoDiagnostic(
                    SmoDiagnosticSeverity.Error,
                    "NON_MONOTONIC_OBJECT_OFFSET",
                    $"Object {entry.Index} starts at logical offset " +
                    $"0x{entry.LogicalOffset:X}, before object {index - 1} at " +
                    $"0x{previousLogicalOffset:X}.",
                    entry.TableOffset,
                    entry.Index));
            }
            previousLogicalOffset = entry.LogicalOffset;

            ulong logicalEnd = entry.LogicalEnd;
            bool withinDeclaredData = logicalEnd <= header.DataSize;
            bool withinFile =
                entry.PhysicalOffset >= header.DataStart &&
                entry.PhysicalOffset <= data.Length &&
                entry.PhysicalEnd >= entry.PhysicalOffset &&
                entry.PhysicalEnd <= data.Length;
            entry.IsWithinDataSection = withinDeclaredData && withinFile;

            if (!withinDeclaredData)
            {
                diagnostics.Add(new SmoDiagnostic(
                    SmoDiagnosticSeverity.Error,
                    "OBJECT_OUTSIDE_DECLARED_DATA",
                    $"Object {entry.Index} occupies logical interval " +
                    $"[0x{entry.LogicalOffset:X}, 0x{logicalEnd:X}), outside " +
                    $"the declared data size 0x{header.DataSize:X}.",
                    entry.TableOffset,
                    entry.Index));
            }

            if (!withinFile)
            {
                diagnostics.Add(new SmoDiagnostic(
                    SmoDiagnosticSeverity.Error,
                    "OBJECT_OUTSIDE_FILE",
                    $"Object {entry.Index} occupies physical interval " +
                    $"[0x{entry.PhysicalOffset:X}, 0x{entry.PhysicalEnd:X}), " +
                    $"outside the {data.Length}-byte file.",
                    entry.TableOffset,
                    entry.Index));
            }

            if (entry.SerializedSize < 8)
            {
                diagnostics.Add(new SmoDiagnostic(
                    SmoDiagnosticSeverity.Error,
                    "OBJECT_TOO_SMALL",
                    $"Object {entry.Index} is only {entry.SerializedSize} bytes " +
                    "long and cannot contain a class ID plus SBOO.",
                    entry.TableOffset,
                    entry.Index));
            }

            ValidateObjectSignature(data, entry, diagnostics);
        }
    }

    private static void ValidateObjectSignature(
        ReadOnlySpan<byte> data,
        SmoObjectEntry entry,
        ICollection<SmoDiagnostic> diagnostics)
    {
        const int signatureSize = sizeof(uint) + 4;
        if (entry.PhysicalOffset < 0 ||
            entry.PhysicalOffset > data.Length - signatureSize)
        {
            diagnostics.Add(new SmoDiagnostic(
                SmoDiagnosticSeverity.Warning,
                "OBJECT_SIGNATURE_UNAVAILABLE",
                $"The signature for object {entry.Index} cannot be read at " +
                $"physical offset 0x{entry.PhysicalOffset:X}.",
                entry.PhysicalOffset,
                entry.Index));
            return;
        }

        int physicalOffset = (int)entry.PhysicalOffset;
        uint actualClassId = ReadUInt32(data, physicalOffset);
        bool tagMatches = data.Slice(physicalOffset + sizeof(uint), 4)
            .SequenceEqual(ObjectTag);
        entry.SignatureMatches =
            actualClassId == entry.TypeHash && tagMatches;
        if (entry.SignatureMatches)
            return;

        string actualTag = Encoding.ASCII.GetString(
            data.Slice(physicalOffset + sizeof(uint), 4));
        diagnostics.Add(new SmoDiagnostic(
            SmoDiagnosticSeverity.Warning,
            "OBJECT_SIGNATURE_MISMATCH",
            $"Object {entry.Index} expects class ID 0x{entry.TypeHash:X8} " +
            $"followed by SBOO, but its physical offset contains class ID " +
            $"0x{actualClassId:X8} followed by {EscapeTag(actualTag)}. Logical " +
            "offsets in modified SMO files may be stale.",
            entry.PhysicalOffset,
            entry.Index));
    }

    private static void ValidateNestedIntervals(
        IReadOnlyList<SmoObjectEntry> objects,
        ICollection<SmoDiagnostic> diagnostics)
    {
        SmoObjectEntry[] sorted = objects
            .Where(entry => entry.IsWithinDataSection && entry.SerializedSize > 0)
            .OrderBy(entry => entry.LogicalOffset)
            .ThenByDescending(entry => entry.LogicalEnd)
            .ThenBy(entry => entry.Index)
            .ToArray();
        var stack = new List<SmoObjectEntry>();

        foreach (SmoObjectEntry entry in sorted)
        {
            while (stack.Count > 0 &&
                   entry.LogicalOffset >= stack[^1].LogicalEnd)
            {
                stack.RemoveAt(stack.Count - 1);
            }

            if (stack.Count > 0 && entry.LogicalEnd > stack[^1].LogicalEnd)
            {
                SmoObjectEntry conflicting = stack[^1];
                diagnostics.Add(new SmoDiagnostic(
                    SmoDiagnosticSeverity.Error,
                    "PARTIALLY_OVERLAPPING_OBJECTS",
                    $"Object {entry.Index} interval " +
                    $"[0x{entry.LogicalOffset:X}, 0x{entry.LogicalEnd:X}) " +
                    $"partially overlaps object {conflicting.Index} interval " +
                    $"[0x{conflicting.LogicalOffset:X}, " +
                    $"0x{conflicting.LogicalEnd:X}); object intervals may be " +
                    "disjoint or nested, but must not cross.",
                    entry.TableOffset,
                    entry.Index));

                while (stack.Count > 0 &&
                       entry.LogicalEnd > stack[^1].LogicalEnd)
                {
                    stack.RemoveAt(stack.Count - 1);
                }
            }

            if (stack.Count > 0)
            {
                entry.ParentIndex = stack[^1].Index;
                entry.NestingDepth = stack[^1].NestingDepth + 1;
            }

            stack.Add(entry);
        }
    }

    private static string DecodeName(
        ReadOnlySpan<byte> rawName,
        int nameOffset,
        int objectIndex,
        ICollection<SmoDiagnostic> diagnostics)
    {
        if (rawName.IsEmpty)
            return string.Empty;

        if (rawName[^1] != 0)
        {
            throw new SmoFormatException(
                $"Object {objectIndex} has a {rawName.Length}-byte name that " +
                "is not terminated by NUL.");
        }

        ReadOnlySpan<byte> text = rawName[..^1];
        int embeddedNul = text.IndexOf((byte)0);
        if (embeddedNul >= 0)
        {
            diagnostics.Add(new SmoDiagnostic(
                SmoDiagnosticSeverity.Warning,
                "EMBEDDED_NUL_IN_NAME",
                $"Object {objectIndex} has an embedded NUL in its stored name.",
                nameOffset + embeddedNul,
                objectIndex));
            text = text[..embeddedNul];
        }

        try
        {
            return StrictUtf8.GetString(text);
        }
        catch (DecoderFallbackException)
        {
            diagnostics.Add(new SmoDiagnostic(
                SmoDiagnosticSeverity.Warning,
                "NON_UTF8_OBJECT_NAME",
                $"Object {objectIndex} name is not UTF-8; it was decoded " +
                "losslessly as Latin-1.",
                nameOffset,
                objectIndex));
            return Encoding.Latin1.GetString(text);
        }
    }

    private static ushort ReadTableUInt16(
        ReadOnlySpan<byte> data,
        ref int offset,
        int limit,
        int objectIndex,
        string fieldName)
    {
        RequireTableBytes(offset, sizeof(ushort), limit, objectIndex, fieldName);
        ushort value = BinaryPrimitives.ReadUInt16LittleEndian(
            data.Slice(offset, sizeof(ushort)));
        offset += sizeof(ushort);
        return value;
    }

    private static uint ReadTableUInt32(
        ReadOnlySpan<byte> data,
        ref int offset,
        int limit,
        int objectIndex,
        string fieldName)
    {
        RequireTableBytes(offset, sizeof(uint), limit, objectIndex, fieldName);
        uint value = ReadUInt32(data, offset);
        offset += sizeof(uint);
        return value;
    }

    private static void RequireTableBytes(
        int offset,
        int length,
        int limit,
        int objectIndex,
        string fieldName)
    {
        if (offset < SmoHeader.ObjectTableOffset ||
            length < 0 ||
            offset > limit - length)
        {
            throw new SmoFormatException(
                $"Object table entry {objectIndex} is truncated while reading " +
                $"{fieldName} at 0x{offset:X}.");
        }
    }

    private static uint ReadUInt32(ReadOnlySpan<byte> data, int offset) =>
        BinaryPrimitives.ReadUInt32LittleEndian(data.Slice(offset, sizeof(uint)));

    private static string EscapeTag(string value)
    {
        var result = new StringBuilder(value.Length + 2);
        result.Append('"');
        foreach (char character in value)
        {
            if (character is >= ' ' and <= '~')
                result.Append(character);
            else
                result.Append('.');
        }
        result.Append('"');
        return result.ToString();
    }
}
