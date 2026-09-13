using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeStaticRenderObject(
        string databasePath,
        SmoResearchClassReport report)
    {
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item =>
                item.NamedObjectCount != item.UniqueObjectCount ||
                item.MinimumSerializedSize != 209) ||
            report.Fields.Any(item =>
                item.SectionFromEnd != 0 || item.Occurrence != 0 ||
                (item.FieldType == 1 && item.PayloadSize != 64) ||
                (item.FieldType == 2 && item.PayloadSize != 64) ||
                item.FieldType is < 0 or > 2) ||
            report.Relations.Any(item => item.Direction switch
            {
                "parent" => item.RelatedTypeHash != SmoClassIds.PartitionNode,
                "child" => item.RelatedTypeHash != SmoClassIds.Model,
                _ => true
            }))
        {
            throw new InvalidDataException(
                "spStaticRenderObject corpus no longer matches the validated " +
                "partition-node parent, two matrices and inline model contract.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        List<StaticRenderObservation> observations =
            LoadStaticRenderObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (objectCount != 61_851 || observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected 61851 decoded spStaticRenderObject objects, got " +
                $"{observations.Count}/{objectCount}.");
        }
        if (observations.Sum(item => item.Fields.Count) != 185_553 ||
            observations.Any(item =>
                item.Data.Renderable.Encoding !=
                    SmoNodeRelationshipEncoding.InlineObject))
        {
            throw new InvalidDataException(
                "spStaticRenderObject field total or inline-model storage changed.");
        }

        Dictionary<string,StaticMatrixProfile> profiles = observations
            .GroupBy(item => item.CorpusKey)
            .ToDictionary(group => group.Key,CreateStaticMatrixProfile);
        RequireStaticMatrixProfiles(profiles);
        (int pcCommon,int pcEqual) = CompareStaticPcCopies(observations);
        StaticPlatformComparison cross = CompareStaticPlatforms(observations);
        if (pcCommon != 29 || pcEqual != 29 ||
            cross != new StaticPlatformComparison(
                29,26,18_390,18_390,18_390,18_388,18_388,18_390))
        {
            throw new InvalidDataException(
                "spStaticRenderObject PC-copy or PC/PS2 pairing changed: " +
                JsonSerializer.Serialize(cross));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spStaticRenderObject","spStaticRenderObjectSerializer",
            "esrosfStaticRenderObjectTransform",
            "esrosfStaticRenderObjectInvTransform",
            "esrosfStaticRenderObjectRenderable"
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
                     AND variant_key='static_render_object_inline_model';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        UpsertStaticRenderFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateStaticRenderFields(connection,transaction,observations);
        int variantId = InsertStaticRenderVariant(
            connection,transaction,report.TypeHash,observations,now);
        AssignStaticRenderVariant(
            connection,transaction,observations,variantId);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "spStaticRenderObject serializer registration, field enums and write diagnostics",
            "The PC executable names Transform, InvTransform and the repeated " +
            "Renderable relationship, written from GetWorldMatrix, " +
            "GetWorldInverseMatrix and GetRenderable(i).",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "MIPS spStaticRenderObject serializer registration and matching field enums",
            "The PS2 executable independently exposes the same three serializer fields.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict reload and complete decode of every unique PC and PS2 object",
            $"{objectCount} objects and 185553 annotated fields. Every object has " +
            "one affine Transform, one engine InvTransform and one inline spModel; " +
            "every physical parent is spPartitionNode. Matrix profiles: " +
            string.Join("; ",profiles.OrderBy(item => item.Key).Select(item =>
                $"{item.Key}: unit={item.Value.UnitScale}, uniform=" +
                $"{item.Value.UniformScale}, mirrored={item.Value.Mirrored}, " +
                $"mathematical-inverse={item.Value.MathematicalInverse}")) + ".",
            null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,variantId,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus spStaticRenderObject ordinal",
            $"PC working/pristine equal resources: {pcEqual}/{pcCommon}. PC/PS2 " +
            $"common resources: {cross.CommonResources}; equal counts: " +
            $"{cross.EqualCountResources}; paired objects: {cross.PairedObjects}; " +
            $"matches name={cross.EqualObjectName}, model={cross.EqualModelName}, " +
            $"Transform={cross.EqualTransform}, InvTransform={cross.EqualInverse}, " +
            $"translation={cross.EqualTranslation}.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='scene_placement',
                       description='Static world-space placement containing one renderable spModel',
                       decode_status='read_only_decode',
                       notes='All PC/PS2 objects strictly decode. Transform mutation is structurally implemented and tested; runtime round-trip in the game remains pending. InvTransform uses transpose-basis semantics and is not a general inverse when scale is authored.'
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
            $"Three fields and the sole observed inline-model variant decoded " +
            $"across {objectCount} placements. PC copies agree in " +
            $"{pcEqual}/{pcCommon} resources; {cross.PairedObjects} PC/PS2 objects " +
            "were paired. Scaled InvTransform convention is now explicit.");
    }

    private static List<StaticRenderObservation> LoadStaticRenderObservations(
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
        command.Parameters.AddWithValue(
            "$hash",(long)SmoClassIds.StaticRenderObject);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<StaticRenderFileLocation>();
        while (reader.Read())
        {
            locations.Add(new StaticRenderFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<StaticRenderObservation>();
        foreach (StaticRenderFileLocation location in locations)
        {
            byte[] bytes = ReadStaticRenderResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.StaticRenderObject))
            {
                if (!SmoStaticRenderObjectDecoder.TryDecode(
                        document,entry,out SmoStaticRenderObjectData? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                result.Add(CreateStaticRenderObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return result;
    }

    private static byte[] ReadStaticRenderResource(
        StaticRenderFileLocation location)
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
                "PCK static-render-object occurrence is incomplete.");
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

    private static StaticRenderObservation CreateStaticRenderObservation(
        StaticRenderFileLocation location,
        SmoDocument document,
        SmoObjectEntry entry,
        int ordinal,
        SmoStaticRenderObjectData decoded)
    {
        IReadOnlyList<SmoObjectField> direct =
            SmoObjectFieldReader.Read(document,entry);
        if (direct.Count != 4)
            throw new InvalidDataException("Unexpected static-render-object shape.");
        var fields = new List<StaticRenderFieldAnnotation>();
        for (int index = 0;index < 3;index++)
        {
            SmoObjectField field = direct[index];
            if (!SmoSerializedFieldRegistry.TryDescribeField(
                    entry.TypeHash,direct,index,
                    out SmoSerializedFieldDescriptor? descriptor) || descriptor is null)
            {
                throw new InvalidDataException(
                    $"Unregistered spStaticRenderObject field {field.FieldType}.");
            }
            object value = field.FieldType switch
            {
                0 => decoded.Renderable,
                1 => MatrixValues(decoded.Transform),
                2 => MatrixValues(decoded.EngineInverseTransform),
                _ => throw new InvalidDataException("Unexpected static field.")
            };
            fields.Add(new StaticRenderFieldAnnotation(
                index,field.FieldType,descriptor.Key,descriptor.PayloadLayout,
                JsonSerializer.Serialize(value)));
        }
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        return new StaticRenderObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            location.PlatformKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            entry.Name.TrimEnd('\0'),decoded,
            decoded.Renderable.TargetName?.TrimEnd('\0') ?? string.Empty,
            serializedHash,fields.AsReadOnly());
    }

    private static float[] MatrixValues(Matrix4x4 value) =>
    [
        value.M11,value.M12,value.M13,value.M14,
        value.M21,value.M22,value.M23,value.M24,
        value.M31,value.M32,value.M33,value.M34,
        value.M41,value.M42,value.M43,value.M44
    ];

    private static StaticMatrixProfile CreateStaticMatrixProfile(
        IEnumerable<StaticRenderObservation> source)
    {
        StaticRenderObservation[] values = source.ToArray();
        int unit = 0,uniform = 0,mirrored = 0,mathematical = 0;
        foreach (StaticRenderObservation item in values)
        {
            Matrix4x4 matrix = item.Data.Transform;
            double x = AxisLength(matrix.M11,matrix.M12,matrix.M13);
            double y = AxisLength(matrix.M21,matrix.M22,matrix.M23);
            double z = AxisLength(matrix.M31,matrix.M32,matrix.M33);
            if (Math.Max(Math.Abs(x-1),Math.Max(Math.Abs(y-1),Math.Abs(z-1)))
                <= 0.00001f)
                unit++;
            if (Math.Max(x,Math.Max(y,z))-Math.Min(x,Math.Min(y,z))
                <= 0.00001f)
                uniform++;
            mirrored += matrix.GetDeterminant() < 0 ? 1 : 0;
            if (IdentityProductError(
                    matrix,item.Data.EngineInverseTransform) <= 0.0001)
                mathematical++;
        }
        return new StaticMatrixProfile(values.Length,unit,uniform,mirrored,mathematical);
    }

    private static double AxisLength(float x,float y,float z) =>
        Math.Sqrt((double)x*x + (double)y*y + (double)z*z);

    private static double IdentityProductError(Matrix4x4 left,Matrix4x4 right)
    {
        float[] a = MatrixValues(left);
        float[] b = MatrixValues(right);
        double maximum = 0;
        for (int row = 0;row < 4;row++)
        for (int column = 0;column < 4;column++)
        {
            double value = 0;
            for (int inner = 0;inner < 4;inner++)
                value += (double)a[row*4+inner] * b[inner*4+column];
            maximum = Math.Max(maximum,Math.Abs(
                value - (row == column ? 1.0 : 0.0)));
        }
        return maximum;
    }

    private static void RequireStaticMatrixProfiles(
        IReadOnlyDictionary<string,StaticMatrixProfile> profiles)
    {
        Dictionary<string,StaticMatrixProfile> expected = new()
        {
            ["pc-working"] = new(20_469,8_511,14_127,1_008,6_785),
            ["pc-pristine"] = new(20_469,8_511,14_127,1_008,6_785),
            ["ps2-pristine"] = new(20_913,8_728,14_571,1_006,6_986)
        };
        if (profiles.Count != expected.Count || expected.Any(item =>
                !profiles.TryGetValue(item.Key,out StaticMatrixProfile? value) ||
                value != item.Value))
        {
            throw new InvalidDataException(
                "spStaticRenderObject scale/inverse profiles changed: " +
                JsonSerializer.Serialize(profiles));
        }
    }

    private static (int Common,int Equal) CompareStaticPcCopies(
        IEnumerable<StaticRenderObservation> observations)
    {
        Dictionary<string,StaticRenderObservation[]> Build(string corpus) =>
            observations.Where(item => item.CorpusKey == corpus)
                .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
                .ToDictionary(group => group.Key,
                    group => group.OrderBy(item => item.Ordinal).ToArray(),
                    StringComparer.Ordinal);
        Dictionary<string,StaticRenderObservation[]> working = Build("pc-working");
        Dictionary<string,StaticRenderObservation[]> pristine = Build("pc-pristine");
        string[] paths = working.Keys.Intersect(
            pristine.Keys,StringComparer.Ordinal).ToArray();
        int equal = paths.Count(path =>
            working[path].Select(item => (item.ObjectName,item.SerializedSha256))
                .SequenceEqual(pristine[path].Select(item =>
                    (item.ObjectName,item.SerializedSha256))));
        return (paths.Length,equal);
    }

    private static StaticPlatformComparison CompareStaticPlatforms(
        IEnumerable<StaticRenderObservation> observations)
    {
        Dictionary<string,StaticRenderObservation[]> Build(string corpus) =>
            observations.Where(item => item.CorpusKey == corpus)
                .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
                .ToDictionary(group => group.Key,
                    group => group.OrderBy(item => item.Ordinal).ToArray(),
                    StringComparer.Ordinal);
        Dictionary<string,StaticRenderObservation[]> pc = Build("pc-pristine");
        Dictionary<string,StaticRenderObservation[]> ps2 = Build("ps2-pristine");
        string[] paths = pc.Keys.Intersect(ps2.Keys,StringComparer.Ordinal).ToArray();
        string[] equalCount = paths.Where(path =>
            pc[path].Length == ps2[path].Length).ToArray();
        int pairs = 0,names = 0,models = 0,transforms = 0,inverses = 0,
            translations = 0;
        foreach (string path in equalCount)
        for (int index = 0;index < pc[path].Length;index++)
        {
            StaticRenderObservation left = pc[path][index];
            StaticRenderObservation right = ps2[path][index];
            pairs++;
            names += left.ObjectName == right.ObjectName ? 1 : 0;
            models += left.ModelName == right.ModelName ? 1 : 0;
            transforms += MatrixDifference(
                left.Data.Transform,right.Data.Transform) <= 0.00001f ? 1 : 0;
            inverses += MatrixDifference(
                left.Data.EngineInverseTransform,
                right.Data.EngineInverseTransform) <= 0.00001f ? 1 : 0;
            translations += Vector3.Distance(
                new Vector3(left.Data.Transform.M41,left.Data.Transform.M42,
                    left.Data.Transform.M43),
                new Vector3(right.Data.Transform.M41,right.Data.Transform.M42,
                    right.Data.Transform.M43)) <= 0.00001f ? 1 : 0;
        }
        return new StaticPlatformComparison(
            paths.Length,equalCount.Length,pairs,names,models,transforms,
            inverses,translations);
    }

    private static float MatrixDifference(Matrix4x4 left,Matrix4x4 right) =>
        MatrixValues(left).Zip(MatrixValues(right))
            .Max(item => MathF.Abs(item.First-item.Second));

    private static void UpsertStaticRenderFieldDefinitions(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<StaticRenderObservation> observations)
    {
        (int Type,string Semantic,string Display,string Kind,string Layout,
            string Notes)[] definitions =
        [
            (0,"static_render_object.renderable","Renderable","object_relationship",
                "object relationship to spModel",
                "Exactly one inline spModel is observed per object."),
            (1,"static_render_object.transform","World transform","matrix4x4",
                "row-vector affine Matrix4x4",
                "Authoritative world placement; translation is row 4."),
            (2,"static_render_object.inverse_transform","Engine inverse transform",
                "matrix4x4","transposed 3x3 basis plus -T*A^T affine Matrix4x4",
                "Not a general mathematical inverse when scale differs from one.")
        ];
        foreach (var definition in definitions)
        {
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
                observedDirectOccurrences = observations.Count,
                payloadBytes = definition.Type == 0 ? (int?)null : 64,
                expectedTargetClass = definition.Type == 0 ? "spModel" : null,
                matrixConvention = definition.Type == 2
                    ? "transpose_3x3_then_negative_translation_dot_transpose"
                    : definition.Type == 1 ? "row_vector_affine" : null,
                mutationStatus = definition.Type is 1 or 2
                    ? "structural_writer_tested_runtime_pending"
                    : "not_enabled"
            }));
            command.Parameters.AddWithValue("$notes",definition.Notes);
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotateStaticRenderFields(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<StaticRenderObservation> observations)
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
        foreach (StaticRenderObservation observation in observations)
        foreach (StaticRenderFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException(
                    "Could not annotate spStaticRenderObject field.");
        }
    }

    private static int InsertStaticRenderVariant(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IEnumerable<StaticRenderObservation> observations,
        string now)
    {
        Dictionary<string,int> corpusCounts = observations
            .GroupBy(item => item.CorpusKey)
            .ToDictionary(group => group.Key,group => group.Count());
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,status,
                discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2','static_render_object_inline_model',
                   'Static placement with inline model','confirmed',
                   $discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """);
        command.Parameters.AddWithValue("$hash",(long)typeHash);
        command.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
        {
            corpusCounts,
            fieldOrder = new[] { 1,2,0 },
            relationshipEncoding = "inline_object",
            targetClass = "spModel"
        }));
        command.Parameters.AddWithValue("$notes",
            "The sole observed serializer shape; serialized-size diversity comes " +
            "from the nested model subtree rather than a static-object subtype.");
        command.Parameters.AddWithValue("$utc",now);
        return Convert.ToInt32(command.ExecuteScalar());
    }

    private static void AssignStaticRenderVariant(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<StaticRenderObservation> observations,
        int variantId)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spStaticRenderObject decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.AddWithValue("$variant",variantId);
        foreach (StaticRenderObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.ExecuteNonQuery();
        }
    }

    private sealed record StaticRenderFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record StaticRenderFieldAnnotation(
        int FieldIndex,int FieldType,string Semantic,string Layout,
        string DecodedJson);
    private sealed record StaticRenderObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string PlatformKey,
        string CanonicalPath,string ObjectName,SmoStaticRenderObjectData Data,
        string ModelName,string SerializedSha256,
        IReadOnlyList<StaticRenderFieldAnnotation> Fields);
    private sealed record StaticMatrixProfile(
        int Objects,int UnitScale,int UniformScale,int Mirrored,
        int MathematicalInverse);
    private sealed record StaticPlatformComparison(
        int CommonResources,int EqualCountResources,int PairedObjects,
        int EqualObjectName,int EqualModelName,int EqualTransform,
        int EqualInverse,int EqualTranslation);
}
