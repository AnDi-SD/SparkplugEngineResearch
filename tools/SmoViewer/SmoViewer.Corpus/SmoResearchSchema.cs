using Microsoft.Data.Sqlite;

namespace SmoViewer.Corpus;

internal static class SmoResearchSchema
{
    public const int Version = 5;
    public const int ScannerRevision = 7;

    private const string Sql = """
        PRAGMA foreign_keys=ON;

        CREATE TABLE IF NOT EXISTS schema_info (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS platforms (
            id INTEGER PRIMARY KEY,
            platform_key TEXT NOT NULL UNIQUE,
            display_name TEXT NOT NULL,
            architecture TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS corpora (
            id INTEGER PRIMARY KEY,
            corpus_key TEXT NOT NULL UNIQUE,
            platform_id INTEGER NOT NULL REFERENCES platforms(id),
            provenance TEXT NOT NULL,
            source_kind TEXT NOT NULL,
            source_root TEXT NOT NULL,
            description TEXT,
            created_utc TEXT NOT NULL,
            updated_utc TEXT NOT NULL,
            CHECK(provenance IN ('pristine','working','modified','recovered','synthetic')),
            CHECK(source_kind IN ('directory','pck'))
        );

        CREATE TABLE IF NOT EXISTS executables (
            id INTEGER PRIMARY KEY,
            corpus_id INTEGER NOT NULL UNIQUE REFERENCES corpora(id) ON DELETE CASCADE,
            file_id INTEGER REFERENCES files(id) ON DELETE SET NULL,
            relative_or_external_path TEXT NOT NULL,
            byte_size INTEGER NOT NULL,
            sha256 TEXT NOT NULL,
            architecture TEXT NOT NULL,
            scanned_utc TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS scans (
            id INTEGER PRIMARY KEY,
            corpus_id INTEGER NOT NULL REFERENCES corpora(id) ON DELETE CASCADE,
            started_utc TEXT NOT NULL,
            finished_utc TEXT,
            scanner_revision INTEGER NOT NULL,
            discovered_containers INTEGER NOT NULL DEFAULT 0,
            discovered_occurrences INTEGER NOT NULL DEFAULT 0,
            scanned_resources INTEGER NOT NULL DEFAULT 0,
            unchanged_resources INTEGER NOT NULL DEFAULT 0,
            parsed_smo INTEGER NOT NULL DEFAULT 0,
            failed_smo INTEGER NOT NULL DEFAULT 0,
            removed_occurrences INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS containers (
            id INTEGER PRIMARY KEY,
            corpus_id INTEGER NOT NULL REFERENCES corpora(id) ON DELETE CASCADE,
            container_kind TEXT NOT NULL,
            relative_path TEXT NOT NULL,
            byte_size INTEGER,
            last_write_utc_ticks INTEGER,
            sha256 TEXT,
            parse_status TEXT NOT NULL,
            parse_error TEXT,
            scanner_revision INTEGER NOT NULL,
            last_scan_id INTEGER NOT NULL REFERENCES scans(id),
            scanned_utc TEXT NOT NULL,
            UNIQUE(corpus_id, relative_path),
            CHECK(container_kind IN ('directory','pck'))
        );

        CREATE TABLE IF NOT EXISTS classes (
            type_hash INTEGER PRIMARY KEY,
            engine_name TEXT,
            is_known INTEGER NOT NULL,
            category TEXT,
            description TEXT,
            decode_status TEXT,
            notes TEXT
        );

        CREATE TABLE IF NOT EXISTS files (
            id INTEGER PRIMARY KEY,
            corpus_id INTEGER NOT NULL REFERENCES corpora(id) ON DELETE CASCADE,
            relative_path TEXT NOT NULL,
            normalized_path TEXT NOT NULL,
            file_name TEXT NOT NULL,
            family TEXT NOT NULL,
            extension TEXT NOT NULL,
            byte_size INTEGER NOT NULL,
            last_write_utc_ticks INTEGER NOT NULL,
            sha256 TEXT NOT NULL,
            serializer_version INTEGER,
            ffps_unknown08 INTEGER,
            platform_mask INTEGER,
            data_start INTEGER,
            data_size INTEGER,
            object_count INTEGER,
            parse_status TEXT NOT NULL,
            parse_error TEXT,
            platform_tag_status TEXT,
            platform_tag_note TEXT,
            resource_platform_key TEXT,
            scanner_revision INTEGER NOT NULL,
            scanned_utc TEXT NOT NULL,
            UNIQUE(corpus_id, normalized_path, sha256)
        );

        CREATE TABLE IF NOT EXISTS resource_formats (
            format_key TEXT PRIMARY KEY,
            display_name TEXT NOT NULL,
            category TEXT NOT NULL,
            description TEXT NOT NULL,
            decode_status TEXT NOT NULL,
            write_status TEXT NOT NULL,
            evidence_status TEXT NOT NULL,
            notes TEXT
        );

        CREATE TABLE IF NOT EXISTS resource_format_extensions (
            format_key TEXT NOT NULL REFERENCES resource_formats(format_key)
                ON DELETE CASCADE,
            extension TEXT NOT NULL,
            is_primary INTEGER NOT NULL DEFAULT 1,
            PRIMARY KEY(format_key, extension)
        );

        CREATE TABLE IF NOT EXISTS resource_format_variants (
            id INTEGER PRIMARY KEY,
            format_key TEXT NOT NULL REFERENCES resource_formats(format_key)
                ON DELETE CASCADE,
            scope_kind TEXT NOT NULL,
            scope_key TEXT NOT NULL,
            variant_key TEXT NOT NULL,
            display_name TEXT NOT NULL,
            status TEXT NOT NULL,
            discriminator_json TEXT,
            notes TEXT,
            created_utc TEXT NOT NULL,
            updated_utc TEXT NOT NULL,
            UNIQUE(format_key, scope_kind, scope_key, variant_key),
            CHECK(scope_kind IN ('common','platform','corpus'))
        );

        CREATE TABLE IF NOT EXISTS file_format_assignments (
            file_id INTEGER PRIMARY KEY REFERENCES files(id) ON DELETE CASCADE,
            format_key TEXT NOT NULL REFERENCES resource_formats(format_key),
            variant_id INTEGER REFERENCES resource_format_variants(id),
            recognition_method TEXT NOT NULL,
            recognition_status TEXT NOT NULL,
            decode_status TEXT NOT NULL,
            parser_revision INTEGER NOT NULL,
            summary_json TEXT,
            error TEXT,
            analyzed_utc TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS resource_properties (
            file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
            property_key TEXT NOT NULL,
            occurrence INTEGER NOT NULL DEFAULT 0,
            value_kind TEXT NOT NULL,
            value_json TEXT NOT NULL,
            evidence_status TEXT NOT NULL,
            locator TEXT,
            notes TEXT,
            PRIMARY KEY(file_id, property_key, occurrence)
        );

        CREATE TABLE IF NOT EXISTS resource_symbols (
            file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
            symbol_kind TEXT NOT NULL,
            ordinal INTEGER NOT NULL,
            symbol_name TEXT NOT NULL,
            locator TEXT,
            evidence_status TEXT NOT NULL,
            PRIMARY KEY(file_id, symbol_kind, ordinal)
        );

        CREATE TABLE IF NOT EXISTS resource_dependencies (
            id INTEGER PRIMARY KEY,
            source_file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
            relation_kind TEXT NOT NULL,
            target_path TEXT NOT NULL,
            normalized_target_path TEXT NOT NULL,
            target_extension TEXT NOT NULL,
            target_file_id INTEGER REFERENCES files(id) ON DELETE SET NULL,
            resolution_status TEXT NOT NULL,
            evidence_status TEXT NOT NULL,
            confidence TEXT NOT NULL,
            locator TEXT,
            notes TEXT,
            UNIQUE(source_file_id, relation_kind, normalized_target_path, locator)
        );

        CREATE TABLE IF NOT EXISTS container_format_assignments (
            container_id INTEGER PRIMARY KEY REFERENCES containers(id) ON DELETE CASCADE,
            format_key TEXT NOT NULL REFERENCES resource_formats(format_key),
            recognition_status TEXT NOT NULL,
            decode_status TEXT NOT NULL,
            parser_revision INTEGER NOT NULL,
            summary_json TEXT,
            error TEXT,
            analyzed_utc TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS container_properties (
            container_id INTEGER NOT NULL REFERENCES containers(id) ON DELETE CASCADE,
            property_key TEXT NOT NULL,
            value_kind TEXT NOT NULL,
            value_json TEXT NOT NULL,
            evidence_status TEXT NOT NULL,
            locator TEXT,
            notes TEXT,
            PRIMARY KEY(container_id, property_key)
        );

        CREATE TABLE IF NOT EXISTS resource_evidence (
            id INTEGER PRIMARY KEY,
            format_key TEXT REFERENCES resource_formats(format_key),
            variant_id INTEGER REFERENCES resource_format_variants(id),
            file_id INTEGER REFERENCES files(id) ON DELETE CASCADE,
            container_id INTEGER REFERENCES containers(id) ON DELETE CASCADE,
            platform_id INTEGER REFERENCES platforms(id),
            corpus_id INTEGER REFERENCES corpora(id),
            property_key TEXT,
            evidence_kind TEXT NOT NULL,
            source_path TEXT NOT NULL,
            locator TEXT,
            observation TEXT NOT NULL,
            confidence TEXT NOT NULL,
            source_sha256 TEXT,
            created_utc TEXT NOT NULL,
            CHECK(format_key IS NOT NULL OR file_id IS NOT NULL OR
                  container_id IS NOT NULL)
        );

        CREATE TABLE IF NOT EXISTS file_occurrences (
            id INTEGER PRIMARY KEY,
            file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
            container_id INTEGER NOT NULL REFERENCES containers(id) ON DELETE CASCADE,
            occurrence_key TEXT NOT NULL,
            physical_path TEXT NOT NULL,
            entry_index INTEGER,
            byte_offset INTEGER,
            byte_size INTEGER NOT NULL,
            last_write_utc_ticks INTEGER NOT NULL,
            last_scan_id INTEGER NOT NULL REFERENCES scans(id),
            UNIQUE(container_id, occurrence_key)
        );

        CREATE TABLE IF NOT EXISTS objects (
            file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
            object_index INTEGER NOT NULL,
            object_id INTEGER NOT NULL,
            parent_index INTEGER,
            name TEXT NOT NULL,
            type_hash INTEGER NOT NULL REFERENCES classes(type_hash),
            table_offset INTEGER NOT NULL,
            logical_offset INTEGER NOT NULL,
            physical_offset INTEGER NOT NULL,
            serialized_size INTEGER NOT NULL,
            signature_matches INTEGER NOT NULL,
            within_data_section INTEGER NOT NULL,
            field_count INTEGER NOT NULL,
            field_shape TEXT NOT NULL,
            field_parse_error TEXT,
            PRIMARY KEY(file_id, object_index)
        );

        CREATE TABLE IF NOT EXISTS direct_fields (
            file_id INTEGER NOT NULL,
            object_index INTEGER NOT NULL,
            field_index INTEGER NOT NULL,
            section_index INTEGER NOT NULL,
            is_section_terminator INTEGER NOT NULL,
            field_type INTEGER NOT NULL,
            occurrence INTEGER NOT NULL,
            raw_header INTEGER NOT NULL,
            size_kind TEXT NOT NULL,
            header_size INTEGER NOT NULL,
            payload_size INTEGER NOT NULL,
            relative_header_offset INTEGER NOT NULL,
            relative_payload_offset INTEGER NOT NULL,
            absolute_header_offset INTEGER NOT NULL,
            absolute_payload_offset INTEGER NOT NULL,
            semantic_key TEXT,
            payload_layout TEXT,
            decoded_value TEXT,
            is_decoded INTEGER NOT NULL DEFAULT 0,
            payload_preview BLOB NOT NULL,
            payload_sha256 TEXT,
            PRIMARY KEY(file_id, object_index, field_index),
            FOREIGN KEY(file_id, object_index)
                REFERENCES objects(file_id, object_index) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS class_variants (
            id INTEGER PRIMARY KEY,
            type_hash INTEGER NOT NULL REFERENCES classes(type_hash),
            scope_kind TEXT NOT NULL,
            scope_key TEXT NOT NULL,
            variant_key TEXT NOT NULL,
            display_name TEXT NOT NULL,
            status TEXT NOT NULL DEFAULT 'hypothesis',
            discriminator_json TEXT,
            notes TEXT,
            created_utc TEXT NOT NULL,
            updated_utc TEXT NOT NULL,
            UNIQUE(type_hash, scope_kind, scope_key, variant_key),
            CHECK(scope_kind IN ('common','platform','corpus'))
        );

        CREATE TABLE IF NOT EXISTS object_variant_assignments (
            file_id INTEGER NOT NULL,
            object_index INTEGER NOT NULL,
            variant_id INTEGER NOT NULL REFERENCES class_variants(id) ON DELETE CASCADE,
            confidence TEXT NOT NULL,
            evidence TEXT,
            PRIMARY KEY(file_id, object_index, variant_id),
            FOREIGN KEY(file_id, object_index)
                REFERENCES objects(file_id, object_index) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS field_definitions (
            id INTEGER PRIMARY KEY,
            type_hash INTEGER NOT NULL REFERENCES classes(type_hash),
            scope_kind TEXT NOT NULL,
            scope_key TEXT NOT NULL,
            section_from_end INTEGER NOT NULL DEFAULT 0,
            field_type INTEGER NOT NULL,
            occurrence INTEGER NOT NULL DEFAULT -1,
            semantic_key TEXT NOT NULL,
            display_name TEXT NOT NULL,
            value_kind TEXT,
            payload_layout TEXT NOT NULL,
            editable_status TEXT NOT NULL DEFAULT 'read_only_research',
            evidence_status TEXT NOT NULL,
            constraints_json TEXT,
            notes TEXT,
            UNIQUE(type_hash, scope_kind, scope_key, section_from_end,
                   field_type, occurrence),
            CHECK(scope_kind IN ('common','platform','corpus'))
        );

        CREATE TABLE IF NOT EXISTS evidence (
            id INTEGER PRIMARY KEY,
            type_hash INTEGER REFERENCES classes(type_hash),
            field_definition_id INTEGER REFERENCES field_definitions(id),
            variant_id INTEGER REFERENCES class_variants(id),
            platform_id INTEGER REFERENCES platforms(id),
            corpus_id INTEGER REFERENCES corpora(id),
            evidence_kind TEXT NOT NULL,
            source_path TEXT NOT NULL,
            locator TEXT,
            observation TEXT NOT NULL,
            confidence TEXT NOT NULL,
            source_sha256 TEXT,
            created_utc TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS native_types (
            id INTEGER PRIMARY KEY,
            type_hash INTEGER NOT NULL,
            class_name TEXT NOT NULL UNIQUE,
            owner_scope TEXT NOT NULL,
            on_pc INTEGER NOT NULL DEFAULT 0,
            on_ps2 INTEGER NOT NULL DEFAULT 0,
            pc_base_hash INTEGER,
            ps2_base_hash INTEGER,
            pc_registration_locator TEXT,
            ps2_registration_locator TEXT,
            updated_utc TEXT NOT NULL,
            CHECK(owner_scope IN ('engine','game')),
            CHECK(on_pc IN (0,1)),
            CHECK(on_ps2 IN (0,1)),
            CHECK(on_pc=1 OR on_ps2=1)
        );

        CREATE TABLE IF NOT EXISTS native_type_scopes (
            native_type_id INTEGER NOT NULL REFERENCES native_types(id)
                ON DELETE CASCADE,
            scope_key TEXT NOT NULL,
            object_count INTEGER,
            resource_count INTEGER,
            evidence_status TEXT NOT NULL,
            provenance TEXT NOT NULL,
            updated_utc TEXT NOT NULL,
            PRIMARY KEY(native_type_id, scope_key),
            CHECK(scope_key IN ('engine','game','smo','san','smo_san'))
        );

        CREATE TABLE IF NOT EXISTS native_research_progress (
            native_type_id INTEGER PRIMARY KEY REFERENCES native_types(id)
                ON DELETE CASCADE,
            research_status TEXT NOT NULL,
            pc_status TEXT NOT NULL,
            ps2_status TEXT NOT NULL,
            coverage_score REAL NOT NULL,
            lower_bound REAL NOT NULL,
            upper_bound REAL NOT NULL,
            priority_tier INTEGER NOT NULL DEFAULT 3,
            summary TEXT NOT NULL,
            updated_utc TEXT NOT NULL,
            CHECK(research_status IN
                ('not_started','identified','scouted','partial','substantial','closed')),
            CHECK(pc_status IN
                ('not_started','deferred','identified','scouted','partial','substantial','closed')),
            CHECK(ps2_status IN
                ('not_started','deferred','identified','scouted','partial','substantial','closed')),
            CHECK(coverage_score BETWEEN 0.0 AND 100.0),
            CHECK(lower_bound BETWEEN 0.0 AND coverage_score),
            CHECK(upper_bound BETWEEN coverage_score AND 100.0),
            CHECK(priority_tier BETWEEN 0 AND 9)
        );

        CREATE TABLE IF NOT EXISTS native_research_evidence (
            id INTEGER PRIMARY KEY,
            native_type_id INTEGER REFERENCES native_types(id)
                ON DELETE CASCADE,
            platform_key TEXT NOT NULL,
            evidence_kind TEXT NOT NULL,
            source_path TEXT NOT NULL,
            locator TEXT NOT NULL DEFAULT '',
            observation TEXT NOT NULL,
            confidence TEXT NOT NULL,
            source_sha256 TEXT,
            created_utc TEXT NOT NULL,
            UNIQUE(native_type_id,platform_key,evidence_kind,source_path,locator),
            CHECK(platform_key IN ('common','pc','ps2')),
            CHECK(confidence IN ('confirmed','probable','hypothesis'))
        );

        CREATE TABLE IF NOT EXISTS native_research_imports (
            manifest_id TEXT PRIMARY KEY,
            source_path TEXT NOT NULL,
            source_sha256 TEXT NOT NULL,
            import_mode TEXT NOT NULL,
            imported_utc TEXT NOT NULL,
            CHECK(import_mode IN ('baseline','incremental'))
        );

        CREATE TABLE IF NOT EXISTS native_coverage_snapshots (
            id INTEGER PRIMARY KEY,
            scope_key TEXT NOT NULL,
            numerator REAL NOT NULL,
            denominator REAL NOT NULL,
            coverage_percent REAL NOT NULL,
            lower_bound REAL NOT NULL,
            upper_bound REAL NOT NULL,
            calculation_method TEXT NOT NULL,
            notes TEXT NOT NULL,
            created_utc TEXT NOT NULL,
            CHECK(scope_key IN ('all','engine','game','smo_san')),
            CHECK(denominator > 0.0),
            CHECK(coverage_percent BETWEEN 0.0 AND 100.0),
            CHECK(lower_bound BETWEEN 0.0 AND coverage_percent),
            CHECK(upper_bound BETWEEN coverage_percent AND 100.0),
            UNIQUE(scope_key,created_utc)
        );

        CREATE TABLE IF NOT EXISTS resource_pairs (
            id INTEGER PRIMARY KEY,
            pc_file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
            ps2_file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
            match_method TEXT NOT NULL,
            confidence TEXT NOT NULL,
            evidence TEXT,
            UNIQUE(pc_file_id, ps2_file_id)
        );

        CREATE TABLE IF NOT EXISTS object_pairs (
            id INTEGER PRIMARY KEY,
            resource_pair_id INTEGER NOT NULL REFERENCES resource_pairs(id) ON DELETE CASCADE,
            pc_object_index INTEGER NOT NULL,
            ps2_object_index INTEGER NOT NULL,
            match_method TEXT NOT NULL,
            confidence TEXT NOT NULL,
            evidence TEXT,
            UNIQUE(resource_pair_id, pc_object_index, ps2_object_index)
        );

        CREATE INDEX IF NOT EXISTS ix_files_corpus_path
            ON files(corpus_id, normalized_path);
        CREATE INDEX IF NOT EXISTS ix_file_formats_key
            ON file_format_assignments(format_key, decode_status);
        CREATE INDEX IF NOT EXISTS ix_resource_properties_key
            ON resource_properties(property_key);
        CREATE INDEX IF NOT EXISTS ix_resource_symbols_name
            ON resource_symbols(symbol_kind, symbol_name COLLATE NOCASE);
        CREATE INDEX IF NOT EXISTS ix_resource_dependencies_target
            ON resource_dependencies(normalized_target_path, resolution_status);
        CREATE INDEX IF NOT EXISTS ix_occurrences_file
            ON file_occurrences(file_id);
        CREATE INDEX IF NOT EXISTS ix_objects_type_hash
            ON objects(type_hash);
        CREATE INDEX IF NOT EXISTS ix_objects_parent
            ON objects(file_id,parent_index);
        CREATE INDEX IF NOT EXISTS ix_objects_name
            ON objects(name COLLATE NOCASE);
        CREATE INDEX IF NOT EXISTS ix_fields_shape
            ON direct_fields(field_type, payload_size, section_index);
        CREATE INDEX IF NOT EXISTS ix_fields_semantic
            ON direct_fields(semantic_key);
        CREATE INDEX IF NOT EXISTS ix_native_types_scope
            ON native_types(owner_scope,on_pc,on_ps2);
        CREATE INDEX IF NOT EXISTS ix_native_type_scopes_key
            ON native_type_scopes(scope_key,native_type_id);
        CREATE INDEX IF NOT EXISTS ix_native_progress_priority
            ON native_research_progress(priority_tier,coverage_score);
        CREATE INDEX IF NOT EXISTS ix_native_evidence_type
            ON native_research_evidence(native_type_id,platform_key,evidence_kind);
        CREATE INDEX IF NOT EXISTS ix_native_coverage_scope
            ON native_coverage_snapshots(scope_key,created_utc);

        CREATE VIEW IF NOT EXISTS corpus_file_type_counts AS
        SELECT c.corpus_key, p.platform_key, f.extension,
               COUNT(*) AS unique_resource_versions,
               COUNT(DISTINCT f.normalized_path) AS logical_paths,
               COUNT(fo.id) AS physical_occurrences
        FROM files f
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        LEFT JOIN file_occurrences fo ON fo.file_id=f.id
        GROUP BY c.id, f.extension;

        CREATE VIEW IF NOT EXISTS corpus_resource_format_counts AS
        SELECT c.corpus_key, p.platform_key, a.format_key,
               rf.display_name, a.recognition_status, a.decode_status,
               COUNT(*) AS unique_resource_versions,
               COUNT(DISTINCT f.normalized_path) AS logical_paths,
               SUM((SELECT COUNT(*) FROM file_occurrences fo
                    WHERE fo.file_id=f.id)) AS physical_occurrences
        FROM file_format_assignments a
        JOIN resource_formats rf ON rf.format_key=a.format_key
        JOIN files f ON f.id=a.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        GROUP BY c.id, a.format_key, a.recognition_status, a.decode_status;

        CREATE VIEW IF NOT EXISTS resource_dependency_status_counts AS
        SELECT c.corpus_key, a.format_key, d.relation_kind,
               d.resolution_status, COUNT(*) AS dependency_count
        FROM resource_dependencies d
        JOIN files f ON f.id=d.source_file_id
        JOIN file_format_assignments a ON a.file_id=f.id
        JOIN corpora c ON c.id=f.corpus_id
        GROUP BY c.id, a.format_key, d.relation_kind, d.resolution_status;

        CREATE VIEW IF NOT EXISTS corpus_class_totals AS
        SELECT c.corpus_key, p.platform_key, o.type_hash, cl.engine_name,
               COUNT(*) AS unique_object_count,
               COUNT(DISTINCT o.file_id) AS unique_resource_count,
               SUM((SELECT COUNT(*) FROM file_occurrences fo
                    WHERE fo.file_id=o.file_id)) AS physical_object_occurrences
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        JOIN classes cl ON cl.type_hash=o.type_hash
        GROUP BY c.id, o.type_hash;

        CREATE VIEW IF NOT EXISTS platform_class_presence AS
        SELECT o.type_hash, cl.engine_name,
               MAX(CASE WHEN p.platform_key='pc' THEN 1 ELSE 0 END) AS on_pc,
               MAX(CASE WHEN p.platform_key='ps2' THEN 1 ELSE 0 END) AS on_ps2,
               COUNT(DISTINCT c.id) AS corpus_count
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        JOIN classes cl ON cl.type_hash=o.type_hash
        GROUP BY o.type_hash;

        CREATE VIEW IF NOT EXISTS class_variant_candidates AS
        SELECT c.corpus_key, p.platform_key, o.type_hash, cl.engine_name,
               o.serialized_size, o.field_shape,
               COUNT(*) AS object_count,
               COUNT(DISTINCT o.file_id) AS resource_count
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        JOIN classes cl ON cl.type_hash=o.type_hash
        GROUP BY c.id, o.type_hash, o.serialized_size, o.field_shape;

        CREATE VIEW IF NOT EXISTS field_shape_counts AS
        SELECT c.corpus_key, p.platform_key, o.type_hash, cl.engine_name,
               d.section_index, d.field_type, d.payload_size,
               COUNT(*) AS occurrence_count,
               COUNT(DISTINCT d.file_id) AS resource_count
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id
                          AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        JOIN classes cl ON cl.type_hash=o.type_hash
        WHERE d.is_section_terminator=0
        GROUP BY c.id, o.type_hash, d.section_index, d.field_type, d.payload_size;

        CREATE VIEW IF NOT EXISTS latest_native_coverage AS
        SELECT snapshot.*
        FROM native_coverage_snapshots snapshot
        JOIN (
            SELECT scope_key, MAX(created_utc) AS created_utc
            FROM native_coverage_snapshots
            GROUP BY scope_key
        ) latest ON latest.scope_key=snapshot.scope_key
                   AND latest.created_utc=snapshot.created_utc;

        CREATE VIEW IF NOT EXISTS native_smo_san_progress AS
        SELECT type.class_name, type.type_hash, scope.scope_key,
               scope.object_count, scope.resource_count,
               COALESCE(progress.research_status,'not_started') AS research_status,
               COALESCE(progress.pc_status,'not_started') AS pc_status,
               COALESCE(progress.ps2_status,'deferred') AS ps2_status,
               COALESCE(progress.coverage_score,0.0) AS coverage_score,
               COALESCE(progress.lower_bound,0.0) AS lower_bound,
               COALESCE(progress.upper_bound,0.0) AS upper_bound,
               COALESCE(progress.priority_tier,9) AS priority_tier,
               COALESCE(progress.summary,'No native research recorded.') AS summary
        FROM native_type_scopes scope
        JOIN native_types type ON type.id=scope.native_type_id
        LEFT JOIN native_research_progress progress
               ON progress.native_type_id=type.id
        WHERE scope.scope_key IN ('smo','san');
        """;

