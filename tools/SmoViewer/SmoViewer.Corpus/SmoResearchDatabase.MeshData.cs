using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeMeshData(
        string databasePath,
        SmoResearchClassReport report)
    {
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Fields.Any(item => item.SectionIndex != 0 ||
                item.SectionFromEnd != 0 || item.FieldType is not (0 or 1 or 2)))
        {
            throw new InvalidDataException(
                "spMeshData corpus no longer matches the validated one-section " +
                "cross/platform/bounds contract.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        List<MeshObservation> observations = LoadMeshObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected {objectCount} decoded spMeshData objects, got " +
                $"{observations.Count}.");
        }

        Dictionary<(string Corpus,string Variant),int> variantCounts = observations
            .GroupBy(item => (item.CorpusKey,item.VariantKey))
            .ToDictionary(group => group.Key,group => group.Count());
        RequireMeshVariantCounts(variantCounts);

        Dictionary<string,string> pcWorking = BuildMeshPathMap(
            observations,"pc-working");
        Dictionary<string,string> pcPristine = BuildMeshPathMap(
            observations,"pc-pristine");
        (int pcCommon,int pcEqual) = ComparePayloadPathMaps(pcWorking,pcPristine);
        if (pcCommon != 396 || pcEqual != 396)
            throw new InvalidDataException(
                "PC mesh resource pairing is incomplete or differs by payload.");

        MeshPlatformComparison cross = CompareMeshPlatforms(observations);
        if (cross.CommonResources != 303 || cross.EqualCountResources != 202 ||
            cross.PairedMeshes != 17_761 || cross.ComparableMeshes != 17_761 ||
            cross.EqualVertexFormat != 6_614 ||
            cross.EqualPrimitiveCount != 17_107 ||
            cross.EqualVertexCount != 3_268)
        {
            throw new InvalidDataException(
                "PC/PS2 mesh pairing no longer matches the validated corpus.");
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] commonTokens =
        [
            "spMesh","spMeshData","spMeshDataSerializer",
            "spPlatformSpecificMeshData","spPS2MeshData",
            "spPS2MeshDataSerializer","esfMeshDataCrossPatform",
            "esfMeshDataPlatformSpecific","esfMeshDataBoundingBox"
        ];
        RequireAsciiTokens(pcExecutable.Path,commonTokens);
        RequireAsciiTokens(ps2Executable.Path,commonTokens);
        RequireAsciiTokens(pcExecutable.Path,
            "spDXMeshData","spDXMeshDataSerializer");

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
        using (SqliteCommand clearVariants = CreateCommand(connection,transaction,"""
                   DELETE FROM class_variants
                   WHERE type_hash=$hash AND variant_key LIKE 'mesh_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        UpsertMeshFieldDefinitions(connection,transaction,report.TypeHash,observations);
        AnnotateMeshFields(connection,transaction,observations);
        Dictionary<string,int> variantIds = InsertMeshVariants(
            connection,transaction,report.TypeHash,observations,now);
        AssignMeshVariants(connection,transaction,observations,variantIds);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "spMeshData/DX/PS2 serializer registrations and mesh field-name strings",
            "The PC executable names cross-platform field 0, platform-specific " +
            "field 1 and bounding-box field 2, and includes both DX and PS2 " +
            "mesh-data factories.",pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "MIPS spPS2MeshDataSerializer and common field-name strings",
            "The PS2 executable independently exposes the same representation " +
            "container and its native PS2 mesh classes.",ps2Executable.Sha256,now);

        string counts = string.Join(", ",variantCounts
            .OrderBy(item => item.Key.Corpus,StringComparer.Ordinal)
            .ThenBy(item => item.Key.Variant,StringComparer.Ordinal)
            .Select(item => $"{item.Key.Corpus}/{item.Key.Variant}={item.Value}"));
        Dictionary<string,int> representationCounts = observations
            .SelectMany(item => new[] { item.CrossPlatform,item.PlatformSpecific })
            .Where(item => item is not null)
            .GroupBy(item => item!.Kind)
            .ToDictionary(group => group.Key,group => group.Count());
        int strictGeometry = observations.Count(item =>
            item.CorpusKey.StartsWith("pc-",StringComparison.Ordinal) &&
            (item.PlatformSpecific?.StrictGeometryDecoded == true ||
             item.CrossPlatform?.StrictGeometryDecoded == true));
        if (observations.Sum(item => item.Fields.Count) != 87_330 ||
            representationCounts.GetValueOrDefault(
                nameof(SmoMeshRepresentationKind.CrossPlatform)) != 1_865 ||
            representationCounts.GetValueOrDefault(
                nameof(SmoMeshRepresentationKind.Direct3D)) != 42_454 ||
            representationCounts.GetValueOrDefault(
                nameof(SmoMeshRepresentationKind.Ps2Native)) != 22_151 ||
            strictGeometry != 44_298)
        {
            throw new InvalidDataException(
                "spMeshData representation or strict-preview totals changed from " +
                "the validated corpus.");
        }
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "all unique spMeshData objects in PC working, PC pristine and PS2 pristine",
            $"{objectCount} objects and {observations.Sum(item => item.Fields.Count)} " +
            $"non-terminal direct fields strictly bounded. Variants: {counts}. " +
            $"Representations: " + string.Join(", ",representationCounts
                .OrderBy(item => item.Key).Select(item => $"{item.Key}={item.Value}")) +
            $". Strict PC geometry previews: {strictGeometry}/45298.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus mesh ordinal; full representation hashes for PC pairs",
            $"PC working/pristine equal mesh resources: {pcEqual}/{pcCommon}. " +
            $"PC/PS2 common resources: {cross.CommonResources}; equal mesh counts: " +
            $"{cross.EqualCountResources}; paired meshes: {cross.PairedMeshes}. " +
            $"Comparable PC/PS2 headers: {cross.ComparableMeshes}. Header matches: " +
            $"format={cross.EqualVertexFormat}, primitive count=" +
            $"{cross.EqualPrimitiveCount}, vertex count={cross.EqualVertexCount}.",
            null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='geometry_resource',
                       description='Cross-platform, Direct3D or native PS2 mesh buffers and bounds',
                       decode_status='read_only_decode',
                       notes='All observed representation containers and PC geometry layouts are structurally decoded. Native PS2 DMA/VIF payload bytes remain bounded read-only data; mutation safety is untested.'
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
            report.Profiles.Sum(item => item.UniqueResourceCount),variantIds.Count,
            observations.Count,evidenceRows,
            $"Three field IDs and seven representation-container variants decoded " +
            $"across {objectCount} meshes. PC equal resources {pcEqual}/{pcCommon}; " +
            $"PC/PS2 paired meshes {cross.PairedMeshes}. Native PS2 DMA/VIF payload " +
            "contents and mutation safety remain read-only research.");
    }

    private static List<MeshObservation> LoadMeshObservations(
        SqliteConnection connection)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT DISTINCT f.id,c.corpus_key,p.platform_key,c.source_kind,
                   c.source_root,f.relative_path,ct.relative_path,fo.byte_offset,
                   fo.byte_size
            FROM files f
            JOIN corpora c ON c.id=f.corpus_id
            JOIN platforms p ON p.id=c.platform_id
            JOIN objects o ON o.file_id=f.id AND o.type_hash=$hash
            LEFT JOIN file_occurrences fo ON fo.id=(
                SELECT MIN(inside_fo.id) FROM file_occurrences inside_fo
                WHERE inside_fo.file_id=f.id)
            LEFT JOIN containers ct ON ct.id=fo.container_id
            ORDER BY c.id,f.normalized_path;
            """;
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.MeshData);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<MeshFileLocation>();
        while (reader.Read())
        {
            locations.Add(new MeshFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var observations = new List<MeshObservation>();
        foreach (MeshFileLocation location in locations)
        {
            byte[] bytes = ReadMeshResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.MeshData))
            {
                if (!SmoMeshDataDecoder.TryDecode(
                        document,entry,out SmoMeshDataInfo? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                observations.Add(CreateMeshObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return observations;
    }

    private static byte[] ReadMeshResource(MeshFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
        {
            return File.ReadAllBytes(Path.Combine(location.SourceRoot,
                location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        }
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
        {
            throw new InvalidDataException("PCK mesh occurrence is incomplete.");
        }
        string archive = Path.Combine(location.SourceRoot,
            location.ContainerPath.Replace('/',Path.DirectorySeparatorChar));
        using var stream = new FileStream(
            archive,FileMode.Open,FileAccess.Read,FileShare.Read);
        stream.Position = location.ByteOffset.Value;
        byte[] data = new byte[checked((int)location.ByteSize)];
        stream.ReadExactly(data);
        return data;
    }

    private static MeshObservation CreateMeshObservation(
        MeshFileLocation location,
        SmoDocument document,
        SmoObjectEntry entry,
        int ordinal,
        SmoMeshDataInfo decoded)
    {
        MeshRepresentationObservation? cross = ToMeshRepresentation(
            decoded.CrossPlatform,document);
        MeshRepresentationObservation? platform = ToMeshRepresentation(
            decoded.PlatformSpecific,document);
        IReadOnlyList<SmoObjectField> direct = SmoObjectFieldReader.Read(document,entry);
        string variantKey = (cross?.Kind,platform?.Kind,decoded.BoundingBox is not null)
            switch
            {
                (nameof(SmoMeshRepresentationKind.CrossPlatform),null,false) =>
                    "mesh_cross_only",
                (null,nameof(SmoMeshRepresentationKind.Direct3D),false) =>
                    "mesh_direct3d_only",
                (nameof(SmoMeshRepresentationKind.CrossPlatform),
                    nameof(SmoMeshRepresentationKind.Direct3D),false) =>
                    "mesh_cross_direct3d",
                (null,nameof(SmoMeshRepresentationKind.Ps2Native),false) =>
                    "mesh_ps2_native_only",
                (nameof(SmoMeshRepresentationKind.CrossPlatform),
                    nameof(SmoMeshRepresentationKind.Ps2Native),false) =>
                    "mesh_cross_ps2_native",
                (null,nameof(SmoMeshRepresentationKind.Ps2Native),true) =>
                    "mesh_ps2_native_bounds",
                (nameof(SmoMeshRepresentationKind.CrossPlatform),
                    nameof(SmoMeshRepresentationKind.Ps2Native),true) =>
                    "mesh_cross_ps2_native_bounds",
                _ => throw new InvalidDataException(
                    $"Unexpected mesh representation tuple in {location.RelativePath} " +
                    $"object {entry.Index}: cross={cross?.Kind ?? "<none>"}, " +
                    $"platform={platform?.Kind ?? "<none>"}, " +
                    $"bounds={decoded.BoundingBox is not null}; fields=" +
                    string.Join(",",direct.Select(item =>
                        $"{item.FieldType}/{(byte)item.SizeKind:X2}/{item.PayloadSize}")) + ".")
            };
        string summary = JsonSerializer.Serialize(new
        {
            crossPlatform = cross,
            platformSpecific = platform,
            decoded.BoundingBox
        });
        var annotations = new List<MeshFieldAnnotation>();
        for (int fieldIndex = 0;fieldIndex < direct.Count;fieldIndex++)
        {
            SmoObjectField field = direct[fieldIndex];
            if (field.FieldType == 0 && field.PayloadSize == 0)
                continue;
            (string Semantic,string Layout,object? Value) item = field.FieldType switch
            {
                0 => ("mesh.cross_platform",
                    "portable primitive/index/vertex buffer representation",cross),
                1 => ("mesh.platform_specific",
                    "Direct3D buffers or 40-byte PS2 header plus DMA qwords",platform),
                2 => ("mesh.bounding_box",
                    "Vector3 minimum plus Vector3 maximum",decoded.BoundingBox),
                _ => throw new InvalidDataException(
                    $"Unexpected mesh field {field.FieldType}.")
            };
            annotations.Add(new MeshFieldAnnotation(
                fieldIndex,field.FieldType,item.Semantic,item.Layout,
                JsonSerializer.Serialize(item.Value)));
        }
        return new MeshObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            location.PlatformKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            cross,platform,decoded.BoundingBox,variantKey,summary,
            annotations.AsReadOnly());
    }

    private static MeshRepresentationObservation? ToMeshRepresentation(
        SmoMeshRepresentationData? value,
        SmoDocument document)
    {
        if (value is null)
            return null;
        string hash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                value.AbsolutePayloadOffset,checked((int)value.PayloadSize))));
        return new MeshRepresentationObservation(
            value.Kind.ToString(),value.PayloadSize,hash,
            value.Geometry is not null,value.Pc,value.Ps2Native);
    }

    private static void RequireMeshVariantCounts(
        IReadOnlyDictionary<(string Corpus,string Variant),int> counts)
    {
        Dictionary<(string Corpus,string Variant),int> expected = new()
        {
            [("pc-working","mesh_cross_only")] = 793,
            [("pc-working","mesh_direct3d_only")] = 21_225,
            [("pc-working","mesh_cross_direct3d")] = 2,
            [("pc-working","mesh_ps2_native_only")] = 494,
            [("pc-working","mesh_cross_ps2_native")] = 135,
            [("pc-pristine","mesh_cross_only")] = 793,
            [("pc-pristine","mesh_direct3d_only")] = 21_225,
            [("pc-pristine","mesh_cross_direct3d")] = 2,
            [("pc-pristine","mesh_ps2_native_only")] = 494,
            [("pc-pristine","mesh_cross_ps2_native")] = 135,
            [("ps2-pristine","mesh_ps2_native_only")] = 33,
            [("ps2-pristine","mesh_ps2_native_bounds")] = 20_855,
            [("ps2-pristine","mesh_cross_ps2_native_bounds")] = 5
        };
        if (counts.Count != expected.Count || expected.Any(item =>
                !counts.TryGetValue(item.Key,out int count) || count != item.Value))
        {
            throw new InvalidDataException(
                "spMeshData representation-variant counts changed from the " +
                "validated corpus: " + string.Join(", ",counts
                    .OrderBy(item => item.Key.Corpus,StringComparer.Ordinal)
                    .ThenBy(item => item.Key.Variant,StringComparer.Ordinal)
                    .Select(item =>
                        $"{item.Key.Corpus}/{item.Key.Variant}={item.Value}")) + ".");
        }
    }

    private static void UpsertMeshFieldDefinitions(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<MeshObservation> observations)
    {
        long Count(int type) => observations.Sum(item =>
            (long)item.Fields.Count(field => field.FieldType == type));
        (int Type,string Semantic,string Display,string Kind,string Layout,string Notes)[]
            definitions =
        [
            (0,"mesh.cross_platform","Cross-platform mesh","mesh_buffers",
                "portable primitive/index/vertex buffer representation",
                $"Optional; observed {Count(0)} times."),
            (1,"mesh.platform_specific","Platform-specific mesh","mesh_buffers",
                "Direct3D E1 buffers or PS2 sphere/count/format/flags plus DMA qwords",
                $"Optional; observed {Count(1)} times."),
            (2,"mesh.bounding_box","Mesh bounding box","bounds3",
                "Vector3 minimum plus Vector3 maximum",
                $"Optional PS2 field; observed {Count(2)} times.")
        ];
        foreach (var definition in definitions)
        {
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',0,$type,-1,$semantic,$display,$kind,
                       $layout,'read_only_research',
                       'confirmed_both_executables_and_full_corpus',$constraints,$notes)
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
            command.Parameters.AddWithValue("$hash",(long)typeHash);
            command.Parameters.AddWithValue("$type",definition.Type);
            command.Parameters.AddWithValue("$semantic",definition.Semantic);
            command.Parameters.AddWithValue("$display",definition.Display);
            command.Parameters.AddWithValue("$kind",definition.Kind);
            command.Parameters.AddWithValue("$layout",definition.Layout);
            command.Parameters.AddWithValue("$constraints",JsonSerializer.Serialize(new
            {
                observedDirectOccurrences = Count(definition.Type),
                editable = false,
                mutationStatus = "not_tested"
            }));
            command.Parameters.AddWithValue("$notes",definition.Notes);
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotateMeshFields(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<MeshObservation> observations)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            UPDATE direct_fields SET semantic_key=$semantic,payload_layout=$layout,
                   decoded_value=$value,is_decoded=1
            WHERE file_id=$file AND object_index=$object AND field_index=$field;
            """);
        command.Parameters.Add("$semantic",SqliteType.Text);
        command.Parameters.Add("$layout",SqliteType.Text);
        command.Parameters.Add("$value",SqliteType.Text);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.Add("$field",SqliteType.Integer);
        foreach (MeshObservation observation in observations)
        foreach (MeshFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException("Could not annotate mesh field.");
        }
    }

    private static Dictionary<string,int> InsertMeshVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<MeshObservation> observations,
        string now)
    {
        (string Key,string Display,string Notes)[] variants =
        [
            ("mesh_cross_only","Cross-platform mesh only","Portable E0 buffers only."),
            ("mesh_direct3d_only","Direct3D mesh only","PC-native E1 buffers only."),
            ("mesh_cross_direct3d","Cross-platform plus Direct3D mesh",
                "Both portable and PC-native copies are retained."),
            ("mesh_ps2_native_only","PS2-native mesh without AABB",
                "Native DMA/VIF representation; field 2 is absent."),
            ("mesh_cross_ps2_native","Cross-platform plus PS2-native mesh without AABB",
                "Portable copy and native DMA/VIF representation are retained; field 2 is absent."),
            ("mesh_ps2_native_bounds","PS2-native mesh with AABB",
                "Native DMA/VIF representation plus 24-byte bounds."),
            ("mesh_cross_ps2_native_bounds","Cross-platform plus PS2-native mesh",
                "Portable copy, native DMA/VIF representation and bounds are retained.")
        ];
        var result = new Dictionary<string,int>(StringComparer.Ordinal);
        foreach ((string key,string display,string notes) in variants)
        {
            Dictionary<string,int> corpusCounts = observations
                .Where(item => item.VariantKey == key)
                .GroupBy(item => item.CorpusKey)
                .ToDictionary(group => group.Key,group => group.Count());
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO class_variants(
                    type_hash,scope_kind,scope_key,variant_key,display_name,status,
                    discriminator_json,notes,created_utc,updated_utc)
                VALUES($hash,'common','pc_ps2',$key,$display,'confirmed',
                       $discriminator,$notes,$utc,$utc);
                SELECT last_insert_rowid();
                """);
            command.Parameters.AddWithValue("$hash",(long)typeHash);
            command.Parameters.AddWithValue("$key",key);
            command.Parameters.AddWithValue("$display",display);
            command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
            {
                corpusCounts,
                representationAndBoundsStructure = key
            }));
            command.Parameters.AddWithValue("$notes",notes);
            command.Parameters.AddWithValue("$utc",now);
            result.Add(key,Convert.ToInt32(command.ExecuteScalar()));
        }
        return result;
    }

    private static void AssignMeshVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<MeshObservation> observations,
        IReadOnlyDictionary<string,int> variantIds)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spMeshData representation decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.Add("$variant",SqliteType.Integer);
        foreach (MeshObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$variant"].Value = variantIds[observation.VariantKey];
            command.ExecuteNonQuery();
        }
    }

    private static Dictionary<string,string> BuildMeshPathMap(
        IEnumerable<MeshObservation> observations,
        string corpusKey) => observations
        .Where(item => item.CorpusKey.Equals(corpusKey,StringComparison.Ordinal))
        .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
        .ToDictionary(group => group.Key,
            group => string.Join("|",group.OrderBy(item => item.Ordinal)
                .Select(item => item.SummaryJson)),StringComparer.Ordinal);

    private static MeshPlatformComparison CompareMeshPlatforms(
        IReadOnlyList<MeshObservation> observations)
    {
        Dictionary<string,MeshObservation[]> pc = observations
            .Where(item => item.CorpusKey == "pc-pristine")
            .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
            .ToDictionary(group => group.Key,
                group => group.OrderBy(item => item.Ordinal).ToArray(),
                StringComparer.Ordinal);
        Dictionary<string,MeshObservation[]> ps2 = observations
            .Where(item => item.CorpusKey == "ps2-pristine")
            .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
            .ToDictionary(group => group.Key,
                group => group.OrderBy(item => item.Ordinal).ToArray(),
                StringComparer.Ordinal);
        string[] paths = pc.Keys.Intersect(ps2.Keys,StringComparer.Ordinal).ToArray();
        int equalCounts = paths.Count(path => pc[path].Length == ps2[path].Length);
        int pairs = 0;
        int formatEqual = 0;
        int primitiveEqual = 0;
        int vertexEqual = 0;
        int comparable = 0;
        foreach (string path in paths)
        {
            if (pc[path].Length != ps2[path].Length)
                continue;
            for (int index = 0;index < pc[path].Length;index++)
            {
                pairs++;
                SmoPcMeshData? left =
                    pc[path][index].PlatformSpecific?.Pc ??
                    pc[path][index].CrossPlatform?.Pc;
                SmoPs2NativeMeshData? right = ps2[path][index].PlatformSpecific?.Ps2;
                if (left is null || right is null)
                    continue;
                comparable++;
                formatEqual += left.VertexFormat == right.VertexFormat ? 1 : 0;
                primitiveEqual += left.PrimitiveCount == right.PrimitiveCount ? 1 : 0;
                vertexEqual += left.VertexCount == right.VertexCount ? 1 : 0;
            }
        }
        return new MeshPlatformComparison(
            paths.Length,equalCounts,pairs,comparable,
            formatEqual,primitiveEqual,vertexEqual);
    }

    private sealed record MeshFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record MeshRepresentationObservation(
        string Kind,uint PayloadSize,string ContentSha256,bool StrictGeometryDecoded,
        SmoPcMeshData? Pc,SmoPs2NativeMeshData? Ps2);
    private sealed record MeshFieldAnnotation(
        int FieldIndex,int FieldType,string Semantic,string Layout,string DecodedJson);
    private sealed record MeshObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string PlatformKey,
        string CanonicalPath,MeshRepresentationObservation? CrossPlatform,
        MeshRepresentationObservation? PlatformSpecific,
        SmoMeshBoundingBoxData? BoundingBox,string VariantKey,string SummaryJson,
        IReadOnlyList<MeshFieldAnnotation> Fields);
    private sealed record MeshPlatformComparison(
        int CommonResources,int EqualCountResources,int PairedMeshes,
        int ComparableMeshes,
        int EqualVertexFormat,int EqualPrimitiveCount,int EqualVertexCount);
}
