using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeTextureData(
        string databasePath,
        SmoResearchClassReport report)
    {
        // The historical profile conflates stored PC/PS2 summaries with source
        // selection. The shared PC observer now preserves skipped/repeated
        // fields explicitly; it does not establish PS2 source equivalence.
        // Stop before opening the database for writing or replacing evidence.
        if (report.Profiles.Select(item => item.PlatformKey).Distinct().Count() > 1)
            throw new NotSupportedException(
                "The historical combined TextureData analysis requires separate PC source and PS2 metadata profiles. " +
                "Existing evidence is preserved; use the focused source checks until those profiles are defined.");
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item =>
                item.NamedObjectCount != item.UniqueObjectCount ||
                item.MinimumSerializedSize < 12) ||
            report.Fields.Any(item =>
                item.SectionIndex != 0 || item.SectionFromEnd != 0 ||
                item.FieldType is not (0 or 3 or 6) ||
                (item.FieldType == 0 && item.PayloadSize == 0) ||
                (item.FieldType == 6 && item.PayloadSize != sizeof(uint))))
        {
            throw new InvalidDataException(
                "spTextureData corpus no longer matches the validated direct " +
                "source/platform field contract.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: false);
        List<TextureObservation> observations = LoadTextureObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected {objectCount} decoded spTextureData objects, got " +
                $"{observations.Count}.");
        }

        Dictionary<(string Corpus,string Variant),int> variantCounts = observations
            .GroupBy(item => (item.CorpusKey,item.VariantKey))
            .ToDictionary(group => group.Key,group => group.Count());
        RequireTextureVariantCounts(variantCounts);

        Dictionary<string,string> pcWorking = BuildTexturePathMap(
            observations,"pc-working");
        Dictionary<string,string> pcPristine = BuildTexturePathMap(
            observations,"pc-pristine");
        Dictionary<string,string> ps2Pristine = BuildTexturePathMap(
            observations,"ps2-pristine");
        (int pcCommon,int pcEqual) = ComparePayloadPathMaps(pcWorking,pcPristine);
        (int platformCommon,int platformEqual) = ComparePayloadPathMaps(
            pcPristine,ps2Pristine);
        if (pcCommon != pcWorking.Count || pcCommon != pcPristine.Count ||
            pcEqual != pcCommon)
        {
            throw new InvalidDataException(
                "Working and pristine PC texture structures/content differ.");
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] commonTokens =
        [
            "spTextureData","spTextureDataSerializer",
            "spPS2TextureDataSerializer","esfTextureDataSourceEmbeded",
            "esfTextureDataCrossPlatform","esfTextureDataPlatformSpecific",
            "esfTextureDataPlatformType"
        ];
        RequireAsciiTokens(pcExecutable.Path,commonTokens);
        RequireAsciiTokens(ps2Executable.Path,commonTokens);
        RequireAsciiTokens(pcExecutable.Path,"spDXTextureDataSerializer");

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
                   WHERE type_hash=$hash AND variant_key LIKE 'texture_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        UpsertTextureFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateTextureFields(connection,transaction,observations);
        Dictionary<string,int> variantIds = InsertTextureVariants(
            connection,transaction,report.TypeHash,observations,now);
        AssignTextureVariants(
            connection,transaction,observations,variantIds);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "spTextureData registration and serializers near RVA 0x002D1D30; " +
            "DX factory 0x0042B660, PS2 factory 0x0042C9E0, base factory 0x0042DC30",
            "Recovered source fields 2/3/4, representation fields 0/1 and " +
            "platform field 6. Platform values 6/7 select Direct3D and 8/9 " +
            "select PS2, with the latter value retaining cross-platform data.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "MIPS spPS2TextureDataSerializer and shared spTextureDataSerializer strings",
            "The PS2 executable independently exposes the same source, cross/native " +
            "and platform-type serializer field names.",ps2Executable.Sha256,now);

        string counts = string.Join(", ",variantCounts
            .OrderBy(item => item.Key.Corpus,StringComparer.Ordinal)
            .ThenBy(item => item.Key.Variant,StringComparer.Ordinal)
            .Select(item => $"{item.Key.Corpus}/{item.Key.Variant}={item.Value}"));
        Dictionary<string,int> representationCounts = observations
            .SelectMany(item => new[]
            {
                item.CrossPlatform?.Kind,
                item.PlatformSpecific?.Kind
            })
            .Where(item => item is not null)
            .GroupBy(item => item!)
            .ToDictionary(group => group.Key!,group => group.Count());
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "all spTextureData objects in PC working, PC pristine and PS2 pristine",
            $"{objectCount} named objects fully bounded and structurally decoded. " +
            $"Variants: {counts}. Representations: " +
            string.Join(", ",representationCounts.OrderBy(item => item.Key)
                .Select(item => $"{item.Key}={item.Value}")) + ".",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus texture name, metadata and content hashes",
            $"PC working/pristine resources are identical: {pcEqual}/{pcCommon}. " +
            $"PC pristine/PS2 common resources: {platformCommon}; byte/representation-" +
            $"identical resources: {platformEqual} (platform-native conversion is expected).",
            null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='rendering_resource',
                       description='Embedded cross-platform, Direct3D or PS2 texture data',
                       decode_status='read_only_decode',
                       notes='All PC/PS2 source, platform, palette and mip containers are structurally decoded; native PS2 swizzle rendering remains read-only research.'
                   WHERE type_hash=$hash;
                   """))
        {
            updateClass.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            updateClass.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);

        int assignmentCount = observations.Count;
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,objectCount,
            report.Profiles.Sum(item => item.UniqueResourceCount),
            variantIds.Count,assignmentCount,evidenceRows,
            $"Five storage variants and {representationCounts.Count} representation " +
            $"kinds decoded across {objectCount} objects. PC pairs " +
            $"{pcEqual}/{pcCommon}; PC/PS2 common resources {platformCommon}. " +
            "Compact data-block header bytes are recorded only as serializer " +
            "length encodings, not pixel-format codes. Mutation safety and PS2 " +
            "swizzle-to-BGRA rendering remain untested.");
    }

    private static List<TextureObservation> LoadTextureObservations(
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.TextureData);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<TextureFileLocation>();
        while (reader.Read())
        {
            locations.Add(new TextureFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),
                reader.GetInt64(8)));
        }

        var observations = new List<TextureObservation>();
        foreach (TextureFileLocation location in locations)
        {
            byte[] bytes = ReadTextureResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.TextureData))
            {
                if (!SmoTextureDataDecoder.TryDecode(
                        document,entry,out SmoTextureDataInfo? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}] {entry.Name}: {error}");
                }
                observations.Add(CreateTextureObservation(
                    location,document,entry,decoded));
            }
        }
        return observations;
    }

    private static byte[] ReadTextureResource(TextureFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
            return File.ReadAllBytes(Path.Combine(
                location.SourceRoot,location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
            throw new InvalidDataException("PCK texture occurrence is incomplete.");
        string archive = Path.Combine(
            location.SourceRoot,
            location.ContainerPath.Replace('/',Path.DirectorySeparatorChar));
        using var stream = new FileStream(
            archive,FileMode.Open,FileAccess.Read,FileShare.Read);
        stream.Position = location.ByteOffset.Value;
        byte[] data = new byte[checked((int)location.ByteSize)];
        stream.ReadExactly(data);
        return data;
    }

    private static TextureObservation CreateTextureObservation(
        TextureFileLocation location,
        SmoDocument document,
        SmoObjectEntry entry,
        SmoTextureDataInfo decoded)
    {
        TextureRepresentationObservation? cross = ToTextureRepresentation(
            decoded.CrossPlatform);
        TextureRepresentationObservation? specific = ToTextureRepresentation(
            decoded.PlatformSpecific);
        string variantKey = decoded.SourceKind == SmoTextureSourceKind.LegacyCrossPlatform
            ? "texture_legacy_cross"
            : specific?.Kind == nameof(SmoTextureRepresentationKind.Direct3DBgra32)
                ? "texture_direct3d_embedded"
                : specific is not null && cross is not null
                    ? "texture_ps2_native_with_cross"
                    : specific is not null
                        ? "texture_ps2_native"
                        : "texture_embedded_cross_only";
        object summary = new
        {
            sourceKind = decoded.SourceKind.ToString(),
            platformType = decoded.PlatformType,
            crossPlatform = cross,
            platformSpecific = specific
        };
        string summaryJson = JsonSerializer.Serialize(summary);
        IReadOnlyList<SmoObjectField> fields = SmoObjectFieldReader.Read(document,entry);
        var annotations = new List<TextureFieldAnnotation>();
        for (int fieldIndex = 0; fieldIndex < fields.Count; fieldIndex++)
        {
            SmoObjectField field = fields[fieldIndex];
            if (field.FieldType == 0 && field.PayloadSize == 0)
                continue;
            (string Semantic,string Layout,string Json) annotation = field.FieldType switch
            {
                0 => ("texture.cross_platform",
                    "nested field 5: width/height/auxiliary/bytes-per-pixel/BGRA32",
                    JsonSerializer.Serialize(cross)),
                3 => ("texture.source_embedded",
                    "source base section plus fields 6/0/1 and native mip containers",
                    summaryJson),
                6 => ("texture.platform_type","UInt32 enum",
                    JsonSerializer.Serialize(decoded.PlatformType)),
                _ => throw new InvalidDataException(
                    $"Unexpected direct texture field {field.FieldType}.")
            };
            annotations.Add(new TextureFieldAnnotation(
                fieldIndex,field.FieldType,annotation.Semantic,
                annotation.Layout,annotation.Json));
        }
        return new TextureObservation(
            location.FileId,entry.Index,location.CorpusKey,location.PlatformKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            entry.Name.TrimEnd('\0'),decoded.SourceKind,decoded.PlatformType,
            cross,specific,variantKey,summaryJson,annotations.AsReadOnly());
    }

    private static TextureRepresentationObservation? ToTextureRepresentation(
        SmoTextureRepresentationData? value)
    {
        if (value is null)
            return null;
        using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        hash.AppendData(value.Palette.Span);
        foreach (SmoTextureMipLevelData mip in value.MipLevels)
            hash.AppendData(mip.PixelData.Span);
        return new TextureRepresentationObservation(
            value.Kind.ToString(),value.Width,value.Height,value.FormatValue,
            value.AuxiliaryValue,value.BitsPerPixel,value.Palette.Length,
            value.MipLevels.Select(mip => new TextureMipObservation(
                mip.Index,mip.Width,mip.Height,mip.Descriptor0,mip.Descriptor1,
                mip.Descriptor2,mip.PixelData.Length)).ToArray(),
            Convert.ToHexString(hash.GetHashAndReset()));
    }

    private static void RequireTextureVariantCounts(
        IReadOnlyDictionary<(string Corpus,string Variant),int> counts)
    {
        Dictionary<(string Corpus,string Variant),int> expected = new()
        {
            [("pc-working","texture_legacy_cross")] = 96,
            [("pc-working","texture_direct3d_embedded")] = 2433,
            [("pc-working","texture_ps2_native")] = 6,
            [("pc-working","texture_ps2_native_with_cross")] = 28,
            [("pc-working","texture_embedded_cross_only")] = 1,
            [("pc-pristine","texture_legacy_cross")] = 96,
            [("pc-pristine","texture_direct3d_embedded")] = 2433,
            [("pc-pristine","texture_ps2_native")] = 6,
            [("pc-pristine","texture_ps2_native_with_cross")] = 28,
            [("pc-pristine","texture_embedded_cross_only")] = 1,
            [("ps2-pristine","texture_ps2_native")] = 2353,
            [("ps2-pristine","texture_ps2_native_with_cross")] = 4
        };
        if (counts.Count != expected.Count || expected.Any(item =>
                !counts.TryGetValue(item.Key,out int count) || count != item.Value))
        {
            throw new InvalidDataException(
                "spTextureData storage-variant counts changed from the validated corpus.");
        }
    }

    private static void UpsertTextureFieldDefinitions(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<TextureObservation> observations)
    {
        long Count(int type) => observations.Sum(item =>
            (long)item.Fields.Count(field => field.FieldType == type));
        (int Type,string Semantic,string Display,string Kind,string Layout,
            string Evidence,string Notes)[] definitions =
        [
            (0,"texture.cross_platform","Cross-platform texture","texture_container",
                "field 5: UInt32 width/height/auxiliary/bytes-per-pixel + BGRA32",
                "confirmed_pc_ps2_full_corpus",
                $"Observed {Count(0)} times directly; also nested in embedded sources."),
            (2,"texture.source_none","No texture source","boolean",
                "Boolean source-state payload",
                "confirmed_both_executables_unobserved_on_concrete_class",
                "Source selector field; absent as a direct field in all corpora."),
            (3,"texture.source_embedded","Embedded texture source","texture_container",
                "base source section + platform type/cross/native representation fields",
                "confirmed_both_executables_and_full_corpus",
                $"Observed {Count(3)} times; complete nested layout decoded."),
            (4,"texture.source_reference","Referenced texture source","resource_reference",
                "serializer reference payload",
                "confirmed_both_executables_unobserved_on_concrete_class",
                "Field name recovered; payload form is absent from all corpora."),
            (6,"texture.platform_type","Texture platform type","uint32_enum",
                "UInt32 enum",
                "confirmed_both_executables_and_full_corpus",
                $"Observed {Count(6)} times directly and additionally inside embedded data; values 1/6/8/9 occur.")
        ];
        foreach (var definition in definitions)
        {
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',0,$type,-1,$semantic,$display,$kind,
                       $layout,'read_only_research',$evidence,$constraints,$notes)
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
            command.Parameters.AddWithValue("$evidence",definition.Evidence);
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

    private static void AnnotateTextureFields(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<TextureObservation> observations)
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
        foreach (TextureObservation observation in observations)
        foreach (TextureFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException("Could not annotate texture field.");
        }
    }

    private static Dictionary<string,int> InsertTextureVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<TextureObservation> observations,
        string now)
    {
        (string Key,string Display,string Notes)[] variants =
        [
            ("texture_legacy_cross","Legacy direct cross-platform BGRA",
                "Direct field 0, optionally preceded by top-level platform value 1."),
            ("texture_direct3d_embedded","Embedded Direct3D BGRA",
                "Source field 3 with platform-specific BGRA mip fields 0/1; platform value 6 is normally present."),
            ("texture_ps2_native","Embedded native PS2 texture",
                "PS2 formats 0/1/3 with 16/256/no palette and bounded mip records."),
            ("texture_ps2_native_with_cross","PS2 native plus cross-platform copy",
                "Both field 0 BGRA and field 1 PS2 native representations are retained."),
            ("texture_embedded_cross_only","Embedded cross-platform only",
                "Field 0 representation with platform value 1 and no native copy.")
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
                sourceAndRepresentationStructure = key
            }));
            command.Parameters.AddWithValue("$notes",notes);
            command.Parameters.AddWithValue("$utc",now);
            result.Add(key,Convert.ToInt32(command.ExecuteScalar()));
        }
        return result;
    }

    private static void AssignTextureVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<TextureObservation> observations,
        IReadOnlyDictionary<string,int> variantIds)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict bounded spTextureData decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.Add("$variant",SqliteType.Integer);
        foreach (TextureObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$variant"].Value = variantIds[observation.VariantKey];
            command.ExecuteNonQuery();
        }
    }

    private static Dictionary<string,string> BuildTexturePathMap(
        IEnumerable<TextureObservation> observations,
        string corpusKey) => observations
        .Where(item => item.CorpusKey.Equals(corpusKey,StringComparison.Ordinal))
        .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
        .ToDictionary(
            group => group.Key,
            group => string.Join("|",group
                .Select(item => item.Name.ToLowerInvariant() + ":" + item.SummaryJson)
                .Order(StringComparer.Ordinal)),
            StringComparer.Ordinal);

    private sealed record TextureFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record TextureMipObservation(
        int Index,int Width,int Height,uint Descriptor0,uint Descriptor1,
        uint Descriptor2,int DataBytes);
    private sealed record TextureRepresentationObservation(
        string Kind,int Width,int Height,uint FormatValue,uint AuxiliaryValue,
        uint BitsPerPixel,int PaletteBytes,
        IReadOnlyList<TextureMipObservation> Mips,string ContentSha256);
    private sealed record TextureFieldAnnotation(
        int FieldIndex,int FieldType,string Semantic,string Layout,string DecodedJson);
    private sealed record TextureObservation(
        int FileId,int ObjectIndex,string CorpusKey,string PlatformKey,
        string CanonicalPath,string Name,SmoTextureSourceKind SourceKind,
        uint? PlatformType,TextureRepresentationObservation? CrossPlatform,
        TextureRepresentationObservation? PlatformSpecific,string VariantKey,
        string SummaryJson,IReadOnlyList<TextureFieldAnnotation> Fields);
}
