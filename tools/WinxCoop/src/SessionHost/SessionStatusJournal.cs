using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace WinxCoop.SessionHost;

// Local advisory UI status only. This format never supplies a game receipt,
// native ACK, transition permission, process identity or network message.
// Each owner serializes its calls; readers retain partial records between pumps.
internal static class SessionStatusJournal
{
    internal const string FileName = "worker-status.jsonl";
    internal const int MaxJournalBytes = 8 * 1024 * 1024;
    internal const int MaxRecordBytes = 65536; // Includes the terminating LF.
    internal const int MaxReadBytesPerPump = 256 * 1024;
    private static readonly UTF8Encoding StrictUtf8 = new(false, true);
    private static readonly JsonSerializerOptions Json = new(SessionConfiguration.Json)
    { WriteIndented = false, MaxDepth = 32 };
    private static readonly JsonDocumentOptions Document = new() { MaxDepth = 32 };

    private static string JournalPath(string directory)
    {
        string root = SessionConfiguration.AbsoluteAscii(directory);
        RuntimeBundle.Ordinary(root, true);
        return Path.Combine(root, FileName);
    }

    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);

    // Serialize a whole bounded record before touching the retained journal.
    // An oversized status cannot leave a partial record in the file.
    private sealed class RecordBuffer : MemoryStream
    {
        private void Room(int count) => Require(count >= 0 && Position <= MaxRecordBytes - 1L - count,
            "Local status record exceeds its64KiB bound including LF.");
        public override void Write(byte[] buffer, int offset, int count)
        { Room(count); base.Write(buffer, offset, count); }
        public override void Write(ReadOnlySpan<byte> buffer)
        { Room(buffer.Length); base.Write(buffer); }
        public override void WriteByte(byte value) { Room(1); base.WriteByte(value); }
    }

    internal sealed class Writer : IDisposable
    {
        private readonly FileStream _file;
        private ulong _sequence;
        private long _bytes;
        private bool _failed, _disposed;
        internal ulong RecordsWritten => _sequence;
        internal long BytesWritten => _bytes;
        internal bool Failed => _failed;
        internal bool Closed => _disposed;

        internal Writer(string directory)
        {
            string path = JournalPath(directory);
            _file = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            try { RuntimeBundle.Ordinary(path, false); }
            catch { _file.Dispose(); throw; }
        }

        internal void Write(object status)
        {
            if (_disposed) throw new ObjectDisposedException(nameof(Writer));
            Require(!_failed, "Local status writer is terminal after a failed append.");
            Require(status is not null, "Local status must be an object.");
            ulong next = checked(_sequence + 1);
            byte[] record;
            using (var buffer = new RecordBuffer())
            {
                JsonSerializer.Serialize(buffer, new { version = 1, sequence = next, status }, Json);
                record = buffer.ToArray();
            }
            using (JsonDocument document = JsonDocument.Parse(record, Document))
                Require(document.RootElement.GetProperty("status").ValueKind == JsonValueKind.Object,
                    "Local status must serialize as an object.");
            Require(record.Length + 1 <= MaxRecordBytes && _bytes <= MaxJournalBytes - record.Length - 1L,
                "Local status journal exceeded its8MiB/64KiB bounds.");
            Require(_file.Position == _bytes && _file.Length == _bytes, "Retained local status journal changed length.");
            try
            {
                _file.Write(record, 0, record.Length); _file.WriteByte((byte)'\n');
                // Visibility to the local reader, without durable disk sync on
                // the native owner's periodic pump. Receipts use another file.
                _file.Flush(flushToDisk: false);
                _bytes = checked(_bytes + record.Length + 1); _sequence = next;
            }
            catch { _failed = true; throw; }
        }

        public void Dispose()
        {
            if (_disposed) return;
            try { _file.Dispose(); }
            finally { _disposed = true; }
        }
    }

    internal sealed class Reader : IDisposable
    {
        private readonly string _path;
        private readonly byte[] _read = new byte[8192], _partial = new byte[MaxRecordBytes - 1];
        private FileStream? _file;
        private Exception? _failure;
        private long _observedLength, _bytesRead;
        private int _partialBytes;
        private ulong _sequence;
        private bool _disposed, _hasUnreadBytes;
        internal ulong LastSequence => _sequence;
        internal long BytesRead => _bytesRead;
        internal int PartialBytes => _partialBytes;
        internal bool HasUnreadBytes => _hasUnreadBytes;
        internal bool FinalComplete { get; private set; }

        internal Reader(string directory) => _path = JournalPath(directory);

        // Returns the newest NEW complete status in this bounded pump, or null.
        // final=true comes only from the retained native parent's actual exit.
        internal JsonObject? ReadLatest(bool final = false)
        {
            if (_disposed) throw new ObjectDisposedException(nameof(Reader));
            if (_failure is not null) throw new InvalidDataException("Local status reader is terminal after a rejected journal.", _failure);
            try
            {
                if (_file is null)
                {
                    try
                    {
                        var opened = new FileStream(_path, FileMode.Open, FileAccess.Read,
                            FileShare.ReadWrite | FileShare.Delete);
                        try { RuntimeBundle.Ordinary(_path, false); _file = opened; }
                        catch { opened.Dispose(); throw; }
                    }
                    catch (FileNotFoundException)
                    {
                        // A failed worker may have exited before constructing
                        // its writer. Actual native cleanup is checked apart.
                        FinalComplete = final; _hasUnreadBytes = false; return null;
                    }
                }
                FileStream input = _file ?? throw new InvalidDataException("Local status journal reader did not retain its file.");
                VerifyLength();
                int budget = MaxReadBytesPerPump;
                JsonObject? latest = null;
                while (budget > 0)
                {
                    int count = input.Read(_read, 0, Math.Min(_read.Length, budget));
                    if (count == 0) break;
                    _bytesRead = checked(_bytesRead + count); budget -= count;
                    for (int index = 0; index < count; ++index)
                    {
                        byte value = _read[index];
                        if (value == (byte)'\n')
                        {
                            Require(_partialBytes != 0, "Empty local status record.");
                            latest = ParseRecord(); _partialBytes = 0;
                        }
                        else
                        {
                            Require(_partialBytes < _partial.Length, "Local status record exceeds its64KiB bound including LF.");
                            _partial[_partialBytes++] = value;
                        }
                    }
                }
                long length = VerifyLength();
                _hasUnreadBytes = _bytesRead < length;
                bool finalEnd = final && !_hasUnreadBytes;
                if (finalEnd) Require(_partialBytes == 0, "Final local status journal has an unterminated record.");
                FinalComplete = finalEnd;
                return latest;
            }
            catch (Exception error) when (error is InvalidDataException or IOException or UnauthorizedAccessException or JsonException or DecoderFallbackException)
            { _failure = error; throw; }
        }

        private long VerifyLength()
        {
            long length = _file!.Length;
            Require(length >= _observedLength && length >= _bytesRead && length <= MaxJournalBytes,
                "Local status journal was truncated or exceeded its8MiB bound.");
            _observedLength = length;
            return length;
        }

        private JsonObject ParseRecord()
        {
            // Do not decode a split UTF8 codepoint until its complete LF-framed
            // record is available. Invalid complete UTF8 is rejected strictly.
            string text = StrictUtf8.GetString(_partial, 0, _partialBytes);
            using JsonDocument document = JsonDocument.Parse(text, Document);
            JsonElement root = document.RootElement, status = default;
            Require(root.ValueKind == JsonValueKind.Object, "Local status wrapper must be an object.");
            bool versionSeen = false, sequenceSeen = false, statusSeen = false;
            ulong next = 0;
            foreach (JsonProperty property in root.EnumerateObject())
            {
                switch (property.Name)
                {
                    case "version":
                        Require(!versionSeen && property.Value.ValueKind == JsonValueKind.Number &&
                            property.Value.TryGetUInt32(out uint version) && version == 1, "Invalid/repeated local status version.");
                        versionSeen = true; break;
                    case "sequence":
                        Require(!sequenceSeen && property.Value.ValueKind == JsonValueKind.Number &&
                            property.Value.TryGetUInt64(out next), "Invalid/repeated local status sequence.");
                        sequenceSeen = true; break;
                    case "status":
                        Require(!statusSeen && property.Value.ValueKind == JsonValueKind.Object, "Invalid/repeated local status object.");
                        statusSeen = true; status = property.Value; break;
                    default: throw new InvalidDataException("Unknown local status wrapper field.");
                }
            }
            Require(versionSeen && sequenceSeen && statusSeen && next == checked(_sequence + 1),
                "Local status sequence must be contiguous from1.");
            JsonObject result = JsonNode.Parse(status.GetRawText())!.AsObject();
            _sequence = next;
            return result;
        }

        public void Dispose()
        {
            if (_disposed) return;
            try { _file?.Dispose(); }
            finally { _disposed = true; }
        }
    }
}
