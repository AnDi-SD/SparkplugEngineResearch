using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeCollisionInfo(
        string databasePath,
        SmoResearchClassReport report)
    {
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item => item.NamedObjectCount != 0) ||
            report.Fields.Any(item =>
                item.SectionFromEnd != 0 || item.Occurrence != 0 ||
                item.FieldType is < 0 or > 2 ||
                (item.FieldType == 1 && item.PayloadSize != 4) ||
                (item.FieldType == 2 && item.PayloadSize != 40)))
        {
            throw new InvalidDataException(
                "spCollisionInfo corpus no longer matches the validated " +
                "single-section Primitive/Group/Transform contract.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        List<CollisionInfoObservation> observations =
            LoadCollisionInfoObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (objectCount != 10_594 || observations.Count != objectCount ||
            observations.Sum(item => item.Fields.Count) != 31_286)
        {
            throw new InvalidDataException(
                $"Expected 10594 decoded spCollisionInfo objects and 31286 " +
                $"fields, got {observations.Count}/{objectCount} and " +
                $"{observations.Sum(item => item.Fields.Count)} fields.");
        }

        Dictionary<(string Corpus,string Variant),int> variantCounts = observations
            .GroupBy(item => (item.CorpusKey,item.VariantKey))
            .ToDictionary(group => group.Key,group => group.Count());
        RequireCollisionInfoVariantCounts(variantCounts);
        CollisionInfoProfile profile = CreateCollisionInfoProfile(observations);
        CollisionInfoProfile expectedProfile = new(
            10_590,4,10_162,423,6,3,4_271,6_313,10,
            10_108,1_974,4,432,4_647,5_511);
        if (profile != expectedProfile)
        {
            throw new InvalidDataException(
                "spCollisionInfo relationship/group/transform profile changed: " +
                JsonSerializer.Serialize(profile));
        }

        (int pcCommon,int pcEqual) = CompareCollisionInfoPcCopies(observations);
        CollisionInfoPlatformComparison cross =
            CompareCollisionInfoPlatforms(observations);

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spCollisionInfoSerializer","esfCollisionInfoPrimitive",
            "esfCollisionInfoGroup","esfCollisionInfoTransform",
            "pDataStream->Read( uCollisionGroup )","spBoundingVolume"
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
                   WHERE type_hash=$hash AND variant_key LIKE 'collision_info_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        UpsertCollisionInfoFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateCollisionInfoFields(connection,transaction,observations);
        Dictionary<string,int> variantIds = InsertCollisionInfoVariants(
            connection,transaction,report.TypeHash,observations,now);
        AssignCollisionInfoVariants(
            connection,transaction,observations,variantIds);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "spCollisionInfo serializer registration, enum names and read/write diagnostics",
            "The PC serializer switches over Primitive, Group and Transform. " +
            "Primitive requests base class 0x21CC76AF (spBoundingVolume); " +
            "Transform reads position, rotation and scale.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "MIPS spCollisionInfo serializer and matching field enums",
            "The PS2 executable independently uses the same three fields, " +
            "base primitive class and transform order.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict reload and complete decode of every unique PC and PS2 object",
            $"{objectCount} objects and 31286 annotated fields. Variants: " +
            string.Join(", ",variantCounts.OrderBy(item => item.Key.Corpus)
                .ThenBy(item => item.Key.Variant).Select(item =>
                    $"{item.Key.Corpus}/{item.Key.Variant}={item.Value}")) +
            $". Inline primitives={profile.InlinePrimitive}; ID-only=" +
            $"{profile.IdOnlyPrimitive}; targets: spMeshBV={profile.Mesh}, " +
            $"spOBBBV={profile.Obb}, spBoxBV={profile.Box}, " +
            $"spSphereBV={profile.Sphere}. Groups: 1={profile.Group1}, " +
            $"2={profile.Group2}, omitted={profile.GroupOmitted}. " +
            $"Transforms={profile.TransformSerialized}, identity=" +
            $"{profile.IdentityTransform}.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus spCollisionInfo ordinal",
            $"PC working/pristine equal resources: {pcEqual}/{pcCommon}. " +
            $"PC/PS2 common resources={cross.CommonResources}; equal collision " +
            $"counts={cross.EqualCountResources}; paired objects=" +
            $"{cross.PairedObjects}; matches: variant={cross.EqualVariant}, " +
            $"primitive type={cross.EqualPrimitiveType}, group=" +
            $"{cross.EqualGroup}, transform presence=" +
            $"{cross.EqualTransformPresence}.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='collision_shape_owner',
                       description='Collision primitive relationship, collision group and optional world transform',
                       decode_status='read_only_decode',
                       notes='Every PC/PS2 object strictly decodes. Three historical field-presence variants and four concrete BV target classes are confirmed. Mutation remains runtime-untested.'
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
            "All Primitive, Group and Transform forms decode across PC and PS2. " +
            $"Confirmed {profile.Mesh} mesh, {profile.Obb} OBB, {profile.Box} box " +
            $"and {profile.Sphere} sphere primitives; mutation remains disabled.");
    }

    private static List<CollisionInfoObservation> LoadCollisionInfoObservations(
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.CollisionInfo);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<CollisionInfoFileLocation>();
        while (reader.Read())
        {
            locations.Add(new CollisionInfoFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<CollisionInfoObservation>();
        foreach (CollisionInfoFileLocation location in locations)
        {
            byte[] bytes = ReadCollisionInfoResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.CollisionInfo))
            {
                if (!SmoCollisionInfoDecoder.TryDecode(
                        document,entry,out SmoCollisionInfoData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                result.Add(CreateCollisionInfoObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return result;
    }

    private static byte[] ReadCollisionInfoResource(
        CollisionInfoFileLocation location)
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
                "PCK collision-info occurrence is incomplete.");
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

    private static CollisionInfoObservation CreateCollisionInfoObservation(
        CollisionInfoFileLocation location,
        SmoDocument document,
        SmoObjectEntry entry,
        int ordinal,
        SmoCollisionInfoData decoded)
    {
        IReadOnlyList<SmoObjectField> direct = SmoObjectFieldReader.Read(document,entry);
        var fields = new List<CollisionInfoFieldAnnotation>();
        for (int index = 0;index < direct.Count - 1;index++)
        {
            SmoObjectField field = direct[index];
            if (!SmoSerializedFieldRegistry.TryDescribeOwnField(
                    entry.TypeHash,direct,index,
                    out SmoSerializedFieldDescriptor? descriptor) || descriptor is null)
            {
                throw new InvalidDataException(
                    $"Unregistered spCollisionInfo field {field.FieldType}.");
            }
            object decodedValue = field.FieldType switch
            {
                0 => decoded.Primitive,
                1 => decoded.CollisionGroup ?? throw new InvalidDataException(
                    "Serialized collision group was not decoded."),
                2 => TransformValues(decoded.Transform ?? throw new InvalidDataException(
                    "Serialized collision transform was not decoded.")),
                _ => throw new InvalidDataException("Unexpected collision-info field.")
            };
            fields.Add(new CollisionInfoFieldAnnotation(
                index,field.FieldType,descriptor.Key,descriptor.PayloadLayout,
                JsonSerializer.Serialize(decodedValue)));
        }
        string variant = decoded.SerializedFieldMask switch
        {
            0b001 => "collision_info_primitive_only",
            0b011 => "collision_info_no_transform",
            0b111 => "collision_info_full",
            _ => throw new InvalidDataException(
                "Unexpected spCollisionInfo field-presence mask.")
        };
        uint? parentType = entry.ParentIndex is int parentIndex
            ? document.Objects[parentIndex].TypeHash
            : null;
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        return new CollisionInfoObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            location.PlatformKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            decoded,variant,parentType,serializedHash,fields.AsReadOnly());
    }

    private static float[] TransformValues(SmoCollisionInfoTransform value) =>
    [
        value.Position.X,value.Position.Y,value.Position.Z,
        value.Rotation.X,value.Rotation.Y,value.Rotation.Z,value.Rotation.W,
        value.Scale.X,value.Scale.Y,value.Scale.Z
    ];

    private static CollisionInfoProfile CreateCollisionInfoProfile(
        IReadOnlyList<CollisionInfoObservation> observations)
    {
        int inline = observations.Count(item => item.Data.Primitive.Encoding ==
            SmoNodeRelationshipEncoding.InlineObject);
        int idOnly = observations.Count(item => item.Data.Primitive.Encoding ==
            SmoNodeRelationshipEncoding.IdOnly);
        int CountTarget(uint typeHash) => observations.Count(item =>
            item.Data.Primitive.TargetTypeHash == typeHash);
        int group1 = observations.Count(item => item.Data.CollisionGroup == 1);
        int group2 = observations.Count(item => item.Data.CollisionGroup == 2);
        int groupOmitted = observations.Count(item => !item.Data.CollisionGroup.HasValue);
        int transforms = observations.Count(item => item.Data.Transform is not null);
        int identity = observations.Count(item => IsIdentity(item.Data.Transform));
        int Parent(uint? typeHash) => observations.Count(
            item => item.ParentType == typeHash);
        return new CollisionInfoProfile(
            inline,idOnly,CountTarget(SmoClassIds.MeshBoundingVolume),
            CountTarget(SmoClassIds.OrientedBoxBoundingVolume),
            CountTarget(SmoClassIds.BoxBoundingVolume),
            CountTarget(SmoClassIds.SphereBoundingVolume),
            group1,group2,groupOmitted,transforms,identity,Parent(null),
            Parent(SmoClassIds.Node),Parent(SmoClassIds.PartitionNode),
            Parent(SmoClassIds.RenderNode));
    }

    private static bool IsIdentity(SmoCollisionInfoTransform? value) =>
        value is not null && value.Position.LengthSquared() <= 0.000001f * 0.000001f &&
        MathF.Abs(value.Rotation.X) <= 0.000001f &&
        MathF.Abs(value.Rotation.Y) <= 0.000001f &&
        MathF.Abs(value.Rotation.Z) <= 0.000001f &&
        MathF.Abs(MathF.Abs(value.Rotation.W)-1) <= 0.000001f &&
        MathF.Abs(value.Scale.X-1) <= 0.000001f &&
        MathF.Abs(value.Scale.Y-1) <= 0.000001f &&
        MathF.Abs(value.Scale.Z-1) <= 0.000001f;

    private static void RequireCollisionInfoVariantCounts(
        IReadOnlyDictionary<(string Corpus,string Variant),int> counts)
    {
        Dictionary<(string Corpus,string Variant),int> expected = new()
        {
            [("pc-working","collision_info_full")] = 3_401,
            [("pc-working","collision_info_no_transform")] = 238,
            [("pc-working","collision_info_primitive_only")] = 4,
            [("pc-pristine","collision_info_full")] = 3_401,
            [("pc-pristine","collision_info_no_transform")] = 238,
            [("pc-pristine","collision_info_primitive_only")] = 4,
            [("ps2-pristine","collision_info_full")] = 3_306,
            [("ps2-pristine","collision_info_primitive_only")] = 2
        };
        if (counts.Count != expected.Count || expected.Any(item =>
                !counts.TryGetValue(item.Key,out int value) || value != item.Value))
        {
            throw new InvalidDataException(
                "spCollisionInfo field-presence distribution changed: " +
                JsonSerializer.Serialize(counts));
        }
    }

    private static (int Common,int Equal) CompareCollisionInfoPcCopies(
        IEnumerable<CollisionInfoObservation> observations)
    {
        Dictionary<string,CollisionInfoObservation[]> Build(string corpus) =>
            observations.Where(item => item.CorpusKey == corpus)
                .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
                .ToDictionary(group => group.Key,
                    group => group.OrderBy(item => item.Ordinal).ToArray(),
                    StringComparer.Ordinal);
        Dictionary<string,CollisionInfoObservation[]> working = Build("pc-working");
        Dictionary<string,CollisionInfoObservation[]> pristine = Build("pc-pristine");
        string[] paths = working.Keys.Intersect(
            pristine.Keys,StringComparer.Ordinal).ToArray();
        int equal = paths.Count(path =>
            working[path].Select(item => item.SerializedSha256)
                .SequenceEqual(pristine[path].Select(item => item.SerializedSha256)));
        return (paths.Length,equal);
    }

    private static CollisionInfoPlatformComparison CompareCollisionInfoPlatforms(
        IEnumerable<CollisionInfoObservation> observations)
    {
        Dictionary<string,CollisionInfoObservation[]> Build(string corpus) =>
            observations.Where(item => item.CorpusKey == corpus)
                .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
                .ToDictionary(group => group.Key,
                    group => group.OrderBy(item => item.Ordinal).ToArray(),
                    StringComparer.Ordinal);
        Dictionary<string,CollisionInfoObservation[]> pc = Build("pc-pristine");
        Dictionary<string,CollisionInfoObservation[]> ps2 = Build("ps2-pristine");
        string[] common = pc.Keys.Intersect(ps2.Keys,StringComparer.Ordinal).ToArray();
        string[] equalCount = common.Where(path =>
            pc[path].Length == ps2[path].Length).ToArray();
        int pairs = 0,variant = 0,primitive = 0,group = 0,transform = 0;
        foreach (string path in equalCount)
        for (int index = 0;index < pc[path].Length;index++)
        {
            CollisionInfoObservation left = pc[path][index];
            CollisionInfoObservation right = ps2[path][index];
            pairs++;
            variant += left.VariantKey == right.VariantKey ? 1 : 0;
            primitive += left.Data.Primitive.TargetTypeHash ==
                         right.Data.Primitive.TargetTypeHash ? 1 : 0;
            group += left.Data.CollisionGroup == right.Data.CollisionGroup ? 1 : 0;
            transform += (left.Data.Transform is null) ==
                         (right.Data.Transform is null) ? 1 : 0;
        }
        return new CollisionInfoPlatformComparison(
            common.Length,equalCount.Length,pairs,variant,primitive,group,transform);
    }

    private static void UpsertCollisionInfoFieldDefinitions(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<CollisionInfoObservation> observations)
    {
        (int Type,string Semantic,string Display,string Kind,string Layout,
            string Notes)[] definitions =
        [
            (0,"collision_info.primitive","Collision primitive",
                "object_relationship","object relationship to spBoundingVolume",
                "Required; resolves to spMeshBV, spOBBBV, spBoxBV or spSphereBV."),
            (1,"collision_info.group","Collision group","uint32","UInt32",
                "Optional in ten historical objects; observed values are 1 and 2."),
            (2,"collision_info.transform","Collision world transform","transform",
                "Vector3 position, Quaternion X/Y/Z/W, Vector3 scale",
                "Optional in 486 historical objects; every serialized value is finite.")
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
                payloadBytes = definition.Type switch { 1 => 4, 2 => 40, _ => (int?)null },
                expectedBaseTargetClass = definition.Type == 0
                    ? "spBoundingVolume (0x21CC76AF)"
                    : null,
                mutationStatus = "runtime_not_tested"
            }));
            command.Parameters.AddWithValue(
                "$notes",$"{definition.Notes} Observed {count} times.");
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotateCollisionInfoFields(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<CollisionInfoObservation> observations)
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
        foreach (CollisionInfoObservation observation in observations)
        foreach (CollisionInfoFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException("Could not annotate spCollisionInfo field.");
        }
    }

    private static Dictionary<string,int> InsertCollisionInfoVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<CollisionInfoObservation> observations,
        string now)
    {
        (string Key,string Display,string Notes)[] variants =
        [
            ("collision_info_full","Primitive, group and transform",
                "Current complete serializer form."),
            ("collision_info_no_transform","Primitive and group",
                "Historical form with no serialized world transform."),
            ("collision_info_primitive_only","Primitive only",
                "Historical form with both group and transform omitted.")
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

    private static void AssignCollisionInfoVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<CollisionInfoObservation> observations,
        IReadOnlyDictionary<string,int> variantIds)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spCollisionInfo decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.Add("$variant",SqliteType.Integer);
        foreach (CollisionInfoObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$variant"].Value = variantIds[observation.VariantKey];
            command.ExecuteNonQuery();
        }
    }

    private sealed record CollisionInfoFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record CollisionInfoFieldAnnotation(
        int FieldIndex,int FieldType,string Semantic,string Layout,
        string DecodedJson);
    private sealed record CollisionInfoObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string PlatformKey,
        string CanonicalPath,SmoCollisionInfoData Data,string VariantKey,
        uint? ParentType,string SerializedSha256,
        IReadOnlyList<CollisionInfoFieldAnnotation> Fields);
    private sealed record CollisionInfoProfile(
        int InlinePrimitive,int IdOnlyPrimitive,int Mesh,int Obb,int Box,int Sphere,
        int Group1,int Group2,int GroupOmitted,int TransformSerialized,
        int IdentityTransform,int RootParent,int NodeParent,int PartitionNodeParent,
        int RenderNodeParent);
    private sealed record CollisionInfoPlatformComparison(
        int CommonResources,int EqualCountResources,int PairedObjects,
        int EqualVariant,int EqualPrimitiveType,int EqualGroup,
        int EqualTransformPresence);
}
