using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private const string ZonePortalVariantKey =
        "directed_zone_portal_with_polygon_and_open_state";

    private static SmoResearchClassAnalysisResult AnalyzeZonePortal(
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
            !parents.SetEquals([SmoClassIds.PartitionNode]) ||
            !children.SetEquals([SmoClassIds.Zone]))
        {
            throw new InvalidDataException(
                "spZonePortal corpus no longer matches its physical hierarchy.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly:false);
        List<ZonePortalObservation> observations =
            LoadZonePortalObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        long fieldCount = observations.Sum(item => (long)item.Fields.Count);
        long vertexCount = observations.Sum(item => (long)item.Data.Polygon.Count);
        if (objectCount != 618 || observations.Count != objectCount ||
            fieldCount != 1_854 || vertexCount != 2_472)
        {
            throw new InvalidDataException(
                $"Expected 618 portals, 1854 semantic fields and 2472 vertices; " +
                $"got {observations.Count}, {fieldCount} and {vertexCount}.");
        }
        RequireZonePortalProfiles(observations);
        ZonePortalPairProfile pairProfile = AnalyzeZonePortalPairs(observations);
        ZonePortalComparison pc = CompareZonePortals(
            observations,"pc-working","pc-pristine");
        ZonePortalComparison cross = CompareZonePortals(
            observations,"pc-pristine","ps2-pristine");
        if (pc != new ZonePortalComparison(18,206,206,206) ||
            cross.CommonResources != 18 || cross.PairedObjects != 206 ||
            cross.EqualSemantic != 206)
        {
            throw new InvalidDataException(
                "spZonePortal PC-copy or PC/PS2 pairing changed: " +
                JsonSerializer.Serialize(new { pc,cross }));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spZonePortalSerializer","ezpsfZonePortalDestinationZone",
            "ezpsfZonePortalPolygon","ezpsfZonePortalOpen",
            "GetDestinationZone()"
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
            clearVariants.Parameters.AddWithValue("$key",ZonePortalVariantKey);
            clearVariants.ExecuteNonQuery();
        }

        UpsertZonePortalFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateZonePortalFields(connection,transaction,observations);
        int variantId = InsertZonePortalVariant(
            connection,transaction,report.TypeHash,observations,now);
        AssignZonePortalVariant(connection,transaction,observations,variantId);

        int inlineDestinations = observations.Count(item =>
            item.WireDestination.Encoding ==
            SmoNodeRelationshipEncoding.InlineObject);
        int references = observations.Count - inlineDestinations;
        int open = observations.Count(item => item.Data.IsOpen);
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "reader/index/writer VA 0x0044DDA0..0x0044E4D8",
            "The x86 serializer maps fields 0/1/2 to DestinationZone, Polygon " +
            "and Open. Runtime members are destination +0x14, vertex count " +
            "+0x18, vertex pointer +0x1C and Boolean open +0x20.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "reader/index/writer VA 0x001A3310..0x001A3920",
            "The independent MIPS serializer uses the same field switch, " +
            "spZone factory hash 0x61254AB3, variable polygon count and runtime " +
            "offsets +0x14/+0x18/+0x1C/+0x20; a null destination is rejected.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload and complete one-section decode",
            $"{objectCount} named directed portals contain exactly three fields. " +
            $"All {open} are open quadrilaterals; DestinationZone uses " +
            $"{inlineDestinations} inline-owned zones and {references} sized " +
            $"references. {fieldCount} direct fields were annotated.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:graph_and_cross_corpus","smo-corpus-v2.sqlite",
            "spPartitionNode source zone plus spZonePortalNode membership",
            $"Each corpus has {pairProfile.PairsPerCorpus} bidirectional portal " +
            $"pairs: the two members exchange source/destination zones. Polygon " +
            $"pair ordering is {JsonSerializer.Serialize(pairProfile.PolygonForms)}. " +
            $"PC working/pristine match {pc.EqualBytes}/{pc.PairedObjects} byte " +
            $"for byte; PC/PS2 match {cross.EqualSemantic}/{cross.PairedObjects} " +
            "after object-ID/ownership normalization.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='spatial_portal',
                       description='Directed open/closed polygon portal from its owning partition-node zone to a destination zone',
                       decode_status='read_only_decode',
                       notes='PC and PS2 share fields 0 DestinationZone, 1 variable-length Polygon and 2 Open. Inline versus reference destination encoding records serialization ownership, not a portal subtype.'
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
            $"One directed portal layout across {objectCount} objects: " +
            $"{inlineDestinations} inline and {references} referenced destinations, " +
            $"all quadrilateral/open, grouped into {pairProfile.PairsPerCorpus} " +
            "bidirectional pairs per corpus. PC copies and all PC/PS2 semantics match.");
    }

    private static List<ZonePortalObservation> LoadZonePortalObservations(
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.ZonePortal);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<ZonePortalFileLocation>();
        while (reader.Read())
        {
            locations.Add(new ZonePortalFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<ZonePortalObservation>();
        foreach (ZonePortalFileLocation location in locations)
        {
            byte[] bytes = ReadZonePortalResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            Dictionary<int,int> pairMembership =
                BuildZonePortalPairMembership(document);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.ZonePortal))
            {
                if (!SmoZonePortalDecoder.TryDecode(
                        document,entry,out SmoZonePortalData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                if (entry.ParentIndex is not int parentIndex ||
                    document.Objects[parentIndex].TypeHash !=
                        SmoClassIds.PartitionNode ||
                    !pairMembership.TryGetValue(entry.Index,out int pairNodeIndex))
                {
                    throw new InvalidDataException(
                        "spZonePortal requires one physical spPartitionNode owner " +
                        "and one logical spZonePortalNode pair membership.");
                }
                SmoNodeRelationship source = GetZonePortalSource(
                    document,document.Objects[parentIndex]);
                result.Add(CreateZonePortalObservation(
                    location,document,entry,ordinal++,pairNodeIndex,source,decoded));
            }
        }
        return result;
    }

    private static Dictionary<int,int> BuildZonePortalPairMembership(
        SmoDocument document)
    {
        Dictionary<uint,SmoObjectEntry> objectsById = document.Objects
            .GroupBy(candidate => candidate.Id).Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key,group => group.Single());
        var result = new Dictionary<int,int>();
        foreach (SmoObjectEntry node in document.Objects.Where(item =>
                     item.TypeHash == SmoClassIds.ZonePortalNode))
        {
            IReadOnlyList<SmoObjectField> fields =
                SmoObjectFieldReader.Read(document,node);
            List<SmoNodeRelationship> portals = fields
                .Where(field => field.PayloadSize > 0)
                .Select(field => SmoNodeDecoder.TryDecodeRelationship(
                    field.Payload.Span,objectsById,
                    out SmoNodeRelationship? relationship)
                    ? relationship : null)
                .Where(item => item?.TargetTypeHash == SmoClassIds.ZonePortal)
                .Cast<SmoNodeRelationship>().ToList();
            if (portals.Count != 2)
                throw new InvalidDataException(
                    "Every observed spZonePortalNode must reference two portals.");
            foreach (SmoNodeRelationship portal in portals)
            {
                if (portal.TargetObjectIndex is not int index ||
                    !result.TryAdd(index,node.Index))
                {
                    throw new InvalidDataException(
                        "A portal has invalid or duplicate spZonePortalNode membership.");
                }
            }
        }
        return result;
    }

    private static SmoNodeRelationship GetZonePortalSource(
        SmoDocument document,SmoObjectEntry partitionNode)
    {
        IReadOnlyList<SmoObjectField> fields =
            SmoObjectFieldReader.Read(document,partitionNode);
        SmoObjectField[] candidates = fields.Where(field =>
            field.FieldType == 3 && field.PayloadSize > 0).ToArray();
        if (candidates.Length != 1 ||
            !SmoNodeDecoder.TryDecodeRelationship(
                document,candidates[0].Payload.Span,
                out SmoNodeRelationship? relationship) || relationship is null ||
            relationship.TargetTypeHash != SmoClassIds.Zone)
        {
            throw new InvalidDataException(
                "Portal-owning spPartitionNode must reference one source zone.");
        }
        return relationship;
    }

    private static byte[] ReadZonePortalResource(ZonePortalFileLocation location)
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
                "PCK zone-portal occurrence is incomplete.");
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

    private static ZonePortalObservation CreateZonePortalObservation(
        ZonePortalFileLocation location,SmoDocument document,SmoObjectEntry entry,
        int ordinal,int pairNodeIndex,SmoNodeRelationship source,
        SmoZonePortalData decoded)
    {
        IReadOnlyList<SmoObjectField> direct =
            SmoObjectFieldReader.Read(document,entry);

        SmoNodeRelationship wireDestination = ReadSpatialWireReference(
            document,entry,"zone_portal.destination_zone");
        if (wireDestination.ObjectId != decoded.DestinationZone.ObjectId)
            throw new InvalidDataException("Authored and loaded zone destinations differ.");
        var fields = new List<ZonePortalFieldAnnotation>();
        for (int index = 0; index < direct.Count; index++)
        {
            SmoObjectField field = direct[index];
            if (field.FieldType == 0 && field.PayloadSize == 0)
                continue;
            if (!SmoSerializedFieldRegistry.TryDescribeOwnField(
                    entry.TypeHash,direct,index,
                    out SmoSerializedFieldDescriptor? descriptor) || descriptor is null)
            {
                throw new InvalidDataException(
                    "Unregistered spZonePortal direct field.");
            }
            string layout;
            string value;
            switch (descriptor.Key)
            {
                case "zone_portal.destination_zone":
                    layout = "non-null sized-reference or owned inline spZone relationship";
                    value = SerializeZonePortalRelationship(wireDestination);
                    break;
                case "zone_portal.polygon":
                    layout = "UInt32 vertex count followed by Vector3 vertices";
                    value = JsonSerializer.Serialize(new
                    {
                        VertexCount=decoded.Polygon.Count,
                        Vertices=decoded.Polygon.Select(vertex =>
                            new[] { vertex.X,vertex.Y,vertex.Z }).ToArray()
                    });
                    break;
                case "zone_portal.open":
                    layout = "one byte Boolean 0 or 1";
                    value = JsonSerializer.Serialize(decoded.IsOpen);
                    break;
                default:
                    throw new InvalidDataException(
                        "Unexpected spZonePortal semantic field.");
            }
            fields.Add(new ZonePortalFieldAnnotation(
                index,descriptor.Key,layout,value));
        }
        string sourceName = source.TargetName?.TrimEnd('\0') ?? string.Empty;
        string destinationName =
            decoded.DestinationZone.TargetName?.TrimEnd('\0') ?? string.Empty;
        string polygonSignature = string.Join(",",decoded.Polygon.Select(vertex =>
            $"{BitConverter.SingleToInt32Bits(vertex.X):X8}" +
            $"{BitConverter.SingleToInt32Bits(vertex.Y):X8}" +
            $"{BitConverter.SingleToInt32Bits(vertex.Z):X8}"));
        string semantic = $"{sourceName.ToLowerInvariant()}->" +
                          $"{destinationName.ToLowerInvariant()}|" +
                          $"{polygonSignature}|open={decoded.IsOpen}";
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        return new ZonePortalObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            entry.Name.TrimEnd('\0'),pairNodeIndex,source,decoded,serializedHash,
            semantic,fields.AsReadOnly(),wireDestination);
    }

    private static string SerializeZonePortalRelationship(
        SmoNodeRelationship relationship) => JsonSerializer.Serialize(new
        {
            relationship.ObjectId,
            Encoding=GetPartitionNodeEncodingName(relationship.Encoding),
            relationship.InlineSerializedSize,
            relationship.TargetObjectIndex,
            TargetTypeHash=relationship.TargetTypeHash.HasValue
                ? $"0x{relationship.TargetTypeHash.Value:X8}" : null,
            TargetClass=relationship.TargetTypeHash.HasValue
                ? SmoClassRegistry.GetDisplayName(relationship.TargetTypeHash.Value)
                : null,
            TargetName=relationship.TargetName?.TrimEnd('\0') ?? string.Empty
        });

    private static void RequireZonePortalProfiles(
        IReadOnlyList<ZonePortalObservation> observations)
    {
        var expected = new Dictionary<string,(int Objects,int Inline,int References,
            int FrontInline,int BackInline,int Open,int Vertices)>
        {
            ["pc-working"]=(206,93,113,27,66,206,824),
            ["pc-pristine"]=(206,93,113,27,66,206,824),
            ["ps2-pristine"]=(206,93,113,27,66,206,824)
        };
        var actual = observations.GroupBy(item => item.CorpusKey).ToDictionary(
            group => group.Key,group => (
                Objects:group.Count(),
                Inline:group.Count(item => item.WireDestination.Encoding ==
                    SmoNodeRelationshipEncoding.InlineObject),
                References:group.Count(item => item.WireDestination.Encoding ==
                    SmoNodeRelationshipEncoding.SizedReference),
                FrontInline:group.Count(item =>
                    item.Name.EndsWith("FrontToBack",StringComparison.Ordinal) &&
                    item.WireDestination.Encoding ==
                        SmoNodeRelationshipEncoding.InlineObject),
                BackInline:group.Count(item =>
                    item.Name.EndsWith("BackToFront",StringComparison.Ordinal) &&
                    item.WireDestination.Encoding ==
                        SmoNodeRelationshipEncoding.InlineObject),
                Open:group.Count(item => item.Data.IsOpen),
                Vertices:group.Sum(item => item.Data.Polygon.Count)));
        if (actual.Count != expected.Count || expected.Any(item =>
                !actual.TryGetValue(item.Key,out var value) || value != item.Value) ||
            observations.Any(item => item.Data.Polygon.Count != 4))
        {
            throw new InvalidDataException(
                "spZonePortal profile changed: " + JsonSerializer.Serialize(actual));
        }
    }

    private static ZonePortalPairProfile AnalyzeZonePortalPairs(
        IReadOnlyList<ZonePortalObservation> observations)
    {
        Dictionary<string,int> forms = new(StringComparer.Ordinal);
        int? pairsPerCorpus = null;
        foreach (IGrouping<string,ZonePortalObservation> corpus in
                 observations.GroupBy(item => item.CorpusKey))
        {
            var pairs = corpus.GroupBy(item => (item.FileId,item.PairNodeIndex))
                .ToArray();
            if (pairs.Any(group => group.Count() != 2))
                throw new InvalidDataException(
                    "spZonePortalNode does not form two-member portal pairs.");
            foreach (IGrouping<(int,int),ZonePortalObservation> pair in pairs)
            {
                ZonePortalObservation[] members = pair.ToArray();
                if (members[0].SourceZone.ObjectId !=
                        members[1].Data.DestinationZone.ObjectId ||
                    members[1].SourceZone.ObjectId !=
                        members[0].Data.DestinationZone.ObjectId)
                {
                    throw new InvalidDataException(
                        "spZonePortalNode pair is not a bidirectional zone edge.");
                }
                string form = ClassifyPortalPolygonPair(
                    members[0].Data.Polygon,members[1].Data.Polygon);
                forms[form] = forms.GetValueOrDefault(form) + 1;
            }
            pairsPerCorpus ??= pairs.Length;
            if (pairsPerCorpus != pairs.Length || pairs.Length != 103)
                throw new InvalidDataException(
                    "Expected 103 portal pairs in every corpus.");
        }
        return new ZonePortalPairProfile(
            pairsPerCorpus ?? 0,forms.OrderBy(item => item.Key)
                .ToDictionary(item => item.Key,item => item.Value));
    }

    private static string ClassifyPortalPolygonPair(
        IReadOnlyList<System.Numerics.Vector3> left,
        IReadOnlyList<System.Numerics.Vector3> right)
    {
        if (left.SequenceEqual(right))
            return "same_order";
        if (left.SequenceEqual(right.Reverse()))
            return "exact_reverse";
        if (left.Count == right.Count && left.All(right.Contains))
            return "same_vertices_rotated_or_reversed";
        return "different_vertices";
    }

    private static ZonePortalComparison CompareZonePortals(
        IEnumerable<ZonePortalObservation> observations,string leftCorpus,
        string rightCorpus)
    {
        Dictionary<(string Path,int Ordinal),ZonePortalObservation> Build(
            string corpus) => observations.Where(item => item.CorpusKey == corpus)
            .ToDictionary(item => (item.CanonicalPath,item.Ordinal));
        var left = Build(leftCorpus);
        var right = Build(rightCorpus);
        var keys = left.Keys.Intersect(right.Keys).ToArray();
        return new ZonePortalComparison(
            keys.Select(item => item.Path).Distinct().Count(),keys.Length,
            keys.Count(key => left[key].SemanticSignature ==
                              right[key].SemanticSignature),
            keys.Count(key => left[key].SerializedSha256 ==
                              right[key].SerializedSha256));
    }

    private static void UpsertZonePortalFieldDefinitions(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<ZonePortalObservation> observations)
    {
        (int Type,string Semantic,string Display,string Kind,string Layout,
            string Notes)[] definitions =
        [
            (0,"zone_portal.destination_zone","Destination zone",
                "object_relationship",
                "non-null sized-reference or owned inline spZone relationship",
                "Required. Inline/reference is ownership encoding, not a subtype."),
            (1,"zone_portal.polygon","Portal polygon","polygon3",
                "UInt32 vertex count followed by little-endian Vector3 vertices",
                "Required; serializer supports a variable count, corpus uses four."),
            (2,"zone_portal.open","Open portal","boolean",
                "one byte 0 or 1","Required; all corpus values are true.")
        ];
        foreach (var definition in definitions)
        {
            int observed = observations.Sum(item => item.Fields.Count(field =>
                field.Semantic == definition.Semantic));
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',0,$type,0,$semantic,$display,$kind,
                       $layout,'read_only_research',
                       'confirmed_both_executables_and_full_corpus',$constraints,
                       $notes)
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
                observedDirectOccurrences=observed,repeated=false,
                mutationStatus="not_enabled"
            }));
            command.Parameters.AddWithValue("$notes",definition.Notes);
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotateZonePortalFields(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<ZonePortalObservation> observations)
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
        foreach (ZonePortalObservation observation in observations)
        foreach (ZonePortalFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException(
                    "Could not annotate spZonePortal field.");
        }
    }

    private static int InsertZonePortalVariant(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IEnumerable<ZonePortalObservation> observations,string now)
    {
        Dictionary<string,int> corpusCounts = observations
            .GroupBy(item => item.CorpusKey)
            .ToDictionary(group => group.Key,group => group.Count());
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,status,
                discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2',$key,
                   'Directed polygon zone portal','confirmed',$discriminator,
                   $notes,$utc,$utc);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$key",ZonePortalVariantKey);
        command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
        {
            corpusCounts,
            fieldOrder=new[] { "destination_zone","polygon","open","terminator" },
            destinationEncoding=new[] { "owned_inline","sized_reference" },
            destinationClass="spZone",observedVertexCount=4,
            observedOpen=true
        }));
        command.Parameters.AddWithValue("$notes",
            "One serializer layout. Destination ownership changes payload size; " +
            "it is not a separate portal variant.");
        command.Parameters.AddWithValue("$utc",now);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void AssignZonePortalVariant(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<ZonePortalObservation> observations,int variantId)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spZonePortal decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.AddWithValue("$variant",variantId);
        foreach (ZonePortalObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.ExecuteNonQuery();
        }
    }

    private sealed record ZonePortalFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record ZonePortalFieldAnnotation(
        int FieldIndex,string Semantic,string Layout,string DecodedJson);
    private sealed record ZonePortalObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string CanonicalPath,
        string Name,int PairNodeIndex,SmoNodeRelationship SourceZone,
        SmoZonePortalData Data,string SerializedSha256,string SemanticSignature,
        IReadOnlyList<ZonePortalFieldAnnotation> Fields,SmoNodeRelationship WireDestination);
    private sealed record ZonePortalComparison(
        int CommonResources,int PairedObjects,int EqualSemantic,int EqualBytes);
    private sealed record ZonePortalPairProfile(
        int PairsPerCorpus,IReadOnlyDictionary<string,int> PolygonForms);
}
