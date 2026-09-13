using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeMeshBoundingVolume(
        string databasePath,
        SmoResearchClassReport report)
    {
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item => item.NamedObjectCount != 0) ||
            report.Fields.Any(item =>
                item.SectionFromEnd != 0 || item.Occurrence != 0 ||
                item.FieldType is < 0 or > 1))
        {
            throw new InvalidDataException(
                "spMeshBV corpus no longer matches the validated geometry/" +
                "optional-face-data contract.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        List<MeshBoundingVolumeObservation> observations =
            LoadMeshBoundingVolumeObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        long annotatedFieldCount = observations.Sum(item => (long)item.Fields.Count);
        if (objectCount != 10_513 || observations.Count != objectCount ||
            annotatedFieldCount != 14_116)
        {
            throw new InvalidDataException(
                $"Expected 10513 decoded spMeshBV objects and 14116 semantic " +
                $"fields, got {observations.Count}/{objectCount} and " +
                $"{annotatedFieldCount} fields.");
        }

        Dictionary<(string Corpus,string Variant),int> variantCounts = observations
            .GroupBy(item => (item.CorpusKey,item.VariantKey))
            .ToDictionary(group => group.Key,group => group.Count());
        RequireMeshBoundingVolumeVariantCounts(variantCounts);
        MeshBoundingVolumeProfile profile = CreateMeshBoundingVolumeProfile(observations);
        MeshBoundingVolumeProfile expectedProfile = new(
            6_910,3_603,291_065,245_554,106_832,63,84_326,23_452,8_591,
            10_158,351,4);
        if (profile != expectedProfile)
        {
            throw new InvalidDataException(
                "spMeshBV geometry/face/parent profile changed: " +
                JsonSerializer.Serialize(profile));
        }
        (int pcCommon,int pcEqual) = CompareMeshBoundingVolumePcCopies(observations);
        MeshBoundingVolumePlatformComparison cross =
            CompareMeshBoundingVolumePlatforms(observations);
        MeshBoundingVolumePlatformComparison expectedCross =
            new(69,60,3_295,3_171,3_281,3_269);
        if ((pcCommon,pcEqual) != (95,95) || cross != expectedCross)
        {
            throw new InvalidDataException(
                "spMeshBV cross-corpus profile changed: " +
                JsonSerializer.Serialize(new { pcCommon,pcEqual,cross }));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spMeshBVSerializer","esfMeshBV","esfMeshBVFaceData","wxFaceData",
            "m_uSurfaceType","m_uFlags","m_uSurfaceID","stone","dirt",
            "grass","water","snow","swamp","mud","deepwater","carpet"
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
                   WHERE type_hash=$hash AND variant_key LIKE 'mesh_bv_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        UpsertMeshBoundingVolumeFieldDefinitions(
            connection,transaction,report.TypeHash,observations,profile);
        AnnotateMeshBoundingVolumeFields(connection,transaction,observations);
        Dictionary<string,int> variantIds = InsertMeshBoundingVolumeVariants(
            connection,transaction,report.TypeHash,observations,now);
        AssignMeshBoundingVolumeVariants(
            connection,transaction,observations,variantIds);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "spMeshBV serializer, esfMeshBV/esfMeshBVFaceData and wxFaceData diagnostics",
            "PC writes version-2 indexed geometry and optionally invokes the " +
            "wxFaceData array serializer. wxFaceData sparse fields are surface " +
            "type (u8), flags (u16) and surface ID (u8). The ground-string " +
            "converter maps values 1..9 to stone, dirt, grass, water, snow, " +
            "swamp, mud, deepwater and carpet.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "MIPS spMeshBV and wxFaceData serializers with matching field diagnostics",
            "PS2 independently uses the same geometry version, face-data class ID " +
            "and three sparse wxFaceData members, including the same nine named " +
            "surface-type strings.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict full-payload reload of directory SMO and PCK entries",
            $"{objectCount} objects and {annotatedFieldCount} semantic fields. " +
            $"Geometry-only={profile.GeometryOnly}; with wxFaceData=" +
            $"{profile.WithFaceData}; triangles={profile.Triangles}; vertices=" +
            $"{profile.Vertices}; face records={profile.FaceRecords}; degenerate " +
            $"triangles={profile.DegenerateTriangles}; serialized face members: " +
            $"surface type={profile.SerializedSurfaceType}, flags=" +
            $"{profile.SerializedFlags}, surface ID={profile.SerializedSurfaceId}. " +
            $"Parents: spCollisionInfo={profile.CollisionInfoParent}, " +
            $"spMeshNavigationSet={profile.NavigationSetParent}, root=" +
            $"{profile.RootParent}.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus spMeshBV ordinal",
            $"PC working/pristine equal resources: {pcEqual}/{pcCommon}. " +
            $"PC/PS2 common resources={cross.CommonResources}; equal mesh counts=" +
            $"{cross.EqualCountResources}; paired objects={cross.PairedObjects}; " +
            $"matches: geometry={cross.EqualGeometry}, face-data presence=" +
            $"{cross.EqualFacePresence}, face-data values={cross.EqualFaceData}.",
            null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='collision_triangle_mesh',
                       description='Version-2 indexed collision triangles with optional per-face wxFaceData',
                       decode_status='read_only_decode',
                       notes='Every PC/PS2 object strictly decodes. wxFaceData surface type, flags and surface ID are exposed; mutation remains runtime-untested.'
                   WHERE type_hash=$hash;
                   """))
        {
            updateClass.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            updateClass.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);

        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>",
            "confirmed_read_only",report.Profiles.Count,objectCount,
            report.Profiles.Sum(item => item.UniqueResourceCount),variantIds.Count,
            observations.Count,evidenceRows,
            $"All version-2 geometry decoded: {profile.Triangles} triangles and " +
            $"{profile.Vertices} vertices. Decoded {profile.FaceRecords} " +
            "wxFaceData records; mutation remains disabled.");
    }

    private static List<MeshBoundingVolumeObservation>
        LoadMeshBoundingVolumeObservations(SqliteConnection connection)
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
        command.Parameters.AddWithValue(
            "$hash",(long)SmoClassIds.MeshBoundingVolume);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<MeshBoundingVolumeFileLocation>();
        while (reader.Read())
        {
            locations.Add(new MeshBoundingVolumeFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<MeshBoundingVolumeObservation>();
        foreach (MeshBoundingVolumeFileLocation location in locations)
        {
            byte[] bytes = ReadMeshBoundingVolumeResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.MeshBoundingVolume))
            {
                if (!SmoMeshBoundingVolumeDecoder.TryDecode(
                        document,entry,out SmoMeshBoundingVolumeData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                result.Add(CreateMeshBoundingVolumeObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return result;
    }

    private static byte[] ReadMeshBoundingVolumeResource(
        MeshBoundingVolumeFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
        {
            return File.ReadAllBytes(Path.Combine(location.SourceRoot,
                location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        }
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
        {
            throw new InvalidDataException("PCK spMeshBV occurrence is incomplete.");
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

    private static MeshBoundingVolumeObservation CreateMeshBoundingVolumeObservation(
        MeshBoundingVolumeFileLocation location,
        SmoDocument document,
        SmoObjectEntry entry,
        int ordinal,
        SmoMeshBoundingVolumeData decoded)
    {
        IReadOnlyList<SmoObjectField> direct = SmoObjectFieldReader.Read(document,entry);
        int degenerateTriangles = CountDegenerateTriangles(decoded);
        (Vector3 minimum,Vector3 maximum) = FindBounds(decoded.Positions);
        var fields = new List<MeshBoundingVolumeFieldAnnotation>();
        for (int index = 0;index < direct.Count - 1;index++)
        {
            SmoObjectField field = direct[index];
            if (!SmoSerializedFieldRegistry.TryDescribeOwnField(
                    entry.TypeHash,direct,index,
                    out SmoSerializedFieldDescriptor? descriptor) || descriptor is null)
            {
                throw new InvalidDataException(
                    $"Unregistered spMeshBV field {field.FieldType}.");
            }
            object decodedValue = field.FieldType switch
            {
                0 => new
                {
                    decoded.Version,
                    decoded.TriangleCount,
                    decoded.VertexCount,
                    Minimum = VectorValues(minimum),
                    Maximum = VectorValues(maximum),
                    DegenerateTriangles = degenerateTriangles
                },
                1 => CreateFaceDataSummary(decoded.FaceData ??
                    throw new InvalidDataException(
                        "Serialized spMeshBV face data was not decoded.")),
                _ => throw new InvalidDataException("Unexpected spMeshBV field.")
            };
            fields.Add(new MeshBoundingVolumeFieldAnnotation(
                index,field.FieldType,descriptor.Key,descriptor.PayloadLayout,
                JsonSerializer.Serialize(decodedValue)));
        }
        string variant = decoded.FaceData is null
            ? "mesh_bv_geometry_only"
            : "mesh_bv_with_face_data";
        uint? parentType = entry.ParentIndex is int parentIndex
            ? document.Objects[parentIndex].TypeHash
            : null;
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        string geometryHash = Convert.ToHexString(SHA256.HashData(direct[0].Payload.Span));
        string? faceHash = decoded.FaceData is null
            ? null
            : Convert.ToHexString(SHA256.HashData(direct[1].Payload.Span));
        return new MeshBoundingVolumeObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            location.PlatformKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            decoded,variant,parentType,serializedHash,geometryHash,faceHash,
            degenerateTriangles,fields.AsReadOnly());
    }

    private static object CreateFaceDataSummary(
        IReadOnlyList<SmoMeshBoundingVolumeFaceData> faces) => new
    {
        ClassId = $"0x{SmoMeshBoundingVolumeDecoder.FaceDataClassId:X8}",
        FaceCount = faces.Count,
        FieldMasks = Histogram(faces.Select(item => (uint)item.SerializedFieldMask)),
        SurfaceTypes = Histogram(faces.Select(item => (uint)item.SurfaceType)),
        Flags = Histogram(faces.Select(item => (uint)item.Flags)),
        SurfaceIds = Histogram(faces.Select(item => (uint)item.SurfaceId))
    };

    private static Dictionary<uint,long> Histogram(IEnumerable<uint> values) =>
        values.GroupBy(value => value).OrderBy(group => group.Key)
            .ToDictionary(group => group.Key,group => group.LongCount());

    private static float[] VectorValues(Vector3 value) =>
        [value.X,value.Y,value.Z];

    private static (Vector3 Minimum,Vector3 Maximum) FindBounds(
        IReadOnlyList<Vector3> positions)
    {
        if (positions.Count == 0)
            return (Vector3.Zero,Vector3.Zero);
        Vector3 minimum = positions[0];
        Vector3 maximum = positions[0];
        for (int index = 1;index < positions.Count;index++)
        {
            minimum = Vector3.Min(minimum,positions[index]);
            maximum = Vector3.Max(maximum,positions[index]);
        }
        return (minimum,maximum);
    }

    private static int CountDegenerateTriangles(SmoMeshBoundingVolumeData data)
    {
        int count = 0;
        for (int index = 0;index < data.TriangleIndices.Count;index += 3)
        {
            Vector3 a = data.Positions[data.TriangleIndices[index]];
            Vector3 b = data.Positions[data.TriangleIndices[index + 1]];
            Vector3 c = data.Positions[data.TriangleIndices[index + 2]];
            if (Vector3.Cross(b - a,c - a).LengthSquared() <= 1e-12f)
                count++;
        }
        return count;
    }

    private static MeshBoundingVolumeProfile CreateMeshBoundingVolumeProfile(
        IReadOnlyList<MeshBoundingVolumeObservation> observations)
    {
        IEnumerable<SmoMeshBoundingVolumeFaceData> faces = observations
            .Where(item => item.Data.FaceData is not null)
            .SelectMany(item => item.Data.FaceData!);
        SmoMeshBoundingVolumeFaceData[] faceArray = faces.ToArray();
        int Parent(uint? hash) => observations.Count(item => item.ParentType == hash);
        return new MeshBoundingVolumeProfile(
            observations.Count(item => item.Data.FaceData is null),
            observations.Count(item => item.Data.FaceData is not null),
            observations.Sum(item => (long)item.Data.TriangleCount),
            observations.Sum(item => (long)item.Data.VertexCount),
            faceArray.LongLength,
            observations.Sum(item => (long)item.DegenerateTriangles),
            faceArray.LongCount(item => (item.SerializedFieldMask & 0b001) != 0),
            faceArray.LongCount(item => (item.SerializedFieldMask & 0b010) != 0),
            faceArray.LongCount(item => (item.SerializedFieldMask & 0b100) != 0),
            Parent(SmoClassIds.CollisionInfo),
            Parent(SmoClassIds.MeshNavigationSet),Parent(null));
    }

    private static void RequireMeshBoundingVolumeVariantCounts(
        IReadOnlyDictionary<(string Corpus,string Variant),int> counts)
    {
        Dictionary<(string Corpus,string Variant),int> expected = new()
        {
            [("pc-working","mesh_bv_geometry_only")] = 2_400,
            [("pc-working","mesh_bv_with_face_data")] = 1_209,
            [("pc-pristine","mesh_bv_geometry_only")] = 2_400,
            [("pc-pristine","mesh_bv_with_face_data")] = 1_209,
            [("ps2-pristine","mesh_bv_geometry_only")] = 2_110,
            [("ps2-pristine","mesh_bv_with_face_data")] = 1_185
        };
        if (counts.Count != expected.Count || expected.Any(item =>
                !counts.TryGetValue(item.Key,out int value) || value != item.Value))
        {
            throw new InvalidDataException(
                "spMeshBV field-presence distribution changed: " +
                JsonSerializer.Serialize(counts));
        }
    }

    private static (int Common,int Equal) CompareMeshBoundingVolumePcCopies(
        IEnumerable<MeshBoundingVolumeObservation> observations)
    {
        Dictionary<string,MeshBoundingVolumeObservation[]> Build(string corpus) =>
            observations.Where(item => item.CorpusKey == corpus)
                .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
                .ToDictionary(group => group.Key,
                    group => group.OrderBy(item => item.Ordinal).ToArray(),
                    StringComparer.Ordinal);
        Dictionary<string,MeshBoundingVolumeObservation[]> working =
            Build("pc-working");
        Dictionary<string,MeshBoundingVolumeObservation[]> pristine =
            Build("pc-pristine");
        string[] paths = working.Keys.Intersect(
            pristine.Keys,StringComparer.Ordinal).ToArray();
        int equal = paths.Count(path =>
            working[path].Select(item => item.SerializedSha256)
                .SequenceEqual(pristine[path].Select(item => item.SerializedSha256)));
        return (paths.Length,equal);
    }

    private static MeshBoundingVolumePlatformComparison
        CompareMeshBoundingVolumePlatforms(
            IEnumerable<MeshBoundingVolumeObservation> observations)
    {
        Dictionary<string,MeshBoundingVolumeObservation[]> Build(string corpus) =>
            observations.Where(item => item.CorpusKey == corpus)
                .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
                .ToDictionary(group => group.Key,
                    group => group.OrderBy(item => item.Ordinal).ToArray(),
                    StringComparer.Ordinal);
        Dictionary<string,MeshBoundingVolumeObservation[]> pc =
            Build("pc-pristine");
        Dictionary<string,MeshBoundingVolumeObservation[]> ps2 =
            Build("ps2-pristine");
        string[] common = pc.Keys.Intersect(ps2.Keys,StringComparer.Ordinal).ToArray();
        int equalCounts = common.Count(path => pc[path].Length == ps2[path].Length);
        int pairs = 0;
        int geometry = 0;
        int facePresence = 0;
        int faceData = 0;
        foreach (string path in common)
        for (int index = 0;index < Math.Min(pc[path].Length,ps2[path].Length);index++)
        {
            MeshBoundingVolumeObservation left = pc[path][index];
            MeshBoundingVolumeObservation right = ps2[path][index];
            pairs++;
            geometry += left.GeometrySha256 == right.GeometrySha256 ? 1 : 0;
            facePresence += (left.FaceSha256 is null) ==
                            (right.FaceSha256 is null) ? 1 : 0;
            faceData += left.FaceSha256 == right.FaceSha256 ? 1 : 0;
        }
        return new MeshBoundingVolumePlatformComparison(
            common.Length,equalCounts,pairs,geometry,facePresence,faceData);
    }

    private static void UpsertMeshBoundingVolumeFieldDefinitions(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<MeshBoundingVolumeObservation> observations,
        MeshBoundingVolumeProfile profile)
    {
        (int Type,string Semantic,string Display,string Kind,string Layout,
            string Notes)[] definitions =
        [
            (0,"mesh_bv.geometry","Collision triangle geometry",
                "indexed_triangle_mesh",
                "UInt32 version=2, UInt32 triangle count, UInt32 zero, " +
                "UInt16[triangle*3] indices, UInt32 zero, UInt32 vertex count, " +
                "UInt32 zero, Vector3[vertex] positions",
                "Required portable geometry shared by PC and PS2."),
            (1,"mesh_bv.face_data","Per-triangle wxFaceData",
                "wx_face_data_array",
                "UInt32 class ID 0x313C4C17, UInt32 face count, repeated " +
                "small-int tagged fields: 1=u8 surface type, 2=u16 flags, " +
                "3=u8 surface ID, 0=face terminator",
                "Optional object-level block; when present its count equals the " +
                "triangle count and all three face members default to zero.")
        ];
        foreach (var definition in definitions)
        {
            long count = observations.Sum(item =>
                (long)item.Fields.Count(field => field.FieldType == definition.Type));
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',0,$type,0,$semantic,$display,
                       $kind,$layout,'read_only_research',
                       'confirmed_both_executables_and_full_corpus',$constraints,$notes)
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
                observedDirectOccurrences = count,
                version = definition.Type == 0 ? 2 : (int?)null,
                faceDataClassId = definition.Type == 1 ? "0x313C4C17" : null,
                triangles = profile.Triangles,
                vertices = profile.Vertices,
                faceRecords = profile.FaceRecords,
                mutationStatus = "runtime_not_tested"
            }));
            command.Parameters.AddWithValue(
                "$notes",$"{definition.Notes} Observed {count} times.");
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotateMeshBoundingVolumeFields(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<MeshBoundingVolumeObservation> observations)
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
        foreach (MeshBoundingVolumeObservation observation in observations)
        foreach (MeshBoundingVolumeFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException("Could not annotate spMeshBV field.");
        }
    }

    private static Dictionary<string,int> InsertMeshBoundingVolumeVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<MeshBoundingVolumeObservation> observations,
        string now)
    {
        (string Key,string Display,string Notes)[] variants =
        [
            ("mesh_bv_geometry_only","Geometry only",
                "Version-2 indices and vertices; no per-triangle metadata."),
            ("mesh_bv_with_face_data","Geometry plus wxFaceData",
                "One sparse wxFaceData record per collision triangle.")
        ];
        var result = new Dictionary<string,int>(StringComparer.Ordinal);
        foreach ((string key,string display,string notes) in variants)
        {
            Dictionary<string,int> corpusCounts = observations
                .Where(item => item.VariantKey == key)
                .GroupBy(item => item.CorpusKey)
                .ToDictionary(group => group.Key,group => group.Count());
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO class_variants(
                    type_hash,scope_kind,scope_key,variant_key,display_name,status,
                    discriminator_json,notes,created_utc,updated_utc)
                VALUES($hash,'common','pc_ps2',$key,$display,'confirmed',
                       $discriminator,$notes,$utc,$utc);
                SELECT last_insert_rowid();
                """);
            command.Parameters.AddWithValue("$hash",(long)typeHash);
            command.Parameters.AddWithValue("$key",key);
            command.Parameters.AddWithValue("$display",display);
            command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
            {
                corpusCounts,
                fieldPresenceVariant = key
            }));
            command.Parameters.AddWithValue("$notes",notes);
            command.Parameters.AddWithValue("$utc",now);
            result.Add(key,Convert.ToInt32(command.ExecuteScalar()));
        }
        return result;
    }

    private static void AssignMeshBoundingVolumeVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<MeshBoundingVolumeObservation> observations,
        IReadOnlyDictionary<string,int> variantIds)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spMeshBV and wxFaceData decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.Add("$variant",SqliteType.Integer);
        foreach (MeshBoundingVolumeObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$variant"].Value = variantIds[observation.VariantKey];
            command.ExecuteNonQuery();
        }
    }

    private sealed record MeshBoundingVolumeFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record MeshBoundingVolumeFieldAnnotation(
        int FieldIndex,int FieldType,string Semantic,string Layout,
        string DecodedJson);
    private sealed record MeshBoundingVolumeObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string PlatformKey,
        string CanonicalPath,SmoMeshBoundingVolumeData Data,string VariantKey,
        uint? ParentType,string SerializedSha256,string GeometrySha256,
        string? FaceSha256,int DegenerateTriangles,
        IReadOnlyList<MeshBoundingVolumeFieldAnnotation> Fields);
    private sealed record MeshBoundingVolumeProfile(
        int GeometryOnly,int WithFaceData,long Triangles,long Vertices,
        long FaceRecords,long DegenerateTriangles,long SerializedSurfaceType,
        long SerializedFlags,long SerializedSurfaceId,int CollisionInfoParent,
        int NavigationSetParent,int RootParent);
    private sealed record MeshBoundingVolumePlatformComparison(
        int CommonResources,int EqualCountResources,int PairedObjects,
        int EqualGeometry,int EqualFacePresence,int EqualFaceData);
}
