using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeSkin(
        string databasePath,
        SmoResearchClassReport report)
    {
        if (report.Profiles.Count != 3 ||
            report.Profiles.Select(item => item.PlatformKey).Distinct().Count() != 2 ||
            report.Profiles.Any(item => item.NamedObjectCount != item.UniqueObjectCount) ||
            report.Fields.Any(item =>
                item.SectionFromEnd is < 0 or > 2 ||
                (item.SectionFromEnd == 2 && item.FieldType is < 0 or > 3) ||
                (item.SectionFromEnd == 1 && item.FieldType is < 0 or > 1) ||
                (item.SectionFromEnd == 0 && item.FieldType != 0)))
        {
            throw new InvalidDataException(
                "spSkin corpus no longer matches the validated three-section " +
                "spRenderable/spModel/spSkin contract.");
        }

        string database = Path.GetFullPath(databasePath);
        using SqliteConnection connection = OpenResearch(database,readOnly: false);
        List<SkinObservation> observations = LoadSkinObservations(connection);
        long objectCount = report.Profiles.Sum(item => item.UniqueObjectCount);
        if (objectCount != 1_758 || observations.Count != objectCount)
        {
            throw new InvalidDataException(
                $"Expected 1758 decoded spSkin objects, got " +
                $"{observations.Count}/{objectCount}.");
        }
        if (observations.Sum(item => item.Fields.Count) != 12_213)
        {
            throw new InvalidDataException(
                "spSkin non-terminal field total changed from 12213.");
        }

        Dictionary<(string Corpus,string Variant),int> variantCounts = observations
            .GroupBy(item => (item.CorpusKey,item.VariantKey))
            .ToDictionary(group => group.Key,group => group.Count());
        RequireSkinVariantCounts(variantCounts);
        RequireSkinPlatformPalettes(observations);
        SkinMatrixConsistency matrixConsistency = GetSkinMatrixConsistency(
            observations);
        if (matrixConsistency != new SkinMatrixConsistency(
                15_093,8_550,8_550,0,240))
        {
            throw new InvalidDataException(
                "Repeated spSkin inverse-bind consistency changed: " +
                JsonSerializer.Serialize(matrixConsistency));
        }

        (int pcCommon,int pcEqual) = CompareSkinPcCopies(observations);
        if (pcCommon != 117 || pcEqual != 117)
        {
            throw new InvalidDataException(
                "PC working/pristine skin payloads no longer match exactly.");
        }
        SkinPlatformComparison cross = CompareSkinPlatforms(observations);
        if (cross != new SkinPlatformComparison(
                103,14,30,30,28,30,30,30,30,11,22,27))
        {
            throw new InvalidDataException(
                "PC/PS2 paired skin semantics changed from the validated corpus: " +
                JsonSerializer.Serialize(cross));
        }

        ExecutableIdentity pcExecutable = FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable = FindExecutable(connection,"ps2");
        string[] tokens =
        [
            "spSkin","spSkinSerializer","esfSkin",
            "spModelSerializer","esfModelBase","esfModelProjectionGroup",
            "spRenderableSerializer","esfRenderableMaterial","esfRenderableFog"
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
                   WHERE type_hash=$hash AND variant_key LIKE 'skin_%';
                   """))
        {
            clearVariants.Parameters.AddWithValue("$hash",(long)report.TypeHash);
            clearVariants.ExecuteNonQuery();
        }

        UpsertSkinFieldDefinitions(
            connection,transaction,report.TypeHash,observations);
        AnnotateSkinFields(connection,transaction,observations);
        Dictionary<string,int> variantIds = InsertSkinVariants(
            connection,transaction,report.TypeHash,observations,now);
        AssignSkinVariants(connection,transaction,observations,variantIds);

        int evidenceRows = 0;
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,pcExecutable.PlatformId,
            pcExecutable.CorpusId,"class_analysis:pc_executable",pcExecutable.Path,
            "spSkin/spModel/spRenderable serializer registrations and field enums",
            "The PC executable identifies the concrete esfSkin field and both " +
            "inherited serializer sections.",pcExecutable.Sha256,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,ps2Executable.PlatformId,
            ps2Executable.CorpusId,"class_analysis:ps2_executable",ps2Executable.Path,
            "MIPS spSkin/spModel/spRenderable serializer registrations and field enums",
            "The PS2 executable independently exposes the same serializer chain and " +
            "the concrete esfSkin field.",ps2Executable.Sha256,now);

        Dictionary<string,int> relationshipEncodings = observations
            .SelectMany(item => new[]
            {
                ("material",item.Data.Renderable.Material?.Encoding),
                ("fog",item.Data.Renderable.Fog?.Encoding),
                ("base",(SmoNodeRelationshipEncoding?)item.Data.BaseMesh.Encoding)
            })
            .GroupBy(item => $"{item.Item1}:{item.Item2?.ToString() ?? "absent"}")
            .ToDictionary(group => group.Key,group => group.Count());
        Dictionary<string,int> paletteEncodings = observations
            .SelectMany(item => item.Data.Bones.Select(bone =>
                $"{item.PlatformKey}:{bone.Encoding}"))
            .GroupBy(item => item)
            .ToDictionary(group => group.Key,group => group.Count());
        Dictionary<string,int> headerValues = observations
            .GroupBy(item => $"{item.PlatformKey}:{item.Data.BlendInfluenceCountHint}")
            .ToDictionary(group => group.Key,group => group.Count());
        Dictionary<string,int> expectedHeaderValues = new()
        {
            ["pc:0"] = 1_426,
            ["pc:1"] = 38,
            ["pc:2"] = 12,
            ["pc:3"] = 12,
            ["pc:4"] = 8,
            ["ps2:0"] = 262
        };
        if (headerValues.Count != expectedHeaderValues.Count ||
            expectedHeaderValues.Any(item =>
                !headerValues.TryGetValue(item.Key,out int count) ||
                count != item.Value))
        {
            throw new InvalidDataException(
                "spSkin blend-influence hint distribution changed.");
        }
        SkinInfluenceHintProfile influenceHints = GetSkinInfluenceHintProfile(
            observations);
        if (influenceHints != new SkinInfluenceHintProfile(70,66,66,4))
        {
            throw new InvalidDataException(
                "spSkin nonzero blend-influence correlation changed: " +
                JsonSerializer.Serialize(influenceHints));
        }
        long affineMatrices = observations.Sum(item =>
            (long)item.Data.Bones.Count(bone => IsAffine(bone.InverseBindMatrix)));
        long invertibleMatrices = observations.Sum(item =>
            (long)item.Data.Bones.Count(bone =>
                Matrix4x4.Invert(bone.InverseBindMatrix,out _)));
        long paletteSlots = observations.Sum(item => (long)item.Data.Bones.Count);
        string counts = string.Join(", ",variantCounts
            .OrderBy(item => item.Key.Corpus,StringComparer.Ordinal)
            .ThenBy(item => item.Key.Variant,StringComparer.Ordinal)
            .Select(item => $"{item.Key.Corpus}/{item.Key.Variant}={item.Value}"));
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict decode of every unique PC working, PC pristine and PS2 pristine spSkin",
            $"{objectCount} skins; 12213 annotated non-terminal fields; " +
            $"{paletteSlots} palette slots; affine matrices={affineMatrices}; " +
            $"invertible matrices={invertibleMatrices}. Variants: {counts}. " +
            $"Per-resource bone targets={matrixConsistency.Targets}; repeated=" +
            $"{matrixConsistency.RepeatedTargets}; consistent repeated=" +
            $"{matrixConsistency.ConsistentRepeatedTargets}; maximum occurrences=" +
            $"{matrixConsistency.MaximumOccurrences}. " +
            "Blend-influence hints: " + string.Join(", ",headerValues
                .OrderBy(item => item.Key).Select(item => $"{item.Key}={item.Value}")) +
            $". Nonzero PC hints matching decoded maximum per-vertex influences=" +
            $"{influenceHints.MatchingDecoded}/{influenceHints.NonzeroDecoded}; " +
            $"structural-only nonzero PC meshes={influenceHints.NonzeroStructuralOnly}. " +
            "Relationships: " + string.Join(", ",relationshipEncodings
                .OrderBy(item => item.Key).Select(item => $"{item.Key}={item.Value}")) +
            ". Palette storage: " + string.Join(", ",paletteEncodings
                .OrderBy(item => item.Key).Select(item => $"{item.Key}={item.Value}")) +
            ".",null,now);
        evidenceRows += InsertEvidence(
            connection,transaction,report.TypeHash,null,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical path plus spSkin ordinal; complete serialized hashes for PC pairs",
            $"PC working/pristine equal skin resources: {pcEqual}/{pcCommon}. " +
            $"PC/PS2 common resources={cross.CommonResources}; equal skin-count " +
            $"resources={cross.EqualCountResources}; paired skins={cross.PairedSkins}; " +
            $"matches: name={cross.EqualObjectName}, AlphaSort={cross.EqualAlphaSort}, " +
            $"Priority={cross.EqualPriority}, effective ProjectionGroup=" +
            $"{cross.EqualProjection}, material={cross.EqualMaterialTarget}, " +
            $"fog={cross.EqualFogTarget}, base mesh={cross.EqualBaseTarget}, " +
            $"first-16 palette node IDs={cross.EqualFirst16BoneIds}, " +
            $"first-16 inverse-bind matrices={cross.EqualFirst16InverseBind}.",null,now);

        using (SqliteCommand updateClass = CreateCommand(connection,transaction,"""
                   UPDATE classes SET category='skinned_renderable_resource',
                       description='Skinned model with material/fog state, mesh, fixed platform palette and inverse-bind matrices',
                       decode_status='read_only_decode',
                       notes='All three serializer sections and every PC/PS2 object decode strictly. PC palettes have 16 slots; PS2 palettes have 64. Mutation remains untested.'
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
            $"Three serializer sections and seven semantic fields decoded. " +
            $"PC uses 16 palette slots, PS2 uses 64; {paletteSlots} inverse-bind " +
            $"matrices validated. Blend-influence hints: " +
            JsonSerializer.Serialize(headerValues) + ". Cross-platform pairing: " +
            JsonSerializer.Serialize(cross) + ". Mutation remains disabled.");
    }

    private static List<SkinObservation> LoadSkinObservations(
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
        command.Parameters.AddWithValue("$hash",(long)SmoClassIds.Skin);
        using SqliteDataReader reader = command.ExecuteReader();
        var locations = new List<SkinFileLocation>();
        while (reader.Read())
        {
            locations.Add(new SkinFileLocation(
                reader.GetInt32(0),reader.GetString(1),reader.GetString(2),
                reader.GetString(3),reader.GetString(4),reader.GetString(5),
                reader.IsDBNull(6) ? null : reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetInt64(7),reader.GetInt64(8)));
        }

        var result = new List<SkinObservation>();
        foreach (SkinFileLocation location in locations)
        {
            byte[] bytes = ReadSkinResource(location);
            SmoDocument document = SmoDocument.Parse(bytes,location.RelativePath);
            int ordinal = 0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                         item.TypeHash == SmoClassIds.Skin))
            {
                if (!SmoSkinDecoder.TryDecode(
                        document,entry,out SmoSkin? decoded,out string error) ||
                    decoded is null)
                {
                    throw new InvalidDataException(
                        $"Could not decode {location.CorpusKey}:" +
                        $"{location.RelativePath} [{entry.Index}]: {error}");
                }
                result.Add(CreateSkinObservation(
                    location,document,entry,ordinal++,decoded));
            }
        }
        return result;
    }

    private static byte[] ReadSkinResource(SkinFileLocation location)
    {
        if (location.SourceKind.Equals("directory",StringComparison.Ordinal))
        {
            return File.ReadAllBytes(Path.Combine(location.SourceRoot,
                location.RelativePath.Replace('/',Path.DirectorySeparatorChar)));
        }
        if (location.ContainerPath is null || !location.ByteOffset.HasValue ||
            location.ByteSize > int.MaxValue)
        {
            throw new InvalidDataException("PCK skin occurrence is incomplete.");
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

    private static SkinObservation CreateSkinObservation(
        SkinFileLocation location,
        SmoDocument document,
        SmoObjectEntry entry,
        int ordinal,
        SmoSkin decoded)
    {
        IReadOnlyList<SmoObjectField> direct = SmoObjectFieldReader.Read(document,entry);
        var annotations = new List<SkinFieldAnnotation>();
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
                    $"Unregistered spSkin field {field.FieldType} in " +
                    $"{location.RelativePath} [{entry.Index}].");
            }
            int sectionFromEnd = direct.Skip(fieldIndex + 1)
                .Count(item => item.FieldType == 0 && item.PayloadSize == 0) - 1;
            object? fieldValue = (sectionFromEnd,field.FieldType) switch
            {
                (2,0) => decoded.Renderable.Material,
                (2,1) => decoded.Renderable.Fog,
                (2,2) => decoded.Renderable.AlphaSortEnable,
                (2,3) => decoded.Renderable.Priority,
                (1,0) => decoded.BaseMesh,
                (1,1) => decoded.ProjectionGroup,
                (0,0) => CreateSkinPaletteSummary(document,decoded),
                _ => throw new InvalidDataException("Unexpected spSkin field mapping.")
            };
            annotations.Add(new SkinFieldAnnotation(
                fieldIndex,sectionFromEnd,field.FieldType,descriptor.Key,
                descriptor.PayloadLayout,JsonSerializer.Serialize(fieldValue)));
        }
        string serializedHash = Convert.ToHexString(SHA256.HashData(
            document.Data.Span.Slice(
                checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize))));
        SkinMeshProfile? meshProfile = TryCreateSkinMeshProfile(document,decoded);
        return new SkinObservation(
            location.FileId,entry.Index,ordinal,location.CorpusKey,location.PlatformKey,
            GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant(),
            entry.Name.TrimEnd('\0'),decoded,meshProfile,GetSkinVariant(decoded),serializedHash,
            annotations.AsReadOnly());
    }

    private static SkinMeshProfile? TryCreateSkinMeshProfile(
        SmoDocument document,SmoSkin decoded)
    {
        if (decoded.BaseMesh.TargetObjectIndex is not int meshIndex ||
            !SmoMeshDecoder.TryDecode(
                document,document.Objects[meshIndex],out SmoMesh? mesh,out _) ||
            mesh is null)
        {
            return null;
        }
        var active = new HashSet<byte>();
        int maxInfluences = 0;
        for (int index = 0;index < mesh.BlendWeights.Length;index++)
        {
            Vector4 weights = mesh.BlendWeights[index];
            SmoBlendIndices indices = mesh.BlendIndices[index];
            int influences = 0;
            if (weights.X > 0.000001f) { active.Add(indices.X); influences++; }
            if (weights.Y > 0.000001f) { active.Add(indices.Y); influences++; }
            if (weights.Z > 0.000001f) { active.Add(indices.Z); influences++; }
            if (weights.W > 0.000001f) { active.Add(indices.W); influences++; }
            maxInfluences = Math.Max(maxInfluences,influences);
        }
        return new SkinMeshProfile(
            mesh.VertexFormat,active.Count,maxInfluences,mesh.HasSkinningData);
    }

    private static object CreateSkinPaletteSummary(
        SmoDocument document,SmoSkin decoded)
    {
        int inline = decoded.Bones.Count(item =>
            item.Encoding == SmoNodeRelationshipEncoding.InlineObject);
        int affine = decoded.Bones.Count(item => IsAffine(item.InverseBindMatrix));
        int invertible = decoded.Bones.Count(item =>
            Matrix4x4.Invert(item.InverseBindMatrix,out _));
        return new
        {
            blendInfluenceCountHint = decoded.BlendInfluenceCountHint,
            slotCount = decoded.Bones.Count,
            inlineNodes = inline,
            sizedReferences = decoded.Bones.Count - inline,
            affineMatrices = affine,
            invertibleMatrices = invertible,
            nodes = decoded.Bones.Select(item => new
            {
                item.PaletteIndex,
                item.NodeObjectId,
                item.NodeObjectIndex,
                name = document.Objects[item.NodeObjectIndex].Name.TrimEnd('\0'),
                item.InlineSerializedSize,
                encoding = item.Encoding.ToString()
            }).ToArray()
        };
    }

    private static bool IsAffine(Matrix4x4 value) =>
        MathF.Abs(value.M14) <= 0.00001f &&
        MathF.Abs(value.M24) <= 0.00001f &&
        MathF.Abs(value.M34) <= 0.00001f &&
        MathF.Abs(value.M44 - 1) <= 0.00001f;

    private static SkinInfluenceHintProfile GetSkinInfluenceHintProfile(
        IEnumerable<SkinObservation> observations)
    {
        SkinObservation[] nonzero = observations.Where(item =>
            item.PlatformKey == "pc" && item.Data.BlendInfluenceCountHint != 0)
            .ToArray();
        SkinObservation[] decoded = nonzero.Where(item => item.Mesh is not null)
            .ToArray();
        return new SkinInfluenceHintProfile(
            nonzero.Length,
            decoded.Length,
            decoded.Count(item =>
                item.Data.BlendInfluenceCountHint ==
                item.Mesh!.MaxInfluencesPerVertex),
            nonzero.Count(item => item.Mesh is null));
    }

    private static SkinMatrixConsistency GetSkinMatrixConsistency(
        IEnumerable<SkinObservation> observations)
    {
        var groups = observations.SelectMany(observation =>
                observation.Data.Bones.Select(bone => new
                {
                    observation.FileId,
                    bone.NodeObjectIndex,
                    bone.InverseBindMatrix
                }))
            .GroupBy(item => (item.FileId,item.NodeObjectIndex))
            .ToArray();
        var repeated = groups.Where(group => group.Count() > 1).ToArray();
        int consistent = repeated.Count(group =>
        {
            Matrix4x4 first = group.First().InverseBindMatrix;
            return group.Skip(1).All(item =>
                ApproximatelyEqualSkinMatrix(first,item.InverseBindMatrix));
        });
        return new SkinMatrixConsistency(
            groups.Length,repeated.Length,consistent,repeated.Length - consistent,
            groups.Max(group => group.Count()));
    }

    private static bool ApproximatelyEqualSkinMatrix(
        Matrix4x4 left,Matrix4x4 right)
    {
        const float epsilon = 0.001f;
        return
            MathF.Abs(left.M11 - right.M11) <= epsilon &&
            MathF.Abs(left.M12 - right.M12) <= epsilon &&
            MathF.Abs(left.M13 - right.M13) <= epsilon &&
            MathF.Abs(left.M14 - right.M14) <= epsilon &&
            MathF.Abs(left.M21 - right.M21) <= epsilon &&
            MathF.Abs(left.M22 - right.M22) <= epsilon &&
            MathF.Abs(left.M23 - right.M23) <= epsilon &&
            MathF.Abs(left.M24 - right.M24) <= epsilon &&
            MathF.Abs(left.M31 - right.M31) <= epsilon &&
            MathF.Abs(left.M32 - right.M32) <= epsilon &&
            MathF.Abs(left.M33 - right.M33) <= epsilon &&
            MathF.Abs(left.M34 - right.M34) <= epsilon &&
            MathF.Abs(left.M41 - right.M41) <= epsilon &&
            MathF.Abs(left.M42 - right.M42) <= epsilon &&
            MathF.Abs(left.M43 - right.M43) <= epsilon &&
            MathF.Abs(left.M44 - right.M44) <= epsilon;
    }

    private static string GetSkinVariant(SmoSkin value) =>
        (value.Renderable.SerializedFieldMask,value.ModelSerializedFieldMask,
            value.SerializedFieldMask) switch
        {
            (0b1111,0b0011,0b0001) => "skin_full",
            (0b1111,0b0001,0b0001) => "skin_no_projection",
            (0b1110,0b0011,0b0001) => "skin_no_material",
            _ => throw new InvalidDataException(
                $"Unexpected spSkin masks renderable=0x" +
                $"{value.Renderable.SerializedFieldMask:X2}, model=0x" +
                $"{value.ModelSerializedFieldMask:X2}, skin=0x" +
                $"{value.SerializedFieldMask:X2}.")
        };

    private static void RequireSkinVariantCounts(
        IReadOnlyDictionary<(string Corpus,string Variant),int> counts)
    {
        Dictionary<(string Corpus,string Variant),int> expected = new()
        {
            [("pc-working","skin_full")] = 703,
            [("pc-working","skin_no_projection")] = 43,
            [("pc-working","skin_no_material")] = 2,
            [("pc-pristine","skin_full")] = 703,
            [("pc-pristine","skin_no_projection")] = 43,
            [("pc-pristine","skin_no_material")] = 2,
            [("ps2-pristine","skin_full")] = 259,
            [("ps2-pristine","skin_no_projection")] = 2,
            [("ps2-pristine","skin_no_material")] = 1
        };
        if (counts.Count != expected.Count || expected.Any(item =>
                !counts.TryGetValue(item.Key,out int count) || count != item.Value))
        {
            throw new InvalidDataException(
                "spSkin field-presence counts changed: " +
                JsonSerializer.Serialize(counts));
        }
    }

    private static void RequireSkinPlatformPalettes(
        IEnumerable<SkinObservation> observations)
    {
        foreach (SkinObservation observation in observations)
        {
            int expected = observation.PlatformKey == "pc" ? 16 : 64;
            if (observation.Data.Bones.Count != expected)
            {
                throw new InvalidDataException(
                    $"{observation.PlatformKey} spSkin palette has " +
                    $"{observation.Data.Bones.Count} slots instead of {expected}.");
            }
        }
    }

    private static (int Common,int Equal) CompareSkinPcCopies(
        IEnumerable<SkinObservation> observations)
    {
        Dictionary<string,SkinObservation[]> Build(string corpus) => observations
            .Where(item => item.CorpusKey == corpus)
            .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
            .ToDictionary(group => group.Key,
                group => group.OrderBy(item => item.Ordinal).ToArray(),
                StringComparer.Ordinal);
        Dictionary<string,SkinObservation[]> working = Build("pc-working");
        Dictionary<string,SkinObservation[]> pristine = Build("pc-pristine");
        string[] paths = working.Keys.Intersect(
            pristine.Keys,StringComparer.Ordinal).ToArray();
        int equal = paths.Count(path =>
            working[path].Select(item => (item.ObjectName,item.SerializedSha256))
                .SequenceEqual(pristine[path].Select(item =>
                    (item.ObjectName,item.SerializedSha256))));
        return (paths.Length,equal);
    }

    private static SkinPlatformComparison CompareSkinPlatforms(
        IEnumerable<SkinObservation> observations)
    {
        Dictionary<string,SkinObservation[]> Build(string corpus) => observations
            .Where(item => item.CorpusKey == corpus)
            .GroupBy(item => item.CanonicalPath,StringComparer.Ordinal)
            .ToDictionary(group => group.Key,
                group => group.OrderBy(item => item.Ordinal).ToArray(),
                StringComparer.Ordinal);
        Dictionary<string,SkinObservation[]> pc = Build("pc-pristine");
        Dictionary<string,SkinObservation[]> ps2 = Build("ps2-pristine");
        string[] paths = pc.Keys.Intersect(ps2.Keys,StringComparer.Ordinal).ToArray();
        string[] equalCount = paths.Where(path =>
            pc[path].Length == ps2[path].Length).ToArray();
        int pairs = 0;
        int names = 0;
        int alpha = 0;
        int priority = 0;
        int projection = 0;
        int material = 0;
        int fog = 0;
        int baseMesh = 0;
        int first16Bones = 0;
        int first16InverseBind = 0;
        foreach (string path in equalCount)
        for (int index = 0;index < pc[path].Length;index++)
        {
            SkinObservation left = pc[path][index];
            SkinObservation right = ps2[path][index];
            pairs++;
            names += left.ObjectName == right.ObjectName ? 1 : 0;
            alpha += left.Data.AlphaSortEnable == right.Data.AlphaSortEnable ? 1 : 0;
            priority += left.Data.Priority == right.Data.Priority ? 1 : 0;
            projection += (left.Data.ProjectionGroup ?? 0) ==
                          (right.Data.ProjectionGroup ?? 0) ? 1 : 0;
            material += left.Data.Renderable.Material?.TargetName ==
                        right.Data.Renderable.Material?.TargetName ? 1 : 0;
            fog += left.Data.Renderable.Fog?.TargetName ==
                   right.Data.Renderable.Fog?.TargetName ? 1 : 0;
            baseMesh += left.Data.BaseMesh.TargetName == right.Data.BaseMesh.TargetName
                ? 1
                : 0;
            string[] leftBones = left.Data.Bones.Take(16)
                .Select(item => item.NodeObjectId.ToString("X8")).ToArray();
            string[] rightBones = right.Data.Bones.Take(16)
                .Select(item => item.NodeObjectId.ToString("X8")).ToArray();
            first16Bones += leftBones.SequenceEqual(rightBones) ? 1 : 0;
            first16InverseBind += left.Data.Bones.Take(16)
                .Zip(right.Data.Bones.Take(16))
                .All(item => ApproximatelyEqualSkinMatrix(
                    item.First.InverseBindMatrix,item.Second.InverseBindMatrix))
                ? 1
                : 0;
        }
        return new SkinPlatformComparison(
            paths.Length,equalCount.Length,pairs,names,alpha,priority,projection,
            material,fog,baseMesh,first16Bones,first16InverseBind);
    }

    private static void UpsertSkinFieldDefinitions(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<SkinObservation> observations)
    {
        long Count(int section,int type) => observations.Sum(item =>
            (long)item.Fields.Count(field =>
                field.SectionFromEnd == section && field.FieldType == type));
        (int Section,int Type,string Semantic,string Display,string Kind,
            string Layout,string Notes)[] definitions =
        [
            (2,0,"renderable.material","Material","object_relationship",
                "object relationship to spMaterialData","Optional in three objects."),
            (2,1,"renderable.fog","Fog","object_relationship",
                "object relationship to spFog","Required in every skin."),
            (2,2,"renderable.alpha_sort","Alpha sort enabled","boolean_u32",
                "UInt32 Boolean 0 or 1","Required in every skin."),
            (2,3,"renderable.priority","Render priority","uint32",
                "UInt32","Required in every skin; observed values 0, 1 and 2."),
            (1,0,"model.base_mesh","Base mesh","object_relationship",
                "object relationship to spMeshData","Required inline mesh."),
            (1,1,"model.projection_group","Projection group","uint32",
                "UInt32","Optional; every serialized value is zero."),
            (0,0,"skin.palette","Bone palette","skin_palette",
                "UInt32 blend-influence count hint, UInt32 slot count, repeated spNode relationship plus inverse-bind Matrix4x4",
                "Required; 16 slots on PC and 64 slots on PS2. Nonzero PC hint equals the decoded maximum active influences per vertex.")
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
                platformSlotCount = definition.Semantic == "skin.palette"
                    ? new { pc = 16, ps2 = 64 }
                    : null,
                editable = false,
                mutationStatus = "not_tested"
            }));
            command.Parameters.AddWithValue(
                "$notes",$"{definition.Notes} Observed {count} times.");
            command.ExecuteNonQuery();
        }
    }

    private static void AnnotateSkinFields(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<SkinObservation> observations)
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
        foreach (SkinObservation observation in observations)
        foreach (SkinFieldAnnotation field in observation.Fields)
        {
            command.Parameters["$semantic"].Value = field.Semantic;
            command.Parameters["$layout"].Value = field.Layout;
            command.Parameters["$value"].Value = field.DecodedJson;
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$field"].Value = field.FieldIndex;
            if (command.ExecuteNonQuery() != 1)
                throw new InvalidDataException("Could not annotate spSkin field.");
        }
    }

    private static Dictionary<string,int> InsertSkinVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        uint typeHash,
        IReadOnlyList<SkinObservation> observations,
        string now)
    {
        (string Key,string Display,string Notes)[] variants =
        [
            ("skin_full","Full skin","All seven semantic fields are present."),
            ("skin_no_projection","Skin without projection group",
                "ProjectionGroup is omitted; effective default is zero."),
            ("skin_no_material","Skin without material",
                "Inherited material relationship is omitted.")
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

    private static void AssignSkinVariants(
        SqliteConnection connection,
        SqliteTransaction transaction,
        IEnumerable<SkinObservation> observations,
        IReadOnlyDictionary<string,int> variantIds)
    {
        using SqliteCommand command = CreateCommand(connection,transaction,"""
            INSERT INTO object_variant_assignments(
                file_id,object_index,variant_id,confidence,evidence)
            VALUES($file,$object,$variant,'confirmed',
                   'strict complete spSkin three-section decoder')
            ON CONFLICT(file_id,object_index,variant_id)
            DO UPDATE SET confidence=excluded.confidence,evidence=excluded.evidence;
            """);
        command.Parameters.Add("$file",SqliteType.Integer);
        command.Parameters.Add("$object",SqliteType.Integer);
        command.Parameters.Add("$variant",SqliteType.Integer);
        foreach (SkinObservation observation in observations)
        {
            command.Parameters["$file"].Value = observation.FileId;
            command.Parameters["$object"].Value = observation.ObjectIndex;
            command.Parameters["$variant"].Value = variantIds[observation.VariantKey];
            command.ExecuteNonQuery();
        }
    }

    private sealed record SkinFileLocation(
        int FileId,string CorpusKey,string PlatformKey,string SourceKind,
        string SourceRoot,string RelativePath,string? ContainerPath,
        long? ByteOffset,long ByteSize);
    private sealed record SkinFieldAnnotation(
        int FieldIndex,int SectionFromEnd,int FieldType,string Semantic,
        string Layout,string DecodedJson);
    private sealed record SkinObservation(
        int FileId,int ObjectIndex,int Ordinal,string CorpusKey,string PlatformKey,
        string CanonicalPath,string ObjectName,SmoSkin Data,SkinMeshProfile? Mesh,
        string VariantKey,
        string SerializedSha256,IReadOnlyList<SkinFieldAnnotation> Fields);
    private sealed record SkinMeshProfile(
        uint VertexFormat,int ActivePaletteSlots,int MaxInfluencesPerVertex,
        bool HasSkinningData);
    private sealed record SkinInfluenceHintProfile(
        int NonzeroPc,int NonzeroDecoded,int MatchingDecoded,
        int NonzeroStructuralOnly);
    private sealed record SkinMatrixConsistency(
        int Targets,int RepeatedTargets,int ConsistentRepeatedTargets,
        int DivergentTargets,int MaximumOccurrences);
    private sealed record SkinPlatformComparison(
        int CommonResources,int EqualCountResources,int PairedSkins,
        int EqualObjectName,int EqualAlphaSort,int EqualPriority,
        int EqualProjection,int EqualMaterialTarget,int EqualFogTarget,
        int EqualBaseTarget,int EqualFirst16BoneIds,int EqualFirst16InverseBind);
}
