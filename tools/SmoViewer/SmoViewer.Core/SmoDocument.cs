using System.Collections.ObjectModel;
using System.Text;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>
/// A parsed FFPS/SMO container. Parsing preserves the complete source bytes and
/// keeps recoverable consistency problems in <see cref="Diagnostics"/>.
/// </summary>
public sealed class SmoDocument
{
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

    /// <summary>
    /// Parses a buffer that is exclusively owned by the caller and transfers
    /// that ownership to the returned document without cloning the container.
    /// The array must not be modified while the document is in use.
    /// </summary>
    public static unsafe SmoDocument ParseOwned(byte[] data, string? sourcePath = null)
    {
        ArgumentNullException.ThrowIfNull(data);
        ReadOnlyMemory<byte> memory = data;
        ReadOnlySpan<byte> span = data;
        NativeMethods.ContainerInfo nativeHeader;
        NativeMethods.ContainerEntry[] nativeEntries;
        try
        {
            // One borrowed input call, then a batch metadata copy. The native
            // reader invokes shared header/FAT routines without inventing
            // factories for classes being inspected. No retained input pointer.
            fixed (byte* input = data)
            {
                using var container = new ContainerHandle(NativeMethods.Check(
                    NativeMethods.spv_container_inspect(input, checked((uint)data.Length))));
                NativeMethods.Check(NativeMethods.spv_container_info(container, out nativeHeader));
                nativeEntries = new NativeMethods.ContainerEntry[checked((int)nativeHeader.ObjectCount)];
                fixed (NativeMethods.ContainerEntry* output = nativeEntries)
                    NativeMethods.Check(NativeMethods.spv_container_entries(container, output, nativeHeader.ObjectCount));
            }
        }
        catch (InvalidDataException exception)
        {
            throw new SmoFormatException(exception.Message, exception);
        }

        var header = new SmoHeader("FFPS", nativeHeader.Version, nativeHeader.ExportTag,
            nativeHeader.FileSize, nativeHeader.PlatformMask, nativeHeader.DataOffset,
            nativeHeader.DataSize, nativeHeader.ObjectCount)
        {
            NativeValidationStatus = (SmoHeaderValidationStatus)nativeHeader.HeaderStatus,
            ObjectTableEnd = checked((int)nativeHeader.DataOffset - sizeof(uint)),
            TerminatorOffset = checked((int)nativeHeader.DataOffset - sizeof(uint))
        };
        var diagnostics = new List<SmoDiagnostic>();
        ValidateDeclaredSizes(header, span.Length, diagnostics);
        var objects = new List<SmoObjectEntry>(nativeEntries.Length);
        for (int index = 0; index < nativeEntries.Length; ++index)
        {
            var entry = nativeEntries[index];
            int nameOffset = checked((int)entry.NameOffset);
            ushort nameLength = checked((ushort)entry.NameBytes);
            var rawName = memory.Slice(nameOffset, nameLength);
            string name = DecodeName(rawName.Span, nameOffset, index, diagnostics);
            objects.Add(new SmoObjectEntry(index, checked((int)entry.TableOffset), entry.Id,
                nameLength, name, rawName, entry.ClassId, entry.Offset, entry.Size,
                (long)header.DataStart + entry.Offset));
        }

        ValidateObjectEntries(span, header, objects, nativeEntries, diagnostics);
        ValidateNestedIntervals(objects, diagnostics);

        return new SmoDocument(
            sourcePath,
            memory,
            header,
            new ReadOnlyCollection<SmoObjectEntry>(objects),
            new ReadOnlyCollection<SmoDiagnostic>(diagnostics));
    }

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
        IReadOnlyList<NativeMethods.ContainerEntry> nativeEntries,
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

            ValidateObjectSignature(data, entry, nativeEntries[index], diagnostics);
        }
    }

    private static void ValidateObjectSignature(
        ReadOnlySpan<byte> data,
        SmoObjectEntry entry,
        NativeMethods.ContainerEntry native,
        ICollection<SmoDiagnostic> diagnostics)
    {
        if ((native.SignatureFlags & 1) == 0)
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
        uint actualClassId = native.SignatureClassId;
        entry.SignatureMatches = (native.SignatureFlags & 7) == 7;
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
