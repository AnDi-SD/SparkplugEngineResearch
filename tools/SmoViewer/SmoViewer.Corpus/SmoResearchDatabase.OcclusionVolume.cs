using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private const string OcclusionVolumeVariantKey =
        "visibility_occluder_with_portable_geometry";

    private static SmoResearchClassAnalysisResult AnalyzeOcclusionVolume(
        string databasePath,SmoResearchClassReport report)
    {
        HashSet<uint> parents = report.Relations
            .Where(item => item.Direction == "parent" && item.RelatedTypeHash.HasValue)
            .Select(item => item.RelatedTypeHash!.Value).ToHashSet();
        HashSet<uint> children = report.Relations
            .Where(item => item.Direction == "child" && item.RelatedTypeHash.HasValue)
            .Select(item => item.RelatedTypeHash!.Value).ToHashSet();
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item => item.NamedObjectCount != item.UniqueObjectCount) ||
            !parents.SetEquals([SmoClassIds.Node]) || children.Count != 0)
        {
            throw new InvalidDataException(
                "spOcclusionVolume corpus no longer matches its physical hierarchy.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly:false);
        List<OcclusionVolumeObservation> observations =
            LoadOcclusionVolumeObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        long fieldCount = observations.Sum(item => (long)item.Fields.Count);
        if (objectCount != 60 || observations.Count != objectCount ||
            fieldCount != 282)
        {
            throw new InvalidDataException(
                $"Expected 60 occluders and 282 semantic fields; got " +
                $"{observations.Count} and {fieldCount}.");
        }
        RequireOcclusionVolumeProfiles(observations);
        OcclusionVolumeComparison pc = CompareOcclusionVolumes(
            observations,"pc-working","pc-pristine");
        OcclusionVolumeComparison cross = CompareOcclusionVolumes(
            observations,"pc-pristine","ps2-pristine");
        if (pc != new OcclusionVolumeComparison(8,20,20,20) ||
            cross != new OcclusionVolumeComparison(8,20,20,20))
        {
            throw new InvalidDataException(
                "spOcclusionVolume PC-copy or PC/PS2 pairing changed: " +
                JsonSerializer.Serialize(new { pc,cross }));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spOcclusionVolume","spOcclusionVolumeSerializer",
            "esfOcclusionVolumeIndexBuffer",
            "esfOcclusionVolumeVertexBuffer",
            "must be either a closed volume or planar"
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
            clearVariants.Parameters.AddWithValue("$key",OcclusionVolumeVariantKey);
            clearVariants.ExecuteNonQuery();
        }

        UpsertOcclusionVolumeFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateOcclusionVolumeFields(connection,transaction,observations);
        int variantId = InsertOcclusionVolumeVariant(
            connection,transaction,report.TypeHash,observations,now);
        AssignOcclusionVolumeVariant(
            connection,transaction,observations,variantId);

        float maximumPlanarityError = observations.Max(
            item => item.Geometry.MaximumPlanarityError);
        double minimumArea = observations.Min(item => item.Geometry.SurfaceArea);
        double maximumArea = observations.Max(item => item.Geometry.SurfaceArea);
        int strictlyConvex = observations.Count(item => item.Geometry.StrictlyConvex);
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "reader VA 0x0044F400..0x0044F682; writer VA 0x0044F690..0x0044F9B3",
            "The x86 serializer delegates section 0 to spNodeSerializer, then " +
            "requires field 0 IndexBuffer and field 1 VertexBuffer. Runtime " +
            "buffer pointers are +0xD0 and +0xC8 respectively. Shape validation " +
            "requires convex geometry that is either closed or planar.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "reader VA 0x001A00B0..0x001A03F8; writer VA 0x001A0410..0x001A0630",
            "The independent MIPS serializer confirms the same two required " +
            "fields and generic buffer members; runtime VertexBuffer/IndexBuffer " +
            "pointers are +0xD0/+0xD8. It repeats the same convex closed-or-planar " +
            "shape validation.",ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload, complete buffer decode and topology validation",
            $"All {objectCount} named instances have two serializer sections, " +
            $"triangle-list/UInt16 index buffers, position-only vertex buffers, " +
            $"and {fieldCount} annotated fields. Every observed mesh is a " +
            $"connected planar disk; {strictlyConvex}/{objectCount} are strictly " +
            $"convex. The remaining six are the same slightly indented decagon " +
            $"reused twice per corpus. Maximum plane error is " +
            $"{maximumPlanarityError:G9}, area range {minimumArea:G9}.." +
            $"{maximumArea:G9}.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus occluder ordinal",
            $"PC working/pristine: {pc.EqualBytes}/{pc.PairedObjects} byte-identical " +
            $"and semantic matches. PC/PS2: {cross.EqualBytes}/" +
            $"{cross.PairedObjects} byte-identical and semantic matches, including " +
            "node transforms, all indices and every vertex.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='visibility_occluder',
                       description='Placed visibility occluder with portable planar or closed triangle geometry',
                       decode_status='read_only_decode',
                       notes='Strict PC/PS2 decode: inherited spNode plus required triangle-list UInt16 IndexBuffer and position-only VertexBuffer. The executable validation requests convex closed or planar shapes. All 60 corpus objects are planar disks; 54 are strictly convex and six copies of one authored decagon have a shallow 11.83-unit indentation.'
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
            $"One portable visibility-occluder layout across {objectCount} objects. " +
            "Observed polygon cardinalities are 4/2, 5/3, 6/4 and 10/8 " +
            "vertices/triangles; 54 are strictly convex and six repeated " +
            "decagons are slightly indented. PC and PS2 are byte-identical for every pair.");
    }

    private static List<OcclusionVolumeObservation>
        LoadOcclusionVolumeObservations(SqliteConnection connection)
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.OcclusionVolume);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<OcclusionVolumeFileLocation>();
        while (reader.Read())
        {
            locations.Add(new OcclusionVolumeFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<OcclusionVolumeObservation>();
        foreach (OcclusionVolumeFileLocation location in locations)
        {
            byte[] bytes = ReadOcclusionVolumeResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.OcclusionVolume))
            {
                if (!SmoOcclusionVolumeDecoder.TryDecode(
                        document,entry,out SmoOcclusionVolumeData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                if (entry.ParentIndex is not int parentIndex ||
                    document.Objects[parentIndex].TypeHash != SmoClassIds.Node ||
                    !document.Objects[parentIndex].Name.TrimEnd('\0').Equals(
                        "Scene Root",StringComparison.Ordinal))
                {
                    throw new InvalidDataException(
                        "spOcclusionVolume must be physically owned by Scene Root spNode.");
                }
                OcclusionVolumeGeometry geometry =
                    ValidateOcclusionVolumeGeometry(decoded);
                result.Add(CreateOcclusionVolumeObservation(
                    location,document,entry,ordinal++,decoded,geometry));
            }
        }
        return result;
    }

    private static byte[] ReadOcclusionVolumeResource(
        OcclusionVolumeFileLocation location)
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
                "PCK occlusion-volume occurrence is incomplete.");
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

    private static OcclusionVolumeObservation CreateOcclusionVolumeObservation(
        OcclusionVolumeFileLocation location,SmoDocument document,
        SmoObjectEntry entry,int ordinal,SmoOcclusionVolumeData decoded,
        OcclusionVolumeGeometry geometry)
    {
        IReadOnlyList<SmoObjectField> direct =
            SmoObjectFieldReader.Read(document,entry);
        var fields = new List<OcclusionVolumeFieldAnnotation>(5);
        var signature = new List<string>(5) { entry.Name.TrimEnd('\0') };
        foreach ((SmoObjectField field,int index) in direct.Select((field,index) =>
                     (field,index)))
        {
            if (field.FieldType == 0 && field.PayloadSize == 0)
                continue;
            if (!SmoSerializedFieldRegistry.TryDescribeField(
                    entry.TypeHash,direct,index,
                    out SmoSerializedFieldDescriptor? descriptor) || descriptor is null)
            {
                throw new InvalidDataException(
                    "Unregistered spOcclusionVolume direct field.");
            }
            string decodedJson = descriptor.Key switch
            {
                "node.position" => JsonSerializer.Serialize(new
                {
                    decoded.Node.Position.X,decoded.Node.Position.Y,
                    decoded.Node.Position.Z
                }),
                "node.rotation" => JsonSerializer.Serialize(new
                {
                    decoded.Node.Rotation.X,decoded.Node.Rotation.Y,
                    decoded.Node.Rotation.Z,decoded.Node.Rotation.W
                }),
                "node.is_animated" => JsonSerializer.Serialize(
                    decoded.Node.IsAnimated),
                "occlusion_volume.index_buffer" => JsonSerializer.Serialize(new
                {
                    decoded.IndexBuffer.PrimitiveType,
                    decoded.IndexBuffer.PrimitiveCount,
                    decoded.IndexBuffer.IndexFormat,
                    Indices=decoded.IndexBuffer.TriangleIndices
                }),
                "occlusion_volume.vertex_buffer" => JsonSerializer.Serialize(new
                {
                    decoded.VertexBuffer.VertexDeclaration,
                    decoded.VertexBuffer.VertexCount,decoded.VertexBuffer.Flags,
                    Positions=decoded.VertexBuffer.Positions.Select(value =>
                        new { value.X,value.Y,value.Z })
                }),
                _ => throw new InvalidDataException(
                    "Unexpected spOcclusionVolume semantic field.")
            };
            fields.Add(new OcclusionVolumeFieldAnnotation(
                index,descriptor.Key,descriptor.PayloadLayout,decodedJson));
            signature.Add(descriptor.Key + ":" +
                          Convert.ToHexString(field.Payload.Span));
        }
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        return new OcclusionVolumeObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            entry.Name.TrimEnd('\0'),entry.SerializedSize,decoded,geometry,
            serializedHash,string.Join("|",signature),fields.AsReadOnly());
    }

    private static OcclusionVolumeGeometry ValidateOcclusionVolumeGeometry(
        SmoOcclusionVolumeData decoded)
    {
        IReadOnlyList<Vector3> vertices = decoded.VertexBuffer.Positions;
        IReadOnlyList<ushort> indices = decoded.IndexBuffer.TriangleIndices;
        int triangleCount = checked((int)decoded.IndexBuffer.PrimitiveCount);
        var edges = new Dictionary<(int A,int B),List<int>>();
        var normals = new List<Vector3>(triangleCount);
        double area = 0;
        for (int triangle = 0;triangle < triangleCount;triangle++)
        {
            int a = indices[triangle * 3];
            int b = indices[triangle * 3 + 1];
            int c = indices[triangle * 3 + 2];
            Vector3 cross = Vector3.Cross(vertices[b] - vertices[a],
                vertices[c] - vertices[a]);
            float magnitude = cross.Length();
            if (magnitude <= 0)
                throw new InvalidDataException("Degenerate occlusion triangle.");
            normals.Add(cross / magnitude);
            area += magnitude * 0.5;
            AddEdge(a,b,triangle);
            AddEdge(b,c,triangle);
            AddEdge(c,a,triangle);
        }
        void AddEdge(int left,int right,int triangle)
        {
            var key = left < right ? (left,right) : (right,left);
            if (!edges.TryGetValue(key,out List<int>? owners))
                edges.Add(key,owners=[]);
            owners.Add(triangle);
        }

        Vector3 normal = normals[0];
        if (normals.Skip(1).Any(item => Vector3.Dot(normal,item) < 0.9999f))
            throw new InvalidDataException(
                "Occlusion triangles have inconsistent winding or normals.");
        float maximumPlanarityError = vertices.Max(vertex =>
            MathF.Abs(Vector3.Dot(normal,vertex - vertices[indices[0]])));
        int boundaryEdges = edges.Count(item => item.Value.Count == 1);
        int internalEdges = edges.Count(item => item.Value.Count == 2);
        if (maximumPlanarityError > 0.001f ||
            edges.Any(item => item.Value.Count is < 1 or > 2) ||
            triangleCount != vertices.Count - 2 ||
            boundaryEdges != vertices.Count || internalEdges != triangleCount - 1)
        {
            throw new InvalidDataException(
                "Corpus occlusion geometry is no longer a planar triangle disk.");
        }

        var triangleLinks = Enumerable.Range(0,triangleCount)
            .Select(_ => new HashSet<int>()).ToArray();
        foreach (List<int> owners in edges.Values.Where(item => item.Count == 2))
        {
            triangleLinks[owners[0]].Add(owners[1]);
            triangleLinks[owners[1]].Add(owners[0]);
        }
        var visitedTriangles = new HashSet<int> { 0 };
        var pending = new Queue<int>();
        pending.Enqueue(0);
        while (pending.TryDequeue(out int triangle))
        foreach (int linked in triangleLinks[triangle])
        {
            if (visitedTriangles.Add(linked))
                pending.Enqueue(linked);
        }
        if (visitedTriangles.Count != triangleCount)
            throw new InvalidDataException("Occlusion triangle disk is disconnected.");

        var boundary = new Dictionary<int,List<int>>();
        foreach ((int a,int b) in edges.Where(item => item.Value.Count == 1)
                     .Select(item => item.Key))
        {
            if (!boundary.TryGetValue(a,out List<int>? aNeighbours))
                boundary.Add(a,aNeighbours=[]);
            if (!boundary.TryGetValue(b,out List<int>? bNeighbours))
                boundary.Add(b,bNeighbours=[]);
            aNeighbours.Add(b);
            bNeighbours.Add(a);
        }
        if (boundary.Count != vertices.Count ||
            boundary.Any(item => item.Value.Count != 2))
            throw new InvalidDataException("Occlusion boundary is not one polygon.");
        var cycle = new List<int>(vertices.Count);
        int start = boundary.Keys.Min();
        int previous = -1;
        int current = start;
        do
        {
            if (cycle.Contains(current))
                throw new InvalidDataException("Occlusion boundary repeats a vertex.");
            cycle.Add(current);
            List<int> neighbours = boundary[current];
            int next = neighbours[0] != previous ? neighbours[0] : neighbours[1];
            previous = current;
            current = next;
        } while (current != start && cycle.Count <= vertices.Count);
        if (current != start || cycle.Count != vertices.Count)
            throw new InvalidDataException("Occlusion boundary is not one cycle.");

        int dropAxis = MathF.Abs(normal.X) >= MathF.Abs(normal.Y)
            ? (MathF.Abs(normal.X) >= MathF.Abs(normal.Z) ? 0 : 2)
            : (MathF.Abs(normal.Y) >= MathF.Abs(normal.Z) ? 1 : 2);
        static Vector2 Project(Vector3 value,int drop) => drop switch
        {
            0 => new Vector2(value.Y,value.Z),
            1 => new Vector2(value.X,value.Z),
            _ => new Vector2(value.X,value.Y)
        };
        float turnSign = 0;
        bool strictlyConvex = true;
        for (int index = 0;index < cycle.Count;index++)
        {
            Vector2 a = Project(vertices[cycle[index]],dropAxis);
            Vector2 b = Project(vertices[cycle[(index + 1) % cycle.Count]],dropAxis);
            Vector2 c = Project(vertices[cycle[(index + 2) % cycle.Count]],dropAxis);
            float turn = (b.X - a.X) * (c.Y - b.Y) -
                         (b.Y - a.Y) * (c.X - b.X);
            if (MathF.Abs(turn) <= 0.0001f)
            {
                strictlyConvex = false;
                continue;
            }
            float sign = MathF.Sign(turn);
            if (turnSign == 0)
                turnSign = sign;
            else if (sign != turnSign)
                strictlyConvex = false;
        }
        return new OcclusionVolumeGeometry(
            maximumPlanarityError,area,boundaryEdges,internalEdges,strictlyConvex);
    }

    private static void RequireOcclusionVolumeProfiles(
        IReadOnlyList<OcclusionVolumeObservation> observations)
    {
        var expectedCardinality = new Dictionary<(int Vertices,int Triangles),int>
        {
            [(4,2)]=16,[(5,3)]=1,[(6,4)]=1,[(10,8)]=2
        };
        foreach (IGrouping<string,OcclusionVolumeObservation> group in
                 observations.GroupBy(item => item.CorpusKey))
        {
            Dictionary<(int Vertices,int Triangles),int> cardinality = group
                .GroupBy(item => (
                    item.Data.VertexBuffer.Positions.Count,
                    checked((int)item.Data.IndexBuffer.PrimitiveCount)))
                .ToDictionary(item => item.Key,item => item.Count());
            if (group.Count() != 20 || group.Count(item =>
                    item.Data.Node.IsFieldSerialized(1)) != 14 ||
                group.Count(item => item.Geometry.StrictlyConvex) != 18 ||
                group.Min(item => item.SerializedSize) != 120 ||
                group.Max(item => item.SerializedSize) != 228 ||
                cardinality.Count != expectedCardinality.Count ||
                expectedCardinality.Any(item =>
                    !cardinality.TryGetValue(item.Key,out int count) ||
                    count != item.Value))
            {
                throw new InvalidDataException(
                    $"spOcclusionVolume profile changed for {group.Key}: " +
                    JsonSerializer.Serialize(cardinality));
            }
        }
        const int requiredMask = (1 << 0) | (1 << 8);
        const int allowedMask = requiredMask | (1 << 1);
        if (observations.Select(item => item.CorpusKey).Distinct().Count() != 3 ||
            observations.Any(item =>
                (item.Data.Node.SerializedFieldMask & requiredMask) != requiredMask ||
                (item.Data.Node.SerializedFieldMask & ~allowedMask) != 0 ||
                item.Data.Node.IsAnimated || item.Data.Node.Children.Count != 0 ||
                item.Data.Node.Collisions.Count != 0 ||
                item.Data.IndexBuffer.PrimitiveType !=
                    SmoOcclusionVolumeDecoder.TriangleListPrimitiveType ||
                item.Data.IndexBuffer.IndexFormat !=
                    SmoOcclusionVolumeDecoder.UInt16IndexFormat ||
                item.Data.VertexBuffer.VertexDeclaration !=
                    SmoOcclusionVolumeDecoder.PositionOnlyVertexDeclaration ||
                item.Data.VertexBuffer.Flags != 0))
        {
            throw new InvalidDataException(
                "spOcclusionVolume fixed node/buffer values changed.");
        }
    }

    private static OcclusionVolumeComparison CompareOcclusionVolumes(
        IEnumerable<OcclusionVolumeObservation> observations,string leftCorpus,
        string rightCorpus)
    {
        Dictionary<(string Path,int Ordinal),OcclusionVolumeObservation> Build(
            string corpus) => observations.Where(item => item.CorpusKey == corpus)
            .ToDictionary(item => (item.CanonicalPath,item.Ordinal));
        var left = Build(leftCorpus);
        var right = Build(rightCorpus);
        var keys = left.Keys.Intersect(right.Keys).ToArray();
        return new OcclusionVolumeComparison(
            keys.Select(item => item.Path).Distinct().Count(),keys.Length,
            keys.Count(key => left[key].SemanticSignature ==
                              right[key].SemanticSignature),
            keys.Count(key => left[key].SerializedSha256 ==
                              right[key].SerializedSha256));
    }

    private static void UpsertOcclusionVolumeFieldDefinitions(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IReadOnlyList<OcclusionVolumeObservation> observations)
    {
        (int Section,int Type,int Occurrence,string Semantic,string Display,
            string Kind,string Layout,bool Repeated,string Evidence,string Notes)[] defs =
        [
            (1,0,0,"node.position","Local position","vector3","three little-endian Single values",false,"confirmed_both_executables_and_full_corpus","Required; present in all 20 objects per corpus."),
            (1,1,0,"node.rotation","Local rotation","quaternion","four little-endian Single values in X/Y/Z/W order",false,"confirmed_both_executables_and_full_corpus","Optional; present in 14/20 objects per corpus."),
            (1,2,0,"node.scale","Local scale","vector3","three little-endian Single values",false,"confirmed_both_executables_not_observed_corpus","Inherited default (1,1,1)."),
            (1,3,0,"node.is_bone","Bone node","boolean","one byte 0 or 1",false,"confirmed_both_executables_not_observed_corpus","Inherited false default."),
            (1,4,0,"node.is_static","Static node","boolean","one byte 0 or 1",false,"confirmed_both_executables_not_observed_corpus","Inherited false default."),
            (1,5,-1,"node.child","Child node","object_relationship","node child relationship",true,"confirmed_both_executables_not_observed_corpus","No occluder uses inherited children."),
            (1,6,0,"node.billboard_axis","Billboard axis","uint32_enum","UInt32 axis 1 or 2",false,"confirmed_both_executables_not_observed_corpus","Inherited default 0."),
            (1,7,-1,"node.collision","Collision info","object_relationship","node collision relationship",true,"confirmed_both_executables_not_observed_corpus","No occluder uses inherited collision links."),
            (1,8,0,"node.is_animated","Animated node","boolean","one byte 0 or 1",false,"confirmed_both_executables_and_full_corpus","Required and always false."),
            (0,0,0,"occlusion_volume.index_buffer","Occlusion index buffer","triangle_index_buffer","UInt32 primitive type/count/index format followed by UInt16 triangle indices",false,"confirmed_both_executables_and_full_corpus","Required; primitive type 2 triangle list and index format 0 UInt16 in every object."),
            (0,1,0,"occlusion_volume.vertex_buffer","Occlusion vertex buffer","position_vertex_buffer","UInt32 declaration/count/flags followed by Vector3 positions",false,"confirmed_both_executables_and_full_corpus","Required; declaration 0 position-only and flags 0 in every object.")
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

    private static void AnnotateOcclusionVolumeFields(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<OcclusionVolumeObservation> observations)
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
        foreach (OcclusionVolumeObservation observation in observations)
        foreach (OcclusionVolumeFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException(
                    "Could not annotate spOcclusionVolume field.");
        }
    }

    private static int InsertOcclusionVolumeVariant(
        SqliteConnection connection,SqliteTransaction transaction,uint typeHash,
        IEnumerable<OcclusionVolumeObservation> observations,string now)
    {
        Dictionary<string,object> corpusShapes = observations
            .GroupBy(item => item.CorpusKey).ToDictionary(
                group => group.Key,group => (object)new
                {
                    rotated=group.Count(item =>
                        item.Data.Node.IsFieldSerialized(1)),
                    cardinalities=group.GroupBy(item =>
                            $"{item.Data.VertexBuffer.VertexCount}/" +
                            $"{item.Data.IndexBuffer.PrimitiveCount}")
                        .OrderBy(item => item.Key)
                        .ToDictionary(item => item.Key,item => item.Count())
                });
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,status,
                discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2',$key,
                   'Portable visibility occluder','confirmed',
                   $discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$key",OcclusionVolumeVariantKey);
        command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
        {
            corpusShapes,sections=new[] { "spNode","spOcclusionVolume" },
            nodeFieldOrder=new[] { "position","rotation?","is_animated",
                "terminator" },
            ownFieldOrder=new[] { "index_buffer","vertex_buffer","terminator" },
            geometry="connected planar triangle disk; 18/20 per corpus strictly convex",
            executableGeometry="convex closed volume or planar shape",
            indexBuffer=new { primitiveType=2,indexFormat=0 },
            vertexBuffer=new { declaration=0,flags=0 }
        }));
        command.Parameters.AddWithValue("$notes",
            "One serializer/runtime layout. Optional node Rotation and polygon " +
            "vertex cardinality are authored data dimensions, not subtypes. " +
            "Closed convex volumes are executable-supported but absent from the " +
            "corpus. Two instances per corpus reuse one decagon with a shallow " +
            "11.83-unit indentation despite the executable's convexity message.");
        command.Parameters.AddWithValue("$utc",now);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void AssignOcclusionVolumeVariant(
        SqliteConnection connection,SqliteTransaction transaction,
        IEnumerable<OcclusionVolumeObservation> observations,int variantId)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete buffer decode and convex planar topology validation')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.AddWithValue("$variant",variantId);
        foreach (OcclusionVolumeObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.ExecuteNonQuery();
        }
    }

    private sealed record OcclusionVolumeFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record OcclusionVolumeFieldAnnotation(
        int FieldIndex,string Semantic,string Layout,string DecodedJson);
    private sealed record OcclusionVolumeGeometry(
        float MaximumPlanarityError,double SurfaceArea,int BoundaryEdges,
        int InternalEdges,bool StrictlyConvex);
    private sealed record OcclusionVolumeObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string CanonicalPath,
        string Name,long SerializedSize,SmoOcclusionVolumeData Data,
        OcclusionVolumeGeometry Geometry,string SerializedSha256,
        string SemanticSignature,
        IReadOnlyList<OcclusionVolumeFieldAnnotation> Fields);
    private sealed record OcclusionVolumeComparison(
        int CommonResources,int PairedObjects,int EqualSemantic,int EqualBytes);
}
