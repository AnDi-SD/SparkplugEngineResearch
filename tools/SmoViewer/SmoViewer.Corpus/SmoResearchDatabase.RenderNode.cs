using System.Buffers.Binary;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeRenderNode(
        string databasePath,
        SmoResearchClassReport report)
    {
        SmoResearchClassFieldShape? invalidField = report.Fields.FirstOrDefault(
            item => item.SectionFromEnd switch
            {
                0 => item.SectionIndex != 1 || item.FieldType != 0 ||
                     item.PayloadSize < SmoNodeDecoder.IdOnlyRelationshipPayloadSize,
                1 => item.SectionIndex != 0 || item.FieldType is < 0 or > 8 ||
                     !IsValidRenderNodeInheritedFieldSize(
                         item.FieldType,item.PayloadSize),
                _ => true
            });
        SmoResearchClassProfile? invalidProfile = report.Profiles.FirstOrDefault(
            item => item.NamedObjectCount != item.UniqueObjectCount ||
                    item.MinimumSerializedSize < 12);
        int platformCount = report.Profiles
            .Select(item => item.PlatformKey).Distinct().Count();
        if (report.Profiles.Count != 3 || platformCount != 2 ||
            invalidProfile is not null || invalidField is not null)
        {
            throw new InvalidDataException(
                "spRenderNode corpus no longer matches the validated inherited-node " +
                "plus repeated-renderable serializer contract. " +
                $"profiles={report.Profiles.Count}, platforms={platformCount}, " +
                $"invalidProfile={invalidProfile}, invalidField={invalidField}");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        Dictionary<NodeObjectKey,RenderNodeObservationBuilder> builders =
            LoadRenderNodeBuilders(connection);
        Dictionary<NodeTargetKey,NodeTarget> targets =
            LoadRenderNodeTargets(connection);
        LoadRenderNodeFields(connection,builders);

        var fieldCounts = new Dictionary<(string Corpus,string Semantic),long>();
        var encodingCounts = new Dictionary<
            (string Corpus,string Semantic,SmoNodeRelationshipEncoding Encoding),long>();
        var targetCounts = new Dictionary<
            (string Corpus,string Semantic,uint TargetType),long>();
        var observations = new List<RenderNodeObservation>(builders.Count);
        foreach (RenderNodeObservationBuilder builder in builders.Values
                     .OrderBy(item => item.FileId)
                     .ThenBy(item => item.ObjectIndex))
        {
            observations.Add(ValidateRenderNodeObservation(
                builder,targets,fieldCounts,encodingCounts,targetCounts));
        }

        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected {objectCount} decoded spRenderNode objects, got " +
                $"{observations.Count}.");
        }

        Dictionary<string,string> pcWorking = BuildRenderNodePathMap(
            observations,"pc-working");
        Dictionary<string,string> pcPristine = BuildRenderNodePathMap(
            observations,"pc-pristine");
        Dictionary<string,string> ps2Pristine = BuildRenderNodePathMap(
            observations,"ps2-pristine");
        (int pcCommon,int pcEqual) = ComparePayloadPathMaps(
            pcWorking,pcPristine);
        (int platformCommon,int platformEqual) = ComparePayloadPathMaps(
            pcPristine,ps2Pristine);
        if (pcCommon != pcWorking.Count || pcCommon != pcPristine.Count)
        {
            throw new InvalidDataException(
                "Working and pristine PC corpora expose different spRenderNode paths.");
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spRenderNode","spRenderNodeSerializer",
            "spRenderNodeSerializer.cpp","esfRenderNodeRenderable"
        ];
        RequireAsciiTokens(pcExecutable.Path,executableTokens);
        RequireAsciiTokens(ps2Executable.Path,executableTokens);

        RenderNodeFieldDefinition[] definitions = BuildRenderNodeFieldDefinitions(
            observations,fieldCounts,encodingCounts,targetCounts);
        Dictionary<(int SectionFromEnd,int FieldType),RenderNodeFieldDefinition>
            definitionsByField = definitions.ToDictionary(
                item => (item.SectionFromEnd,item.Definition.Type));
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
                   WHERE type_hash=$hash AND variant_key LIKE 'render_node_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        foreach (RenderNodeFieldDefinition definition in definitions)
        {
            UpsertNodeFieldDefinition(
                connection,transaction,report.TypeHash,definition.Definition,
                definition.SectionFromEnd);
        }

        using (SqliteCommand decoded = CreateCommand(connection,transaction,"""
                   UPDATE direct_fields SET semantic_key=$semantic,
                       payload_layout=$layout,decoded_value=$value,is_decoded=1
                   WHERE file_id=$file AND object_index=$object
                     AND field_index=$field;
                   """))
        {
            SqliteParameter semantic = decoded.Parameters.Add(
                "$semantic",SqliteType.Text);
            SqliteParameter layout = decoded.Parameters.Add("$layout",SqliteType.Text);
            SqliteParameter value = decoded.Parameters.Add("$value",SqliteType.Text);
            SqliteParameter file = decoded.Parameters.Add("$file",SqliteType.Integer);
            SqliteParameter objectIndex = decoded.Parameters.Add(
                "$object",SqliteType.Integer);
            SqliteParameter fieldIndex = decoded.Parameters.Add(
                "$field",SqliteType.Integer);
            foreach (RenderNodeObservation observation in observations)
            foreach (RenderNodeFieldObservation field in observation.Fields)
            {
                RenderNodeFieldDefinition definition =
                    definitionsByField[(field.SectionFromEnd,field.FieldType)];
                semantic.Value = definition.Definition.Semantic;
                layout.Value = definition.Definition.Layout;
                value.Value = field.DecodedJson;
                file.Value = observation.FileId;
                objectIndex.Value = observation.ObjectIndex;
                fieldIndex.Value = field.FieldIndex;
                if (decoded.ExecuteNonQuery() != 1)
                {
                    throw new InvalidDataException(
                        $"Could not annotate spRenderNode field " +
                        $"{observation.FileId}:{observation.ObjectIndex}:" +
                        $"{field.FieldIndex}.");
                }
            }
        }

        int variantId;
        using (SqliteCommand variant = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,
                status,discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2','render_node_serializer_contract',
                   'Scene node with renderable list','confirmed',$discriminator,
                   $notes,$utc,$utc);
            SELECT last_insert_rowid();
            """))
        {
            variant.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            variant.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
            {
                inheritedSection = "spNode fields 0..8",
                ownSection = "repeated esfRenderNodeRenderable field 0",
                observedTargetClasses = targetCounts
                    .Where(item => item.Key.Semantic == "render_node.renderable")
                    .Select(item => $"0x{item.Key.TargetType:X8}")
                    .Distinct().Order(StringComparer.Ordinal).ToArray(),
                maximumRenderableCount = observations.Max(item => item.RenderableCount),
                emptyRenderableListAllowed = observations.Any(
                    item => item.RenderableCount == 0)
            }));
            variant.Parameters.AddWithValue("$notes",
                "Renderable composition, billboard/static/animated flags and child " +
                "relationships are orthogonal features, not class subtypes.");
            variant.Parameters.AddWithValue("$utc",now);
            variantId = Convert.ToInt32(variant.ExecuteScalar());
        }
        using (SqliteCommand assignments = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            SELECT o.file_id,o.object_index,$variant,'confirmed',
                   'full PC+PS2 corpus: inherited node and renderable sections validated'
            FROM objects o WHERE o.type_hash=$hash
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,
                          evidence=excluded.evidence;
            """))
        {
            assignments.Parameters.AddWithValue("$variant",variantId);
            assignments.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            assignments.ExecuteNonQuery();
        }

        long directFieldCount = observations.Sum(item => (long)item.Fields.Count);
        long renderableCount = fieldCounts
            .Where(item => item.Key.Semantic == "render_node.renderable")
            .Sum(item => item.Value);
        long childCount = fieldCounts
            .Where(item => item.Key.Semantic == "node.child")
            .Sum(item => item.Value);
        long billboardCount = fieldCounts
            .Where(item => item.Key.Semantic == "node.billboard_axis")
            .Sum(item => item.Value);
        long idOnlyCount = encodingCounts
            .Where(item => item.Key.Semantic == "render_node.renderable" &&
                           item.Key.Encoding == SmoNodeRelationshipEncoding.IdOnly)
            .Sum(item => item.Value);
        string corpusCounts = string.Join(", ",report.Profiles
            .OrderBy(item => item.CorpusKey,StringComparer.Ordinal)
            .Select(item => $"{item.CorpusKey}={item.UniqueObjectCount}"));
        string targetSummary = string.Join(", ",targetCounts
            .Where(item => item.Key.Semantic == "render_node.renderable")
            .GroupBy(item => item.Key.TargetType)
            .OrderByDescending(group => group.Sum(item => item.Value))
            .Select(group =>
                $"{SmoClassRegistry.GetDisplayName(group.Key)}=" +
                $"{group.Sum(item => item.Value)}"));
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "spRenderNode serializer VA 0x00469340..0x00469692",
            "Calls spNodeSerializer first, then loops field 0 " +
            "esfRenderNodeRenderable with relationship header code 7; class " +
            "registration binds hash 0x603625D0.",pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "spRenderNode serializer VA 0x001980A0..0x00198278; hash at 0x00198280",
            "MIPS serializer independently calls spNodeSerializer and loops the " +
            "same field-0 relationship with header code 7.",ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "all concrete spRenderNode objects and both serializer sections",
            $"{objectCount} named objects ({corpusCounts}); {directFieldCount} " +
            $"decoded fields; renderable relations={renderableCount}, child " +
            $"relations={childCount}, billboard fields={billboardCount}, PC ID-only " +
            $"renderables={idOnlyCount}; targets: {targetSummary}.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus normalized inherited state and relationships",
            $"PC working/pristine semantic graphs: {pcEqual}/{pcCommon}; PC " +
            $"pristine/PS2: {platformEqual}/{platformCommon}.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='scene_graph',
                       description='Transform node with repeated renderable relationships',
                       decode_status='read_only_decode',
                       notes='Common PC/PS2 two-section serializer; inherits spNode and owns field 0 esfRenderNodeRenderable.'
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
            1,observations.Count,evidenceRows,
            $"One common two-section serializer contract; {directFieldCount} fields " +
            $"decoded. Renderable relations={renderableCount}, including " +
            $"{idOnlyCount} compact PC ID-only fields; targets: {targetSummary}. " +
            $"Inherited child relations={childCount}, billboard fields={billboardCount}. " +
            $"PC pairs {pcEqual}/{pcCommon}; normalized PC/PS2 graphs " +
            $"{platformEqual}/{platformCommon}. Mutation safety remains untested.");
    }

    private static Dictionary<NodeObjectKey,RenderNodeObservationBuilder>
        LoadRenderNodeBuilders(SqliteConnection connection)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT o.file_id,o.object_index,o.object_id,c.corpus_key,p.platform_key,
                   f.relative_path,o.name,o.serialized_size,o.field_shape,
                   (SELECT COUNT(*) FROM direct_fields d
                    WHERE d.file_id=o.file_id AND d.object_index=o.object_index
                      AND d.is_section_terminator=1)
            FROM objects o
            JOIN files f ON f.id=o.file_id
            JOIN corpora c ON c.id=f.corpus_id
            JOIN platforms p ON p.id=c.platform_id
            WHERE o.type_hash=$hash
            ORDER BY c.id,f.normalized_path,o.object_index;
            """;
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.RenderNode);
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new Dictionary<NodeObjectKey,RenderNodeObservationBuilder>();
        while (reader.Read())
        {
            var builder = new RenderNodeObservationBuilder(
                reader.GetInt32(0),reader.GetInt32(1),
                checked((uint)reader.GetInt64(2)),reader.GetString(3),
                reader.GetString(4),GetCanonicalResourcePath(reader.GetString(5))
                    .ToLowerInvariant(),reader.GetString(6).TrimEnd('\0'),
                reader.GetInt32(7),reader.GetString(8),reader.GetInt32(9));
            result.Add(new NodeObjectKey(builder.FileId,builder.ObjectIndex),builder);
        }
        return result;
    }

    private static Dictionary<NodeTargetKey,NodeTarget> LoadRenderNodeTargets(
        SqliteConnection connection)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT o.file_id,o.object_id,o.object_index,o.type_hash,o.name,
                   o.serialized_size,o.signature_matches,o.within_data_section
            FROM objects o
            WHERE EXISTS(
                SELECT 1 FROM objects node
                WHERE node.file_id=o.file_id AND node.type_hash=$hash)
            ORDER BY o.file_id,o.object_index;
            """;
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.RenderNode);
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new Dictionary<NodeTargetKey,NodeTarget>();
        while (reader.Read())
        {
            var key = new NodeTargetKey(
                reader.GetInt32(0),checked((uint)reader.GetInt64(1)));
            if (!result.TryAdd(key,new NodeTarget(
                    reader.GetInt32(2),checked((uint)reader.GetInt64(3)),
                    reader.GetString(4).TrimEnd('\0'),reader.GetInt32(5),
                    reader.GetBoolean(6),reader.GetBoolean(7))))
            {
                throw new InvalidDataException(
                    $"Duplicate object ID 0x{key.ObjectId:X8} in file {key.FileId}.");
            }
        }
        return result;
    }

    private static void LoadRenderNodeFields(
        SqliteConnection connection,
        IReadOnlyDictionary<NodeObjectKey,RenderNodeObservationBuilder> builders)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT d.file_id,d.object_index,d.field_index,d.section_index,
                   d.field_type,d.occurrence,d.payload_size,d.payload_preview
            FROM direct_fields d
            JOIN objects o ON o.file_id=d.file_id
                          AND o.object_index=d.object_index
            WHERE o.type_hash=$hash AND d.is_section_terminator=0
            ORDER BY d.file_id,d.object_index,d.field_index;
            """;
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.RenderNode);
        using SqliteDataReader reader = command.ExecuteReader();
        while (reader.Read())
        {
            var key = new NodeObjectKey(reader.GetInt32(0),reader.GetInt32(1));
            if (!builders.TryGetValue(key,out RenderNodeObservationBuilder? builder))
                throw new InvalidDataException("Orphan spRenderNode direct field row.");
            builder.Fields.Add(new NodeRawField(
                reader.GetInt32(2),reader.GetInt32(3),reader.GetInt32(4),
                reader.GetInt32(5),checked((uint)reader.GetInt64(6)),
                reader.GetFieldValue<byte[]>(7)));
        }
    }

    private static RenderNodeObservation ValidateRenderNodeObservation(
        RenderNodeObservationBuilder builder,
        IReadOnlyDictionary<NodeTargetKey,NodeTarget> targets,
        IDictionary<(string Corpus,string Semantic),long> fieldCounts,
        IDictionary<(string Corpus,string Semantic,SmoNodeRelationshipEncoding Encoding),long>
            encodingCounts,
        IDictionary<(string Corpus,string Semantic,uint TargetType),long> targetCounts)
    {
        if (builder.SectionCount != 2)
            throw new InvalidDataException("spRenderNode must contain exactly two sections.");
        var seenNodeScalars = new HashSet<int>();
        var decoded = new List<RenderNodeFieldObservation>(builder.Fields.Count);
        var scalarSignatures = new Dictionary<int,string>
        {
            [0] = "000000000000000000000000",
            [1] = "0000000000000000000000000000803F",
            [2] = "0000803F0000803F0000803F",
            [3] = "0", [4] = "0", [6] = "0", [8] = "0"
        };
        int previousNodeRank = -1;
        int renderableCount = 0;
        foreach (NodeRawField field in builder.Fields)
        {
            if (field.PayloadPreview.Length != Math.Min(48,field.PayloadSize))
                throw new InvalidDataException("Invalid spRenderNode payload preview.");
            if (field.SectionIndex == 1)
            {
                if (field.FieldType != 0 ||
                    field.PayloadSize < SmoNodeDecoder.IdOnlyRelationshipPayloadSize)
                {
                    throw new InvalidDataException(
                        "Invalid spRenderNode own serializer field.");
                }
                DecodedRenderNodeRelationship relationship =
                    DecodeRenderNodeRelationship(
                        builder,field,targets,"render_node.renderable",
                        target => target.TypeHash is
                            SmoClassIds.Model or SmoClassIds.Skin or
                            SmoClassIds.ParticleSystem or SmoClassIds.LensFlare);
                CountRenderNodeRelationship(
                    builder.CorpusKey,relationship,"render_node.renderable",
                    fieldCounts,encodingCounts,targetCounts);
                decoded.Add(new RenderNodeFieldObservation(
                    field.FieldIndex,0,field.FieldType,
                    relationship.DecodedJson,relationship.SemanticSignature));
                renderableCount++;
                continue;
            }
            if (field.SectionIndex != 0 || field.FieldType is < 0 or > 8 ||
                !IsValidRenderNodeInheritedFieldSize(
                    field.FieldType,checked((int)field.PayloadSize)) ||
                (field.FieldType is not 5 and not 7 &&
                 !seenNodeScalars.Add(field.FieldType)))
            {
                throw new InvalidDataException(
                    $"Invalid inherited spNode field in {builder.CorpusKey}:" +
                    $"{builder.CanonicalPath} [{builder.ObjectIndex}].");
            }
            int rank = GetNodeFieldRank(field.FieldType);
            if (rank < previousNodeRank)
                throw new InvalidDataException("Inherited spNode fields violate order.");
            previousNodeRank = rank;
            string semantic = GetNodeSemantic(field.FieldType);
            fieldCounts.Increment((builder.CorpusKey,semantic));
            string json;
            string signature;
            switch (field.FieldType)
            {
                case 0:
                case 2:
                {
                    System.Numerics.Vector3 value = ReadNodeVector(field.PayloadPreview);
                    json = JsonSerializer.Serialize(new[] { value.X,value.Y,value.Z });
                    signature = Convert.ToHexString(field.PayloadPreview);
                    break;
                }
                case 1:
                {
                    System.Numerics.Quaternion value =
                        ReadNodeQuaternion(field.PayloadPreview);
                    json = JsonSerializer.Serialize(
                        new[] { value.X,value.Y,value.Z,value.W });
                    signature = Convert.ToHexString(field.PayloadPreview);
                    break;
                }
                case 3:
                case 4:
                case 8:
                    if (field.PayloadPreview[0] > 1 ||
                        (field.FieldType is 3 or 4 && field.PayloadPreview[0] != 1))
                        throw new InvalidDataException("Invalid inherited Boolean value.");
                    bool boolean = field.PayloadPreview[0] != 0;
                    json = JsonSerializer.Serialize(boolean);
                    signature = boolean ? "1" : "0";
                    break;
                case 6:
                    uint axis = BinaryPrimitives.ReadUInt32LittleEndian(
                        field.PayloadPreview);
                    if (axis is not 1 and not 2)
                        throw new InvalidDataException("Invalid billboard axis.");
                    json = JsonSerializer.Serialize(axis);
                    signature = axis.ToString(
                        System.Globalization.CultureInfo.InvariantCulture);
                    break;
                case 5:
                case 7:
                {
                    DecodedRenderNodeRelationship relationship =
                        DecodeRenderNodeRelationship(
                            builder,field,targets,semantic,
                            target => field.FieldType != 7 ||
                                      target.TypeHash == SmoClassIds.CollisionInfo);
                    CountRenderNodeRelationship(
                        builder.CorpusKey,relationship,semantic,
                        fieldCounts,encodingCounts,targetCounts,
                        fieldAlreadyCounted: true);
                    json = relationship.DecodedJson;
                    signature = relationship.SemanticSignature;
                    break;
                }
                default:
                    throw new InvalidDataException("Unexpected inherited node field.");
            }
            decoded.Add(new RenderNodeFieldObservation(
                field.FieldIndex,1,field.FieldType,json,signature));
            if (field.FieldType is not 5 and not 7)
                scalarSignatures[field.FieldType] = signature;
        }

        string graphSignature = string.Join("|",decoded
            .Where(item => item.FieldType is 5 or 7 || item.SectionFromEnd == 0)
            .Select(item => $"s{item.SectionFromEnd}f{item.FieldType}:" +
                            item.SemanticSignature));
        string semanticSignature = builder.Name.ToLowerInvariant() + "|" +
            string.Join("|",new[] { 0,1,2,3,4,6,8 }.Select(type =>
                $"f{type}:{scalarSignatures[type]}")) + "|" + graphSignature;
        return new RenderNodeObservation(
            builder.FileId,builder.ObjectIndex,builder.CorpusKey,
            builder.CanonicalPath,builder.Name,decoded.AsReadOnly(),
            renderableCount,semanticSignature);
    }

    private static DecodedRenderNodeRelationship DecodeRenderNodeRelationship(
        RenderNodeObservationBuilder builder,
        NodeRawField field,
        IReadOnlyDictionary<NodeTargetKey,NodeTarget> targets,
        string semantic,
        Func<NodeTarget,bool> targetValidator)
    {
        if (!SmoNodeDecoder.TryDecodeRelationshipPrefix(
                field.PayloadPreview,field.PayloadSize,
                out uint targetId,out uint inlineSize,
                out SmoNodeRelationshipEncoding encoding) ||
            !targets.TryGetValue(
                new NodeTargetKey(builder.FileId,targetId),out NodeTarget? target) ||
            !target.SignatureMatches || !target.WithinDataSection ||
            !targetValidator(target) ||
            (inlineSize > 0 && inlineSize != target.SerializedSize) ||
            (encoding == SmoNodeRelationshipEncoding.InlineObject &&
             (field.PayloadPreview.Length < 16 ||
              BinaryPrimitives.ReadUInt32LittleEndian(
                  field.PayloadPreview.AsSpan(8)) != target.TypeHash ||
              !field.PayloadPreview.AsSpan(12,4).SequenceEqual("SBOO"u8))))
        {
            throw new InvalidDataException(
                $"Invalid {semantic} relationship in {builder.CorpusKey}:" +
                $"{builder.CanonicalPath} [{builder.ObjectIndex}].");
        }
        string encodingName = encoding switch
        {
            SmoNodeRelationshipEncoding.IdOnly => "id_only",
            SmoNodeRelationshipEncoding.SizedReference => "sized_reference",
            _ => "inline_object"
        };
        string json = JsonSerializer.Serialize(new
        {
            objectId = targetId,
            targetObjectIndex = target.ObjectIndex,
            targetTypeHash = $"0x{target.TypeHash:X8}",
            targetClass = SmoClassRegistry.GetDisplayName(target.TypeHash),
            targetName = target.Name,
            encoding = encodingName,
            inlineSerializedSize = inlineSize
        });
        return new DecodedRenderNodeRelationship(
            target.TypeHash,encoding,json,
            $"0x{target.TypeHash:X8}:{target.Name.ToLowerInvariant()}");
    }

    private static void CountRenderNodeRelationship(
        string corpusKey,
        DecodedRenderNodeRelationship relationship,
        string semantic,
        IDictionary<(string Corpus,string Semantic),long> fieldCounts,
        IDictionary<(string Corpus,string Semantic,SmoNodeRelationshipEncoding Encoding),long>
            encodingCounts,
        IDictionary<(string Corpus,string Semantic,uint TargetType),long> targetCounts,
        bool fieldAlreadyCounted = false)
    {
        if (!fieldAlreadyCounted)
            fieldCounts.Increment((corpusKey,semantic));
        encodingCounts.Increment((corpusKey,semantic,relationship.Encoding));
        targetCounts.Increment((corpusKey,semantic,relationship.TargetType));
    }

    private static RenderNodeFieldDefinition[] BuildRenderNodeFieldDefinitions(
        IReadOnlyList<RenderNodeObservation> observations,
        IReadOnlyDictionary<(string Corpus,string Semantic),long> fieldCounts,
        IReadOnlyDictionary<
            (string Corpus,string Semantic,SmoNodeRelationshipEncoding Encoding),long>
            encodingCounts,
        IReadOnlyDictionary<(string Corpus,string Semantic,uint TargetType),long>
            targetCounts)
    {
        object Counts(string semantic) => fieldCounts
            .Where(item => item.Key.Semantic == semantic)
            .OrderBy(item => item.Key.Corpus,StringComparer.Ordinal)
            .ToDictionary(item => item.Key.Corpus,item => item.Value);
        string ScalarConstraints(string semantic,object defaultValue,params int[] sizes) =>
            JsonSerializer.Serialize(new
            {
                observedOccurrences = Counts(semantic),
                payloadBytes = sizes,
                executableDefault = defaultValue,
                inheritedFrom = "spNode",
                mutationStatus = "not_tested"
            });
        string RelationshipConstraints(string semantic) => JsonSerializer.Serialize(new
        {
            observedOccurrences = Counts(semantic),
            encodings = encodingCounts.Where(item => item.Key.Semantic == semantic)
                .GroupBy(item => item.Key.Encoding)
                .ToDictionary(group => group.Key.ToString(),
                    group => group.Sum(item => item.Value)),
            targetClasses = targetCounts.Where(item => item.Key.Semantic == semantic)
                .GroupBy(item => item.Key.TargetType)
                .ToDictionary(group => $"0x{group.Key:X8}",
                    group => group.Sum(item => item.Value)),
            repeated = true,
            mutationStatus = "not_tested"
        });
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> node =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(SmoClassIds.Node);
        NodeFieldDefinition NodeDefinition(
            int type,string kind,string constraints,string notes) =>
            new(type,node[type].Key,node[type].DisplayName,kind,
                node[type].PayloadLayout,
                "confirmed_inherited_both_executables_and_full_corpus",
                constraints,notes);
        return
        [
            new(1,NodeDefinition(0,"vector3",
                ScalarConstraints("node.position",new[] { 0f,0f,0f },12),
                "Inherited optional position; zero is omitted.")),
            new(1,NodeDefinition(1,"quaternion",
                ScalarConstraints("node.rotation",new[] { 0f,0f,0f,1f },16),
                "Inherited optional identity rotation.")),
            new(1,NodeDefinition(2,"vector3",
                ScalarConstraints("node.scale",new[] { 1f,1f,1f },12),
                "Inherited optional unit scale.")),
            new(1,NodeDefinition(3,"boolean",
                ScalarConstraints("node.is_bone",false,1),
                "Inherited optional bone flag.")),
            new(1,NodeDefinition(4,"boolean",
                ScalarConstraints("node.is_static",false,1),
                "Inherited optional static flag.")),
            new(1,NodeDefinition(5,"object_relationship",
                RelationshipConstraints("node.child"),
                "Inherited repeated logical child relationship.")),
            new(1,NodeDefinition(6,"uint32_enum",
                JsonSerializer.Serialize(new
                {
                    observedOccurrences = Counts("node.billboard_axis"),
                    supportedValues = new[] { 1,2 }, executableDefault = 0,
                    inheritedFrom = "spNode", mutationStatus = "not_tested"
                }),
                "Inherited billboard field; unlike concrete spNode, observed here.")),
            new(1,NodeDefinition(7,"object_relationship",
                RelationshipConstraints("node.collision"),
                "Inherited repeated collision-info relationship.")),
            new(1,NodeDefinition(8,"boolean",
                ScalarConstraints("node.is_animated",false,1),
                "Inherited animated flag; legacy PC objects may omit false.")),
            new(0,new NodeFieldDefinition(
                0,"render_node.renderable","Renderable","object_relationship",
                "UInt32 object ID; optional UInt32 inline size and inline SBOO",
                "confirmed_both_executables_and_full_corpus",
                JsonSerializer.Serialize(new
                {
                    observedOccurrences = Counts("render_node.renderable"),
                    encodings = encodingCounts
                        .Where(item => item.Key.Semantic == "render_node.renderable")
                        .GroupBy(item => item.Key.Encoding)
                        .ToDictionary(group => group.Key.ToString(),
                            group => group.Sum(item => item.Value)),
                    targetClasses = targetCounts
                        .Where(item => item.Key.Semantic == "render_node.renderable")
                        .GroupBy(item => item.Key.TargetType)
                        .ToDictionary(group => $"0x{group.Key:X8}",
                            group => group.Sum(item => item.Value)),
                    emptyObjectCount = observations.Count(
                        item => item.RenderableCount == 0),
                    maximumPerObject = observations.Max(
                        item => item.RenderableCount),
                    repeated = true, mutationStatus = "not_tested"
                }),
                "Repeated own field; empty lists are valid. Three distinct PC " +
                "fields use ID-only encoding (six rows across both PC corpora)."))
        ];
    }

    private static string GetNodeSemantic(int fieldType) => fieldType switch
    {
        0 => "node.position", 1 => "node.rotation", 2 => "node.scale",
        3 => "node.is_bone", 4 => "node.is_static", 5 => "node.child",
        6 => "node.billboard_axis", 7 => "node.collision",
        8 => "node.is_animated",
        _ => throw new ArgumentOutOfRangeException(nameof(fieldType))
    };

    private static bool IsValidRenderNodeInheritedFieldSize(
        int fieldType,int payloadSize) =>
        (fieldType == 7 &&
         payloadSize == SmoNodeDecoder.IdOnlyRelationshipPayloadSize) ||
        IsValidNodeFieldSize(fieldType,payloadSize);

    private static Dictionary<string,string> BuildRenderNodePathMap(
        IEnumerable<RenderNodeObservation> observations,
        string corpusKey) => observations
        .Where(item => item.CorpusKey.Equals(corpusKey,StringComparison.Ordinal))
        .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
        .ToDictionary(
            group => group.Key,
            group => string.Join("\n",group
                .Select(item => item.SemanticSignature)
                .Order(StringComparer.Ordinal)),
            StringComparer.Ordinal);

    private sealed class RenderNodeObservationBuilder
    {
        public RenderNodeObservationBuilder(
            int fileId,int objectIndex,uint objectId,string corpusKey,
            string platformKey,string canonicalPath,string name,
            int serializedSize,string fieldShape,int sectionCount)
        {
            FileId=fileId; ObjectIndex=objectIndex; ObjectId=objectId;
            CorpusKey=corpusKey; PlatformKey=platformKey;
            CanonicalPath=canonicalPath; Name=name;
            SerializedSize=serializedSize; FieldShape=fieldShape;
            SectionCount=sectionCount;
        }
        public int FileId { get; }
        public int ObjectIndex { get; }
        public uint ObjectId { get; }
        public string CorpusKey { get; }
        public string PlatformKey { get; }
        public string CanonicalPath { get; }
        public string Name { get; }
        public int SerializedSize { get; }
        public string FieldShape { get; }
        public int SectionCount { get; }
        public List<NodeRawField> Fields { get; } = new();
    }

    private sealed record RenderNodeFieldObservation(
        int FieldIndex,int SectionFromEnd,int FieldType,
        string DecodedJson,string SemanticSignature);
    private sealed record RenderNodeObservation(
        int FileId,int ObjectIndex,string CorpusKey,string CanonicalPath,string Name,
        IReadOnlyList<RenderNodeFieldObservation> Fields,int RenderableCount,
        string SemanticSignature);
    private sealed record RenderNodeFieldDefinition(
        int SectionFromEnd,NodeFieldDefinition Definition);
    private sealed record DecodedRenderNodeRelationship(
        uint TargetType,SmoNodeRelationshipEncoding Encoding,
        string DecodedJson,string SemanticSignature);
}
