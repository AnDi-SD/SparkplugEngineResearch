using System.Buffers.Binary;
using System.Numerics;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeNode(
        string databasePath,
        SmoResearchClassReport report)
    {
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item =>
                item.NamedObjectCount != item.UniqueObjectCount ||
                item.MinimumSerializedSize < 11) ||
            report.Fields.Any(item =>
                item.SectionIndex != 0 || item.SectionFromEnd != 0 ||
                item.FieldType is < 0 or > 8 ||
                item.FieldType == 6 ||
                !IsValidNodeFieldSize(item.FieldType,item.PayloadSize)))
        {
            throw new InvalidDataException(
                "spNode corpus no longer matches the validated single-section " +
                "serializer contract.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        Dictionary<NodeObjectKey,NodeObservationBuilder> builders =
            LoadNodeBuilders(connection);
        Dictionary<NodeTargetKey,NodeTarget> targets = LoadNodeTargets(connection);
        LoadNodeFields(connection,builders);

        var observations = new List<NodeObservation>(builders.Count);
        var fieldCounts = new Dictionary<(string Corpus,int Type),long>();
        var relationshipEncodingCounts = new Dictionary<
            (string Corpus,int Type,SmoNodeRelationshipEncoding Encoding),long>();
        var relationshipTargetCounts = new Dictionary<
            (string Corpus,int Type,uint TargetType),long>();
        foreach (NodeObservationBuilder builder in builders.Values.OrderBy(
                     item => item.FileId).ThenBy(item => item.ObjectIndex))
        {
            observations.Add(ValidateNodeObservation(
                builder,targets,fieldCounts,relationshipEncodingCounts,
                relationshipTargetCounts));
        }

        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (observations.Count != objectCount ||
            observations.Any(item => item.Fields.Any(field => field.FieldType == 6)))
        {
            throw new InvalidDataException(
                $"Expected {objectCount} decoded spNode objects and no concrete " +
                "spNode billboard field.");
        }

        Dictionary<string,string> pcWorking = BuildNodePathMap(
            observations,"pc-working");
        Dictionary<string,string> pcPristine = BuildNodePathMap(
            observations,"pc-pristine");
        Dictionary<string,string> ps2Pristine = BuildNodePathMap(
            observations,"ps2-pristine");
        (int pcCommon,int pcEqual) = ComparePayloadPathMaps(
            pcWorking,pcPristine);
        (int platformCommon,int platformEqual) = ComparePayloadPathMaps(
            pcPristine,ps2Pristine);
        if (pcCommon != pcWorking.Count || pcCommon != pcPristine.Count)
        {
            throw new InvalidDataException(
                "Working and pristine PC corpora do not expose matching spNode paths.");
        }
        string[] pcDifferences = pcWorking.Keys
            .Intersect(pcPristine.Keys,StringComparer.Ordinal)
            .Where(path => !pcWorking[path].Equals(
                pcPristine[path],StringComparison.Ordinal))
            .Order(StringComparer.Ordinal)
            .Take(12)
            .ToArray();
        string[] platformDifferences = pcPristine.Keys
            .Intersect(ps2Pristine.Keys,StringComparer.Ordinal)
            .Where(path => !pcPristine[path].Equals(
                ps2Pristine[path],StringComparison.Ordinal))
            .Order(StringComparer.Ordinal)
            .Take(12)
            .ToArray();

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] executableTokens =
        [
            "spNode","spNodeSerializer","esfNodePosition","esfNodeRotation",
            "esfNodeScale","esfNodeIsBone","esfNodeIsStatic","esfNodeChild",
            "esfNodeBillboardAxis","esfNodeCollision","esfNodeIsAnimated"
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
                   WHERE type_hash=$hash AND variant_key LIKE 'node_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        NodeFieldDefinition[] definitions = BuildNodeFieldDefinitions(
            observations,fieldCounts,relationshipEncodingCounts,
            relationshipTargetCounts);
        foreach (NodeFieldDefinition definition in definitions)
            UpsertNodeFieldDefinition(
                connection,transaction,report.TypeHash,definition);

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
            foreach (NodeObservation observation in observations)
            foreach (NodeFieldObservation field in observation.Fields)
            {
                NodeFieldDefinition definition = definitions[field.FieldType];
                semantic.Value = definition.Semantic;
                layout.Value = definition.Layout;
                value.Value = field.DecodedJson;
                file.Value = observation.FileId;
                objectIndex.Value = observation.ObjectIndex;
                fieldIndex.Value = field.FieldIndex;
                if (decoded.ExecuteNonQuery() != 1)
                {
                    throw new InvalidDataException(
                        $"Could not annotate spNode field {observation.FileId}:" +
                        $"{observation.ObjectIndex}:{field.FieldIndex}.");
                }
            }
        }

        int variantId;
        using (SqliteCommand variant = CreateCommand(connection,transaction,"""
            INSERT INTO class_variants(
                type_hash,scope_kind,scope_key,variant_key,display_name,
                status,discriminator_json,notes,created_utc,updated_utc)
            VALUES($hash,'common','pc_ps2','node_serializer_contract',
                   'Base scene-graph node','confirmed',$discriminator,$notes,$utc,$utc);
            SELECT last_insert_rowid();
            """))
        {
            variant.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            variant.Parameters.AddWithValue("$discriminator",JsonSerializer.Serialize(new
            {
                serializerFields = Enumerable.Range(0,9).ToArray(),
                observedFields = Enumerable.Range(0,9).Where(type =>
                    fieldCounts.Keys.Any(key => key.Type == type)).ToArray(),
                unobservedButExecutableConfirmedFields = new[] { 6 },
                orthogonalFeatures = new[]
                {
                    "bone","static","animated","children","collision","billboard"
                }
            }));
            variant.Parameters.AddWithValue("$notes",
                "One serializer contract. Bone/static/animated state and graph " +
                "relationships are independent fields, not separate class subtypes. " +
                "The four-byte PC child form is an encoding variant of field 5.");
            variant.Parameters.AddWithValue("$utc",now);
            variantId = Convert.ToInt32(variant.ExecuteScalar());
        }
        using (SqliteCommand assignments = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            SELECT o.file_id,o.object_index,$variant,'confirmed',
                   'full PC+PS2 corpus: all nine fields validated with defaults'
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
        long childCount = fieldCounts.ValuesForType(5);
        long collisionCount = fieldCounts.ValuesForType(7);
        long idOnlyCount = relationshipEncodingCounts
            .Where(item => item.Key.Type == 5 &&
                           item.Key.Encoding == SmoNodeRelationshipEncoding.IdOnly)
            .Sum(item => item.Value);
        string corpusCounts = string.Join(", ",report.Profiles
            .OrderBy(item => item.CorpusKey,StringComparer.Ordinal)
            .Select(profile =>
                $"{profile.CorpusKey}={profile.UniqueObjectCount}"));
        string differenceList = platformDifferences.Length == 0
            ? "none"
            : string.Join(", ",platformDifferences);
        string pcDifferenceList = pcDifferences.Length == 0
            ? "none"
            : string.Join(", ",pcDifferences);
        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "spNode serializer 0x00463F10..0x00464719; registration RVA 0x002D397C",
            "Fields are position=0, rotation=1, scale=2, isBone=3, isStatic=4, " +
            "child=5, billboardAxis=6, collision=7 and isAnimated=8. Position, " +
            "identity rotation, unit scale and false bone/static are omitted; " +
            "animated is always written; child/collision are repeated sections.",
            pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "spNode serializer 0x00196CD0..0x001974B0; class hash at 0x001974C0",
            "MIPS serializer independently uses the same nine IDs, defaults, " +
            "flag bits, relationship loops and billboard values 1/2.",
            ps2Executable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "all concrete spNode objects and direct fields in three corpora",
            $"{objectCount} named objects ({corpusCounts}); {directFieldCount} " +
            $"decoded value fields; child relations={childCount}, collision " +
            $"relations={collisionCount}; ID-only child fields={idOnlyCount}; " +
            "field 6 is unobserved on concrete spNode; legacy PC objects may " +
            "omit field 8 and use false by default.",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus normalized node name/state/relationship graph",
            $"PC working/pristine semantic graphs: {pcEqual}/{pcCommon}; " +
            $"differing PC paths (first 12): {pcDifferenceList}. PC " +
            $"pristine/PS2: {platformEqual}/{platformCommon}. Differing PC/PS2 " +
            $"paths (first 12): {differenceList}.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='scene_graph',
                       description='Base transform and object-graph node',
                       decode_status='read_only_decode',
                       notes='Common PC/PS2 nine-field serializer; compact PC child-ID references are supported.'
                   WHERE type_hash=$hash;
                   """))
        {
            updateClass.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            updateClass.ExecuteNonQuery();
        }
        transaction.Commit();
        Checkpoint(connection);

        int assignmentCount;
        using (SqliteCommand assignments = connection.CreateCommand())
        {
            assignments.CommandText = """
                SELECT COUNT(*) FROM object_variant_assignments a
                JOIN class_variants v ON v.id=a.variant_id
                WHERE v.type_hash=$hash
                  AND v.variant_key='node_serializer_contract';
                """;
            assignments.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            assignmentCount = Convert.ToInt32(assignments.ExecuteScalar());
        }
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName ?? "<unknown>","confirmed_read_only",
            report.Profiles.Count,objectCount,
            report.Profiles.Sum(item => item.UniqueResourceCount),
            1,assignmentCount,evidenceRows,
            $"One common nine-field serializer contract; {directFieldCount} fields " +
            $"decoded. Child relations={childCount}, including {idOnlyCount} compact " +
            $"PC ID-only fields; collision relations={collisionCount}. Field 6 is " +
            "executable-confirmed but unobserved on concrete spNode; legacy PC " +
            "objects may omit field 8. " +
            $"PC pairs {pcEqual}/{pcCommon}; normalized PC/PS2 graphs " +
            $"{platformEqual}/{platformCommon}. Mutation safety remains untested.");
    }

    private static Dictionary<NodeObjectKey,NodeObservationBuilder>
        LoadNodeBuilders(SqliteConnection connection)
    {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            SELECT o.file_id,o.object_index,o.object_id,c.corpus_key,p.platform_key,
                   f.relative_path,o.name,o.serialized_size,o.field_shape
            FROM objects o
            JOIN files f ON f.id=o.file_id
            JOIN corpora c ON c.id=f.corpus_id
            JOIN platforms p ON p.id=c.platform_id
            WHERE o.type_hash=$hash
            ORDER BY c.id,f.normalized_path,o.object_index;
            """;
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.Node);
        using SqliteDataReader reader = command.ExecuteReader();
        var result = new Dictionary<NodeObjectKey,NodeObservationBuilder>();
        while (reader.Read())
        {
            var builder = new NodeObservationBuilder(
                reader.GetInt32(0),reader.GetInt32(1),
                checked((uint)reader.GetInt64(2)),reader.GetString(3),
                reader.GetString(4),GetCanonicalResourcePath(reader.GetString(5))
                    .ToLowerInvariant(),
                reader.GetString(6).TrimEnd('\0'),reader.GetInt32(7),reader.GetString(8));
            result.Add(new NodeObjectKey(builder.FileId,builder.ObjectIndex),builder);
        }
        return result;
    }

    private static Dictionary<NodeTargetKey,NodeTarget> LoadNodeTargets(
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.Node);
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

    private static void LoadNodeFields(
        SqliteConnection connection,
        IReadOnlyDictionary<NodeObjectKey,NodeObservationBuilder> builders)
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.Node);
        using SqliteDataReader reader = command.ExecuteReader();
        while (reader.Read())
        {
            var key = new NodeObjectKey(reader.GetInt32(0),reader.GetInt32(1));
            if (!builders.TryGetValue(key,out NodeObservationBuilder? builder))
                throw new InvalidDataException("Orphan spNode direct field row.");
            builder.Fields.Add(new NodeRawField(
                reader.GetInt32(2),reader.GetInt32(3),reader.GetInt32(4),
                reader.GetInt32(5),checked((uint)reader.GetInt64(6)),
                reader.GetFieldValue<byte[]>(7)));
        }
    }

    private static NodeObservation ValidateNodeObservation(
        NodeObservationBuilder builder,
        IReadOnlyDictionary<NodeTargetKey,NodeTarget> targets,
        IDictionary<(string Corpus,int Type),long> fieldCounts,
        IDictionary<(string Corpus,int Type,SmoNodeRelationshipEncoding Encoding),long>
            relationshipEncodingCounts,
        IDictionary<(string Corpus,int Type,uint TargetType),long>
            relationshipTargetCounts)
    {
        var seenScalars = new HashSet<int>();
        var decoded = new List<NodeFieldObservation>(builder.Fields.Count);
        var scalarSignatures = new Dictionary<int,string>
        {
            [0] = "000000000000000000000000",
            [1] = "0000000000000000000000000000803F",
            [2] = "0000803F0000803F0000803F",
            [3] = "0",
            [4] = "0",
            [6] = "0",
            [8] = "0"
        };
        int previousRank = -1;
        foreach (NodeRawField field in builder.Fields)
        {
            if (field.SectionIndex != 0 || field.FieldType is < 0 or > 8 ||
                field.FieldType == 6 ||
                !IsValidNodeFieldSize(field.FieldType,checked((int)field.PayloadSize)) ||
                field.PayloadPreview.Length != Math.Min(48,field.PayloadSize) ||
                (field.FieldType is not 5 and not 7 &&
                 !seenScalars.Add(field.FieldType)))
            {
                throw new InvalidDataException(
                    $"Invalid spNode field in {builder.CorpusKey}:" +
                    $"{builder.CanonicalPath} [{builder.ObjectIndex}] type " +
                    $"{field.FieldType}, size {field.PayloadSize}.");
            }
            int rank = GetNodeFieldRank(field.FieldType);
            if (rank < previousRank)
                throw new InvalidDataException("spNode fields violate serializer order.");
            previousRank = rank;
            fieldCounts.Increment((builder.CorpusKey,field.FieldType));

            string json;
            string signature;
            switch (field.FieldType)
            {
                case 0:
                case 2:
                {
                    Vector3 value = ReadNodeVector(field.PayloadPreview);
                    json = JsonSerializer.Serialize(new[] { value.X,value.Y,value.Z });
                    signature = Convert.ToHexString(field.PayloadPreview);
                    break;
                }
                case 1:
                {
                    Quaternion value = ReadNodeQuaternion(field.PayloadPreview);
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
                    {
                        throw new InvalidDataException("Invalid spNode Boolean value.");
                    }
                    bool boolean = field.PayloadPreview[0] != 0;
                    json = JsonSerializer.Serialize(boolean);
                    signature = boolean ? "1" : "0";
                    break;
                case 5:
                case 7:
                {
                    if (!SmoNodeDecoder.TryDecodeRelationshipPrefix(
                            field.PayloadPreview,field.PayloadSize,
                            out uint targetId,out uint inlineSize,
                            out SmoNodeRelationshipEncoding encoding) ||
                        !targets.TryGetValue(
                            new NodeTargetKey(builder.FileId,targetId),out NodeTarget? target) ||
                        !target.SignatureMatches || !target.WithinDataSection ||
                        (inlineSize > 0 && inlineSize != target.SerializedSize) ||
                        (encoding == SmoNodeRelationshipEncoding.InlineObject &&
                         (field.PayloadPreview.Length < 16 ||
                          BinaryPrimitives.ReadUInt32LittleEndian(
                              field.PayloadPreview.AsSpan(8)) != target.TypeHash ||
                          !field.PayloadPreview.AsSpan(12,4).SequenceEqual("SBOO"u8))) ||
                        (field.FieldType == 7 &&
                         target.TypeHash != SmoClassIds.CollisionInfo))
                    {
                        throw new InvalidDataException(
                            $"Invalid spNode relationship in {builder.CorpusKey}:" +
                            $"{builder.CanonicalPath} [{builder.ObjectIndex}].");
                    }
                    relationshipEncodingCounts.Increment(
                        (builder.CorpusKey,field.FieldType,encoding));
                    relationshipTargetCounts.Increment(
                        (builder.CorpusKey,field.FieldType,target.TypeHash));
                    string encodingName = encoding switch
                    {
                        SmoNodeRelationshipEncoding.IdOnly => "id_only",
                        SmoNodeRelationshipEncoding.SizedReference =>
                            "sized_reference",
                        _ => "inline_object"
                    };
                    json = JsonSerializer.Serialize(new
                    {
                        objectId = targetId,
                        targetObjectIndex = target.ObjectIndex,
                        targetTypeHash = $"0x{target.TypeHash:X8}",
                        targetClass = SmoClassRegistry.GetDisplayName(target.TypeHash),
                        targetName = target.Name,
                        encoding = encodingName,
                        inlineSerializedSize = inlineSize
                    });
                    signature =
                        $"0x{target.TypeHash:X8}:{target.Name.ToLowerInvariant()}";
                    break;
                }
                default:
                    throw new InvalidDataException("Unexpected spNode field.");
            }
            decoded.Add(new NodeFieldObservation(
                field.FieldIndex,field.FieldType,json,signature));
            if (field.FieldType is not 5 and not 7)
                scalarSignatures[field.FieldType] = signature;
        }
        string relationshipSignature = string.Join("|",decoded
            .Where(field => field.FieldType is 5 or 7)
            .Select(field => $"f{field.FieldType}:{field.SemanticSignature}"));
        string semanticSignature = builder.Name.ToLowerInvariant() + "|" +
            string.Join("|",new[] { 0,1,2,3,4,6,8 }.Select(type =>
                $"f{type}:{scalarSignatures[type]}")) + "|" + relationshipSignature;
        return new NodeObservation(
            builder.FileId,builder.ObjectIndex,builder.CorpusKey,
            builder.CanonicalPath,builder.Name,decoded.AsReadOnly(),semanticSignature);
    }

    private static NodeFieldDefinition[] BuildNodeFieldDefinitions(
        IReadOnlyList<NodeObservation> observations,
        IReadOnlyDictionary<(string Corpus,int Type),long> fieldCounts,
        IReadOnlyDictionary<
            (string Corpus,int Type,SmoNodeRelationshipEncoding Encoding),long>
            relationshipEncodingCounts,
        IReadOnlyDictionary<(string Corpus,int Type,uint TargetType),long>
            relationshipTargetCounts)
    {
        object Counts(int type) => fieldCounts
            .Where(item => item.Key.Type == type)
            .OrderBy(item => item.Key.Corpus,StringComparer.Ordinal)
            .ToDictionary(item => item.Key.Corpus,item => item.Value);
        string RelationshipConstraints(int type) => JsonSerializer.Serialize(new
        {
            observedOccurrences = Counts(type),
            encodings = relationshipEncodingCounts
                .Where(item => item.Key.Type == type)
                .GroupBy(item => item.Key.Encoding)
                .ToDictionary(
                    group => group.Key.ToString(),group => group.Sum(item => item.Value)),
            targetClasses = relationshipTargetCounts
                .Where(item => item.Key.Type == type)
                .GroupBy(item => item.Key.TargetType)
                .ToDictionary(
                    group => $"0x{group.Key:X8}",
                    group => group.Sum(item => item.Value)),
            repeated = true,
            mutationStatus = "not_tested"
        });
        string ScalarConstraints(int type,object defaultValue,params int[] sizes) =>
            JsonSerializer.Serialize(new
            {
                observedOccurrences = Counts(type),
                payloadBytes = sizes,
                executableDefault = defaultValue,
                mutationStatus = "not_tested"
            });
        return
        [
            new(0,"node.position","Local position","vector3","Vector3 (X, Y, Z)",
                "confirmed_both_executables_and_full_corpus",
                ScalarConstraints(0,new[] { 0.0f,0.0f,0.0f },12),
                "Optional; zero vector is omitted."),
            new(1,"node.rotation","Local rotation","quaternion",
                "Quaternion (X, Y, Z, W)",
                "confirmed_both_executables_and_full_corpus",
                ScalarConstraints(1,new[] { 0.0f,0.0f,0.0f,1.0f },16),
                "Optional; identity quaternion is omitted."),
            new(2,"node.scale","Local scale","vector3","Vector3 (X, Y, Z)",
                "confirmed_both_executables_and_full_corpus",
                ScalarConstraints(2,new[] { 1.0f,1.0f,1.0f },12),
                "Optional; unit scale is omitted."),
            new(3,"node.is_bone","Bone node","boolean","Boolean byte",
                "confirmed_both_executables_and_full_corpus",
                ScalarConstraints(3,false,1),
                "Optional; only true is emitted."),
            new(4,"node.is_static","Static node","boolean","Boolean byte",
                "confirmed_both_executables_and_full_corpus",
                ScalarConstraints(4,false,1),
                "Optional; only true is emitted."),
            new(5,"node.child","Child node","object_relationship",
                "UInt32 object ID; optional UInt32 inline size and inline SBOO",
                "confirmed_both_executables_and_full_corpus",
                RelationshipConstraints(5),
                "Repeated. PC additionally contains a valid four-byte ID-only form."),
            new(6,"node.billboard_axis","Billboard axis","uint32_enum","UInt32 enum",
                "confirmed_both_executables_unobserved_on_concrete_class",
                JsonSerializer.Serialize(new
                {
                    observedOccurrences = 0,
                    supportedValues = new[] { 1,2 },
                    executableDefault = 0,
                    exactAxisNames = "not_recovered",
                    mutationStatus = "not_tested"
                }),
                "Optional engine values 1/2; absent from all concrete spNode objects."),
            new(7,"node.collision","Collision info","object_relationship",
                "UInt32 object ID, UInt32 inline size, optional inline SBOO",
                "confirmed_both_executables_and_full_corpus",
                RelationshipConstraints(7),
                "Repeated; every resolved target is spCollisionInfo."),
            new(8,"node.is_animated","Animated node","boolean","Boolean byte",
                "confirmed_both_executables_and_full_corpus",
                JsonSerializer.Serialize(new
                {
                    observedOccurrences = Counts(8),
                    falseCount = observations.Sum(item => item.Fields.Count(field =>
                        field.FieldType == 8 && field.DecodedJson == "false")),
                    trueCount = observations.Sum(item => item.Fields.Count(field =>
                        field.FieldType == 8 && field.DecodedJson == "true")),
                    omittedLegacyCount = observations.Count -
                        observations.Sum(item => item.Fields.Count(field =>
                            field.FieldType == 8)),
                    executableWriterAlwaysSerializes = true,
                    omittedValueDefault = false,
                    mutationStatus = "not_tested"
                }),
                "Current writers emit false and true; legacy PC objects may omit false.")
        ];
    }

    private static void UpsertNodeFieldDefinition(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        NodeFieldDefinition definition,
        int sectionFromEnd = 0)
    {
        using SqliteCommand field = CreateCommand(connection,transaction,"""
            INSERT INTO field_definitions(
                type_hash,scope_kind,scope_key,section_from_end,field_type,
                occurrence,semantic_key,display_name,value_kind,payload_layout,
                editable_status,evidence_status,constraints_json,notes)
            VALUES($hash,'common','pc_ps2',$section,$type,-1,$semantic,$display,
                   $kind,$layout,'read_only_research',$evidence,$constraints,$notes)
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
        field.Parameters.AddWithValue("$hash",(long)typeHash);
        field.Parameters.AddWithValue("$section",sectionFromEnd);
        field.Parameters.AddWithValue("$type",definition.Type);
        field.Parameters.AddWithValue("$semantic",definition.Semantic);
        field.Parameters.AddWithValue("$display",definition.Display);
        field.Parameters.AddWithValue("$kind",definition.Kind);
        field.Parameters.AddWithValue("$layout",definition.Layout);
        field.Parameters.AddWithValue("$evidence",definition.Evidence);
        field.Parameters.AddWithValue("$constraints",definition.Constraints);
        field.Parameters.AddWithValue("$notes",definition.Notes);
        field.ExecuteNonQuery();
    }

    private static Dictionary<string,string> BuildNodePathMap(
        IEnumerable<NodeObservation> observations,
        string corpusKey) => observations
        .Where(item => item.CorpusKey.Equals(corpusKey,StringComparison.Ordinal))
        .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
        .ToDictionary(
            group => group.Key,
            group => string.Join("\n",group
                .Select(item => item.SemanticSignature)
                .Order(StringComparer.Ordinal)),
            StringComparer.Ordinal);

    private static bool IsValidNodeFieldSize(int fieldType,int payloadSize) =>
        fieldType switch
        {
            0 or 2 => payloadSize == SmoNodeDecoder.VectorPayloadSize,
            1 => payloadSize == SmoNodeDecoder.QuaternionPayloadSize,
            3 or 4 or 8 => payloadSize == SmoNodeDecoder.BooleanPayloadSize,
            5 => payloadSize == SmoNodeDecoder.IdOnlyRelationshipPayloadSize ||
                 payloadSize >= SmoNodeDecoder.SizedRelationshipPrefixSize,
            6 => payloadSize == sizeof(uint),
            7 => payloadSize >= SmoNodeDecoder.SizedRelationshipPrefixSize,
            _ => false
        };

    private static int GetNodeFieldRank(int fieldType) => fieldType switch
    {
        0 => 0, 1 => 1, 2 => 2, 3 => 3, 4 => 4, 8 => 5,
        5 => 6, 6 => 7, 7 => 8,
        _ => int.MaxValue
    };

    private static Vector3 ReadNodeVector(byte[] payload)
    {
        if (payload.Length != SmoNodeDecoder.VectorPayloadSize)
            throw new InvalidDataException("Invalid spNode Vector3 payload.");
        var value = new Vector3(
            ReadNodeSingle(payload,0),ReadNodeSingle(payload,4),
            ReadNodeSingle(payload,8));
        if (!float.IsFinite(value.X) || !float.IsFinite(value.Y) ||
            !float.IsFinite(value.Z))
            throw new InvalidDataException("Non-finite spNode Vector3.");
        return value;
    }

    private static Quaternion ReadNodeQuaternion(byte[] payload)
    {
        if (payload.Length != SmoNodeDecoder.QuaternionPayloadSize)
            throw new InvalidDataException("Invalid spNode Quaternion payload.");
        var value = new Quaternion(
            ReadNodeSingle(payload,0),ReadNodeSingle(payload,4),
            ReadNodeSingle(payload,8),ReadNodeSingle(payload,12));
        if (!float.IsFinite(value.X) || !float.IsFinite(value.Y) ||
            !float.IsFinite(value.Z) || !float.IsFinite(value.W) ||
            value.LengthSquared() < 0.000001f)
            throw new InvalidDataException("Invalid spNode Quaternion value.");
        return value;
    }

    private static float ReadNodeSingle(byte[] payload,int offset) =>
        BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(
            payload.AsSpan(offset,sizeof(float))));

    private static void Increment<TKey>(
        this IDictionary<TKey,long> values,
        TKey key) where TKey : notnull
    {
        values.TryGetValue(key,out long count);
        values[key] = count + 1;
    }

    private static long ValuesForType(
        this IReadOnlyDictionary<(string Corpus,int Type),long> values,
        int type) => values.Where(item => item.Key.Type == type).Sum(item => item.Value);

    private readonly record struct NodeObjectKey(int FileId,int ObjectIndex);
    private readonly record struct NodeTargetKey(int FileId,uint ObjectId);
    private sealed record NodeTarget(
        int ObjectIndex,uint TypeHash,string Name,int SerializedSize,
        bool SignatureMatches,bool WithinDataSection);
    private sealed record NodeRawField(
        int FieldIndex,int SectionIndex,int FieldType,int Occurrence,
        uint PayloadSize,byte[] PayloadPreview);
    private sealed record NodeFieldObservation(
        int FieldIndex,int FieldType,string DecodedJson,string SemanticSignature);
    private sealed record NodeObservation(
        int FileId,int ObjectIndex,string CorpusKey,string CanonicalPath,string Name,
        IReadOnlyList<NodeFieldObservation> Fields,string SemanticSignature);
    private sealed record NodeFieldDefinition(
        int Type,string Semantic,string Display,string Kind,string Layout,
        string Evidence,string Constraints,string Notes);
    private sealed class NodeObservationBuilder
    {
        public NodeObservationBuilder(
            int fileId,int objectIndex,uint objectId,string corpusKey,
            string platformKey,string canonicalPath,string name,
            int serializedSize,string fieldShape)
        {
            FileId=fileId; ObjectIndex=objectIndex; ObjectId=objectId;
            CorpusKey=corpusKey; PlatformKey=platformKey;
            CanonicalPath=canonicalPath; Name=name;
            SerializedSize=serializedSize; FieldShape=fieldShape;
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
        public List<NodeRawField> Fields { get; } = [];
    }
}
