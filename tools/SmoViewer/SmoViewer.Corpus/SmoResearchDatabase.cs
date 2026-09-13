using System.Diagnostics;
using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    public static SmoResearchUpdateResult UpdateDirectory(
        string databasePath,
        string corpusKey,
        string platformKey,
        string provenance,
        string sourceDirectory,
        string executablePath)
    {
        ValidateIdentity(corpusKey, platformKey, provenance);
        string root = Path.GetFullPath(sourceDirectory);
        if (!Directory.Exists(root))
            throw new DirectoryNotFoundException($"Corpus directory not found: {root}");
        string executable = RequireFile(executablePath, "Executable");
        string database = PrepareDatabase(databasePath);
        var stopwatch = Stopwatch.StartNew();
        var counters = new UpdateCounters();
        string[] paths = Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories)
            .Order(StringComparer.OrdinalIgnoreCase)
            .ToArray();

        using SqliteConnection connection = OpenResearch(database, readOnly: false);
        using SqliteTransaction transaction = connection.BeginTransaction();
        int corpusId = EnsureCorpus(
            connection, transaction, corpusKey, platformKey, provenance,
            "directory", root);
        int scanId = InsertScan(connection, transaction, corpusId, 2);
        SeedKnowledge(connection, transaction, platformKey);
        int containerId = UpsertContainer(
            connection, transaction, corpusId, scanId, "directory", ".",
            null, null, null, "ok", null);
        UpsertContainerKnowledge(
            connection, transaction, containerId, "directory", "confirmed", "indexed",
            JsonSerializer.Serialize(new { Root = root, Files = paths.Length }),
            new Dictionary<string, (string, string, string)>
            {
                ["directory.file_count"] = ("integer", JsonSerializer.Serialize(paths.Length),
                    "confirmed_scanner"),
                ["directory.role"] = ("string", JsonSerializer.Serialize("resource_root"),
                    "confirmed_scanner")
            });
        Dictionary<string, ExistingOccurrence> existing = LoadOccurrences(
            connection, transaction, containerId);

        foreach (string path in paths)
        {
            string relativePath = NormalizePath(Path.GetRelativePath(root, path));
            var info = new FileInfo(path);
            counters.Occurrences++;
            if (existing.TryGetValue(relativePath, out ExistingOccurrence? occurrence) &&
                occurrence.ByteSize == info.Length &&
                occurrence.LastWriteUtcTicks == info.LastWriteTimeUtc.Ticks &&
                occurrence.ScannerRevision == SmoResearchSchema.ScannerRevision &&
                !occurrence.ParseStatus.Equals("error", StringComparison.Ordinal))
            {
                TouchOccurrence(connection, transaction, occurrence.Id, scanId);
                counters.UnchangedResources++;
                continue;
            }

            string extension = Path.GetExtension(path).ToLowerInvariant();
            byte[]? data = GameResourceKnowledge.RequiresPayload(extension)
                ? File.ReadAllBytes(path)
                : null;
            string sha256 = data is not null
                ? Convert.ToHexString(SHA256.HashData(data))
                : HashFile(path);
            int fileId = IndexResource(
                connection, transaction, corpusId, platformKey, relativePath,
                info.Length, info.LastWriteTimeUtc.Ticks, sha256, data, counters);
            UpsertOccurrence(
                connection, transaction, fileId, containerId, relativePath,
                relativePath, null, null, info.Length, info.LastWriteTimeUtc.Ticks,
                scanId);
            counters.ScannedResources++;
        }

        counters.RemovedOccurrences += RemoveMissingOccurrences(
            connection, transaction, containerId, scanId);
        IndexAuxiliaryGameFiles(
            connection, transaction, corpusId, scanId, platformKey, root, executable,
            counters, out int executableFileId);
        DeleteOrphanResources(connection, transaction, corpusId);
        ResolveResourceDependencies(connection, transaction, corpusId);
        UpsertExecutable(
            connection, transaction, corpusId, platformKey, executable, executableFileId);
        FinishScan(connection, transaction, scanId, counters);
        transaction.Commit();
        Checkpoint(connection);
        stopwatch.Stop();
        return CreateResult(
            connection, database, corpusKey, platformKey, provenance,
            "directory", 2, counters, stopwatch.Elapsed);
    }

    public static SmoResearchUpdateResult UpdatePckDirectory(
        string databasePath,
        string corpusKey,
        string platformKey,
        string provenance,
        string pckDirectory,
        string executablePath)
    {
        ValidateIdentity(corpusKey, platformKey, provenance);
        string root = Path.GetFullPath(pckDirectory);
        if (!Directory.Exists(root))
            throw new DirectoryNotFoundException($"PCK directory not found: {root}");
        string executable = RequireFile(executablePath, "Executable");
        string database = PrepareDatabase(databasePath);
        string[] archives = Directory.EnumerateFiles(
                root, "*.pck", SearchOption.AllDirectories)
            .Order(StringComparer.OrdinalIgnoreCase)
            .ToArray();
        var stopwatch = Stopwatch.StartNew();
        var counters = new UpdateCounters();

        using SqliteConnection connection = OpenResearch(database, readOnly: false);
        using SqliteTransaction transaction = connection.BeginTransaction();
        int corpusId = EnsureCorpus(
            connection, transaction, corpusKey, platformKey, provenance,
            "pck", root);
        int scanId = InsertScan(connection, transaction, corpusId, archives.Length + 1);
        SeedKnowledge(connection, transaction, platformKey);
        Dictionary<string, ExistingContainer> existingContainers = LoadContainers(
            connection, transaction, corpusId);

        foreach (string archivePath in archives)
        {
            string relativeArchive = NormalizePath(
                Path.GetRelativePath(root, archivePath));
            var archiveInfo = new FileInfo(archivePath);
            if (existingContainers.TryGetValue(
                    relativeArchive, out ExistingContainer? existing) &&
                existing.ByteSize == archiveInfo.Length &&
                existing.LastWriteUtcTicks == archiveInfo.LastWriteTimeUtc.Ticks &&
                existing.ScannerRevision == SmoResearchSchema.ScannerRevision &&
                existing.ParseStatus.Equals("ok", StringComparison.Ordinal))
            {
                TouchContainer(connection, transaction, existing.Id, scanId);
                (long occurrenceCount, int resourceCount) = TouchContainerOccurrences(
                    connection, transaction, existing.Id, scanId);
                counters.Occurrences += occurrenceCount;
                counters.UnchangedResources += resourceCount;
                continue;
            }

            SparkplugPckArchive archive = SparkplugPckArchive.Load(archivePath);
            string archiveHash = HashFile(archivePath);
            int containerId = UpsertContainer(
                connection, transaction, corpusId, scanId, "pck", relativeArchive,
                archiveInfo.Length, archiveInfo.LastWriteTimeUtc.Ticks, archiveHash,
                "ok", null);
            UpsertContainerKnowledge(
                connection, transaction, containerId, "pck", "confirmed", "ok",
                JsonSerializer.Serialize(new
                {
                    Entries = archive.Entries.Count,
                    archive.StringTableSize,
                    SectorSize = 0x800
                }),
                new Dictionary<string, (string, string, string)>
                {
                    ["pck.entry_count"] = ("integer",
                        JsonSerializer.Serialize(archive.Entries.Count), "confirmed_parser"),
                    ["pck.string_table_bytes"] = ("uint32",
                        JsonSerializer.Serialize(archive.StringTableSize), "confirmed_parser"),
                    ["pck.sector_size"] = ("integer", JsonSerializer.Serialize(0x800),
                        "confirmed_parser")
                });
            using var stream = new FileStream(
                archivePath, FileMode.Open, FileAccess.Read, FileShare.Read);
            foreach (SparkplugPckEntry entry in archive.Entries)
            {
                byte[] data = SparkplugPckArchive.ReadEntry(stream, entry);
                string sha256 = Convert.ToHexString(SHA256.HashData(data));
                int fileId = IndexResource(
                    connection, transaction, corpusId, platformKey,
                    entry.LogicalPath, data.Length,
                    archiveInfo.LastWriteTimeUtc.Ticks, sha256, data, counters);
                string occurrenceKey = entry.Index.ToString(
                    System.Globalization.CultureInfo.InvariantCulture);
                UpsertOccurrence(
                    connection, transaction, fileId, containerId, occurrenceKey,
                    entry.LogicalPath, entry.Index, entry.ByteOffset, data.Length,
                    archiveInfo.LastWriteTimeUtc.Ticks, scanId);
                counters.Occurrences++;
                counters.ScannedResources++;
            }
            counters.RemovedOccurrences += RemoveMissingOccurrences(
                connection, transaction, containerId, scanId);
        }

        IndexAuxiliaryGameFiles(
            connection, transaction, corpusId, scanId, platformKey, root, executable,
            counters, out int executableFileId);
        counters.RemovedOccurrences += RemoveMissingContainers(
            connection, transaction, corpusId, scanId);
        DeleteOrphanResources(connection, transaction, corpusId);
        ResolveResourceDependencies(connection, transaction, corpusId);
        UpsertExecutable(
            connection, transaction, corpusId, platformKey, executable, executableFileId);
        FinishScan(connection, transaction, scanId, counters);
        transaction.Commit();
        Checkpoint(connection);
        stopwatch.Stop();
        return CreateResult(
            connection, database, corpusKey, platformKey, provenance,
            "pck", archives.Length + 1, counters, stopwatch.Elapsed);
    }

    public static SmoResearchSummary GetSummary(string databasePath)
    {
        string database = Path.GetFullPath(databasePath);
        if (!File.Exists(database))
            throw new FileNotFoundException("Research database not found.", database);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        var corpora = new List<SmoResearchCorpusSummary>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT c.id, c.corpus_key, p.platform_key, c.provenance, c.source_kind
                FROM corpora c JOIN platforms p ON p.id=c.platform_id
                ORDER BY c.id;
                """;
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                int corpusId = reader.GetInt32(0);
                corpora.Add(ReadCorpusSummary(
                    connection, corpusId, reader.GetString(1), reader.GetString(2),
                    reader.GetString(3), reader.GetString(4)));
            }
        }

        int classes = checked((int)Scalar(
            connection, "SELECT COUNT(DISTINCT type_hash) FROM objects;"));
        int pcOnly = checked((int)Scalar(connection,
            "SELECT COUNT(*) FROM platform_class_presence WHERE on_pc=1 AND on_ps2=0;"));
        int ps2Only = checked((int)Scalar(connection,
            "SELECT COUNT(*) FROM platform_class_presence WHERE on_pc=0 AND on_ps2=1;"));
        int common = checked((int)Scalar(connection,
            "SELECT COUNT(*) FROM platform_class_presence WHERE on_pc=1 AND on_ps2=1;"));
        return new SmoResearchSummary(
            database,
            SmoResearchSchema.Version,
            corpora,
            classes,
            pcOnly,
            ps2Only,
            common,
            new FileInfo(database).Length);
    }

    public static int ImportRuntimeEvidence(
        string databasePath,
        IReadOnlyList<SmoResearchRuntimeEvidence> records)
    {
        ArgumentNullException.ThrowIfNull(records);
        if (records.Count == 0)
            throw new ArgumentException("Runtime evidence list is empty.", nameof(records));
        string database = Path.GetFullPath(databasePath);
        if (!File.Exists(database))
            throw new FileNotFoundException("Research database not found.", database);

        using SqliteConnection connection = OpenResearch(database, readOnly: false);
        using SqliteTransaction transaction = connection.BeginTransaction();
        int imported = 0;
        foreach (SmoResearchRuntimeEvidence record in records)
        {
            ValidateRuntimeEvidence(record);
            uint? typeHash = string.IsNullOrWhiteSpace(record.TypeIdentifier)
                ? null
                : ResolveClass(connection, record.TypeIdentifier).TypeHash;
            int? platformId = ResolveOptionalId(
                connection,
                transaction,
                "SELECT id FROM platforms WHERE platform_key=$key;",
                record.PlatformKey,
                "platform");
            int? corpusId = ResolveOptionalId(
                connection,
                transaction,
                "SELECT id FROM corpora WHERE corpus_key=$key;",
                record.CorpusKey,
                "corpus");

            using (SqliteCommand delete = CreateCommand(connection, transaction, """
                DELETE FROM evidence
                WHERE COALESCE(type_hash,-1)=COALESCE($hash,-1)
                  AND evidence_kind=$kind
                  AND source_path=$path
                  AND COALESCE(locator,'')=COALESCE($locator,'');
                """))
            {
                AddNullable(delete, "$hash", typeHash.HasValue ? (long)typeHash.Value : null);
                delete.Parameters.AddWithValue("$kind", record.EvidenceKind);
                delete.Parameters.AddWithValue("$path", record.SourcePath);
                AddNullable(delete, "$locator", record.Locator);
                delete.ExecuteNonQuery();
            }

            using SqliteCommand insert = CreateCommand(connection, transaction, """
                INSERT INTO evidence(
                    type_hash,variant_id,platform_id,corpus_id,evidence_kind,
                    source_path,locator,observation,confidence,source_sha256,created_utc)
                VALUES($hash,NULL,$platform,$corpus,$kind,$path,$locator,
                       $observation,$confidence,$sha,$utc);
                """);
            AddNullable(insert, "$hash", typeHash.HasValue ? (long)typeHash.Value : null);
            AddNullable(insert, "$platform", platformId);
            AddNullable(insert, "$corpus", corpusId);
            insert.Parameters.AddWithValue("$kind", record.EvidenceKind);
            insert.Parameters.AddWithValue("$path", record.SourcePath);
            AddNullable(insert, "$locator", record.Locator);
            insert.Parameters.AddWithValue("$observation", record.Observation);
            insert.Parameters.AddWithValue("$confidence", record.Confidence);
            AddNullable(insert, "$sha", record.SourceSha256);
            insert.Parameters.AddWithValue("$utc", record.CreatedUtc);
            imported += insert.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);
        return imported;
    }

    private static void ValidateRuntimeEvidence(SmoResearchRuntimeEvidence record)
    {
        ArgumentNullException.ThrowIfNull(record);
        ArgumentException.ThrowIfNullOrWhiteSpace(record.EvidenceKind);
        ArgumentException.ThrowIfNullOrWhiteSpace(record.SourcePath);
        ArgumentException.ThrowIfNullOrWhiteSpace(record.Observation);
        if (record.Confidence is not ("confirmed" or "probable" or "hypothesis"))
        {
            throw new ArgumentException(
                $"Unsupported runtime evidence confidence '{record.Confidence}'.");
        }
        if (!DateTimeOffset.TryParse(
                record.CreatedUtc,
                System.Globalization.CultureInfo.InvariantCulture,
                System.Globalization.DateTimeStyles.RoundtripKind,
                out _))
        {
            throw new ArgumentException(
                $"Runtime evidence timestamp is not ISO-8601: '{record.CreatedUtc}'.");
        }
        if (record.SourceSha256 is { Length: > 0 } sha &&
            (sha.Length != 64 || sha.Any(character => !Uri.IsHexDigit(character))))
        {
            throw new ArgumentException("Runtime evidence source SHA-256 is invalid.");
        }
    }

    private static int? ResolveOptionalId(
        SqliteConnection connection,
        SqliteTransaction transaction,
        string sql,
        string? key,
        string kind)
    {
        if (string.IsNullOrWhiteSpace(key))
            return null;
        using SqliteCommand command = CreateCommand(connection, transaction, sql);
        command.Parameters.AddWithValue("$key", key);
        object? value = command.ExecuteScalar();
        if (value is null or DBNull)
            throw new InvalidOperationException($"Unknown {kind} key '{key}'.");
        return Convert.ToInt32(value, System.Globalization.CultureInfo.InvariantCulture);
    }

    public static IReadOnlyList<SmoResearchClassCount> GetClassCounts(
        string databasePath)
    {
        string database = Path.GetFullPath(databasePath);
        if (!File.Exists(database))
            throw new FileNotFoundException("Research database not found.", database);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT corpus_key,platform_key,type_hash,engine_name,
                   unique_object_count,unique_resource_count,
                   physical_object_occurrences
            FROM corpus_class_totals
            ORDER BY corpus_key,unique_object_count DESC,type_hash;
            """;
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new List<SmoResearchClassCount>();
        while (reader.Read())
        {
            result.Add(new SmoResearchClassCount(
                reader.GetString(0),reader.GetString(1),
                checked((uint)reader.GetInt64(2)),
                reader.IsDBNull(3) ? null : reader.GetString(3),
                reader.GetInt64(4),reader.GetInt32(5),reader.GetInt64(6)));
        }
        return result;
    }

    public static SmoResearchClassReport GetClassReport(
        string databasePath,
        string classIdentifier)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(classIdentifier);
        string database = Path.GetFullPath(databasePath);
        if (!File.Exists(database))
            throw new FileNotFoundException("Research database not found.", database);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        (uint typeHash, string? engineName) = ResolveClass(
            connection, classIdentifier);

        var profiles = new List<SmoResearchClassProfile>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT c.corpus_key,p.platform_key,COUNT(*),COUNT(DISTINCT o.file_id),
                       SUM((SELECT COUNT(*) FROM file_occurrences fo
                            WHERE fo.file_id=o.file_id)),
                       SUM(CASE WHEN o.name<>'' THEN 1 ELSE 0 END),
                       MIN(o.serialized_size),MAX(o.serialized_size)
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                JOIN platforms p ON p.id=c.platform_id
                WHERE o.type_hash=$hash
                GROUP BY c.id
                ORDER BY c.id;
                """;
            command.Parameters.AddWithValue("$hash", (long)typeHash);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                profiles.Add(new SmoResearchClassProfile(
                    reader.GetString(0),reader.GetString(1),reader.GetInt64(2),
                    reader.GetInt32(3),reader.GetInt64(4),reader.GetInt64(5),
                    checked((int)reader.GetInt64(6)),checked((int)reader.GetInt64(7))));
            }
        }
        if (profiles.Count == 0)
        {
            throw new InvalidOperationException(
                $"Class 0x{typeHash:X8} ({engineName ?? "<unknown>"}) is not observed.");
        }

        var resources = new List<SmoResearchClassResource>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT c.corpus_key,f.relative_path,COUNT(*),
                       COUNT(*) * (SELECT COUNT(*) FROM file_occurrences fo
                                   WHERE fo.file_id=f.id)
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                WHERE o.type_hash=$hash
                GROUP BY c.id,f.id
                ORDER BY c.id,COUNT(*) DESC,f.normalized_path;
                """;
            command.Parameters.AddWithValue("$hash", (long)typeHash);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                resources.Add(new SmoResearchClassResource(
                    reader.GetString(0),reader.GetString(1),reader.GetInt64(2),
                    reader.GetInt64(3)));
            }
        }

        var counterparts = new List<SmoResearchClassCounterpart>();
        var candidateClassCounts = new Dictionary<int, int>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT file_id,COUNT(*) FROM objects
                WHERE type_hash=$hash GROUP BY file_id;
                """;
            command.Parameters.AddWithValue("$hash", (long)typeHash);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
                candidateClassCounts.Add(reader.GetInt32(0),reader.GetInt32(1));
        }
        var candidatesByCanonicalPath = new Dictionary<
            string,List<ResourceCandidateIdentity>>(StringComparer.OrdinalIgnoreCase);
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT f.id,c.corpus_key,p.platform_key,f.relative_path,
                       f.normalized_path,f.sha256,COALESCE(f.object_count,0),COUNT(fo.id)
                FROM files f
                JOIN corpora c ON c.id=f.corpus_id
                JOIN platforms p ON p.id=c.platform_id
                LEFT JOIN file_occurrences fo ON fo.file_id=f.id
                GROUP BY f.id
                ORDER BY c.id,f.normalized_path,f.sha256;
                """;
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                var candidate = new ResourceCandidateIdentity(
                    reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                    reader.GetString(3),reader.GetString(5),reader.GetInt32(6),
                    reader.GetInt32(7));
                string canonicalPath = GetCanonicalResourcePath(reader.GetString(4));
                if (!candidatesByCanonicalPath.TryGetValue(
                        canonicalPath, out List<ResourceCandidateIdentity>? group))
                {
                    group = [];
                    candidatesByCanonicalPath.Add(canonicalPath, group);
                }
                group.Add(candidate);
            }
        }
        Dictionary<string,string> corpusPlatforms = profiles.ToDictionary(
            item => item.CorpusKey,item => item.PlatformKey,StringComparer.Ordinal);
        foreach (SmoResearchClassResource resource in resources)
        {
            if (!candidatesByCanonicalPath.TryGetValue(
                    GetCanonicalResourcePath(resource.RelativePath),
                    out List<ResourceCandidateIdentity>? candidates))
                continue;
            string sourcePlatform = corpusPlatforms[resource.CorpusKey];
            foreach (ResourceCandidateIdentity candidate in candidates)
            {
                if (candidate.PlatformKey.Equals(sourcePlatform, StringComparison.Ordinal))
                    continue;
                counterparts.Add(new SmoResearchClassCounterpart(
                    resource.CorpusKey,resource.RelativePath,candidate.CorpusKey,
                    candidate.PlatformKey,candidate.RelativePath,candidate.Sha256,
                    candidate.ObjectCount,
                    candidateClassCounts.GetValueOrDefault(candidate.FileId),
                    candidate.PhysicalOccurrences));
            }
        }

        var relations = new List<SmoResearchClassRelation>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT c.corpus_key,'parent',parent.type_hash,cl.engine_name,
                       COUNT(*),COUNT(DISTINCT o.file_id)
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                LEFT JOIN objects parent ON parent.file_id=o.file_id
                                        AND parent.object_index=o.parent_index
                LEFT JOIN classes cl ON cl.type_hash=parent.type_hash
                WHERE o.type_hash=$hash
                GROUP BY c.id,parent.type_hash
                UNION ALL
                SELECT c.corpus_key,'child',child.type_hash,cl.engine_name,
                       COUNT(*),COUNT(DISTINCT o.file_id)
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                JOIN objects child ON child.file_id=o.file_id
                                  AND child.parent_index=o.object_index
                LEFT JOIN classes cl ON cl.type_hash=child.type_hash
                WHERE o.type_hash=$hash
                GROUP BY c.id,child.type_hash
                ORDER BY 1,2,5 DESC,3;
                """;
            command.Parameters.AddWithValue("$hash", (long)typeHash);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                relations.Add(new SmoResearchClassRelation(
                    reader.GetString(0),reader.GetString(1),
                    reader.IsDBNull(2) ? null : checked((uint)reader.GetInt64(2)),
                    reader.IsDBNull(3) ? null : reader.GetString(3),
                    reader.GetInt64(4),reader.GetInt32(5)));
            }
        }

        var variants = new List<SmoResearchClassVariantCandidate>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT c.corpus_key,p.platform_key,o.serialized_size,o.field_shape,
                       COUNT(*) AS object_count,
                       COUNT(DISTINCT o.file_id) AS resource_count
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                JOIN platforms p ON p.id=c.platform_id
                WHERE o.type_hash=$hash
                GROUP BY c.id,o.serialized_size,o.field_shape
                ORDER BY c.corpus_key,object_count DESC,
                         o.serialized_size,o.field_shape;
                """;
            command.Parameters.AddWithValue("$hash", (long)typeHash);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                variants.Add(new SmoResearchClassVariantCandidate(
                    reader.GetString(0),reader.GetString(1),reader.GetInt32(2),
                    reader.GetString(3),reader.GetInt64(4),reader.GetInt32(5)));
            }
        }

        var fields = new List<SmoResearchClassFieldShape>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT c.corpus_key,p.platform_key,d.section_index,
                       ((SELECT MAX(dd.section_index) FROM direct_fields dd
                         WHERE dd.file_id=d.file_id
                           AND dd.object_index=d.object_index)
                        - d.section_index) AS section_from_end,
                       d.field_type,d.occurrence,d.payload_size,COUNT(*),
                       COUNT(DISTINCT d.file_id),d.semantic_key,d.payload_layout
                FROM objects o
                JOIN direct_fields d ON d.file_id=o.file_id
                                    AND d.object_index=o.object_index
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                JOIN platforms p ON p.id=c.platform_id
                WHERE o.type_hash=$hash AND d.is_section_terminator=0
                GROUP BY c.id,d.section_index,section_from_end,d.field_type,
                         d.occurrence,d.payload_size,d.semantic_key,d.payload_layout
                ORDER BY c.id,section_from_end,d.field_type,d.occurrence,d.payload_size;
                """;
            command.Parameters.AddWithValue("$hash", (long)typeHash);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                fields.Add(new SmoResearchClassFieldShape(
                    reader.GetString(0),reader.GetString(1),reader.GetInt32(2),
                    reader.GetInt32(3),reader.GetInt32(4),reader.GetInt32(5),
                    reader.GetInt32(6),reader.GetInt64(7),reader.GetInt32(8),
                    reader.IsDBNull(9) ? null : reader.GetString(9),
                    reader.IsDBNull(10) ? null : reader.GetString(10)));
            }
        }

        var fieldSamples = new List<SmoResearchClassFieldSample>();
        var fieldSampleCounts = new Dictionary<
            (string Corpus, int SectionFromEnd, int Type, int Occurrence, int Size), int>();
        var distinctSamples = new HashSet<
            (string Corpus, int SectionFromEnd, int Type, int Occurrence, int Size, string Hex)>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT c.corpus_key,
                       ((SELECT MAX(dd.section_index) FROM direct_fields dd
                         WHERE dd.file_id=d.file_id
                           AND dd.object_index=d.object_index)
                        - d.section_index) AS section_from_end,
                       d.field_type,d.occurrence,d.payload_size,d.payload_preview,
                       d.decoded_value,f.relative_path,o.object_index,o.name
                FROM objects o
                JOIN direct_fields d ON d.file_id=o.file_id
                                    AND d.object_index=o.object_index
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                WHERE o.type_hash=$hash AND d.is_section_terminator=0
                ORDER BY c.id,d.field_type,d.occurrence,d.payload_size,
                         f.normalized_path,o.object_index,d.field_index;
                """;
            command.Parameters.AddWithValue("$hash", (long)typeHash);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                string corpus = reader.GetString(0);
                int sectionFromEnd = reader.GetInt32(1);
                int fieldType = reader.GetInt32(2);
                int occurrence = reader.GetInt32(3);
                int payloadSize = reader.GetInt32(4);
                string hex = Convert.ToHexString(reader.GetFieldValue<byte[]>(5));
                var shape = (corpus,sectionFromEnd,fieldType,occurrence,payloadSize);
                var distinct = (corpus,sectionFromEnd,fieldType,occurrence,payloadSize,hex);
                fieldSampleCounts.TryGetValue(shape, out int count);
                if (count >= 5 || !distinctSamples.Add(distinct))
                    continue;
                fieldSampleCounts[shape] = count + 1;
                fieldSamples.Add(new SmoResearchClassFieldSample(
                    corpus,sectionFromEnd,fieldType,occurrence,payloadSize,hex,
                    reader.IsDBNull(6) ? null : reader.GetString(6),
                    reader.GetString(7),reader.GetInt32(8),reader.GetString(9)));
            }
        }

        var examples = new List<SmoResearchClassExample>();
        var exampleCounts = new Dictionary<string, int>(StringComparer.Ordinal);
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT c.corpus_key,f.relative_path,o.object_index,o.object_id,o.name,
                       o.parent_index,parent.name,parent.type_hash,
                       o.serialized_size,o.field_shape
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                LEFT JOIN objects parent ON parent.file_id=o.file_id
                                        AND parent.object_index=o.parent_index
                WHERE o.type_hash=$hash
                ORDER BY c.id,f.normalized_path,o.object_index;
                """;
            command.Parameters.AddWithValue("$hash", (long)typeHash);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                string corpus = reader.GetString(0);
                exampleCounts.TryGetValue(corpus, out int count);
                if (count >= 12)
                    continue;
                exampleCounts[corpus] = count + 1;
                examples.Add(new SmoResearchClassExample(
                    corpus,reader.GetString(1),reader.GetInt32(2),
                    checked((uint)reader.GetInt64(3)),reader.GetString(4),
                    reader.IsDBNull(5) ? null : reader.GetInt32(5),
                    reader.IsDBNull(6) ? null : reader.GetString(6),
                    reader.IsDBNull(7) ? null : checked((uint)reader.GetInt64(7)),
                    reader.GetInt32(8),reader.GetString(9)));
            }
        }

        return new SmoResearchClassReport(
            typeHash,engineName,profiles,resources,counterparts,relations,variants,
            fields,fieldSamples,examples);
    }

    public static SmoResearchClassAnalysisResult AnalyzeClass(
        string databasePath,
        string classIdentifier)
    {
        SmoResearchClassReport report = GetClassReport(databasePath, classIdentifier);
        return report.TypeHash switch
        {
            SmoClassIds.Node => AnalyzeNode(databasePath, report),
            SmoClassIds.RenderNode => AnalyzeRenderNode(databasePath, report),
            SmoClassIds.TextureData => AnalyzeTextureData(databasePath, report),
            SmoClassIds.MaterialData => AnalyzeMaterialData(databasePath, report),
            SmoClassIds.MeshData => AnalyzeMeshData(databasePath, report),
            SmoClassIds.Model => AnalyzeModel(databasePath, report),
            SmoClassIds.StaticRenderObject =>
                AnalyzeStaticRenderObject(databasePath, report),
            SmoClassIds.Skin => AnalyzeSkin(databasePath, report),
            SmoClassIds.CollisionInfo =>
                AnalyzeCollisionInfo(databasePath, report),
            SmoClassIds.MeshBoundingVolume =>
                AnalyzeMeshBoundingVolume(databasePath, report),
            SmoClassIds.PartitionRenderable =>
                AnalyzePartitionRenderable(databasePath, report),
            SmoClassIds.PartitionNode => AnalyzePartitionNode(databasePath,report),
            SmoClassIds.OctreeNode => AnalyzeOctreeNode(databasePath,report),
            SmoClassIds.PartitionSystem =>
                AnalyzePartitionSystem(databasePath,report),
            SmoClassIds.Zone => AnalyzeZone(databasePath,report),
            SmoClassIds.ZonePortal => AnalyzeZonePortal(databasePath,report),
            SmoClassIds.ZonePortalNode =>
                AnalyzeZonePortalNode(databasePath,report),
            SmoClassIds.BspNode => AnalyzeBspNode(databasePath,report),
            SmoClassIds.OcclusionVolume =>
                AnalyzeOcclusionVolume(databasePath,report),
            SmoClassIds.MeshNavigationSet =>
                AnalyzeMeshNavigationSet(databasePath,report),
            SmoClassIds.NavigationPortal =>
                AnalyzeNavigationPortal(databasePath,report),
            SmoClassIds.NavigationGraph =>
                AnalyzeNavigationGraph(databasePath,report),
            SmoClassIds.SkyBox => AnalyzeSkyBox(databasePath,report),
            SmoClassIds.ParticleSystem => AnalyzeParticleSystem(databasePath,report),
            SmoClassIds.AnimTextureController =>
                AnalyzeAnimTextureController(databasePath,report),
            SmoClassIds.LensFlare => AnalyzeLensFlare(databasePath,report),
            SmoClassIds.Font => AnalyzeFont(databasePath,report),
            SmoClassIds.TextRenderable => AnalyzeTextRenderable(databasePath,report),
            SmoClassIds.TextNode => AnalyzeTextNode(databasePath,report),
            SmoClassIds.MaterialColorController =>
                AnalyzeMaterialColorController(databasePath, report),
            SmoClassIds.Fog => AnalyzeFog(databasePath, report),
            SmoClassIds.OrientedBoxBoundingVolume =>
                AnalyzeOrientedBoxBoundingVolume(databasePath, report),
            SmoClassIds.BoxBoundingVolume =>
                AnalyzeBoxBoundingVolume(databasePath, report),
            SmoClassIds.SphereBoundingVolume =>
                AnalyzeSphereBoundingVolume(databasePath, report),
            SmoClassIds.UvController =>
                AnalyzeUvController(databasePath, report),
            SmoClassIds.LightData =>
                AnalyzeLightData(databasePath, report),
            _ => throw new InvalidOperationException(
                $"A reproducible analyzer for 0x{report.TypeHash:X8} " +
                $"({report.EngineName ?? "<unknown>"}) has not been implemented yet.")
        };
    }

    private static SmoResearchClassAnalysisResult AnalyzeSphereBoundingVolume(
        string databasePath,
        SmoResearchClassReport report)
    {
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item =>
                item.NamedObjectCount != 0 || item.MinimumSerializedSize != 14 ||
                item.MaximumSerializedSize != 14) ||
            report.Variants.Any(item =>
                item.SerializedSize != 14 || item.FieldShape != "s0:f1:4|s0:end") ||
            report.Relations.Any(item =>
                item.Direction != "parent" ||
                item.RelatedTypeHash != SmoClassIds.CollisionInfo) ||
            report.Fields.Any(item =>
                item.SectionFromEnd != 0 || item.FieldType != 1 ||
                item.Occurrence != 0 ||
                item.PayloadSize != SmoSphereBoundingVolumeDecoder.RadiusPayloadSize))
        {
            throw new InvalidDataException(
                "spSphereBV corpus shape no longer matches the validated unnamed " +
                "14-byte child-of-spCollisionInfo structure.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        var observations = new List<SphereObservation>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT o.file_id,o.object_index,c.corpus_key,p.platform_key,
                       f.relative_path,d.payload_preview
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                JOIN platforms p ON p.id=c.platform_id
                JOIN direct_fields d ON d.file_id=o.file_id
                                    AND d.object_index=o.object_index
                WHERE o.type_hash=$hash AND d.is_section_terminator=0
                  AND d.section_index=0 AND d.field_type=1
                  AND d.occurrence=0 AND d.payload_size=$bytes
                ORDER BY c.id,f.normalized_path,o.object_index;
                """;
            command.Parameters.AddWithValue(
                "$hash",(long)SmoClassIds.SphereBoundingVolume);
            command.Parameters.AddWithValue(
                "$bytes",SmoSphereBoundingVolumeDecoder.RadiusPayloadSize);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                byte[] payload = reader.GetFieldValue<byte[]>(5);
                if (!SmoSphereBoundingVolumeDecoder.TryDecodeRadius(
                        payload,out float radius) ||
                    !float.IsFinite(radius) || radius <= 0.0f)
                {
                    throw new InvalidDataException(
                        $"Invalid spSphereBV radius in {reader.GetString(2)}:" +
                        $"{reader.GetString(4)}.");
                }
                observations.Add(new SphereObservation(
                    reader.GetInt32(0),reader.GetInt32(1),reader.GetString(2),
                    reader.GetString(3),GetCanonicalResourcePath(reader.GetString(4)),
                    payload,radius));
            }
        }

        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected one spSphereBV radius for each of {objectCount} objects, " +
                $"but found {observations.Count}.");
        }

        IGrouping<string,SphereObservation>[] payloadGroups = observations
            .GroupBy(item => Convert.ToHexString(item.Payload),StringComparer.Ordinal)
            .OrderBy(item => item.Key,StringComparer.Ordinal)
            .ToArray();
        float minimumRadius = observations.Min(item => item.Radius);
        float maximumRadius = observations.Max(item => item.Radius);
        string corpusPayloadCounts = string.Join(", ",observations
            .GroupBy(item => item.CorpusKey,StringComparer.Ordinal)
            .OrderBy(item => item.Key,StringComparer.Ordinal)
            .Select(corpus => $"{corpus.Key}={corpus.Select(item =>
                    Convert.ToHexString(item.Payload)).Distinct(StringComparer.Ordinal).Count()}"));

        Dictionary<string,string> pcWorking = BuildSpherePathMap(
            observations,"pc-working");
        Dictionary<string,string> pcPristine = BuildSpherePathMap(
            observations,"pc-pristine");
        Dictionary<string,string> ps2Pristine = BuildSpherePathMap(
            observations,"ps2-pristine");
        (int pcCommon,int pcEqual) = ComparePayloadPathMaps(
            pcWorking,pcPristine);
        (int platformCommon,int platformEqual) = ComparePayloadPathMaps(
            pcPristine,ps2Pristine);
        if (pcCommon != pcWorking.Count || pcCommon != pcPristine.Count ||
            pcEqual != pcCommon || platformCommon != pcPristine.Count ||
            platformCommon != ps2Pristine.Count || platformEqual != platformCommon)
        {
            throw new InvalidDataException(
                "spSphereBV radius payloads differ across matching PC or PC/PS2 " +
                "resources.");
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        RequireAsciiTokens(pcExecutable.Path,
            "spSphereBV","spSphereBVSerializer",
            "esfSphereBVPosition","esfSphereBVRadius");
        RequireAsciiTokens(ps2Executable.Path,
            "spSphereBV","spSphereBVSerializer",
            "esfSphereBVPosition","esfSphereBVRadius");

        string now = DateTime.UtcNow.ToString("O");
        const string radiusLayout = "Single radius";
        using SqliteTransaction transaction = connection.BeginTransaction();
        using (SqliteCommand clearEvidence = CreateCommand(connection,transaction,"""
                   DELETE FROM evidence WHERE type_hash=$hash
                   AND evidence_kind LIKE 'class_analysis:%';
                   """))
        {
            clearEvidence.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearEvidence.ExecuteNonQuery();
        }
        using (SqliteCommand clearVariants = CreateCommand(connection,transaction,"""
                   DELETE FROM class_variants
                   WHERE type_hash=$hash AND scope_kind='common'
                     AND scope_key='pc_ps2' AND variant_key LIKE 'sphere_bv_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        var fields = new[]
        {
            new
            {
                Type = 0,Semantic = "sphere_bv.position",
                Display = "Local sphere position",Kind = "vector3",
                Layout = "Vector3 (X, Y, Z)",
                Evidence = "confirmed_both_executables_unobserved_in_corpus",
                Constraints = JsonSerializer.Serialize(new
                {
                    observedOccurrences = 0,
                    executableDefault = new[] { 0.0f,0.0f,0.0f },
                    mutationStatus = "not_tested"
                }),
                Notes = "Optional field 0; both serializers omit the zero-vector default."
            },
            new
            {
                Type = 1,Semantic = "sphere_bv.radius",Display = "Sphere radius",
                Kind = "single",Layout = radiusLayout,
                Evidence = "confirmed_both_executables_and_full_corpus",
                Constraints = JsonSerializer.Serialize(new
                {
                    observedPayloadBytes =
                        SmoSphereBoundingVolumeDecoder.RadiusPayloadSize,
                    distinctObservedPayloads = payloadGroups.Length,
                    observedRange = new[] { minimumRadius,maximumRadius },
                    finitePositiveObservedValues = true,
                    serializedByExecutableWithoutDefaultComparison = true,
                    mutationStatus = "not_tested"
                }),
                Notes = "Field 1 is a radius, not a diameter. Both loaders copy " +
                        "the same Single to the sphere radius and base bounding-volume " +
                        "radius members."
            }
        };
        foreach (var definition in fields)
        {
            using SqliteCommand field = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',0,$type,-1,$semantic,$display,
                       $kind,$layout,'read_only_research',$evidence,$constraints,$notes)
                ON CONFLICT(type_hash,scope_kind,scope_key,section_from_end,
                            field_type,occurrence)
                DO UPDATE SET semantic_key=excluded.semantic_key,
                              display_name=excluded.display_name,
                              value_kind=excluded.value_kind,
                              payload_layout=excluded.payload_layout,
                              editable_status=excluded.editable_status,
                              evidence_status=excluded.evidence_status,
                              constraints_json=excluded.constraints_json,
                              notes=excluded.notes;
                """);
            field.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            field.Parameters.AddWithValue("$type",definition.Type);
            field.Parameters.AddWithValue("$semantic",definition.Semantic);
            field.Parameters.AddWithValue("$display",definition.Display);
            field.Parameters.AddWithValue("$kind",definition.Kind);
            field.Parameters.AddWithValue("$layout",definition.Layout);
            field.Parameters.AddWithValue("$evidence",definition.Evidence);
            field.Parameters.AddWithValue("$constraints",definition.Constraints);
            field.Parameters.AddWithValue("$notes",definition.Notes);
            field.ExecuteNonQuery();
        }

        foreach (IGrouping<string,SphereObservation> group in payloadGroups)
        {
            SphereObservation sample = group.First();
            using SqliteCommand decodedFields = CreateCommand(connection,transaction,"""
                UPDATE direct_fields SET semantic_key='sphere_bv.radius',
                    payload_layout=$layout,decoded_value=$value,is_decoded=1
                WHERE is_section_terminator=0 AND field_type=1
                  AND payload_size=$bytes AND payload_preview=$payload
                  AND EXISTS(
                      SELECT 1 FROM objects o
                      WHERE o.file_id=direct_fields.file_id
                        AND o.object_index=direct_fields.object_index
                        AND o.type_hash=$hash);
                """);
            decodedFields.Parameters.AddWithValue("$layout",radiusLayout);
            decodedFields.Parameters.AddWithValue("$value",JsonSerializer.Serialize(new
            {
                radius = sample.Radius
            }));
            decodedFields.Parameters.AddWithValue(
                "$bytes",SmoSphereBoundingVolumeDecoder.RadiusPayloadSize);
            decodedFields.Parameters.Add("$payload",SqliteType.Blob).Value =
                sample.Payload;
            decodedFields.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            decodedFields.ExecuteNonQuery();
        }

        int variantId;
        using (SqliteCommand variant = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,
                status,discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2','sphere_bv_radius_only',
                   'Radius-only sphere BV','confirmed',$discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """))
        {
            variant.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            variant.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
            {
                serializedSize = 14,
                fieldShape = "s0:f1:4|s0:end",
                observedFields = new[] { 1 }
            }));
            variant.Parameters.AddWithValue("$notes",
                $"Observed in all {objectCount} PC and PS2 objects. The one " +
                "radius value is a parameter, not a separate structural subtype.");
            variant.Parameters.AddWithValue("$utc",now);
            variantId = Convert.ToInt32(variant.ExecuteScalar());
        }
        using (SqliteCommand assignments = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            SELECT o.file_id,o.object_index,$variant,'confirmed',
                   'full PC+PS2 corpus: exact radius-only field shape'
            FROM objects o WHERE o.type_hash=$hash
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,
                          evidence=excluded.evidence;
            """))
        {
            assignments.Parameters.AddWithValue("$variant",variantId);
            assignments.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            assignments.ExecuteNonQuery();
        }

        string range =
            $"radius={FormatResearchFloat(minimumRadius)}.." +
            $"{FormatResearchFloat(maximumRadius)}";
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "load 0x0043A2E0..0x0043A4C1; serialize 0x0043A4D0..0x0043A741",
            "Field IDs are position=0 and radius=1. Position zero is omitted; " +
            "radius is always serialized. Loading copies the field-1 Single to " +
            "the concrete radius at +0x34 and base bounding radius at +0x24.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "load 0x0018A2D0..0x0018A454; serialize 0x0018A470..0x0018A668",
            "MIPS code uses the same two field IDs, optional zero position, " +
            "mandatory radius output and duplicated radius members as PC.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "all spSphereBV objects and direct fields in three corpora",
            $"{objectCount} unnamed children of spCollisionInfo in " +
            $"{report.Profiles.Sum(item => item.UniqueResourceCount)} corpus-resource " +
            $"rows; one 14-byte shape; {payloadGroups.Length} radius payload; " +
            $"per corpus: {corpusPayloadCounts}; {range}.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus 4-byte sphere radius payload",
            $"PC working/pristine: {pcEqual}/{pcCommon} matching resources; " +
            $"PC pristine/PS2 pristine: {platformEqual}/{platformCommon} matching resources.",
            null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='collision',
                       description='Spherical collision bounding volume',
                       decode_status='read_only_decode',
                       notes='Common PC/PS2 serializer: optional local position; all observed objects store only radius.'
                   WHERE type_hash=$hash;
                   """))
        {
            updateClass.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            updateClass.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);

        int assignmentCount;
        using (SqliteCommand assignments = connection.CreateCommand())
        {
            assignments.CommandText = """
                SELECT COUNT(*) FROM object_variant_assignments a
                JOIN class_variants v ON v.id=a.variant_id
                WHERE v.type_hash=$hash
                  AND v.variant_key='sphere_bv_radius_only';
                """;
            assignments.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            assignmentCount = Convert.ToInt32(assignments.ExecuteScalar());
        }
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,objectCount,
            report.Profiles.Sum(item => item.UniqueResourceCount),
            payloadGroups.Length,assignmentCount,evidenceRows,
            $"One radius-only layout; payloads: {corpusPayloadCounts}; " +
            $"PC pairs {pcEqual}/{pcCommon}; PC/PS2 pairs " +
            $"{platformEqual}/{platformCommon}; {range}. " +
            "Optional position is executable-confirmed but unobserved. " +
            "Mutation safety remains untested.");
    }

    private static SmoResearchClassAnalysisResult AnalyzeBoxBoundingVolume(
        string databasePath,
        SmoResearchClassReport report)
    {
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item =>
                item.NamedObjectCount != 0 || item.MinimumSerializedSize != 23 ||
                item.MaximumSerializedSize != 23) ||
            report.Variants.Any(item =>
                item.SerializedSize != 23 || item.FieldShape != "s0:f1:12|s0:end") ||
            report.Relations.Any(item =>
                item.Direction != "parent" ||
                item.RelatedTypeHash != SmoClassIds.CollisionInfo) ||
            report.Fields.Any(item =>
                item.SectionFromEnd != 0 || item.FieldType != 1 ||
                item.Occurrence != 0 ||
                item.PayloadSize != SmoBoxBoundingVolumeDecoder.VectorPayloadSize))
        {
            throw new InvalidDataException(
                "spBoxBV corpus shape no longer matches the validated unnamed " +
                "23-byte child-of-spCollisionInfo structure.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: false);
        var observations = new List<BoxObservation>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT o.file_id,o.object_index,c.corpus_key,p.platform_key,
                       f.relative_path,d.payload_preview
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                JOIN platforms p ON p.id=c.platform_id
                JOIN direct_fields d ON d.file_id=o.file_id
                                    AND d.object_index=o.object_index
                WHERE o.type_hash=$hash AND d.is_section_terminator=0
                  AND d.section_index=0 AND d.field_type=1
                  AND d.occurrence=0 AND d.payload_size=$bytes
                ORDER BY c.id,f.normalized_path,o.object_index;
                """;
            command.Parameters.AddWithValue(
                "$hash",(long)SmoClassIds.BoxBoundingVolume);
            command.Parameters.AddWithValue(
                "$bytes",SmoBoxBoundingVolumeDecoder.VectorPayloadSize);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                byte[] payload = reader.GetFieldValue<byte[]>(5);
                if (!SmoBoxBoundingVolumeDecoder.TryDecodeSize(
                        payload,out SmoBoxSizeData? size) || size is null ||
                    !IsFinitePositive(size.FullSize))
                {
                    throw new InvalidDataException(
                        $"Invalid spBoxBV size in {reader.GetString(2)}:" +
                        $"{reader.GetString(4)}.");
                }
                observations.Add(new BoxObservation(
                    reader.GetInt32(0),reader.GetInt32(1),reader.GetString(2),
                    reader.GetString(3),GetCanonicalResourcePath(reader.GetString(4)),
                    payload,size));
            }
        }

        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected one spBoxBV size for each of {objectCount} objects, " +
                $"but found {observations.Count}.");
        }

        IGrouping<string,BoxObservation>[] payloadGroups = observations
            .GroupBy(item => Convert.ToHexString(item.Payload),StringComparer.Ordinal)
            .OrderBy(item => item.Key,StringComparer.Ordinal)
            .ToArray();
        string corpusPayloadCounts = string.Join(", ",observations
            .GroupBy(item => item.CorpusKey,StringComparer.Ordinal)
            .OrderBy(item => item.Key,StringComparer.Ordinal)
            .Select(corpus => $"{corpus.Key}={corpus.Select(item =>
                    Convert.ToHexString(item.Payload)).Distinct(StringComparer.Ordinal).Count()}"));
        float minimumX = observations.Min(item => item.Data.FullSize.X);
        float maximumX = observations.Max(item => item.Data.FullSize.X);
        float minimumY = observations.Min(item => item.Data.FullSize.Y);
        float maximumY = observations.Max(item => item.Data.FullSize.Y);
        float minimumZ = observations.Min(item => item.Data.FullSize.Z);
        float maximumZ = observations.Max(item => item.Data.FullSize.Z);

        Dictionary<string,string> pcWorking = BuildBoxPathMap(
            observations,"pc-working");
        Dictionary<string,string> pcPristine = BuildBoxPathMap(
            observations,"pc-pristine");
        Dictionary<string,string> ps2Pristine = BuildBoxPathMap(
            observations,"ps2-pristine");
        (int pcCommon,int pcEqual) = ComparePayloadPathMaps(
            pcWorking,pcPristine);
        (int platformCommon,int platformEqual) = ComparePayloadPathMaps(
            pcPristine,ps2Pristine);
        if (pcCommon != pcWorking.Count || pcCommon != pcPristine.Count ||
            pcEqual != pcCommon || platformCommon != pcPristine.Count ||
            platformCommon != ps2Pristine.Count || platformEqual != platformCommon)
        {
            throw new InvalidDataException(
                "spBoxBV payload multisets differ across matching PC or PC/PS2 " +
                "resources.");
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        RequireAsciiTokens(pcExecutable.Path,
            "spBoxBV","spBoxBVSerializer","esfBoxBVPosition","esfBoxBVSize");
        RequireAsciiTokens(ps2Executable.Path,
            "spBoxBV","spBoxBVSerializer","esfBoxBVPosition","esfBoxBVSize");

        string now = DateTime.UtcNow.ToString("O");
        string sizeLayout =
            "Vector3 full dimensions (X, Y, Z); half-extents=size*0.5";
        using SqliteTransaction transaction = connection.BeginTransaction();
        using (SqliteCommand clearEvidence = CreateCommand(connection,transaction,"""
                   DELETE FROM evidence WHERE type_hash=$hash
                   AND evidence_kind LIKE 'class_analysis:%';
                   """))
        {
            clearEvidence.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearEvidence.ExecuteNonQuery();
        }
        using (SqliteCommand clearVariants = CreateCommand(connection,transaction,"""
                   DELETE FROM class_variants
                   WHERE type_hash=$hash AND scope_kind='common'
                     AND scope_key='pc_ps2' AND variant_key LIKE 'box_bv_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        var fields = new[]
        {
            new
            {
                Type = 0,Semantic = "box_bv.position",
                Display = "Local box position",Kind = "vector3",
                Layout = "Vector3 (X, Y, Z)",
                Evidence = "confirmed_both_executables_unobserved_in_corpus",
                Constraints = JsonSerializer.Serialize(new
                {
                    observedOccurrences = 0,
                    executableDefault = new[] { 0.0f,0.0f,0.0f },
                    mutationStatus = "not_tested"
                }),
                Notes = "Optional field 0; both serializers omit the zero-vector default."
            },
            new
            {
                Type = 1,Semantic = "box_bv.size",Display = "Full box size",
                Kind = "vector3",Layout = sizeLayout,
                Evidence = "confirmed_both_executables_and_full_corpus",
                Constraints = JsonSerializer.Serialize(new
                {
                    observedPayloadBytes =
                        SmoBoxBoundingVolumeDecoder.VectorPayloadSize,
                    distinctObservedPayloads = payloadGroups.Length,
                    observedXRange = new[] { minimumX,maximumX },
                    observedYRange = new[] { minimumY,maximumY },
                    observedZRange = new[] { minimumZ,maximumZ },
                    finitePositiveObservedComponents = true,
                    serializedByExecutableWithoutDefaultComparison = true,
                    mutationStatus = "not_tested"
                }),
                Notes = "Both engines store full dimensions, derive half-extents " +
                        "with a 0.5 multiplier and store their vector length as the " +
                        "bounding-sphere radius."
            }
        };
        foreach (var definition in fields)
        {
            using SqliteCommand field = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',0,$type,-1,$semantic,$display,
                       $kind,$layout,'read_only_research',$evidence,$constraints,$notes)
                ON CONFLICT(type_hash,scope_kind,scope_key,section_from_end,
                            field_type,occurrence)
                DO UPDATE SET semantic_key=excluded.semantic_key,
                              display_name=excluded.display_name,
                              value_kind=excluded.value_kind,
                              payload_layout=excluded.payload_layout,
                              editable_status=excluded.editable_status,
                              evidence_status=excluded.evidence_status,
                              constraints_json=excluded.constraints_json,
                              notes=excluded.notes;
                """);
            field.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            field.Parameters.AddWithValue("$type",definition.Type);
            field.Parameters.AddWithValue("$semantic",definition.Semantic);
            field.Parameters.AddWithValue("$display",definition.Display);
            field.Parameters.AddWithValue("$kind",definition.Kind);
            field.Parameters.AddWithValue("$layout",definition.Layout);
            field.Parameters.AddWithValue("$evidence",definition.Evidence);
            field.Parameters.AddWithValue("$constraints",definition.Constraints);
            field.Parameters.AddWithValue("$notes",definition.Notes);
            field.ExecuteNonQuery();
        }

        foreach (IGrouping<string,BoxObservation> group in payloadGroups)
        {
            BoxObservation sample = group.First();
            using SqliteCommand decodedFields = CreateCommand(connection,transaction,"""
                UPDATE direct_fields SET semantic_key='box_bv.size',
                    payload_layout=$layout,decoded_value=$value,is_decoded=1
                WHERE is_section_terminator=0 AND field_type=1
                  AND payload_size=$bytes AND payload_preview=$payload
                  AND EXISTS(
                      SELECT 1 FROM objects o
                      WHERE o.file_id=direct_fields.file_id
                        AND o.object_index=direct_fields.object_index
                        AND o.type_hash=$hash);
                """);
            decodedFields.Parameters.AddWithValue("$layout",sizeLayout);
            decodedFields.Parameters.AddWithValue("$value",JsonSerializer.Serialize(new
            {
                x = sample.Data.FullSize.X,
                y = sample.Data.FullSize.Y,
                z = sample.Data.FullSize.Z,
                halfX = sample.Data.HalfExtents.X,
                halfY = sample.Data.HalfExtents.Y,
                halfZ = sample.Data.HalfExtents.Z,
                boundingSphereRadius = sample.Data.BoundingSphereRadius
            }));
            decodedFields.Parameters.AddWithValue(
                "$bytes",SmoBoxBoundingVolumeDecoder.VectorPayloadSize);
            decodedFields.Parameters.Add("$payload",SqliteType.Blob).Value =
                sample.Payload;
            decodedFields.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            decodedFields.ExecuteNonQuery();
        }

        int variantId;
        using (SqliteCommand variant = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,
                status,discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2','box_bv_size_only','Size-only box BV',
                   'confirmed',$discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """))
        {
            variant.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            variant.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
            {
                serializedSize = 23,
                fieldShape = "s0:f1:12|s0:end",
                observedFields = new[] { 1 }
            }));
            variant.Parameters.AddWithValue("$notes",
                $"Observed in all {objectCount} PC and PS2 objects. The " +
                $"{payloadGroups.Length} dimension vectors are parameters, not " +
                "structural subtypes.");
            variant.Parameters.AddWithValue("$utc",now);
            variantId = Convert.ToInt32(variant.ExecuteScalar());
        }
        using (SqliteCommand assignments = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            SELECT o.file_id,o.object_index,$variant,'confirmed',
                   'full PC+PS2 corpus: exact size-only field shape'
            FROM objects o WHERE o.type_hash=$hash
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,
                          evidence=excluded.evidence;
            """))
        {
            assignments.Parameters.AddWithValue("$variant",variantId);
            assignments.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            assignments.ExecuteNonQuery();
        }

        string ranges =
            $"X={FormatResearchFloat(minimumX)}..{FormatResearchFloat(maximumX)}, " +
            $"Y={FormatResearchFloat(minimumY)}..{FormatResearchFloat(maximumY)}, " +
            $"Z={FormatResearchFloat(minimumZ)}..{FormatResearchFloat(maximumZ)}";
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "load 0x00439480..0x004396A1; serialize 0x004396C0..0x00439931",
            "Field IDs are position=0 and size=1. Position zero is omitted; size " +
            "is always serialized. Loading stores full dimensions at +0x34/+0x38/" +
            "+0x3C, multiplies them by 0.5 and stores the half-extents length at +0x24.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "load size handling 0x00188934..0x00188988; serialize 0x00188A50..0x00188C58",
            "MIPS code uses the same two field IDs, object offsets, optional zero " +
            "position and full-size 0.5/radius calculation as PC.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "all spBoxBV objects and direct fields in three corpora",
            $"{objectCount} unnamed children of spCollisionInfo in " +
            $"{report.Profiles.Sum(item => item.UniqueResourceCount)} corpus-resource " +
            $"rows; one 23-byte shape; {payloadGroups.Length} size vectors; " +
            $"per corpus: {corpusPayloadCounts}; ranges: {ranges}.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus sorted multiset of 12-byte box sizes",
            $"PC working/pristine: {pcEqual}/{pcCommon} matching resources; " +
            $"PC pristine/PS2 pristine: {platformEqual}/{platformCommon} matching resources.",
            null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='collision',
                       description='Axis-aligned box collision bounding volume',
                       decode_status='read_only_decode',
                       notes='Common PC/PS2 serializer: optional local position; all observed objects store only full size.'
                   WHERE type_hash=$hash;
                   """))
        {
            updateClass.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            updateClass.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);

        int assignmentCount;
        using (SqliteCommand assignments = connection.CreateCommand())
        {
            assignments.CommandText = """
                SELECT COUNT(*) FROM object_variant_assignments a
                JOIN class_variants v ON v.id=a.variant_id
                WHERE v.type_hash=$hash AND v.variant_key='box_bv_size_only';
                """;
            assignments.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            assignmentCount = Convert.ToInt32(assignments.ExecuteScalar());
        }
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,objectCount,
            report.Profiles.Sum(item => item.UniqueResourceCount),
            payloadGroups.Length,assignmentCount,evidenceRows,
            $"One size-only layout; payloads: {corpusPayloadCounts}; " +
            $"PC pairs {pcEqual}/{pcCommon}; PC/PS2 pairs " +
            $"{platformEqual}/{platformCommon}; ranges: {ranges}. " +
            "Optional position is executable-confirmed but unobserved. " +
            "Mutation safety remains untested.");
    }

    private static SmoResearchClassAnalysisResult AnalyzeOrientedBoxBoundingVolume(
        string databasePath,
        SmoResearchClassReport report)
    {
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item =>
                item.NamedObjectCount != 0 || item.MinimumSerializedSize != 23 ||
                item.MaximumSerializedSize != 23) ||
            report.Variants.Any(item =>
                item.SerializedSize != 23 || item.FieldShape != "s0:f1:12|s0:end") ||
            report.Relations.Any(item =>
                item.Direction != "parent" ||
                item.RelatedTypeHash != SmoClassIds.CollisionInfo) ||
            report.Fields.Any(item =>
                item.SectionFromEnd != 0 || item.FieldType != 1 ||
                item.Occurrence != 0 ||
                item.PayloadSize !=
                    SmoOrientedBoxBoundingVolumeDecoder.VectorPayloadSize))
        {
            throw new InvalidDataException(
                "spOBBBV corpus shape no longer matches the validated unnamed " +
                "23-byte child-of-spCollisionInfo structure.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: false);
        var observations = new List<OrientedBoxObservation>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT o.file_id,o.object_index,c.corpus_key,p.platform_key,
                       f.relative_path,d.payload_preview
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                JOIN platforms p ON p.id=c.platform_id
                JOIN direct_fields d ON d.file_id=o.file_id
                                    AND d.object_index=o.object_index
                WHERE o.type_hash=$hash AND d.is_section_terminator=0
                  AND d.section_index=0 AND d.field_type=1
                  AND d.occurrence=0 AND d.payload_size=$bytes
                ORDER BY c.id,f.normalized_path,o.object_index;
                """;
            command.Parameters.AddWithValue(
                "$hash", (long)SmoClassIds.OrientedBoxBoundingVolume);
            command.Parameters.AddWithValue(
                "$bytes", SmoOrientedBoxBoundingVolumeDecoder.VectorPayloadSize);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                byte[] payload = reader.GetFieldValue<byte[]>(5);
                if (!SmoOrientedBoxBoundingVolumeDecoder.TryDecodeSize(
                        payload, out SmoOrientedBoxSizeData? size) || size is null ||
                    !IsFinitePositive(size.FullSize))
                {
                    throw new InvalidDataException(
                        $"Invalid spOBBBV size in {reader.GetString(2)}:" +
                        $"{reader.GetString(4)}.");
                }
                observations.Add(new OrientedBoxObservation(
                    reader.GetInt32(0),reader.GetInt32(1),reader.GetString(2),
                    reader.GetString(3),GetCanonicalResourcePath(reader.GetString(4)),
                    payload,size));
            }
        }

        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected one spOBBBV size for each of {objectCount} objects, " +
                $"but found {observations.Count}.");
        }

        IGrouping<string,OrientedBoxObservation>[] payloadGroups = observations
            .GroupBy(item => Convert.ToHexString(item.Payload), StringComparer.Ordinal)
            .OrderBy(item => item.Key, StringComparer.Ordinal)
            .ToArray();
        string corpusPayloadCounts = string.Join(", ", observations
            .GroupBy(item => item.CorpusKey, StringComparer.Ordinal)
            .OrderBy(item => item.Key, StringComparer.Ordinal)
            .Select(corpus => $"{corpus.Key}={corpus.Select(item =>
                    Convert.ToHexString(item.Payload)).Distinct(StringComparer.Ordinal).Count()}"));
        float minimumX = observations.Min(item => item.Data.FullSize.X);
        float maximumX = observations.Max(item => item.Data.FullSize.X);
        float minimumY = observations.Min(item => item.Data.FullSize.Y);
        float maximumY = observations.Max(item => item.Data.FullSize.Y);
        float minimumZ = observations.Min(item => item.Data.FullSize.Z);
        float maximumZ = observations.Max(item => item.Data.FullSize.Z);

        Dictionary<string,string> pcWorking = BuildOrientedBoxPathMap(
            observations, "pc-working");
        Dictionary<string,string> pcPristine = BuildOrientedBoxPathMap(
            observations, "pc-pristine");
        Dictionary<string,string> ps2Pristine = BuildOrientedBoxPathMap(
            observations, "ps2-pristine");
        (int pcCommon, int pcEqual) = ComparePayloadPathMaps(
            pcWorking, pcPristine);
        (int platformCommon, int platformEqual) = ComparePayloadPathMaps(
            pcPristine, ps2Pristine);
        if (pcCommon != pcPristine.Count || pcEqual != pcCommon)
        {
            throw new InvalidDataException(
                "OBB payload multisets differ between matching pc-working and " +
                "pc-pristine resources.");
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection, "pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection, "ps2");
        RequireAsciiTokens(pcExecutable.Path,
            "spOBBBV","spOBBBVSerializer","esfOBBBVPosition",
            "esfOBBBVSize","esfOBBBVRotation");
        RequireAsciiTokens(ps2Executable.Path,
            "spOBBBV","spOBBBVSerializer","esfOBBBVPosition",
            "esfOBBBVSize","esfOBBBVRotation");

        string now = DateTime.UtcNow.ToString("O");
        string sizeLayout = "Vector3 full dimensions (X, Y, Z); half-extents=size*0.5";
        using SqliteTransaction transaction = connection.BeginTransaction();
        using (SqliteCommand clearEvidence = CreateCommand(connection, transaction, """
                   DELETE FROM evidence WHERE type_hash=$hash
                   AND evidence_kind LIKE 'class_analysis:%';
                   """))
        {
            clearEvidence.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            clearEvidence.ExecuteNonQuery();
        }
        using (SqliteCommand clearVariants = CreateCommand(connection, transaction, """
                   DELETE FROM class_variants
                   WHERE type_hash=$hash AND scope_kind='common'
                     AND scope_key='pc_ps2' AND variant_key LIKE 'obb_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        var fields = new[]
        {
            new
            {
                Type = 0, Semantic = "obb.position", Display = "Local OBB position",
                Kind = "vector3", Layout = "Vector3 (X, Y, Z)",
                Evidence = "confirmed_both_executables_unobserved_in_corpus",
                Constraints = JsonSerializer.Serialize(new
                {
                    observedOccurrences = 0,
                    executableDefault = new[] { 0.0f, 0.0f, 0.0f },
                    mutationStatus = "not_tested"
                }),
                Notes = "Optional field 0; both serializers omit the zero-vector default."
            },
            new
            {
                Type = 1, Semantic = "obb.size", Display = "Full OBB size",
                Kind = "vector3", Layout = sizeLayout,
                Evidence = "confirmed_both_executables_and_full_corpus",
                Constraints = JsonSerializer.Serialize(new
                {
                    observedPayloadBytes =
                        SmoOrientedBoxBoundingVolumeDecoder.VectorPayloadSize,
                    distinctObservedPayloads = payloadGroups.Length,
                    observedXRange = new[] { minimumX, maximumX },
                    observedYRange = new[] { minimumY, maximumY },
                    observedZRange = new[] { minimumZ, maximumZ },
                    finitePositiveObservedComponents = true,
                    mutationStatus = "not_tested"
                }),
                Notes = "The engine derives half-extents with a 0.5 multiplier and " +
                        "stores their vector length as the bounding-sphere radius."
            },
            new
            {
                Type = 2, Semantic = "obb.rotation", Display = "Local OBB rotation",
                Kind = "quaternion", Layout = "Quaternion (X, Y, Z, W)",
                Evidence = "confirmed_both_executables_unobserved_in_corpus",
                Constraints = JsonSerializer.Serialize(new
                {
                    observedOccurrences = 0,
                    executableDefault = new[] { 0.0f, 0.0f, 0.0f, 1.0f },
                    mutationStatus = "not_tested"
                }),
                Notes = "Optional field 2; serializers convert the internal rotation " +
                        "matrix to a quaternion and omit the identity default."
            }
        };
        foreach (var definition in fields)
        {
            using SqliteCommand field = CreateCommand(connection, transaction, """
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',0,$type,-1,$semantic,$display,
                       $kind,$layout,'read_only_research',$evidence,$constraints,$notes)
                ON CONFLICT(type_hash,scope_kind,scope_key,section_from_end,
                            field_type,occurrence)
                DO UPDATE SET semantic_key=excluded.semantic_key,
                              display_name=excluded.display_name,
                              value_kind=excluded.value_kind,
                              payload_layout=excluded.payload_layout,
                              editable_status=excluded.editable_status,
                              evidence_status=excluded.evidence_status,
                              constraints_json=excluded.constraints_json,
                              notes=excluded.notes;
                """);
            field.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            field.Parameters.AddWithValue("$type", definition.Type);
            field.Parameters.AddWithValue("$semantic", definition.Semantic);
            field.Parameters.AddWithValue("$display", definition.Display);
            field.Parameters.AddWithValue("$kind", definition.Kind);
            field.Parameters.AddWithValue("$layout", definition.Layout);
            field.Parameters.AddWithValue("$evidence", definition.Evidence);
            field.Parameters.AddWithValue("$constraints", definition.Constraints);
            field.Parameters.AddWithValue("$notes", definition.Notes);
            field.ExecuteNonQuery();
        }

        foreach (IGrouping<string,OrientedBoxObservation> group in payloadGroups)
        {
            OrientedBoxObservation sample = group.First();
            using SqliteCommand decodedFields = CreateCommand(connection, transaction, """
                UPDATE direct_fields SET semantic_key='obb.size',
                    payload_layout=$layout,decoded_value=$value,is_decoded=1
                WHERE is_section_terminator=0 AND field_type=1
                  AND payload_size=$bytes AND payload_preview=$payload
                  AND EXISTS(
                      SELECT 1 FROM objects o
                      WHERE o.file_id=direct_fields.file_id
                        AND o.object_index=direct_fields.object_index
                        AND o.type_hash=$hash);
                """);
            decodedFields.Parameters.AddWithValue("$layout", sizeLayout);
            decodedFields.Parameters.AddWithValue("$value", JsonSerializer.Serialize(new
            {
                x = sample.Data.FullSize.X,
                y = sample.Data.FullSize.Y,
                z = sample.Data.FullSize.Z,
                halfX = sample.Data.HalfExtents.X,
                halfY = sample.Data.HalfExtents.Y,
                halfZ = sample.Data.HalfExtents.Z,
                boundingSphereRadius = sample.Data.BoundingSphereRadius
            }));
            decodedFields.Parameters.AddWithValue(
                "$bytes", SmoOrientedBoxBoundingVolumeDecoder.VectorPayloadSize);
            decodedFields.Parameters.Add("$payload", SqliteType.Blob).Value = sample.Payload;
            decodedFields.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            decodedFields.ExecuteNonQuery();
        }

        int variantId;
        using (SqliteCommand variant = CreateCommand(connection, transaction, """
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,
                status,discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2','obb_size_only','Size-only OBB',
                   'confirmed',$discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """))
        {
            variant.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            variant.Parameters.AddWithValue("$discriminator", JsonSerializer.Serialize(new
            {
                serializedSize = 23,
                fieldShape = "s0:f1:12|s0:end",
                observedFields = new[] { 1 }
            }));
            variant.Parameters.AddWithValue("$notes",
                $"Observed in all {objectCount} PC and PS2 objects. The 80 distinct " +
                "dimension vectors are parameters, not structural subtypes.");
            variant.Parameters.AddWithValue("$utc", now);
            variantId = Convert.ToInt32(variant.ExecuteScalar());
        }
        using (SqliteCommand assignments = CreateCommand(connection, transaction, """
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            SELECT o.file_id,o.object_index,$variant,'confirmed',
                   'full PC+PS2 corpus: exact size-only field shape'
            FROM objects o WHERE o.type_hash=$hash
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,
                          evidence=excluded.evidence;
            """))
        {
            assignments.Parameters.AddWithValue("$variant", variantId);
            assignments.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            assignments.ExecuteNonQuery();
        }

        string ranges =
            $"X={FormatResearchFloat(minimumX)}..{FormatResearchFloat(maximumX)}, " +
            $"Y={FormatResearchFloat(minimumY)}..{FormatResearchFloat(maximumY)}, " +
            $"Z={FormatResearchFloat(minimumZ)}..{FormatResearchFloat(maximumZ)}";
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "load 0x00439BA0..0x00439E61; serialize 0x00439E70..0x0043A151",
            "Field IDs are position=0, size=1, rotation=2. Size is copied to " +
            "+0x58/+0x5C/+0x60; the engine multiplies it by 0.5 and stores the " +
            "half-extents length at +0x24. Position zero and identity rotation " +
            "are omitted; rotation is serialized as a quaternion.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "load 0x00189960..0x00189BA0; serialize 0x00189BC0..0x0018A03C",
            "MIPS code uses the same three IDs, object offsets, full-size 0.5 " +
            "calculation, optional defaults and quaternion representation as PC.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "all spOBBBV objects and direct fields in three corpora",
            $"{objectCount} unnamed children of spCollisionInfo in " +
            $"{report.Profiles.Sum(item => item.UniqueResourceCount)} corpus-resource " +
            $"rows; one 23-byte shape; {payloadGroups.Length} size vectors; " +
            $"per corpus: {corpusPayloadCounts}; ranges: {ranges}.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus sorted multiset of 12-byte OBB sizes",
            $"PC working/pristine: {pcEqual}/{pcCommon} matching resources; " +
            $"PC pristine/PS2 pristine: {platformEqual}/{platformCommon} matching resources.",
            null,now);

        using (SqliteCommand updateClass = CreateCommand(connection, transaction, """
                   UPDATE classes SET category='collision',
                       description='Oriented-box collision bounding volume',
                       decode_status='read_only_decode',
                       notes='Common PC/PS2 serializer: optional position and rotation; all observed objects store only full size.'
                   WHERE type_hash=$hash;
                   """))
        {
            updateClass.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            updateClass.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);

        int assignmentCount;
        using (SqliteCommand assignments = connection.CreateCommand())
        {
            assignments.CommandText = """
                SELECT COUNT(*) FROM object_variant_assignments a
                JOIN class_variants v ON v.id=a.variant_id
                WHERE v.type_hash=$hash AND v.variant_key='obb_size_only';
                """;
            assignments.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            assignmentCount = Convert.ToInt32(assignments.ExecuteScalar());
        }
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,objectCount,
            report.Profiles.Sum(item => item.UniqueResourceCount),
            payloadGroups.Length,assignmentCount,evidenceRows,
            $"One size-only layout; payloads: {corpusPayloadCounts}; " +
            $"PC pairs {pcEqual}/{pcCommon}; PC/PS2 pairs " +
            $"{platformEqual}/{platformCommon}; ranges: {ranges}. " +
            "Optional position/rotation are executable-confirmed but unobserved. " +
            "Mutation safety remains untested.");
    }

    private static SmoResearchClassAnalysisResult AnalyzeFog(
        string databasePath,
        SmoResearchClassReport report)
    {
        HashSet<uint?> allowedParents =
        [null,SmoClassIds.Model,SmoClassIds.Skin,SmoClassIds.ParticleSystem];
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item =>
                item.NamedObjectCount != 0 || item.MinimumSerializedSize != 31 ||
                item.MaximumSerializedSize != 31) ||
            report.Variants.Any(item =>
                item.SerializedSize != 31 ||
                item.FieldShape != "s0:f0:20|s0:end") ||
            report.Relations.Any(item =>
                item.Direction != "parent" ||
                !allowedParents.Contains(item.RelatedTypeHash)) ||
            report.Fields.Any(item =>
                item.SectionFromEnd != 0 || item.FieldType != 0 ||
                item.Occurrence != 0 || item.PayloadSize != SmoFogDecoder.PayloadSize))
        {
            throw new InvalidDataException(
                "spFog corpus shape no longer matches the validated unnamed " +
                "31-byte single-field structure.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: false);
        var observations = new List<FogObservation>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT o.file_id,o.object_index,c.corpus_key,p.platform_key,
                       f.relative_path,d.payload_preview
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                JOIN platforms p ON p.id=c.platform_id
                JOIN direct_fields d ON d.file_id=o.file_id
                                    AND d.object_index=o.object_index
                WHERE o.type_hash=$hash AND d.is_section_terminator=0
                  AND d.section_index=0 AND d.field_type=0
                  AND d.occurrence=0 AND d.payload_size=$bytes
                ORDER BY c.id,f.normalized_path,o.object_index;
                """;
            command.Parameters.AddWithValue("$hash", (long)SmoClassIds.Fog);
            command.Parameters.AddWithValue("$bytes", SmoFogDecoder.PayloadSize);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                byte[] payload = reader.GetFieldValue<byte[]>(5);
                if (!SmoFogDecoder.TryDecode(payload, out SmoFogData? fog) ||
                    fog is null || !float.IsFinite(fog.Start) ||
                    !float.IsFinite(fog.End) || !float.IsFinite(fog.Density))
                {
                    throw new InvalidDataException(
                        $"Invalid spFog payload in {reader.GetString(2)}:" +
                        $"{reader.GetString(4)}.");
                }
                observations.Add(new FogObservation(
                    reader.GetInt32(0),reader.GetInt32(1),reader.GetString(2),
                    reader.GetString(3),GetCanonicalResourcePath(reader.GetString(4)),
                    payload,fog));
            }
        }

        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected one spFog payload for each of {objectCount} objects, " +
                $"but found {observations.Count}.");
        }
        IGrouping<string,FogObservation>[] payloadGroups = observations
            .GroupBy(item => Convert.ToHexString(item.Payload), StringComparer.Ordinal)
            .OrderBy(item => item.Key, StringComparer.Ordinal)
            .ToArray();
        IGrouping<uint,FogObservation>[] typeGroups = observations
            .GroupBy(item => item.Data.Type)
            .OrderBy(item => item.Key)
            .ToArray();
        string corpusTypeDistribution = string.Join("; ", observations
            .GroupBy(item => item.CorpusKey, StringComparer.Ordinal)
            .OrderBy(item => item.Key, StringComparer.Ordinal)
            .Select(corpus => $"{corpus.Key}: " + string.Join(", ", corpus
                .GroupBy(item => item.Data.Type)
                .OrderBy(item => item.Key)
                .Select(type => $"{type.Key}={type.Count()}"))));
        string corpusPayloadCounts = string.Join(", ", observations
            .GroupBy(item => item.CorpusKey, StringComparer.Ordinal)
            .OrderBy(item => item.Key, StringComparer.Ordinal)
            .Select(corpus => $"{corpus.Key}={corpus.Select(item =>
                    Convert.ToHexString(item.Payload)).Distinct(StringComparer.Ordinal).Count()}"));
        int distinctColors = observations.Select(item => item.Data.Color).Distinct().Count();
        float minimumStart = observations.Min(item => item.Data.Start);
        float maximumStart = observations.Max(item => item.Data.Start);
        float minimumEnd = observations.Min(item => item.Data.End);
        float maximumEnd = observations.Max(item => item.Data.End);
        float[] densities = observations.Select(item => item.Data.Density)
            .Distinct().Order().ToArray();
        if (!typeGroups.Select(item => item.Key).SequenceEqual(
                [(uint)SmoFogType.None,(uint)SmoFogType.Linear]))
        {
            throw new InvalidDataException(
                "The full corpus no longer contains only the validated fog " +
                "types 0 (none) and 3 (linear).");
        }

        Dictionary<string,FogObservation> pcWorking = BuildFogPathMap(
            observations, "pc-working");
        Dictionary<string,FogObservation> pcPristine = BuildFogPathMap(
            observations, "pc-pristine");
        Dictionary<string,FogObservation> ps2Pristine = BuildFogPathMap(
            observations, "ps2-pristine");
        (int pcCommon, int pcEqual) = CompareFogPathMaps(pcWorking, pcPristine);
        (int platformCommon, int platformEqual) = CompareFogPathMaps(
            pcPristine, ps2Pristine);
        if (pcCommon != pcPristine.Count || pcEqual != pcCommon)
        {
            throw new InvalidDataException(
                "Fog payloads differ between the matching pc-working and " +
                "pc-pristine resources.");
        }
        var foglessResources = new List<(string Corpus, string Path)>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT c.corpus_key,f.relative_path
                FROM files f JOIN corpora c ON c.id=f.corpus_id
                WHERE f.extension='.smo' AND f.parse_status='ok'
                  AND NOT EXISTS(
                      SELECT 1 FROM objects o
                      WHERE o.file_id=f.id AND o.type_hash=$hash)
                ORDER BY c.id,f.normalized_path;
                """;
            command.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
                foglessResources.Add((reader.GetString(0),reader.GetString(1)));
        }
        string foglessSummary = string.Join("; ", foglessResources
            .GroupBy(item => item.Corpus, StringComparer.Ordinal)
            .OrderBy(item => item.Key, StringComparer.Ordinal)
            .Select(corpus => $"{corpus.Key}: " +
                string.Join(", ", corpus.Select(item => item.Path))));

        ExecutableIdentity pcExecutable = FindExecutable(connection, "pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection, "ps2");
        RequireAsciiTokens(pcExecutable.Path,
            "spFog","spFogSerializer",
            "DataBlockSerializer.WriteBegin(esfFog",
            "pDataStream->Write(pFog->GetDensity())");
        RequireAsciiTokens(ps2Executable.Path,
            "spFog","spFogSerializer",
            "DataBlockSerializer.WriteBegin(esfFog",
            "pDataStream->Write(pFog->GetDensity())");

        string now = DateTime.UtcNow.ToString("O");
        string layout =
            "UInt32 fog type, ARGB UInt32 color, Single start, Single end, " +
            "Single density";
        using SqliteTransaction transaction = connection.BeginTransaction();
        using (SqliteCommand clearEvidence = CreateCommand(connection, transaction, """
                   DELETE FROM evidence WHERE type_hash=$hash
                   AND evidence_kind LIKE 'class_analysis:%';
                   """))
        {
            clearEvidence.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            clearEvidence.ExecuteNonQuery();
        }
        using (SqliteCommand clearVariants = CreateCommand(connection, transaction, """
                   DELETE FROM class_variants
                   WHERE type_hash=$hash AND scope_kind='common'
                     AND scope_key='pc_ps2' AND variant_key LIKE 'fog_type_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }
        using (SqliteCommand field = CreateCommand(connection, transaction, """
                   INSERT INTO field_definitions(
                       type_hash,scope_kind,scope_key,section_from_end,field_type,
                       occurrence,semantic_key,display_name,value_kind,payload_layout,
                       editable_status,evidence_status,constraints_json,notes)
                   VALUES($hash,'common','pc_ps2',0,0,-1,'fog.parameters',
                          'Fog parameters','fog_parameters',$layout,
                          'read_only_research','confirmed_both_executables_and_full_corpus',
                          $constraints,$notes)
                   ON CONFLICT(type_hash,scope_kind,scope_key,section_from_end,
                               field_type,occurrence)
                   DO UPDATE SET semantic_key=excluded.semantic_key,
                                 display_name=excluded.display_name,
                                 value_kind=excluded.value_kind,
                                 payload_layout=excluded.payload_layout,
                                 editable_status=excluded.editable_status,
                                 evidence_status=excluded.evidence_status,
                                 constraints_json=excluded.constraints_json,
                                 notes=excluded.notes;
                   """))
        {
            field.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            field.Parameters.AddWithValue("$layout", layout);
            field.Parameters.AddWithValue("$constraints", JsonSerializer.Serialize(new
            {
                observedPayloadBytes = SmoFogDecoder.PayloadSize,
                observedTypes = typeGroups.Select(item => item.Key).ToArray(),
                distinctObservedPayloads = payloadGroups.Length,
                distinctObservedColors = distinctColors,
                observedStartRange = new[] { minimumStart, maximumStart },
                observedEndRange = new[] { minimumEnd, maximumEnd },
                observedDensities = densities,
                finiteObservedFloats = true,
                mutationStatus = "not_tested"
            }));
            field.Parameters.AddWithValue("$notes",
                "Field order and object member offsets agree in PC and PS2 code. " +
                "Types 0 and 3 are observed; editing remains disabled until runtime tests.");
            field.ExecuteNonQuery();
        }

        foreach (IGrouping<string,FogObservation> group in payloadGroups)
        {
            FogObservation sample = group.First();
            using SqliteCommand decodedFields = CreateCommand(connection, transaction, """
                UPDATE direct_fields SET semantic_key='fog.parameters',
                    payload_layout=$layout,decoded_value=$value,is_decoded=1
                WHERE is_section_terminator=0 AND field_type=0
                  AND payload_size=$bytes AND payload_preview=$payload
                  AND EXISTS(
                      SELECT 1 FROM objects o
                      WHERE o.file_id=direct_fields.file_id
                        AND o.object_index=direct_fields.object_index
                        AND o.type_hash=$hash);
                """);
            decodedFields.Parameters.AddWithValue("$layout", layout);
            decodedFields.Parameters.AddWithValue(
                "$value", JsonSerializer.Serialize(sample.Data));
            decodedFields.Parameters.AddWithValue("$bytes", SmoFogDecoder.PayloadSize);
            decodedFields.Parameters.Add("$payload", SqliteType.Blob).Value = sample.Payload;
            decodedFields.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            decodedFields.ExecuteNonQuery();
        }

        var variantIds = new Dictionary<uint,int>();
        foreach (IGrouping<uint,FogObservation> typeGroup in typeGroups)
        {
            uint type = typeGroup.Key;
            string typeName = SmoFogDecoder.GetTypeName(type);
            using SqliteCommand variant = CreateCommand(connection, transaction, """
                INSERT INTO class_variants(
                    type_hash,scope_kind,scope_key,variant_key,display_name,
                    status,discriminator_json,notes,created_utc,updated_utc)
                VALUES($hash,'common','pc_ps2',$key,$display,'confirmed',
                       $discriminator,$notes,$utc,$utc);
                SELECT last_insert_rowid();
                """);
            variant.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            variant.Parameters.AddWithValue("$key", $"fog_type_{type}_{typeName}");
            variant.Parameters.AddWithValue("$display", $"Fog type {type} ({typeName})");
            variant.Parameters.AddWithValue("$discriminator", JsonSerializer.Serialize(new
            {
                serializedSize = 31,
                fieldShape = "s0:f0:20|s0:end",
                fieldType = 0,
                payloadBytes = SmoFogDecoder.PayloadSize,
                fogType = type
            }));
            variant.Parameters.AddWithValue("$notes",
                $"Observed in {typeGroup.Count()} objects across PC and PS2; " +
                "color/distances/density are parameters, not separate subtypes.");
            variant.Parameters.AddWithValue("$utc", now);
            variantIds.Add(type, Convert.ToInt32(variant.ExecuteScalar()));
        }
        foreach ((uint type, int variantId) in variantIds)
        {
            byte[] typeBytes = BitConverter.GetBytes(type);
            using SqliteCommand assignments = CreateCommand(connection, transaction, """
                INSERT INTO object_variant_assignments(
                    file_id,object_index,variant_id,confidence,evidence)
                SELECT o.file_id,o.object_index,$variant,'confirmed',
                       'full PC+PS2 corpus: exact field shape and fog type'
                FROM objects o
                JOIN direct_fields d ON d.file_id=o.file_id
                                    AND d.object_index=o.object_index
                WHERE o.type_hash=$hash AND d.is_section_terminator=0
                  AND d.field_type=0 AND d.payload_size=$bytes
                  AND SUBSTR(d.payload_preview,1,4)=$type
                ON CONFLICT(file_id,object_index,variant_id)
                DO UPDATE SET confidence=excluded.confidence,
                              evidence=excluded.evidence;
                """);
            assignments.Parameters.AddWithValue("$variant", variantId);
            assignments.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            assignments.Parameters.AddWithValue("$bytes", SmoFogDecoder.PayloadSize);
            assignments.Parameters.Add("$type", SqliteType.Blob).Value = typeBytes;
            assignments.ExecuteNonQuery();
        }

        string typeDistribution = string.Join(", ", typeGroups.Select(item =>
            $"{item.Key} ({SmoFogDecoder.GetTypeName(item.Key)})={item.Count()}"));
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "class ID getter 0x0043B820; load 0x0043B910..0x0043BAE1; " +
            "serialize 0x0043BCF0..0x0043C074; runtime selector " +
            "0x004DF0B9..0x004DF0DC",
            "Field 0 transfers type/color/start/end/density between the stream " +
            "and object members +0x14/+0x18/+0x1C/+0x20/+0x24. Runtime skips " +
            "fog objects whose type member is zero.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "load 0x0018ADE0..0x0018AFF4; serialize 0x0018B010..0x0018B250; " +
            "class ID getter 0x0018B270",
            "MIPS code uses the same field order and object member offsets as PC.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "all spFog objects and direct fields in three corpora",
            $"{objectCount} unnamed objects in {report.Profiles.Sum(item => item.UniqueResourceCount)} " +
            $"resources; one 31-byte shape, {payloadGroups.Length} payloads; " +
            $"type distribution: {typeDistribution}; per corpus: " +
            $"{corpusTypeDistribution}; payloads: {corpusPayloadCounts}; " +
            $"colors={distinctColors}, start={FormatResearchFloat(minimumStart)}.." +
            $"{FormatResearchFloat(maximumStart)}, end={FormatResearchFloat(minimumEnd)}.." +
            $"{FormatResearchFloat(maximumEnd)}, densities=[" +
            $"{string.Join(",", densities.Select(FormatResearchFloat))}]. " +
            $"SMO without fog: {foglessSummary}.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus exact 20-byte payload",
            $"PC working/pristine: {pcEqual}/{pcCommon} matching payloads; " +
            $"PC pristine/PS2 pristine: {platformEqual}/{platformCommon} matching payloads.",
            null,now);

        using (SqliteCommand updateClass = CreateCommand(connection, transaction, """
                   UPDATE classes SET category='environment',
                       description='Fog mode, color, range and density parameters',
                       decode_status='read_only_decode',
                       notes='One common PC/PS2 layout; observed types are 0 (none) and 3 (linear).'
                   WHERE type_hash=$hash;
                   """))
        {
            updateClass.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            updateClass.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);

        int assignmentCount;
        using (SqliteCommand assignments = connection.CreateCommand())
        {
            assignments.CommandText = """
                SELECT COUNT(*) FROM object_variant_assignments a
                JOIN class_variants v ON v.id=a.variant_id
                WHERE v.type_hash=$hash AND v.variant_key LIKE 'fog_type_%';
                """;
            assignments.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            assignmentCount = Convert.ToInt32(assignments.ExecuteScalar());
        }
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,objectCount,
            report.Profiles.Sum(item => item.UniqueResourceCount),
            payloadGroups.Length,assignmentCount,evidenceRows,
            $"Types: {typeDistribution}; per corpus: {corpusTypeDistribution}; " +
            $"payloads: {corpusPayloadCounts}; PC pairs {pcEqual}/{pcCommon}; " +
            $"PC/PS2 pairs {platformEqual}/{platformCommon}; " +
            $"colors={distinctColors}, start={FormatResearchFloat(minimumStart)}.." +
            $"{FormatResearchFloat(maximumStart)}, end={FormatResearchFloat(minimumEnd)}.." +
            $"{FormatResearchFloat(maximumEnd)}, densities=[" +
            $"{string.Join(",", densities.Select(FormatResearchFloat))}]. " +
            $"Fogless: {foglessSummary}. Mutation safety remains untested.");
    }

    private static SmoResearchClassAnalysisResult AnalyzeMaterialColorController(
        string databasePath,
        SmoResearchClassReport report)
    {
        if (report.Profiles.Any(item => item.PlatformKey != "pc") ||
            report.Profiles.Any(item =>
                item.MinimumSerializedSize != 54 || item.MaximumSerializedSize != 54) ||
            report.Variants.Any(item =>
                item.SerializedSize != 54 ||
                item.FieldShape != "s0:f0:40|s0:end") ||
            report.Relations.Any(item =>
                item.Direction != "parent" ||
                item.RelatedTypeHash != SmoClassIds.MaterialData) ||
            report.Fields.Any(item =>
                item.SectionFromEnd != 0 || item.FieldType != 0 ||
                item.Occurrence != 0 || item.PayloadSize != 40))
        {
            throw new InvalidDataException(
                "spMaterialColorController corpus shape no longer matches the " +
                "validated 54-byte child-of-spMaterialData structure.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: false);
        var distinctPayloads = new List<byte[]>();
        long payloadOccurrences = 0;
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT d.payload_preview,COUNT(*)
                FROM direct_fields d
                JOIN objects o ON o.file_id=d.file_id
                              AND o.object_index=d.object_index
                WHERE o.type_hash=$hash AND d.is_section_terminator=0
                      AND d.field_type=0 AND d.payload_size=40
                GROUP BY d.payload_preview;
                """;
            command.Parameters.AddWithValue(
                "$hash", (long)SmoClassIds.MaterialColorController);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                distinctPayloads.Add(reader.GetFieldValue<byte[]>(0));
                payloadOccurrences += reader.GetInt64(1);
            }
        }
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (distinctPayloads.Count != 1 || payloadOccurrences != objectCount ||
            !SmoMaterialColorControllerDecoder.TryDecode(
                distinctPayloads[0], out SmoMaterialColorControllerData? controller) ||
            controller is null || !IsObservedControllerProfile(controller))
        {
            throw new InvalidDataException(
                "spMaterialColorController payloads are no longer the single " +
                "validated evaluator profile.");
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection, "pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection, "ps2");
        RequireAsciiTokens(pcExecutable.Path,
            "spMaterialColorController",
            "spMatColorControllerSerializer",
            "esfColorFuncEvalFrequency",
            "esfFunctionEvalYOffset");
        RequireAsciiTokens(ps2Executable.Path,
            "spMaterialColorController",
            "spMatColorControllerSerializer",
            "esfColorFuncEvalFrequency",
            "esfFunctionEvalYOffset");

        string now = DateTime.UtcNow.ToString("O");
        string payloadHash = Convert.ToHexString(SHA256.HashData(distinctPayloads[0]));
        using SqliteTransaction transaction = connection.BeginTransaction();
        using (SqliteCommand field = CreateCommand(connection, transaction, """
                   INSERT INTO field_definitions(
                       type_hash,scope_kind,scope_key,section_from_end,field_type,
                       occurrence,semantic_key,display_name,value_kind,payload_layout,
                       editable_status,evidence_status,constraints_json,notes)
                   VALUES($hash,'platform','pc',0,0,-1,$key,$display,$kind,$layout,
                          'read_only_research','confirmed_executable_and_full_corpus',
                          $constraints,$notes)
                   ON CONFLICT(type_hash,scope_kind,scope_key,section_from_end,
                               field_type,occurrence)
                   DO UPDATE SET semantic_key=excluded.semantic_key,
                                 display_name=excluded.display_name,
                                 value_kind=excluded.value_kind,
                                 payload_layout=excluded.payload_layout,
                                 editable_status=excluded.editable_status,
                                 evidence_status=excluded.evidence_status,
                                 constraints_json=excluded.constraints_json,
                                 notes=excluded.notes;
                   """))
        {
            field.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            field.Parameters.AddWithValue(
                "$key", "material_color_controller.evaluators");
            field.Parameters.AddWithValue("$display", "Material color evaluators");
            field.Parameters.AddWithValue("$kind", "nested_evaluator_sections");
            field.Parameters.AddWithValue("$layout",
                "ambient/diffuse/specular/emissive ColorFunctionalEvaluator + " +
                "alpha FunctionalEvaluator");
            field.Parameters.AddWithValue("$constraints", JsonSerializer.Serialize(new
            {
                observedPayloadBytes = 40,
                distinctObservedPayloads = 1,
                payloadSha256 = payloadHash,
                mutationStatus = "not_tested"
            }));
            field.Parameters.AddWithValue("$notes",
                "Nested field names/defaults recovered from pristine PC serializer; " +
                "all observed PC objects share one payload. Read-only until mutation test.");
            field.ExecuteNonQuery();
        }
        using (SqliteCommand decodedFields = CreateCommand(connection, transaction, """
                   UPDATE direct_fields SET
                       semantic_key='material_color_controller.evaluators',
                       payload_layout=$layout,decoded_value=$value,is_decoded=1
                   WHERE is_section_terminator=0 AND field_type=0
                     AND payload_size=40
                     AND EXISTS(
                         SELECT 1 FROM objects o
                         WHERE o.file_id=direct_fields.file_id
                           AND o.object_index=direct_fields.object_index
                           AND o.type_hash=$hash);
                   """))
        {
            decodedFields.Parameters.AddWithValue("$layout",
                "ambient/diffuse/specular/emissive ColorFunctionalEvaluator + " +
                "alpha FunctionalEvaluator");
            decodedFields.Parameters.AddWithValue(
                "$value",JsonSerializer.Serialize(controller));
            decodedFields.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            decodedFields.ExecuteNonQuery();
        }

        int variantId;
        using (SqliteCommand variant = CreateCommand(connection, transaction, """
                   INSERT INTO class_variants(
                       type_hash,scope_kind,scope_key,variant_key,display_name,
                       status,discriminator_json,notes,created_utc,updated_utc)
                   VALUES($hash,'platform','pc',$key,$display,'confirmed',
                          $discriminator,$notes,$utc,$utc)
                   ON CONFLICT(type_hash,scope_kind,scope_key,variant_key)
                   DO UPDATE SET display_name=excluded.display_name,
                                 status=excluded.status,
                                 discriminator_json=excluded.discriminator_json,
                                 notes=excluded.notes,updated_utc=excluded.updated_utc;
                   SELECT id FROM class_variants
                   WHERE type_hash=$hash AND scope_kind='platform'
                         AND scope_key='pc' AND variant_key=$key;
                   """))
        {
            variant.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            variant.Parameters.AddWithValue("$key", "shared_evaluator_profile");
            variant.Parameters.AddWithValue("$display", "Shared evaluator profile");
            variant.Parameters.AddWithValue("$discriminator", JsonSerializer.Serialize(new
            {
                serializedSize = 54,
                fieldShape = "s0:f0:40|s0:end",
                payloadSha256 = payloadHash
            }));
            variant.Parameters.AddWithValue("$notes",
                "The only observed PC form; parent is always spMaterialData. " +
                "This is an observed structural variant, not an edit-safe preset.");
            variant.Parameters.AddWithValue("$utc", now);
            variantId = Convert.ToInt32(variant.ExecuteScalar());
        }

        using (SqliteCommand assignments = CreateCommand(connection, transaction, """
                   INSERT INTO object_variant_assignments(
                       file_id,object_index,variant_id,confidence,evidence)
                   SELECT o.file_id,o.object_index,$variant,'confirmed',
                          'full PC corpus: exact size, field shape and payload hash'
                   FROM objects o
                   JOIN files f ON f.id=o.file_id
                   JOIN corpora c ON c.id=f.corpus_id
                   JOIN platforms p ON p.id=c.platform_id
                   WHERE o.type_hash=$hash AND p.platform_key='pc'
                   ON CONFLICT(file_id,object_index,variant_id)
                   DO UPDATE SET confidence=excluded.confidence,
                                 evidence=excluded.evidence;
                   """))
        {
            assignments.Parameters.AddWithValue("$variant", variantId);
            assignments.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            assignments.ExecuteNonQuery();
        }

        using (SqliteCommand clear = CreateCommand(connection, transaction, """
                   DELETE FROM evidence WHERE type_hash=$hash
                   AND evidence_kind LIKE 'class_analysis:%';
                   """))
        {
            clear.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            clear.ExecuteNonQuery();
        }
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "VA 0x004412E0..0x00441A73; CFESerializer 0x0047E320/0x0047E850; " +
            "FESerializer 0x0047EE00/0x0047F220",
            "Serializer order is ambient, diffuse, specular, emissive, alpha; " +
            "nested field names and defaults recovered from code and diagnostics.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "class string at file offset 0x3451B0; serializer diagnostics " +
            "at 0x34B8C0..0x34BD10",
            "PS2 executable registers the class and contains the same evaluator " +
            "serializer vocabulary even though no PS2 corpus object uses it.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "objects/direct_fields grouped across pc-pristine and pc-working",
            $"{objectCount} PC objects in {report.Resources.Count / report.Profiles.Count} " +
            "resources; all are unnamed 54-byte children of spMaterialData and " +
            $"share one 40-byte payload (SHA-256 {payloadHash}).",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:negative_corpus",
            "ps2-pristine","all 317 unique SMO / 160387 objects",
            "No serialized spMaterialColorController instance is present on PS2.",
            null,now);

        using (SqliteCommand updateClass = CreateCommand(connection, transaction, """
                   UPDATE classes SET
                       category='material_controller',
                       description='Controls ambient, diffuse, specular, emissive and alpha evaluators',
                       decode_status='read_only_decode',
                       notes='PC-only in the observed corpus; PS2 executable supports the class but PS2 resources omit it.'
                   WHERE type_hash=$hash;
                   """))
        {
            updateClass.Parameters.AddWithValue("$hash", (long)report.TypeHash);
            updateClass.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);

        int assignmentCount;
        using (SqliteCommand assignments = connection.CreateCommand())
        {
            assignments.CommandText = """
                SELECT COUNT(*) FROM object_variant_assignments a
                JOIN objects o ON o.file_id=a.file_id
                              AND o.object_index=a.object_index
                WHERE o.type_hash=$hash;
                """;
            assignments.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            assignmentCount = Convert.ToInt32(assignments.ExecuteScalar());
        }
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,objectCount,
            report.Profiles.Sum(item => item.UniqueResourceCount),
            distinctPayloads.Count,assignmentCount,evidenceRows,
            "One PC structural/payload variant; PS2 executable support confirmed, " +
            "but no PS2 serialized instances. Mutation safety remains untested.");
    }

    private static SmoResearchClassAnalysisResult AnalyzeLightData(
        string databasePath,
        SmoResearchClassReport report)
    {
        static int ExpectedPayloadSize(int fieldType) => fieldType switch
        {
            1 or 3 or 8 => 1,
            >= 0 and <= 7 => 4,
            _ => -1
        };

        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item => item.NamedObjectCount != item.UniqueObjectCount) ||
            report.Relations.Any(item => item.Direction switch
            {
                "parent" => item.RelatedTypeHash is null or SmoClassIds.Node,
                "child" => item.RelatedTypeHash == SmoClassIds.RenderNode,
                _ => false
            } == false) ||
            report.Fields.Where(item => item.SectionFromEnd == 0).Any(item =>
                ExpectedPayloadSize(item.FieldType) != item.PayloadSize))
        {
            throw new InvalidDataException(
                "spLightData corpus shape no longer matches the validated named " +
                "light structure with a final nine-field optional section.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        var builders = new Dictionary<(int FileId,int ObjectIndex),LightObservationBuilder>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT o.file_id,o.object_index,o.object_id,c.corpus_key,
                       p.platform_key,f.relative_path,o.name,o.serialized_size,
                       o.field_shape,d.field_index,d.field_type,d.occurrence,
                       d.raw_header,d.header_size,d.payload_size,d.payload_preview,
                       d.is_section_terminator
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                JOIN platforms p ON p.id=c.platform_id
                JOIN direct_fields d ON d.file_id=o.file_id
                                    AND d.object_index=o.object_index
                WHERE o.type_hash=$hash
                  AND d.section_index=(
                      SELECT MAX(last.section_index) FROM direct_fields last
                      WHERE last.file_id=o.file_id
                        AND last.object_index=o.object_index)
                ORDER BY c.id,f.normalized_path,o.object_index,d.field_index;
                """;
            command.Parameters.AddWithValue("$hash",(long)SmoClassIds.LightData);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                var key = (reader.GetInt32(0),reader.GetInt32(1));
                if (!builders.TryGetValue(key,out LightObservationBuilder? builder))
                {
                    builder = new LightObservationBuilder(
                        key.Item1,key.Item2,checked((uint)reader.GetInt64(2)),
                        reader.GetString(3),reader.GetString(4),
                        GetCanonicalResourcePath(reader.GetString(5)),
                        reader.GetString(6).TrimEnd('\0'),reader.GetInt32(7),
                        reader.GetString(8));
                    builders.Add(key,builder);
                }

                int fieldIndex = reader.GetInt32(9);
                int fieldType = reader.GetInt32(10);
                int occurrence = reader.GetInt32(11);
                byte rawHeader = checked((byte)reader.GetInt32(12));
                int headerSize = reader.GetInt32(13);
                uint payloadSize = checked((uint)reader.GetInt64(14));
                byte[] payload = reader.GetFieldValue<byte[]>(15);
                if (payload.Length != payloadSize)
                {
                    throw new InvalidDataException(
                        $"Incomplete spLightData field payload in {builder.CorpusKey}:" +
                        $"{builder.CanonicalPath} object {builder.ObjectIndex}.");
                }
                builder.DecoderFields.Add(new SmoObjectField(
                    builder.ObjectIndex,builder.ObjectId,fieldType,occurrence,
                    rawHeader,(SmoDataBlockSizeCode)(rawHeader >> 5),headerSize,
                    payloadSize,0,0,0,0,payload));
                if (!reader.GetBoolean(16))
                {
                    builder.SerializedFields.Add(new LightFieldObservation(
                        fieldIndex,fieldType,payload));
                }
            }
        }

        var observations = new List<LightObservation>(builders.Count);
        foreach (LightObservationBuilder builder in builders.Values)
        {
            if (!SmoLightDataDecoder.TryDecode(
                    builder.DecoderFields,out SmoLightData? decoded,out string lightError) ||
                decoded is null || decoded.Type > (uint)SmoLightType.Ambient ||
                !float.IsFinite(decoded.Intensity) || decoded.Intensity <= 0.0f ||
                !float.IsFinite(decoded.Range) || decoded.Range <= 0.0f ||
                !float.IsFinite(decoded.HotspotAngle) ||
                !float.IsFinite(decoded.FalloffAngle) ||
                decoded.ProjectShadowVolume || decoded.AttenuationEnabled ||
                !decoded.Enabled)
            {
                throw new InvalidDataException(
                    $"Invalid spLightData values in {builder.CorpusKey}:" +
                    $"{builder.CanonicalPath} object {builder.ObjectIndex}: {lightError}");
            }

            string signature = string.Join("|",builder.SerializedFields.Select(field =>
                $"{field.FieldType}:{Convert.ToHexString(field.Payload)}"));
            observations.Add(new LightObservation(
                builder.FileId,builder.ObjectIndex,builder.CorpusKey,
                builder.PlatformKey,builder.CanonicalPath,builder.ObjectName,
                builder.SerializedSize,builder.FieldShape,
                builder.SerializedFields.AsReadOnly(),decoded,signature));
        }

        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (observations.Count != objectCount ||
            observations.Any(item => item.Data.Type switch
            {
                (uint)SmoLightType.Ambient => item.ObjectName != "Ambient01",
                (uint)SmoLightType.Directional or (uint)SmoLightType.Point =>
                    item.ObjectName != "Light",
                _ => false
            }))
        {
            throw new InvalidDataException(
                "spLightData object names no longer match the validated type/name " +
                "correlation (Light versus Ambient01).");
        }

        int FieldOccurrences(int fieldType) => observations.Count(item =>
            item.Data.IsFieldSerialized(fieldType));
        float[] SerializedSingles(int fieldType) => observations
            .SelectMany(item => item.Fields.Where(field => field.FieldType == fieldType))
            .Select(field => BitConverter.Int32BitsToSingle(
                BinaryPrimitives.ReadInt32LittleEndian(field.Payload)))
            .ToArray();
        float[] intensityValues = SerializedSingles(4);
        float[] rangeValues = SerializedSingles(5);
        float[] hotspotValues = SerializedSingles(6);
        float[] falloffValues = SerializedSingles(7);
        int distinctColors = observations.Select(item => item.Data.ColorRgba).Distinct().Count();
        IGrouping<string,LightObservation>[] payloadGroups = observations
            .GroupBy(item => item.SerializedSignature,StringComparer.Ordinal)
            .OrderBy(group => group.Key,StringComparer.Ordinal)
            .ToArray();

        string TypeDistribution(string corpusKey) => string.Join(",",observations
            .Where(item => item.CorpusKey == corpusKey)
            .GroupBy(item => item.Data.Type).OrderBy(group => group.Key)
            .Select(group =>
                $"{SmoLightDataDecoder.GetTypeName(group.Key)}={group.Count()}"));
        string typeDistribution = string.Join("; ",report.Profiles.Select(profile =>
            $"{profile.CorpusKey}[{TypeDistribution(profile.CorpusKey)}]"));

        Dictionary<string,string> BuildPathMap(string corpusKey) => observations
            .Where(item => item.CorpusKey == corpusKey)
            .GroupBy(item => item.CanonicalPath,StringComparer.OrdinalIgnoreCase)
            .ToDictionary(
                group => group.Key,
                group => string.Join("|",group
                    .Select(item => item.SerializedSignature)
                    .Order(StringComparer.Ordinal)),
                StringComparer.OrdinalIgnoreCase);
        Dictionary<string,string> pcWorking = BuildPathMap("pc-working");
        Dictionary<string,string> pcPristine = BuildPathMap("pc-pristine");
        Dictionary<string,string> ps2Pristine = BuildPathMap("ps2-pristine");
        (int pcCommon,int pcEqual) = ComparePayloadPathMaps(pcWorking,pcPristine);
        (int platformCommon,int platformEqual) = ComparePayloadPathMaps(
            pcPristine,ps2Pristine);
        string[] platformDifferences = pcPristine.Keys
            .Intersect(ps2Pristine.Keys,StringComparer.OrdinalIgnoreCase)
            .Where(path => !StringComparer.Ordinal.Equals(
                pcPristine[path],ps2Pristine[path]))
            .Order(StringComparer.OrdinalIgnoreCase)
            .ToArray();
        if (pcCommon != pcPristine.Count || pcEqual != pcCommon)
        {
            throw new InvalidDataException(
                "spLightData fields differ between matching pc-working and " +
                "pc-pristine resources.");
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spLightData","spLightDataSerializer","esfLightType",
            "esfLightProjectShadow","esfLightColor","esfLightAttenuation",
            "asfLightIntensity","esfLightRange","esfLightAngle",
            "esfLightFalloff","esfLightEnabled"
        ];
        RequireAsciiTokens(pcExecutable.Path,executableTokens);
        RequireAsciiTokens(ps2Executable.Path,executableTokens);

        string now = DateTime.UtcNow.ToString("O");
        using SqliteTransaction transaction = connection.BeginTransaction();
        using (SqliteCommand clearEvidence = CreateCommand(connection,transaction,"""
                   DELETE FROM evidence WHERE type_hash=$hash
                   AND evidence_kind LIKE 'class_analysis:%';
                   """))
        {
            clearEvidence.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearEvidence.ExecuteNonQuery();
        }
        using (SqliteCommand clearAssignments = CreateCommand(connection,transaction,"""
                   DELETE FROM object_variant_assignments
                   WHERE variant_id IN(
                       SELECT id FROM class_variants
                       WHERE type_hash=$hash AND scope_kind='common'
                         AND scope_key='pc_ps2' AND variant_key LIKE 'light_%');
                   """))
        {
            clearAssignments.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearAssignments.ExecuteNonQuery();
        }
        using (SqliteCommand clearVariants = CreateCommand(connection,transaction,"""
                   DELETE FROM class_variants
                   WHERE type_hash=$hash AND scope_kind='common'
                     AND scope_key='pc_ps2' AND variant_key LIKE 'light_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        string FieldSemantic(int fieldType) => fieldType switch
        {
            0 => "light.type", 1 => "light.project_shadow",
            2 => "light.color", 3 => "light.attenuation",
            4 => "light.intensity", 5 => "light.range",
            6 => "light.hotspot_angle", 7 => "light.falloff_angle",
            8 => "light.enabled", _ => throw new ArgumentOutOfRangeException()
        };
        string FieldDisplay(int fieldType) => fieldType switch
        {
            0 => "Light type", 1 => "Project shadow volume", 2 => "Light color",
            3 => "Attenuation enabled", 4 => "Intensity", 5 => "Range",
            6 => "Hotspot angle", 7 => "Falloff angle", 8 => "Enabled",
            _ => throw new ArgumentOutOfRangeException()
        };
        string FieldKind(int fieldType) => fieldType switch
        {
            0 => "enum_u32", 1 or 3 or 8 => "boolean",
            2 => "argb_u32", >= 4 and <= 7 => "single",
            _ => throw new ArgumentOutOfRangeException()
        };
        string FieldLayout(int fieldType) => fieldType switch
        {
            0 => "UInt32: 0 directional, 1 point, 2 spot, 3 ambient",
            1 or 3 or 8 => "Boolean byte",
            2 => "ARGB UInt32 (little-endian bytes BGRA)",
            4 or 5 => "IEEE-754 Single",
            6 or 7 => "IEEE-754 Single radians (raw, not normalized)",
            _ => throw new ArgumentOutOfRangeException()
        };
        string FieldDefault(int fieldType) => fieldType switch
        {
            0 => "0 (directional)", 1 => "false", 2 => "0xFFFFFFFF",
            3 => "false", 4 => "1", 5 => "200", 6 or 7 => "0",
            8 => "false", _ => throw new ArgumentOutOfRangeException()
        };
        string FieldEvidence(int fieldType) => fieldType is 1 or 3
            ? "confirmed_both_executables_unobserved_in_corpus"
            : "confirmed_both_executables_and_full_corpus";

        for (int fieldType = 0; fieldType <= 8; fieldType++)
        {
            string constraints = JsonSerializer.Serialize(new
            {
                payloadBytes = ExpectedPayloadSize(fieldType),
                executableDefault = FieldDefault(fieldType),
                observedOccurrences = FieldOccurrences(fieldType),
                mutationStatus = "not_tested"
            });
            string notes = fieldType switch
            {
                0 => "PC visualization switch proves 0=directional, 1=point and " +
                     "2=spot; all type-3 objects are named Ambient01 on both platforms.",
                1 => "Optional false-default field; no corpus object enables shadow volumes.",
                2 => $"Optional white-default ARGB color; {distinctColors} effective colors observed.",
                3 => "Optional false-default field; no corpus object enables attenuation.",
                4 => $"Optional 1.0-default intensity; serialized range " +
                     $"{FormatResearchFloat(intensityValues.Min())}.." +
                     $"{FormatResearchFloat(intensityValues.Max())}.",
                5 => $"Optional 200.0-default world-space range; serialized range " +
                     $"{FormatResearchFloat(rangeValues.Min())}.." +
                     $"{FormatResearchFloat(rangeValues.Max())}.",
                6 => $"Optional zero-default hotspot angle; raw observed radians " +
                     $"{FormatResearchFloat(hotspotValues.Min())}.." +
                     $"{FormatResearchFloat(hotspotValues.Max())}.",
                7 => $"Optional zero-default falloff angle; raw observed radians " +
                     $"{FormatResearchFloat(falloffValues.Min())}.." +
                     $"{FormatResearchFloat(falloffValues.Max())}.",
                8 => "Optional false-default enabled flag; every corpus object explicitly stores true.",
                _ => string.Empty
            };
            using SqliteCommand field = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',0,$type,-1,$semantic,$display,
                       $kind,$layout,'read_only_research',$evidence,$constraints,$notes)
                ON CONFLICT(type_hash,scope_kind,scope_key,section_from_end,
                            field_type,occurrence)
                DO UPDATE SET semantic_key=excluded.semantic_key,
                              display_name=excluded.display_name,
                              value_kind=excluded.value_kind,
                              payload_layout=excluded.payload_layout,
                              editable_status=excluded.editable_status,
                              evidence_status=excluded.evidence_status,
                              constraints_json=excluded.constraints_json,
                              notes=excluded.notes;
                """);
            field.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            field.Parameters.AddWithValue("$type",fieldType);
            field.Parameters.AddWithValue("$semantic",FieldSemantic(fieldType));
            field.Parameters.AddWithValue("$display",FieldDisplay(fieldType));
            field.Parameters.AddWithValue("$kind",FieldKind(fieldType));
            field.Parameters.AddWithValue("$layout",FieldLayout(fieldType));
            field.Parameters.AddWithValue("$evidence",FieldEvidence(fieldType));
            field.Parameters.AddWithValue("$constraints",constraints);
            field.Parameters.AddWithValue("$notes",notes);
            field.ExecuteNonQuery();
        }

        static string DecodeFieldValue(LightFieldObservation field)
        {
            ReadOnlySpan<byte> payload = field.Payload;
            return field.FieldType switch
            {
                0 => $"{BinaryPrimitives.ReadUInt32LittleEndian(payload)} (" +
                     $"{SmoLightDataDecoder.GetTypeName(BinaryPrimitives.ReadUInt32LittleEndian(payload))})",
                1 or 3 or 8 => payload[0] == 0 ? "false" : "true",
                2 => $"#{BinaryPrimitives.ReadUInt32LittleEndian(payload):X8}",
                >= 4 and <= 7 => FormatResearchFloat(
                    BitConverter.Int32BitsToSingle(
                        BinaryPrimitives.ReadInt32LittleEndian(payload))),
                _ => throw new InvalidDataException("Unexpected light field type.")
            };
        }
        using (SqliteCommand decodedField = CreateCommand(connection,transaction,"""
                   UPDATE direct_fields SET semantic_key=$key,payload_layout=$layout,
                       decoded_value=$value,is_decoded=1
                   WHERE file_id=$file AND object_index=$object AND field_index=$field;
                   """))
        {
            decodedField.Parameters.Add("$key",SqliteType.Text);
            decodedField.Parameters.Add("$layout",SqliteType.Text);
            decodedField.Parameters.Add("$value",SqliteType.Text);
            decodedField.Parameters.Add("$file",SqliteType.Integer);
            decodedField.Parameters.Add("$object",SqliteType.Integer);
            decodedField.Parameters.Add("$field",SqliteType.Integer);
            foreach (LightObservation observation in observations)
            {
                foreach (LightFieldObservation field in observation.Fields)
                {
                    decodedField.Parameters["$key"].Value = FieldSemantic(field.FieldType);
                    decodedField.Parameters["$layout"].Value = FieldLayout(field.FieldType);
                    decodedField.Parameters["$value"].Value = DecodeFieldValue(field);
                    decodedField.Parameters["$file"].Value = observation.FileId;
                    decodedField.Parameters["$object"].Value = observation.ObjectIndex;
                    decodedField.Parameters["$field"].Value = field.FieldIndex;
                    decodedField.ExecuteNonQuery();
                }
            }
        }

        (uint Type,string Key,string Display,string Status,string Notes)[] variants =
        [
            ((uint)SmoLightType.Directional,"light_directional","Directional light",
                "confirmed","Executable type 0; omission of field 0 selects this default."),
            ((uint)SmoLightType.Point,"light_point","Point light","confirmed",
                "Executable type 1; PC visualization uses the sphere model."),
            ((uint)SmoLightType.Spot,"light_spot","Spot light",
                "executable_confirmed_unobserved",
                "Executable type 2; PC visualization uses the outer-cone model; no corpus object uses it."),
            ((uint)SmoLightType.Ambient,"light_ambient","Ambient light","confirmed",
                "Type 3 is used exclusively by objects named Ambient01 in all corpora.")
        ];
        var variantIds = new Dictionary<uint,int>();
        foreach (var definition in variants)
        {
            int count = observations.Count(item => item.Data.Type == definition.Type);
            using SqliteCommand variant = CreateCommand(connection,transaction,"""
                       INSERT INTO class_variants(
                           type_hash,scope_kind,scope_key,variant_key,display_name,
                           status,discriminator_json,notes,created_utc,updated_utc)
                       VALUES($hash,'common','pc_ps2',$key,$display,$status,
                              $discriminator,$notes,$utc,$utc);
                       SELECT last_insert_rowid();
                       """);
            variant.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            variant.Parameters.AddWithValue("$key",definition.Key);
            variant.Parameters.AddWithValue("$display",definition.Display);
            variant.Parameters.AddWithValue("$status",definition.Status);
            variant.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
            {
                field0Value = definition.Type,
                field0MayBeOmitted = definition.Type == SmoLightDataDecoder.DefaultType,
                observedObjects = count
            }));
            variant.Parameters.AddWithValue("$notes",definition.Notes);
            variant.Parameters.AddWithValue("$utc",now);
            variantIds.Add(definition.Type,Convert.ToInt32(variant.ExecuteScalar()));
        }
        using (SqliteCommand assignment = CreateCommand(connection,transaction,"""
                   INSERT INTO object_variant_assignments(
                       file_id,object_index,variant_id,confidence,evidence)
                   VALUES($file,$object,$variant,'confirmed',$evidence);
                   """))
        {
            assignment.Parameters.Add("$file",SqliteType.Integer);
            assignment.Parameters.Add("$object",SqliteType.Integer);
            assignment.Parameters.Add("$variant",SqliteType.Integer);
            assignment.Parameters.Add("$evidence",SqliteType.Text);
            foreach (LightObservation observation in observations)
            {
                assignment.Parameters["$file"].Value = observation.FileId;
                assignment.Parameters["$object"].Value = observation.ObjectIndex;
                assignment.Parameters["$variant"].Value = variantIds[observation.Data.Type];
                assignment.Parameters["$evidence"].Value =
                    $"decoded effective light type {observation.Data.Type} " +
                    $"({SmoLightDataDecoder.GetTypeName(observation.Data.Type)})";
                assignment.ExecuteNonQuery();
            }
        }

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "serialize VA 0x004401F3..0x004405D4; type visualization switch " +
            "VA 0x004317D0..0x004319B9; diagnostics VA 0x006E19A0..0x006E1E48",
            "Fields 0..8 and defaults are explicit. Field 4 is Intensity, not " +
            "attenuation. The visualization switch maps 0 directional, 1 point " +
            "and 2 spot.",pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "serialize VA 0x00191FC0..0x001926E0; diagnostics " +
            "VA 0x004510A0..0x00451480 and 0x00458BB0..0x00459220",
            "PS2 writes the same nine IDs with the same bool/UInt32/Single " +
            "payloads and defaults: type 0, white color, intensity 1, range 200, " +
            "zero angles; the writer omits false flags, while the light constructor " +
            "initializes Enabled to true.",ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "all final serializer sections for spLightData in three corpora",
            $"Decoded {objectCount} named objects in " +
            $"{report.Profiles.Sum(item => item.UniqueResourceCount)} corpus-resource rows; " +
            $"{payloadGroups.Length} distinct sparse payloads; types: {typeDistribution}; " +
            $"field occurrences 0..8=[{string.Join(',',Enumerable.Range(0,9).Select(FieldOccurrences))}].",
            null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus sorted multiset of own-section field payloads",
            $"PC working/pristine: {pcEqual}/{pcCommon} equal resources; " +
            $"PC pristine/PS2 pristine: {platformEqual}/{platformCommon} equal resources; " +
            $"different paths=[{string.Join(',',platformDifferences)}].",
            null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='light',
                       description='Directional, point, spot or ambient light data',
                       decode_status='read_only_decode',
                       notes='Common PC/PS2 nine-field optional serializer; type variants and all defaults decoded.'
                   WHERE type_hash=$hash;
                   """))
        {
            updateClass.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            updateClass.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);

        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,objectCount,
            report.Profiles.Sum(item => item.UniqueResourceCount),
            payloadGroups.Length,observations.Count,evidenceRows,
            $"Four engine light types: directional, point, spot and ambient; " +
            $"spot is executable-confirmed but unobserved. Types: {typeDistribution}. " +
            $"All ten serialized sizes are sparse default-omission shapes. " +
            $"PC pairs {pcEqual}/{pcCommon}; PC/PS2 pairs " +
            $"{platformEqual}/{platformCommon}; differing paths: " +
            $"[{string.Join(',',platformDifferences)}]. Mutation safety remains untested.");
    }

    private static SmoResearchClassAnalysisResult AnalyzeUvController(
        string databasePath,
        SmoResearchClassReport report)
    {
        int[] serializedSizes = [78,83,88,93,98,103,108,113,118,128,133];
        int[] payloadSizes = [64,69,74,79,84,89,94,99,104,114,119];
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item =>
                item.NamedObjectCount != 0 || item.MinimumSerializedSize != 78 ||
                item.MaximumSerializedSize != 133) ||
            !report.Variants.Select(item => item.SerializedSize)
                .Distinct().Order().SequenceEqual(serializedSizes) ||
            report.Variants.Any(item =>
                item.FieldShape != $"s0:f0:{item.SerializedSize - 14}|s0:end") ||
            report.Relations.Any(item =>
                item.Direction != "parent" ||
                item.RelatedTypeHash != SmoClassIds.MaterialData) ||
            report.Fields.Any(item =>
                item.SectionFromEnd != 0 || item.FieldType != 0 ||
                item.Occurrence != 0 || !payloadSizes.Contains(item.PayloadSize)))
        {
            throw new InvalidDataException(
                "spUVController corpus shape no longer matches the validated " +
                "unnamed child-of-spMaterialData structure.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        var locations = new List<UvPayloadLocation>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT o.file_id,o.object_index,d.field_index,c.corpus_key,
                       p.platform_key,f.relative_path,o.serialized_size,
                       d.payload_size,d.absolute_payload_offset,c.source_kind,
                       c.source_root,co.relative_path,fo.physical_path,
                       COALESCE(fo.byte_offset,0)
                FROM objects o
                JOIN files f ON f.id=o.file_id
                JOIN corpora c ON c.id=f.corpus_id
                JOIN platforms p ON p.id=c.platform_id
                JOIN direct_fields d ON d.file_id=o.file_id
                                    AND d.object_index=o.object_index
                JOIN file_occurrences fo ON fo.file_id=f.id
                     AND fo.id=(SELECT MIN(fo2.id) FROM file_occurrences fo2
                                WHERE fo2.file_id=f.id)
                JOIN containers co ON co.id=fo.container_id
                WHERE o.type_hash=$hash AND d.is_section_terminator=0
                  AND d.section_index=0 AND d.field_type=0 AND d.occurrence=0
                ORDER BY c.id,f.normalized_path,o.object_index;
                """;
            command.Parameters.AddWithValue("$hash",(long)SmoClassIds.UvController);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
            {
                locations.Add(new UvPayloadLocation(
                    reader.GetInt32(0),reader.GetInt32(1),reader.GetInt32(2),
                    reader.GetString(3),reader.GetString(4),reader.GetString(5),
                    reader.GetInt32(6),reader.GetInt32(7),reader.GetInt32(8),
                    reader.GetString(9),reader.GetString(10),reader.GetString(11),
                    reader.GetString(12),reader.GetInt64(13)));
            }
        }

        var observations = new List<UvControllerObservation>(locations.Count);
        var streams = new Dictionary<string,FileStream>(
            StringComparer.OrdinalIgnoreCase);
        try
        {
            foreach (UvPayloadLocation location in locations)
            {
                string relativeSource = location.SourceKind == "directory"
                    ? location.PhysicalPath
                    : location.ContainerPath;
                string sourcePath = Path.GetFullPath(Path.Combine(
                    location.SourceRoot,
                    relativeSource.Replace('/',Path.DirectorySeparatorChar)));
                if (!streams.TryGetValue(sourcePath,out FileStream? stream))
                {
                    stream = new FileStream(
                        sourcePath,FileMode.Open,FileAccess.Read,FileShare.Read);
                    streams.Add(sourcePath,stream);
                }
                long entryOffset = location.SourceKind == "pck"
                    ? location.EntryOffset
                    : 0;
                stream.Position = checked(entryOffset + location.PayloadOffset);
                byte[] payload = new byte[location.PayloadSize];
                stream.ReadExactly(payload);
                if (!SmoUvControllerDecoder.TryDecode(
                        payload,out SmoUvControllerData? decoded) ||
                    decoded is null || !IsFiniteUvController(decoded))
                {
                    throw new InvalidDataException(
                        $"Invalid spUVController payload in {location.CorpusKey}:" +
                        $"{location.RelativePath} object {location.ObjectIndex}.");
                }
                observations.Add(new UvControllerObservation(
                    location.FileId,location.ObjectIndex,location.FieldIndex,
                    location.CorpusKey,location.PlatformKey,
                    GetCanonicalResourcePath(location.RelativePath),
                    location.SerializedSize,payload,decoded));
            }
        }
        finally
        {
            foreach (FileStream stream in streams.Values)
                stream.Dispose();
        }

        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected one decoded UV payload for each of {objectCount} " +
                $"objects, but found {observations.Count}.");
        }

        IGrouping<string,UvControllerObservation>[] payloadGroups = observations
            .GroupBy(item => Convert.ToHexString(SHA256.HashData(item.Payload)),
                StringComparer.Ordinal)
            .OrderBy(item => item.Key,StringComparer.Ordinal)
            .ToArray();
        Dictionary<string,string> BuildPathMap(string corpus) => observations
            .Where(item => item.CorpusKey == corpus)
            .GroupBy(item => item.CanonicalPath,StringComparer.OrdinalIgnoreCase)
            .ToDictionary(
                group => group.Key,
                group => string.Join("|",group.Select(item =>
                        Convert.ToHexString(SHA256.HashData(item.Payload)))
                    .Order(StringComparer.Ordinal)),
                StringComparer.OrdinalIgnoreCase);
        Dictionary<string,string> pcWorking = BuildPathMap("pc-working");
        Dictionary<string,string> pcPristine = BuildPathMap("pc-pristine");
        Dictionary<string,string> ps2Pristine = BuildPathMap("ps2-pristine");
        (int pcCommon,int pcEqual) = ComparePayloadPathMaps(pcWorking,pcPristine);
        (int platformCommon,int platformEqual) = ComparePayloadPathMaps(
            pcPristine,ps2Pristine);
        string[] platformDifferences = pcPristine.Keys
            .Intersect(ps2Pristine.Keys,StringComparer.OrdinalIgnoreCase)
            .Where(path => !StringComparer.Ordinal.Equals(
                pcPristine[path],ps2Pristine[path]))
            .Order(StringComparer.OrdinalIgnoreCase)
            .ToArray();
        if (pcCommon != pcPristine.Count || pcEqual != pcCommon)
        {
            throw new InvalidDataException(
                "UV-controller payload multisets differ between matching " +
                "pc-working and pc-pristine resources.");
        }

        SmoFunctionalEvaluatorData[] Evaluators(SmoUvControllerData item) =>
        [
            item.TranslationX,item.TranslationY,item.TranslationZ,
            item.ScaleX,item.ScaleY,item.ScaleZ,item.Rotation
        ];
        string[] evaluatorNames =
        ["translationX","translationY","translationZ",
         "scaleX","scaleY","scaleZ","rotation"];
        var functionTypes = new Dictionary<string,uint[]>();
        var functionTypeDistribution = new Dictionary<string,string>();
        for (int index = 0; index < evaluatorNames.Length; index++)
        {
            int evaluatorIndex = index;
            functionTypes[evaluatorNames[index]] = observations
                .Select(item => Evaluators(item.Data)[evaluatorIndex].FunctionType)
                .Distinct().Order().ToArray();
            functionTypeDistribution[evaluatorNames[index]] = string.Join(",",
                observations
                    .GroupBy(item =>
                        Evaluators(item.Data)[evaluatorIndex].FunctionType)
                    .OrderBy(group => group.Key)
                    .Select(group => $"{group.Key}:{group.Count()}"));
        }
        int distinctPivots = observations.Select(item => item.Data.UvPivot)
            .Distinct().Count();
        int distinctAxes = observations.Select(item => item.Data.RotationAxis)
            .Distinct().Count();
        static string FormatVector(Vector3 value) =>
            $"({FormatResearchFloat(value.X)},{FormatResearchFloat(value.Y)}," +
            $"{FormatResearchFloat(value.Z)})";
        string pivotSummary = string.Join(",",observations
            .Select(item => item.Data.UvPivot).Distinct()
            .OrderBy(item => item.X).ThenBy(item => item.Y).ThenBy(item => item.Z)
            .Select(FormatVector));
        string axisSummary = string.Join(",",observations
            .Select(item => item.Data.RotationAxis).Distinct()
            .OrderBy(item => item.X).ThenBy(item => item.Y).ThenBy(item => item.Z)
            .Select(FormatVector));
        string shapeDistribution = string.Join(", ",observations
            .GroupBy(item => item.SerializedSize).OrderBy(item => item.Key)
            .Select(group => $"{group.Key}={group.Count()}"));

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spUVController","spUVControllerSerializer","spTransFunctionEval",
            "spTransFunctionEvalSerializer","GetTranslationFunction",
            "GetScaleFunction","GetRotationFunction","GetUVPivot",
            "GetRotationAxis"
        ];
        RequireAsciiTokens(pcExecutable.Path,executableTokens);
        RequireAsciiTokens(ps2Executable.Path,executableTokens);

        string layout = "spTransFunctionEval: FunctionalEvaluator translation " +
            "X/Y/Z, scale X/Y/Z, rotation; Vector3 UV pivot; Vector3 rotation axis";
        string now = DateTime.UtcNow.ToString("O");
        using SqliteTransaction transaction = connection.BeginTransaction();
        using (SqliteCommand clearEvidence = CreateCommand(connection,transaction,"""
                   DELETE FROM evidence WHERE type_hash=$hash
                   AND evidence_kind LIKE 'class_analysis:%';
                   """))
        {
            clearEvidence.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearEvidence.ExecuteNonQuery();
        }
        using (SqliteCommand field = CreateCommand(connection,transaction,"""
                   INSERT INTO field_definitions(
                       type_hash,scope_kind,scope_key,section_from_end,field_type,
                       occurrence,semantic_key,display_name,value_kind,payload_layout,
                       editable_status,evidence_status,constraints_json,notes)
                   VALUES($hash,'common','pc_ps2',0,0,-1,$key,$display,$kind,$layout,
                          'read_only_research','confirmed_both_executables_and_full_corpus',
                          $constraints,$notes)
                   ON CONFLICT(type_hash,scope_kind,scope_key,section_from_end,
                               field_type,occurrence)
                   DO UPDATE SET semantic_key=excluded.semantic_key,
                                 display_name=excluded.display_name,
                                 value_kind=excluded.value_kind,
                                 payload_layout=excluded.payload_layout,
                                 editable_status=excluded.editable_status,
                                 evidence_status=excluded.evidence_status,
                                 constraints_json=excluded.constraints_json,
                                 notes=excluded.notes;
                   """))
        {
            field.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            field.Parameters.AddWithValue(
                "$key","uv_controller.transform_evaluators");
            field.Parameters.AddWithValue("$display","UV transform evaluators");
            field.Parameters.AddWithValue("$kind","nested_transform_evaluators");
            field.Parameters.AddWithValue("$layout",layout);
            field.Parameters.AddWithValue("$constraints",JsonSerializer.Serialize(new
            {
                observedPayloadSizes = payloadSizes,
                observedSerializedSizes = serializedSizes,
                distinctObservedPayloads = payloadGroups.Length,
                sparseEncodingShapes = serializedSizes.Length,
                functionTypes,
                functionTypeDistribution,
                distinctUvPivots = distinctPivots,
                uvPivots = pivotSummary,
                distinctRotationAxes = distinctAxes,
                rotationAxes = axisSummary,
                mutationStatus = "not_tested"
            }));
            field.Parameters.AddWithValue("$notes",
                "All eleven byte sizes are sparse encodings of the same transform " +
                "layout; omitted evaluator fields take serializer defaults.");
            field.ExecuteNonQuery();
        }

        using (SqliteCommand decoded = CreateCommand(connection,transaction,"""
                   UPDATE direct_fields SET semantic_key=$key,payload_layout=$layout,
                       decoded_value=$value,is_decoded=1
                   WHERE file_id=$file AND object_index=$object AND field_index=$field
                     AND (semantic_key IS NOT $key OR payload_layout IS NOT $layout
                          OR decoded_value IS NOT $value OR is_decoded<>1);
                   """))
        {
            decoded.Parameters.Add("$key",SqliteType.Text);
            decoded.Parameters.Add("$layout",SqliteType.Text);
            decoded.Parameters.Add("$value",SqliteType.Text);
            decoded.Parameters.Add("$file",SqliteType.Integer);
            decoded.Parameters.Add("$object",SqliteType.Integer);
            decoded.Parameters.Add("$field",SqliteType.Integer);
            foreach (UvControllerObservation observation in observations)
            {
                decoded.Parameters["$key"].Value =
                    "uv_controller.transform_evaluators";
                decoded.Parameters["$layout"].Value = layout;
                decoded.Parameters["$value"].Value =
                    JsonSerializer.Serialize(observation.Data);
                decoded.Parameters["$file"].Value = observation.FileId;
                decoded.Parameters["$object"].Value = observation.ObjectIndex;
                decoded.Parameters["$field"].Value = observation.FieldIndex;
                decoded.ExecuteNonQuery();
            }
        }

        int variantId;
        using (SqliteCommand variant = CreateCommand(connection,transaction,"""
                   INSERT INTO class_variants(
                       type_hash,scope_kind,scope_key,variant_key,display_name,
                       status,discriminator_json,notes,created_utc,updated_utc)
                   VALUES($hash,'common','pc_ps2','uv_transform_sparse',
                          'Sparse UV transform evaluator','confirmed',
                          $discriminator,$notes,$utc,$utc)
                   ON CONFLICT(type_hash,scope_kind,scope_key,variant_key)
                   DO UPDATE SET display_name=excluded.display_name,
                                 status=excluded.status,
                                 discriminator_json=excluded.discriminator_json,
                                 notes=excluded.notes,updated_utc=excluded.updated_utc;
                   SELECT id FROM class_variants
                   WHERE type_hash=$hash AND scope_kind='common'
                     AND scope_key='pc_ps2'
                     AND variant_key='uv_transform_sparse';
                   """))
        {
            variant.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            variant.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
            {
                fieldShape = "one field-0 spTransFunctionEval plus terminator",
                serializedSizes,
                payloadSizes
            }));
            variant.Parameters.AddWithValue("$notes",
                $"All {objectCount} objects share one semantic layout. The eleven " +
                "sizes represent omitted default-valued evaluator fields, not subtypes.");
            variant.Parameters.AddWithValue("$utc",now);
            variantId = Convert.ToInt32(variant.ExecuteScalar());
        }
        using (SqliteCommand assignments = CreateCommand(connection,transaction,"""
                   INSERT INTO object_variant_assignments(
                       file_id,object_index,variant_id,confidence,evidence)
                   SELECT file_id,object_index,$variant,'confirmed',
                          'full PC+PS2 corpus: decoded common transform layout'
                   FROM objects WHERE type_hash=$hash
                   ON CONFLICT(file_id,object_index,variant_id)
                   DO UPDATE SET confidence=excluded.confidence,
                                 evidence=excluded.evidence
                   WHERE confidence<>excluded.confidence
                      OR evidence<>excluded.evidence;
                   """))
        {
            assignments.Parameters.AddWithValue("$variant",variantId);
            assignments.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            assignments.ExecuteNonQuery();
        }

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "spUVController load/serialize VA 0x00440BE0..0x00440FD7; " +
            "spTransFunctionEval diagnostics at VA 0x006EACB0..0x006EB06C",
            "Field 0 embeds spTransFunctionEval. Serializer vocabulary confirms " +
            "translation XYZ, scale XYZ, rotation, UV pivot and rotation axis.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "nested load 0x00187AE0..0x00187EAC; serialize " +
            "0x00187EC0..0x001880F8; wrapper 0x00188350..0x00188640",
            "PS2 uses the same seven evaluator calls and writes UV pivot then " +
            "rotation axis with the same resource framing as PC.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "full payloads reopened from directory SMO and PCK entry offsets",
            $"Decoded {objectCount} objects across three corpora; " +
            $"{payloadGroups.Length} distinct payloads; sizes: {shapeDistribution}; " +
            $"UV pivots={pivotSummary}; rotation axes={axisSummary}; function " +
            $"types={string.Join(";",functionTypeDistribution.Select(item => $"{item.Key}={item.Value}"))}.",
            null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical path plus sorted multiset of complete field-0 payload hashes",
            $"PC working/pristine: {pcEqual}/{pcCommon} equal resources; " +
            $"PC pristine/PS2 pristine: {platformEqual}/{platformCommon} equal resources; " +
            $"different paths=[{string.Join(',',platformDifferences)}].",
            null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='material_controller',
                       description='Animated UV translation, scale and rotation controller',
                       decode_status='read_only_decode',
                       notes='Common PC/PS2 spTransFunctionEval layout; eleven observed sizes are sparse default omission forms.'
                   WHERE type_hash=$hash;
                   """))
        {
            updateClass.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            updateClass.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);

        int assignmentCount;
        using (SqliteCommand assignments = connection.CreateCommand())
        {
            assignments.CommandText = """
                SELECT COUNT(*) FROM object_variant_assignments a
                JOIN objects o ON o.file_id=a.file_id
                              AND o.object_index=a.object_index
                WHERE o.type_hash=$hash;
                """;
            assignments.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            assignmentCount = Convert.ToInt32(assignments.ExecuteScalar());
        }
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,objectCount,
            report.Profiles.Sum(item => item.UniqueResourceCount),
            payloadGroups.Length,assignmentCount,evidenceRows,
            $"One semantic PC/PS2 layout in eleven sparse sizes; " +
            $"payloads={payloadGroups.Length}, pivots={distinctPivots}, " +
            $"axes={distinctAxes}; function types: " +
            $"{string.Join("; ",functionTypes.Select(item => $"{item.Key}=[{string.Join(',',item.Value)}]"))}; " +
            $"pivot values: {pivotSummary}; axis values: {axisSummary}; " +
            $"PC pairs {pcEqual}/{pcCommon}; " +
            $"PC/PS2 pairs {platformEqual}/{platformCommon}; differing paths: " +
            $"[{string.Join(',',platformDifferences)}]. " +
            "Mutation safety remains untested.");
    }

    public static IReadOnlyList<SmoResearchPlatformConflict> GetPlatformConflicts(
        string databasePath)
    {
        string database = Path.GetFullPath(databasePath);
        if (!File.Exists(database))
            throw new FileNotFoundException("Research database not found.", database);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT c.corpus_key,f.relative_path,f.platform_mask,
                   f.resource_platform_key,f.platform_tag_note
            FROM files f JOIN corpora c ON c.id=f.corpus_id
            WHERE f.platform_tag_status='conflict'
            ORDER BY c.corpus_key,f.normalized_path;
            """;
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new List<SmoResearchPlatformConflict>();
        while (reader.Read())
        {
            result.Add(new SmoResearchPlatformConflict(
                reader.GetString(0),reader.GetString(1),
                checked((uint)reader.GetInt64(2)),
                reader.IsDBNull(3) ? null : reader.GetString(3),
                reader.IsDBNull(4) ? null : reader.GetString(4)));
        }
        return result;
    }

    public static IReadOnlyList<SmoResearchResourceMatch> FindResources(
        string databasePath,
        string pathSubstring)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(pathSubstring);
        string database = Path.GetFullPath(databasePath);
        if (!File.Exists(database))
            throw new FileNotFoundException("Research database not found.", database);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT c.corpus_key,p.platform_key,f.relative_path,f.extension,
                   f.byte_size,f.sha256,f.serializer_version,f.ffps_unknown08,
                   f.platform_mask,f.parse_status,
                   COALESCE(f.object_count,0),COUNT(fo.id)
            FROM files f
            JOIN corpora c ON c.id=f.corpus_id
            JOIN platforms p ON p.id=c.platform_id
            LEFT JOIN file_occurrences fo ON fo.file_id=f.id
            WHERE INSTR(LOWER(f.normalized_path),LOWER($query))>0
            GROUP BY f.id
            ORDER BY c.id,f.normalized_path,f.sha256;
            """;
        command.Parameters.AddWithValue("$query", NormalizePath(pathSubstring));
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new List<SmoResearchResourceMatch>();
        while (reader.Read())
        {
            result.Add(new SmoResearchResourceMatch(
                reader.GetString(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetInt64(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : checked((uint)reader.GetInt64(6)),
                reader.IsDBNull(7) ? null : checked((uint)reader.GetInt64(7)),
                reader.IsDBNull(8) ? null : checked((uint)reader.GetInt64(8)),
                reader.GetString(9),reader.GetInt32(10),reader.GetInt32(11)));
        }
        return result;
    }

    public static IReadOnlyList<SmoResearchHeaderProfile> GetHeaderProfiles(
        string databasePath)
    {
        string database = Path.GetFullPath(databasePath);
        if (!File.Exists(database))
            throw new FileNotFoundException("Research database not found.", database);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT c.corpus_key,p.platform_key,f.serializer_version,f.platform_mask,
                   COUNT(*),COUNT(DISTINCT f.ffps_unknown08),
                   MIN(f.ffps_unknown08),MAX(f.ffps_unknown08)
            FROM files f
            JOIN corpora c ON c.id=f.corpus_id
            JOIN platforms p ON p.id=c.platform_id
            WHERE f.extension='.smo' AND f.parse_status='ok'
              AND f.serializer_version IS NOT NULL
              AND f.ffps_unknown08 IS NOT NULL
              AND f.platform_mask IS NOT NULL
            GROUP BY c.id,f.serializer_version,f.platform_mask
            ORDER BY c.id,f.platform_mask;
            """;
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new List<SmoResearchHeaderProfile>();
        while (reader.Read())
        {
            result.Add(new SmoResearchHeaderProfile(
                reader.GetString(0), reader.GetString(1),
                checked((uint)reader.GetInt64(2)),
                checked((uint)reader.GetInt64(3)),
                reader.GetInt32(4), reader.GetInt32(5),
                checked((uint)reader.GetInt64(6)),
                checked((uint)reader.GetInt64(7))));
        }
        return result;
    }

    public static string CheckIntegrity(string databasePath)
    {
        string database = Path.GetFullPath(databasePath);
        if (!File.Exists(database))
            throw new FileNotFoundException("Research database not found.", database);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = "PRAGMA integrity_check;";
        return Convert.ToString(command.ExecuteScalar()) ?? string.Empty;
    }

    public static SmoResearchCorpusComparison CompareCorpora(
        string databasePath,
        string leftCorpus,
        string rightCorpus)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(leftCorpus);
        ArgumentException.ThrowIfNullOrWhiteSpace(rightCorpus);
        string database = Path.GetFullPath(databasePath);
        if (!File.Exists(database))
            throw new FileNotFoundException("Research database not found.", database);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        Dictionary<string, ResourceIdentity> left = LoadResourceIdentities(
            connection, leftCorpus);
        Dictionary<string, ResourceIdentity> right = LoadResourceIdentities(
            connection, rightCorpus);
        var differences = new List<SmoResearchResourceDifference>();
        int common = 0;
        int identical = 0;
        int changed = 0;
        int leftOnly = 0;
        int rightOnly = 0;
        foreach (string path in left.Keys.Union(right.Keys).Order(StringComparer.Ordinal))
        {
            bool hasLeft = left.TryGetValue(path, out ResourceIdentity? leftItem);
            bool hasRight = right.TryGetValue(path, out ResourceIdentity? rightItem);
            if (!hasLeft)
            {
                rightOnly++;
                differences.Add(new SmoResearchResourceDifference(
                    path,"right_only",null,rightItem!.ByteSize,null,rightItem.Sha256));
                continue;
            }
            if (!hasRight)
            {
                leftOnly++;
                differences.Add(new SmoResearchResourceDifference(
                    path,"left_only",leftItem!.ByteSize,null,leftItem.Sha256,null));
                continue;
            }
            common++;
            if (leftItem!.Sha256.Equals(rightItem!.Sha256, StringComparison.Ordinal))
            {
                identical++;
                continue;
            }
            changed++;
            differences.Add(new SmoResearchResourceDifference(
                path,"content_changed",leftItem.ByteSize,rightItem.ByteSize,
                leftItem.Sha256,rightItem.Sha256));
        }
        return new SmoResearchCorpusComparison(
            leftCorpus,rightCorpus,left.Count,right.Count,common,identical,
            changed,leftOnly,rightOnly,differences);
    }

    private static string PrepareDatabase(string databasePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(databasePath);
        SmoCorpusDatabase.EnsureSqliteInitialized();
        string fullPath = Path.GetFullPath(databasePath);
        Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!);
        return fullPath;
    }

    private static SqliteConnection OpenResearch(string path, bool readOnly)
    {
        SmoCorpusDatabase.EnsureSqliteInitialized();
        SqliteConnection connection = SmoCorpusDatabase.Open(path, readOnly);
        if (!readOnly)
            SmoCorpusDatabase.ConfigureWriter(connection);
        if (readOnly)
            SmoResearchSchema.Validate(connection);
        else
            SmoResearchSchema.Initialize(connection);
        return connection;
    }

    private static void ValidateIdentity(
        string corpusKey,
        string platformKey,
        string provenance)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(corpusKey);
        if (platformKey is not ("pc" or "ps2"))
            throw new ArgumentException("Platform must be 'pc' or 'ps2'.");
        if (provenance is not ("pristine" or "working" or "modified" or
            "recovered" or "synthetic"))
        {
            throw new ArgumentException(
                "Provenance must be pristine, working, modified, recovered, or synthetic.");
        }
    }

    private static string RequireFile(string path, string kind)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        string fullPath = Path.GetFullPath(path);
        if (!File.Exists(fullPath))
            throw new FileNotFoundException($"{kind} not found.", fullPath);
        return fullPath;
    }

    private static int EnsureCorpus(
        SqliteConnection connection,
        SqliteTransaction transaction,
        string corpusKey,
        string platformKey,
        string provenance,
        string sourceKind,
        string sourceRoot)
    {
        using SqliteCommand find = CreateCommand(connection, transaction, """
            SELECT c.id, p.platform_key, c.provenance, c.source_kind, c.source_root
            FROM corpora c JOIN platforms p ON p.id=c.platform_id
            WHERE c.corpus_key=$key;
            """);
        find.Parameters.AddWithValue("$key", corpusKey);
        using (SqliteDataReader reader = find.ExecuteReader())
        {
            if (reader.Read())
            {
                int id = reader.GetInt32(0);
                string storedRoot = reader.GetString(4);
                if (!reader.GetString(1).Equals(platformKey, StringComparison.Ordinal) ||
                    !reader.GetString(2).Equals(provenance, StringComparison.Ordinal) ||
                    !reader.GetString(3).Equals(sourceKind, StringComparison.Ordinal) ||
                    !Path.GetFullPath(storedRoot).Equals(
                        Path.GetFullPath(sourceRoot), StringComparison.OrdinalIgnoreCase))
                {
                    throw new InvalidOperationException(
                        $"Corpus '{corpusKey}' is already registered with different identity.");
                }
                reader.Close();
                using SqliteCommand touch = CreateCommand(
                    connection, transaction,
                    "UPDATE corpora SET updated_utc=$utc WHERE id=$id;");
                touch.Parameters.AddWithValue("$utc", DateTime.UtcNow.ToString("O"));
                touch.Parameters.AddWithValue("$id", id);
                touch.ExecuteNonQuery();
                return id;
            }
        }

        using SqliteCommand insert = CreateCommand(connection, transaction, """
            INSERT INTO corpora(
                corpus_key, platform_id, provenance, source_kind, source_root,
                created_utc, updated_utc)
            SELECT $key, id, $provenance, $kind, $root, $utc, $utc
            FROM platforms WHERE platform_key=$platform;
            SELECT last_insert_rowid();
            """);
        insert.Parameters.AddWithValue("$key", corpusKey);
        insert.Parameters.AddWithValue("$platform", platformKey);
        insert.Parameters.AddWithValue("$provenance", provenance);
        insert.Parameters.AddWithValue("$kind", sourceKind);
        insert.Parameters.AddWithValue("$root", sourceRoot);
        insert.Parameters.AddWithValue("$utc", DateTime.UtcNow.ToString("O"));
        return Convert.ToInt32(insert.ExecuteScalar());
    }

    private static int InsertScan(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int corpusId,
        int containers)
    {
        using SqliteCommand command = CreateCommand(connection, transaction, """
            INSERT INTO scans(
                corpus_id, started_utc, scanner_revision, discovered_containers)
            VALUES($corpus,$utc,$revision,$containers);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$corpus", corpusId);
        command.Parameters.AddWithValue("$utc", DateTime.UtcNow.ToString("O"));
        command.Parameters.AddWithValue("$revision", SmoResearchSchema.ScannerRevision);
        command.Parameters.AddWithValue("$containers", containers);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void SeedKnowledge(
        SqliteConnection connection,
        SqliteTransaction transaction,
        string platformKey)
    {
        SmoCorpusDatabase.SeedClasses(connection, transaction);
        SeedResourceKnowledge(connection, transaction);
        if (!platformKey.Equals("pc", StringComparison.Ordinal))
            return;
        using SqliteCommand command = CreateCommand(connection, transaction, """
            INSERT INTO field_definitions(
                type_hash, scope_kind, scope_key, section_from_end, field_type,
                occurrence, semantic_key, display_name, payload_layout,
                editable_status, evidence_status)
            VALUES($hash,'platform','pc',0,$type,-1,$key,$display,$layout,
                   'read_only_research','confirmed_pc_executable_and_corpus')
            ON CONFLICT(type_hash,scope_kind,scope_key,section_from_end,
                        field_type,occurrence)
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

    private static void UpsertExecutable(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int corpusId,
        string platformKey,
        string path,
        int? fileId)
    {
        var info = new FileInfo(path);
        using SqliteCommand command = CreateCommand(connection, transaction, """
            INSERT INTO executables(
                corpus_id, file_id, relative_or_external_path, byte_size, sha256,
                architecture, scanned_utc)
            VALUES($corpus,$file,$path,$bytes,$sha,$architecture,$utc)
            ON CONFLICT(corpus_id) DO UPDATE SET
                file_id=excluded.file_id,
                relative_or_external_path=excluded.relative_or_external_path,
                byte_size=excluded.byte_size, sha256=excluded.sha256,
                architecture=excluded.architecture, scanned_utc=excluded.scanned_utc;
            """);
        command.Parameters.AddWithValue("$corpus", corpusId);
        AddNullable(command, "$file", fileId);
        command.Parameters.AddWithValue("$path", path);
        command.Parameters.AddWithValue("$bytes", info.Length);
        command.Parameters.AddWithValue("$sha", HashFile(path));
        command.Parameters.AddWithValue(
            "$architecture", platformKey == "pc" ? "x86-le" : "mips32-le");
        command.Parameters.AddWithValue("$utc", DateTime.UtcNow.ToString("O"));
        command.ExecuteNonQuery();
    }

    private static int UpsertContainer(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int corpusId,
        int scanId,
        string kind,
        string relativePath,
        long? byteSize,
        long? ticks,
        string? sha256,
        string status,
        string? error)
    {
        using SqliteCommand command = CreateCommand(connection, transaction, """
            INSERT INTO containers(
                corpus_id,container_kind,relative_path,byte_size,
                last_write_utc_ticks,sha256,parse_status,parse_error,
                scanner_revision,last_scan_id,scanned_utc)
            VALUES($corpus,$kind,$path,$bytes,$ticks,$sha,$status,$error,
                   $revision,$scan,$utc)
            ON CONFLICT(corpus_id,relative_path) DO UPDATE SET
                container_kind=excluded.container_kind,
                byte_size=excluded.byte_size,
                last_write_utc_ticks=excluded.last_write_utc_ticks,
                sha256=excluded.sha256,parse_status=excluded.parse_status,
                parse_error=excluded.parse_error,
                scanner_revision=excluded.scanner_revision,
                last_scan_id=excluded.last_scan_id,scanned_utc=excluded.scanned_utc;
            """);
        command.Parameters.AddWithValue("$corpus", corpusId);
        command.Parameters.AddWithValue("$kind", kind);
        command.Parameters.AddWithValue("$path", relativePath);
        AddNullable(command, "$bytes", byteSize);
        AddNullable(command, "$ticks", ticks);
        AddNullable(command, "$sha", sha256);
        command.Parameters.AddWithValue("$status", status);
        AddNullable(command, "$error", error);
        command.Parameters.AddWithValue("$revision", SmoResearchSchema.ScannerRevision);
        command.Parameters.AddWithValue("$scan", scanId);
        command.Parameters.AddWithValue("$utc", DateTime.UtcNow.ToString("O"));
        command.ExecuteNonQuery();
        using SqliteCommand select = CreateCommand(connection, transaction,
            "SELECT id FROM containers WHERE corpus_id=$corpus AND relative_path=$path;");
        select.Parameters.AddWithValue("$corpus", corpusId);
        select.Parameters.AddWithValue("$path", relativePath);
        return Convert.ToInt32(select.ExecuteScalar());
    }

    private static int IndexResource(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int corpusId,
        string platformKey,
        string logicalPath,
        long byteSize,
        long lastWriteTicks,
        string sha256,
        byte[]? data,
        UpdateCounters counters)
    {
        string normalized = NormalizePath(logicalPath).ToLowerInvariant();
        string extension = Path.GetExtension(logicalPath).ToLowerInvariant();
        bool reuseExistingObjectIndex = false;
        using SqliteCommand find = CreateCommand(connection, transaction, """
            SELECT id,parse_status,scanner_revision,
                   COALESCE((SELECT parser_revision FROM file_format_assignments a
                             WHERE a.file_id=files.id),0)
            FROM files
            WHERE corpus_id=$corpus AND normalized_path=$path AND sha256=$sha;
            """);
        find.Parameters.AddWithValue("$corpus", corpusId);
        find.Parameters.AddWithValue("$path", normalized);
        find.Parameters.AddWithValue("$sha", sha256);
        using (SqliteDataReader reader = find.ExecuteReader())
        {
            if (reader.Read())
            {
                int existingId = reader.GetInt32(0);
                string status = reader.GetString(1);
                int revision = reader.GetInt32(2);
                int resourceParserRevision = reader.GetInt32(3);
                if (!status.Equals("error", StringComparison.Ordinal) &&
                    revision == SmoResearchSchema.ScannerRevision &&
                    resourceParserRevision == GameResourceKnowledge.ParserRevision)
                {
                    return existingId;
                }
                reuseExistingObjectIndex = extension.Equals(
                    ".smo", StringComparison.OrdinalIgnoreCase) &&
                    status.Equals("ok", StringComparison.Ordinal) && revision >= 3;
            }
        }

        int fileId = UpsertResourceRow(
            connection, transaction, corpusId, logicalPath, normalized,
            extension, byteSize, lastWriteTicks, sha256);
        IndexResourceKnowledge(
            connection, transaction, fileId, platformKey, logicalPath, data);
        if (!extension.Equals(".smo", StringComparison.OrdinalIgnoreCase))
        {
            UpdateResourceParse(
                connection, transaction, fileId, "indexed", null,
                null, null, null, null, null, null, null, null, null);
            return fileId;
        }
        if (data is null)
            throw new InvalidOperationException("SMO resource bytes were not supplied.");

        try
        {
            SmoDocument document = SmoDocument.ParseOwned(
                data, $"{platformKey}:{logicalPath}");
            string resourcePlatform = GetResourcePlatform(document.Header.PlatformMask);
            bool platformMatches = resourcePlatform == "common" ||
                                   resourcePlatform == platformKey;
            string platformStatus = platformMatches ? "match" : "conflict";
            string? platformNote = platformMatches
                ? null
                : $"Corpus platform {platformKey} conflicts with FFPS platform mask " +
                  $"0x{document.Header.PlatformMask:X}.";
            UpdateResourceParse(
                connection, transaction, fileId, "ok", null,
                document.Header.SerializerVersion, document.Header.Unknown08,
                document.Header.PlatformMask, document.Header.DataStart,
                document.Header.DataSize, document.Objects.Count,
                platformStatus, platformNote, resourcePlatform);
            counters.ParsedSmo++;
            if (!platformMatches)
                counters.PlatformConflicts++;
            if (reuseExistingObjectIndex)
            {
                counters.UnchangedResources++;
                return fileId;
            }
            SmoCorpusDatabase.DeleteIndexedObjects(connection, transaction, fileId);
            SmoCorpusDatabase.InsertDocument(
                connection, transaction, fileId, document,
                applyKnownFieldSemantics: resourcePlatform != "ps2");
        }
        catch (Exception exception) when (
            exception is IOException or InvalidDataException or SmoFormatException or
                OverflowException)
        {
            UpdateResourceParse(
                connection, transaction, fileId, "error", exception.Message,
                null, null, null, null, null, null, null, null, null);
            SmoCorpusDatabase.DeleteIndexedObjects(connection, transaction, fileId);
            counters.FailedSmo++;
        }
        return fileId;
    }

    private static int UpsertResourceRow(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int corpusId,
        string logicalPath,
        string normalizedPath,
        string extension,
        long byteSize,
        long ticks,
        string sha256)
    {
        using SqliteCommand command = CreateCommand(connection, transaction, """
            INSERT INTO files(
                corpus_id,relative_path,normalized_path,file_name,family,extension,
                byte_size,last_write_utc_ticks,sha256,parse_status,
                scanner_revision,scanned_utc)
            VALUES($corpus,$path,$normalized,$name,$family,$extension,
                   $bytes,$ticks,$sha,'pending',$revision,$utc)
            ON CONFLICT(corpus_id,normalized_path,sha256) DO UPDATE SET
                relative_path=excluded.relative_path,file_name=excluded.file_name,
                family=excluded.family,extension=excluded.extension,
                byte_size=excluded.byte_size,last_write_utc_ticks=excluded.last_write_utc_ticks,
                scanner_revision=excluded.scanner_revision,scanned_utc=excluded.scanned_utc;
            """);
        command.Parameters.AddWithValue("$corpus", corpusId);
        command.Parameters.AddWithValue("$path", NormalizePath(logicalPath));
        command.Parameters.AddWithValue("$normalized", normalizedPath);
        command.Parameters.AddWithValue("$name", Path.GetFileName(logicalPath));
        command.Parameters.AddWithValue("$family", GetFamily(NormalizePath(logicalPath)));
        command.Parameters.AddWithValue("$extension", extension);
        command.Parameters.AddWithValue("$bytes", byteSize);
        command.Parameters.AddWithValue("$ticks", ticks);
        command.Parameters.AddWithValue("$sha", sha256);
        command.Parameters.AddWithValue("$revision", SmoResearchSchema.ScannerRevision);
        command.Parameters.AddWithValue("$utc", DateTime.UtcNow.ToString("O"));
        command.ExecuteNonQuery();
        using SqliteCommand select = CreateCommand(connection, transaction, """
            SELECT id FROM files
            WHERE corpus_id=$corpus AND normalized_path=$path AND sha256=$sha;
            """);
        select.Parameters.AddWithValue("$corpus", corpusId);
        select.Parameters.AddWithValue("$path", normalizedPath);
        select.Parameters.AddWithValue("$sha", sha256);
        return Convert.ToInt32(select.ExecuteScalar());
    }

    private static void UpdateResourceParse(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int fileId,
        string status,
        string? error,
        uint? serializerVersion,
        uint? unknown08,
        uint? platformMask,
        uint? dataStart,
        uint? dataSize,
        int? objectCount,
        string? platformStatus,
        string? platformNote,
        string? resourcePlatform)
    {
        using SqliteCommand command = CreateCommand(connection, transaction, """
            UPDATE files SET parse_status=$status,parse_error=$error,
                serializer_version=$serializer,ffps_unknown08=$unknown08,
                platform_mask=$platform_mask,data_start=$start,data_size=$size,
                object_count=$objects,platform_tag_status=$platform_status,
                platform_tag_note=$platform_note,
                resource_platform_key=$resource_platform,
                scanner_revision=$revision,scanned_utc=$utc
            WHERE id=$id;
            """);
        command.Parameters.AddWithValue("$status", status);
        AddNullable(command, "$error", error);
        AddNullable(command, "$serializer", serializerVersion is uint sv ? (long)sv : null);
        AddNullable(command, "$unknown08", unknown08 is uint u ? (long)u : null);
        AddNullable(command, "$platform_mask", platformMask is uint pm ? (long)pm : null);
        AddNullable(command, "$start", dataStart is uint s ? (long)s : null);
        AddNullable(command, "$size", dataSize is uint d ? (long)d : null);
        AddNullable(command, "$objects", objectCount);
        AddNullable(command, "$platform_status", platformStatus);
        AddNullable(command, "$platform_note", platformNote);
        AddNullable(command, "$resource_platform", resourcePlatform);
        command.Parameters.AddWithValue("$revision", SmoResearchSchema.ScannerRevision);
        command.Parameters.AddWithValue("$utc", DateTime.UtcNow.ToString("O"));
        command.Parameters.AddWithValue("$id", fileId);
        command.ExecuteNonQuery();
    }

    private static void UpsertOccurrence(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int fileId,
        int containerId,
        string occurrenceKey,
        string physicalPath,
        int? entryIndex,
        long? byteOffset,
        long byteSize,
        long ticks,
        int scanId)
    {
        using SqliteCommand command = CreateCommand(connection, transaction, """
            INSERT INTO file_occurrences(
                file_id,container_id,occurrence_key,physical_path,entry_index,
                byte_offset,byte_size,last_write_utc_ticks,last_scan_id)
            VALUES($file,$container,$key,$path,$entry,$offset,$bytes,$ticks,$scan)
            ON CONFLICT(container_id,occurrence_key) DO UPDATE SET
                file_id=excluded.file_id,physical_path=excluded.physical_path,
                entry_index=excluded.entry_index,byte_offset=excluded.byte_offset,
                byte_size=excluded.byte_size,
                last_write_utc_ticks=excluded.last_write_utc_ticks,
                last_scan_id=excluded.last_scan_id;
            """);
        command.Parameters.AddWithValue("$file", fileId);
        command.Parameters.AddWithValue("$container", containerId);
        command.Parameters.AddWithValue("$key", occurrenceKey);
        command.Parameters.AddWithValue("$path", NormalizePath(physicalPath));
        AddNullable(command, "$entry", entryIndex);
        AddNullable(command, "$offset", byteOffset);
        command.Parameters.AddWithValue("$bytes", byteSize);
        command.Parameters.AddWithValue("$ticks", ticks);
        command.Parameters.AddWithValue("$scan", scanId);
        command.ExecuteNonQuery();
    }

    private static Dictionary<string, ExistingOccurrence> LoadOccurrences(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int containerId)
    {
        using SqliteCommand command = CreateCommand(connection, transaction, """
            SELECT fo.id,fo.occurrence_key,fo.byte_size,fo.last_write_utc_ticks,
                   f.scanner_revision,f.parse_status,f.extension
            FROM file_occurrences fo JOIN files f ON f.id=fo.file_id
            WHERE fo.container_id=$container;
            """);
        command.Parameters.AddWithValue("$container", containerId);
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new Dictionary<string, ExistingOccurrence>(
            StringComparer.OrdinalIgnoreCase);
        while (reader.Read())
        {
            result[reader.GetString(1)] = new ExistingOccurrence(
                reader.GetInt32(0),reader.GetInt64(2),reader.GetInt64(3),
                reader.GetInt32(4),reader.GetString(5),reader.GetString(6));
        }
        return result;
    }

    private static Dictionary<string, ExistingContainer> LoadContainers(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int corpusId)
    {
        using SqliteCommand command = CreateCommand(connection, transaction, """
            SELECT id,relative_path,byte_size,last_write_utc_ticks,
                   scanner_revision,parse_status FROM containers
            WHERE corpus_id=$corpus;
            """);
        command.Parameters.AddWithValue("$corpus", corpusId);
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new Dictionary<string, ExistingContainer>(
            StringComparer.OrdinalIgnoreCase);
        while (reader.Read())
        {
            result[reader.GetString(1)] = new ExistingContainer(
                reader.GetInt32(0),reader.IsDBNull(2) ? null : reader.GetInt64(2),
                reader.IsDBNull(3) ? null : reader.GetInt64(3),reader.GetInt32(4),
                reader.GetString(5));
        }
        return result;
    }

    private static void TouchOccurrence(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int occurrenceId,
        int scanId)
    {
        using SqliteCommand command = CreateCommand(connection, transaction,
            "UPDATE file_occurrences SET last_scan_id=$scan WHERE id=$id;");
        command.Parameters.AddWithValue("$scan", scanId);
        command.Parameters.AddWithValue("$id", occurrenceId);
        command.ExecuteNonQuery();
    }

    private static void TouchContainer(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int containerId,
        int scanId)
    {
        using SqliteCommand command = CreateCommand(connection, transaction,
            "UPDATE containers SET last_scan_id=$scan WHERE id=$id;");
        command.Parameters.AddWithValue("$scan", scanId);
        command.Parameters.AddWithValue("$id", containerId);
        command.ExecuteNonQuery();
    }

    private static (long Occurrences, int Resources) TouchContainerOccurrences(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int containerId,
        int scanId)
    {
        long occurrences;
        int resources;
        using (SqliteCommand count = CreateCommand(connection, transaction, """
                   SELECT COUNT(*),COUNT(DISTINCT file_id) FROM file_occurrences
                   WHERE container_id=$container;
                   """))
        {
            count.Parameters.AddWithValue("$container", containerId);
            using SqliteDataReader reader = count.ExecuteReader();
            reader.Read();
            occurrences = reader.GetInt64(0);
            resources = reader.GetInt32(1);
        }
        using SqliteCommand touch = CreateCommand(connection, transaction,
            "UPDATE file_occurrences SET last_scan_id=$scan WHERE container_id=$container;");
        touch.Parameters.AddWithValue("$scan", scanId);
        touch.Parameters.AddWithValue("$container", containerId);
        touch.ExecuteNonQuery();
        return (occurrences, resources);
    }

    private static int RemoveMissingOccurrences(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int containerId,
        int scanId)
    {
        using SqliteCommand command = CreateCommand(connection, transaction, """
            DELETE FROM file_occurrences
            WHERE container_id=$container AND last_scan_id<>$scan;
            """);
        command.Parameters.AddWithValue("$container", containerId);
        command.Parameters.AddWithValue("$scan", scanId);
        return command.ExecuteNonQuery();
    }

    private static int RemoveMissingContainers(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int corpusId,
        int scanId)
    {
        int occurrences;
        using (SqliteCommand count = CreateCommand(connection, transaction, """
                   SELECT COUNT(*) FROM file_occurrences fo
                   JOIN containers c ON c.id=fo.container_id
                   WHERE c.corpus_id=$corpus AND c.last_scan_id<>$scan;
                   """))
        {
            count.Parameters.AddWithValue("$corpus", corpusId);
            count.Parameters.AddWithValue("$scan", scanId);
            occurrences = Convert.ToInt32(count.ExecuteScalar());
        }
        using SqliteCommand remove = CreateCommand(connection, transaction,
            "DELETE FROM containers WHERE corpus_id=$corpus AND last_scan_id<>$scan;");
        remove.Parameters.AddWithValue("$corpus", corpusId);
        remove.Parameters.AddWithValue("$scan", scanId);
        remove.ExecuteNonQuery();
        return occurrences;
    }

    private static void DeleteOrphanResources(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int corpusId)
    {
        using SqliteCommand command = CreateCommand(connection, transaction, """
            DELETE FROM files WHERE corpus_id=$corpus
            AND NOT EXISTS(SELECT 1 FROM file_occurrences fo WHERE fo.file_id=files.id);
            """);
        command.Parameters.AddWithValue("$corpus", corpusId);
        command.ExecuteNonQuery();
    }

    private static void FinishScan(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int scanId,
        UpdateCounters counters)
    {
        using SqliteCommand command = CreateCommand(connection, transaction, """
            UPDATE scans SET finished_utc=$utc,
                discovered_occurrences=$occurrences,
                scanned_resources=$scanned,unchanged_resources=$unchanged,
                parsed_smo=$parsed,failed_smo=$failed,
                removed_occurrences=$removed
            WHERE id=$id;
            """);
        command.Parameters.AddWithValue("$utc", DateTime.UtcNow.ToString("O"));
        command.Parameters.AddWithValue("$occurrences", counters.Occurrences);
        command.Parameters.AddWithValue("$scanned", counters.ScannedResources);
        command.Parameters.AddWithValue("$unchanged", counters.UnchangedResources);
        command.Parameters.AddWithValue("$parsed", counters.ParsedSmo);
        command.Parameters.AddWithValue("$failed", counters.FailedSmo);
        command.Parameters.AddWithValue("$removed", counters.RemovedOccurrences);
        command.Parameters.AddWithValue("$id", scanId);
        command.ExecuteNonQuery();
    }

    private static SmoResearchUpdateResult CreateResult(
        SqliteConnection connection,
        string databasePath,
        string corpusKey,
        string platformKey,
        string provenance,
        string sourceKind,
        int containers,
        UpdateCounters counters,
        TimeSpan elapsed)
    {
        int corpusId = GetCorpusId(connection, corpusKey);
        long resources = Scalar(connection,
            "SELECT COUNT(*) FROM files WHERE corpus_id=$corpus;", corpusId);
        long smo = Scalar(connection,
            "SELECT COUNT(*) FROM files WHERE corpus_id=$corpus AND extension='.smo';",
            corpusId);
        long objects = Scalar(connection, """
            SELECT COUNT(*) FROM objects o JOIN files f ON f.id=o.file_id
            WHERE f.corpus_id=$corpus;
            """, corpusId);
        long fields = Scalar(connection, """
            SELECT COUNT(*) FROM direct_fields d JOIN files f ON f.id=d.file_id
            WHERE f.corpus_id=$corpus;
            """, corpusId);
        return new SmoResearchUpdateResult(
            databasePath,corpusKey,platformKey,provenance,sourceKind,
            containers,counters.Occurrences,counters.ScannedResources,
            counters.UnchangedResources,counters.ParsedSmo,counters.FailedSmo,
            counters.PlatformConflicts,counters.RemovedOccurrences,resources,smo,
            objects,fields,elapsed);
    }

    private static SmoResearchCorpusSummary ReadCorpusSummary(
        SqliteConnection connection,
        int corpusId,
        string corpusKey,
        string platformKey,
        string provenance,
        string sourceKind)
    {
        long resources = Scalar(connection,
            "SELECT COUNT(*) FROM files WHERE corpus_id=$corpus;", corpusId);
        long occurrences = Scalar(connection, """
            SELECT COUNT(*) FROM file_occurrences fo
            JOIN files f ON f.id=fo.file_id WHERE f.corpus_id=$corpus;
            """, corpusId);
        long smo = Scalar(connection,
            "SELECT COUNT(*) FROM files WHERE corpus_id=$corpus AND extension='.smo';",
            corpusId);
        long parsed = Scalar(connection, """
            SELECT COUNT(*) FROM files WHERE corpus_id=$corpus
            AND extension='.smo' AND parse_status='ok';
            """, corpusId);
        long failed = Scalar(connection, """
            SELECT COUNT(*) FROM files WHERE corpus_id=$corpus
            AND extension='.smo' AND parse_status='error';
            """, corpusId);
        long conflicts = Scalar(connection, """
            SELECT COUNT(*) FROM files WHERE corpus_id=$corpus
            AND platform_tag_status='conflict';
            """, corpusId);
        long objects = Scalar(connection, """
            SELECT COUNT(*) FROM objects o JOIN files f ON f.id=o.file_id
            WHERE f.corpus_id=$corpus;
            """, corpusId);
        long fields = Scalar(connection, """
            SELECT COUNT(*) FROM direct_fields d JOIN files f ON f.id=d.file_id
            WHERE f.corpus_id=$corpus;
            """, corpusId);
        int classes = checked((int)Scalar(connection, """
            SELECT COUNT(DISTINCT o.type_hash) FROM objects o
            JOIN files f ON f.id=o.file_id WHERE f.corpus_id=$corpus;
            """, corpusId));
        return new SmoResearchCorpusSummary(
            corpusKey,platformKey,provenance,sourceKind,resources,occurrences,
            smo,parsed,failed,conflicts,objects,fields,classes);
    }

    private static int GetCorpusId(SqliteConnection connection, string corpusKey)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = "SELECT id FROM corpora WHERE corpus_key=$key;";
        command.Parameters.AddWithValue("$key", corpusKey);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static bool IsObservedControllerProfile(
        SmoMaterialColorControllerData value)
    {
        static bool ColorMatches(SmoColorFunctionalEvaluatorData item) =>
            item.Color1 == 0xFF000000 && item.Color2 == 0xFF000000 &&
            item.FunctionType == 0 && item.Frequency == 0.1f &&
            item.Amplitude == 1.0f && item.XOffset == 0.0f &&
            item.YOffset == 0.0f && item.Pitch == 0.0f;
        return ColorMatches(value.Ambient) && ColorMatches(value.Diffuse) &&
               ColorMatches(value.Specular) && ColorMatches(value.Emissive) &&
               value.Alpha.FunctionType == 0 && value.Alpha.Frequency == 0.1f &&
               value.Alpha.Amplitude == 0.0f && value.Alpha.XOffset == 0.0f &&
               value.Alpha.YOffset == 1.0f && value.Alpha.Pitch == 0.0f;
    }

    private static bool IsFinitePositive(Vector3 value) =>
        float.IsFinite(value.X) && value.X > 0.0f &&
        float.IsFinite(value.Y) && value.Y > 0.0f &&
        float.IsFinite(value.Z) && value.Z > 0.0f;

    private static bool IsFiniteUvController(SmoUvControllerData value)
    {
        static bool EvaluatorIsFinite(SmoFunctionalEvaluatorData evaluator) =>
            float.IsFinite(evaluator.Frequency) &&
            float.IsFinite(evaluator.Amplitude) &&
            float.IsFinite(evaluator.XOffset) &&
            float.IsFinite(evaluator.YOffset) &&
            float.IsFinite(evaluator.Pitch);
        static bool VectorIsFinite(Vector3 vector) =>
            float.IsFinite(vector.X) && float.IsFinite(vector.Y) &&
            float.IsFinite(vector.Z);
        return EvaluatorIsFinite(value.TranslationX) &&
               EvaluatorIsFinite(value.TranslationY) &&
               EvaluatorIsFinite(value.TranslationZ) &&
               EvaluatorIsFinite(value.ScaleX) &&
               EvaluatorIsFinite(value.ScaleY) &&
               EvaluatorIsFinite(value.ScaleZ) &&
               EvaluatorIsFinite(value.Rotation) &&
               VectorIsFinite(value.UvPivot) &&
               VectorIsFinite(value.RotationAxis);
    }

    private static Dictionary<string,string> BuildOrientedBoxPathMap(
        IEnumerable<OrientedBoxObservation> observations,
        string corpusKey) => observations
        .Where(item => item.CorpusKey.Equals(corpusKey, StringComparison.Ordinal))
        .GroupBy(item => item.CanonicalPath, StringComparer.OrdinalIgnoreCase)
        .ToDictionary(
            group => group.Key,
            group => string.Join("|", group
                .Select(item => Convert.ToHexString(item.Payload))
                .OrderBy(value => value, StringComparer.Ordinal)),
            StringComparer.OrdinalIgnoreCase);

    private static Dictionary<string,string> BuildBoxPathMap(
        IEnumerable<BoxObservation> observations,
        string corpusKey) => observations
        .Where(item => item.CorpusKey.Equals(corpusKey,StringComparison.Ordinal))
        .GroupBy(item => item.CanonicalPath,StringComparer.OrdinalIgnoreCase)
        .ToDictionary(
            group => group.Key,
            group => string.Join("|",group
                .Select(item => Convert.ToHexString(item.Payload))
                .OrderBy(value => value,StringComparer.Ordinal)),
            StringComparer.OrdinalIgnoreCase);

    private static Dictionary<string,string> BuildSpherePathMap(
        IEnumerable<SphereObservation> observations,
        string corpusKey) => observations
        .Where(item => item.CorpusKey.Equals(corpusKey,StringComparison.Ordinal))
        .GroupBy(item => item.CanonicalPath,StringComparer.OrdinalIgnoreCase)
        .ToDictionary(
            group => group.Key,
            group => string.Join("|",group
                .Select(item => Convert.ToHexString(item.Payload))
                .OrderBy(value => value,StringComparer.Ordinal)),
            StringComparer.OrdinalIgnoreCase);

    private static (int Common, int Equal) ComparePayloadPathMaps(
        IReadOnlyDictionary<string,string> left,
        IReadOnlyDictionary<string,string> right)
    {
        int common = 0;
        int equal = 0;
        foreach ((string path, string leftPayloads) in left)
        {
            if (!right.TryGetValue(path, out string? rightPayloads))
                continue;
            common++;
            if (leftPayloads.Equals(rightPayloads, StringComparison.Ordinal))
                equal++;
        }
        return (common,equal);
    }

    private static Dictionary<string,FogObservation> BuildFogPathMap(
        IEnumerable<FogObservation> observations,
        string corpusKey)
    {
        var result = new Dictionary<string,FogObservation>(
            StringComparer.OrdinalIgnoreCase);
        foreach (FogObservation observation in observations.Where(item =>
                     item.CorpusKey.Equals(corpusKey, StringComparison.Ordinal)))
        {
            if (!result.TryAdd(observation.CanonicalPath, observation))
            {
                throw new InvalidDataException(
                    $"More than one spFog object uses canonical path " +
                    $"'{observation.CanonicalPath}' in {corpusKey}.");
            }
        }
        return result;
    }

    private static (int Common, int Equal) CompareFogPathMaps(
        IReadOnlyDictionary<string,FogObservation> left,
        IReadOnlyDictionary<string,FogObservation> right)
    {
        int common = 0;
        int equal = 0;
        foreach ((string path, FogObservation leftObservation) in left)
        {
            if (!right.TryGetValue(path, out FogObservation? rightObservation))
                continue;
            common++;
            if (leftObservation.Payload.AsSpan().SequenceEqual(rightObservation.Payload))
                equal++;
        }
        return (common,equal);
    }

    private static ExecutableIdentity FindExecutable(
        SqliteConnection connection,
        string platformKey)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT e.relative_or_external_path,e.sha256,p.id,c.id
            FROM executables e
            JOIN corpora c ON c.id=e.corpus_id
            JOIN platforms p ON p.id=c.platform_id
            WHERE p.platform_key=$platform
            ORDER BY CASE c.provenance WHEN 'pristine' THEN 0 ELSE 1 END,c.id
            LIMIT 1;
            """;
        command.Parameters.AddWithValue("$platform", platformKey);
        using SqliteDataReader reader = command.ExecuteReader();
        if (!reader.Read())
            throw new InvalidOperationException(
                $"No executable is registered for platform '{platformKey}'.");
        return new ExecutableIdentity(
            reader.GetString(0),reader.GetString(1),reader.GetInt32(2),reader.GetInt32(3));
    }

    private static void RequireAsciiTokens(string path, params string[] tokens)
    {
        byte[] data = File.ReadAllBytes(path);
        foreach (string token in tokens)
        {
            byte[] pattern = System.Text.Encoding.ASCII.GetBytes(token);
            if (data.AsSpan().IndexOf(pattern) < 0)
            {
                throw new InvalidDataException(
                    $"Executable evidence token '{token}' was not found in {path}.");
            }
        }
    }

    private static int InsertEvidence(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        int? variantId,
        int? platformId,
        int? corpusId,
        string kind,
        string sourcePath,
        string locator,
        string observation,
        string? sourceSha256,
        string createdUtc)
    {
        using SqliteCommand command = CreateCommand(connection, transaction, """
            INSERT INTO evidence(
                type_hash,variant_id,platform_id,corpus_id,evidence_kind,
                source_path,locator,observation,confidence,source_sha256,created_utc)
            VALUES($hash,$variant,$platform,$corpus,$kind,$path,$locator,
                   $observation,'confirmed',$sha,$utc);
            """);
        command.Parameters.AddWithValue("$hash", (long)typeHash);
        AddNullable(command,"$variant",variantId);
        AddNullable(command,"$platform",platformId);
        AddNullable(command,"$corpus",corpusId);
        command.Parameters.AddWithValue("$kind",kind);
        command.Parameters.AddWithValue("$path",sourcePath);
        command.Parameters.AddWithValue("$locator",locator);
        command.Parameters.AddWithValue("$observation",observation);
        AddNullable(command,"$sha",sourceSha256);
        command.Parameters.AddWithValue("$utc",createdUtc);
        return command.ExecuteNonQuery();
    }

    private static (uint TypeHash, string? EngineName) ResolveClass(
        SqliteConnection connection,
        string classIdentifier)
    {
        uint? requestedHash = null;
        string trimmed = classIdentifier.Trim();
        if (trimmed.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
        {
            if (uint.TryParse(
                    trimmed.AsSpan(2),
                    System.Globalization.NumberStyles.HexNumber,
                    System.Globalization.CultureInfo.InvariantCulture,
                    out uint hexadecimal))
            {
                requestedHash = hexadecimal;
            }
        }
        else if (uint.TryParse(
                     trimmed,
                     System.Globalization.NumberStyles.Integer,
                     System.Globalization.CultureInfo.InvariantCulture,
                     out uint decimalHash))
        {
            requestedHash = decimalHash;
        }

        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = requestedHash.HasValue
            ? "SELECT type_hash,engine_name FROM classes WHERE type_hash=$value;"
            : "SELECT type_hash,engine_name FROM classes WHERE engine_name=$value COLLATE NOCASE;";
        command.Parameters.AddWithValue(
            "$value", requestedHash.HasValue ? (object)(long)requestedHash.Value : trimmed);
        using SqliteDataReader reader = command.ExecuteReader();
        if (!reader.Read())
            throw new InvalidOperationException($"Unknown class identifier '{classIdentifier}'.");
        return (
            checked((uint)reader.GetInt64(0)),
            reader.IsDBNull(1) ? null : reader.GetString(1));
    }

    private static Dictionary<string, ResourceIdentity> LoadResourceIdentities(
        SqliteConnection connection,
        string corpusKey)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT f.normalized_path,f.byte_size,f.sha256
            FROM files f JOIN corpora c ON c.id=f.corpus_id
            WHERE c.corpus_key=$corpus
            ORDER BY f.normalized_path,f.id;
            """;
        command.Parameters.AddWithValue("$corpus", corpusKey);
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new Dictionary<string, ResourceIdentity>(StringComparer.Ordinal);
        while (reader.Read())
        {
            string path = reader.GetString(0);
            if (!result.TryAdd(path, new ResourceIdentity(
                    reader.GetInt64(1),reader.GetString(2))))
            {
                throw new InvalidOperationException(
                    $"Corpus '{corpusKey}' has multiple content versions for '{path}'; " +
                    "direct corpus comparison requires one version per logical path.");
            }
        }
        if (result.Count == 0)
            throw new InvalidOperationException($"Corpus '{corpusKey}' is empty or missing.");
        return result;
    }

    private static long Scalar(
        SqliteConnection connection,
        string sql,
        int? corpusId = null)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = sql;
        if (corpusId.HasValue)
            command.Parameters.AddWithValue("$corpus", corpusId.Value);
        return Convert.ToInt64(command.ExecuteScalar());
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

    private static string HashFile(string path)
    {
        using var stream = new FileStream(
            path, FileMode.Open, FileAccess.Read, FileShare.Read);
        return Convert.ToHexString(SHA256.HashData(stream));
    }

    private static string NormalizePath(string path) =>
        path.Replace('\\', '/').TrimStart('/');

    private static string FormatResearchFloat(float value) =>
        value.ToString("G9", System.Globalization.CultureInfo.InvariantCulture);

    private static string GetCanonicalResourcePath(string path)
    {
        string normalized = NormalizePath(path);
        return normalized.StartsWith("data/", StringComparison.OrdinalIgnoreCase)
            ? normalized[5..]
            : normalized;
    }

    private static string GetFamily(string relativePath)
    {
        int separator = relativePath.IndexOf('/');
        return separator < 0 ? "." : relativePath[..separator];
    }

    private static string GetResourcePlatform(uint platformMask)
    {
        bool common = (platformMask & 0x01) != 0;
        bool pc = (platformMask & 0x02) != 0;
        bool ps2 = (platformMask & 0x08) != 0;
        if (!common && !pc && !ps2)
            return "unknown";
        return (pc, ps2) switch
        {
            (true, false) => "pc",
            (false, true) => "ps2",
            (false, false) => "common",
            _ => "mixed"
        };
    }

    private static void Checkpoint(SqliteConnection connection)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = "PRAGMA wal_checkpoint(TRUNCATE);";
        command.ExecuteNonQuery();
    }

    private sealed class UpdateCounters
    {
        public long Occurrences { get; set; }
        public int ScannedResources { get; set; }
        public int UnchangedResources { get; set; }
        public int ParsedSmo { get; set; }
        public int FailedSmo { get; set; }
        public int PlatformConflicts { get; set; }
        public int RemovedOccurrences { get; set; }
    }

    private sealed record ExistingOccurrence(
        int Id,
        long ByteSize,
        long LastWriteUtcTicks,
        int ScannerRevision,
        string ParseStatus,
        string Extension);

    private sealed record ExistingContainer(
        int Id,
        long? ByteSize,
        long? LastWriteUtcTicks,
        int ScannerRevision,
        string ParseStatus);

    private sealed record ResourceIdentity(long ByteSize, string Sha256);

    private sealed record ResourceCandidateIdentity(
        int FileId,
        string CorpusKey,
        string PlatformKey,
        string RelativePath,
        string Sha256,
        int ObjectCount,
        int PhysicalOccurrences);

    private sealed record ExecutableIdentity(
        string Path,
        string Sha256,
        int PlatformId,
        int CorpusId);

    private sealed record FogObservation(
        int FileId,
        int ObjectIndex,
        string CorpusKey,
        string PlatformKey,
        string CanonicalPath,
        byte[] Payload,
        SmoFogData Data);

    private sealed record OrientedBoxObservation(
        int FileId,
        int ObjectIndex,
        string CorpusKey,
        string PlatformKey,
        string CanonicalPath,
        byte[] Payload,
        SmoOrientedBoxSizeData Data);

    private sealed record BoxObservation(
        int FileId,
        int ObjectIndex,
        string CorpusKey,
        string PlatformKey,
        string CanonicalPath,
        byte[] Payload,
        SmoBoxSizeData Data);

    private sealed record SphereObservation(
        int FileId,
        int ObjectIndex,
        string CorpusKey,
        string PlatformKey,
        string CanonicalPath,
        byte[] Payload,
        float Radius);

    private sealed record LightFieldObservation(
        int FieldIndex,
        int FieldType,
        byte[] Payload);

    private sealed class LightObservationBuilder
    {
        public LightObservationBuilder(
            int fileId,
            int objectIndex,
            uint objectId,
            string corpusKey,
            string platformKey,
            string canonicalPath,
            string objectName,
            int serializedSize,
            string fieldShape)
        {
            FileId = fileId;
            ObjectIndex = objectIndex;
            ObjectId = objectId;
            CorpusKey = corpusKey;
            PlatformKey = platformKey;
            CanonicalPath = canonicalPath;
            ObjectName = objectName;
            SerializedSize = serializedSize;
            FieldShape = fieldShape;
        }

        public int FileId { get; }
        public int ObjectIndex { get; }
        public uint ObjectId { get; }
        public string CorpusKey { get; }
        public string PlatformKey { get; }
        public string CanonicalPath { get; }
        public string ObjectName { get; }
        public int SerializedSize { get; }
        public string FieldShape { get; }
        public List<SmoObjectField> DecoderFields { get; } = [];
        public List<LightFieldObservation> SerializedFields { get; } = [];
    }

    private sealed record LightObservation(
        int FileId,
        int ObjectIndex,
        string CorpusKey,
        string PlatformKey,
        string CanonicalPath,
        string ObjectName,
        int SerializedSize,
        string FieldShape,
        IReadOnlyList<LightFieldObservation> Fields,
        SmoLightData Data,
        string SerializedSignature);

    private sealed record UvPayloadLocation(
        int FileId,
        int ObjectIndex,
        int FieldIndex,
        string CorpusKey,
        string PlatformKey,
        string RelativePath,
        int SerializedSize,
        int PayloadSize,
        int PayloadOffset,
        string SourceKind,
        string SourceRoot,
        string ContainerPath,
        string PhysicalPath,
        long EntryOffset);

    private sealed record UvControllerObservation(
        int FileId,
        int ObjectIndex,
        int FieldIndex,
        string CorpusKey,
        string PlatformKey,
        string CanonicalPath,
        int SerializedSize,
        byte[] Payload,
        SmoUvControllerData Data);
}
