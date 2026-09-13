using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private const string ZonePortalNodeVariantKey =
        "node_with_ordered_bidirectional_zone_portal_pair";

    private static SmoResearchClassAnalysisResult AnalyzeZonePortalNode(
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
            !parents.SetEquals([SmoClassIds.PartitionSystem]) || children.Count != 0)
        {
            throw new InvalidDataException(
                "spZonePortalNode corpus no longer matches its physical hierarchy.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly:false);
        List<ZonePortalNodeObservation> observations =
            LoadZonePortalNodeObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        long fieldCount = observations.Sum(item => (long)item.Fields.Count);
        long relationshipCount = observations.Sum(
            item => (long)item.Data.ZonePortals.Count);
        if (objectCount != 309 || observations.Count != objectCount ||
            fieldCount != 1_305 || relationshipCount != 618)
        {
            throw new InvalidDataException(
                $"Expected 309 portal nodes, 1305 semantic fields and 618 " +
                $"portal references; got {observations.Count}, {fieldCount} " +
                $"and {relationshipCount}.");
        }
        RequireZonePortalNodeProfiles(observations);
        ZonePortalNodeComparison pc = CompareZonePortalNodes(
            observations,"pc-working","pc-pristine");
        ZonePortalNodeComparison cross = CompareZonePortalNodes(
            observations,"pc-pristine","ps2-pristine");
        if (pc != new ZonePortalNodeComparison(18,103,103,103) ||
            cross.CommonResources != 18 || cross.PairedObjects != 103 ||
            cross.EqualSemantic != 103)
        {
            throw new InvalidDataException(
                "spZonePortalNode PC-copy or PC/PS2 pairing changed: " +
                JsonSerializer.Serialize(new { pc,cross }));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spZonePortalNodeSerializer","ezpnsfZonePortalNodeZonePortal",
            "GetZonePortal( i )","No empty spZonePortalNode->spZonePortal"
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
            clearVariants.Parameters.AddWithValue("$key",ZonePortalNodeVariantKey);
            clearVariants.ExecuteNonQuery();
        }

        UpsertZonePortalNodeFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateZonePortalNodeFields(connection,transaction,observations);
        int variantId = InsertZonePortalNodeVariant(
            connection,transaction,report.TypeHash,observations,now);
        AssignZonePortalNodeVariant(
            connection,transaction,observations,variantId);

        int positionOnly = observations.Count(item =>
            !item.Data.Node.IsFieldSerialized(1) &&
            !item.Data.Node.IsFieldSerialized(4));
        int rotated = observations.Count(item =>
            item.Data.Node.IsFieldSerialized(1));
        int explicitlyStatic = observations.Count(item =>
            item.Data.Node.IsFieldSerialized(4));
        int centroidPositions = observations.Count(item =>
            item.PositionMatchesPolygonCentroid);
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "index/reader/writer VA 0x0044E5F0..0x0044EB52",
            "The x86 serializer delegates its first section to spNodeSerializer " +
            "and repeats only ezpnsfZonePortalNodeZonePortal in the second. " +
            "The runtime portal vector begins/ends at +0xB8/+0xBC; null links " +
            "are rejected.",pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "reader/index/writer VA 0x001A2CC0..0x001A3100",
            "The independent MIPS serializer has the same repeated field-0 " +
            "contract, creates class 0x6523AC37 (spZonePortal), and stores its " +
            "runtime array/count at +0xC0/+0xC4.",ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload and complete two-section decode",
            $"{objectCount} named portal nodes contain {relationshipCount} " +
            $"non-null sized references and {fieldCount} annotated fields. " +
            $"Inherited wire shapes are Position-only {positionOnly}, optional " +
            $"Rotation {rotated}, and explicit Static=true {explicitlyStatic}; " +
            "Animated is always false.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:graph_and_cross_corpus","smo-corpus-v2.sqlite",
            "ordered portal targets plus source/destination and polygon checks",
            $"All {objectCount} nodes order BackToFront before FrontToBack. " +
            "Every pair exchanges source/destination zones, is open, and uses " +
            $"exactly reversed polygon winding. Position is within 0.001 of the " +
            $"polygon centroid for only {centroidPositions}/{objectCount}, so it " +
            $"is independent inherited placement data. PC copies match " +
            $"{pc.EqualBytes}/{pc.PairedObjects} byte-for-byte; PC/PS2 semantic " +
            $"pairs match {cross.EqualSemantic}/{cross.PairedObjects} and raw " +
            $"objects match {cross.EqualBytes}/{cross.PairedObjects}; the other " +
            "15 differ only in the two serialized reference object IDs.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='spatial_portal_pair',
                       description='Placed spNode that orders the BackToFront and FrontToBack spZonePortal edges of one bidirectional zone boundary',
                       decode_status='read_only_decode',
                       notes='PC and PS2 share inherited Position/optional Rotation or Static/Animated=false followed by two sized spZonePortal references. All corpus objects order BackToFront then FrontToBack.'
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
            $"One two-section portal-pair layout across {objectCount} objects " +
            $"and {relationshipCount} ordered references. All pairs are " +
            "bidirectional with exact reverse winding; PC copies and all " +
            "PC/PS2 semantics match.");
    }

    private static List<ZonePortalNodeObservation> LoadZonePortalNodeObservations(
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.ZonePortalNode);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<ZonePortalNodeFileLocation>();
        while (reader.Read())
        {
            locations.Add(new ZonePortalNodeFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<ZonePortalNodeObservation>();
        foreach (ZonePortalNodeFileLocation location in locations)
        {
            byte[] bytes = ReadZonePortalNodeResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            var referencedPortals = new HashSet<int>();
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.ZonePortalNode))
            {
                if (!SmoZonePortalNodeDecoder.TryDecode(
                        document,entry,out SmoZonePortalNodeData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                if (entry.ParentIndex is not int parentIndex ||
                    document.Objects[parentIndex].TypeHash !=
                        SmoClassIds.PartitionSystem ||
                    !SmoPartitionSystemDecoder.TryDecode(
                        document,document.Objects[parentIndex],
                        out SmoPartitionSystemData? system,out _) || system is null ||
                    system.Node.Children.Count(item =>
                        item.TargetObjectIndex == entry.Index) != 1)
                {
                    throw new InvalidDataException(
                        "spZonePortalNode must be one owned child of spPartitionSystem.");
                }
                ZonePortalNodePair pair = BuildZonePortalNodePair(
                    document,entry,decoded);
                foreach (SmoLoadedReference portal in decoded.ZonePortals)
                {
                    if (portal.TargetObjectIndex is not int portalIndex ||
                        !referencedPortals.Add(portalIndex))
                    {
                        throw new InvalidDataException(
                            "A portal has missing or duplicate portal-node membership.");
                    }
                }
                result.Add(CreateZonePortalNodeObservation(
                    location,document,entry,ordinal++,decoded,pair));
            }
            int portalCount = document.Objects.Count(item =>
                item.TypeHash == SmoClassIds.ZonePortal);
            if (referencedPortals.Count != portalCount)
                throw new InvalidDataException(
                    "Portal-node references do not cover every spZonePortal once.");
        }
        return result;
    }

    private static ZonePortalNodePair BuildZonePortalNodePair(
        SmoDocument document,SmoObjectEntry entry,SmoZonePortalNodeData decoded)
    {
        SmoLoadedReference backRelationship = decoded.ZonePortals[0];
        SmoLoadedReference frontRelationship = decoded.ZonePortals[1];
        if (backRelationship.TargetObjectIndex is not int backIndex ||
            frontRelationship.TargetObjectIndex is not int frontIndex ||
            !backRelationship.TargetName!.EndsWith(
                "BackToFront",StringComparison.Ordinal) ||
            !frontRelationship.TargetName!.EndsWith(
                "FrontToBack",StringComparison.Ordinal))
        {
            throw new InvalidDataException(
                "spZonePortalNode must order BackToFront before FrontToBack.");
        }
        SmoObjectEntry backEntry = document.Objects[backIndex];
        SmoObjectEntry frontEntry = document.Objects[frontIndex];
        string backError = string.Empty;
        string frontError = string.Empty;
        if (!SmoZonePortalDecoder.TryDecode(
                document,backEntry,out SmoZonePortalData? back,out backError) ||
            back is null || !SmoZonePortalDecoder.TryDecode(
                document,frontEntry,out SmoZonePortalData? front,
                out frontError) || front is null)
        {
            throw new InvalidDataException(
                $"Could not decode portal-node members: {backError} {frontError}");
        }
        if (backEntry.ParentIndex is not int backParent ||
            frontEntry.ParentIndex is not int frontParent)
        {
            throw new InvalidDataException("Portal-node member lacks a source owner.");
        }
        SmoNodeRelationship backSource = GetZonePortalSource(
            document,document.Objects[backParent]);
        SmoNodeRelationship frontSource = GetZonePortalSource(
            document,document.Objects[frontParent]);
        if (backSource.ObjectId != front.DestinationZone.ObjectId ||
            frontSource.ObjectId != back.DestinationZone.ObjectId ||
            !back.IsOpen || !front.IsOpen ||
            ClassifyPortalPolygonPair(back.Polygon,front.Polygon) !=
                "exact_reverse")
        {
            throw new InvalidDataException(
                "spZonePortalNode members must be open reverse-winding " +
                "bidirectional zone edges.");
        }
        Vector3 centroid = Vector3.Zero;
        foreach (Vector3 vertex in back.Polygon)
            centroid += vertex;
        centroid /= back.Polygon.Count;
        bool positionMatches = Vector3.Distance(
            decoded.Node.Position,centroid) <= 0.001f;
        return new ZonePortalNodePair(
            backRelationship,frontRelationship,backSource,frontSource,
            back,front,positionMatches);
    }

    private static byte[] ReadZonePortalNodeResource(
        ZonePortalNodeFileLocation location)
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
                "PCK zone-portal-node occurrence is incomplete.");
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

    private static ZonePortalNodeObservation CreateZonePortalNodeObservation(
        ZonePortalNodeFileLocation location,SmoDocument document,
        SmoObjectEntry entry,int ordinal,SmoZonePortalNodeData decoded,
        ZonePortalNodePair pair)
    {
        IReadOnlyList<SmoObjectField> direct =
            SmoObjectFieldReader.Read(document,entry);
        var fields = new List<ZonePortalNodeFieldAnnotation>();
        var signature = new List<string>();
        int portalOrdinal = 0;
        for (int index = 0; index < direct.Count; index++)
        {
            SmoObjectField field = direct[index];
            if (field.FieldType == 0 && field.PayloadSize == 0)
                continue;
            if (!SmoSerializedFieldRegistry.TryDescribeField(
                    entry.TypeHash,direct,index,
                    out SmoSerializedFieldDescriptor? descriptor) ||
                descriptor is null)
            {
                throw new InvalidDataException(
                    "Unregistered spZonePortalNode direct field.");
            }
            if (descriptor.Key is "node.position" or "node.rotation")
            {
                int count = descriptor.Key == "node.position" ? 3 : 4;
                float[] values = Enumerable.Range(0,count)
                    .Select(component => ZonePortalNodeReadSingle(
                        field.Payload.Span,component * sizeof(float))).ToArray();
                fields.Add(new ZonePortalNodeFieldAnnotation(
                    index,descriptor.Key,descriptor.PayloadLayout,
                    JsonSerializer.Serialize(values)));
                signature.Add($"{descriptor.Key}:" +
                              Convert.ToHexString(field.Payload.Span));
                continue;
            }
            if (descriptor.Key is "node.is_static" or "node.is_animated")
            {
                bool flag = field.Payload.Span[0] != 0;
                fields.Add(new ZonePortalNodeFieldAnnotation(
                    index,descriptor.Key,"Boolean",JsonSerializer.Serialize(flag)));
                signature.Add($"{descriptor.Key}:{flag}");
                continue;
            }
            if (descriptor.Key != "zone_portal_node.zone_portal" ||
                portalOrdinal >= decoded.ZonePortals.Count)
            {
                throw new InvalidDataException(
                    "Unexpected spZonePortalNode semantic field.");
            }
            SmoNodeRelationship relationship = ReadSpatialWireReference(
                document,entry,"zone_portal_node.zone_portal",portalOrdinal);
            if (relationship.ObjectId != decoded.ZonePortals[portalOrdinal++].ObjectId)
                throw new InvalidDataException("Authored and loaded portal-node identities differ.");
            string direction = portalOrdinal == 1
                ? "back_to_front" : "front_to_back";
            fields.Add(new ZonePortalNodeFieldAnnotation(
                index,descriptor.Key,"non-null sized-reference to spZonePortal",
                JsonSerializer.Serialize(new
                {
                    relationship.ObjectId,Encoding=GetPartitionNodeEncodingName(relationship.Encoding),
                    relationship.InlineSerializedSize,
                    relationship.TargetObjectIndex,
                    TargetTypeHash=$"0x{SmoClassIds.ZonePortal:X8}",
                    TargetClass="spZonePortal",
                    TargetName=relationship.TargetName?.TrimEnd('\0') ?? string.Empty,
                    Direction=direction
                })));
            signature.Add($"{descriptor.Key}:{direction}:" +
                          relationship.TargetName!.TrimEnd('\0').ToLowerInvariant());
        }
        signature.Add(ZonePortalNodePortalSignature(
            pair.BackSource,pair.BackPortal));
        signature.Add(ZonePortalNodePortalSignature(
            pair.FrontSource,pair.FrontPortal));
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        return new ZonePortalNodeObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            entry.Name.TrimEnd('\0'),decoded,pair.PositionMatchesPolygonCentroid,
            serializedHash,string.Join("|",signature),fields.AsReadOnly());
    }

    private static string ZonePortalNodePortalSignature(
        SmoNodeRelationship source,SmoZonePortalData portal)
    {
        string polygon = string.Join(",",portal.Polygon.Select(vertex =>
            $"{BitConverter.SingleToInt32Bits(vertex.X):X8}" +
            $"{BitConverter.SingleToInt32Bits(vertex.Y):X8}" +
            $"{BitConverter.SingleToInt32Bits(vertex.Z):X8}"));
        return $"{source.TargetName?.TrimEnd('\0').ToLowerInvariant()}->" +
               $"{portal.DestinationZone.TargetName?.TrimEnd('\0').ToLowerInvariant()}" +
               $"|{polygon}|open={portal.IsOpen}";
    }

    private static void RequireZonePortalNodeProfiles(
        IReadOnlyList<ZonePortalNodeObservation> observations)
    {
        var expected = new Dictionary<string,(int Objects,int PositionOnly,
            int Rotated,int Static,int Centered,int DistinctPositions)>
        {
            ["pc-working"]=(103,80,1,22,69,65),
            ["pc-pristine"]=(103,80,1,22,69,65),
            ["ps2-pristine"]=(103,80,1,22,69,65)
        };
        var actual = observations.GroupBy(item => item.CorpusKey).ToDictionary(
            group => group.Key,group => (
                Objects:group.Count(),
                PositionOnly:group.Count(item =>
                    !item.Data.Node.IsFieldSerialized(1) &&
                    !item.Data.Node.IsFieldSerialized(4)),
                Rotated:group.Count(item =>
                    item.Data.Node.IsFieldSerialized(1)),
                Static:group.Count(item =>
                    item.Data.Node.IsFieldSerialized(4)),
                Centered:group.Count(item =>
                    item.PositionMatchesPolygonCentroid),
                DistinctPositions:group.Select(item => item.Data.Node.Position)
                    .Distinct().Count()));
        if (actual.Count != expected.Count || expected.Any(item =>
                !actual.TryGetValue(item.Key,out var value) || value != item.Value) ||
            observations.Any(item => item.Data.ZonePortals.Count != 2 ||
                item.Data.Node.IsAnimated || item.Data.Node.Children.Count != 0 ||
                item.Data.Node.Collisions.Count != 0))
        {
            throw new InvalidDataException(
                "spZonePortalNode profile changed: " +
                JsonSerializer.Serialize(actual));
        }
    }

    private static ZonePortalNodeComparison CompareZonePortalNodes(
        IEnumerable<ZonePortalNodeObservation> observations,string leftCorpus,
        string rightCorpus)
    {
        Dictionary<(string Path,int Ordinal),ZonePortalNodeObservation> Build(
            string corpus) => observations.Where(item => item.CorpusKey == corpus)
            .ToDictionary(item => (item.CanonicalPath,item.Ordinal));
        var left = Build(leftCorpus);
        var right = Build(rightCorpus);
        var keys = left.Keys.Intersect(right.Keys).ToArray();
        return new ZonePortalNodeComparison(
            keys.Select(item => item.Path).Distinct().Count(),keys.Length,
            keys.Count(key => left[key].SemanticSignature ==
                              right[key].SemanticSignature),
            keys.Count(key => left[key].SerializedSha256 ==
                              right[key].SerializedSha256));
    }

    private static void UpsertZonePortalNodeFieldDefinitions(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<ZonePortalNodeObservation> observations)
    {
        (int Section,int Type,int Occurrence,string Semantic,string Display,
            string Kind,string Layout,bool Repeated,string Evidence,string Notes)[] defs =
        [
            (1,0,0,"node.position","Local position","vector3","three little-endian Single values",false,"confirmed_both_executables_and_full_corpus","Required; 103/103 objects per corpus and 65 distinct values."),
            (1,1,0,"node.rotation","Local rotation","quaternion","four little-endian Single values in X/Y/Z/W order",false,"confirmed_both_executables_and_full_corpus","Optional; one cloud01_02 portal node per corpus stores (0.5,0.5,0.5,-0.5)."),
            (1,2,0,"node.scale","Local scale","vector3","three little-endian Single values",false,"confirmed_both_executables_not_observed_corpus","Inherited default (1,1,1)."),
            (1,3,0,"node.is_bone","Bone node","boolean","one byte 0 or 1",false,"confirmed_both_executables_not_observed_corpus","Inherited false default."),
            (1,4,0,"node.is_static","Static node","boolean","one byte 0 or 1",false,"confirmed_both_executables_and_full_corpus","Optional; 22/103 objects per corpus explicitly store true."),
            (1,5,-1,"node.child","Child node","object_relationship","node child relationship",true,"confirmed_both_executables_not_observed_corpus","No portal node uses inherited node children."),
            (1,6,0,"node.billboard_axis","Billboard axis","uint32_enum","UInt32 axis 1 or 2",false,"confirmed_both_executables_not_observed_corpus","Inherited default 0."),
            (1,7,-1,"node.collision","Collision info","object_relationship","node collision relationship",true,"confirmed_both_executables_not_observed_corpus","No portal node uses inherited collision links."),
            (1,8,0,"node.is_animated","Animated node","boolean","one byte 0 or 1",false,"confirmed_both_executables_and_full_corpus","Required and always false."),
            (0,0,-1,"zone_portal_node.zone_portal","Zone portal","object_relationship","non-null sized-reference to spZonePortal",true,"confirmed_both_executables_and_full_corpus","Exactly two in the corpus: BackToFront followed by FrontToBack.")
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

    private static void AnnotateZonePortalNodeFields(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<ZonePortalNodeObservation> observations)
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
        foreach (ZonePortalNodeObservation observation in observations)
        foreach (ZonePortalNodeFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException(
                    "Could not annotate spZonePortalNode field.");
        }
    }

    private static int InsertZonePortalNodeVariant(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IEnumerable<ZonePortalNodeObservation> observations,string now)
    {
        Dictionary<string,object> corpusShapes = observations
            .GroupBy(item => item.CorpusKey).ToDictionary(
                group => group.Key,group => (object)new
                {
                    positionOnly=group.Count(item =>
                        !item.Data.Node.IsFieldSerialized(1) &&
                        !item.Data.Node.IsFieldSerialized(4)),
                    rotated=group.Count(item =>
                        item.Data.Node.IsFieldSerialized(1)),
                    explicitlyStatic=group.Count(item =>
                        item.Data.Node.IsFieldSerialized(4))
                });
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,status,
                discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2',$key,
                   'Placed bidirectional zone-portal pair','confirmed',
                   $discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$key",ZonePortalNodeVariantKey);
        command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
        {
            corpusShapes,sections=new[] { "spNode","spZonePortalNode" },
            nodeFieldOrder=new[] { "position","rotation?","is_static?",
                "is_animated","terminator" },
            ownFieldOrder=new[] { "back_to_front","front_to_back","terminator" },
            portalEncoding="sized_reference",portalCount=2
        }));
        command.Parameters.AddWithValue("$notes",
            "One serializer/runtime class. The three raw shapes differ only by " +
            "optional inherited spNode Rotation or Static fields, not by a " +
            "portal-node subtype.");
        command.Parameters.AddWithValue("$utc",now);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void AssignZonePortalNodeVariant(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<ZonePortalNodeObservation> observations,int variantId)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spZonePortalNode decoder and pair graph')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.AddWithValue("$variant",variantId);
        foreach (ZonePortalNodeObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.ExecuteNonQuery();
        }
    }

    private static float ZonePortalNodeReadSingle(
        ReadOnlySpan<byte> source,int offset) =>
        BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(
            source.Slice(offset,sizeof(float))));

    private sealed record ZonePortalNodeFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record ZonePortalNodeFieldAnnotation(
        int FieldIndex,string Semantic,string Layout,string DecodedJson);
    private sealed record ZonePortalNodePair(
        SmoLoadedReference BackRelationship,
        SmoLoadedReference FrontRelationship,
        SmoNodeRelationship BackSource,
        SmoNodeRelationship FrontSource,
        SmoZonePortalData BackPortal,
        SmoZonePortalData FrontPortal,
        bool PositionMatchesPolygonCentroid);
    private sealed record ZonePortalNodeObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string CanonicalPath,
        string Name,SmoZonePortalNodeData Data,
        bool PositionMatchesPolygonCentroid,string SerializedSha256,
        string SemanticSignature,
        IReadOnlyList<ZonePortalNodeFieldAnnotation> Fields);
    private sealed record ZonePortalNodeComparison(
        int CommonResources,int PairedObjects,int EqualSemantic,int EqualBytes);
}
