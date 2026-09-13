using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private const string BspNodeVariantKey = "bsp_node_common_binary_split";

    private static SmoResearchClassAnalysisResult AnalyzeBspNode(
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
            !parents.SetEquals([SmoClassIds.BspNode,SmoClassIds.PartitionSystem]) ||
            !children.SetEquals([SmoClassIds.BspNode]))
        {
            throw new InvalidDataException(
                "spBSPNode corpus no longer matches its physical hierarchy.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly:false);
        List<BspNodeObservation> observations = LoadBspNodeObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        long fieldCount = observations.Sum(item => (long)item.Fields.Count);
        long relationshipCount = observations.Count * 5L;
        if (objectCount != 324 || observations.Count != objectCount ||
            fieldCount != 2_268 || relationshipCount != 1_620)
        {
            throw new InvalidDataException(
                $"Expected 324 BSP nodes, 2268 semantic fields and 1620 " +
                $"relationship fields; got {observations.Count}, {fieldCount} " +
                $"and {relationshipCount}.");
        }
        RequireBspNodeProfiles(observations);
        BspForestStatistics forest = ValidateBspForests(observations);
        BspNodeComparison pc = CompareBspNodes(
            observations,"pc-working","pc-pristine");
        BspNodeComparison cross = CompareBspNodes(
            observations,"pc-pristine","ps2-pristine");
        if (pc.CommonResources != 18 || pc.PairedObjects != 108 ||
            pc.EqualSemantic != 108 || pc.EqualPlanes != 108 ||
            cross.CommonResources != 18 || cross.PairedObjects != 108 ||
            cross.EqualSemantic != 108 || cross.EqualPlanes != 108)
        {
            throw new InvalidDataException(
                "spBSPNode PC-copy or PC/PS2 pairing changed: " +
                JsonSerializer.Serialize(new { pc,cross }));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spBSPNode","spBSPNodeSerializer","ebspnsfBSPNodePlane",
            "ebspnsfBSPNodePolygon","epnsfPartitionNodeChild"
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
            clearVariants.Parameters.AddWithValue("$key",BspNodeVariantKey);
            clearVariants.ExecuteNonQuery();
        }
        using (SqliteCommand clearPreliminaryDefinitions =
               CreateCommand(connection,transaction,"""
                   DELETE FROM field_definitions
                   WHERE type_hash=$hash AND scope_kind='platform'
                     AND semantic_key IN ('bsp.plane','bsp.polygon');
                   """))
        {
            clearPreliminaryDefinitions.Parameters.AddWithValue(
                "$hash",(long)report.TypeHash);
            clearPreliminaryDefinitions.ExecuteNonQuery();
        }

        UpsertBspNodeFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateBspNodeFields(connection,transaction,observations);
        int variantId = InsertBspNodeVariant(
            connection,transaction,report.TypeHash,observations,now);
        AssignBspNodeVariant(connection,transaction,observations,variantId);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "reader VA 0x0044CEB0..0x0044D20F; writer VA 0x0044D220..0x0044D602",
            "The PC serializer calls the spPartitionNode serializer, then maps " +
            "required Plane field 0 to runtime offsets +0x84/+0x90. Optional " +
            "Polygon field 1 uses +0xA4/+0xA8 and is emitted only for a nonzero count.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "reader VA 0x0019F8D0..0x0019FBA8; writer VA 0x0019FBC0..0x0019FEA0",
            "The independent PS2 serializer confirms required Plane field 0 at " +
            "runtime offsets +0x70/+0x7C and optional Polygon field 1 at +0x90/+0x94.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload, complete decode and recursive topology validation",
            $"{objectCount} unnamed objects form {forest.TreeCount} full binary " +
            $"trees: {forest.InlineBspEdges} inline split edges and " +
            $"{forest.TerminalPartitionEdges} referenced spPartitionNode leaves. " +
            "Every node has slots 0 and 1, a unit plane normal, no Polygon and " +
            $"serialized subtree size exactly 101 bytes per BSP split. All {fieldCount} " +
            "present fields were annotated.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus spBSPNode ordinal",
            $"PC working/pristine: {pc.EqualBytes}/{pc.PairedObjects} byte-identical " +
            $"objects and {pc.EqualSemantic} semantic matches. PC/PS2: " +
            $"{cross.EqualBytes}/{cross.PairedObjects} byte-identical objects and " +
            $"{cross.EqualSemantic} semantic matches, including topology, child " +
            "encodings and plane values.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='spatial_bsp_split_node',
                       description='Binary spatial partition split with a plane and two indexed child branches',
                       decode_status='read_only_decode',
                       notes='Strict PC/PS2 decode: inherited spPartitionNode section plus required Plane and optional Polygon. Corpus trees use slots 0/1 and omit Polygon; slot-side semantics remain unknown.'
                   WHERE type_hash=$hash;
                   """))
        {
            updateClass.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            updateClass.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);

        Dictionary<string,int> axes = observations.GroupBy(item =>
                ClassifyBspAxis(item.Data.Plane!.Normal))
            .ToDictionary(group => group.Key,group => group.Count());
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,objectCount,
            report.Profiles.Sum(item => item.UniqueResourceCount),1,
            observations.Count,evidenceRows,
            $"One common recursive BSP layout decoded across {objectCount} objects " +
            $"and {forest.TreeCount} trees. PC/PS2 semantics match for all " +
            $"{cross.PairedObjects} paired nodes. Plane axes: " +
            string.Join(", ",axes.OrderBy(item => item.Key)
                .Select(item => $"{item.Key}={item.Value}")) + ".");
    }

    private static List<BspNodeObservation> LoadBspNodeObservations(
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.BspNode);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<BspNodeFileLocation>();
        while (reader.Read())
        {
            locations.Add(new BspNodeFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<BspNodeObservation>();
        foreach (BspNodeFileLocation location in locations)
        {
            byte[] bytes = ReadBspNodeResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.BspNode))
            {
                if (!SmoBspNodeDecoder.TryDecode(
                        document,entry,out SmoBspNodeData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                if (entry.ParentIndex is not int parentIndex ||
                    document.Objects[parentIndex].TypeHash is not
                        SmoClassIds.BspNode and not SmoClassIds.PartitionSystem)
                {
                    throw new InvalidDataException(
                        "spBSPNode has an unexpected physical parent.");
                }
                result.Add(CreateBspNodeObservation(
                    location,document,entry,ordinal++,decoded,parentIndex,
                    document.Objects[parentIndex].TypeHash));
            }
        }
        return result;
    }

    private static byte[] ReadBspNodeResource(BspNodeFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
        {
            return File.ReadAllBytes(Path.Combine(location.SourceRoot,
                location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        }
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
            throw new InvalidDataException("PCK BSP-node occurrence is incomplete.");
        string archive = Path.Combine(location.SourceRoot,
            location.ContainerPath.Replace('/',Path.DirectorySeparatorChar));
        using var stream = new FileStream(
            archive,FileMode.Open,FileAccess.Read,FileShare.Read);
        stream.Position = location.ByteOffset.Value;
        byte[] data = new byte[checked((int)location.ByteSize)];
        stream.ReadExactly(data);
        return data;
    }

    private static BspNodeObservation CreateBspNodeObservation(
        BspNodeFileLocation location,SmoDocument document,SmoObjectEntry entry,
        int ordinal,SmoBspNodeData decoded,int parentIndex,uint parentTypeHash)
    {
        IReadOnlyList<SmoObjectField> direct = SmoObjectFieldReader.Read(document,entry);
        SmoBspPlane plane = decoded.Plane ?? throw new InvalidDataException(
            "Historical BSP profile requires an authored plane; the loaded constructor state has none.");
        if (direct.Count != 9)
            throw new InvalidDataException(
                "Corpus spBSPNode unexpectedly contains its optional Polygon field.");
        var fields = new List<BspNodeFieldAnnotation>(7)
        {
            new(0,"partition_node.debug_color","ARGB UInt32",
                JsonSerializer.Serialize(new
                {
                    Argb=$"0x{decoded.PartitionNode.DebugColorArgb:X8}"
                }))
        };
        Dictionary<uint,SmoObjectEntry> objectsById = document.Objects
            .GroupBy(candidate => candidate.Id).Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key,group => group.Single());
        var signatureRelationships = new List<string>(4);
        var wireChildren = new List<SpatialWireChild>();
        for (int index=1;index<=5;index++)
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
                throw new InvalidDataException("Invalid BSP relationship field.");
            string encoding = GetPartitionNodeEncodingName(relationship.Encoding);
            if (slot is uint childSlot)
                wireChildren.Add(new(childSlot,relationship));
            string targetName = relationship.TargetName?.TrimEnd('\0') ?? string.Empty;
            signatureRelationships.Add(
                $"{field.FieldType}:{slot}:{encoding}:{relationship.TargetTypeHash:X8}:" +
                $"{relationship.InlineSerializedSize}");
            (string semantic,string layout) = field.FieldType switch
            {
                2 => ("partition_node.child",
                    "UInt32 branch slot followed by inline BSP or referenced partition-node relationship"),
                3 => ("partition_node.zone","nullable ID-only relationship to spZone"),
                5 => ("partition_node.partition_system",
                    "sized-reference relationship to spPartitionSystem"),
                6 => ("partition_node.partition_renderable","null object ID"),
                _ => throw new InvalidDataException("Unexpected BSP base field.")
            };
            fields.Add(new BspNodeFieldAnnotation(
                index,semantic,layout,JsonSerializer.Serialize(new
                {
                    Slot=slot,relationship.ObjectId,Encoding=encoding,
                    relationship.InlineSerializedSize,relationship.TargetObjectIndex,
                    TargetTypeHash=relationship.TargetTypeHash.HasValue
                        ? $"0x{relationship.TargetTypeHash.Value:X8}" : null,
                    TargetName=targetName
                })));
        }
        fields.Add(new BspNodeFieldAnnotation(
            7,"bsp_node.plane","unit Vector3 normal followed by Single constant",
            JsonSerializer.Serialize(new
            {
                Normal=new
                {
                    plane.Normal.X,plane.Normal.Y,
                    plane.Normal.Z
                },
                plane.Constant,
                Axis=ClassifyBspAxis(plane.Normal)
            })));
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        string semanticSignature = JsonSerializer.Serialize(new
        {
            decoded.PartitionNode.DebugColorArgb,
            PartitionSystem=GetPartitionNodeEncodingName(
                ReadSpatialWireReference(document,entry,"partition_node.partition_system").Encoding),
            Zone=GetPartitionNodeEncodingName(
                ReadSpatialWireReference(document,entry,"partition_node.zone").Encoding),
            Relationships=signatureRelationships,
            plane.Normal,plane.Constant,Polygon=decoded.Polygon
        });
        return new BspNodeObservation(
            location.FileId,entry.Index,ordinal,entry.Id,parentIndex,parentTypeHash,
            location.CorpusKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            decoded,serializedHash,semanticSignature,fields.AsReadOnly(),
            entry.SerializedSize,wireChildren.AsReadOnly());
    }

    private static void RequireBspNodeProfiles(
        IReadOnlyList<BspNodeObservation> observations)
    {
        if (observations.Any(item =>
                item.Data.PartitionNode.DebugColorArgb != 0xFFFFFFFFu ||
                item.Data.Polygon.Count != 0 ||
                item.Data.PartitionNode.Children.Count != 2 ||
                item.Data.PartitionNode.Zone.ObjectId != 0 ||
                item.Data.PartitionNode.PartitionRenderable.ObjectId != 0))
        {
            throw new InvalidDataException(
                "spBSPNode fixed values or child cardinality changed.");
        }
        var expected = new Dictionary<string,(int Objects,long Minimum,long Maximum,
            int InlineBspChildren,int PartitionLeaves)>
        {
            ["pc-working"]=(108,101,1_616,90,126),
            ["pc-pristine"]=(108,101,1_616,90,126),
            ["ps2-pristine"]=(108,101,1_616,90,126)
        };
        var actual = observations.GroupBy(item => item.CorpusKey).ToDictionary(
            group => group.Key,group =>
            {
                var relations = group.SelectMany(item =>
                    item.WireChildren.Select(child => child.Relationship));
                return (
                    Objects:group.Count(),Minimum:group.Min(item => item.SerializedSize),
                    Maximum:group.Max(item => item.SerializedSize),
                    InlineBspChildren:relations.Count(item =>
                        item.TargetTypeHash == SmoClassIds.BspNode &&
                        item.Encoding == SmoNodeRelationshipEncoding.InlineObject),
                    PartitionLeaves:relations.Count(item =>
                        item.TargetTypeHash == SmoClassIds.PartitionNode &&
                        item.Encoding == SmoNodeRelationshipEncoding.SizedReference));
            });
        if (actual.Count != expected.Count || expected.Any(item =>
                !actual.TryGetValue(item.Key,out var value) || value != item.Value))
        {
            throw new InvalidDataException(
                "spBSPNode profile changed: " + JsonSerializer.Serialize(actual));
        }
    }

    private static BspForestStatistics ValidateBspForests(
        IReadOnlyList<BspNodeObservation> observations)
    {
        int trees = 0;
        int inlineEdges = 0;
        int terminalEdges = 0;
        int maximumDepth = 0;
        foreach (IGrouping<(string Corpus,string Path),BspNodeObservation> file in
                 observations.GroupBy(item => (item.CorpusKey,item.CanonicalPath)))
        {
            Dictionary<int,BspNodeObservation> byIndex =
                file.ToDictionary(item => item.ObjectIndex);
            BspNodeObservation[] roots = file.Where(item =>
                item.ParentTypeHash == SmoClassIds.PartitionSystem).ToArray();
            if (roots.Length != 1)
                throw new InvalidDataException("A BSP resource must have exactly one root.");
            var seen = new HashSet<int>();
            int fileTerminals = 0;
            (int Nodes,int Depth) Visit(BspNodeObservation node,int depth)
            {
                if (!seen.Add(node.ObjectIndex))
                    throw new InvalidDataException("BSP tree contains a cycle/shared split.");
                int nodes = 1;
                int deepest = depth;
                foreach (SpatialWireChild child in node.WireChildren)
                {
                    SmoNodeRelationship relationship = child.Relationship;
                    if (relationship.TargetTypeHash == SmoClassIds.PartitionNode)
                    {
                        if (relationship.Encoding !=
                                SmoNodeRelationshipEncoding.SizedReference ||
                            relationship.InlineSerializedSize != 0)
                            throw new InvalidDataException("Invalid BSP terminal leaf.");
                        fileTerminals++;
                        terminalEdges++;
                        continue;
                    }
                    if (relationship.TargetTypeHash != SmoClassIds.BspNode ||
                        relationship.Encoding != SmoNodeRelationshipEncoding.InlineObject ||
                        relationship.TargetObjectIndex is not int childIndex ||
                        !byIndex.TryGetValue(childIndex,out BspNodeObservation? nested) ||
                        nested.ParentIndex != node.ObjectIndex ||
                        nested.SerializedSize != relationship.InlineSerializedSize)
                    {
                        throw new InvalidDataException("Invalid inline BSP child.");
                    }
                    inlineEdges++;
                    (int childNodes,int childDepth) = Visit(nested,depth + 1);
                    nodes += childNodes;
                    deepest = Math.Max(deepest,childDepth);
                }
                if (node.SerializedSize != nodes * 101L)
                    throw new InvalidDataException(
                        "BSP subtree size is not 101 bytes per split node.");
                return (nodes,deepest);
            }
            (int nodeCount,int depth) = Visit(roots[0],0);
            if (nodeCount != file.Count() || seen.Count != file.Count() ||
                fileTerminals != nodeCount + 1)
                throw new InvalidDataException("BSP tree is not a complete binary tree.");
            trees++;
            maximumDepth = Math.Max(maximumDepth,depth);
        }
        if (trees != 54 || inlineEdges != 270 || terminalEdges != 378 ||
            maximumDepth != 6)
        {
            throw new InvalidDataException(
                "BSP forest totals changed: " + JsonSerializer.Serialize(new
                {
                    trees,inlineEdges,terminalEdges,maximumDepth
                }));
        }
        return new BspForestStatistics(
            trees,inlineEdges,terminalEdges,maximumDepth);
    }

    private static BspNodeComparison CompareBspNodes(
        IEnumerable<BspNodeObservation> observations,string leftCorpus,
        string rightCorpus)
    {
        Dictionary<(string Path,int Ordinal),BspNodeObservation> Build(string corpus) =>
            observations.Where(item => item.CorpusKey == corpus)
                .ToDictionary(item => (item.CanonicalPath,item.Ordinal));
        var left=Build(leftCorpus);
        var right=Build(rightCorpus);
        var keys=left.Keys.Intersect(right.Keys).ToArray();
        return new BspNodeComparison(
            keys.Select(item => item.Path).Distinct().Count(),keys.Length,
            keys.Count(key => left[key].SerializedSha256 == right[key].SerializedSha256),
            keys.Count(key => left[key].SemanticSignature == right[key].SemanticSignature),
            keys.Count(key => left[key].Data.Plane == right[key].Data.Plane));
    }

    private static string ClassifyBspAxis(Vector3 normal)
    {
        (string Name,Vector3 Axis)[] axes =
        [
            ("+X",Vector3.UnitX),("-X",-Vector3.UnitX),
            ("+Y",Vector3.UnitY),("-Y",-Vector3.UnitY),
            ("+Z",Vector3.UnitZ),("-Z",-Vector3.UnitZ)
        ];
        foreach ((string name,Vector3 axis) in axes)
        {
            if (Vector3.DistanceSquared(normal,axis) <= 0.0000000001f)
                return name;
        }
        return "oblique";
    }

    private static void UpsertBspNodeFieldDefinitions(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<BspNodeObservation> observations)
    {
        (int Section,int Type,int Occurrence,string Semantic,string Display,
            string Kind,string Layout,bool Repeated,string Evidence,string Notes)[] defs =
        [
            (1,0,-1,"partition_node.collision_info","Collision info","object_relationship",
                "spCollisionInfo relationship",true,"confirmed_both_executables_not_observed_corpus","Inherited; absent from BSP nodes."),
            (1,1,0,"partition_node.debug_color","Debug color","argb32",
                "ARGB UInt32 stored little-endian",false,"confirmed_both_executables_and_full_corpus","Inherited; always 0xFFFFFFFF."),
            (1,2,-1,"partition_node.child","Indexed BSP branch","indexed_object_relationship",
                "UInt32 slot plus inline BSP or referenced partition-node relationship",true,"confirmed_both_executables_and_full_corpus","Exactly two fields with slots 0 and 1; geometric side names remain unknown."),
            (1,3,0,"partition_node.zone","Zone","object_relationship",
                "nullable ID-only relationship to spZone",false,"confirmed_both_executables_and_full_corpus","Inherited; BSP uses ID-only encoding unlike concrete/octree nodes and all corpus values are null."),
            (1,4,-1,"partition_node.zone_portal","Zone portal","object_relationship",
                "inline spZonePortal relationship",true,"confirmed_both_executables_not_observed_corpus","Inherited; absent from BSP nodes."),
            (1,5,0,"partition_node.partition_system","Partition system","object_relationship",
                "sized-reference relationship to spPartitionSystem",false,"confirmed_both_executables_and_full_corpus","Inherited; required."),
            (1,6,0,"partition_node.partition_renderable","Partition renderable","object_relationship",
                "null object ID",false,"confirmed_both_executables_and_full_corpus","Inherited; always null in BSP nodes."),
            (1,7,-1,"partition_node.static_render_object","Static render object","object_relationship",
                "spStaticRenderObject relationship",true,"confirmed_both_executables_not_observed_corpus","Inherited; absent from BSP nodes."),
            (0,0,0,"bsp_node.plane","Split plane","plane4",
                "unit Vector3 normal followed by Single constant",false,"confirmed_both_executables_and_full_corpus","Required; all corpus normals are finite and unit length."),
            (0,1,0,"bsp_node.polygon","Split polygon","vector3_array",
                "UInt32 count followed by Vector3[count]",false,"confirmed_both_executables_not_observed_corpus","Optional executable-supported field; absent from all 324 corpus nodes.")
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

    private static void AnnotateBspNodeFields(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<BspNodeObservation> observations)
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
        foreach (BspNodeObservation observation in observations)
        foreach (BspNodeFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value=field.Semantic;
            command.Parameters["$layout"].Value=field.Layout;
            command.Parameters["$value"].Value=field.DecodedJson;
            command.Parameters["$file"].Value=observation.FileId;
            command.Parameters["$object"].Value=observation.ObjectIndex;
            command.Parameters["$field"].Value=field.FieldIndex;
            if (command.ExecuteNonQuery()!=1)
                throw new InvalidDataException("Could not annotate spBSPNode field.");
        }
    }

    private static int InsertBspNodeVariant(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IEnumerable<BspNodeObservation> observations,string now)
    {
        Dictionary<string,int> corpusCounts=observations.GroupBy(item=>item.CorpusKey)
            .ToDictionary(group=>group.Key,group=>group.Count());
        using SqliteCommand command=CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,status,
                discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2',$key,'Common recursive BSP split layout',
                   'confirmed',$discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$key",BspNodeVariantKey);
        command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
        {
            corpusCounts,
            inheritedFieldOrder=new[] { "debug_color","partition_system","zone",
                "child[0]","child[1]","null_partition_renderable","terminator" },
            ownFieldOrder=new[] { "plane","optional_polygon","terminator" },
            subtreeSizeFormula="101 * BSP split count"
        }));
        command.Parameters.AddWithValue("$notes",
            "The sole normalized layout. Twenty raw field shapes arise only from " +
            "different inline subtree sizes; Polygon is executable-supported but " +
            "unobserved. Slots intentionally remain numbered rather than named by side.");
        command.Parameters.AddWithValue("$utc",now);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void AssignBspNodeVariant(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<BspNodeObservation> observations,int variantId)
    {
        using SqliteCommand command=CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spBSPNode decoder and tree validator')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.AddWithValue("$variant",variantId);
        foreach (BspNodeObservation observation in observations)
        {
            command.Parameters["$file"].Value=observation.FileId;
            command.Parameters["$object"].Value=observation.ObjectIndex;
            command.ExecuteNonQuery();
        }
    }

    private sealed record BspNodeFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record BspNodeFieldAnnotation(
        int FieldIndex,string Semantic,string Layout,string DecodedJson);
    private sealed record BspNodeObservation(
        int FileId,int ObjectIndex,int Ordinal,uint ObjectId,int ParentIndex,
        uint ParentTypeHash,string CorpusKey,string CanonicalPath,SmoBspNodeData Data,
        string SerializedSha256,string SemanticSignature,
        IReadOnlyList<BspNodeFieldAnnotation> Fields,long SerializedSize,
        IReadOnlyList<SpatialWireChild> WireChildren);
    private sealed record SpatialWireChild(uint SlotIndex,SmoNodeRelationship Relationship);
    private sealed record BspForestStatistics(
        int TreeCount,int InlineBspEdges,int TerminalPartitionEdges,int MaximumDepth);
    private sealed record BspNodeComparison(
        int CommonResources,int PairedObjects,int EqualBytes,int EqualSemantic,
        int EqualPlanes);
}
