using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private const string PartitionSystemVariantKey =
        "partition_system_render_node_and_polymorphic_root";

    private static SmoResearchClassAnalysisResult AnalyzePartitionSystem(
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
            report.Profiles.Any(item => item.NamedObjectCount != item.UniqueObjectCount) ||
            !parents.SetEquals([SmoClassIds.Node]) ||
            !children.SetEquals(
                [SmoClassIds.Zone,SmoClassIds.ZonePortalNode,SmoClassIds.BspNode]))
        {
            throw new InvalidDataException(
                "spPartitionSystem corpus no longer matches its physical hierarchy.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly:false);
        List<PartitionSystemObservation> observations =
            LoadPartitionSystemObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        long fieldCount = observations.Sum(item => (long)item.Fields.Count);
        long childCount = observations.Sum(item => (long)item.Data.Node.Children.Count);
        long collisionCount = observations.Sum(
            item => (long)item.Data.Node.Collisions.Count);
        long relationshipCount = childCount + collisionCount + observations.Count +
                                 observations.Sum(item => item.Data.Renderables.Count);
        if (objectCount != 88 || observations.Count != objectCount ||
            fieldCount != 5_587 || childCount != 676 ||
            collisionCount != 4_647 || relationshipCount != 5_411)
        {
            throw new InvalidDataException(
                $"Expected 88 systems, 5587 semantic fields and 5411 " +
                $"relationships (676 children, 4647 collisions); got " +
                $"{observations.Count}, {fieldCount}, {relationshipCount}, " +
                $"{childCount} and {collisionCount}.");
        }
        RequirePartitionSystemProfiles(observations);
        PartitionSystemComparison pc = ComparePartitionSystems(
            observations,"pc-working","pc-pristine");
        PartitionSystemComparison cross = ComparePartitionSystems(
            observations,"pc-pristine","ps2-pristine");
        if (pc != new PartitionSystemComparison(29,29,29,29) ||
            cross.CommonResources != 29 || cross.PairedObjects != 29 ||
            cross.EqualSemantic != 29)
        {
            throw new InvalidDataException(
                "spPartitionSystem PC-copy or PC/PS2 pairing changed: " +
                JsonSerializer.Serialize(new { pc,cross }));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spPartitionSystem","spPartitionSystemSerializer",
            "epssfPartitionRoot","GetPartitionRoot()"
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
            clearVariants.Parameters.AddWithValue("$key",PartitionSystemVariantKey);
            clearVariants.ExecuteNonQuery();
        }

        UpsertPartitionSystemFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotatePartitionSystemFields(connection,transaction,observations);
        int variantId = InsertPartitionSystemVariant(
            connection,transaction,report.TypeHash,observations,now);
        AssignPartitionSystemVariant(
            connection,transaction,observations,variantId);

        int bspRoots = observations.Count(item =>
            item.Data.PartitionRoot.TargetTypeHash == SmoClassIds.BspNode);
        int octreeRoots = observations.Count(item =>
            item.Data.PartitionRoot.TargetTypeHash == SmoClassIds.OctreeNode);
        int zoneReferences = observations.SelectMany(item => item.Data.Node.Children)
            .Count(item => item.TargetTypeHash == SmoClassIds.Zone &&
                           item.Encoding ==
                               SmoNodeRelationshipEncoding.SizedReference);
        int portals = observations.SelectMany(item => item.Data.Node.Children)
            .Count(item => item.TargetTypeHash == SmoClassIds.ZonePortalNode);
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "reader/index/writer VA 0x0044AEF0..0x0044B359",
            "The concrete serializer calls spRenderNodeSerializer, requires one " +
            "epssfPartitionRoot relationship, and accesses the runtime pointer " +
            "at +0x1D4.",pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "reader/index/writer VA 0x001A2720..0x001A2AB4",
            "The independent MIPS serializer calls spRenderNodeSerializer and " +
            "uses the same required field 0; the runtime pointer is at +0x1E0.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload and complete three-section decode",
            $"{objectCount} named systems have Static=true, Animated=true, one " +
            $"owned inline zone, {zoneReferences} additional zone references, " +
            $"{portals} owned portal nodes and {collisionCount} collision " +
            $"references. Renderable lists are empty. Roots: {bspRoots} inline " +
            $"spBSPNode and {octreeRoots} spOctreeNode references; {fieldCount} " +
            "semantic fields were annotated.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus spPartitionSystem ordinal",
            $"PC working/pristine: {pc.EqualBytes}/{pc.PairedObjects} complete " +
            $"objects are byte-identical. PC/PS2: {cross.EqualSemantic}/" +
            $"{cross.PairedObjects} shared object graphs match after normalizing " +
            "IDs and platform-dependent inline subtree sizes. PS2 additionally " +
            "contains Gardenia03.smo.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='spatial_partition_system',
                       description='Render-node-derived owner of zones, portals, collision references and a BSP or octree partition root',
                       decode_status='read_only_decode',
                       notes='All PC/PS2 objects strictly decode as spNode + spRenderNode + required epssfPartitionRoot. The inherited renderable list is supported by the serializer but empty in the corpus.'
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
            $"One common three-section layout across {objectCount} systems and " +
            $"{relationshipCount} relationships. Roots split into {bspRoots} BSP " +
            $"and {octreeRoots} octree relations; all {cross.PairedObjects} shared " +
            "PC/PS2 systems have equal normalized graphs.");
    }

    private static List<PartitionSystemObservation>
        LoadPartitionSystemObservations(SqliteConnection connection)
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.PartitionSystem);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<PartitionSystemFileLocation>();
        while (reader.Read())
        {
            locations.Add(new PartitionSystemFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<PartitionSystemObservation>();
        foreach (PartitionSystemFileLocation location in locations)
        {
            byte[] bytes = ReadPartitionSystemResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.PartitionSystem))
            {
                if (!SmoPartitionSystemDecoder.TryDecode(
                        document,entry,out SmoPartitionSystemData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                if (entry.ParentIndex is not int parentIndex ||
                    document.Objects[parentIndex].TypeHash != SmoClassIds.Node)
                {
                    throw new InvalidDataException(
                        "spPartitionSystem has an unexpected physical parent.");
                }
                result.Add(CreatePartitionSystemObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return result;
    }

    private static byte[] ReadPartitionSystemResource(
        PartitionSystemFileLocation location)
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
                "PCK partition-system occurrence is incomplete.");
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

    private static PartitionSystemObservation CreatePartitionSystemObservation(
        PartitionSystemFileLocation location,SmoDocument document,
        SmoObjectEntry entry,int ordinal,SmoPartitionSystemData decoded)
    {
        IReadOnlyList<SmoObjectField> direct = SmoObjectFieldReader.Read(document,entry);
        Dictionary<uint,SmoObjectEntry> objectsById = document.Objects
            .GroupBy(candidate => candidate.Id).Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key,group => group.Single());
        var fields = new List<PartitionSystemFieldAnnotation>();
        var signature = new List<string>();
        for (int index = 0; index < direct.Count; index++)
        {
            SmoObjectField field = direct[index];
            if (field.FieldType == 0 && field.PayloadSize == 0)
                continue;
            if (!SmoSerializedFieldRegistry.TryDescribeField(
                    entry.TypeHash,direct,index,
                    out SmoSerializedFieldDescriptor? descriptor) || descriptor is null)
            {
                throw new InvalidDataException(
                    "Unregistered spPartitionSystem direct field.");
            }
            if (descriptor.Key is "node.is_static" or "node.is_animated")
            {
                bool value = field.PayloadSize == 1 && field.Payload.Span[0] == 1;
                fields.Add(new PartitionSystemFieldAnnotation(
                    index,descriptor.Key,"Boolean",JsonSerializer.Serialize(value)));
                signature.Add($"{descriptor.Key}:{value}");
                continue;
            }
            if (!SmoNodeDecoder.TryDecodeRelationship(
                    field.Payload.Span,objectsById,
                    out SmoNodeRelationship? relationship) || relationship is null)
            {
                throw new InvalidDataException(
                    "Invalid spPartitionSystem relationship during annotation.");
            }
            string encoding = PartitionSystemEncodingName(relationship.Encoding);
            string targetName = relationship.TargetName?.TrimEnd('\0') ?? string.Empty;
            string layout = descriptor.Key switch
            {
                "node.child" => "zone or zone-portal-node relationship",
                "node.collision" => "sized-reference relationship to spCollisionInfo",
                "render_node.renderable" => "inherited renderable relationship",
                "partition_system.partition_root" =>
                    "inline spBSPNode or sized-reference spOctreeNode relationship",
                _ => descriptor.PayloadLayout
            };
            fields.Add(new PartitionSystemFieldAnnotation(
                index,descriptor.Key,layout,JsonSerializer.Serialize(new
                {
                    relationship.ObjectId,Encoding=encoding,
                    relationship.InlineSerializedSize,
                    relationship.TargetObjectIndex,
                    TargetTypeHash=relationship.TargetTypeHash.HasValue
                        ? $"0x{relationship.TargetTypeHash.Value:X8}" : null,
                    TargetClass=relationship.TargetTypeHash.HasValue
                        ? SmoClassRegistry.GetDisplayName(
                            relationship.TargetTypeHash.Value) : null,
                    TargetName=targetName
                })));
            signature.Add($"{descriptor.Key}:{encoding}:" +
                          $"{relationship.TargetTypeHash:X8}:" +
                          targetName.ToLowerInvariant());
        }
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        return new PartitionSystemObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            decoded,serializedHash,string.Join("|",signature),fields.AsReadOnly());
    }

    private static void RequirePartitionSystemProfiles(
        IReadOnlyList<PartitionSystemObservation> observations)
    {
        var expected = new Dictionary<string,(int Objects,int Children,
            int Collisions,int BspRoots,int OctreeRoots)>
        {
            ["pc-working"]=(29,225,1_549,18,11),
            ["pc-pristine"]=(29,225,1_549,18,11),
            ["ps2-pristine"]=(30,226,1_549,18,12)
        };
        var actual = observations.GroupBy(item => item.CorpusKey).ToDictionary(
            group => group.Key,group => (
                Objects:group.Count(),
                Children:group.Sum(item => item.Data.Node.Children.Count),
                Collisions:group.Sum(item => item.Data.Node.Collisions.Count),
                BspRoots:group.Count(item =>
                    item.Data.PartitionRoot.TargetTypeHash == SmoClassIds.BspNode),
                OctreeRoots:group.Count(item =>
                    item.Data.PartitionRoot.TargetTypeHash == SmoClassIds.OctreeNode)));
        if (actual.Count != expected.Count || expected.Any(item =>
                !actual.TryGetValue(item.Key,out var value) || value != item.Value) ||
            observations.Any(item => !item.Data.Node.IsStatic ||
                !item.Data.Node.IsAnimated || item.Data.Renderables.Count != 0))
        {
            throw new InvalidDataException(
                "spPartitionSystem profile changed: " + JsonSerializer.Serialize(actual));
        }
    }

    private static PartitionSystemComparison ComparePartitionSystems(
        IEnumerable<PartitionSystemObservation> observations,string leftCorpus,
        string rightCorpus)
    {
        Dictionary<(string Path,int Ordinal),PartitionSystemObservation> Build(
            string corpus) => observations.Where(item => item.CorpusKey == corpus)
                .ToDictionary(item => (item.CanonicalPath,item.Ordinal));
        var left = Build(leftCorpus);
        var right = Build(rightCorpus);
        var keys = left.Keys.Intersect(right.Keys).ToArray();
        return new PartitionSystemComparison(
            keys.Select(item => item.Path).Distinct().Count(),keys.Length,
            keys.Count(key => left[key].SemanticSignature ==
                              right[key].SemanticSignature),
            keys.Count(key => left[key].SerializedSha256 ==
                              right[key].SerializedSha256));
    }

    private static void UpsertPartitionSystemFieldDefinitions(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<PartitionSystemObservation> observations)
    {
        (int Section,int Type,int Occurrence,string Semantic,string Display,
            string Kind,string Layout,bool Repeated,string Evidence,string Notes)[] defs =
        [
            (2,0,0,"node.position","Local position","vector3","three little-endian Single values",false,"confirmed_both_executables_not_observed_corpus","Inherited default (0,0,0)."),
            (2,1,0,"node.rotation","Local rotation","quaternion","four little-endian Single values in X/Y/Z/W order",false,"confirmed_both_executables_not_observed_corpus","Inherited identity default."),
            (2,2,0,"node.scale","Local scale","vector3","three little-endian Single values",false,"confirmed_both_executables_not_observed_corpus","Inherited default (1,1,1)."),
            (2,3,0,"node.is_bone","Bone node","boolean","one byte 0 or 1",false,"confirmed_both_executables_not_observed_corpus","Inherited false default."),
            (2,4,0,"node.is_static","Static node","boolean","one byte 0 or 1",false,"confirmed_both_executables_and_full_corpus","Required and always true."),
            (2,5,-1,"node.child","Child node","object_relationship","owned inline zone, sized zone reference, or owned inline spZonePortalNode",true,"confirmed_both_executables_and_full_corpus","Ordered as owned zone, zone references, then portals."),
            (2,6,0,"node.billboard_axis","Billboard axis","uint32_enum","UInt32 axis 1 or 2",false,"confirmed_both_executables_not_observed_corpus","Inherited default 0."),
            (2,7,-1,"node.collision","Collision info","object_relationship","sized-reference relationship to spCollisionInfo",true,"confirmed_both_executables_and_full_corpus","All system-level collision references."),
            (2,8,0,"node.is_animated","Animated node","boolean","one byte 0 or 1",false,"confirmed_both_executables_and_full_corpus","Required and always true."),
            (1,0,-1,"render_node.renderable","Renderable","object_relationship","inherited renderable relationship",true,"confirmed_both_executables_not_observed_corpus","The list is empty in every corpus object."),
            (0,0,0,"partition_system.partition_root","Partition root","object_relationship","inline spBSPNode or sized-reference spOctreeNode relationship",false,"confirmed_both_executables_and_full_corpus","Required non-null polymorphic spPartitionNode root.")
        ];
        foreach (var definition in defs)
        {
            int observed = observations.Sum(item => item.Fields.Count(field =>
                field.Semantic == definition.Semantic));
            using SqliteCommand command = CreateCommand(connection,transaction,"""
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

    private static void AnnotatePartitionSystemFields(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<PartitionSystemObservation> observations)
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
        foreach (PartitionSystemObservation observation in observations)
        foreach (PartitionSystemFieldAnnotation field in observation.Fields)
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
                    "Could not annotate spPartitionSystem field.");
            }
        }
    }

    private static int InsertPartitionSystemVariant(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IEnumerable<PartitionSystemObservation> observations,string now)
    {
        Dictionary<string,int> corpusCounts = observations
            .GroupBy(item => item.CorpusKey)
            .ToDictionary(group => group.Key,group => group.Count());
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,status,
                discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2',$key,
                   'Render-node partition system with polymorphic root',
                   'confirmed',$discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$key",PartitionSystemVariantKey);
        command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
        {
            corpusCounts,
            sections=new[] { "spNode","spRenderNode","spPartitionSystem" },
            nodeFieldOrder=new[] { "is_static","is_animated","child[]",
                "collision[]","terminator" },
            renderNodeFieldOrder=new[] { "renderable[]","terminator" },
            ownFieldOrder=new[] { "partition_root","terminator" },
            rootClasses=new[] { "spBSPNode","spOctreeNode" }
        }));
        command.Parameters.AddWithValue("$notes",
            "One serializer layout. Raw shapes vary with repeated relationships " +
            "and the sizes of inline zone, portal and BSP subtrees.");
        command.Parameters.AddWithValue("$utc",now);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void AssignPartitionSystemVariant(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<PartitionSystemObservation> observations,int variantId)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spPartitionSystem decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.AddWithValue("$variant",variantId);
        foreach (PartitionSystemObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.ExecuteNonQuery();
        }
    }

    private static string PartitionSystemEncodingName(
        SmoNodeRelationshipEncoding encoding) => encoding switch
        {
            SmoNodeRelationshipEncoding.IdOnly => "id_only",
            SmoNodeRelationshipEncoding.SizedReference => "sized_reference",
            _ => "inline_object"
        };

    private sealed record PartitionSystemFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record PartitionSystemFieldAnnotation(
        int FieldIndex,string Semantic,string Layout,string DecodedJson);
    private sealed record PartitionSystemObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string CanonicalPath,
        SmoPartitionSystemData Data,string SerializedSha256,string SemanticSignature,
        IReadOnlyList<PartitionSystemFieldAnnotation> Fields);
    private sealed record PartitionSystemComparison(
        int CommonResources,int PairedObjects,int EqualSemantic,int EqualBytes);
}
