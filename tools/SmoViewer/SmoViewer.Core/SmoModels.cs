namespace SmoViewer.Core;

/// <summary>
/// Describes how strongly a parser diagnostic should be treated.
/// </summary>
public enum SmoDiagnosticSeverity
{
    Info,
    Warning,
    Error
}

/// <summary>
/// A non-fatal consistency issue discovered while parsing an SMO document.
/// </summary>
public sealed record SmoDiagnostic(
    SmoDiagnosticSeverity Severity,
    string Code,
    string Message,
    long? Offset = null,
    int? ObjectIndex = null);

/// <summary>
/// Result of the reconstructed header validator before runtime resource loading.
/// </summary>
public enum SmoHeaderValidationStatus : uint
{
    Valid, HeaderReadFailed, StreamSizeUnavailable, WrongFileType, WrongVersion,
    UnsupportedPlatform, DataOffsetBeyondEnd
}

/// <summary>Application metadata for the seven-word FFPS header and following FAT count.</summary>
public sealed class SmoHeader
{
    public const int Size = 0x20;
    public const int ObjectTableOffset = Size;

    internal SmoHeader(
        string signature,
        uint unknown04,
        uint unknown08,
        uint fileSize,
        uint version,
        uint dataStart,
        uint dataSize,
        uint objectCount)
    {
        Signature = signature;
        SerializerVersion = unknown04;
        Unknown08 = unknown08;
        FileSize = fileSize;
        PlatformMask = version;
        DataStart = dataStart;
        DataSize = dataSize;
        ObjectCount = objectCount;
    }

    public string Signature { get; }
    /// <summary>Original header validator result; raw inspection is not a runtime load.</summary>
    public SmoHeaderValidationStatus NativeValidationStatus { get; internal set; }
    public uint SerializerVersion { get; }

    public uint Unknown08 { get; }

    /// <summary>The complete file size declared at header offset 0x0C.</summary>
    public uint FileSize { get; }

    /// <summary>An alias that makes the declared nature of <see cref="FileSize"/> explicit.</summary>
    public uint DeclaredFileSize => FileSize;

    public uint PlatformMask { get; }

    /// <summary>The physical offset of the first serialized object.</summary>
    public uint DataStart { get; }

    public uint DataStartOffset => DataStart;

    /// <summary>The size of the serialized data section declared in the header.</summary>
    public uint DataSize { get; }

    public uint DeclaredDataSize => DataSize;
    public uint ObjectCount { get; }

    /// <summary>The first byte after the variable-length object table.</summary>
    public int ObjectTableEnd { get; internal set; }

    /// <summary>The offset of the four-byte zero terminator before the data section.</summary>
    public int TerminatorOffset { get; internal set; }

    public ulong DeclaredDataEnd => (ulong)DataStart + DataSize;
}

/// <summary>
/// An entry in the variable-length FFPS object directory.
/// </summary>
public sealed class SmoObjectEntry
{
    internal SmoObjectEntry(
        int index,
        int tableOffset,
        uint id,
        ushort nameLength,
        string name,
        ReadOnlyMemory<byte> rawName,
        uint typeHash,
        uint logicalOffset,
        uint serializedSize,
        long physicalOffset)
    {
        Index = index;
        TableOffset = tableOffset;
        Id = id;
        NameLength = nameLength;
        Name = name;
        RawName = rawName;
        TypeHash = typeHash;
        LogicalOffset = logicalOffset;
        SerializedSize = serializedSize;
        PhysicalOffset = physicalOffset;
        ClassName = SmoClassRegistry.TryGetName(typeHash, out string? className)
            ? className
            : null;
    }

    public int Index { get; }
    public int TableOffset { get; }
    public uint Id { get; }

    /// <summary>
    /// The stored byte count, including the trailing NUL when it is non-zero.
    /// </summary>
    public ushort NameLength { get; }

    public string Name { get; }

    /// <summary>The exact stored name bytes, including a trailing NUL when present.</summary>
    public ReadOnlyMemory<byte> RawName { get; }

    public uint TypeHash { get; }
    public string? ClassName { get; }
    public uint LogicalOffset { get; }
    public uint SerializedSize { get; }

    /// <summary>
    /// The physical object address: <c>Header.DataStart + LogicalOffset</c>.
    /// </summary>
    public long PhysicalOffset { get; }

    public ulong LogicalEnd => (ulong)LogicalOffset + SerializedSize;
    public long PhysicalEnd => PhysicalOffset + SerializedSize;

    /// <summary>The closest containing object, if the intervals form a valid tree.</summary>
    public int? ParentIndex { get; internal set; }

    public int NestingDepth { get; internal set; }
    public bool IsWithinDataSection { get; internal set; }
    public bool SignatureMatches { get; internal set; }
}
