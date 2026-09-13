using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private const string OctreeNodeVariantKey = "octree_node_common_layout";

    private static SmoResearchClassAnalysisResult AnalyzeOctreeNode(
        string databasePath,SmoResearchClassReport report)
    {
        RequireSpatialRuntimeProfile(report);
        HashSet<uint> parents = report.Relations
            .Where(item => item.Direction == "parent" && item.RelatedTypeHash.HasValue)
            .Select(item => item.RelatedTypeHash!.Value).ToHashSet();
        HashSet<uint> children = report.Relations
            .Where(item => item.Direction == "child" && item.RelatedTypeHash.HasValue)
            .Select(item => item.RelatedTypeHash!.Value).ToHashSet();
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item => item.NamedObjectCount != 0) ||
            !parents.SetEquals([SmoClassIds.OctreeNode,SmoClassIds.Zone]) ||
            !children.SetEquals([SmoClassIds.PartitionNode,SmoClassIds.OctreeNode]))
        {
            throw new InvalidDataException(
                "spOctreeNode corpus no longer matches its physical hierarchy.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly:false);
        List<OctreeNodeObservation> observations =
            LoadOctreeNodeObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        long relationshipCount = observations.Count * 11L;
        long inlineChildren = observations.Sum(item => item.Data.PartitionNode.Children.Count);
        long fieldCount = observations.Sum(item => (long)item.Fields.Count);
        if (objectCount != 2_256 || observations.Count != objectCount ||
            relationshipCount != 24_816 || inlineChildren != 18_048 ||
            fieldCount != 33_840)
        {
            throw new InvalidDataException(
                $"Expected 2256 octree nodes, 24816 relationships, 18048 inline " +
                $"children and 33840 semantic fields; got {observations.Count}, " +
                $"{relationshipCount}, {inlineChildren} and {fieldCount}.");
        }
        RequireOctreeNodeProfiles(observations);
        Dictionary<uint,string> octantGeometry =
            RecoverOctreeChildGeometry(observations);
        OctreeNodeComparison pc = CompareOctreeNodes(
            observations,"pc-working","pc-pristine",compareBytes:true);
        OctreeNodeComparison cross = CompareOctreeNodes(
            observations,"pc-pristine","ps2-pristine",compareBytes:false);
        if (pc != new OctreeNodeComparison(11,731,731,731) ||
            cross != new OctreeNodeComparison(11,731,731,731))
        {
            throw new InvalidDataException(
                "spOctreeNode PC-copy or PC/PS2 pairing changed: " +
                JsonSerializer.Serialize(new { pc,cross }));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spOctreeNode","spOctreeNodeSerializer","eonsfOctreeNodePivot",
            "eonsfOctreeNodeMins","eonsfOctreeNodeMaxs",
            "epnsfPartitionNodeChild"
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
            clearVariants.Parameters.AddWithValue("$key",OctreeNodeVariantKey);
            clearVariants.ExecuteNonQuery();
        }

        UpsertOctreeNodeFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateOctreeNodeFields(connection,transaction,observations);
        int variantId = InsertOctreeNodeVariant(
            connection,transaction,report.TypeHash,observations,now);
        AssignOctreeNodeVariant(connection,transaction,observations,variantId);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "reader/writer VA 0x0044C8E0..0x0044CD6D",
            "The PC serializer first calls the spPartitionNode serializer, then " +
            "maps fields 0/1/2 to Pivot/Mins/Maxs at runtime offsets " +
            "+0x84/+0xB0/+0xBC.",pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "reader/writer VA 0x001A0800..0x001A0BEC",
            "The independent PS2 serializer confirms fields 0/1/2 as " +
            "Pivot/Mins/Maxs at runtime offsets +0x70/+0x9C/+0xA8.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload and complete decode of every PC and PS2 object",
            $"{objectCount} unnamed objects have one inherited partition section, " +
            "exactly eight indexed inline children, one own Pivot/Mins/Maxs " +
            $"section and {fieldCount} annotated semantic fields. Every Pivot is " +
            "component-wise inside its Mins/Maxs box. Nested-node octants: " +
            string.Join(", ",octantGeometry.OrderBy(item => item.Key)
                .Select(item => $"{item.Key}={item.Value}")) + ".",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus spOctreeNode ordinal",
            $"PC working/pristine: {pc.EqualSemantic}/{pc.PairedObjects} byte-identical " +
            $"pairs. PC/PS2: {cross.EqualSemantic}/{cross.PairedObjects} semantic " +
            "matches, including child slots/types and all three vectors. PS2 adds " +
            "63 nodes in Gardenia03.smo.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='spatial_octree_node',
                       description='Eight-way partition node with an explicit pivot and axis-aligned minimum/maximum bounds',
                       decode_status='read_only_decode',
                       notes='All PC/PS2 objects strictly decode with one inherited spPartitionNode section and one three-Vector3 octree section. Child field 2 is UInt32 slot plus inline relationship.'
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
            $"One common two-section octree layout decoded across {objectCount} " +
            $"objects. Every node has eight indexed inline children; all " +
            $"{cross.PairedObjects} shared PC/PS2 nodes are semantically equal. " +
            "Octants: " + string.Join(", ",octantGeometry.OrderBy(item => item.Key)
                .Select(item => $"{item.Key}={item.Value}")) + ".");
    }

    private static List<OctreeNodeObservation> LoadOctreeNodeObservations(
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.OctreeNode);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<OctreeNodeFileLocation>();
        while (reader.Read())
        {
            locations.Add(new OctreeNodeFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<OctreeNodeObservation>();
        foreach (OctreeNodeFileLocation location in locations)
        {
            byte[] bytes = ReadOctreeNodeResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.OctreeNode))
            {
                if (!SmoOctreeNodeDecoder.TryDecode(
                        document,entry,out SmoOctreeNodeData? decoded,
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
                        "spOctreeNode has an unexpected physical parent.");
                }
                result.Add(CreateOctreeNodeObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return result;
    }

    private static byte[] ReadOctreeNodeResource(OctreeNodeFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
        {
            return File.ReadAllBytes(Path.Combine(location.SourceRoot,
                location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        }
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
            throw new InvalidDataException("PCK octree-node occurrence is incomplete.");
        string archive = Path.Combine(location.SourceRoot,
            location.ContainerPath.Replace('/',Path.DirectorySeparatorChar));
        using var stream = new FileStream(
            archive,FileMode.Open,FileAccess.Read,FileShare.Read);
        stream.Position = location.ByteOffset.Value;
        byte[] data = new byte[checked((int)location.ByteSize)];
        stream.ReadExactly(data);
        return data;
    }

    private static OctreeNodeObservation CreateOctreeNodeObservation(
        OctreeNodeFileLocation location,SmoDocument document,SmoObjectEntry entry,
        int ordinal,SmoOctreeNodeData decoded)
    {
        IReadOnlyList<SmoObjectField> direct = SmoObjectFieldReader.Read(document,entry);
        var fields = new List<OctreeNodeFieldAnnotation>(15);
        fields.Add(new OctreeNodeFieldAnnotation(
            0,"partition_node.debug_color","ARGB UInt32",
            JsonSerializer.Serialize(new { Argb=$"0x{decoded.PartitionNode.DebugColorArgb:X8}" })));
        Dictionary<uint,SmoObjectEntry> objectsById = document.Objects
            .GroupBy(candidate => candidate.Id).Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key,group => group.Single());
        var signatureRelationships = new List<string>(10);
        for (int index=1;index<=11;index++)
        {
            SmoObjectField field = direct[index];
            uint? slot = null;
            ReadOnlySpan<byte> relationshipPayload = field.Payload.Span;
            if (field.FieldType == 2)
            {
                slot = BinaryPrimitives.ReadUInt32LittleEndian(
                    relationshipPayload[..sizeof(uint)]);
                relationshipPayload = relationshipPayload[sizeof(uint)..];
            }
            if (!SmoNodeDecoder.TryDecodeRelationship(
                    relationshipPayload,objectsById,
                    out SmoNodeRelationship? relationship) || relationship is null)
                throw new InvalidDataException("Invalid octree relationship field.");
            string encoding = GetPartitionNodeEncodingName(relationship.Encoding);
            string targetName = relationship.TargetName?.TrimEnd('\0') ?? string.Empty;
            signatureRelationships.Add(
                $"{field.FieldType}:{slot}:{encoding}:{relationship.TargetTypeHash:X8}:" +
                targetName);
            (string semantic,string layout) = field.FieldType switch
            {
                2 => ("partition_node.child",
                    "UInt32 octant slot followed by inline spPartitionNode/spOctreeNode relationship"),
                3 => ("partition_node.zone","sized-reference relationship to spZone"),
                5 => ("partition_node.partition_system",
                    "sized-reference relationship to spPartitionSystem"),
                6 => ("partition_node.partition_renderable","null object ID"),
                _ => throw new InvalidDataException("Unexpected octree base field.")
            };
            fields.Add(new OctreeNodeFieldAnnotation(
                index,semantic,layout,JsonSerializer.Serialize(new
                {
                    Slot=slot,relationship.ObjectId,Encoding=encoding,
                    relationship.InlineSerializedSize,relationship.TargetObjectIndex,
                    TargetTypeHash=relationship.TargetTypeHash.HasValue
                        ? $"0x{relationship.TargetTypeHash.Value:X8}" : null,
                    TargetName=targetName
                })));
        }
        AddVectorField(fields,13,"octree_node.pivot","Pivot",decoded.Pivot);
        AddVectorField(fields,14,"octree_node.minimum","Bounds minimum",decoded.Minimum);
        AddVectorField(fields,15,"octree_node.maximum","Bounds maximum",decoded.Maximum);
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        string semanticSignature = JsonSerializer.Serialize(new
        {
            decoded.PartitionNode.DebugColorArgb,Relationships=signatureRelationships,
            decoded.Pivot,decoded.Minimum,decoded.Maximum
        });
        return new OctreeNodeObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            decoded,serializedHash,semanticSignature,fields.AsReadOnly(),
            entry.SerializedSize);
    }

    private static void AddVectorField(
        List<OctreeNodeFieldAnnotation> fields,int fieldIndex,string semantic,
        string layout,Vector3 value) => fields.Add(new OctreeNodeFieldAnnotation(
            fieldIndex,semantic,layout,JsonSerializer.Serialize(new
            {
                value.X,value.Y,value.Z
            })));

    private static void RequireOctreeNodeProfiles(
        IReadOnlyList<OctreeNodeObservation> observations)
    {
        if (observations.Any(item =>
                item.Data.PartitionNode.DebugColorArgb != 0xFF8080FFu))
        {
            throw new InvalidDataException(
                "spOctreeNode DebugColor is no longer the corpus-wide 0xFF8080FF.");
        }
        var expected = new Dictionary<string,(int Objects,long Minimum,long Maximum,
            int PartitionChildren,int OctreeChildren)>
        {
            ["pc-working"]=(731,620,4_850_123,5_128,720),
            ["pc-pristine"]=(731,620,4_850_123,5_128,720),
            ["ps2-pristine"]=(794,620,3_839_287,5_570,782)
        };
        var actual = observations.GroupBy(item => item.CorpusKey).ToDictionary(
            group => group.Key,group =>
            {
                var relations = group.SelectMany(item =>
                    item.Data.PartitionNode.Children.Select(child => child.Relationship));
                return (
                    Objects:group.Count(),Minimum:group.Min(item => item.SerializedSize),
                    Maximum:group.Max(item => item.SerializedSize),
                    PartitionChildren:relations.Count(item =>
                        item.TargetTypeHash == SmoClassIds.PartitionNode),
                    OctreeChildren:relations.Count(item =>
                        item.TargetTypeHash == SmoClassIds.OctreeNode));
            });
        if (actual.Count != expected.Count || expected.Any(item =>
                !actual.TryGetValue(item.Key,out var value) || value != item.Value))
        {
            throw new InvalidDataException(
                "spOctreeNode profile changed: " + JsonSerializer.Serialize(actual));
        }
    }

    private static Dictionary<uint,string> RecoverOctreeChildGeometry(
        IReadOnlyList<OctreeNodeObservation> observations)
    {
        var patterns = new Dictionary<uint,HashSet<string>>();
        foreach (IGrouping<int,OctreeNodeObservation> file in
                 observations.GroupBy(item => item.FileId))
        {
            Dictionary<int,OctreeNodeObservation> byIndex =
                file.ToDictionary(item => item.ObjectIndex);
            foreach (OctreeNodeObservation parent in file)
            foreach (SmoPartitionNodeChild child in parent.Data.PartitionNode.Children)
            {
                if (child.Relationship.TargetObjectIndex is not int childIndex ||
                    !byIndex.TryGetValue(childIndex,out OctreeNodeObservation? nested))
                    continue;
                string pattern = string.Concat(
                    ClassifyOctreeAxis(parent.Data.Minimum.X,parent.Data.Pivot.X,
                        parent.Data.Maximum.X,nested.Data.Minimum.X,
                        nested.Data.Maximum.X),
                    ClassifyOctreeAxis(parent.Data.Minimum.Y,parent.Data.Pivot.Y,
                        parent.Data.Maximum.Y,nested.Data.Minimum.Y,
                        nested.Data.Maximum.Y),
                    ClassifyOctreeAxis(parent.Data.Minimum.Z,parent.Data.Pivot.Z,
                        parent.Data.Maximum.Z,nested.Data.Minimum.Z,
                        nested.Data.Maximum.Z));
                if (pattern.Contains('?'))
                    throw new InvalidDataException(
                        "Nested spOctreeNode bounds do not match the parent split.");
                if (!patterns.TryGetValue(child.SlotIndex,out HashSet<string>? values))
                    patterns[child.SlotIndex]=values=[];
                values.Add(pattern);
            }
        }
        if (patterns.Count != 8 || patterns.Any(item => item.Value.Count != 1))
            throw new InvalidDataException(
                "Could not recover one stable geometry pattern per octant: " +
                JsonSerializer.Serialize(patterns));
        return patterns.ToDictionary(item => item.Key,item => item.Value.Single());
    }

    private static char ClassifyOctreeAxis(
        float parentMinimum,float parentPivot,float parentMaximum,
        float childMinimum,float childMaximum)
    {
        if (childMinimum == parentMinimum && childMaximum == parentPivot)
            return 'L';
        if (childMinimum == parentPivot && childMaximum == parentMaximum)
            return 'H';
        return '?';
    }

    private static OctreeNodeComparison CompareOctreeNodes(
        IEnumerable<OctreeNodeObservation> observations,string leftCorpus,
        string rightCorpus,bool compareBytes)
    {
        Dictionary<(string Path,int Ordinal),OctreeNodeObservation> Build(string corpus) =>
            observations.Where(item => item.CorpusKey == corpus)
                .ToDictionary(item => (item.CanonicalPath,item.Ordinal));
        var left=Build(leftCorpus);
        var right=Build(rightCorpus);
        var keys=left.Keys.Intersect(right.Keys).ToArray();
        int resources=keys.Select(item => item.Path).Distinct().Count();
        int equalSemantic=keys.Count(key => compareBytes
            ? left[key].SerializedSha256 == right[key].SerializedSha256
            : left[key].SemanticSignature == right[key].SemanticSignature);
        int equalBounds=keys.Count(key =>
            left[key].Data.Pivot == right[key].Data.Pivot &&
            left[key].Data.Minimum == right[key].Data.Minimum &&
            left[key].Data.Maximum == right[key].Data.Maximum);
        return new OctreeNodeComparison(resources,keys.Length,equalSemantic,equalBounds);
    }

    private static void UpsertOctreeNodeFieldDefinitions(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<OctreeNodeObservation> observations)
    {
        (int Section,int Type,int Occurrence,string Semantic,string Display,
            string Kind,string Layout,bool Repeated,string Evidence,string Notes)[] defs =
        [
            (1,0,-1,"partition_node.collision_info","Collision info","object_relationship",
                "spCollisionInfo relationship",true,"confirmed_both_executables_not_observed_corpus","Inherited; absent from octree nodes."),
            (1,1,0,"partition_node.debug_color","Debug color","argb32",
                "ARGB UInt32 stored little-endian",false,"confirmed_both_executables_and_full_corpus","Inherited; required."),
            (1,2,-1,"partition_node.child","Indexed octant child","indexed_object_relationship",
                "UInt32 slot plus inline spPartitionNode/spOctreeNode relationship",true,"confirmed_both_executables_and_full_corpus","Exactly eight fields with slots 0..7."),
            (1,3,0,"partition_node.zone","Zone","object_relationship",
                "sized-reference relationship to spZone",false,"confirmed_both_executables_and_full_corpus","Inherited; required."),
            (1,4,-1,"partition_node.zone_portal","Zone portal","object_relationship",
                "inline spZonePortal relationship",true,"confirmed_both_executables_not_observed_corpus","Inherited; absent from octree nodes."),
            (1,5,0,"partition_node.partition_system","Partition system","object_relationship",
                "sized-reference relationship to spPartitionSystem",false,"confirmed_both_executables_and_full_corpus","Inherited; required."),
            (1,6,0,"partition_node.partition_renderable","Partition renderable","object_relationship",
                "null object ID",false,"confirmed_both_executables_and_full_corpus","Inherited; always null in octree nodes."),
            (1,7,-1,"partition_node.static_render_object","Static render object","object_relationship",
                "spStaticRenderObject relationship",true,"confirmed_both_executables_not_observed_corpus","Inherited; absent from octree nodes."),
            (0,0,0,"octree_node.pivot","Partition pivot","vector3","three little-endian Single values",false,"confirmed_both_executables_and_full_corpus","Required; component-wise inside the bounds."),
            (0,1,0,"octree_node.minimum","Bounds minimum","vector3","three little-endian Single values",false,"confirmed_both_executables_and_full_corpus","Required AABB minimum."),
            (0,2,0,"octree_node.maximum","Bounds maximum","vector3","three little-endian Single values",false,"confirmed_both_executables_and_full_corpus","Required AABB maximum.")
        ];
        foreach (var definition in defs)
        {
            int observed = observations.Sum(item => item.Fields.Count(field =>
                field.Semantic == definition.Semantic));
            using SqliteCommand command=CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',$section,$type,$occurrence,$semantic,
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
            command.Parameters.AddWithValue("$section",definition.Section);
            command.Parameters.AddWithValue("$type",definition.Type);
            command.Parameters.AddWithValue("$occurrence",definition.Occurrence);
            command.Parameters.AddWithValue("$semantic",definition.Semantic);
            command.Parameters.AddWithValue("$display",definition.Display);
            command.Parameters.AddWithValue("$kind",definition.Kind);
            command.Parameters.AddWithValue("$layout",definition.Layout);
            command.Parameters.AddWithValue("$evidence",definition.Evidence);
            command.Parameters.AddWithValue("$constraints",JsonSerializer.Serialize(new
            {
                observedDirectOccurrences=observed,repeated=definition.Repeated,
                mutationStatus="not_enabled"
            }));
            command.Parameters.AddWithValue("$notes",definition.Notes);
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotateOctreeNodeFields(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<OctreeNodeObservation> observations)
    {
        using SqliteCommand command=CreateCommand(connection,transaction,"""
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
        foreach (OctreeNodeObservation observation in observations)
        foreach (OctreeNodeFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value=field.Semantic;
            command.Parameters["$layout"].Value=field.Layout;
            command.Parameters["$value"].Value=field.DecodedJson;
            command.Parameters["$file"].Value=observation.FileId;
            command.Parameters["$object"].Value=observation.ObjectIndex;
            command.Parameters["$field"].Value=field.FieldIndex;
            if (command.ExecuteNonQuery()!=1)
                throw new InvalidDataException("Could not annotate spOctreeNode field.");
        }
    }

    private static int InsertOctreeNodeVariant(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IEnumerable<OctreeNodeObservation> observations,string now)
    {
        Dictionary<string,int> corpusCounts=observations.GroupBy(item=>item.CorpusKey)
            .ToDictionary(group=>group.Key,group=>group.Count());
        using SqliteCommand command=CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,status,
                discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2',$key,'Common eight-way octree layout',
                   'confirmed',$discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$key",OctreeNodeVariantKey);
        command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
        {
            corpusCounts,
            inheritedFieldOrder=new[] { "debug_color","partition_system","zone",
                "child[0..7]","null_partition_renderable","terminator" },
            ownFieldOrder=new[] { "pivot","minimum","maximum","terminator" }
        }));
        command.Parameters.AddWithValue("$notes",
            "The sole normalized layout. Raw field-shape diversity comes only " +
            "from the serialized sizes of the eight inline child subtrees.");
        command.Parameters.AddWithValue("$utc",now);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void AssignOctreeNodeVariant(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<OctreeNodeObservation> observations,int variantId)
    {
        using SqliteCommand command=CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spOctreeNode decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.AddWithValue("$variant",variantId);
        foreach (OctreeNodeObservation observation in observations)
        {
            command.Parameters["$file"].Value=observation.FileId;
            command.Parameters["$object"].Value=observation.ObjectIndex;
            command.ExecuteNonQuery();
        }
    }

    private sealed record OctreeNodeFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record OctreeNodeFieldAnnotation(
        int FieldIndex,string Semantic,string Layout,string DecodedJson);
    private sealed record OctreeNodeObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string CanonicalPath,
        SmoOctreeNodeData Data,string SerializedSha256,string SemanticSignature,
        IReadOnlyList<OctreeNodeFieldAnnotation> Fields,long SerializedSize);
    private sealed record OctreeNodeComparison(
        int CommonResources,int PairedObjects,int EqualSemantic,int EqualBounds);
}
