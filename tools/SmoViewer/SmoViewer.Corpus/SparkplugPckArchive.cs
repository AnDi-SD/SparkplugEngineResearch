using System.Buffers.Binary;
using System.Text;

namespace SmoViewer.Corpus;

public sealed record SparkplugPckEntry(
    int Index,
    string DirectoryPath,
    string FileName,
    string LogicalPath,
    uint NameOffset,
    uint SectorOffset,
    uint ByteOffset,
    uint ByteSize,
    uint NextDirectoryOffset);

public sealed class SparkplugPckArchive
{
    private const int SectorSize = 0x800;
    private const int IndexHeaderSize = 8;
    private const int IndexEntrySize = 20;

    private SparkplugPckArchive(
        string path,
        uint stringTableSize,
        IReadOnlyList<SparkplugPckEntry> entries)
    {
        Path = path;
        StringTableSize = stringTableSize;
        Entries = entries;
    }

    public string Path { get; }
    public uint StringTableSize { get; }
    public IReadOnlyList<SparkplugPckEntry> Entries { get; }

    public static SparkplugPckArchive Load(string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        string fullPath = System.IO.Path.GetFullPath(path);
        byte[] header;
        using (var stream = new FileStream(
                   fullPath, FileMode.Open, FileAccess.Read, FileShare.Read))
        {
            if (stream.Length < sizeof(uint) + IndexHeaderSize)
                throw new InvalidDataException($"PCK is too small: {fullPath}");
            header = new byte[sizeof(uint)];
            stream.ReadExactly(header);
            uint stringTableSize = BinaryPrimitives.ReadUInt32LittleEndian(header);
            long indexOffset = sizeof(uint) + (long)stringTableSize;
            if (indexOffset > stream.Length - IndexHeaderSize)
            {
                throw new InvalidDataException(
                    $"PCK string table ends outside the archive: {fullPath}");
            }

            byte[] stringTable = new byte[checked((int)stringTableSize)];
            stream.ReadExactly(stringTable);
            Span<byte> indexHeader = stackalloc byte[IndexHeaderSize];
            stream.ReadExactly(indexHeader);
            uint fileCount = BinaryPrimitives.ReadUInt32LittleEndian(indexHeader);
            long indexBytes = checked((long)fileCount * IndexEntrySize);
            if (indexBytes > stream.Length - stream.Position)
            {
                throw new InvalidDataException(
                    $"PCK declares {fileCount} entries outside the archive: {fullPath}");
            }

            var entries = new List<SparkplugPckEntry>(checked((int)fileCount));
            uint directoryOffset = 0;
            byte[] row = new byte[IndexEntrySize];
            for (int index = 0; index < fileCount; index++)
            {
                stream.ReadExactly(row);
                uint nameOffset = ReadUInt32(row, 0);
                uint sectorOffset = ReadUInt32(row, 4);
                uint byteOffset = ReadUInt32(row, 8);
                uint byteSize = ReadUInt32(row, 12);
                uint nextDirectoryOffset = ReadUInt32(row, 16);
                if ((long)sectorOffset * SectorSize != byteOffset)
                {
                    throw new InvalidDataException(
                        $"PCK entry {index} sector/byte offsets disagree in {fullPath}.");
                }
                if ((ulong)byteOffset + byteSize > (ulong)stream.Length)
                {
                    throw new InvalidDataException(
                        $"PCK entry {index} ends outside {fullPath}.");
                }

                string directory = ReadString(stringTable, directoryOffset, fullPath);
                string fileName = ReadString(stringTable, nameOffset, fullPath);
                string logicalPath = NormalizeLogicalPath(directory, fileName);
                entries.Add(new SparkplugPckEntry(
                    index,
                    directory,
                    fileName,
                    logicalPath,
                    nameOffset,
                    sectorOffset,
                    byteOffset,
                    byteSize,
                    nextDirectoryOffset));
                directoryOffset = nextDirectoryOffset;
            }
            return new SparkplugPckArchive(fullPath, stringTableSize, entries.AsReadOnly());
        }
    }

    public static byte[] ReadEntry(
        FileStream stream,
        SparkplugPckEntry entry)
    {
        ArgumentNullException.ThrowIfNull(stream);
        ArgumentNullException.ThrowIfNull(entry);
        if (!stream.CanSeek || !stream.CanRead)
            throw new ArgumentException("PCK stream must be readable and seekable.");
        if ((ulong)entry.ByteOffset + entry.ByteSize > (ulong)stream.Length)
            throw new InvalidDataException("PCK entry is outside the supplied stream.");
        stream.Position = entry.ByteOffset;
        byte[] data = new byte[checked((int)entry.ByteSize)];
        stream.ReadExactly(data);
        return data;
    }

    private static uint ReadUInt32(ReadOnlySpan<byte> data, int offset) =>
        BinaryPrimitives.ReadUInt32LittleEndian(data.Slice(offset, sizeof(uint)));

    private static string ReadString(
        ReadOnlySpan<byte> stringTable,
        uint offset,
        string archivePath)
    {
        if (offset >= stringTable.Length)
        {
            throw new InvalidDataException(
                $"PCK string offset 0x{offset:X} is outside {archivePath}.");
        }
        ReadOnlySpan<byte> remainder = stringTable[checked((int)offset)..];
        int terminator = remainder.IndexOf((byte)0);
        if (terminator < 0)
            throw new InvalidDataException($"PCK string is not terminated in {archivePath}.");
        return Encoding.Latin1.GetString(remainder[..terminator]);
    }

    private static string NormalizeLogicalPath(string directory, string fileName)
    {
        string path = string.IsNullOrEmpty(directory)
            ? fileName
            : $"{directory}/{fileName}";
        return path.Replace('\\', '/').TrimStart('/');
    }
}