    public static void Initialize(SqliteConnection connection)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = Sql;
        command.ExecuteNonQuery();

        using SqliteCommand version = connection.CreateCommand();
        version.CommandText = """
            INSERT INTO schema_info(key,value) VALUES('schema_version',$version)
            ON CONFLICT(key) DO NOTHING;
            SELECT value FROM schema_info WHERE key='schema_version';
            """;
        version.Parameters.AddWithValue("$version", Version.ToString());
        string actual = Convert.ToString(version.ExecuteScalar()) ?? string.Empty;
        if (actual.Equals("3", StringComparison.Ordinal))
        {
            MigrateVersion3To4(connection);
            actual = "4";
        }
        if (actual.Equals("4", StringComparison.Ordinal))
        {
            MigrateVersion4To5(connection);
            actual = Version.ToString();
        }
        if (!actual.Equals(Version.ToString(), StringComparison.Ordinal))
        {
            throw new InvalidDataException(
                $"Unsupported research database schema {actual}; expected {Version}.");
        }

        using SqliteCommand seed = connection.CreateCommand();
        seed.CommandText = """
            INSERT INTO platforms(platform_key,display_name,architecture)
            VALUES('pc','PC','x86-le') ON CONFLICT(platform_key) DO NOTHING;
            INSERT INTO platforms(platform_key,display_name,architecture)
            VALUES('ps2','PlayStation 2','mips32-le')
            ON CONFLICT(platform_key) DO NOTHING;
            """;
        seed.ExecuteNonQuery();
    }

    private static void MigrateVersion3To4(SqliteConnection connection)
    {
        if (!ColumnExists(connection, "executables", "file_id"))
        {
            using SqliteCommand alter = connection.CreateCommand();
            alter.CommandText =
                "ALTER TABLE executables ADD COLUMN file_id INTEGER REFERENCES files(id) ON DELETE SET NULL;";
            alter.ExecuteNonQuery();
        }
        using SqliteCommand version = connection.CreateCommand();
        version.CommandText =
            "UPDATE schema_info SET value=$version WHERE key='schema_version';";
        version.Parameters.AddWithValue("$version", "4");
        version.ExecuteNonQuery();
    }

    private static void MigrateVersion4To5(SqliteConnection connection)
    {
        // The idempotent CREATE TABLE/VIEW block runs before version dispatch,
        // so migration only needs to publish that the native research layer is
        // now part of the canonical schema.
        using SqliteCommand version = connection.CreateCommand();
        version.CommandText =
            "UPDATE schema_info SET value=$version WHERE key='schema_version';";
        version.Parameters.AddWithValue("$version", Version.ToString());
        version.ExecuteNonQuery();
    }

    private static bool ColumnExists(
        SqliteConnection connection,
        string table,
        string column)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = $"PRAGMA table_info({table});";
        using SqliteDataReader reader = command.ExecuteReader();
        while (reader.Read())
        {
            if (reader.GetString(1).Equals(column, StringComparison.OrdinalIgnoreCase))
                return true;
        }
        return false;
    }

    public static void Validate(SqliteConnection connection)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText =
            "SELECT value FROM schema_info WHERE key='schema_version';";
        string actual = Convert.ToString(command.ExecuteScalar()) ?? string.Empty;
        if (!actual.Equals(Version.ToString(), StringComparison.Ordinal))
        {
            throw new InvalidDataException(
                $"Unsupported research database schema {actual}; expected {Version}.");
        }
    }

}
