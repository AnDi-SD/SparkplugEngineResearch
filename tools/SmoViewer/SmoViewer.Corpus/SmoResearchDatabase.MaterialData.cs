using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeMaterialData(
        string databasePath,
        SmoResearchClassReport report)
    {
        int[] observedFields = [0,1,2,3,4,6,8,9,10,11,12,17];
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item => item.NamedObjectCount != 0) ||
            report.Fields.Any(item =>
                item.SectionIndex != 0 || item.SectionFromEnd != 0 ||
                !observedFields.Contains(item.FieldType)))
        {
            throw new InvalidDataException(
                "spMaterialData corpus no longer matches the validated unnamed, " +
                "single-section material contract.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database, readOnly: false);
        List<MaterialObservation> observations = LoadMaterialObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected {objectCount} decoded spMaterialData objects, got " +
                $"{observations.Count}.");
        }

        Dictionary<(string Corpus,string Variant),int> variantCounts = observations
            .GroupBy(item => (item.CorpusKey,item.VariantKey))
            .ToDictionary(group => group.Key,group => group.Count());
        RequireMaterialVariantCounts(variantCounts);

        Dictionary<string,string> pcWorking = BuildMaterialPathMap(
            observations,"pc-working",fullSummary: true);
        Dictionary<string,string> pcPristine = BuildMaterialPathMap(
            observations,"pc-pristine",fullSummary: true);
        (int pcCommon,int pcEqual) = ComparePayloadPathMaps(pcWorking,pcPristine);
        if (pcCommon != pcWorking.Count || pcCommon != pcPristine.Count ||
            pcEqual != pcCommon)
        {
            throw new InvalidDataException(
                "Working and pristine PC material structures differ.");
        }

        MaterialCrossPlatformComparison cross = CompareMaterialPlatforms(observations);
        if (cross.CommonResources != 314 || cross.EqualCountResources != 302 ||
            cross.PairedMaterials != 28_793 || cross.EqualOwnState != 28_067)
        {
            throw new InvalidDataException(
                "PC/PS2 material pairing no longer matches the validated corpus: " +
                $"common={cross.CommonResources}, equal-count={cross.EqualCountResources}, " +
                $"paired={cross.PairedMaterials}, equal-state={cross.EqualOwnState}.");
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] commonTokens =
        [
            "spMaterialData","spMaterialDataSerializer","spMaterialSerializer",
            "spPS2MaterialDataSerializer","spMaterialPassLayer",
            "spMaterialTextureLayer","spStdLayer","spTextureStateBlockOld",
            "esfMaterialRenderStates","esfMaterialVertexAlpha",
            "esfMaterialColor","esfMaterialPass","esfMaterialLayer",
            "esfMaterialColorController","esfMaterialLayerStaticUVTransform",
            "esfMaterialLayerTexture","esfMaterialLayerAnimController",
            "esfMaterialLayerUVController","esfMaterialLayerTextureStates"
        ];
        RequireAsciiTokens(pcExecutable.Path,commonTokens);
        RequireAsciiTokens(ps2Executable.Path,commonTokens);
        RequireAsciiTokens(pcExecutable.Path,"spDXMaterialDataSerializer");

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
                   WHERE type_hash=$hash AND variant_key LIKE 'material_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        UpsertMaterialFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateMaterialFields(connection,transaction,observations);
        Dictionary<string,int> variantIds = InsertMaterialVariants(
            connection,transaction,report.TypeHash,observations,now);
        AssignMaterialVariants(connection,transaction,observations,variantIds);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "spMaterialData registration, DX/base serializers, layer classes and field-name strings",
            "The executable names the render-state, vertex-alpha, color, repeated pass, " +
            "layer, texture-state, static-UV and controller relationships recovered " +
            "from the serialized layout.",pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "MIPS spPS2MaterialDataSerializer and shared material/layer field strings",
            "The PS2 executable independently exposes the same material/pass/layer " +
            "serializer vocabulary and legacy texture-state block name.",
            ps2Executable.Sha256,now);

        string counts = string.Join(", ",variantCounts
            .OrderBy(item => item.Key.Corpus,StringComparer.Ordinal)
            .ThenBy(item => item.Key.Variant,StringComparer.Ordinal)
            .Select(item => $"{item.Key.Corpus}/{item.Key.Variant}={item.Value}"));
        Dictionary<string,int> passCounts = observations
            .GroupBy(item => item.Material.Passes.Count.ToString())
            .ToDictionary(group => group.Key,group => group.Count());
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "all unique spMaterialData objects in PC working, PC pristine and PS2 pristine",
            $"{objectCount} unnamed objects strictly decoded through their final " +
            $"terminator. Variants: {counts}. Material pass counts: " +
            string.Join(", ",passCounts.OrderBy(item => item.Key)
                .Select(item => $"{item.Key}={item.Value}")) + ".",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus material ordinal within each SMO",
            $"PC working/pristine material resources are identical: {pcEqual}/{pcCommon}. " +
            $"PC/PS2 common resources: {cross.CommonResources}; equal material counts: " +
            $"{cross.EqualCountResources}; paired materials: {cross.PairedMaterials}; " +
            $"equal normalized own-state: {cross.EqualOwnState}. Differences: " +
            string.Join(", ",cross.DifferenceCounts.OrderBy(item => item.Key)
                .Select(item => $"{item.Key}={item.Value}")) + ".",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='rendering_resource',
                       description='Render states, repeated texture passes, colors and controllers',
                       decode_status='read_only_decode',
                       notes='All observed PC/PS2 material fields and one-to-three pass variants are structurally decoded. Unknown executable-only layer features and mutation safety remain research-only.'
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
            variantIds.Count,observations.Count,evidenceRows,
            $"Twelve observed field IDs and four structural variants decoded across " +
            $"{objectCount} materials. PC pairs {pcEqual}/{pcCommon}; PC/PS2 own-state " +
            $"pairs {cross.EqualOwnState}/{cross.PairedMaterials}. Controller semantics " +
            "are linked, but material mutation safety is not yet established.");
    }

    private static List<MaterialObservation> LoadMaterialObservations(
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.MaterialData);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<MaterialFileLocation>();
        while (reader.Read())
        {
            locations.Add(new MaterialFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var observations = new List<MaterialObservation>();
        foreach (MaterialFileLocation location in locations)
        {
            byte[] bytes = ReadMaterialResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.MaterialData))
            {
                if (!SmoMaterialDataDecoder.TryDecode(
                        document,entry,out SmoMaterialDataInfo? decoded,
                        out string error) || decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                observations.Add(CreateMaterialObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return observations;
    }

    private static byte[] ReadMaterialResource(MaterialFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
        {
            return File.ReadAllBytes(Path.Combine(
                location.SourceRoot,
                location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        }
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
        {
            throw new InvalidDataException("PCK material occurrence is incomplete.");
        }
        string archive = Path.Combine(
            location.SourceRoot,
            location.ContainerPath.Replace('/',Path.DirectorySeparatorChar));
        using var stream = new FileStream(
            archive,FileMode.Open,FileAccess.Read,FileShare.Read);
        stream.Position = location.ByteOffset.Value;
        byte[] data = new byte[checked((int)location.ByteSize)];
        stream.ReadExactly(data);
        return data;
    }

    private static MaterialObservation CreateMaterialObservation(
        MaterialFileLocation location,
        SmoDocument document,
        SmoObjectEntry entry,
        int ordinal,
        SmoMaterialDataInfo material)
    {
        string variantKey = material.UsesLegacyTextureStates
            ? material.Passes.Count == 1
                ? "material_legacy_single_pass"
                : "material_legacy_multi_pass"
            : material.Passes.Count == 1
                ? "material_current_single_pass"
                : "material_current_multi_pass";
        string summaryJson = JsonSerializer.Serialize(material);
        string ownStateJson = JsonSerializer.Serialize(CreateMaterialOwnState(material));
        IReadOnlyList<MaterialFieldAnnotation> fields =
            CreateMaterialFieldAnnotations(document,entry,material);
        return new MaterialObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,
            location.PlatformKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            material,variantKey,summaryJson,ownStateJson,fields);
    }

    private static object CreateMaterialOwnState(SmoMaterialDataInfo material) => new
    {
        material.RenderStates,
        material.UsesVertexAlpha,
        passes = material.Passes.Select(pass => new
        {
            pass.FinalBlendOperation,
            pass.LayerClassId,
            pass.TextureStates,
            pass.StaticUvTransform,
            texture = RelationshipShape(pass.Texture),
            animationController = RelationshipShape(pass.AnimationController),
            uvController = RelationshipShape(pass.UvController)
        }),
        material.Color
    };

    private static string? RelationshipShape(SmoMaterialRelationshipData? value) =>
        value?.StorageKind.ToString();

    private static IReadOnlyList<MaterialFieldAnnotation>
        CreateMaterialFieldAnnotations(
            SmoDocument document,
            SmoObjectEntry entry,
            SmoMaterialDataInfo material)
    {
        IReadOnlyList<SmoObjectField> direct = SmoObjectFieldReader.Read(document,entry);
        var result = new List<MaterialFieldAnnotation>(direct.Count - 1);
        int offset = 0;
        void Add(string semantic,string layout,object? value)
        {
            SmoObjectField field = direct[offset];
            result.Add(new MaterialFieldAnnotation(
                offset,field.FieldType,semantic,layout,JsonSerializer.Serialize(value)));
            offset++;
        }

        if (direct[offset].FieldType == 1)
            Add("material.vertex_alpha","Boolean true byte",true);
        Add("material.render_states","11 UInt32 engine render-state values",
            material.RenderStates);
        foreach (SmoMaterialPassData pass in material.Passes)
        {
            Add("material.pass","UInt32 FinalBlendOp; repeated-pass boundary",new
            {
                pass = pass.Index,
                finalBlendOperation = pass.FinalBlendOperation
            });
            Add("material.layer","UInt32 layer class ID",new
            {
                pass = pass.Index,
                classId = $"0x{pass.LayerClassId:X8}",
                className = SmoClassRegistry.GetDisplayName(pass.LayerClassId)
            });
            Add(pass.TextureStatesFieldType == 8
                    ? "material.texture_states_legacy"
                    : "material.texture_states",
                "9 UInt32 texture-state values",new
                {
                    pass = pass.Index,
                    states = pass.TextureStates
                });
            if (pass.StaticUvTransform is not null)
            {
                Add("material.static_uv_transform",
                    "UInt32 Boolean plus row-major 3x3 Single matrix",new
                    {
                        pass = pass.Index,
                        pass.StaticUvTransform.Enabled,
                        pass.StaticUvTransform.Matrix3x3
                    });
            }
            if (pass.Texture is not null)
                Add("material.texture","spTextureData object relationship",new
                {
                    pass = pass.Index,
                    relationship = pass.Texture
                });
            if (pass.AnimationController is not null)
            {
                Add("material.animation_controller",
                    "spAnimTexController object relationship",new
                    {
                        pass = pass.Index,
                        relationship = pass.AnimationController
                    });
            }
            if (pass.UvController is not null)
                Add("material.uv_controller","spUVController object relationship",new
                {
                    pass = pass.Index,
                    relationship = pass.UvController
                });
        }
        Add("material.color",
            "four ARGB UInt32 colors plus Single specular power",material.Color);
        Add("material.color_controller",
            "spMaterialColorController object relationship",material.ColorController);
        if (offset != direct.Count - 1 || direct[offset].FieldType != 0 ||
            direct[offset].PayloadSize != 0)
        {
            throw new InvalidDataException(
                "Material annotation cursor did not reach the final terminator.");
        }
        return result.AsReadOnly();
    }

    private static void RequireMaterialVariantCounts(
        IReadOnlyDictionary<(string Corpus,string Variant),int> counts)
    {
        Dictionary<(string Corpus,string Variant),int> expected = new()
        {
            [("pc-working","material_current_single_pass")] = 32_347,
            [("pc-working","material_current_multi_pass")] = 1_002,
            [("pc-working","material_legacy_single_pass")] = 2_974,
            [("pc-working","material_legacy_multi_pass")] = 11,
            [("pc-pristine","material_current_single_pass")] = 32_347,
            [("pc-pristine","material_current_multi_pass")] = 1_002,
            [("pc-pristine","material_legacy_single_pass")] = 2_974,
            [("pc-pristine","material_legacy_multi_pass")] = 11,
            [("ps2-pristine","material_current_single_pass")] = 31_952,
            [("ps2-pristine","material_current_multi_pass")] = 934,
            [("ps2-pristine","material_legacy_single_pass")] = 34
        };
        if (counts.Count != expected.Count || expected.Any(item =>
                !counts.TryGetValue(item.Key,out int count) || count != item.Value))
        {
            throw new InvalidDataException(
                "spMaterialData structural-variant counts changed from the " +
                "validated corpus.");
        }
    }

    private static void UpsertMaterialFieldDefinitions(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<MaterialObservation> observations)
    {
        long Count(int type) => observations.Sum(item =>
            (long)item.Fields.Count(field => field.FieldType == type));
        (int Type,string Semantic,string Display,string Kind,string Layout,
            string Evidence,string Notes)[] definitions =
        [
            (0,"material.render_states","Material render states","uint32_array",
                "11 UInt32 values","confirmed_both_executables_and_full_corpus",
                $"Observed {Count(0)} non-terminal times; a zero-sized field 0 terminates every object."),
            (1,"material.vertex_alpha","Use vertex alpha","boolean",
                "one true byte","confirmed_both_executables_and_full_corpus",
                $"Optional; observed {Count(1)} times."),
            (2,"material.color","Material colors","color_block",
                "ambient/diffuse/specular/emissive ARGB UInt32 plus Single power",
                "confirmed_both_executables_and_full_corpus",
                $"Observed once in every material: {Count(2)} times."),
            (3,"material.pass","Material pass","uint32_enum",
                "UInt32 FinalBlendOp and repeated-pass boundary",
                "confirmed_both_executables_and_full_corpus",
                $"Observed {Count(3)} times; one to three passes per material."),
            (4,"material.layer","Material layer class","class_id",
                "UInt32 class ID","confirmed_both_executables_and_full_corpus",
                $"Observed {Count(4)} times; all concrete values are spStdLayer."),
            (6,"material.color_controller","Material color controller","object_relationship",
                "spMaterialColorController ID/reference/inline relationship",
                "confirmed_both_executables_and_full_corpus",
                $"Observed once in every material: {Count(6)} times; PS2 concrete values are null."),
            (8,"material.texture_states_legacy","Legacy texture states","uint32_array",
                "9 UInt32 values (spTextureStateBlockOld)",
                "confirmed_both_executables_and_full_corpus",
                $"Observed {Count(8)} pass blocks."),
            (9,"material.static_uv_transform","Static UV transform","matrix3x3",
                "UInt32 Boolean plus row-major 3x3 Single matrix",
                "confirmed_both_executables_and_full_corpus",
                $"Optional; observed {Count(9)} pass blocks."),
            (10,"material.texture","Layer texture","object_relationship",
                "spTextureData ID/reference/inline relationship",
                "confirmed_both_executables_and_full_corpus",
                $"Optional; observed {Count(10)} pass relationships."),
            (11,"material.animation_controller","Layer animation controller","object_relationship",
                "spAnimTexController ID/reference/inline relationship",
                "confirmed_both_executables_and_full_corpus",
                $"Optional; observed {Count(11)} inline relationships."),
            (12,"material.uv_controller","Layer UV controller","object_relationship",
                "spUVController ID/reference/inline relationship",
                "confirmed_both_executables_and_full_corpus",
                $"Optional; observed {Count(12)} inline relationships."),
            (17,"material.texture_states","Texture states","uint32_array",
                "9 UInt32 values","confirmed_both_executables_and_full_corpus",
                $"Observed {Count(17)} current-format pass blocks.")
        ];
        foreach (var definition in definitions)
        {
            using SqliteCommand command = CreateCommand(connection,transaction,"""
                INSERT INTO field_definitions(
                    type_hash,scope_kind,scope_key,section_from_end,field_type,
                    occurrence,semantic_key,display_name,value_kind,payload_layout,
                    editable_status,evidence_status,constraints_json,notes)
                VALUES($hash,'common','pc_ps2',0,$type,-1,$semantic,$display,$kind,
                       $layout,'read_only_research',$evidence,$constraints,$notes)
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
            command.Parameters.AddWithValue("$evidence",definition.Evidence);
            command.Parameters.AddWithValue("$constraints",JsonSerializer.Serialize(new
            {
                observedDirectOccurrences = Count(definition.Type),
                editable = false,
                mutationStatus = "not_tested"
            }));
            command.Parameters.AddWithValue("$notes",definition.Notes);
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotateMaterialFields(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<MaterialObservation> observations)
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
        foreach (MaterialObservation observation in observations)
        foreach (MaterialFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException("Could not annotate material field.");
        }
    }

    private static Dictionary<string,int> InsertMaterialVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<MaterialObservation> observations,
        string now)
    {
        (string Key,string Display,string Notes)[] variants =
        [
            ("material_current_single_pass","Current single-pass material",
                "One pass using field 17 texture states."),
            ("material_current_multi_pass","Current multi-pass material",
                "Two or three repeated passes using field 17 texture states."),
            ("material_legacy_single_pass","Legacy single-pass material",
                "One pass using field 8 spTextureStateBlockOld states."),
            ("material_legacy_multi_pass","Legacy multi-pass material",
                "Two or three repeated passes using field 8 states; observed only on PC.")
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
                textureStateField = key.Contains("legacy",StringComparison.Ordinal) ? 8 : 17,
                passCount = key.Contains("single",StringComparison.Ordinal) ? "1" : "2-3"
            }));
            command.Parameters.AddWithValue("$notes",notes);
            command.Parameters.AddWithValue("$utc",now);
            result.Add(key,Convert.ToInt32(command.ExecuteScalar()));
        }
        return result;
    }

    private static void AssignMaterialVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<MaterialObservation> observations,
        IReadOnlyDictionary<string,int> variantIds)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spMaterialData decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.Add("$variant",SqliteType.Integer);
        foreach (MaterialObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$variant"].Value = variantIds[observation.VariantKey];
            command.ExecuteNonQuery();
        }
    }

    private static Dictionary<string,string> BuildMaterialPathMap(
        IEnumerable<MaterialObservation> observations,
        string corpusKey,
        bool fullSummary) => observations
        .Where(item => item.CorpusKey.Equals(corpusKey,StringComparison.Ordinal))
        .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
        .ToDictionary(
            group => group.Key,
            group => string.Join("|",group.OrderBy(item => item.Ordinal)
                .Select(item => fullSummary ? item.SummaryJson : item.OwnStateJson)),
            StringComparer.Ordinal);

    private static MaterialCrossPlatformComparison CompareMaterialPlatforms(
        IReadOnlyList<MaterialObservation> observations)
    {
        Dictionary<string,MaterialObservation[]> pc = observations
            .Where(item => item.CorpusKey == "pc-pristine")
            .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
            .ToDictionary(group => group.Key,
                group => group.OrderBy(item => item.Ordinal).ToArray(),
                StringComparer.Ordinal);
        Dictionary<string,MaterialObservation[]> ps2 = observations
            .Where(item => item.CorpusKey == "ps2-pristine")
            .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
            .ToDictionary(group => group.Key,
                group => group.OrderBy(item => item.Ordinal).ToArray(),
                StringComparer.Ordinal);
        string[] paths = pc.Keys.Intersect(ps2.Keys,StringComparer.Ordinal).ToArray();
        int equalCountResources = paths.Count(path => pc[path].Length == ps2[path].Length);
        int paired = 0;
        int equal = 0;
        var differences = new Dictionary<string,int>(StringComparer.Ordinal)
        {
            ["final_blend"] = 0,
            ["render_states"] = 0,
            ["texture_states"] = 0,
            ["texture_relationship"] = 0,
            ["pass_count"] = 0,
            ["uv_relationship"] = 0,
            ["material_colors"] = 0,
            ["vertex_alpha"] = 0,
            ["static_uv"] = 0,
            ["specular_power"] = 0
        };
        foreach (string path in paths)
        {
            if (pc[path].Length != ps2[path].Length)
                continue;
            int count = pc[path].Length;
            paired += count;
            for (int index = 0; index < count; index++)
            {
                SmoMaterialDataInfo left = pc[path][index].Material;
                SmoMaterialDataInfo right = ps2[path][index].Material;
                if (pc[path][index].OwnStateJson == ps2[path][index].OwnStateJson)
                    equal++;
                IncrementMaterialDifferences(left,right,differences);
            }
        }
        return new MaterialCrossPlatformComparison(
            paths.Length,equalCountResources,paired,equal,differences);
    }

    private static void IncrementMaterialDifferences(
        SmoMaterialDataInfo left,
        SmoMaterialDataInfo right,
        IDictionary<string,int> counts)
    {
        if (!left.RenderStates.SequenceEqual(right.RenderStates))
            counts["render_states"]++;
        if (left.UsesVertexAlpha != right.UsesVertexAlpha)
            counts["vertex_alpha"]++;
        if (left.Passes.Count != right.Passes.Count)
            counts["pass_count"]++;
        if (left.Color.AmbientArgb != right.Color.AmbientArgb ||
            left.Color.DiffuseArgb != right.Color.DiffuseArgb ||
            left.Color.SpecularArgb != right.Color.SpecularArgb ||
            left.Color.EmissiveArgb != right.Color.EmissiveArgb)
        {
            counts["material_colors"]++;
        }
        if (left.Color.SpecularPower != right.Color.SpecularPower)
            counts["specular_power"]++;
        bool blend = false;
        bool textureStates = false;
        bool textureRelationship = false;
        bool uvRelationship = false;
        bool staticUv = false;
        int passCount = Math.Min(left.Passes.Count,right.Passes.Count);
        for (int index = 0; index < passCount; index++)
        {
            SmoMaterialPassData a = left.Passes[index];
            SmoMaterialPassData b = right.Passes[index];
            blend |= a.FinalBlendOperation != b.FinalBlendOperation;
            textureStates |= !a.TextureStates.SequenceEqual(b.TextureStates);
            textureRelationship |= RelationshipShape(a.Texture) != RelationshipShape(b.Texture);
            uvRelationship |= RelationshipShape(a.UvController) != RelationshipShape(b.UvController);
            staticUv |= JsonSerializer.Serialize(a.StaticUvTransform) !=
                        JsonSerializer.Serialize(b.StaticUvTransform);
        }
        if (blend)
            counts["final_blend"]++;
        if (textureStates)
            counts["texture_states"]++;
        if (textureRelationship)
            counts["texture_relationship"]++;
        if (uvRelationship)
            counts["uv_relationship"]++;
        if (staticUv)
            counts["static_uv"]++;
    }

    private sealed record MaterialFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record MaterialFieldAnnotation(
        int FieldIndex,int FieldType,string Semantic,string Layout,string DecodedJson);
    private sealed record MaterialObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string PlatformKey,
        string CanonicalPath,SmoMaterialDataInfo Material,string VariantKey,
        string SummaryJson,string OwnStateJson,
        IReadOnlyList<MaterialFieldAnnotation> Fields);
    private sealed record MaterialCrossPlatformComparison(
        int CommonResources,int EqualCountResources,int PairedMaterials,
        int EqualOwnState,IReadOnlyDictionary<string,int> DifferenceCounts);
}
