using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private const string MeshNavigationInlineVariant =
        "mesh_navigation_set_inline_mesh";
    private const string MeshNavigationReferenceVariant =
        "mesh_navigation_set_sized_mesh_reference";

    private static SmoResearchClassAnalysisResult AnalyzeMeshNavigationSet(
        string databasePath,SmoResearchClassReport report)
    {
        HashSet<uint> parents = report.Relations
            .Where(item => item.Direction == "parent" && item.RelatedTypeHash.HasValue)
            .Select(item => item.RelatedTypeHash!.Value).ToHashSet();
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item => item.NamedObjectCount != item.UniqueObjectCount) ||
            !parents.SetEquals(
                [SmoClassIds.NavigationGraph,SmoClassIds.NavigationPortal]))
        {
            throw new InvalidDataException(
                "spMeshNavigationSet corpus no longer matches its profile or hierarchy.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly:false);
        List<MeshNavigationObservation> observations =
            LoadMeshNavigationObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        long fieldCount = observations.Sum(item => (long)item.Fields.Count);
        if (objectCount != 355 || observations.Count != objectCount ||
            fieldCount != 3254)
        {
            throw new InvalidDataException(
                $"Expected 355 navigation sets and 3254 semantic fields; got " +
                $"{observations.Count} and {fieldCount}.");
        }
        RequireMeshNavigationProfiles(observations);
        RequirePortalPairing(observations);
        MeshNavigationComparison pc = CompareMeshNavigation(
            observations,"pc-working","pc-pristine");
        MeshNavigationComparison cross = CompareMeshNavigation(
            observations,"pc-pristine","ps2-pristine");
        if (pc != new MeshNavigationComparison(27,119,119,119) ||
            cross != new MeshNavigationComparison(26,117,117,108))
        {
            throw new InvalidDataException(
                "spMeshNavigationSet PC-copy or PC/PS2 pairing changed: " +
                JsonSerializer.Serialize(new { pc,cross }));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spNavigationSet","spNavigationSetSerializer",
            "spMeshNavigationSet","spMeshNavigationSetSerializer",
            "esfNavSetNumNodes","esfNavSetTransTable",
            "esfNavSetPortalTransTable","esfNavSetLinksTable",
            "esfNavSetPortal","esfNavSetEnable","esfNavSetMesh"
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
                   WHERE type_hash=$hash
                     AND variant_key LIKE 'mesh_navigation_set_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }
        using (SqliteCommand clearPreliminaryDefinitions =
               CreateCommand(connection,transaction,"""
                   DELETE FROM field_definitions
                   WHERE type_hash=$hash AND scope_kind='platform'
                     AND semantic_key LIKE 'navigation_set.%';
                   """))
        {
            clearPreliminaryDefinitions.Parameters.AddWithValue(
                "$hash",(long)report.TypeHash);
            clearPreliminaryDefinitions.ExecuteNonQuery();
        }

        UpsertMeshNavigationFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateMeshNavigationFields(connection,transaction,observations);
        Dictionary<string,int> variants = InsertMeshNavigationVariants(
            connection,transaction,report.TypeHash,observations,now);
        AssignMeshNavigationVariants(
            connection,transaction,observations,variants);

        int totalLinks = observations.Sum(item => item.Data.LinkCount);
        int manualLinks = observations.Sum(item => item.Topology.ManualLinks);
        int omittedEdges = observations.Sum(item => item.Topology.OmittedSharedEdges);
        int portalRelationships = observations.Sum(item => item.Data.Portals.Count);
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variants[MeshNavigationInlineVariant],
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "base reader VA 0x00448800..0x00448E48; base writer VA " +
            "0x00447F00..0x004487C1; mesh reader VA 0x00448FE0..0x00449124; " +
            "mesh writer VA 0x00449130..0x004493AA",
            "The x86 serializer identifies spNavigationSet as class 0x74F9013E " +
            "derived from spNode and spMeshNavigationSet as 0x7297173C. Base " +
            "fields 0..5 are NodeCount, two UInt32 matrices, byte-addressed " +
            "links, repeated portal relationships and Enabled. The derived " +
            "field is one relationship to spMeshBV. Runtime members are " +
            "NodeCount +0xB4, matrices +0xB8/+0xBC, Enabled +0xE1 and mesh +0xE4.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variants[MeshNavigationInlineVariant],
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "mesh reader VA 0x0019A920..0x0019AA34; index VA " +
            "0x0019AA40..0x0019AA9C; mesh writer VA 0x0019AAA0..0x0019AC34; " +
            "base reader VA 0x0019E3C0..0x0019EAA8; base writer VA " +
            "0x0019EBE0..0x0019F37C",
            "The independent MIPS code returns the same class IDs and repeats " +
            "the exact field order and scalar widths. Links serialize UInt8 " +
            "node ID, UInt32 neighbour count and UInt8 neighbour IDs. Runtime " +
            "members are NodeCount +0xC0, matrices +0xC4/+0xC8, link arrays " +
            "+0xCC/+0xD0, portals +0xD4/+0xD8, Enabled +0xE9 and mesh +0xF0.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variants[MeshNavigationInlineVariant],
            null,null,"class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload and complete navigation/mesh relationship decode",
            $"All {objectCount} objects decode strictly with {fieldCount} annotated " +
            $"fields. NodeCount is 2..59 and always equals the linked spMeshBV " +
            $"triangle count. All {totalLinks} directed links are reciprocal. " +
            $"Every routing selector reaches every graph-reachable node; value 3 " +
            $"marks self/unreachable node cells and portal-route termination when " +
            $"it is outside the current node degree. The " +
            $"{portalRelationships} portal relationships resolve to " +
            $"{portalRelationships / 2} portals, each exactly once inline and " +
            $"once by sized reference. Geometry comparison finds {manualLinks} " +
            $"authored links without one exact shared edge and {omittedEdges} " +
            $"shared-edge directions intentionally absent from navigation.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variants[MeshNavigationInlineVariant],
            null,null,"class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus navigation-set ordinal",
            $"PC working/pristine: {pc.EqualBytes}/{pc.PairedObjects} exact and " +
            $"semantic matches. PC/PS2: {cross.EqualSemantic}/" +
            $"{cross.PairedObjects} navigation-semantic matches and " +
            $"{cross.EqualBytes}/{cross.PairedObjects} complete serialized-byte " +
            $"matches. The nine byte differences are relationship identity/" +
            $"embedded-descendant differences; matrices, links, placement, " +
            $"Enabled and target classes remain identical.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='navigation_mesh_set',
                       description='Placed triangle navigation graph with precomputed node/portal routing and spMeshBV geometry',
                       decode_status='read_only_decode',
                       notes='Complete PC/PS2 decode of inherited spNode, spNavigationSet fields 0..5 and derived mesh relationship. Non-terminal UInt32 matrix cells select an ordered outgoing link; out-of-degree value 3 is the terminal/unreachable marker. Explicit topology is authoritative: the corpus contains hand-authored cross-seam links and disabled geometric adjacencies.'
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
            report.Profiles.Sum(item => item.UniqueResourceCount),
            observations.Select(item => item.SerializedSha256).Distinct().Count(),
            observations.Count,evidenceRows,
            $"Two storage variants across {objectCount} objects: 351 inline " +
            $"spMeshBV relationships and four PC copies of two test-world sized " +
            $"references. All node and portal routing semantics are shared by PC " +
            $"and PS2; editing remains disabled pending coordinated graph rebuild.");
    }

    private static List<MeshNavigationObservation> LoadMeshNavigationObservations(
        SqliteConnection connection)
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.MeshNavigationSet);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<MeshNavigationFileLocation>();
        while (reader.Read())
        {
            locations.Add(new MeshNavigationFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<MeshNavigationObservation>();
        foreach (MeshNavigationFileLocation location in locations)
        {
            byte[] bytes = ReadMeshNavigationResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.MeshNavigationSet))
            {
                if (!SmoMeshNavigationSetDecoder.TryDecode(
                        document,entry,out SmoMeshNavigationSetData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                SmoObjectEntry meshEntry = document.Objects[
                    decoded.NavigationMesh.TargetObjectIndex!.Value];
                if (!SmoMeshBoundingVolumeDecoder.TryDecode(
                        document,meshEntry,out SmoMeshBoundingVolumeData? mesh,out _) ||
                    mesh is null)
                {
                    throw new InvalidDataException("Navigation mesh target did not decode.");
                }
                result.Add(CreateMeshNavigationObservation(
                    location,document,entry,ordinal++,decoded,mesh));
            }
        }
        return result;
    }

    private static byte[] ReadMeshNavigationResource(
        MeshNavigationFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
        {
            return File.ReadAllBytes(Path.Combine(location.SourceRoot,
                location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        }
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
        {
            throw new InvalidDataException("PCK navigation occurrence is incomplete.");
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

    private static MeshNavigationObservation CreateMeshNavigationObservation(
        MeshNavigationFileLocation location,SmoDocument document,
        SmoObjectEntry entry,int ordinal,SmoMeshNavigationSetData decoded,
        SmoMeshBoundingVolumeData mesh)
    {
        IReadOnlyList<SmoObjectField> direct = SmoObjectFieldReader.Read(document,entry);
        var annotations = new List<MeshNavigationFieldAnnotation>();
        int portalIndex = 0;
        foreach ((SmoObjectField field,int index) in direct.Select((field,index) =>
                     (field,index)))
        {
            if (field.FieldType == 0 && field.PayloadSize == 0)
                continue;
            if (!SmoSerializedFieldRegistry.TryDescribeField(
                    entry.TypeHash,direct,index,out var descriptor) || descriptor is null)
            {
                throw new InvalidDataException(
                    "Unregistered spMeshNavigationSet direct field.");
            }
            object decodedValue = descriptor.Key switch
            {
                "node.position" => new
                {
                    decoded.Node.Position.X,decoded.Node.Position.Y,
                    decoded.Node.Position.Z
                },
                "node.is_animated" => decoded.Node.IsAnimated,
                "navigation_set.node_count" => decoded.NodeCount,
                "navigation_set.transition_table" => MatrixValue(
                    decoded.NodeTransitions),
                "navigation_set.portal_transition_table" => MatrixValue(
                    decoded.PortalTransitions),
                "navigation_set.links" => new
                {
                    NodeCount=decoded.Links.Count,
                    LinkCount=decoded.LinkCount,
                    Reciprocal=decoded.HasReciprocalLinks,
                    Nodes=decoded.Links.Select(item => new
                    {
                        item.NodeId,Neighbours=item.NeighbourNodeIds
                    })
                },
                "navigation_set.portal" => RelationshipValue(
                    decoded.Portals[portalIndex++]),
                "navigation_set.enabled" => decoded.Enabled,
                "navigation_set.mesh" => RelationshipValue(decoded.NavigationMesh),
                _ => throw new InvalidDataException(
                    $"Unexpected navigation semantic {descriptor.Key}.")
            };
            annotations.Add(new MeshNavigationFieldAnnotation(
                index,descriptor.Key,descriptor.PayloadLayout,
                JsonSerializer.Serialize(decodedValue)));
        }
        if (portalIndex != decoded.Portals.Count)
            throw new InvalidDataException("Navigation portal annotation count changed.");

        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        string semantic = JsonSerializer.Serialize(new
        {
            Position=new[] { decoded.Node.Position.X,decoded.Node.Position.Y,
                decoded.Node.Position.Z },
            decoded.Node.IsAnimated,decoded.NodeCount,
            NodeTransitions=decoded.NodeTransitions.LinkSelectors,
            PortalDimensions=new[] { decoded.PortalTransitions.RowCount,
                decoded.PortalTransitions.ColumnCount },
            PortalTransitions=decoded.PortalTransitions.LinkSelectors,
            Links=decoded.Links.Select(item => new
                { item.NodeId,item.NeighbourNodeIds }),
            PortalCount=decoded.Portals.Count,decoded.Enabled,
            MeshType=decoded.NavigationMesh.TargetTypeHash
        });
        return new MeshNavigationObservation(
            location.FileId,entry.Index,entry.Id,ordinal,location.CorpusKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            entry.Name.TrimEnd('\0'),entry.SerializedSize,decoded,
            ClassifyMeshTopology(decoded,mesh),serializedHash,semantic,
            annotations.AsReadOnly());
    }

    private static object MatrixValue(SmoNavigationTransitionTable table) => new
    {
        table.RowCount,table.ColumnCount,Selectors=table.LinkSelectors,
        Histogram=table.LinkSelectors.GroupBy(item => item).OrderBy(item => item.Key)
            .ToDictionary(item => item.Key.ToString(),item => item.Count())
    };

    private static object RelationshipValue(SmoNodeRelationship relationship) => new
    {
        relationship.ObjectId,relationship.InlineSerializedSize,
        Encoding=relationship.Encoding.ToString(),relationship.TargetObjectIndex,
        relationship.TargetTypeHash,relationship.TargetName
    };

    private static MeshNavigationTopology ClassifyMeshTopology(
        SmoMeshNavigationSetData navigation,SmoMeshBoundingVolumeData mesh)
    {
        var edgeOwners = new Dictionary<MeshEdge,List<int>>();
        for (int triangle = 0;triangle < mesh.TriangleCount;triangle++)
        {
            int[] indices =
            [
                mesh.TriangleIndices[triangle * 3],
                mesh.TriangleIndices[triangle * 3 + 1],
                mesh.TriangleIndices[triangle * 3 + 2]
            ];
            for (int edge = 0;edge < 3;edge++)
            {
                MeshEdge key = MeshEdge.Create(
                    mesh.Positions[indices[edge]],mesh.Positions[indices[(edge + 1) % 3]]);
                if (!edgeOwners.TryGetValue(key,out List<int>? owners))
                    edgeOwners[key] = owners = [];
                owners.Add(triangle);
            }
        }
        var geometric = Enumerable.Range(0,mesh.TriangleCount)
            .Select(_ => new HashSet<int>()).ToArray();
        foreach (List<int> owners in edgeOwners.Values.Where(item => item.Count == 2))
        {
            geometric[owners[0]].Add(owners[1]);
            geometric[owners[1]].Add(owners[0]);
        }
        int shared = 0;
        int manual = 0;
        int omitted = 0;
        foreach (SmoNavigationNodeLinks node in navigation.Links)
        {
            HashSet<int> links = node.NeighbourNodeIds.Select(item => (int)item)
                .ToHashSet();
            shared += links.Intersect(geometric[node.NodeId]).Count();
            manual += links.Except(geometric[node.NodeId]).Count();
            omitted += geometric[node.NodeId].Except(links).Count();
        }
        return new MeshNavigationTopology(shared,manual,omitted);
    }

    private static void RequireMeshNavigationProfiles(
        IReadOnlyList<MeshNavigationObservation> observations)
    {
        Dictionary<string,int> expected = new()
        {
            ["pc-working"] = 119,["pc-pristine"] = 119,["ps2-pristine"] = 117
        };
        foreach (IGrouping<string,MeshNavigationObservation> group in
                 observations.GroupBy(item => item.CorpusKey))
        {
            if (!expected.TryGetValue(group.Key,out int count) || group.Count() != count ||
                group.Min(item => item.Data.NodeCount) != 2 ||
                group.Max(item => item.Data.NodeCount) != 59 ||
                group.Any(item => !item.Data.Enabled ||
                                  !item.Data.HasReciprocalLinks))
            {
                throw new InvalidDataException(
                    $"spMeshNavigationSet profile changed for {group.Key}.");
            }
        }
        int inline = observations.Count(item =>
            item.Data.NavigationMesh.Encoding == SmoNodeRelationshipEncoding.InlineObject);
        int references = observations.Count(item =>
            item.Data.NavigationMesh.Encoding == SmoNodeRelationshipEncoding.SizedReference);
        if (inline != 351 || references != 4 ||
            observations.Where(item =>
                    item.Data.NavigationMesh.Encoding ==
                    SmoNodeRelationshipEncoding.SizedReference)
                .Any(item => item.CorpusKey == "ps2-pristine" ||
                             !item.CanonicalPath.EndsWith(
                                 "levels/gardenia/test_world_navmesh.smo",
                                 StringComparison.Ordinal)))
        {
            throw new InvalidDataException(
                "Navigation mesh relationship storage profile changed.");
        }
    }

    private static void RequirePortalPairing(
        IReadOnlyList<MeshNavigationObservation> observations)
    {
        var occurrences = observations.SelectMany(item => item.Data.Portals.Select(portal =>
            new { item.FileId,portal.ObjectId,portal.Encoding })).ToArray();
        foreach (var group in occurrences.GroupBy(item => (item.FileId,item.ObjectId)))
        {
            if (group.Count() != 2 ||
                group.Count(item => item.Encoding ==
                    SmoNodeRelationshipEncoding.InlineObject) != 1 ||
                group.Count(item => item.Encoding ==
                    SmoNodeRelationshipEncoding.SizedReference) != 1)
            {
                throw new InvalidDataException(
                    "Each navigation portal must be inline-owned once and referenced once.");
            }
        }
    }

    private static MeshNavigationComparison CompareMeshNavigation(
        IEnumerable<MeshNavigationObservation> observations,string leftCorpus,
        string rightCorpus)
    {
        Dictionary<(string Path,int Ordinal),MeshNavigationObservation> Build(
            string corpus) => observations.Where(item => item.CorpusKey == corpus)
            .ToDictionary(item => (item.CanonicalPath,item.Ordinal));
        var left = Build(leftCorpus);
        var right = Build(rightCorpus);
        var keys = left.Keys.Intersect(right.Keys).ToArray();
        return new MeshNavigationComparison(
            keys.Select(item => item.Path).Distinct().Count(),keys.Length,
            keys.Count(key => left[key].SemanticSignature ==
                              right[key].SemanticSignature),
            keys.Count(key => left[key].SerializedSha256 ==
                              right[key].SerializedSha256));
    }

    private static void UpsertMeshNavigationFieldDefinitions(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<MeshNavigationObservation> observations)
    {
        (int Section,int Type,int Occurrence,string Semantic,string Display,
            string Kind,string Layout,bool Repeated,string Evidence,string Notes)[] defs =
        [
            (2,0,0,"node.position","Local position","vector3","three little-endian Single values",false,"confirmed_both_executables_and_full_corpus","Observed in every navigation set."),
            (2,1,0,"node.rotation","Local rotation","quaternion","four little-endian Single values X/Y/Z/W",false,"confirmed_both_executables_not_observed_corpus","Inherited optional identity default."),
            (2,2,0,"node.scale","Local scale","vector3","three little-endian Single values",false,"confirmed_both_executables_not_observed_corpus","Inherited optional (1,1,1) default."),
            (2,3,0,"node.is_bone","Bone node","boolean","one byte 0 or 1",false,"confirmed_both_executables_not_observed_corpus","Inherited false default."),
            (2,4,0,"node.is_static","Static node","boolean","one byte 0 or 1",false,"confirmed_both_executables_not_observed_corpus","Inherited false default."),
            (2,5,-1,"node.child","Child node","object_relationship","node child relationship",true,"confirmed_both_executables_not_observed_corpus","No direct node-child fields observed."),
            (2,6,0,"node.billboard_axis","Billboard axis","uint32_enum","UInt32 axis 1 or 2",false,"confirmed_both_executables_not_observed_corpus","Inherited default 0."),
            (2,7,-1,"node.collision","Collision info","object_relationship","node collision relationship",true,"confirmed_both_executables_not_observed_corpus","No inherited collision fields observed."),
            (2,8,0,"node.is_animated","Animated node","boolean","one byte 0 or 1",false,"confirmed_both_executables_and_full_corpus","True in all production resources; omitted/default false only in two PC test-world objects."),
            (1,0,0,"navigation_set.node_count","Navigation node count","uint32","little-endian UInt32",false,"confirmed_both_executables_and_full_corpus","Required; 2..59 and equals spMeshBV triangle count."),
            (1,1,0,"navigation_set.transition_table","Node transition table","uint32_matrix","UInt32 rows/columns followed by row-major UInt32 outgoing-link selectors or terminal marker 3",false,"confirmed_both_executables_and_full_corpus","Required NodeCount x NodeCount; routes every graph-reachable pair. Diagonal and unreachable cells are 3."),
            (1,2,0,"navigation_set.portal_transition_table","Portal transition table","uint32_matrix","UInt32 portal/node dimensions followed by row-major UInt32 outgoing-link selectors or terminal marker 3",false,"confirmed_both_executables_and_full_corpus","Required PortalCount x NodeCount; following a row from every node terminates at out-of-degree value 3 without cycles."),
            (1,3,0,"navigation_set.links","Navigation links","byte_adjacency","UInt32 node count; repeated UInt8 node ID, UInt32 link count, UInt8 neighbour IDs",false,"confirmed_both_executables_and_full_corpus","Required dense node order; every observed directed link is reciprocal."),
            (1,4,-1,"navigation_set.portal","Navigation portal","object_relationship","one spNavigationPortal relationship per occurrence",true,"confirmed_both_executables_and_full_corpus","Optional/repeated; each portal appears inline once and as a sized reference once."),
            (1,5,0,"navigation_set.enabled","Navigation enabled","boolean","one byte 0 or 1",false,"confirmed_both_executables_and_full_corpus","Required and true in every observed object."),
            (0,0,0,"navigation_set.mesh","Navigation mesh","object_relationship","one relationship to spMeshBV",false,"confirmed_both_executables_and_full_corpus","Required; inline in 351 objects, sized reference in four PC test-world copies.")
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
                mutationStatus="coordinated_graph_rebuild_required"
            }));
            command.Parameters.AddWithValue("$notes",definition.Notes);
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotateMeshNavigationFields(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<MeshNavigationObservation> observations)
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
        foreach (MeshNavigationObservation observation in observations)
        foreach (MeshNavigationFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException(
                    "Could not annotate spMeshNavigationSet field.");
        }
    }

    private static Dictionary<string,int> InsertMeshNavigationVariants(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<MeshNavigationObservation> observations,string now)
    {
        var result = new Dictionary<string,int>();
        foreach ((string key,string display,SmoNodeRelationshipEncoding encoding) in new[]
        {
            (MeshNavigationInlineVariant,"Inline navigation mesh",
                SmoNodeRelationshipEncoding.InlineObject),
            (MeshNavigationReferenceVariant,"Referenced test-world navigation mesh",
                SmoNodeRelationshipEncoding.SizedReference)
        })
        {
            MeshNavigationObservation[] members = observations.Where(item =>
                item.Data.NavigationMesh.Encoding == encoding).ToArray();
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO class_variants(
                    type_hash,scope_kind,scope_key,variant_key,display_name,status,
                    discriminator_json,notes,created_utc,updated_utc)
                VALUES($hash,$scope_kind,$scope_key,$key,$display,'confirmed',
                       $discriminator,$notes,$utc,$utc);
                SELECT last_insert_rowid();
                """);
            bool common = encoding == SmoNodeRelationshipEncoding.InlineObject;
            command.Parameters.AddWithValue("$hash",(long)typeHash);
            command.Parameters.AddWithValue("$scope_kind",common ? "common" : "platform");
            command.Parameters.AddWithValue("$scope_key",common ? "pc_ps2" : "pc");
            command.Parameters.AddWithValue("$key",key);
            command.Parameters.AddWithValue("$display",display);
            command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
            {
                meshRelationshipEncoding=encoding.ToString(),objects=members.Length,
                corpora=members.GroupBy(item => item.CorpusKey)
                    .ToDictionary(item => item.Key,item => item.Count()),
                sections=new[] { "spNode","spNavigationSet","spMeshNavigationSet" },
                baseFields=new[] { "node_count","transition_table",
                    "portal_transition_table","links","portal*","enabled" }
            }));
            command.Parameters.AddWithValue("$notes",common
                ? "Production layout. The spMeshBV is physically inline-owned by the navigation set."
                : "Only nav_01/nav_02 in the PC-only Gardenia test_world_navmesh.smo; the spMeshBV is a sized reference and inherited IsAnimated is omitted/default false.");
            command.Parameters.AddWithValue("$utc",now);
            result[key] = Convert.ToInt32(command.ExecuteScalar());
        }
        return result;
    }

    private static void AssignMeshNavigationVariants(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<MeshNavigationObservation> observations,
        IReadOnlyDictionary<string,int> variants)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',$evidence)
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.Add("$variant",SqliteType.Integer);
        command.Parameters.Add("$evidence",SqliteType.Text);
        foreach (MeshNavigationObservation observation in observations)
        {
            string key = observation.Data.NavigationMesh.Encoding ==
                         SmoNodeRelationshipEncoding.InlineObject
                ? MeshNavigationInlineVariant : MeshNavigationReferenceVariant;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$variant"].Value = variants[key];
            command.Parameters["$evidence"].Value =
                "strict three-section decode and resolved spMeshBV relationship";
            command.ExecuteNonQuery();
        }
    }

    private readonly record struct MeshVertex(int X,int Y,int Z)
    {
        public static MeshVertex Create(Vector3 value) => new(
            BitConverter.SingleToInt32Bits(value.X),
            BitConverter.SingleToInt32Bits(value.Y),
            BitConverter.SingleToInt32Bits(value.Z));

        public static int Compare(MeshVertex left,MeshVertex right)
        {
            int result = left.X.CompareTo(right.X);
            if (result != 0) return result;
            result = left.Y.CompareTo(right.Y);
            return result != 0 ? result : left.Z.CompareTo(right.Z);
        }
    }

    private readonly record struct MeshEdge(MeshVertex A,MeshVertex B)
    {
        public static MeshEdge Create(Vector3 a,Vector3 b)
        {
            MeshVertex left = MeshVertex.Create(a);
            MeshVertex right = MeshVertex.Create(b);
            return MeshVertex.Compare(left,right) <= 0
                ? new MeshEdge(left,right) : new MeshEdge(right,left);
        }
    }

    private sealed record MeshNavigationFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record MeshNavigationFieldAnnotation(
        int FieldIndex,string Semantic,string Layout,string DecodedJson);
    private sealed record MeshNavigationTopology(
        int SharedEdgeLinks,int ManualLinks,int OmittedSharedEdges);
    private sealed record MeshNavigationObservation(
        int FileId,int ObjectIndex,uint ObjectId,int Ordinal,string CorpusKey,
        string CanonicalPath,string Name,long SerializedSize,
        SmoMeshNavigationSetData Data,MeshNavigationTopology Topology,
        string SerializedSha256,string SemanticSignature,
        IReadOnlyList<MeshNavigationFieldAnnotation> Fields);
    private sealed record MeshNavigationComparison(
        int CommonResources,int PairedObjects,int EqualSemantic,int EqualBytes);
}
