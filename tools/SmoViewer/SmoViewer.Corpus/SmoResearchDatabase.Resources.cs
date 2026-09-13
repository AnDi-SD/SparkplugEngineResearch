using Microsoft.Data.Sqlite;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    public static IReadOnlyList<SmoResearchRegisteredSource> GetRegisteredSources(
        string databasePath)
    {
        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT c.corpus_key,p.platform_key,c.provenance,c.source_kind,c.source_root,
                   e.relative_or_external_path
            FROM corpora c JOIN platforms p ON p.id=c.platform_id
            LEFT JOIN executables e ON e.corpus_id=c.id
            ORDER BY c.id;
            """;
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new List<SmoResearchRegisteredSource>();
        while (reader.Read())
            result.Add(new(reader.GetString(0), reader.GetString(1), reader.GetString(2),
                reader.GetString(3), reader.GetString(4),
                reader.IsDBNull(5) ? null : reader.GetString(5)));
        return result;
    }

    public static int RefreshUnknownResourceFormats(string databasePath)
    {
        string database = PrepareDatabase(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: false);
        using SqliteTransaction transaction = connection.BeginTransaction();
        SeedResourceKnowledge(connection, transaction);
        var rows = new List<(int FileId, string Platform, string Path, string Extension)>();
        using (SqliteCommand command = CreateCommand(connection, transaction, """
                   SELECT f.id,p.platform_key,f.relative_path,f.extension
                   FROM file_format_assignments a
                   JOIN files f ON f.id=a.file_id
                   JOIN corpora c ON c.id=f.corpus_id
                   JOIN platforms p ON p.id=c.platform_id
                   WHERE a.format_key='unknown'
                   ORDER BY f.id;
                   """))
        using (SqliteDataReader reader = command.ExecuteReader())
        {
            while (reader.Read())
                rows.Add((reader.GetInt32(0), reader.GetString(1), reader.GetString(2),
                    reader.GetString(3)));
        }
        int changed = 0;
        foreach ((int fileId, string platform, string path, string extension) in rows)
        {
            if (GameResourceKnowledge.RequiresPayload(extension))
                continue;
            GameResourceAnalysis analysis = GameResourceKnowledge.Analyze(platform, path, null);
            if (analysis.FormatKey == "unknown")
                continue;
            IndexResourceKnowledge(
                connection, transaction, fileId, platform, path, data: null);
            changed++;
        }
        transaction.Commit();
        Checkpoint(connection);
        return changed;
    }

    private static void SeedResourceKnowledge(
        SqliteConnection connection,
        SqliteTransaction transaction)
    {
        using SqliteCommand format = CreateCommand(connection, transaction, """
            INSERT INTO resource_formats(
                format_key,display_name,category,description,decode_status,
                write_status,evidence_status,notes)
            VALUES($key,$name,$category,$description,$decode,$write,$evidence,$notes)
            ON CONFLICT(format_key) DO UPDATE SET
                display_name=excluded.display_name,category=excluded.category,
                description=excluded.description,decode_status=excluded.decode_status,
                write_status=excluded.write_status,evidence_status=excluded.evidence_status,
                notes=excluded.notes;
            """);
        foreach (GameResourceFormatDefinition definition in GameResourceKnowledge.Formats)
        {
            format.Parameters.Clear();
            format.Parameters.AddWithValue("$key", definition.Key);
            format.Parameters.AddWithValue("$name", definition.DisplayName);
            format.Parameters.AddWithValue("$category", definition.Category);
            format.Parameters.AddWithValue("$description", definition.Description);
            format.Parameters.AddWithValue("$decode", definition.DecodeStatus);
            format.Parameters.AddWithValue("$write", definition.WriteStatus);
            format.Parameters.AddWithValue("$evidence", definition.EvidenceStatus);
            AddNullable(format, "$notes", definition.Notes);
            format.ExecuteNonQuery();

            using SqliteCommand clearExtensions = CreateCommand(connection, transaction,
                "DELETE FROM resource_format_extensions WHERE format_key=$key;");
            clearExtensions.Parameters.AddWithValue("$key", definition.Key);
            clearExtensions.ExecuteNonQuery();
            foreach (string extension in definition.Extensions)
            {
                using SqliteCommand addExtension = CreateCommand(connection, transaction, """
                    INSERT INTO resource_format_extensions(format_key,extension,is_primary)
                    VALUES($key,$extension,1);
                    """);
                addExtension.Parameters.AddWithValue("$key", definition.Key);
                addExtension.Parameters.AddWithValue("$extension", extension.ToLowerInvariant());
                addExtension.ExecuteNonQuery();
            }
        }

        string utc = DateTime.UtcNow.ToString("O");
        foreach (GameResourceVariantDefinition definition in GameResourceKnowledge.Variants)
        {
            using SqliteCommand variant = CreateCommand(connection, transaction, """
                INSERT INTO resource_format_variants(
                    format_key,scope_kind,scope_key,variant_key,display_name,status,
                    discriminator_json,notes,created_utc,updated_utc)
                VALUES($format,$scope_kind,$scope_key,$variant,$name,$status,
                       $discriminator,$notes,$utc,$utc)
                ON CONFLICT(format_key,scope_kind,scope_key,variant_key) DO UPDATE SET
                    display_name=excluded.display_name,status=excluded.status,
                    discriminator_json=excluded.discriminator_json,notes=excluded.notes,
                    updated_utc=excluded.updated_utc;
                """);
            variant.Parameters.AddWithValue("$format", definition.FormatKey);
            variant.Parameters.AddWithValue("$scope_kind", definition.ScopeKind);
            variant.Parameters.AddWithValue("$scope_key", definition.ScopeKey);
            variant.Parameters.AddWithValue("$variant", definition.Key);
            variant.Parameters.AddWithValue("$name", definition.DisplayName);
            variant.Parameters.AddWithValue("$status", definition.Status);
            AddNullable(variant, "$discriminator", definition.DiscriminatorJson);
            AddNullable(variant, "$notes", definition.Notes);
            variant.Parameters.AddWithValue("$utc", utc);
            variant.ExecuteNonQuery();
        }

        foreach (GameResourceEvidenceDefinition definition in GameResourceKnowledge.Evidence)
        {
            using SqliteCommand evidence = CreateCommand(connection, transaction, """
                INSERT INTO resource_evidence(
                    format_key,variant_id,platform_id,evidence_kind,source_path,locator,
                    observation,confidence,created_utc)
                SELECT $format,
                       (SELECT id FROM resource_format_variants
                        WHERE format_key=$format AND variant_key=$variant
                        ORDER BY id LIMIT 1),
                       (SELECT id FROM platforms WHERE platform_key=$platform),
                       $kind,$source,$locator,$observation,$confidence,$utc
                WHERE NOT EXISTS(
                    SELECT 1 FROM resource_evidence
                    WHERE format_key=$format AND file_id IS NULL AND container_id IS NULL
                      AND evidence_kind=$kind AND source_path=$source
                      AND COALESCE(locator,'')=COALESCE($locator,'')
                      AND observation=$observation);
                """);
            evidence.Parameters.AddWithValue("$format", definition.FormatKey);
            AddNullable(evidence, "$variant", definition.VariantKey);
            AddNullable(evidence, "$platform", definition.PlatformKey);
            evidence.Parameters.AddWithValue("$kind", definition.EvidenceKind);
            evidence.Parameters.AddWithValue("$source", definition.SourcePath);
            AddNullable(evidence, "$locator", definition.Locator);
            evidence.Parameters.AddWithValue("$observation", definition.Observation);
            evidence.Parameters.AddWithValue("$confidence", definition.Confidence);
            evidence.Parameters.AddWithValue("$utc", utc);
            evidence.ExecuteNonQuery();
        }
    }

    private static void IndexResourceKnowledge(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int fileId,
        string platformKey,
        string logicalPath,
        byte[]? data)
    {
        GameResourceAnalysis analysis = GameResourceKnowledge.Analyze(
            platformKey, logicalPath, data);
        int? variantId = ResolveResourceVariant(
            connection, transaction, analysis.FormatKey, analysis.VariantKey, platformKey);
        using (SqliteCommand assignment = CreateCommand(connection, transaction, """
                   INSERT INTO file_format_assignments(
                       file_id,format_key,variant_id,recognition_method,recognition_status,
                       decode_status,parser_revision,summary_json,error,analyzed_utc)
                   VALUES($file,$format,$variant,$method,$recognition,$decode,$revision,
                          $summary,$error,$utc)
                   ON CONFLICT(file_id) DO UPDATE SET
                       format_key=excluded.format_key,variant_id=excluded.variant_id,
                       recognition_method=excluded.recognition_method,
                       recognition_status=excluded.recognition_status,
                       decode_status=excluded.decode_status,
                       parser_revision=excluded.parser_revision,
                       summary_json=excluded.summary_json,error=excluded.error,
                       analyzed_utc=excluded.analyzed_utc;
                   """))
        {
            assignment.Parameters.AddWithValue("$file", fileId);
            assignment.Parameters.AddWithValue("$format", analysis.FormatKey);
            AddNullable(assignment, "$variant", variantId);
            assignment.Parameters.AddWithValue("$method", analysis.RecognitionMethod);
            assignment.Parameters.AddWithValue("$recognition", analysis.RecognitionStatus);
            assignment.Parameters.AddWithValue("$decode", analysis.DecodeStatus);
            assignment.Parameters.AddWithValue("$revision", GameResourceKnowledge.ParserRevision);
            AddNullable(assignment, "$summary", analysis.SummaryJson);
            AddNullable(assignment, "$error", analysis.Error);
            assignment.Parameters.AddWithValue("$utc", DateTime.UtcNow.ToString("O"));
            assignment.ExecuteNonQuery();
        }

        foreach (string table in new[]
                 {
                     "resource_properties", "resource_symbols", "resource_dependencies"
                 })
        {
            string owner = table == "resource_dependencies" ? "source_file_id" : "file_id";
            using SqliteCommand clear = CreateCommand(connection, transaction,
                $"DELETE FROM {table} WHERE {owner}=$file;");
            clear.Parameters.AddWithValue("$file", fileId);
            clear.ExecuteNonQuery();
        }

        foreach (GameResourceProperty property in analysis.Properties)
        {
            using SqliteCommand insert = CreateCommand(connection, transaction, """
                INSERT INTO resource_properties(
                    file_id,property_key,occurrence,value_kind,value_json,
                    evidence_status,locator,notes)
                VALUES($file,$key,$occurrence,$kind,$value,$evidence,$locator,$notes);
                """);
            insert.Parameters.AddWithValue("$file", fileId);
            insert.Parameters.AddWithValue("$key", property.Key);
            insert.Parameters.AddWithValue("$occurrence", property.Occurrence);
            insert.Parameters.AddWithValue("$kind", property.ValueKind);
            insert.Parameters.AddWithValue("$value", property.ValueJson);
            insert.Parameters.AddWithValue("$evidence", property.EvidenceStatus);
            AddNullable(insert, "$locator", property.Locator);
            AddNullable(insert, "$notes", property.Notes);
            insert.ExecuteNonQuery();
        }
        foreach (GameResourceSymbol symbol in analysis.Symbols)
        {
            using SqliteCommand insert = CreateCommand(connection, transaction, """
                INSERT INTO resource_symbols(
                    file_id,symbol_kind,ordinal,symbol_name,locator,evidence_status)
                VALUES($file,$kind,$ordinal,$name,$locator,$evidence);
                """);
            insert.Parameters.AddWithValue("$file", fileId);
            insert.Parameters.AddWithValue("$kind", symbol.Kind);
            insert.Parameters.AddWithValue("$ordinal", symbol.Ordinal);
            insert.Parameters.AddWithValue("$name", symbol.Name);
            AddNullable(insert, "$locator", symbol.Locator);
            insert.Parameters.AddWithValue("$evidence", symbol.EvidenceStatus);
            insert.ExecuteNonQuery();
        }
        foreach (GameResourceDependency dependency in analysis.Dependencies)
        {
            string target = NormalizeDependencyPath(dependency.TargetPath);
            using SqliteCommand insert = CreateCommand(connection, transaction, """
                INSERT INTO resource_dependencies(
                    source_file_id,relation_kind,target_path,normalized_target_path,
                    target_extension,resolution_status,evidence_status,confidence,locator,notes)
                VALUES($file,$kind,$target,$normalized,$extension,'unresolved',
                       $evidence,$confidence,$locator,$notes);
                """);
            insert.Parameters.AddWithValue("$file", fileId);
            insert.Parameters.AddWithValue("$kind", dependency.Kind);
            insert.Parameters.AddWithValue("$target", dependency.TargetPath);
            insert.Parameters.AddWithValue("$normalized", target);
            insert.Parameters.AddWithValue("$extension", dependency.TargetExtension);
            insert.Parameters.AddWithValue("$evidence", dependency.EvidenceStatus);
            insert.Parameters.AddWithValue("$confidence", dependency.Confidence);
            AddNullable(insert, "$locator", dependency.Locator);
            AddNullable(insert, "$notes", dependency.Notes);
            insert.ExecuteNonQuery();
        }
    }

    private static int? ResolveResourceVariant(
        SqliteConnection connection,
        SqliteTransaction transaction,
        string formatKey,
        string? variantKey,
        string platformKey)
    {
        if (variantKey is null) return null;
        using SqliteCommand command = CreateCommand(connection, transaction, """
            SELECT id FROM resource_format_variants
            WHERE format_key=$format AND variant_key=$variant
              AND ((scope_kind='platform' AND scope_key=$platform)
                   OR (scope_kind='common' AND scope_key='*'))
            ORDER BY CASE scope_kind WHEN 'platform' THEN 0 ELSE 1 END,id
            LIMIT 1;
            """);
        command.Parameters.AddWithValue("$format", formatKey);
        command.Parameters.AddWithValue("$variant", variantKey);
        command.Parameters.AddWithValue("$platform", platformKey);
        object? value = command.ExecuteScalar();
        return value is null or DBNull ? null : Convert.ToInt32(value);
    }

    private static void UpsertContainerKnowledge(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int containerId,
        string formatKey,
        string recognitionStatus,
        string decodeStatus,
        string? summaryJson,
        IReadOnlyDictionary<string, (string Kind, string Json, string Evidence)> properties)
    {
        using (SqliteCommand assignment = CreateCommand(connection, transaction, """
                   INSERT INTO container_format_assignments(
                       container_id,format_key,recognition_status,decode_status,
                       parser_revision,summary_json,analyzed_utc)
                   VALUES($container,$format,$recognition,$decode,$revision,$summary,$utc)
                   ON CONFLICT(container_id) DO UPDATE SET
                       format_key=excluded.format_key,
                       recognition_status=excluded.recognition_status,
                       decode_status=excluded.decode_status,
                       parser_revision=excluded.parser_revision,
                       summary_json=excluded.summary_json,error=NULL,
                       analyzed_utc=excluded.analyzed_utc;
                   """))
        {
            assignment.Parameters.AddWithValue("$container", containerId);
            assignment.Parameters.AddWithValue("$format", formatKey);
            assignment.Parameters.AddWithValue("$recognition", recognitionStatus);
            assignment.Parameters.AddWithValue("$decode", decodeStatus);
            assignment.Parameters.AddWithValue("$revision", GameResourceKnowledge.ParserRevision);
            AddNullable(assignment, "$summary", summaryJson);
            assignment.Parameters.AddWithValue("$utc", DateTime.UtcNow.ToString("O"));
            assignment.ExecuteNonQuery();
        }
        using (SqliteCommand clear = CreateCommand(connection, transaction,
                   "DELETE FROM container_properties WHERE container_id=$container;"))
        {
            clear.Parameters.AddWithValue("$container", containerId);
            clear.ExecuteNonQuery();
        }
        foreach ((string key, (string kind, string json, string evidence)) in properties)
        {
            using SqliteCommand insert = CreateCommand(connection, transaction, """
                INSERT INTO container_properties(
                    container_id,property_key,value_kind,value_json,evidence_status)
                VALUES($container,$key,$kind,$json,$evidence);
                """);
            insert.Parameters.AddWithValue("$container", containerId);
            insert.Parameters.AddWithValue("$key", key);
            insert.Parameters.AddWithValue("$kind", kind);
            insert.Parameters.AddWithValue("$json", json);
            insert.Parameters.AddWithValue("$evidence", evidence);
            insert.ExecuteNonQuery();
        }
    }

    private static void ResolveResourceDependencies(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int corpusId)
    {
        var files = new List<DependencyFile>();
        using (SqliteCommand command = CreateCommand(connection, transaction, """
                   SELECT id,normalized_path,file_name FROM files WHERE corpus_id=$corpus;
                   """))
        {
            command.Parameters.AddWithValue("$corpus", corpusId);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
                files.Add(new(reader.GetInt32(0), reader.GetString(1), reader.GetString(2)));
        }
        Dictionary<string, List<DependencyFile>> byPath = files
            .GroupBy(item => CanonicalDependencyPath(item.NormalizedPath),
                StringComparer.OrdinalIgnoreCase)
            .ToDictionary(group => group.Key, group => group.ToList(), StringComparer.OrdinalIgnoreCase);
        Dictionary<string, List<DependencyFile>> byName = files
            .GroupBy(item => item.FileName, StringComparer.OrdinalIgnoreCase)
            .ToDictionary(group => group.Key, group => group.ToList(), StringComparer.OrdinalIgnoreCase);

        var dependencies = new List<DependencyRow>();
        using (SqliteCommand command = CreateCommand(connection, transaction, """
                   SELECT d.id,f.normalized_path,d.normalized_target_path
                   FROM resource_dependencies d JOIN files f ON f.id=d.source_file_id
                   WHERE f.corpus_id=$corpus;
                   """))
        {
            command.Parameters.AddWithValue("$corpus", corpusId);
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
                dependencies.Add(new(reader.GetInt32(0), reader.GetString(1), reader.GetString(2)));
        }

        foreach (DependencyRow dependency in dependencies)
        {
            string target = CanonicalDependencyPath(dependency.TargetPath);
            List<DependencyFile>? candidates = null;
            string status;
            if (byPath.TryGetValue(target, out List<DependencyFile>? exact))
            {
                candidates = exact;
                status = exact.Count == 1 ? "resolved_exact_path" : "ambiguous_exact_path";
            }
            else
            {
                string sourceDirectory = Path.GetDirectoryName(
                    dependency.SourcePath.Replace('/', Path.DirectorySeparatorChar)) ?? string.Empty;
                string adjacent = CanonicalDependencyPath(NormalizePath(Path.Combine(
                    sourceDirectory, dependency.TargetPath)));
                if (byPath.TryGetValue(adjacent, out List<DependencyFile>? near))
                {
                    candidates = near;
                    status = near.Count == 1 ? "resolved_same_directory" : "ambiguous_same_directory";
                }
                else
                {
                    string name = Path.GetFileName(dependency.TargetPath);
                    byName.TryGetValue(name, out candidates);
                    status = candidates switch
                    {
                        { Count: 1 } => "resolved_unique_basename",
                        { Count: > 1 } => "ambiguous_basename",
                        _ => "unresolved"
                    };
                }
            }
            int? targetFile = candidates is { Count: 1 } ? candidates[0].Id : null;
            using SqliteCommand update = CreateCommand(connection, transaction, """
                UPDATE resource_dependencies
                SET target_file_id=$target,resolution_status=$status WHERE id=$id;
                """);
            AddNullable(update, "$target", targetFile);
            update.Parameters.AddWithValue("$status", status);
            update.Parameters.AddWithValue("$id", dependency.Id);
            update.ExecuteNonQuery();
        }
    }

    private static int IndexAuxiliaryGameFiles(
        SqliteConnection connection,
        SqliteTransaction transaction,
        int corpusId,
        int scanId,
        string platformKey,
        string sourceRoot,
        string executablePath,
        UpdateCounters counters,
        out int executableFileId)
    {
        string executable = Path.GetFullPath(executablePath);
        string gameRoot = Path.GetDirectoryName(executable)!;
        string excludedRoot = Path.GetFullPath(sourceRoot)
            .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string excludedPrefix = excludedRoot + Path.DirectorySeparatorChar;
        var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        if (Directory.Exists(gameRoot))
        {
            foreach (string candidate in Directory.EnumerateFiles(
                         gameRoot, "*", SearchOption.AllDirectories))
            {
                string full = Path.GetFullPath(candidate);
                if (full.Equals(excludedRoot, StringComparison.OrdinalIgnoreCase) ||
                    full.StartsWith(excludedPrefix, StringComparison.OrdinalIgnoreCase))
                    continue;
                paths.Add(full);
            }
        }
        paths.Add(executable);

        int containerId = UpsertContainer(
            connection, transaction, corpusId, scanId, "directory", "@game",
            null, null, null, "ok", null);
        Dictionary<string, ExistingOccurrence> existing = LoadOccurrences(
            connection, transaction, containerId);
        foreach (string path in paths.Order(StringComparer.OrdinalIgnoreCase))
        {
            string relative = IsPathWithin(gameRoot, path)
                ? NormalizePath(Path.GetRelativePath(gameRoot, path))
                : Path.GetFileName(path);
            string logicalPath = $"@game/{relative}";
            var info = new FileInfo(path);
            counters.Occurrences++;
            if (existing.TryGetValue(logicalPath, out ExistingOccurrence? occurrence) &&
                occurrence.ByteSize == info.Length &&
                occurrence.LastWriteUtcTicks == info.LastWriteTimeUtc.Ticks &&
                occurrence.ScannerRevision == SmoResearchSchema.ScannerRevision &&
                !occurrence.ParseStatus.Equals("error", StringComparison.Ordinal))
            {
                TouchOccurrence(connection, transaction, occurrence.Id, scanId);
                counters.UnchangedResources++;
                continue;
            }
            byte[] data = File.ReadAllBytes(path);
            string sha256 = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(data));
            int fileId = IndexResource(
                connection, transaction, corpusId, platformKey, logicalPath,
                data.Length, info.LastWriteTimeUtc.Ticks, sha256, data, counters);
            UpsertOccurrence(
                connection, transaction, fileId, containerId, logicalPath,
                relative, null, null, data.Length, info.LastWriteTimeUtc.Ticks, scanId);
            counters.ScannedResources++;
        }
        counters.RemovedOccurrences += RemoveMissingOccurrences(
            connection, transaction, containerId, scanId);
        UpsertContainerKnowledge(
            connection, transaction, containerId, "directory", "confirmed", "indexed",
            System.Text.Json.JsonSerializer.Serialize(new { Root = gameRoot, Files = paths.Count }),
            new Dictionary<string, (string, string, string)>
            {
                ["directory.file_count"] = ("integer",
                    System.Text.Json.JsonSerializer.Serialize(paths.Count), "confirmed_scanner"),
                ["directory.role"] = ("string",
                    System.Text.Json.JsonSerializer.Serialize("game_root_auxiliary"), "confirmed_scanner")
            });

        string executableLogical = IsPathWithin(gameRoot, executable)
            ? $"@game/{NormalizePath(Path.GetRelativePath(gameRoot, executable))}"
            : $"@game/{Path.GetFileName(executable)}";
        using SqliteCommand find = CreateCommand(connection, transaction, """
            SELECT id FROM files WHERE corpus_id=$corpus AND normalized_path=$path
            ORDER BY id DESC LIMIT 1;
            """);
        find.Parameters.AddWithValue("$corpus", corpusId);
        find.Parameters.AddWithValue("$path", executableLogical.ToLowerInvariant());
        executableFileId = Convert.ToInt32(find.ExecuteScalar());
        return containerId;
    }

    private static bool IsPathWithin(string root, string path)
    {
        string fullRoot = Path.GetFullPath(root)
            .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string fullPath = Path.GetFullPath(path);
        return fullPath.Equals(fullRoot, StringComparison.OrdinalIgnoreCase) ||
               fullPath.StartsWith(fullRoot + Path.DirectorySeparatorChar,
                   StringComparison.OrdinalIgnoreCase);
    }

    public static GameResourceAudit GetResourceAudit(string databasePath)
    {
        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        IReadOnlyList<GameResourceFormatSummary> formats = GetResourceFormats(connection);
        var dependencies = new List<GameResourceDependencySummary>();
        using (SqliteCommand command = connection.CreateCommand())
        {
            command.CommandText = """
                SELECT corpus_key,format_key,relation_kind,resolution_status,dependency_count
                FROM resource_dependency_status_counts
                ORDER BY corpus_key,format_key,relation_kind,resolution_status;
                """;
            using SqliteDataReader reader = command.ExecuteReader();
            while (reader.Read())
                dependencies.Add(new(reader.GetString(0), reader.GetString(1), reader.GetString(2),
                    reader.GetString(3), reader.GetInt64(4)));
        }
        long ScalarText(string sql)
        {
            using SqliteCommand command = connection.CreateCommand();
            command.CommandText = sql;
            return Convert.ToInt64(command.ExecuteScalar());
        }
        return new(database, SmoResearchSchema.Version,
            ScalarText("SELECT COUNT(*) FROM files;"),
            ScalarText("SELECT COUNT(*) FROM file_format_assignments;"),
            ScalarText("SELECT COUNT(*) FROM files f WHERE NOT EXISTS(SELECT 1 FROM file_format_assignments a WHERE a.file_id=f.id);"),
            ScalarText("SELECT COUNT(*) FROM file_format_assignments WHERE decode_status='error';"),
            ScalarText("SELECT COUNT(*) FROM resource_dependencies;"),
            ScalarText("SELECT COUNT(*) FROM resource_dependencies WHERE resolution_status LIKE 'resolved_%';"),
            ScalarText("SELECT COUNT(*) FROM resource_dependencies WHERE resolution_status LIKE 'ambiguous_%';"),
            ScalarText("SELECT COUNT(*) FROM resource_dependencies WHERE resolution_status='unresolved';"),
            formats, dependencies);
    }

    public static IReadOnlyList<GameResourceFormatSummary> GetResourceFormats(string databasePath)
    {
        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        return GetResourceFormats(connection);
    }

    public static IReadOnlyList<GameResourceAnalysisError> GetResourceAnalysisErrors(
        string databasePath)
    {
        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT c.corpus_key,p.platform_key,f.relative_path,a.format_key,
                   a.recognition_status,COALESCE(a.error,'')
            FROM file_format_assignments a
            JOIN files f ON f.id=a.file_id
            JOIN corpora c ON c.id=f.corpus_id
            JOIN platforms p ON p.id=c.platform_id
            WHERE a.decode_status='error'
            ORDER BY c.corpus_key,a.format_key,f.normalized_path;
            """;
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new List<GameResourceAnalysisError>();
        while (reader.Read())
            result.Add(new(reader.GetString(0), reader.GetString(1), reader.GetString(2),
                reader.GetString(3), reader.GetString(4), reader.GetString(5)));
        return result;
    }

    public static IReadOnlyList<GameResourceFileRecord> GetFormatResources(
        string databasePath,
        string formatKey)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(formatKey);
        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: true);
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT c.corpus_key,p.platform_key,f.relative_path,f.extension,
                   f.byte_size,f.sha256,a.format_key,a.recognition_status,
                   a.decode_status,a.error,
                   (SELECT COUNT(*) FROM file_occurrences fo WHERE fo.file_id=f.id)
            FROM file_format_assignments a
            JOIN files f ON f.id=a.file_id
            JOIN corpora c ON c.id=f.corpus_id
            JOIN platforms p ON p.id=c.platform_id
            WHERE a.format_key=$format
            ORDER BY c.corpus_key,f.normalized_path,f.id;
            """;
        command.Parameters.AddWithValue("$format", formatKey.ToLowerInvariant());
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new List<GameResourceFileRecord>();
        while (reader.Read())
            result.Add(new(reader.GetString(0), reader.GetString(1), reader.GetString(2),
                reader.GetString(3), reader.GetInt64(4), reader.GetString(5),
                reader.GetString(6), reader.GetString(7), reader.GetString(8),
                reader.IsDBNull(9) ? null : reader.GetString(9), reader.GetInt32(10)));
        return result;
    }

    private static IReadOnlyList<GameResourceFormatSummary> GetResourceFormats(
        SqliteConnection connection)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT rf.format_key,rf.display_name,rf.category,rf.decode_status,
                   rf.write_status,rf.evidence_status,
                   COALESCE((SELECT group_concat(extension, ', ')
                             FROM (SELECT extension FROM resource_format_extensions e
                                   WHERE e.format_key=rf.format_key ORDER BY extension)),'') AS extensions,
                   (SELECT COUNT(*) FROM resource_format_variants v
                    WHERE v.format_key=rf.format_key) AS variants,
                   COUNT(a.file_id),COUNT(DISTINCT f.normalized_path),
                   COALESCE(SUM((SELECT COUNT(*) FROM file_occurrences fo
                                 WHERE fo.file_id=f.id)),0),
                   COALESCE(SUM(CASE WHEN a.decode_status IN ('ok','external_standard','external_standard_header') THEN 1 ELSE 0 END),0),
                   COALESCE(SUM(CASE WHEN a.decode_status LIKE '%partial%' OR a.decode_status='partial' THEN 1 ELSE 0 END),0),
                   COALESCE(SUM(CASE WHEN a.decode_status='inventory_only' THEN 1 ELSE 0 END),0),
                   COALESCE(SUM(CASE WHEN a.decode_status='error' THEN 1 ELSE 0 END),0)
            FROM resource_formats rf
            LEFT JOIN file_format_assignments a ON a.format_key=rf.format_key
            LEFT JOIN files f ON f.id=a.file_id
            GROUP BY rf.format_key
            ORDER BY COUNT(a.file_id) DESC,rf.format_key;
            """;
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new List<GameResourceFormatSummary>();
        while (reader.Read())
            result.Add(new(reader.GetString(0), reader.GetString(1), reader.GetString(2),
                reader.GetString(3), reader.GetString(4), reader.GetString(5), reader.GetString(6),
                reader.GetInt32(7), reader.GetInt64(8), reader.GetInt64(9), reader.GetInt64(10),
                reader.GetInt64(11), reader.GetInt64(12), reader.GetInt64(13), reader.GetInt64(14)));
        return result;
    }

    private static string NormalizeDependencyPath(string path) =>
        NormalizePath(path.Trim().Trim('"', '\'', '\0')).ToLowerInvariant();

    private static string CanonicalDependencyPath(string path)
    {
        string normalized = NormalizeDependencyPath(path);
        return normalized.StartsWith("data/", StringComparison.OrdinalIgnoreCase)
            ? normalized[5..]
            : normalized;
    }

    private sealed record DependencyFile(int Id, string NormalizedPath, string FileName);
    private sealed record DependencyRow(int Id, string SourcePath, string TargetPath);
}
