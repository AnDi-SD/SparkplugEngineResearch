using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeNavigationPortal(
        string databasePath,SmoResearchClassReport report)
    {
        using SqliteConnection connection = OpenResearch(
            Path.GetFullPath(databasePath),readOnly:false);
        List<NavigationPortalObservation> observations =
            LoadNavigationPortalObservations(connection);
        if (observations.Count != 209 || observations.Sum(item => item.Fields.Count) != 9365)
            throw new InvalidDataException("spNavigationPortal corpus profile changed.");
        RequireCounts(observations,item => item.CorpusKey,
            new Dictionary<string,int>
            { ["pc-working"]=70,["pc-pristine"]=70,["ps2-pristine"]=69 });
        NavigationPairing pc = CompareNavigation(
            observations,item => item.CorpusKey,item => item.PairingKey,
            item => item.SemanticSignature,item => item.SerializedSha256,
            "pc-working","pc-pristine");
        NavigationPairing cross = CompareNavigation(
            observations,item => item.CorpusKey,item => item.PairingKey,
            item => item.SemanticSignature,item => item.SerializedSha256,
            "pc-pristine","ps2-pristine");
        if (pc != new NavigationPairing(70,70,70) ||
            cross != new NavigationPairing(69,69,65))
        {
            throw new InvalidDataException("spNavigationPortal corpus pairing changed.");
        }
        Dictionary<string,int> encodings = observations.GroupBy(item =>
                string.Join("_",item.Data.EndpointSets.Select(set => set.Encoding)))
            .ToDictionary(group => group.Key,group => group.Count());
        if (encodings.Count != 3 || encodings.Values.Sum() != 209)
            throw new InvalidDataException("Portal relationship storage variants changed.");

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] tokens =
        [
            "spNavigationPortal","spNavigationPortalSerializer",
            "esfNavPortalGraph","esfNavPortalNavSets","esfNavPortalNavNodes",
            "esfNavPortalPath","m_uSrcSet","m_uDstSet","m_uPathIndex"
        ];
        RequireAsciiTokens(pcExecutable.Path,tokens);
        RequireAsciiTokens(ps2Executable.Path,tokens);

        string now = DateTime.UtcNow.ToString("O");
        using SqliteTransaction transaction = connection.BeginTransaction();
        ClearNavigationAnalysis(connection,transaction,report.TypeHash,"navigation_portal_%");
        UpsertNavigationPortalDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateNavigationFields(connection,transaction,observations.SelectMany(item =>
            item.Fields.Select(field => (item.FileId,item.ObjectIndex,field))));
        Dictionary<string,int> variants = InsertNavigationPortalVariants(
            connection,transaction,report.TypeHash,observations,now);
        AssignNavigationPortalVariants(connection,transaction,observations,variants);

        int evidenceRows = 0;
        int primaryVariant = variants.Values.First();
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,primaryVariant,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "registration RVA 0x002D23C0; serializer VA 0x00447000..0x004476D4; " +
            "field-name/assertion strings RVA 0x002E3430..0x002E3AE4",
            "The x86 registration proves direct inheritance from spNode. The writer " +
            "serializes graph, two endpoint sets, repeated node pairs and repeated " +
            "three-byte path memberships. Assertion strings independently name " +
            "m_uSrcSet, m_uDstSet and m_uPathIndex.",pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,primaryVariant,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "independent MIPS executable token and serializer-field identity match",
            "The PS2 executable contains the same portal class, serializer and enum " +
            "tokens, including the three named path-member bytes; the PS2 corpus " +
            "uses the same two-section layout and scalar widths.",ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,primaryVariant,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload, target decode and graph-topology inversion",
            $"All {observations.Count} portals decode. Their " +
            $"{observations.Sum(item => item.Data.NodePairs.Count)} node pairs fit both " +
            "endpoint navigation-set NodeCounts. All " +
            $"{observations.Sum(item => item.Data.PathMemberships.Count)} membership " +
            "rows invert graph alternative routes exactly; both endpoint relationship " +
            "orders and all inline/sized-reference locations resolve.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,primaryVariant,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical path, object name and ordinal comparison",
            $"PC working/pristine: {pc.EqualBytes}/{pc.Paired} exact. PC/PS2: " +
            $"{cross.EqualSemantic}/{cross.Paired} semantic and " +
            $"{cross.EqualBytes}/{cross.Paired} exact serialized matches. One portal " +
            "exists only in the PC test-world resource.",null,now);
        UpdateNavigationClass(connection,transaction,report.TypeHash,
            "navigation_portal",
            "Navigation edge joining two spMeshNavigationSet objects, with endpoint triangle pairs and inverse alternative-route membership",
            "Complete PC/PS2 two-section read-only decode. Path rows are (source set, destination set, alternative path index), not movement commands; graph topology determines order. Editing requires coordinated graph/set rebuild.");
        transaction.Commit();
        Checkpoint(connection);
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,observations.Count,
            report.Profiles.Sum(item => item.UniqueResourceCount),
            observations.Select(item => item.SerializedSha256).Distinct().Count(),
            observations.Count,evidenceRows,
            "Three endpoint relationship-storage variants; all node IDs and all " +
            "8,044 graph path-membership rows validated on PC and PS2.");
    }

    private static SmoResearchClassAnalysisResult AnalyzeNavigationGraph(
        string databasePath,SmoResearchClassReport report)
    {
        using SqliteConnection connection = OpenResearch(
            Path.GetFullPath(databasePath),readOnly:false);
        List<NavigationGraphObservation> observations =
            LoadNavigationGraphObservations(connection);
        if (observations.Count != 80 || observations.Sum(item => item.Fields.Count) != 3793)
            throw new InvalidDataException("spNavigationGraph corpus profile changed.");
        RequireCounts(observations,item => item.CorpusKey,
            new Dictionary<string,int>
            { ["pc-working"]=27,["pc-pristine"]=27,["ps2-pristine"]=26 });
        NavigationPairing pc = CompareNavigation(
            observations,item => item.CorpusKey,item => item.PairingKey,
            item => item.SemanticSignature,item => item.SerializedSha256,
            "pc-working","pc-pristine");
        NavigationPairing cross = CompareNavigation(
            observations,item => item.CorpusKey,item => item.PairingKey,
            item => item.SemanticSignature,item => item.SerializedSha256,
            "pc-pristine","ps2-pristine");
        if (pc != new NavigationPairing(27,27,27) ||
            cross != new NavigationPairing(26,26,23))
        {
            throw new InvalidDataException("spNavigationGraph corpus pairing changed.");
        }
        int alternatives = observations.Sum(item =>
            item.Data.Paths.Sum(path => path.Alternatives.Count));
        int reachable = observations.Sum(item => item.Data.Paths.Count(path => path.IsReachable));
        if (alternatives != 2344 || reachable != 1282 ||
            observations.SelectMany(item => item.Data.Paths)
                .SelectMany(path => path.Alternatives).Any(item => item.Reserved != 0))
        {
            throw new InvalidDataException("Graph path topology profile changed.");
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] tokens =
        [
            "spNavigationGraph","spNavigationGraphSerializer",
            "pNavGraph->GetNavigationSet","pNavGraph->GetNavigationPortal",
            "pNavGraph->GetNavigationSetCount","Path.m_uNumPathInfos",
            "Path.m_uNextPortal"
        ];
        RequireAsciiTokens(pcExecutable.Path,tokens);
        RequireAsciiTokens(ps2Executable.Path,tokens);

        string now = DateTime.UtcNow.ToString("O");
        using SqliteTransaction transaction = connection.BeginTransaction();
        ClearNavigationAnalysis(connection,transaction,report.TypeHash,"navigation_graph_%");
        UpsertNavigationGraphDefinitions(connection,transaction,report.TypeHash,observations);
        AnnotateNavigationFields(connection,transaction,observations.SelectMany(item =>
            item.Fields.Select(field => (item.FileId,item.ObjectIndex,field))));
        Dictionary<string,int> variants = InsertNavigationGraphVariants(
            connection,transaction,report.TypeHash,observations,now);
        AssignNavigationGraphVariants(connection,transaction,observations,variants);

        int evidenceRows = 0;
        int primaryVariant = variants["navigation_graph_production"];
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,primaryVariant,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "writer VA 0x004454F5..0x00445872; reader assertion strings RVA 0x002E32F8",
            "The x86 serializer writes repeated set and portal relationships, set " +
            "count, then a square row-major table. Each path contains UInt32 source/" +
            "destination indices, UInt8 next portal, UInt8 alternative count and " +
            "two UInt8 values per alternative. Runtime path records have stride 16.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,primaryVariant,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "independent MIPS executable token and serializer-field identity match",
            "The PS2 executable names the same class, serializer, fields and path " +
            "members. All PS2 resources independently reproduce the three sections, " +
            "row-major table and scalar widths.",ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,primaryVariant,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload and full portal-topology traversal",
            $"All {observations.Count} graphs decode: 2,507 square table rows, " +
            $"{reachable} reachable directions and {alternatives} alternative routes. " +
            "Every next portal is incident to the current set and repeated next hops " +
            "reach the destination without cycles. Every alternative's first portal " +
            "and full membership chain are proven by portal endpoint topology.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,primaryVariant,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path comparison",
            $"PC working/pristine: {pc.EqualBytes}/{pc.Paired} exact. PC/PS2: " +
            $"{cross.EqualSemantic}/{cross.Paired} semantic and " +
            $"{cross.EqualBytes}/{cross.Paired} exact serialized matches. One false-" +
            "Animated graph exists only in the two PC copies of test_world_navmesh.",
            null,now);
        UpdateNavigationClass(connection,transaction,report.TypeHash,
            "navigation_graph",
            "Root navigation graph with owned sets/portals and square precomputed alternative-route table",
            "Complete PC/PS2 read-only decode of inherited spNode, empty spRenderNode and graph fields 0..3. 255 is self/unreachable. Alternative byte 0 selects the first portal; byte 1 is invariant zero and remains reserved pending runtime evidence.");
        transaction.Commit();
        Checkpoint(connection);
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,observations.Count,
            report.Profiles.Sum(item => item.UniqueResourceCount),
            observations.Select(item => item.SerializedSha256).Distinct().Count(),
            observations.Count,evidenceRows,
            "Production and PC-only test variants; all square tables, next hops and " +
            "2,344 alternative routes validated against 8,044 portal memberships.");
    }

    private static List<NavigationPortalObservation> LoadNavigationPortalObservations(
        SqliteConnection connection)
    {
        var result = new List<NavigationPortalObservation>();
        foreach (MeshNavigationFileLocation location in
                 LoadNavigationLocations(connection,SmoClassIds.NavigationPortal))
        {
            SmoDocument document = SmoDocument.Parse(
                ReadMeshNavigationResource(location),location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.NavigationPortal))
            {
                if (!SmoNavigationPortalDecoder.TryDecode(
                        document,entry,out var data,out string error) || data is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:{location.RelativePath} " +
                        $"[{entry.Index}]: {error}");
                }
                IReadOnlyList<SmoObjectField> fields = SmoObjectFieldReader.Read(document,entry);
                var annotations = new List<MeshNavigationFieldAnnotation>();
                int nodePair = 0, membership = 0;
                foreach ((SmoObjectField field,int index) in fields.Select((field,index)=>(field,index)))
                {
                    if (field.FieldType == 0 && field.PayloadSize == 0) continue;
                    if (!SmoSerializedFieldRegistry.TryDescribeField(
                            entry.TypeHash,fields,index,out var descriptor) || descriptor is null)
                        throw new InvalidDataException("Unregistered portal field.");
                    object decoded = descriptor.Key switch
                    {
                        "node.position" => new { data.Node.Position.X,data.Node.Position.Y,data.Node.Position.Z },
                        "node.is_animated" => data.Node.IsAnimated,
                        "navigation_portal.graph" => RelationshipValue(data.Graph),
                        "navigation_portal.sets" => data.EndpointSets.Select(RelationshipValue),
                        "navigation_portal.nodes" => data.NodePairs[nodePair++],
                        "navigation_portal.path" => data.PathMemberships[membership++],
                        _ => throw new InvalidDataException(
                            $"Unexpected portal semantic {descriptor.Key}.")
                    };
                    annotations.Add(new MeshNavigationFieldAnnotation(
                        index,descriptor.Key,descriptor.PayloadLayout,
                        JsonSerializer.Serialize(decoded)));
                }
                string bytesHash = ObjectHash(document,entry);
                string semantic = JsonSerializer.Serialize(new
                {
                    Position=new[] { data.Node.Position.X,data.Node.Position.Y,data.Node.Position.Z },
                    data.Node.IsAnimated,NodePairs=data.NodePairs,
                    Memberships=data.PathMemberships
                });
                string canonical = GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant();
                string pairing = $"{canonical}|{entry.Name.TrimEnd('\0').ToLowerInvariant()}|{ordinal}";
                result.Add(new NavigationPortalObservation(
                    location.FileId,entry.Index,location.CorpusKey,pairing,data,
                    bytesHash,semantic,annotations.AsReadOnly()));
                ordinal++;
            }
        }
        return result;
    }

    private static List<NavigationGraphObservation> LoadNavigationGraphObservations(
        SqliteConnection connection)
    {
        var result = new List<NavigationGraphObservation>();
        foreach (MeshNavigationFileLocation location in
                 LoadNavigationLocations(connection,SmoClassIds.NavigationGraph))
        {
            SmoDocument document = SmoDocument.Parse(
                ReadMeshNavigationResource(location),location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.NavigationGraph))
            {
                if (!SmoNavigationGraphDecoder.TryDecode(
                        document,entry,out var data,out string error) || data is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:{location.RelativePath} " +
                        $"[{entry.Index}]: {error}");
                }
                IReadOnlyList<SmoObjectField> fields = SmoObjectFieldReader.Read(document,entry);
                var annotations = new List<MeshNavigationFieldAnnotation>();
                int child=0,set=0,portal=0,path=0;
                foreach ((SmoObjectField field,int index) in fields.Select((field,index)=>(field,index)))
                {
                    if (field.FieldType == 0 && field.PayloadSize == 0) continue;
                    if (!SmoSerializedFieldRegistry.TryDescribeField(
                            entry.TypeHash,fields,index,out var descriptor) || descriptor is null)
                        throw new InvalidDataException("Unregistered graph field.");
                    object decoded = descriptor.Key switch
                    {
                        "node.position" => new { data.RenderNode.Node.Position.X,
                            data.RenderNode.Node.Position.Y,data.RenderNode.Node.Position.Z },
                        "node.child" => RelationshipValue(data.RenderNode.Node.Children[child++]),
                        "node.is_animated" => data.RenderNode.Node.IsAnimated,
                        "navigation_graph.set" => RelationshipValue(data.NavigationSets[set++]),
                        "navigation_graph.portal" => RelationshipValue(data.Portals[portal++]),
                        "navigation_graph.path_table_size" => data.PathTableSize,
                        "navigation_graph.path" => data.Paths[path++],
                        _ => throw new InvalidDataException(
                            $"Unexpected graph semantic {descriptor.Key}.")
                    };
                    annotations.Add(new MeshNavigationFieldAnnotation(
                        index,descriptor.Key,descriptor.PayloadLayout,
                        JsonSerializer.Serialize(decoded)));
                }
                string bytesHash = ObjectHash(document,entry);
                string semantic = JsonSerializer.Serialize(new
                {
                    Position=new[] { data.RenderNode.Node.Position.X,
                        data.RenderNode.Node.Position.Y,data.RenderNode.Node.Position.Z },
                    data.RenderNode.Node.IsAnimated,data.PathTableSize,
                    SetTypes=data.NavigationSets.Select(item => item.TargetTypeHash),
                    PortalTypes=data.Portals.Select(item => item.TargetTypeHash),
                    Paths=data.Paths
                });
                string canonical = GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant();
                string pairing = $"{canonical}|{ordinal}";
                result.Add(new NavigationGraphObservation(
                    location.FileId,entry.Index,location.CorpusKey,pairing,data,
                    bytesHash,semantic,annotations.AsReadOnly()));
                ordinal++;
            }
        }
        return result;
    }

    private static List<MeshNavigationFileLocation> LoadNavigationLocations(
        SqliteConnection connection,uint typeHash)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT DISTINCT f.id,c.corpus_key,p.platform_key,c.source_kind,
                   c.source_root,f.relative_path,ct.relative_path,fo.byte_offset,
                   fo.byte_size
            FROM files f JOIN corpora c ON c.id=f.corpus_id
            JOIN platforms p ON p.id=c.platform_id
            JOIN objects o ON o.file_id=f.id AND o.type_hash=$hash
            LEFT JOIN file_occurrences fo ON fo.id=(
                SELECT MIN(inside_fo.id) FROM file_occurrences inside_fo
                WHERE inside_fo.file_id=f.id)
            LEFT JOIN containers ct ON ct.id=fo.container_id
            ORDER BY c.id,f.normalized_path;
            """;
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new List<MeshNavigationFileLocation>();
        while (reader.Read())
        {
            result.Add(new MeshNavigationFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }
        return result;
    }

    private static void ClearNavigationAnalysis(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        string variantPattern)
    {
        foreach (string sql in new[]
        {
            "DELETE FROM evidence WHERE type_hash=$hash AND evidence_kind LIKE 'class_analysis:%';",
            "DELETE FROM class_variants WHERE type_hash=$hash AND variant_key LIKE $variant;",
            "DELETE FROM field_definitions WHERE type_hash=$hash;"
        })
        {
            using SqliteCommand command = CreateCommand(connection,transaction,sql);
            command.Parameters.AddWithValue("$hash",(long)typeHash);
            command.Parameters.AddWithValue("$variant",variantPattern);
            command.ExecuteNonQuery();
        }
    }

    private static void UpsertNavigationPortalDefinitions(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<NavigationPortalObservation> observations) =>
        UpsertNavigationDefinitions(connection,transaction,typeHash,observations
            .SelectMany(item => item.Fields),
        [
            (0,0,0,"navigation_portal.graph","Navigation graph","object_relationship","one sized relationship to spNavigationGraph",false,"Required and resolved in all objects."),
            (0,1,0,"navigation_portal.sets","Endpoint navigation sets","relationship_pair","two concatenated sized/inline spMeshNavigationSet relationships",false,"Required ordered endpoints; three storage combinations observed."),
            (0,2,-1,"navigation_portal.nodes","Endpoint node pair","uint8_pair","UInt8 node ID in endpoint set 0 then UInt8 node ID in endpoint set 1",true,"Repeated 1..12; every ID is below the matching set NodeCount."),
            (0,3,-1,"navigation_portal.path","Alternative path membership","uint8_triple","UInt8 source set, destination set, alternative path index",true,"Repeated 2..130; reverse index into spNavigationGraph path alternatives.")
        ],includeNode:true,includeRenderNode:false);

    private static void UpsertNavigationGraphDefinitions(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<NavigationGraphObservation> observations) =>
        UpsertNavigationDefinitions(connection,transaction,typeHash,observations
            .SelectMany(item => item.Fields),
        [
            (0,0,-1,"navigation_graph.set","Navigation set","object_relationship","one sized relationship to spMeshNavigationSet",true,"Repeated 1..11 in graph order."),
            (0,1,-1,"navigation_graph.portal","Navigation portal","object_relationship","one sized relationship to spNavigationPortal",true,"Repeated 0..10 in portal-index order."),
            (0,2,0,"navigation_graph.path_table_size","Path table size","uint32","little-endian UInt32",false,"Required; equals navigation-set count."),
            (0,3,-1,"navigation_graph.path","Navigation path","navigation_path","UInt32 source/destination; UInt8 next portal/count; repeated UInt8 first portal/reserved pairs",true,"Exactly size squared in row-major order; reserved byte is zero throughout PC and PS2.")
        ],includeNode:true,includeRenderNode:true);

    private static void UpsertNavigationDefinitions(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IEnumerable<MeshNavigationFieldAnnotation> fields,
        IReadOnlyList<(int Section,int Type,int Occurrence,string Semantic,string Display,
            string Kind,string Layout,bool Repeated,string Notes)> own,
        bool includeNode,bool includeRenderNode)
    {
        var definitions = new List<(int,int,int,string,string,string,string,bool,string)>();
        if (includeNode)
        {
            int nodeSection = includeRenderNode ? 2 : 1;
            definitions.AddRange(new[]
            {
                (nodeSection,0,0,"node.position","Local position","vector3","three little-endian Single values",false,"Observed in every object."),
                (nodeSection,1,0,"node.rotation","Local rotation","quaternion","four little-endian Single values",false,"Optional inherited identity default; not observed."),
                (nodeSection,2,0,"node.scale","Local scale","vector3","three little-endian Single values",false,"Optional inherited unit default; not observed."),
                (nodeSection,3,0,"node.is_bone","Bone node","boolean","one byte 0 or 1",false,"Optional false default; not observed."),
                (nodeSection,4,0,"node.is_static","Static node","boolean","one byte 0 or 1",false,"Optional false default; not observed."),
                (nodeSection,5,-1,"node.child","Child node","object_relationship","one object relationship",true,includeRenderNode ? "Graph owns exactly all listed sets and portals." : "Not observed for portals."),
                (nodeSection,6,0,"node.billboard_axis","Billboard axis","uint32_enum","UInt32 axis 1 or 2",false,"Optional zero default; not observed."),
                (nodeSection,7,-1,"node.collision","Collision info","object_relationship","one object relationship",true,"Not observed."),
                (nodeSection,8,0,"node.is_animated","Animated node","boolean","one byte 0 or 1",false,"True in production; omitted/default false in PC test-world objects.")
            });
        }
        if (includeRenderNode)
            definitions.Add((1,0,-1,"render_node.renderable","Renderable","object_relationship","one object relationship",true,"Empty in every navigation graph."));
        definitions.AddRange(own);
        MeshNavigationFieldAnnotation[] observedFields = fields.ToArray();
        foreach (var definition in definitions)
        {
            int observed = observedFields.Count(item => item.Semantic == definition.Item4);
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',$section,$type,$occurrence,$semantic,
                       $display,$kind,$layout,'read_only_research',$evidence,$constraints,$notes);
                """);
            command.Parameters.AddWithValue("$hash",(long)typeHash);
            command.Parameters.AddWithValue("$section",definition.Item1);
            command.Parameters.AddWithValue("$type",definition.Item2);
            command.Parameters.AddWithValue("$occurrence",definition.Item3);
            command.Parameters.AddWithValue("$semantic",definition.Item4);
            command.Parameters.AddWithValue("$display",definition.Item5);
            command.Parameters.AddWithValue("$kind",definition.Item6);
            command.Parameters.AddWithValue("$layout",definition.Item7);
            command.Parameters.AddWithValue("$evidence",observed > 0
                ? "confirmed_both_executables_and_full_corpus"
                : "confirmed_both_executables_not_observed_corpus");
            command.Parameters.AddWithValue("$constraints",JsonSerializer.Serialize(new
            {
                observedDirectOccurrences=observed,repeated=definition.Item8,
                mutationStatus="coordinated_navigation_rebuild_required"
            }));
            command.Parameters.AddWithValue("$notes",definition.Item9);
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotateNavigationFields(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<(int FileId,int ObjectIndex,MeshNavigationFieldAnnotation Field)> fields)
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
        foreach (var item in fields)
        {
            command.Parameters["$semantic"].Value=item.Field.Semantic;
            command.Parameters["$layout"].Value=item.Field.Layout;
            command.Parameters["$value"].Value=item.Field.DecodedJson;
            command.Parameters["$file"].Value=item.FileId;
            command.Parameters["$object"].Value=item.ObjectIndex;
            command.Parameters["$field"].Value=item.Field.FieldIndex;
            if (command.ExecuteNonQuery()!=1)
                throw new InvalidDataException("Could not annotate navigation field.");
        }
    }

    private static Dictionary<string,int> InsertNavigationPortalVariants(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<NavigationPortalObservation> observations,string now)
    {
        var result = new Dictionary<string,int>();
        foreach (IGrouping<string,NavigationPortalObservation> group in observations.GroupBy(item =>
                     string.Join("_",item.Data.EndpointSets.Select(set => set.Encoding))))
        {
            string key = "navigation_portal_"+group.Key.ToLowerInvariant();
            result[group.Key] = InsertNavigationVariant(connection,transaction,typeHash,key,
                group.Key.Replace('_',' '),JsonSerializer.Serialize(new
                {
                    endpointEncodings=group.First().Data.EndpointSets.Select(set=>set.Encoding.ToString()),
                    objects=group.Count(),corpora=group.GroupBy(item=>item.CorpusKey)
                        .ToDictionary(item=>item.Key,item=>item.Count())
                }),"Relationship storage only; decoded portal semantics are identical.",now);
        }
        return result;
    }

    private static Dictionary<string,int> InsertNavigationGraphVariants(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<NavigationGraphObservation> observations,string now)
    {
        var result = new Dictionary<string,int>();
        foreach ((string key,bool animated,string display) in new[]
        {
            ("navigation_graph_production",true,"Production animated graph"),
            ("navigation_graph_pc_test",false,"PC test-world graph")
        })
        {
            NavigationGraphObservation[] members = observations.Where(item =>
                item.Data.RenderNode.Node.IsAnimated == animated).ToArray();
            result[key]=InsertNavigationVariant(connection,transaction,typeHash,key,display,
                JsonSerializer.Serialize(new { isAnimated=animated,objects=members.Length,
                    corpora=members.GroupBy(item=>item.CorpusKey)
                        .ToDictionary(item=>item.Key,item=>item.Count()) }),
                animated ? "Shared production form." :
                    "Only the two PC copies of Gardenia/test_world_navmesh.smo; inherited Animated is omitted/default false.",now);
        }
        return result;
    }

    private static int InsertNavigationVariant(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        string key,string display,string discriminator,string notes,string now)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,status,
                discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2',$key,$display,'confirmed',
                   $discriminator,$notes,$utc,$utc); SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$key",key);
        command.Parameters.AddWithValue("$display",display);
        command.Parameters.AddWithValue("$discriminator",discriminator);
        command.Parameters.AddWithValue("$notes",notes);
        command.Parameters.AddWithValue("$utc",now);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void AssignNavigationPortalVariants(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<NavigationPortalObservation> observations,
        IReadOnlyDictionary<string,int> variants)
    {
        foreach (NavigationPortalObservation item in observations)
        {
            string key=string.Join("_",item.Data.EndpointSets.Select(set=>set.Encoding));
            AssignNavigationVariant(connection,transaction,item.FileId,item.ObjectIndex,
                variants[key],"strict two-section decode and resolved endpoint storage");
        }
    }

    private static void AssignNavigationGraphVariants(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<NavigationGraphObservation> observations,
        IReadOnlyDictionary<string,int> variants)
    {
        foreach (NavigationGraphObservation item in observations)
        {
            string key=item.Data.RenderNode.Node.IsAnimated
                ? "navigation_graph_production" : "navigation_graph_pc_test";
            AssignNavigationVariant(connection,transaction,item.FileId,item.ObjectIndex,
                variants[key],"strict graph decode and complete route-topology validation");
        }
    }

    private static void AssignNavigationVariant(
        SqliteConnection connection,SqliteTransaction transaction,int fileId,
        int objectIndex,int variantId,string evidence)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',$evidence)
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.AddWithValue("$file",fileId);
        command.Parameters.AddWithValue("$object",objectIndex);
        command.Parameters.AddWithValue("$variant",variantId);
        command.Parameters.AddWithValue("$evidence",evidence);
        command.ExecuteNonQuery();
    }

    private static void UpdateNavigationClass(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        string category,string description,string notes)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            UPDATE classes SET category=$category,description=$description,
                decode_status='read_only_decode',notes=$notes WHERE type_hash=$hash;
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$category",category);
        command.Parameters.AddWithValue("$description",description);
        command.Parameters.AddWithValue("$notes",notes);
        command.ExecuteNonQuery();
    }

    private static string ObjectHash(SmoDocument document,SmoObjectEntry entry) =>
        Convert.ToHexString(SHA256.HashData(document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));

    private static void RequireCounts<T>(
        IEnumerable<T> items,Func<T,string> key,IReadOnlyDictionary<string,int> expected)
    {
        Dictionary<string,int> actual=items.GroupBy(key)
            .ToDictionary(group=>group.Key,group=>group.Count());
        if (actual.Count!=expected.Count || expected.Any(pair =>
                !actual.TryGetValue(pair.Key,out int count) || count!=pair.Value))
            throw new InvalidDataException("Navigation platform profile changed.");
    }

    private static NavigationPairing CompareNavigation<T>(
        IEnumerable<T> items,Func<T,string> corpus,Func<T,string> key,
        Func<T,string> semantic,Func<T,string> bytes,string leftName,string rightName)
    {
        Dictionary<string,T> left=items.Where(item=>corpus(item)==leftName).ToDictionary(key);
        Dictionary<string,T> right=items.Where(item=>corpus(item)==rightName).ToDictionary(key);
        string[] common=left.Keys.Intersect(right.Keys).ToArray();
        return new NavigationPairing(common.Length,
            common.Count(item=>semantic(left[item])==semantic(right[item])),
            common.Count(item=>bytes(left[item])==bytes(right[item])));
    }

    private sealed record NavigationPortalObservation(
        int FileId,int ObjectIndex,string CorpusKey,string PairingKey,
        SmoNavigationPortalData Data,string SerializedSha256,string SemanticSignature,
        IReadOnlyList<MeshNavigationFieldAnnotation> Fields);
    private sealed record NavigationGraphObservation(
        int FileId,int ObjectIndex,string CorpusKey,string PairingKey,
        SmoNavigationGraphData Data,string SerializedSha256,string SemanticSignature,
        IReadOnlyList<MeshNavigationFieldAnnotation> Fields);
    private sealed record NavigationPairing(int Paired,int EqualSemantic,int EqualBytes);
}
