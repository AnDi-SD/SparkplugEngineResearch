using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeModel(
        string databasePath,
        SmoResearchClassReport report)
    {
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item => item.NamedObjectCount != item.UniqueObjectCount) ||
            report.Fields.Any(item =>
                item.SectionFromEnd is < 0 or > 1 ||
                (item.SectionFromEnd == 1 && item.FieldType is < 0 or > 3) ||
                (item.SectionFromEnd == 0 && item.FieldType is < 0 or > 1)))
        {
            throw new InvalidDataException(
                "spModel corpus no longer matches the validated two-section " +
                "spRenderable/spModel contract.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        List<ModelObservation> observations = LoadModelObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (objectCount != 118_720 || observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected 118720 decoded spModel objects, got " +
                $"{observations.Count}/{objectCount}.");
        }

        Dictionary<(string Corpus,string Variant),int> variantCounts = observations
            .GroupBy(item => (item.CorpusKey,item.VariantKey))
            .ToDictionary(group => group.Key,group => group.Count());
        RequireModelVariantCounts(variantCounts);
        if (observations.Sum(item => item.Fields.Count) != 705_928)
        {
            throw new InvalidDataException(
                "spModel non-terminal field total changed from 705928.");
        }

        (int pcCommon,int pcEqual) = CompareModelPcCopies(observations);
        if (pcCommon != 311 || pcEqual != 311)
        {
            throw new InvalidDataException(
                "PC working/pristine model payloads no longer match exactly.");
        }
        ModelPlatformComparison cross = CompareModelPlatforms(observations);
        if (cross != new ModelPlatformComparison(
                227,215,32_067,31_549,32_036,31_946,32_007,32_023,
                32_067,32_067,31_141))
        {
            throw new InvalidDataException(
                "PC/PS2 paired model semantics changed from the validated corpus: " +
                JsonSerializer.Serialize(cross));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] tokens =
        [
            "spModel","spModelSerializer","esfModelBase",
            "esfModelProjectionGroup","spRenderable","spRenderableSerializer",
            "esfRenderableMaterial","esfRenderableFog"
        ];
        RequireAsciiTokens(pcExecutable.Path,tokens);
        RequireAsciiTokens(ps2Executable.Path,tokens);

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
                   WHERE type_hash=$hash AND variant_key LIKE 'model_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        UpsertModelFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateModelFields(connection,transaction,observations);
        Dictionary<string,int> variantIds = InsertModelVariants(
            connection,transaction,report.TypeHash,observations,now);
        AssignModelVariants(connection,transaction,observations,variantIds);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "spModel/spRenderable serializer registrations and field-name strings",
            "The PC executable identifies the model base-mesh and projection-group " +
            "fields plus inherited material and fog fields.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "MIPS spModel/spRenderable serializer registrations and field-name strings",
            "The PS2 executable independently exposes the same two serializer " +
            "sections and named relationships.",ps2Executable.Sha256,now);

        string counts = string.Join(", ",variantCounts
            .OrderBy(item => item.Key.Corpus,StringComparer.Ordinal)
            .ThenBy(item => item.Key.Variant,StringComparer.Ordinal)
            .Select(item => $"{item.Key.Corpus}/{item.Key.Variant}={item.Value}"));
        Dictionary<string,int> encodings = observations
            .SelectMany(item => new[]
            {
                ("material",item.Data.Renderable.Material?.Encoding),
                ("fog",item.Data.Renderable.Fog?.Encoding),
                ("base",(SmoNodeRelationshipEncoding?)item.Data.BaseMesh.Encoding)
            })
            .GroupBy(item => $"{item.Item1}:{item.Item2?.ToString() ?? "absent"}")
            .ToDictionary(group => group.Key,group => group.Count());
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict decode of every unique PC working, PC pristine and PS2 pristine spModel",
            $"{objectCount} objects; 705928 annotated non-terminal fields; seven " +
            $"presence variants. {counts}. Relationship storage: " +
            string.Join(", ",encodings.OrderBy(item => item.Key)
                .Select(item => $"{item.Key}={item.Value}")) + ".",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical path plus spModel ordinal; complete serialized hashes for PC pairs",
            $"PC working/pristine equal model resources: {pcEqual}/{pcCommon}. " +
            $"PC/PS2 common resources: {cross.CommonResources}; equal model counts: " +
            $"{cross.EqualCountResources}; paired models: {cross.PairedModels}. " +
            $"Matches: name={cross.EqualObjectName}, structure={cross.EqualPresenceShape}, " +
            $"AlphaSort={cross.EqualAlphaSort}, Priority={cross.EqualPriority}, " +
            $"effective ProjectionGroup={cross.EqualProjection}, material target=" +
            $"{cross.EqualMaterialTarget}, fog target={cross.EqualFogTarget}, " +
            $"base-mesh target={cross.EqualBaseTarget}.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='renderable_resource',
                       description='Renderable model binding a mesh, material, fog and render ordering state',
                       decode_status='read_only_decode',
                       notes='All seven PC/PS2 field-presence forms and all relationship encodings are strictly decoded. ProjectionGroup semantics beyond the executable name and mutation safety remain untested.'
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
            report.Profiles.Sum(item => item.UniqueResourceCount),variantIds.Count,
            observations.Count,evidenceRows,
            $"Two serializer sections, six semantic fields and seven presence " +
            $"variants decoded across {objectCount} models. PC copies agree in " +
            $"{pcEqual}/{pcCommon} resources; {cross.PairedModels} PC/PS2 models " +
            "were paired. Mutation remains disabled.");
    }

    private static List<ModelObservation> LoadModelObservations(
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.Model);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<ModelFileLocation>();
        while (reader.Read())
        {
            locations.Add(new ModelFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<ModelObservation>();
        foreach (ModelFileLocation location in locations)
        {
            byte[] bytes = ReadModelResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.Model))
            {
                if (!SmoModelDecoder.TryDecode(
                        document,entry,out SmoModelData? decoded,out string error) ||
                    decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                result.Add(CreateModelObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return result;
    }

    private static byte[] ReadModelResource(ModelFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
        {
            return File.ReadAllBytes(Path.Combine(location.SourceRoot,
                location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        }
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
        {
            throw new InvalidDataException("PCK model occurrence is incomplete.");
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

    private static ModelObservation CreateModelObservation(
        ModelFileLocation location,
        SmoDocument document,
        SmoObjectEntry entry,
        int ordinal,
        SmoModelData decoded)
    {
        string variant = GetModelVariant(decoded);
        IReadOnlyList<SmoObjectField> direct = SmoObjectFieldReader.Read(document,entry);
        int firstTerminator = Enumerable.Range(0,direct.Count)
            .First(index => IsModelTerminator(direct[index]));
        var annotations = new List<ModelFieldAnnotation>();
        for (int fieldIndex = 0;fieldIndex < direct.Count;fieldIndex++)
        {
            SmoObjectField field = direct[fieldIndex];
            if (field.FieldType == 0 && field.PayloadSize == 0)
                continue;
            if (!SmoSerializedFieldRegistry.TryDescribeField(
                    entry.TypeHash,direct,fieldIndex,
                    out SmoSerializedFieldDescriptor? descriptor) || descriptor is null)
            {
                throw new InvalidDataException(
                    $"Unregistered spModel field {field.FieldType} in " +
                    $"{location.RelativePath} [{entry.Index}].");
            }
            int sectionFromEnd = fieldIndex < firstTerminator
                ? 1
                : 0;
            object? fieldValue = (sectionFromEnd,field.FieldType) switch
            {
                (1,0) => decoded.Renderable.Material,
                (1,1) => decoded.Renderable.Fog,
                (1,2) => decoded.Renderable.AlphaSortEnable,
                (1,3) => decoded.Renderable.Priority,
                (0,0) => decoded.BaseMesh,
                (0,1) => decoded.ProjectionGroup,
                _ => throw new InvalidDataException("Unexpected spModel field mapping.")
            };
            annotations.Add(new ModelFieldAnnotation(
                fieldIndex,sectionFromEnd,field.FieldType,descriptor.Key,
                descriptor.PayloadLayout,JsonSerializer.Serialize(fieldValue)));
        }
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        return new ModelObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,location.PlatformKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            entry.Name.TrimEnd('\0'),decoded,variant,serializedHash,
            annotations.AsReadOnly());
    }

    private static bool IsModelTerminator(SmoObjectField field) =>
        field.FieldType == 0 && field.PayloadSize == 0;

    private static string GetModelVariant(SmoModelData value) =>
        (value.Renderable.SerializedFieldMask,value.SerializedFieldMask) switch
        {
            (0b0011,0b0001) => "model_legacy_compact",
            (0b1111,0b0001) => "model_no_projection",
            (0b1111,0b0011) => "model_full",
            (0b1101,0b0001) => "model_no_fog_no_projection",
            (0b1101,0b0011) => "model_no_fog",
            (0b1110,0b0001) => "model_no_material_no_projection",
            (0b1110,0b0011) => "model_no_material",
            _ => throw new InvalidDataException(
                $"Unexpected spModel masks renderable=0x" +
                $"{value.Renderable.SerializedFieldMask:X2}, model=0x" +
                $"{value.SerializedFieldMask:X2}.")
        };

    private static void RequireModelVariantCounts(
        IReadOnlyDictionary<(string Corpus,string Variant),int> counts)
    {
        Dictionary<(string Corpus,string Variant),int> expected = new()
        {
            [("pc-working","model_legacy_compact")] = 2,
            [("pc-working","model_no_projection")] = 2_886,
            [("pc-working","model_full")] = 37_489,
            [("pc-working","model_no_fog_no_projection")] = 3,
            [("pc-working","model_no_fog")] = 51,
            [("pc-working","model_no_material_no_projection")] = 43,
            [("pc-working","model_no_material")] = 81,
            [("pc-pristine","model_legacy_compact")] = 2,
            [("pc-pristine","model_no_projection")] = 2_886,
            [("pc-pristine","model_full")] = 37_489,
            [("pc-pristine","model_no_fog_no_projection")] = 3,
            [("pc-pristine","model_no_fog")] = 51,
            [("pc-pristine","model_no_material_no_projection")] = 43,
            [("pc-pristine","model_no_material")] = 81,
            [("ps2-pristine","model_no_projection")] = 31,
            [("ps2-pristine","model_full")] = 37_450,
            [("ps2-pristine","model_no_fog")] = 51,
            [("ps2-pristine","model_no_material")] = 78
        };
        if (counts.Count != expected.Count || expected.Any(item =>
                !counts.TryGetValue(item.Key,out int count) || count != item.Value))
        {
            throw new InvalidDataException(
                "spModel field-presence counts changed: " + string.Join(", ",counts
                    .OrderBy(item => item.Key.Corpus,StringComparer.Ordinal)
                    .ThenBy(item => item.Key.Variant,StringComparer.Ordinal)
                    .Select(item =>
                        $"{item.Key.Corpus}/{item.Key.Variant}={item.Value}")) + ".");
        }
    }

    private static (int Common,int Equal) CompareModelPcCopies(
        IEnumerable<ModelObservation> observations)
    {
        Dictionary<string,ModelObservation[]> Build(string corpus) => observations
            .Where(item => item.CorpusKey == corpus)
            .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
            .ToDictionary(group => group.Key,
                group => group.OrderBy(item => item.Ordinal).ToArray(),
                StringComparer.Ordinal);
        Dictionary<string,ModelObservation[]> working = Build("pc-working");
        Dictionary<string,ModelObservation[]> pristine = Build("pc-pristine");
        string[] paths = working.Keys.Intersect(
            pristine.Keys,StringComparer.Ordinal).ToArray();
        int equal = paths.Count(path =>
            working[path].Select(item => (item.ObjectName,item.SerializedSha256))
                .SequenceEqual(pristine[path].Select(item =>
                    (item.ObjectName,item.SerializedSha256))));
        return (paths.Length,equal);
    }

    private static ModelPlatformComparison CompareModelPlatforms(
        IEnumerable<ModelObservation> observations)
    {
        Dictionary<string,ModelObservation[]> Build(string corpus) => observations
            .Where(item => item.CorpusKey == corpus)
            .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
            .ToDictionary(group => group.Key,
                group => group.OrderBy(item => item.Ordinal).ToArray(),
                StringComparer.Ordinal);
        Dictionary<string,ModelObservation[]> pc = Build("pc-pristine");
        Dictionary<string,ModelObservation[]> ps2 = Build("ps2-pristine");
        string[] paths = pc.Keys.Intersect(ps2.Keys,StringComparer.Ordinal).ToArray();
        string[] equalCount = paths.Where(path =>
            pc[path].Length == ps2[path].Length).ToArray();
        int pairs = 0;
        int names = 0;
        int shapes = 0;
        int alpha = 0;
        int priority = 0;
        int projection = 0;
        int material = 0;
        int fog = 0;
        int baseMesh = 0;
        foreach (string path in equalCount)
        for (int index = 0;index < pc[path].Length;index++)
        {
            ModelObservation left = pc[path][index];
            ModelObservation right = ps2[path][index];
            pairs++;
            names += left.ObjectName == right.ObjectName ? 1 : 0;
            shapes += left.VariantKey == right.VariantKey ? 1 : 0;
            alpha += left.Data.Renderable.AlphaSortEnable ==
                     right.Data.Renderable.AlphaSortEnable ? 1 : 0;
            priority += left.Data.Renderable.Priority ==
                        right.Data.Renderable.Priority ? 1 : 0;
            projection += (left.Data.ProjectionGroup ?? 0) ==
                          (right.Data.ProjectionGroup ?? 0) ? 1 : 0;
            material += left.Data.Renderable.Material?.TargetName ==
                        right.Data.Renderable.Material?.TargetName ? 1 : 0;
            fog += left.Data.Renderable.Fog?.TargetName ==
                   right.Data.Renderable.Fog?.TargetName ? 1 : 0;
            baseMesh += left.Data.BaseMesh.TargetName == right.Data.BaseMesh.TargetName
                ? 1
                : 0;
        }
        return new ModelPlatformComparison(
            paths.Length,equalCount.Length,pairs,names,shapes,alpha,priority,
            projection,material,fog,baseMesh);
    }

    private static void UpsertModelFieldDefinitions(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<ModelObservation> observations)
    {
        long Count(int section,int type) => observations.Sum(item =>
            (long)item.Fields.Count(field =>
                field.SectionFromEnd == section && field.FieldType == type));
        (int Section,int Type,string Semantic,string Display,string Kind,
            string Layout,string Notes)[] definitions =
        [
            (1,0,"renderable.material","Material","object_relationship",
                "object relationship to spMaterialData","Optional material binding."),
            (1,1,"renderable.fog","Fog","object_relationship",
                "object relationship to spFog","Optional fog binding."),
            (1,2,"renderable.alpha_sort","Alpha sort enabled","boolean_u32",
                "UInt32 Boolean 0 or 1","Omitted only by two legacy PC models."),
            (1,3,"renderable.priority","Render priority","uint32",
                "UInt32","Omitted with AlphaSortEnable in two legacy PC models."),
            (0,0,"model.base_mesh","Base mesh","object_relationship",
                "object relationship to spMeshData","Required in every model."),
            (0,1,"model.projection_group","Projection group","uint32",
                "UInt32","Optional; observed values are 0 and 3.")
        ];
        foreach (var definition in definitions)
        {
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',$section,$type,-1,$semantic,$display,
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
            long count = Count(definition.Section,definition.Type);
            command.Parameters.AddWithValue("$hash",(long)typeHash);
            command.Parameters.AddWithValue("$section",definition.Section);
            command.Parameters.AddWithValue("$type",definition.Type);
            command.Parameters.AddWithValue("$semantic",definition.Semantic);
            command.Parameters.AddWithValue("$display",definition.Display);
            command.Parameters.AddWithValue("$kind",definition.Kind);
            command.Parameters.AddWithValue("$layout",definition.Layout);
            command.Parameters.AddWithValue("$constraints",JsonSerializer.Serialize(new
            {
                observedDirectOccurrences = count,
                expectedTargetClass = definition.Kind == "object_relationship"
                    ? definition.Type switch
                    {
                        0 when definition.Section == 1 => "spMaterialData",
                        1 when definition.Section == 1 => "spFog",
                        0 => "spMeshData",
                        _ => null
                    }
                    : null,
                editable = false,
                mutationStatus = "not_tested"
            }));
            command.Parameters.AddWithValue(
                "$notes",$"{definition.Notes} Observed {count} times.");
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotateModelFields(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<ModelObservation> observations)
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
        foreach (ModelObservation observation in observations)
        foreach (ModelFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException("Could not annotate spModel field.");
        }
    }

    private static Dictionary<string,int> InsertModelVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<ModelObservation> observations,
        string now)
    {
        (string Key,string Display,string Notes)[] variants =
        [
            ("model_legacy_compact","Legacy compact model",
                "Material, fog and base mesh use ID-only relationships; render-order fields and projection group are omitted."),
            ("model_no_projection","Model without projection group",
                "Complete renderable section; optional ProjectionGroup is omitted."),
            ("model_full","Full model","All six semantic fields are present."),
            ("model_no_fog_no_projection","Model without fog or projection group",
                "Fog and ProjectionGroup are omitted."),
            ("model_no_fog","Model without fog","Fog is omitted."),
            ("model_no_material_no_projection","Model without material or projection group",
                "Material and ProjectionGroup are omitted."),
            ("model_no_material","Model without material","Material is omitted.")
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

    private static void AssignModelVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<ModelObservation> observations,
        IReadOnlyDictionary<string,int> variantIds)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spModel two-section decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.Add("$variant",SqliteType.Integer);
        foreach (ModelObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$variant"].Value = variantIds[observation.VariantKey];
            command.ExecuteNonQuery();
        }
    }

    private sealed record ModelFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record ModelFieldAnnotation(
        int FieldIndex,int SectionFromEnd,int FieldType,string Semantic,
        string Layout,string DecodedJson);
    private sealed record ModelObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string PlatformKey,
        string CanonicalPath,string ObjectName,SmoModelData Data,string VariantKey,
        string SerializedSha256,IReadOnlyList<ModelFieldAnnotation> Fields);
    private sealed record ModelPlatformComparison(
        int CommonResources,int EqualCountResources,int PairedModels,
        int EqualObjectName,int EqualPresenceShape,int EqualAlphaSort,
        int EqualPriority,int EqualProjection,int EqualMaterialTarget,
        int EqualFogTarget,int EqualBaseTarget);
}
