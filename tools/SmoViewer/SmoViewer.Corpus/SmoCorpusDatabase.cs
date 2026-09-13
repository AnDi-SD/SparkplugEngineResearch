using System.Diagnostics;
using System.Security.Cryptography;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static class SmoCorpusDatabase
{
    private const int PayloadPreviewBytes = 48;
    private const int PayloadHashLimit = 4096;
    private static int _sqliteInitialized;

    public static SmoCorpusUpdateResult Update(
        string databasePath,
        string sourcePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(databasePath);
        ArgumentException.ThrowIfNullOrWhiteSpace(sourcePath);
        EnsureSqliteInitialized();

        var stopwatch = Stopwatch.StartNew();
        string fullDatabasePath = Path.GetFullPath(databasePath);
        string fullSourcePath = Path.GetFullPath(sourcePath);
        bool singleFile = File.Exists(fullSourcePath);
        if (!singleFile && !Directory.Exists(fullSourcePath))
        {
            throw new DirectoryNotFoundException(
                $"SMO source file or directory not found: {fullSourcePath}");
        }
        if (singleFile && !Path.GetExtension(fullSourcePath).Equals(
                ".smo", StringComparison.OrdinalIgnoreCase))
        {
            throw new ArgumentException("The corpus source file must have the .smo extension.");
        }

        string sourceRoot = singleFile
            ? Path.GetDirectoryName(fullSourcePath)!
            : Path.TrimEndingDirectorySeparator(fullSourcePath);
        string[] files = singleFile
            ? [fullSourcePath]
            : Directory.EnumerateFiles(
                    sourceRoot, "*.smo", SearchOption.AllDirectories)
                .Order(StringComparer.OrdinalIgnoreCase)
                .ToArray();

        Directory.CreateDirectory(Path.GetDirectoryName(fullDatabasePath)!);
        using SqliteConnection connection = Open(fullDatabasePath, readOnly: false);
        ConfigureWriter(connection);
        SmoCorpusSchema.Initialize(connection);
        EnsureSourceRoot(connection, sourceRoot);

        int scanId;
        int scannedFiles = 0;
        int unchangedFiles = 0;
        int failedFiles = 0;
        int removedFiles = 0;
        string startedUtc = DateTime.UtcNow.ToString("O");
        using (SqliteTransaction transaction = connection.BeginTransaction())
        {
            scanId = InsertScan(
                connection, transaction, sourceRoot, files.Length, startedUtc);
            SeedClasses(connection, transaction);
            SeedFieldDefinitions(connection, transaction);
            Dictionary<string, ExistingFile> existing = LoadExistingFiles(
                connection, transaction);

            foreach (string file in files)
            {
                string relativePath = NormalizeRelativePath(
                    Path.GetRelativePath(sourceRoot, file));
                var info = new FileInfo(file);
                long modifiedTicks = info.LastWriteTimeUtc.Ticks;
                if (existing.TryGetValue(relativePath, out ExistingFile? current) &&
                    current.ByteSize == info.Length &&
                    current.LastWriteUtcTicks == modifiedTicks &&
                    current.ScannerRevision == SmoCorpusSchema.ScannerRevision &&
                    current.ParseStatus.Equals("ok", StringComparison.Ordinal))
                {
                    TouchFile(connection, transaction, current.Id, scanId);
                    unchangedFiles++;
                    continue;
                }

                try
                {
                    SmoDocument document = SmoDocument.Load(file);
                    int fileId = UpsertParsedFile(
                        connection,
                        transaction,
                        existing.TryGetValue(relativePath, out current) ? current.Id : null,
                        relativePath,
                        GetFamily(relativePath),
                        info,
                        Convert.ToHexString(SHA256.HashData(document.Data.Span)),
                        document,
                        scanId,
                        startedUtc);
                    DeleteIndexedObjects(connection, transaction, fileId);
                    InsertDocument(connection, transaction, fileId, document);
                    scannedFiles++;
                }
                catch (Exception exception) when (
                    exception is IOException or UnauthorizedAccessException or
                        SmoFormatException or OverflowException)
                {
                    int fileId = UpsertFailedFile(
                        connection,
                        transaction,
                        existing.TryGetValue(relativePath, out current) ? current.Id : null,
                        relativePath,
                        GetFamily(relativePath),
                        info,
                        exception.Message,
                        scanId,
                        startedUtc);
                    DeleteIndexedObjects(connection, transaction, fileId);
                    failedFiles++;
                }
            }

            if (!singleFile)
            {
                using SqliteCommand remove = CreateCommand(
                    connection,
                    transaction,
                    "DELETE FROM files WHERE last_scan_id <> $scan;");
                remove.Parameters.AddWithValue("$scan", scanId);
                removedFiles = remove.ExecuteNonQuery();
            }

            using (SqliteCommand finish = CreateCommand(
                       connection,
                       transaction,
                       """
                       UPDATE scans
                       SET finished_utc=$finished, scanned_files=$scanned,
                           unchanged_files=$unchanged, failed_files=$failed,
                           removed_files=$removed
                       WHERE id=$id;
                       """))
            {
                finish.Parameters.AddWithValue("$finished", DateTime.UtcNow.ToString("O"));
                finish.Parameters.AddWithValue("$scanned", scannedFiles);
                finish.Parameters.AddWithValue("$unchanged", unchangedFiles);
                finish.Parameters.AddWithValue("$failed", failedFiles);
                finish.Parameters.AddWithValue("$removed", removedFiles);
                finish.Parameters.AddWithValue("$id", scanId);
                finish.ExecuteNonQuery();
            }
            transaction.Commit();
        }

        using (SqliteCommand checkpoint = connection.CreateCommand())
        {
            checkpoint.CommandText = "PRAGMA wal_checkpoint(TRUNCATE);";
            checkpoint.ExecuteNonQuery();
        }
        SmoCorpusSummary summary = ReadSummary(connection, fullDatabasePath);
        stopwatch.Stop();
        return new SmoCorpusUpdateResult(
            fullDatabasePath,
            sourceRoot,
            files.Length,
            scannedFiles,
            unchangedFiles,
            failedFiles,
            removedFiles,
            summary.Objects,
            summary.DirectFields,
            summary.KnownClasses,
            summary.UnknownClasses,
            new FileInfo(fullDatabasePath).Length,
            stopwatch.Elapsed);
    }

    public static SmoCorpusSummary GetSummary(string databasePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(databasePath);
        EnsureSqliteInitialized();
        string fullPath = Path.GetFullPath(databasePath);
        if (!File.Exists(fullPath))
            throw new FileNotFoundException("SMO corpus database not found.", fullPath);
        using SqliteConnection connection = Open(fullPath, readOnly: true);
        return ReadSummary(connection, fullPath);
    }

    public static IReadOnlyList<SmoCorpusClassSummary> GetClasses(
        string databasePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(databasePath);
        EnsureSqliteInitialized();
        string fullPath = Path.GetFullPath(databasePath);
        if (!File.Exists(fullPath))
            throw new FileNotFoundException("SMO corpus database not found.", fullPath);
        using SqliteConnection connection = Open(fullPath, readOnly: true);
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT type_hash, engine_name, is_known, object_count,
                   file_count, family_count
            FROM class_totals
            WHERE object_count > 0
            ORDER BY object_count DESC, type_hash;
            """;
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new List<SmoCorpusClassSummary>();
        while (reader.Read())
        {
            result.Add(new SmoCorpusClassSummary(
                checked((uint)reader.GetInt64(0)),
                reader.IsDBNull(1) ? null : reader.GetString(1),
                reader.GetInt64(2) != 0,
                reader.GetInt64(3),
                reader.GetInt32(4),
                reader.GetInt32(5)));
        }
        return result;
    }

    public static IReadOnlyList<SmoCorpusClassMetrics> GetClassMetrics(
        string databasePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(databasePath);
        EnsureSqliteInitialized();
        string fullPath = Path.GetFullPath(databasePath);
        if (!File.Exists(fullPath))
            throw new FileNotFoundException("SMO corpus database not found.", fullPath);
        using SqliteConnection connection = Open(fullPath, readOnly: true);
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT o.type_hash, c.engine_name,
                   COUNT(*) AS object_count,
                   COUNT(DISTINCT o.file_id) AS file_count,
                   COUNT(DISTINCT f.family) AS family_count,
                   COUNT(DISTINCT o.serialized_size) AS size_count,
                   COUNT(DISTINCT o.field_shape) AS shape_count,
                   COUNT(DISTINCT CAST(o.serialized_size AS TEXT) || char(31) ||
                         o.field_shape) AS structural_variant_count,
                   MIN(o.serialized_size), MAX(o.serialized_size),
                   AVG(o.serialized_size),
                   MIN(o.field_count), MAX(o.field_count), AVG(o.field_count),
                   SUM(CASE WHEN o.field_parse_error IS NULL THEN 0 ELSE 1 END)
            FROM objects o
            JOIN classes c ON c.type_hash=o.type_hash
            JOIN files f ON f.id=o.file_id
            GROUP BY o.type_hash
            ORDER BY object_count DESC, o.type_hash;
            """;
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new List<SmoCorpusClassMetrics>();
        while (reader.Read())
        {
            result.Add(new SmoCorpusClassMetrics(
                checked((uint)reader.GetInt64(0)),
                reader.IsDBNull(1) ? null : reader.GetString(1),
                reader.GetInt64(2),
                reader.GetInt32(3),
                reader.GetInt32(4),
                reader.GetInt32(5),
                reader.GetInt32(6),
                reader.GetInt32(7),
                reader.GetInt64(8),
                reader.GetInt64(9),
                reader.GetDouble(10),
                reader.GetInt32(11),
                reader.GetInt32(12),
                reader.GetDouble(13),
                reader.GetInt64(14)));
        }
        return result;
    }

    internal static void EnsureSqliteInitialized()
    {
        if (Interlocked.Exchange(ref _sqliteInitialized, 1) != 0)
            return;
        SQLitePCL.raw.SetProvider(new SQLitePCL.SQLite3Provider_winsqlite3());
        SQLitePCL.raw.FreezeProvider();
    }

    internal static SqliteConnection Open(string path, bool readOnly)
    {
        var builder = new SqliteConnectionStringBuilder
        {
            DataSource = path,
            Mode = readOnly ? SqliteOpenMode.ReadOnly : SqliteOpenMode.ReadWriteCreate,
            Cache = SqliteCacheMode.Private,
            Pooling = false
        };
        var connection = new SqliteConnection(builder.ToString());
        connection.Open();
        using SqliteCommand foreignKeys = connection.CreateCommand();
        foreignKeys.CommandText = "PRAGMA foreign_keys=ON;";
        foreignKeys.ExecuteNonQuery();
        return connection;
    }

    internal static void ConfigureWriter(SqliteConnection connection)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            PRAGMA journal_mode=WAL;
            PRAGMA synchronous=NORMAL;
            PRAGMA temp_store=MEMORY;
            """;
        command.ExecuteNonQuery();
    }

    private static void EnsureSourceRoot(
        SqliteConnection connection,
        string sourceRoot)
    {
        using SqliteCommand select = connection.CreateCommand();
        select.CommandText = "SELECT value FROM schema_info WHERE key='source_root';";
        string? stored = select.ExecuteScalar() as string;
        if (stored is not null && !Path.GetFullPath(stored).Equals(
                Path.GetFullPath(sourceRoot), StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                $"Database belongs to source root '{stored}', not '{sourceRoot}'.");
        }
        if (stored is not null)
            return;
        using SqliteCommand insert = connection.CreateCommand();
        insert.CommandText =
            "INSERT INTO schema_info(key,value) VALUES('source_root',$root);";
        insert.Parameters.AddWithValue("$root", sourceRoot);
        insert.ExecuteNonQuery();
    }

    private static int InsertScan(
        SqliteConnection connection,
        SqliteTransaction transaction,
        string sourceRoot,
        int fileCount,
        string startedUtc)
    {
        using SqliteCommand command = CreateCommand(
            connection,
            transaction,
            """
            INSERT INTO scans(
                started_utc, source_root, scanner_revision, discovered_files)
            VALUES($started, $root, $revision, $files);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$started", startedUtc);
        command.Parameters.AddWithValue("$root", sourceRoot);
        command.Parameters.AddWithValue("$revision", SmoCorpusSchema.ScannerRevision);
        command.Parameters.AddWithValue("$files", fileCount);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    internal static void SeedClasses(
        SqliteConnection connection,
        SqliteTransaction transaction)
    {
        using SqliteCommand command = CreateCommand(
            connection,
            transaction,
            """
            INSERT INTO classes(type_hash, engine_name, is_known)
            VALUES($hash, $name, 1)
            ON CONFLICT(type_hash) DO UPDATE SET
                engine_name=excluded.engine_name, is_known=1;
            """);
        var hash = command.Parameters.Add("$hash", SqliteType.Integer);
        var name = command.Parameters.Add("$name", SqliteType.Text);
        foreach ((uint typeHash, string className) in SmoClassRegistry.KnownClasses)
        {
            hash.Value = (long)typeHash;
            name.Value = className;
            command.ExecuteNonQuery();
        }
    }

    private static void SeedFieldDefinitions(
        SqliteConnection connection,
        SqliteTransaction transaction)
    {
        using SqliteCommand command = CreateCommand(
            connection,
            transaction,
            """
            INSERT INTO field_definitions(
                type_hash, section_from_end, field_type, occurrence,
                semantic_key, display_name, payload_layout,
                editable_status, evidence_status)
            VALUES($hash, 0, $type, -1, $key, $display, $layout,
                   'read_only_research', 'confirmed_executable_and_corpus')
            ON CONFLICT(type_hash, section_from_end, field_type, occurrence)
            DO UPDATE SET semantic_key=excluded.semantic_key,
                          display_name=excluded.display_name,
                          payload_layout=excluded.payload_layout,
                          evidence_status=excluded.evidence_status;
            """);
        var hash = command.Parameters.Add("$hash", SqliteType.Integer);
        var type = command.Parameters.Add("$type", SqliteType.Integer);
        var key = command.Parameters.Add("$key", SqliteType.Text);
        var display = command.Parameters.Add("$display", SqliteType.Text);
        var layout = command.Parameters.Add("$layout", SqliteType.Text);
        foreach (uint classId in SmoSerializedFieldRegistry.KnownClassIds)
        foreach (SmoSerializedFieldDescriptor descriptor in
                 SmoSerializedFieldRegistry.GetOwnFieldDefinitions(classId).Values)
        {
            hash.Value = (long)classId;
            type.Value = descriptor.FieldType;
            key.Value = descriptor.Key;
            display.Value = descriptor.DisplayName;
            layout.Value = descriptor.PayloadLayout;
            command.ExecuteNonQuery();
        }
    }

    private static Dictionary<string, ExistingFile> LoadExistingFiles(
        SqliteConnection connection,
        SqliteTransaction transaction)
    {
        using SqliteCommand command = CreateCommand(
            connection,
            transaction,
            """
            SELECT id, relative_path, byte_size, last_write_utc_ticks,
                   scanner_revision, parse_status
            FROM files;
            """);
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new Dictionary<string, ExistingFile>(
            StringComparer.OrdinalIgnoreCase);
        while (reader.Read())
        {
            string path = reader.GetString(1);
            result[path] = new ExistingFile(
                reader.GetInt32(0),
                reader.GetInt64(2),
                reader.GetInt64(3),
                reader.GetInt32(4),
                reader.GetString(5));
        }
        return result;
    }

    private static void TouchFile(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int fileId,
        int scanId)
    {
        using SqliteCommand command = CreateCommand(
            connection, transaction,
            "UPDATE files SET last_scan_id=$scan WHERE id=$id;");
        command.Parameters.AddWithValue("$scan", scanId);
        command.Parameters.AddWithValue("$id", fileId);
        command.ExecuteNonQuery();
    }

    private static int UpsertParsedFile(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int? fileId,
        string relativePath,
        string family,
        FileInfo info,
        string sha256,
        SmoDocument document,
        int scanId,
        string scannedUtc) =>
        UpsertFile(
            connection, transaction, fileId, relativePath, family, info,
            sha256, document.Header.SerializerVersion, document.Header.Unknown08,
            document.Header.PlatformMask, document.Header.DataStart,
            document.Header.DataSize, document.Objects.Count, "ok", null,
            scanId, scannedUtc);

    private static int UpsertFailedFile(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int? fileId,
        string relativePath,
        string family,
        FileInfo info,
        string error,
        int scanId,
        string scannedUtc) =>
        UpsertFile(
            connection, transaction, fileId, relativePath, family, info,
            null, null, null, null, null, null, null, "error", error, scanId, scannedUtc);

    private static int UpsertFile(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int? fileId,
        string relativePath,
        string family,
        FileInfo info,
        string? sha256,
        uint? serializerVersion,
        uint? unknown08,
        uint? platformMask,
        uint? dataStart,
        uint? dataSize,
        int? objectCount,
        string status,
        string? error,
        int scanId,
        string scannedUtc)
    {
        using SqliteCommand command = CreateCommand(
            connection,
            transaction,
            """
            INSERT INTO files(
                relative_path, file_name, family, byte_size,
                last_write_utc_ticks, sha256, serializer_version, ffps_unknown08,
                platform_mask, data_start,
                data_size, object_count, parse_status, parse_error,
                scanner_revision, last_scan_id, scanned_utc)
            VALUES($path, $name, $family, $bytes, $ticks, $sha,
                   $serializer, $unknown08, $platform_mask, $start,
                   $data, $objects, $status, $error,
                   $revision, $scan, $scanned)
            ON CONFLICT(relative_path) DO UPDATE SET
                file_name=excluded.file_name, family=excluded.family,
                byte_size=excluded.byte_size,
                last_write_utc_ticks=excluded.last_write_utc_ticks,
                sha256=excluded.sha256,
                serializer_version=excluded.serializer_version,
                ffps_unknown08=excluded.ffps_unknown08,
                platform_mask=excluded.platform_mask,
                data_start=excluded.data_start, data_size=excluded.data_size,
                object_count=excluded.object_count,
                parse_status=excluded.parse_status, parse_error=excluded.parse_error,
                scanner_revision=excluded.scanner_revision,
                last_scan_id=excluded.last_scan_id, scanned_utc=excluded.scanned_utc;
            """);
        command.Parameters.AddWithValue("$path", relativePath);
        command.Parameters.AddWithValue("$name", Path.GetFileName(relativePath));
        command.Parameters.AddWithValue("$family", family);
        command.Parameters.AddWithValue("$bytes", info.Length);
        command.Parameters.AddWithValue("$ticks", info.LastWriteTimeUtc.Ticks);
        AddNullable(command, "$sha", sha256);
        AddNullable(command, "$serializer", serializerVersion is uint sv ? (long)sv : null);
        AddNullable(command, "$unknown08", unknown08 is uint u ? (long)u : null);
        AddNullable(command, "$platform_mask", platformMask is uint pm ? (long)pm : null);
        AddNullable(command, "$start", dataStart is uint s ? (long)s : null);
        AddNullable(command, "$data", dataSize is uint d ? (long)d : null);
        AddNullable(command, "$objects", objectCount);
        command.Parameters.AddWithValue("$status", status);
        AddNullable(command, "$error", error);
        command.Parameters.AddWithValue("$revision", SmoCorpusSchema.ScannerRevision);
        command.Parameters.AddWithValue("$scan", scanId);
        command.Parameters.AddWithValue("$scanned", scannedUtc);
        command.ExecuteNonQuery();
        if (fileId.HasValue)
            return fileId.Value;
        using SqliteCommand select = CreateCommand(
            connection, transaction,
            "SELECT id FROM files WHERE relative_path=$path;");
        select.Parameters.AddWithValue("$path", relativePath);
        return Convert.ToInt32(select.ExecuteScalar());
    }

    internal static void DeleteIndexedObjects(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int fileId)
    {
        using SqliteCommand command = CreateCommand(
            connection, transaction,
            "DELETE FROM objects WHERE file_id=$file;");
        command.Parameters.AddWithValue("$file", fileId);
        command.ExecuteNonQuery();
    }

    internal static void InsertDocument(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int fileId,
        SmoDocument document,
        bool applyKnownFieldSemantics = true)
    {
        using SqliteCommand insertClass = CreateCommand(
            connection,
            transaction,
            """
            INSERT INTO classes(type_hash, engine_name, is_known)
            VALUES($hash, NULL, 0) ON CONFLICT(type_hash) DO NOTHING;
            """);
        var unknownHash = insertClass.Parameters.Add("$hash", SqliteType.Integer);

        using SqliteCommand insertObject = CreateObjectInsert(
            connection, transaction);
        using SqliteCommand insertField = CreateFieldInsert(
            connection, transaction);

        foreach (SmoObjectEntry entry in document.Objects)
        {
            if (!SmoClassRegistry.TryGetName(entry.TypeHash, out _))
            {
                unknownHash.Value = (long)entry.TypeHash;
                insertClass.ExecuteNonQuery();
            }

            bool fieldsRead = SmoObjectFieldReader.TryRead(
                document, entry, out IReadOnlyList<SmoObjectField>? fields,
                out string fieldError);
            fields = fieldsRead ? fields : Array.Empty<SmoObjectField>();
            string fieldShape = BuildFieldShape(fields);
            BindObject(
                insertObject, fileId, entry, fields.Count, fieldShape,
                fieldsRead ? null : fieldError);
            insertObject.ExecuteNonQuery();

            Dictionary<(int Type, int Occurrence), SmoSerializedFieldValue> knownValues =
                applyKnownFieldSemantics
                    ? SmoSerializedFieldInspector.Inspect(document, entry)
                        .ToDictionary(
                            item => (item.Descriptor.FieldType, item.Occurrence))
                    : [];
            int section = 0;
            for (int fieldIndex = 0; fieldIndex < fields.Count; fieldIndex++)
            {
                SmoObjectField field = fields[fieldIndex];
                bool terminator = field.FieldType == 0 && field.PayloadSize == 0;
                knownValues.TryGetValue(
                    (field.FieldType, field.Occurrence), out SmoSerializedFieldValue? known);
                BindField(
                    insertField, fileId, entry.Index, fieldIndex, section,
                    terminator, field, known);
                insertField.ExecuteNonQuery();
                if (terminator)
                    section++;
            }
        }
    }

    private static SqliteCommand CreateObjectInsert(
        SqliteConnection connection,
        SqliteTransaction transaction)
    {
        SqliteCommand command = CreateCommand(
            connection,
            transaction,
            """
            INSERT INTO objects(
                file_id, object_index, object_id, parent_index, name, type_hash,
                table_offset, logical_offset, physical_offset, serialized_size,
                signature_matches, within_data_section, field_count, field_shape,
                field_parse_error)
            VALUES($file, $index, $id, $parent, $name, $hash, $table, $logical,
                   $physical, $size, $signature, $within, $field_count,
                   $field_shape, $field_error);
            """);
        foreach (string name in new[]
                 {
                     "$file", "$index", "$id", "$parent", "$hash", "$table",
                     "$logical", "$physical", "$size", "$signature", "$within",
                     "$field_count"
                 })
            command.Parameters.Add(name, SqliteType.Integer);
        foreach (string name in new[] { "$name", "$field_shape", "$field_error" })
            command.Parameters.Add(name, SqliteType.Text);
        return command;
    }

    private static void BindObject(
        SqliteCommand command,
        int fileId,
        SmoObjectEntry entry,
        int fieldCount,
        string fieldShape,
        string? fieldError)
    {
        Set(command, "$file", fileId);
        Set(command, "$index", entry.Index);
        Set(command, "$id", (long)entry.Id);
        Set(command, "$parent", entry.ParentIndex);
        Set(command, "$name", entry.Name.TrimEnd('\0'));
        Set(command, "$hash", (long)entry.TypeHash);
        Set(command, "$table", entry.TableOffset);
        Set(command, "$logical", (long)entry.LogicalOffset);
        Set(command, "$physical", entry.PhysicalOffset);
        Set(command, "$size", (long)entry.SerializedSize);
        Set(command, "$signature", entry.SignatureMatches ? 1 : 0);
        Set(command, "$within", entry.IsWithinDataSection ? 1 : 0);
        Set(command, "$field_count", fieldCount);
        Set(command, "$field_shape", fieldShape);
        Set(command, "$field_error", fieldError);
    }

    private static SqliteCommand CreateFieldInsert(
        SqliteConnection connection,
        SqliteTransaction transaction)
    {
        SqliteCommand command = CreateCommand(
            connection,
            transaction,
            """
            INSERT INTO direct_fields(
                file_id, object_index, field_index, section_index,
                is_section_terminator, field_type, occurrence, raw_header,
                size_kind, header_size, payload_size, relative_header_offset,
                relative_payload_offset, absolute_header_offset,
                absolute_payload_offset, semantic_key, payload_layout,
                decoded_value, is_decoded, payload_preview, payload_sha256)
            VALUES($file, $object, $index, $section, $terminator, $type,
                   $occurrence, $raw, $size_kind, $header_size, $payload_size,
                   $rel_header, $rel_payload, $abs_header, $abs_payload,
                   $semantic, $layout, $decoded, $is_decoded, $preview, $sha);
            """);
        foreach (string name in new[]
                 {
                     "$file", "$object", "$index", "$section", "$terminator",
                     "$type", "$occurrence", "$raw", "$header_size", "$payload_size",
                     "$rel_header", "$rel_payload", "$abs_header", "$abs_payload",
                     "$is_decoded"
                 })
            command.Parameters.Add(name, SqliteType.Integer);
        foreach (string name in new[]
                 { "$size_kind", "$semantic", "$layout", "$decoded", "$sha" })
            command.Parameters.Add(name, SqliteType.Text);
        command.Parameters.Add("$preview", SqliteType.Blob);
        return command;
    }

    private static void BindField(
        SqliteCommand command,
        int fileId,
        int objectIndex,
        int fieldIndex,
        int section,
        bool terminator,
        SmoObjectField field,
        SmoSerializedFieldValue? known)
    {
        ReadOnlySpan<byte> payload = field.Payload.Span;
        byte[] preview = payload[..Math.Min(payload.Length, PayloadPreviewBytes)].ToArray();
        string? hash = payload.Length <= PayloadHashLimit
            ? Convert.ToHexString(SHA256.HashData(payload))
            : null;
        Set(command, "$file", fileId);
        Set(command, "$object", objectIndex);
        Set(command, "$index", fieldIndex);
        Set(command, "$section", section);
        Set(command, "$terminator", terminator ? 1 : 0);
        Set(command, "$type", field.FieldType);
        Set(command, "$occurrence", field.Occurrence);
        Set(command, "$raw", field.RawHeader);
        Set(command, "$size_kind", field.SizeKind.ToString());
        Set(command, "$header_size", field.HeaderSize);
        Set(command, "$payload_size", (long)field.PayloadSize);
        Set(command, "$rel_header", field.RelativeHeaderOffset);
        Set(command, "$rel_payload", field.RelativePayloadOffset);
        Set(command, "$abs_header", field.AbsoluteHeaderOffset);
        Set(command, "$abs_payload", field.AbsolutePayloadOffset);
        Set(command, "$semantic", known?.Descriptor.Key);
        Set(command, "$layout", known?.Descriptor.PayloadLayout);
        Set(command, "$decoded", known?.DisplayValue);
        Set(command, "$is_decoded", known?.IsDecoded == true ? 1 : 0);
        Set(command, "$preview", preview);
        Set(command, "$sha", hash);
    }

    private static string BuildFieldShape(IReadOnlyList<SmoObjectField> fields)
    {
        if (fields.Count == 0)
            return string.Empty;
        int section = 0;
        var parts = new string[fields.Count];
        for (int index = 0; index < fields.Count; index++)
        {
            SmoObjectField field = fields[index];
            bool terminator = field.FieldType == 0 && field.PayloadSize == 0;
            parts[index] = terminator
                ? $"s{section}:end"
                : $"s{section}:f{field.FieldType}:{field.PayloadSize}";
            if (terminator)
                section++;
        }
        return string.Join('|', parts);
    }

    private static SmoCorpusSummary ReadSummary(
        SqliteConnection connection,
        string databasePath)
    {
        static long Scalar(SqliteConnection connection, string sql)
        {
            using SqliteCommand command = connection.CreateCommand();
            command.CommandText = sql;
            return Convert.ToInt64(command.ExecuteScalar());
        }

        using SqliteCommand rootCommand = connection.CreateCommand();
        rootCommand.CommandText =
            "SELECT value FROM schema_info WHERE key='source_root';";
        string? root = rootCommand.ExecuteScalar() as string;
        int files = checked((int)Scalar(connection, "SELECT COUNT(*) FROM files;"));
        int parsed = checked((int)Scalar(
            connection, "SELECT COUNT(*) FROM files WHERE parse_status='ok';"));
        int failed = checked((int)Scalar(
            connection, "SELECT COUNT(*) FROM files WHERE parse_status<>'ok';"));
        long objects = Scalar(connection, "SELECT COUNT(*) FROM objects;");
        long fields = Scalar(connection, "SELECT COUNT(*) FROM direct_fields;");
        int classes = checked((int)Scalar(
            connection, "SELECT COUNT(DISTINCT type_hash) FROM objects;"));
        int known = checked((int)Scalar(connection, """
            SELECT COUNT(DISTINCT o.type_hash) FROM objects o
            JOIN classes c ON c.type_hash=o.type_hash WHERE c.is_known=1;
            """));
        int unknown = checked((int)Scalar(connection, """
            SELECT COUNT(DISTINCT o.type_hash) FROM objects o
            JOIN classes c ON c.type_hash=o.type_hash WHERE c.is_known=0;
            """));
        int variants = checked((int)Scalar(
            connection, "SELECT COUNT(*) FROM class_variants;"));
        int assignments = checked((int)Scalar(
            connection, "SELECT COUNT(*) FROM object_variant_assignments;"));
        int definitions = checked((int)Scalar(
            connection, "SELECT COUNT(*) FROM field_definitions;"));
        return new SmoCorpusSummary(
            databasePath,
            root,
            files,
            parsed,
            failed,
            objects,
            fields,
            classes,
            known,
            unknown,
            variants,
            assignments,
            definitions,
            File.Exists(databasePath) ? new FileInfo(databasePath).Length : 0);
    }

    private static SqliteCommand CreateCommand(
        SqliteConnection connection,
        SqliteTransaction transaction,
        string sql)
    {
        SqliteCommand command = connection.CreateCommand();
        command.Transaction = transaction;
        command.CommandText = sql;
        return command;
    }

    private static void AddNullable(
        SqliteCommand command,
        string name,
        object? value) =>
        command.Parameters.AddWithValue(name, value ?? DBNull.Value);

    private static void Set(SqliteCommand command, string name, object? value) =>
        command.Parameters[name].Value = value ?? DBNull.Value;

    private static string NormalizeRelativePath(string path) =>
        path.Replace(Path.DirectorySeparatorChar, '/');

    private static string GetFamily(string relativePath)
    {
        int separator = relativePath.IndexOf('/');
        return separator < 0 ? "." : relativePath[..separator];
    }

    private sealed record ExistingFile(
        int Id,
        long ByteSize,
        long LastWriteUtcTicks,
        int ScannerRevision,
        string ParseStatus);
}
