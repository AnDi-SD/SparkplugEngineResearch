using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private const string ZoneVariantKey =
        "zone_node_and_polymorphic_local_partition_roots";

    private static SmoResearchClassAnalysisResult AnalyzeZone(
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
            !parents.SetEquals(
                [SmoClassIds.PartitionSystem,SmoClassIds.ZonePortal,SmoClassIds.Node]) ||
            !children.SetEquals([SmoClassIds.PartitionNode,SmoClassIds.OctreeNode]))
        {
            throw new InvalidDataException(
                "spZone corpus no longer matches its physical hierarchy.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly:false);
        List<ZoneObservation> observations = LoadZoneObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        long fieldCount = observations.Sum(item => (long)item.Fields.Count);
        long relationshipCount = observations.Sum(
            item => (long)item.Data.LocalPartitionRoots.Count);
        if (objectCount != 369 || observations.Count != objectCount ||
            fieldCount != 1_231 || relationshipCount != 412)
        {
            throw new InvalidDataException(
                $"Expected 369 zones, 1231 semantic fields and 412 local roots; " +
                $"got {observations.Count}, {fieldCount} and {relationshipCount}.");
        }
        RequireZoneProfiles(observations);
        ZoneComparison pc = CompareZones(
            observations,"pc-working","pc-pristine");
        ZoneComparison cross = CompareZones(
            observations,"pc-pristine","ps2-pristine");
        if (pc != new ZoneComparison(30,123,123,123) ||
            cross.CommonResources != 30 || cross.PairedObjects != 123 ||
            cross.EqualSemantic != 122)
        {
            throw new InvalidDataException(
                "spZone PC-copy or PC/PS2 pairing changed: " +
                JsonSerializer.Serialize(new { pc,cross }));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spZoneSerializer","ezsfZoneLocalPartitionRoot",
            "GetLocalPartitionRoot( i )","No empty spZone->spPartitionNode"
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
            clearVariants.Parameters.AddWithValue("$key",ZoneVariantKey);
            clearVariants.ExecuteNonQuery();
        }

        UpsertZoneFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateZoneFields(connection,transaction,observations);
        int variantId = InsertZoneVariant(
            connection,transaction,report.TypeHash,observations,now);
        AssignZoneVariant(connection,transaction,observations,variantId);

        int partitionRoots = observations.SelectMany(
                item => item.Data.LocalPartitionRoots)
            .Count(item => item.TargetTypeHash == SmoClassIds.PartitionNode);
        int octreeRoots = observations.SelectMany(
                item => item.Data.LocalPartitionRoots)
            .Count(item => item.TargetTypeHash == SmoClassIds.OctreeNode);
        int rootless = observations.Count(item =>
            item.Data.LocalPartitionRoots.Count == 0);
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "reader/index/writer VA 0x0044D720..0x0044DC82",
            "The serializer delegates the inherited section to spNodeSerializer, " +
            "repeats field 0 for LocalPartitionRoot, creates spPartitionNode on " +
            "read, and stores its runtime vector at +0xB8/+0xBC.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "reader/index/writer VA 0x001A3B30..0x001A3F70",
            "The independent MIPS serializer has the same repeated field-0 " +
            "contract and spPartitionNode factory hash 0x67672341; its runtime " +
            "root array/count are at +0xC0/+0xC4.",ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload and complete two-section decode",
            $"{objectCount} named sector/zone objects contain {relationshipCount} " +
            $"owned inline local roots: {partitionRoots} spPartitionNode and " +
            $"{octreeRoots} spOctreeNode. Root multiplicity is 0..4; only the two " +
            $"PC corpus copies of Gardenia03 are rootless. {fieldCount} semantic " +
            "fields were annotated.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus spZone ordinal",
            $"PC working/pristine: {pc.EqualBytes}/{pc.PairedObjects} complete " +
            $"objects are byte-identical. PC/PS2: {cross.EqualSemantic}/" +
            $"{cross.PairedObjects} local graphs match after excluding inline " +
            "platform subtree bytes. Gardenia03 is the sole semantic difference: " +
            "PC has a rootless spNode-owned zone, PS2 an octree-root zone owned by " +
            "spPartitionSystem.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='spatial_zone',
                       description='Named spatial sector with inherited placement and zero or more owned local partition-tree roots',
                       decode_status='read_only_decode',
                       notes='All PC/PS2 objects strictly decode as spNode + repeated ezsfZoneLocalPartitionRoot. The declared spPartitionNode relationship is polymorphic and also stores spOctreeNode.'
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
            $"One two-section serializer layout across {objectCount} zones and " +
            $"{relationshipCount} local roots ({partitionRoots} partition, " +
            $"{octreeRoots} octree). PC copies match exactly; {cross.EqualSemantic}/" +
            $"{cross.PairedObjects} PC/PS2 zone graphs match.");
    }

    private static List<ZoneObservation> LoadZoneObservations(
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.Zone);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<ZoneFileLocation>();
        while (reader.Read())
        {
            locations.Add(new ZoneFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<ZoneObservation>();
        foreach (ZoneFileLocation location in locations)
        {
            byte[] bytes = ReadZoneResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.Zone))
            {
                if (!SmoZoneDecoder.TryDecode(
                        document,entry,out SmoZoneData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                if (entry.ParentIndex is not int parentIndex ||
                    document.Objects[parentIndex].TypeHash is not
                        SmoClassIds.PartitionSystem and not SmoClassIds.ZonePortal and
                        not SmoClassIds.Node)
                {
                    throw new InvalidDataException(
                        "spZone has an unexpected physical parent.");
                }
                result.Add(CreateZoneObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return result;
    }

    private static byte[] ReadZoneResource(ZoneFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
        {
            return File.ReadAllBytes(Path.Combine(location.SourceRoot,
                location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        }
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
        {
            throw new InvalidDataException("PCK zone occurrence is incomplete.");
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

    private static ZoneObservation CreateZoneObservation(
        ZoneFileLocation location,SmoDocument document,SmoObjectEntry entry,
        int ordinal,SmoZoneData decoded)
    {
        IReadOnlyList<SmoObjectField> direct = SmoObjectFieldReader.Read(document,entry);
        Dictionary<uint,SmoObjectEntry> objectsById = document.Objects
            .GroupBy(candidate => candidate.Id).Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key,group => group.Single());
        var fields = new List<ZoneFieldAnnotation>();
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
                throw new InvalidDataException("Unregistered spZone direct field.");
            }

            if (descriptor.Key is "node.position" or "node.rotation")
            {
                int count = descriptor.Key == "node.position" ? 3 : 4;
                float[] values = Enumerable.Range(0,count)
                    .Select(component => ZoneReadSingle(
                        field.Payload.Span,component * sizeof(float))).ToArray();
                fields.Add(new ZoneFieldAnnotation(
                    index,descriptor.Key,descriptor.PayloadLayout,
                    JsonSerializer.Serialize(values)));
                signature.Add($"{descriptor.Key}:" +
                              Convert.ToHexString(field.Payload.Span));
                continue;
            }
            if (descriptor.Key is "node.is_static" or "node.is_animated")
            {
                bool value = field.PayloadSize == 1 && field.Payload.Span[0] == 1;
                fields.Add(new ZoneFieldAnnotation(
                    index,descriptor.Key,"Boolean",JsonSerializer.Serialize(value)));
                signature.Add($"{descriptor.Key}:{value}");
                continue;
            }
            if (descriptor.Key != "zone.local_partition_root" ||
                !SmoNodeDecoder.TryDecodeRelationship(
                    field.Payload.Span,objectsById,
                    out SmoNodeRelationship? relationship) || relationship is null)
            {
                throw new InvalidDataException(
                    "Invalid spZone local-root relationship during annotation.");
            }
            string targetName = relationship.TargetName?.TrimEnd('\0') ?? string.Empty;
            fields.Add(new ZoneFieldAnnotation(
                index,descriptor.Key,
                "owned inline spPartitionNode or spOctreeNode relationship",
                JsonSerializer.Serialize(new
                {
                    relationship.ObjectId,Encoding="inline_object",
                    relationship.InlineSerializedSize,
                    relationship.TargetObjectIndex,
                    TargetTypeHash=relationship.TargetTypeHash.HasValue
                        ? $"0x{relationship.TargetTypeHash.Value:X8}" : null,
                    TargetClass=relationship.TargetTypeHash.HasValue
                        ? SmoClassRegistry.GetDisplayName(
                            relationship.TargetTypeHash.Value) : null,
                    TargetName=targetName
                })));
            signature.Add($"{descriptor.Key}:" +
                          $"{relationship.TargetTypeHash:X8}:" +
                          targetName.ToLowerInvariant());
        }
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        return new ZoneObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            decoded,serializedHash,string.Join("|",signature),fields.AsReadOnly());
    }

    private static void RequireZoneProfiles(
        IReadOnlyList<ZoneObservation> observations)
    {
        var expected = new Dictionary<string,(int Objects,int Roots,
            int PartitionRoots,int OctreeRoots,int Rootless)>
        {
            ["pc-working"]=(123,137,126,11,1),
            ["pc-pristine"]=(123,137,126,11,1),
            ["ps2-pristine"]=(123,138,126,12,0)
        };
        var actual = observations.GroupBy(item => item.CorpusKey).ToDictionary(
            group => group.Key,group => (
                Objects:group.Count(),
                Roots:group.Sum(item => item.Data.LocalPartitionRoots.Count),
                PartitionRoots:group.SelectMany(
                        item => item.Data.LocalPartitionRoots)
                    .Count(item => item.TargetTypeHash == SmoClassIds.PartitionNode),
                OctreeRoots:group.SelectMany(item => item.Data.LocalPartitionRoots)
                    .Count(item => item.TargetTypeHash == SmoClassIds.OctreeNode),
                Rootless:group.Count(item =>
                    item.Data.LocalPartitionRoots.Count == 0)));
        if (actual.Count != expected.Count || expected.Any(item =>
                !actual.TryGetValue(item.Key,out var value) || value != item.Value) ||
            observations.Any(item => item.Data.Node.IsAnimated ||
                item.Data.Node.Children.Count != 0 ||
                item.Data.Node.Collisions.Count != 0 ||
                item.Data.LocalPartitionRoots.Count > 4))
        {
            throw new InvalidDataException(
                "spZone profile changed: " + JsonSerializer.Serialize(actual));
        }
    }

    private static ZoneComparison CompareZones(
        IEnumerable<ZoneObservation> observations,string leftCorpus,
        string rightCorpus)
    {
        Dictionary<(string Path,int Ordinal),ZoneObservation> Build(string corpus) =>
            observations.Where(item => item.CorpusKey == corpus)
                .ToDictionary(item => (item.CanonicalPath,item.Ordinal));
        var left = Build(leftCorpus);
        var right = Build(rightCorpus);
        var keys = left.Keys.Intersect(right.Keys).ToArray();
        return new ZoneComparison(
            keys.Select(item => item.Path).Distinct().Count(),keys.Length,
            keys.Count(key => left[key].SemanticSignature ==
                              right[key].SemanticSignature),
            keys.Count(key => left[key].SerializedSha256 ==
                              right[key].SerializedSha256));
    }

    private static void UpsertZoneFieldDefinitions(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<ZoneObservation> observations)
    {
        (int Section,int Type,int Occurrence,string Semantic,string Display,
            string Kind,string Layout,bool Repeated,string Evidence,string Notes)[] defs =
        [
            (1,0,0,"node.position","Local position","vector3","three little-endian Single values",false,"confirmed_both_executables_and_full_corpus","Required; 123/123 objects per corpus."),
            (1,1,0,"node.rotation","Local rotation","quaternion","four little-endian Single values in X/Y/Z/W order",false,"confirmed_both_executables_and_full_corpus","Optional; the same two zones per corpus store one quaternion."),
            (1,2,0,"node.scale","Local scale","vector3","three little-endian Single values",false,"confirmed_both_executables_not_observed_corpus","Inherited default (1,1,1)."),
            (1,3,0,"node.is_bone","Bone node","boolean","one byte 0 or 1",false,"confirmed_both_executables_not_observed_corpus","Inherited false default."),
            (1,4,0,"node.is_static","Static node","boolean","one byte 0 or 1",false,"confirmed_both_executables_and_full_corpus","Optional; 25/123 objects per corpus explicitly store true."),
            (1,5,-1,"node.child","Child node","object_relationship","node child relationship",true,"confirmed_both_executables_not_observed_corpus","No zone uses inherited node children."),
            (1,6,0,"node.billboard_axis","Billboard axis","uint32_enum","UInt32 axis 1 or 2",false,"confirmed_both_executables_not_observed_corpus","Inherited default 0."),
            (1,7,-1,"node.collision","Collision info","object_relationship","node collision relationship",true,"confirmed_both_executables_not_observed_corpus","No zone uses inherited collision links."),
            (1,8,0,"node.is_animated","Animated node","boolean","one byte 0 or 1",false,"confirmed_both_executables_and_full_corpus","Required and always false."),
            (0,0,-1,"zone.local_partition_root","Local partition root","object_relationship","owned inline spPartitionNode or spOctreeNode relationship",true,"confirmed_both_executables_and_full_corpus","Repeated 0..4; concrete root class is polymorphic.")
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

    private static void AnnotateZoneFields(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<ZoneObservation> observations)
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
        foreach (ZoneObservation observation in observations)
        foreach (ZoneFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException("Could not annotate spZone field.");
        }
    }

    private static int InsertZoneVariant(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IEnumerable<ZoneObservation> observations,string now)
    {
        Dictionary<string,int> corpusCounts = observations
            .GroupBy(item => item.CorpusKey)
            .ToDictionary(group => group.Key,group => group.Count());
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,status,
                discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2',$key,
                   'Node zone with polymorphic local partition roots',
                   'confirmed',$discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$key",ZoneVariantKey);
        command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
        {
            corpusCounts,
            sections=new[] { "spNode","spZone" },
            nodeFieldOrder=new[] { "position","rotation?","is_static?",
                "is_animated","terminator" },
            ownFieldOrder=new[] { "local_partition_root[]","terminator" },
            rootClasses=new[] { "spPartitionNode","spOctreeNode" },
            observedRootCount=new { minimum=0,maximum=4 }
        }));
        command.Parameters.AddWithValue("$notes",
            "One serializer layout. Raw sizes vary with 0..4 inline local trees; " +
            "root class and multiplicity are data, not separate serializer variants.");
        command.Parameters.AddWithValue("$utc",now);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void AssignZoneVariant(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<ZoneObservation> observations,int variantId)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spZone decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.AddWithValue("$variant",variantId);
        foreach (ZoneObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.ExecuteNonQuery();
        }
    }

    private static float ZoneReadSingle(ReadOnlySpan<byte> source,int offset) =>
        BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(
            source.Slice(offset,sizeof(float))));

    private sealed record ZoneFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record ZoneFieldAnnotation(
        int FieldIndex,string Semantic,string Layout,string DecodedJson);
    private sealed record ZoneObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string CanonicalPath,
        SmoZoneData Data,string SerializedSha256,string SemanticSignature,
        IReadOnlyList<ZoneFieldAnnotation> Fields);
    private sealed record ZoneComparison(
        int CommonResources,int PairedObjects,int EqualSemantic,int EqualBytes);
}
