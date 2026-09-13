using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private const string PartitionRenderableVariantKey =
        "partition_renderable_debug_color_inline_models";

    private static SmoResearchClassAnalysisResult AnalyzePartitionRenderable(
        string databasePath,
        SmoResearchClassReport report)
    {
        RequireSpatialRuntimeProfile(report);
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item => item.NamedObjectCount != 0) ||
            report.Relations.Any(item => item.Direction switch
            {
                "parent" => item.RelatedTypeHash != SmoClassIds.PartitionNode,
                "child" => item.RelatedTypeHash != SmoClassIds.Model,
                _ => true
            }))
        {
            throw new InvalidDataException(
                "spPartitionRenderable corpus no longer matches the validated " +
                "unnamed partition-node child with owned spModel children contract.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        List<PartitionRenderableObservation> observations =
            LoadPartitionRenderableObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        long relationshipCount = observations.Sum(item =>
            (long)item.Data.Renderables.Count);
        long fieldCount = observations.Sum(item => (long)item.Fields.Count);
        if (objectCount != 7_208 || observations.Count != objectCount ||
            relationshipCount != 19_989 || fieldCount != 27_197)
        {
            throw new InvalidDataException(
                "Expected 7208 spPartitionRenderable objects, 19989 model " +
                $"relationships and 27197 semantic fields; got {observations.Count}, " +
                $"{relationshipCount} and {fieldCount}.");
        }

        RequirePartitionRenderableProfiles(observations);
        (int pcCommon,int pcEqual,int pcPairs) =
            ComparePartitionRenderablePcCopies(observations);
        PartitionRenderablePlatformComparison cross =
            ComparePartitionRenderablePlatforms(observations);
        var expectedCross = new PartitionRenderablePlatformComparison(
            29,29,2_324,2_324,2_323,2_323);
        if ((pcCommon,pcEqual,pcPairs) != (29,29,2_324) || cross != expectedCross)
        {
            throw new InvalidDataException(
                "spPartitionRenderable PC-copy or PC/PS2 pairing changed: " +
                JsonSerializer.Serialize(cross));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spPartitionRenderable","spPartitionRenderableSerializer",
            "eprsfPartitionRenderableRenderable",
            "eprsfPartitionRenderableDebugColor",
            "pPartitionRenderable->GetRenderable( i )",
            "pPartitionRenderable->GetDebugColor().uColor"
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
        using (SqliteCommand clearVariants = CreateCommand(connection,transaction,"""
                   DELETE FROM class_variants
                   WHERE type_hash=$hash AND variant_key=$key;
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.Parameters.AddWithValue("$key",PartitionRenderableVariantKey);
            clearVariants.ExecuteNonQuery();
        }

        UpsertPartitionRenderableFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotatePartitionRenderableFields(connection,transaction,observations);
        int variantId = InsertPartitionRenderableVariant(
            connection,transaction,report.TypeHash,observations,now);
        AssignPartitionRenderableVariant(
            connection,transaction,observations,variantId);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "reader VA 0x0044ECF0..0x0044EF33; writer VA 0x0044EF40..0x0044F2BB",
            "The PC serializer names field 1 DebugColor and repeated field 0 " +
            "Renderable. It writes the color first, then GetRenderable(i); the " +
            "reader stores DebugColor at runtime offset +0x84.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "reader VA 0x001A2030..0x001A2220; writer VA 0x001A22E0..0x001A24D0",
            "The PS2 serializer independently exposes the same DebugColor and " +
            "repeated Renderable fields; DebugColor resides at runtime offset +0x80.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict reload and complete decode of every unique PC and PS2 object",
            $"{objectCount} unnamed objects, {relationshipCount} non-null inline " +
            $"physical-child spModel relationships and {fieldCount} annotated " +
            "semantic fields. Every physical parent is spPartitionNode. Each " +
            "object has one ARGB debug color and 1..67 renderables.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus spPartitionRenderable ordinal",
            $"PC working/pristine: {pcEqual}/{pcCommon} resources and {pcPairs} " +
            "paired objects are byte-identical. PC/PS2 common resources: " +
            $"{cross.CommonResources}; equal counts: {cross.EqualCountResources}; " +
            $"paired objects: {cross.PairedObjects}; matching DebugColor: " +
            $"{cross.EqualDebugColor}; renderable count: {cross.EqualRenderableCount}; " +
            $"ordered model names: {cross.EqualTargetNames}. The sole mismatch is " +
            "Alfea03 ordinal 1, where PS2 appends light_ray-000 and detach ray-000.",
            null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='partition_renderable_group',
                       description='Partition leaf grouping one or more inline spModel renderables under one debug color',
                       decode_status='read_only_decode',
                       notes='All PC/PS2 objects strictly decode. Serialized-size diversity comes from nested models; the sole observed structural variant is one ARGB DebugColor followed by 1..67 inline physical-child spModel relationships.'
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
            report.Profiles.Sum(item => item.UniqueResourceCount),1,
            observations.Count,evidenceRows,
            $"One DebugColor-plus-inline-models variant decoded across {objectCount} " +
            $"objects and {relationshipCount} relationships. PC copies are " +
            $"identical in {pcEqual}/{pcCommon} resources; {cross.PairedObjects} " +
            "PC/PS2 objects were paired with one known renderable-list mismatch.");
    }

    private static List<PartitionRenderableObservation>
        LoadPartitionRenderableObservations(SqliteConnection connection)
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
        command.Parameters.AddWithValue(
            "$hash",(long)SmoClassIds.PartitionRenderable);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<PartitionRenderableFileLocation>();
        while (reader.Read())
        {
            locations.Add(new PartitionRenderableFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<PartitionRenderableObservation>();
        foreach (PartitionRenderableFileLocation location in locations)
        {
            byte[] bytes = ReadPartitionRenderableResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.PartitionRenderable))
            {
                if (!SmoPartitionRenderableDecoder.TryDecode(
                        document,entry,out SmoPartitionRenderableData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                if (entry.ParentIndex is not int parentIndex ||
                    document.Objects[parentIndex].TypeHash != SmoClassIds.PartitionNode)
                {
                    throw new InvalidDataException(
                        "spPartitionRenderable parent is not spPartitionNode.");
                }
                result.Add(CreatePartitionRenderableObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return result;
    }

    private static byte[] ReadPartitionRenderableResource(
        PartitionRenderableFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
        {
            return File.ReadAllBytes(Path.Combine(location.SourceRoot,
                location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        }
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
        {
            throw new InvalidDataException(
                "PCK partition-renderable occurrence is incomplete.");
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

    private static PartitionRenderableObservation
        CreatePartitionRenderableObservation(
            PartitionRenderableFileLocation location,
            SmoDocument document,
            SmoObjectEntry entry,
            int ordinal,
            SmoPartitionRenderableData decoded)
    {
        IReadOnlyList<SmoObjectField> direct =
            SmoObjectFieldReader.Read(document,entry);
        if (direct.Count != decoded.Renderables.Count + 2)
            throw new InvalidDataException("Unexpected partition-renderable shape.");

        var fields = new List<PartitionRenderableFieldAnnotation>(
            decoded.Renderables.Count + 1);
        SmoObjectField colorField = direct[0];
        fields.Add(new PartitionRenderableFieldAnnotation(
            0,colorField.FieldType,"partition_renderable.debug_color","ARGB UInt32",
            JsonSerializer.Serialize(new
            {
                Argb = $"0x{decoded.DebugColorArgb:X8}",
                Alpha = (decoded.DebugColorArgb >> 24) & 0xFF,
                Red = (decoded.DebugColorArgb >> 16) & 0xFF,
                Green = (decoded.DebugColorArgb >> 8) & 0xFF,
                Blue = decoded.DebugColorArgb & 0xFF
            })));
        var targetNames = new List<string>(decoded.Renderables.Count);
        for (int index = 0;index < decoded.Renderables.Count;index++)
        {
            SmoObjectField field = direct[index + 1];
            SmoNodeRelationship relationship = ReadSpatialWireReference(
                document,entry,"partition_renderable.renderable",index);
            if (relationship.ObjectId != decoded.Renderables[index].ObjectId)
                throw new InvalidDataException("Authored and loaded partition renderable identities differ.");
            string targetName = relationship.TargetName?.TrimEnd('\0') ?? string.Empty;
            targetNames.Add(targetName);
            fields.Add(new PartitionRenderableFieldAnnotation(
                index + 1,field.FieldType,"partition_renderable.renderable",
                "serialized object relationship to spModel",
                JsonSerializer.Serialize(new
                {
                    relationship.ObjectId,
                    Encoding = GetPartitionNodeEncodingName(relationship.Encoding),
                    relationship.TargetObjectIndex,
                    TargetTypeHash = $"0x{relationship.TargetTypeHash:X8}",
                    TargetName = targetName
                })));
        }
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        return new PartitionRenderableObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            location.PlatformKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            decoded,serializedHash,targetNames.AsReadOnly(),fields.AsReadOnly());
    }

    private static void RequirePartitionRenderableProfiles(
        IReadOnlyList<PartitionRenderableObservation> observations)
    {
        var expected = new Dictionary<string,PartitionRenderableProfile>
        {
            ["pc-working"] = new(2_324,6_541,1,67),
            ["pc-pristine"] = new(2_324,6_541,1,67),
            ["ps2-pristine"] = new(2_560,6_907,1,67)
        };
        Dictionary<string,PartitionRenderableProfile> actual = observations
            .GroupBy(item => item.CorpusKey)
            .ToDictionary(group => group.Key,group => new PartitionRenderableProfile(
                group.Count(),group.Sum(item => item.Data.Renderables.Count),
                group.Min(item => item.Data.Renderables.Count),
                group.Max(item => item.Data.Renderables.Count)));
        if (actual.Count != expected.Count || expected.Any(item =>
                !actual.TryGetValue(item.Key,out PartitionRenderableProfile? value) ||
                value != item.Value))
        {
            throw new InvalidDataException(
                "spPartitionRenderable cardinality profiles changed: " +
                JsonSerializer.Serialize(actual));
        }

        Dictionary<uint,int> pcColors = ExpectedPartitionRenderablePcColors();
        Dictionary<uint,int> ps2Colors = ExpectedPartitionRenderablePs2Colors();
        foreach (IGrouping<string,PartitionRenderableObservation> group in
                 observations.GroupBy(item => item.CorpusKey))
        {
            Dictionary<uint,int> actualColors = group
                .GroupBy(item => item.Data.DebugColorArgb)
                .ToDictionary(item => item.Key,item => item.Count());
            Dictionary<uint,int> expectedColors = group.Key == "ps2-pristine"
                ? ps2Colors : pcColors;
            if (actualColors.Count != expectedColors.Count || expectedColors.Any(
                    item => !actualColors.TryGetValue(item.Key,out int count) ||
                            count != item.Value))
            {
                throw new InvalidDataException(
                    $"spPartitionRenderable DebugColor profile changed for {group.Key}.");
            }
        }
    }

    private static Dictionary<uint,int> ExpectedPartitionRenderablePcColors() =>
        new()
        {
            [0x8F48488F] = 224,[0x9F50509F] = 245,[0xAF5858AF] = 289,
            [0xBF6060BF] = 319,[0xCF6868CF] = 227,[0xDF7070DF] = 257,
            [0xEF7878EF] = 307,[0xFF8080FF] = 357,[0xFF80C0FF] = 8,
            [0xFF80FF80] = 18,[0xFF80FFC0] = 4,[0xFF80FFFF] = 16,
            [0xFFC080FF] = 1,[0xFFC0FF80] = 1,[0xFFFF8080] = 20,
            [0xFFFF80C0] = 1,[0xFFFF80FF] = 18,[0xFFFFC080] = 1,
            [0xFFFFFF80] = 11
        };

    private static Dictionary<uint,int> ExpectedPartitionRenderablePs2Colors() =>
        new()
        {
            [0x8F48488F] = 255,[0x9F50509F] = 278,[0xAF5858AF] = 313,
            [0xBF6060BF] = 344,[0xCF6868CF] = 258,[0xDF7070DF] = 295,
            [0xEF7878EF] = 332,[0xFF8080FF] = 386,[0xFF80C0FF] = 8,
            [0xFF80FF80] = 18,[0xFF80FFC0] = 4,[0xFF80FFFF] = 16,
            [0xFFC080FF] = 1,[0xFFC0FF80] = 1,[0xFFFF8080] = 20,
            [0xFFFF80C0] = 1,[0xFFFF80FF] = 18,[0xFFFFC080] = 1,
            [0xFFFFFF80] = 11
        };

    private static (int Common,int Equal,int Pairs)
        ComparePartitionRenderablePcCopies(
            IEnumerable<PartitionRenderableObservation> observations)
    {
        Dictionary<string,PartitionRenderableObservation[]> Build(string corpus) =>
            observations.Where(item => item.CorpusKey == corpus)
                .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
                .ToDictionary(group => group.Key,
                    group => group.OrderBy(item => item.Ordinal).ToArray(),
                    StringComparer.Ordinal);
        Dictionary<string,PartitionRenderableObservation[]> working =
            Build("pc-working");
        Dictionary<string,PartitionRenderableObservation[]> pristine =
            Build("pc-pristine");
        string[] paths = working.Keys.Intersect(
            pristine.Keys,StringComparer.Ordinal).ToArray();
        int equal = paths.Count(path =>
            working[path].Select(item => item.SerializedSha256)
                .SequenceEqual(pristine[path].Select(item => item.SerializedSha256)));
        int pairs = paths.Sum(path => Math.Min(
            working[path].Length,pristine[path].Length));
        return (paths.Length,equal,pairs);
    }

    private static PartitionRenderablePlatformComparison
        ComparePartitionRenderablePlatforms(
            IEnumerable<PartitionRenderableObservation> observations)
    {
        Dictionary<string,PartitionRenderableObservation[]> Build(string corpus) =>
            observations.Where(item => item.CorpusKey == corpus)
                .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
                .ToDictionary(group => group.Key,
                    group => group.OrderBy(item => item.Ordinal).ToArray(),
                    StringComparer.Ordinal);
        Dictionary<string,PartitionRenderableObservation[]> pc =
            Build("pc-pristine");
        Dictionary<string,PartitionRenderableObservation[]> ps2 =
            Build("ps2-pristine");
        string[] common = pc.Keys.Intersect(ps2.Keys,StringComparer.Ordinal).ToArray();
        int equalCounts = common.Count(path => pc[path].Length == ps2[path].Length);
        int pairs = 0,colors = 0,counts = 0,names = 0;
        foreach (string path in common)
        for (int index = 0;index < Math.Min(pc[path].Length,ps2[path].Length);index++)
        {
            PartitionRenderableObservation left = pc[path][index];
            PartitionRenderableObservation right = ps2[path][index];
            pairs++;
            colors += left.Data.DebugColorArgb == right.Data.DebugColorArgb ? 1 : 0;
            counts += left.Data.Renderables.Count == right.Data.Renderables.Count
                ? 1 : 0;
            names += left.TargetNames.SequenceEqual(right.TargetNames) ? 1 : 0;
        }
        return new PartitionRenderablePlatformComparison(
            common.Length,equalCounts,pairs,colors,counts,names);
    }

    private static void UpsertPartitionRenderableFieldDefinitions(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<PartitionRenderableObservation> observations)
    {
        int relationshipCount = observations.Sum(item => item.Data.Renderables.Count);
        (int Type,int Occurrence,string Semantic,string Display,string Kind,
            string Layout,string Constraints,string Notes)[] definitions =
        [
            (0,-1,"partition_renderable.renderable","Renderable",
                "object_relationship","inline object relationship to spModel",
                JsonSerializer.Serialize(new
                {
                    observedDirectOccurrences = relationshipCount,
                    repeated = true,minimumPerObject = 1,maximumPerObject = 67,
                    encoding = "inline_object",targetClass = "spModel",
                    ownership = "physical_child",mutationStatus = "not_enabled"
                }),
                "Repeated serializer field eprsfPartitionRenderableRenderable; " +
                "all observed targets are non-null owned spModel objects."),
            (1,0,"partition_renderable.debug_color","Debug color","argb32",
                "ARGB UInt32 stored little-endian (BGRA bytes)",
                JsonSerializer.Serialize(new
                {
                    observedDirectOccurrences = observations.Count,
                    distinctValues = observations.Select(item =>
                        item.Data.DebugColorArgb).Distinct().Count(),
                    byteOrder = "little_endian_bgra",mutationStatus = "not_enabled"
                }),
                "Required serializer field eprsfPartitionRenderableDebugColor; " +
                "19 exact ARGB values are observed on both platforms.")
        ];
        foreach (var definition in definitions)
        {
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',0,$type,$occurrence,$semantic,
                       $display,$kind,$layout,'read_only_research',
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
            command.Parameters.AddWithValue("$occurrence",definition.Occurrence);
            command.Parameters.AddWithValue("$semantic",definition.Semantic);
            command.Parameters.AddWithValue("$display",definition.Display);
            command.Parameters.AddWithValue("$kind",definition.Kind);
            command.Parameters.AddWithValue("$layout",definition.Layout);
            command.Parameters.AddWithValue("$constraints",definition.Constraints);
            command.Parameters.AddWithValue("$notes",definition.Notes);
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotatePartitionRenderableFields(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<PartitionRenderableObservation> observations)
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
        foreach (PartitionRenderableObservation observation in observations)
        foreach (PartitionRenderableFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
            {
                throw new InvalidDataException(
                    "Could not annotate spPartitionRenderable field.");
            }
        }
    }

    private static int InsertPartitionRenderableVariant(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IEnumerable<PartitionRenderableObservation> observations,
        string now)
    {
        Dictionary<string,int> corpusCounts = observations
            .GroupBy(item => item.CorpusKey)
            .ToDictionary(group => group.Key,group => group.Count());
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,status,
                discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2',$key,
                   'Debug color with inline models','confirmed',
                   $discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$key",PartitionRenderableVariantKey);
        command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
        {
            corpusCounts,
            fieldOrder = new[] { "debug_color", "renderable[1..67]" },
            relationshipEncoding = "inline_object",
            targetClass = "spModel"
        }));
        command.Parameters.AddWithValue("$notes",
            "The sole observed serializer shape. Object-size diversity comes from " +
            "the nested spModel subtrees, not partition-renderable subtypes.");
        command.Parameters.AddWithValue("$utc",now);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void AssignPartitionRenderableVariant(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<PartitionRenderableObservation> observations,
        int variantId)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spPartitionRenderable decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.AddWithValue("$variant",variantId);
        foreach (PartitionRenderableObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.ExecuteNonQuery();
        }
    }

    private sealed record PartitionRenderableFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record PartitionRenderableFieldAnnotation(
        int FieldIndex,int FieldType,string Semantic,string Layout,
        string DecodedJson);
    private sealed record PartitionRenderableObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string PlatformKey,
        string CanonicalPath,SmoPartitionRenderableData Data,string SerializedSha256,
        IReadOnlyList<string> TargetNames,
        IReadOnlyList<PartitionRenderableFieldAnnotation> Fields);
    private sealed record PartitionRenderableProfile(
        int Objects,int Renderables,int MinimumPerObject,int MaximumPerObject);
    private sealed record PartitionRenderablePlatformComparison(
        int CommonResources,int EqualCountResources,int PairedObjects,
        int EqualDebugColor,int EqualRenderableCount,int EqualTargetNames);
}
