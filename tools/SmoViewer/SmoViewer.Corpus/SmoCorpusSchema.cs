using Microsoft.Data.Sqlite;

namespace SmoViewer.Corpus;

internal static class SmoCorpusSchema
{
    public const int Version = 2;
    public const int ScannerRevision = 2;

    private const string Sql = """
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS schema_info (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS scans (
            id INTEGER PRIMARY KEY,
            started_utc TEXT NOT NULL,
            finished_utc TEXT,
            source_root TEXT NOT NULL,
            scanner_revision INTEGER NOT NULL,
            discovered_files INTEGER NOT NULL DEFAULT 0,
            scanned_files INTEGER NOT NULL DEFAULT 0,
            unchanged_files INTEGER NOT NULL DEFAULT 0,
            failed_files INTEGER NOT NULL DEFAULT 0,
            removed_files INTEGER NOT NULL DEFAULT 0
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
            relative_path TEXT NOT NULL UNIQUE COLLATE NOCASE,
            file_name TEXT NOT NULL,
            family TEXT NOT NULL,
            byte_size INTEGER NOT NULL,
            last_write_utc_ticks INTEGER NOT NULL,
            sha256 TEXT,
            serializer_version INTEGER,
            ffps_unknown08 INTEGER,
            platform_mask INTEGER,
            data_start INTEGER,
            data_size INTEGER,
            object_count INTEGER,
            parse_status TEXT NOT NULL,
            parse_error TEXT,
            scanner_revision INTEGER NOT NULL,
            last_scan_id INTEGER NOT NULL REFERENCES scans(id),
            scanned_utc TEXT NOT NULL
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
            variant_key TEXT NOT NULL,
            display_name TEXT NOT NULL,
            status TEXT NOT NULL DEFAULT 'hypothesis',
            discriminator_json TEXT,
            notes TEXT,
            created_utc TEXT NOT NULL,
            updated_utc TEXT NOT NULL,
            UNIQUE(type_hash, variant_key)
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
            UNIQUE(type_hash, section_from_end, field_type, occurrence)
        );

        CREATE INDEX IF NOT EXISTS ix_objects_type_hash
            ON objects(type_hash);
        CREATE INDEX IF NOT EXISTS ix_objects_name
            ON objects(name COLLATE NOCASE);
        CREATE INDEX IF NOT EXISTS ix_fields_shape
            ON direct_fields(field_type, payload_size, section_index);
        CREATE INDEX IF NOT EXISTS ix_fields_semantic
            ON direct_fields(semantic_key);

        CREATE VIEW IF NOT EXISTS file_class_counts AS
        SELECT o.file_id, f.relative_path, o.type_hash, c.engine_name,
               COUNT(*) AS object_count
        FROM objects o
        JOIN files f ON f.id = o.file_id
        JOIN classes c ON c.type_hash = o.type_hash
        GROUP BY o.file_id, o.type_hash;

        CREATE VIEW IF NOT EXISTS class_totals AS
        SELECT c.type_hash, c.engine_name, c.is_known,
               COUNT(o.object_index) AS object_count,
               COUNT(DISTINCT o.file_id) AS file_count,
               COUNT(DISTINCT f.family) AS family_count
        FROM classes c
        LEFT JOIN objects o ON o.type_hash = c.type_hash
        LEFT JOIN files f ON f.id = o.file_id
        GROUP BY c.type_hash;

        CREATE VIEW IF NOT EXISTS class_variant_candidates AS
        SELECT o.type_hash, c.engine_name, o.serialized_size, o.field_shape,
               COUNT(*) AS object_count,
               COUNT(DISTINCT o.file_id) AS file_count
        FROM objects o
        JOIN classes c ON c.type_hash = o.type_hash
        GROUP BY o.type_hash, o.serialized_size, o.field_shape;

        CREATE VIEW IF NOT EXISTS field_shape_counts AS
        SELECT o.type_hash, c.engine_name, d.section_index, d.field_type,
               d.payload_size, COUNT(*) AS occurrence_count,
               COUNT(DISTINCT d.file_id) AS file_count
        FROM direct_fields d
        JOIN objects o ON o.file_id = d.file_id
                          AND o.object_index = d.object_index
        JOIN classes c ON c.type_hash = o.type_hash
        WHERE d.is_section_terminator = 0
        GROUP BY o.type_hash, d.section_index, d.field_type, d.payload_size;
        """;

    public static void Initialize(SqliteConnection connection)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = Sql;
        command.ExecuteNonQuery();

        using SqliteCommand version = connection.CreateCommand();
        version.CommandText = """
            INSERT INTO schema_info(key, value) VALUES('schema_version', $version)
            ON CONFLICT(key) DO NOTHING;
            SELECT value FROM schema_info WHERE key = 'schema_version';
            """;
        version.Parameters.AddWithValue("$version", Version.ToString());
        string actual = Convert.ToString(version.ExecuteScalar()) ?? string.Empty;
        if (!actual.Equals(Version.ToString(), StringComparison.Ordinal))
        {
            throw new InvalidDataException(
                $"Unsupported SMO corpus database schema {actual}; expected {Version}.");
        }
    }
}
