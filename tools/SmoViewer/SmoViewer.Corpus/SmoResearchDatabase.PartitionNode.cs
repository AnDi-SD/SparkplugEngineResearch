using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private const string PartitionNodeVariantKey =
        "partition_node_common_relationship_layout";

    private static SmoResearchClassAnalysisResult AnalyzePartitionNode(
        string databasePath,
        SmoResearchClassReport report)
    {
        RequireSpatialRuntimeProfile(report);
        HashSet<uint> parents = report.Relations
            .Where(item => item.Direction == "parent")
            .Where(item => item.RelatedTypeHash.HasValue)
            .Select(item => item.RelatedTypeHash!.Value).ToHashSet();
        HashSet<uint> children = report.Relations
            .Where(item => item.Direction == "child")
            .Where(item => item.RelatedTypeHash.HasValue)
            .Select(item => item.RelatedTypeHash!.Value).ToHashSet();
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item => item.NamedObjectCount != 0) ||
            !parents.SetEquals([SmoClassIds.OctreeNode,SmoClassIds.Zone]) ||
            !children.SetEquals([
                SmoClassIds.CollisionInfo,SmoClassIds.ZonePortal,
                SmoClassIds.StaticRenderObject,SmoClassIds.PartitionRenderable]))
        {
            throw new InvalidDataException(
                "spPartitionNode corpus no longer matches its validated parent/" +
                "owned-child contract.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        List<PartitionNodeObservation> observations =
            LoadPartitionNodeObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        long relationshipCount = observations.Sum(item =>
            3L + item.Data.Children.Count + item.Data.CollisionInfos.Count +
            item.Data.ZonePortals.Count + item.Data.StaticRenderObjects.Count);
        long fieldCount = observations.Sum(item => (long)item.Fields.Count);
        long inlineChildren = observations.Sum(item => item.Relationships.Count(
            relationship => relationship.Encoding ==
                SmoNodeRelationshipEncoding.InlineObject));
        if (objectCount != 16_204 || observations.Count != objectCount ||
            relationshipCount != 232_035 || fieldCount != 248_239 ||
            inlineChildren != 74_324 ||
            observations.Sum(item => item.Data.Children.Count) != 0)
        {
            throw new InvalidDataException(
                "Expected 16204 partition nodes, 232035 relationships, 248239 " +
                $"semantic fields and 74324 inline children; got " +
                $"{observations.Count}, {relationshipCount}, {fieldCount} and " +
                $"{inlineChildren}.");
        }

        RequirePartitionNodeProfiles(observations);
        PartitionNodeComparison pc = ComparePartitionNodes(
            observations,"pc-working","pc-pristine",compareBytes: true);
        PartitionNodeComparison cross = ComparePartitionNodes(
            observations,"pc-pristine","ps2-pristine",compareBytes: false);
        if (pc != new PartitionNodeComparison(29,5_254,5_254,5_254) ||
            cross != new PartitionNodeComparison(29,5_254,5_251,5_254))
        {
            throw new InvalidDataException(
                "spPartitionNode PC-copy or PC/PS2 pairing changed: " +
                JsonSerializer.Serialize(new { pc,cross }));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spPartitionNode","spPartitionNodeSerializer",
            "epnsfPartitionNodeDebugColor",
            "epnsfPartitionNodePartitionSystem","epnsfPartitionNodeZone",
            "epnsfPartitionNodeChild","epnsfPartitionNodeCollisionInfo",
            "epnsfPartitionNodeZonePortal",
            "epnsfPartitionNodeStaticRenderObject",
            "epnsfPartitionNodePartitionRenderable"
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
            clearVariants.Parameters.AddWithValue("$key",PartitionNodeVariantKey);
            clearVariants.ExecuteNonQuery();
        }

        UpsertPartitionNodeFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotatePartitionNodeFields(connection,transaction,observations);
        int variantId = InsertPartitionNodeVariant(
            connection,transaction,report.TypeHash,observations,now);
        AssignPartitionNodeVariant(connection,transaction,observations,variantId);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "reader/writer around VA 0x0044B700..0x0044C6D5",
            "The PC serializer names fields 0..7 and implements ordered " +
            "DebugColor, PartitionSystem, Zone, Child, CollisionInfo, " +
            "ZonePortal, StaticRenderObject and PartitionRenderable output.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "writer VA 0x001A16D0..0x001A1DEC",
            "The independent PS2 writer confirms the same field numbers/order. " +
            "Runtime offsets expose DebugColor +0x40, Zone +0x50, " +
            "PartitionSystem +0x60 and PartitionRenderable +0x64.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload and complete decode of every PC and PS2 object",
            $"{objectCount} unnamed objects, {relationshipCount} relationships, " +
            $"{inlineChildren} inline physical children and {fieldCount} annotated " +
            "semantic fields. Child field 2 has zero occurrences in concrete " +
            "spPartitionNode objects (derived spOctreeNode sections use it); all " +
            "other fields resolve with the expected target classes and encodings.",
            null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus spPartitionNode ordinal",
            $"PC working/pristine: {pc.EqualSemantic}/{pc.PairedObjects} semantic " +
            "and byte-identical pairs. PC/PS2: " +
            $"{cross.EqualSemantic}/{cross.PairedObjects} complete semantic matches " +
            $"and {cross.EqualColor} matching DebugColor values. The three " +
            "relationship-list differences are Alfea broken 01 ordinal 2, " +
            "Cloud01 02 ordinal 2 and BMS 03 ordinal 151.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='spatial_partition_node',
                       description='Partition-tree node linking its system and zone to collision, portal, static-render and partition-renderable objects',
                       decode_status='read_only_decode',
                       notes='All concrete PC/PS2 objects strictly decode with one common serializer layout. Field 2 Child is absent from concrete spPartitionNode objects but occurs 18048 times in inherited spOctreeNode sections as UInt32 slot plus inline relationship.'
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
            $"One common partition-node layout decoded across {objectCount} " +
            $"objects and {relationshipCount} relationships; PC copies are " +
            $"identical and {cross.EqualSemantic}/{cross.PairedObjects} shared " +
            "PC/PS2 nodes are semantically equal.");
    }

    private static List<PartitionNodeObservation>
        LoadPartitionNodeObservations(SqliteConnection connection)
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.PartitionNode);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<PartitionNodeFileLocation>();
        while (reader.Read())
        {
            locations.Add(new PartitionNodeFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<PartitionNodeObservation>();
        foreach (PartitionNodeFileLocation location in locations)
        {
            byte[] bytes = ReadPartitionNodeResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.PartitionNode))
            {
                if (!SmoPartitionNodeDecoder.TryDecode(
                        document,entry,out SmoPartitionNodeData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                if (entry.ParentIndex is not int parentIndex ||
                    document.Objects[parentIndex].TypeHash is not
                        SmoClassIds.OctreeNode and not SmoClassIds.Zone)
                {
                    throw new InvalidDataException(
                        "spPartitionNode has an unexpected physical parent.");
                }
                result.Add(CreatePartitionNodeObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return result;
    }

    private static byte[] ReadPartitionNodeResource(PartitionNodeFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
        {
            return File.ReadAllBytes(Path.Combine(location.SourceRoot,
                location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        }
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
            throw new InvalidDataException("PCK partition-node occurrence is incomplete.");
        string archive = Path.Combine(location.SourceRoot,
            location.ContainerPath.Replace('/',Path.DirectorySeparatorChar));
        using var stream = new FileStream(
            archive,FileMode.Open,FileAccess.Read,FileShare.Read);
        stream.Position = location.ByteOffset.Value;
        byte[] data = new byte[checked((int)location.ByteSize)];
        stream.ReadExactly(data);
        return data;
    }

    private static PartitionNodeObservation CreatePartitionNodeObservation(
        PartitionNodeFileLocation location,
        SmoDocument document,
        SmoObjectEntry entry,
        int ordinal,
        SmoPartitionNodeData decoded)
    {
        IReadOnlyList<SmoObjectField> direct =
            SmoObjectFieldReader.Read(document,entry);
        var fields = new List<PartitionNodeFieldAnnotation>(direct.Count - 1);
        fields.Add(new PartitionNodeFieldAnnotation(
            0,"partition_node.debug_color","ARGB UInt32",
            JsonSerializer.Serialize(new
            {
                Argb = $"0x{decoded.DebugColorArgb:X8}",
                Alpha = (decoded.DebugColorArgb >> 24) & 0xFF,
                Red = (decoded.DebugColorArgb >> 16) & 0xFF,
                Green = (decoded.DebugColorArgb >> 8) & 0xFF,
                Blue = decoded.DebugColorArgb & 0xFF
            })));
        Dictionary<uint,SmoObjectEntry> objectsById = document.Objects
            .GroupBy(candidate => candidate.Id)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key,group => group.Single());
        var relationships = new List<SmoNodeRelationship>(direct.Count - 2);
        var signature = new List<string>(direct.Count - 2);
        for (int index = 1;index < direct.Count - 1;index++)
        {
            SmoObjectField field = direct[index];
            if (!SmoNodeDecoder.TryDecodeRelationship(
                    field.Payload.Span,objectsById,
                    out SmoNodeRelationship? relationship) || relationship is null)
                throw new InvalidDataException("Invalid decoded partition-node relation.");
            relationships.Add(relationship);
            string encoding = GetPartitionNodeEncodingName(relationship.Encoding);
            string targetName = relationship.TargetName?.TrimEnd('\0') ?? string.Empty;
            signature.Add($"{field.FieldType}:{encoding}:{targetName}");
            (string semantic,string layout) = field.FieldType switch
            {
                0 => ("partition_node.collision_info",
                    "object relationship to spCollisionInfo"),
                2 => ("partition_node.child",
                    "object relationship to spPartitionNode"),
                3 => ("partition_node.zone","sized-reference relationship to spZone"),
                4 => ("partition_node.zone_portal",
                    "inline object relationship to spZonePortal"),
                5 => ("partition_node.partition_system",
                    "sized-reference relationship to spPartitionSystem"),
                6 => ("partition_node.partition_renderable",
                    "null ID or inline relationship to spPartitionRenderable"),
                7 => ("partition_node.static_render_object",
                    "object relationship to spStaticRenderObject"),
                _ => throw new InvalidDataException("Unexpected partition-node field.")
            };
            fields.Add(new PartitionNodeFieldAnnotation(
                index,semantic,layout,JsonSerializer.Serialize(new
                {
                    relationship.ObjectId,Encoding = encoding,
                    relationship.InlineSerializedSize,
                    relationship.TargetObjectIndex,
                    TargetTypeHash = relationship.TargetTypeHash.HasValue
                        ? $"0x{relationship.TargetTypeHash.Value:X8}" : null,
                    TargetName = targetName
                })));
        }
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        string semanticSignature = JsonSerializer.Serialize(new
        {
            decoded.DebugColorArgb,Relationships = signature
        });
        return new PartitionNodeObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            decoded,serializedHash,semanticSignature,relationships.AsReadOnly(),
            fields.AsReadOnly());
    }

    private static string GetPartitionNodeEncodingName(
        SmoNodeRelationshipEncoding encoding) => encoding switch
        {
            SmoNodeRelationshipEncoding.IdOnly => "id_only",
            SmoNodeRelationshipEncoding.SizedReference => "sized_reference",
            SmoNodeRelationshipEncoding.InlineObject => "inline_object",
            _ => encoding.ToString()
        };

    private static void RequirePartitionNodeProfiles(
        IReadOnlyList<PartitionNodeObservation> observations)
    {
        var expected = new Dictionary<string,PartitionNodeProfile>
        {
            ["pc-working"] = new(5_254,8_140,0,206,52_171,2_324,2_930),
            ["pc-pristine"] = new(5_254,8_140,0,206,52_171,2_324,2_930),
            ["ps2-pristine"] = new(5_696,8_140,0,206,54_043,2_560,3_136)
        };
        Dictionary<string,PartitionNodeProfile> actual = observations
            .GroupBy(item => item.CorpusKey)
            .ToDictionary(group => group.Key,group => new PartitionNodeProfile(
                group.Count(),group.Sum(item => item.Data.CollisionInfos.Count),
                group.Sum(item => item.Data.Children.Count),
                group.Sum(item => item.Data.ZonePortals.Count),
                group.Sum(item => item.Data.StaticRenderObjects.Count),
                group.Count(item => item.Data.PartitionRenderable.ObjectId != 0),
                group.Count(item => item.Data.PartitionRenderable.ObjectId == 0)));
        if (actual.Count != expected.Count || expected.Any(item =>
                !actual.TryGetValue(item.Key,out PartitionNodeProfile? value) ||
                value != item.Value))
            throw new InvalidDataException(
                "spPartitionNode cardinality profiles changed: " +
                JsonSerializer.Serialize(actual));

        Dictionary<uint,int> pcColors = ExpectedPartitionNodePcColors();
        Dictionary<uint,int> ps2Colors = ExpectedPartitionNodePs2Colors();
        foreach (IGrouping<string,PartitionNodeObservation> group in
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
                throw new InvalidDataException(
                    $"spPartitionNode DebugColor profile changed for {group.Key}.");
        }
    }

    private static Dictionary<uint,int> ExpectedPartitionNodePcColors() => new()
    {
        [0x8F48488F]=643,[0x9F50509F]=625,[0xAF5858AF]=659,
        [0xBF6060BF]=656,[0xCF6868CF]=628,[0xDF7070DF]=623,
        [0xEF7878EF]=651,[0xFF8080FF]=667,[0xFF80C0FF]=8,
        [0xFF80FF80]=18,[0xFF80FFC0]=5,[0xFF80FFFF]=17,
        [0xFFC080FF]=1,[0xFFC0FF80]=1,[0xFFFF8080]=21,
        [0xFFFF80C0]=1,[0xFFFF80FF]=18,[0xFFFFC080]=1,[0xFFFFFF80]=11
    };

    private static Dictionary<uint,int> ExpectedPartitionNodePs2Colors() => new()
    {
        [0x8F48488F]=697,[0x9F50509F]=678,[0xAF5858AF]=716,
        [0xBF6060BF]=712,[0xCF6868CF]=680,[0xDF7070DF]=677,
        [0xEF7878EF]=709,[0xFF8080FF]=725,[0xFF80C0FF]=8,
        [0xFF80FF80]=18,[0xFF80FFC0]=5,[0xFF80FFFF]=17,
        [0xFFC080FF]=1,[0xFFC0FF80]=1,[0xFFFF8080]=21,
        [0xFFFF80C0]=1,[0xFFFF80FF]=18,[0xFFFFC080]=1,[0xFFFFFF80]=11
    };

    private static PartitionNodeComparison ComparePartitionNodes(
        IEnumerable<PartitionNodeObservation> observations,
        string leftCorpus,
        string rightCorpus,
        bool compareBytes)
    {
        Dictionary<(string Path,int Ordinal),PartitionNodeObservation> Build(
            string corpus) => observations.Where(item => item.CorpusKey == corpus)
            .ToDictionary(item => (item.CanonicalPath,item.Ordinal));
        Dictionary<(string Path,int Ordinal),PartitionNodeObservation> left =
            Build(leftCorpus);
        Dictionary<(string Path,int Ordinal),PartitionNodeObservation> right =
            Build(rightCorpus);
        var keys = left.Keys.Intersect(right.Keys).ToArray();
        int resources = keys.Select(item => item.Path).Distinct().Count();
        int equalSemantic = keys.Count(key => compareBytes
            ? left[key].SerializedSha256 == right[key].SerializedSha256
            : left[key].SemanticSignature == right[key].SemanticSignature);
        int equalColor = keys.Count(key =>
            left[key].Data.DebugColorArgb == right[key].Data.DebugColorArgb);
        return new PartitionNodeComparison(
            resources,keys.Length,equalSemantic,equalColor);
    }

    private static void UpsertPartitionNodeFieldDefinitions(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<PartitionNodeObservation> observations)
    {
        int[] occurrences = new int[8];
        foreach (PartitionNodeObservation observation in observations)
        foreach (PartitionNodeFieldAnnotation field in observation.Fields)
        {
            int type = field.Semantic switch
            {
                "partition_node.collision_info" => 0,
                "partition_node.debug_color" => 1,
                "partition_node.child" => 2,
                "partition_node.zone" => 3,
                "partition_node.zone_portal" => 4,
                "partition_node.partition_system" => 5,
                "partition_node.partition_renderable" => 6,
                "partition_node.static_render_object" => 7,
                _ => throw new InvalidDataException("Unknown partition-node semantic.")
            };
            occurrences[type]++;
        }
        (int Type,int Occurrence,string Semantic,string Display,string Kind,
            string Layout,bool Repeated,string Notes)[] definitions =
        [
            (0,-1,"partition_node.collision_info","Collision info",
                "object_relationship","spCollisionInfo relationship",true,
                "0..69; sized reference or owned inline object."),
            (1,0,"partition_node.debug_color","Debug color","argb32",
                "ARGB UInt32 stored little-endian (BGRA bytes)",false,
                "Required; 19 exact values on both platforms."),
            (2,-1,"partition_node.child","Child partition node",
                "indexed_object_relationship",
                "UInt32 slot plus inline spPartitionNode/spOctreeNode relationship",true,
                "Zero occurrences in concrete nodes; 18048 in inherited octree sections."),
            (3,0,"partition_node.zone","Zone","object_relationship",
                "sized-reference relationship to spZone",false,"Required and non-null."),
            (4,-1,"partition_node.zone_portal","Zone portal","object_relationship",
                "inline relationship to spZonePortal",true,"0..6 owned objects."),
            (5,0,"partition_node.partition_system","Partition system",
                "object_relationship","sized-reference relationship to spPartitionSystem",
                false,"Required and non-null."),
            (6,0,"partition_node.partition_renderable","Partition renderable",
                "object_relationship","null ID or inline spPartitionRenderable",
                false,"Exactly one field; null or one owned inline object."),
            (7,-1,"partition_node.static_render_object","Static render object",
                "object_relationship","spStaticRenderObject relationship",true,
                "0..595; sized reference or owned inline object.")
        ];
        foreach (var definition in definitions)
        {
            const string evidence = "confirmed_both_executables_and_full_corpus";
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',0,$type,$occurrence,$semantic,
                       $display,$kind,$layout,'read_only_research',$evidence,
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
                """);
            command.Parameters.AddWithValue("$hash",(long)typeHash);
            command.Parameters.AddWithValue("$type",definition.Type);
            command.Parameters.AddWithValue("$occurrence",definition.Occurrence);
            command.Parameters.AddWithValue("$semantic",definition.Semantic);
            command.Parameters.AddWithValue("$display",definition.Display);
            command.Parameters.AddWithValue("$kind",definition.Kind);
            command.Parameters.AddWithValue("$layout",definition.Layout);
            command.Parameters.AddWithValue("$evidence",evidence);
            command.Parameters.AddWithValue("$constraints",JsonSerializer.Serialize(new
            {
                observedDirectOccurrences = occurrences[definition.Type],
                repeated = definition.Repeated,mutationStatus = "not_enabled"
            }));
            command.Parameters.AddWithValue("$notes",definition.Notes);
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotatePartitionNodeFields(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<PartitionNodeObservation> observations)
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
        foreach (PartitionNodeObservation observation in observations)
        foreach (PartitionNodeFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException("Could not annotate spPartitionNode field.");
        }
    }

    private static int InsertPartitionNodeVariant(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IEnumerable<PartitionNodeObservation> observations,
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
                   'Common partition relationship layout','confirmed',
                   $discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$key",PartitionNodeVariantKey);
        command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
        {
            corpusCounts,
            fieldOrder = new[]
            {
                "debug_color","partition_system","zone","child*",
                "collision_info*","zone_portal*","static_render_object*",
                "partition_renderable","terminator"
            },
            childFieldConcreteOccurrences = 0,
            childFieldDerivedOctreeOccurrences = 18_048
        }));
        command.Parameters.AddWithValue("$notes",
            "The sole observed serializer layout; list cardinality and inline " +
            "subtree sizes explain the broad serialized-size range.");
        command.Parameters.AddWithValue("$utc",now);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void AssignPartitionNodeVariant(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<PartitionNodeObservation> observations,
        int variantId)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spPartitionNode decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.AddWithValue("$variant",variantId);
        foreach (PartitionNodeObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.ExecuteNonQuery();
        }
    }

    private sealed record PartitionNodeFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record PartitionNodeFieldAnnotation(
        int FieldIndex,string Semantic,string Layout,string DecodedJson);
    private sealed record PartitionNodeObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string CanonicalPath,
        SmoPartitionNodeData Data,string SerializedSha256,string SemanticSignature,
        IReadOnlyList<SmoNodeRelationship> Relationships,
        IReadOnlyList<PartitionNodeFieldAnnotation> Fields);
    private sealed record PartitionNodeProfile(
        int Objects,int Collisions,int Children,int Portals,int StaticObjects,
        int PartitionRenderables,int NullPartitionRenderables);
    private sealed record PartitionNodeComparison(
        int CommonResources,int PairedObjects,int EqualSemantic,int EqualColor);
}
