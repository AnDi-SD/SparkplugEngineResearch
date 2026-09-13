using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeAnimTextureController(
        string databasePath,SmoResearchClassReport report)
    {
        using SqliteConnection connection=OpenResearch(
            Path.GetFullPath(databasePath),readOnly:false);
        List<AnimTextureObservation> observations=LoadAnimTextureObservations(connection);
        if (observations.Count!=28 || observations.Sum(item=>item.Fields.Count)!=28 ||
            observations.Sum(item=>item.Data.Frames.Count)!=974)
            throw new InvalidDataException("spAnimTexController profile changed.");
        RequireCounts(observations,item=>item.CorpusKey,new Dictionary<string,int>
        { ["pc-working"]=9,["pc-pristine"]=9,["ps2-pristine"]=10 });
        NavigationPairing pc=CompareNavigation(observations,item=>item.CorpusKey,
            item=>item.PairingKey,item=>item.SemanticSignature,item=>item.SerializedSha256,
            "pc-working","pc-pristine");
        NavigationPairing cross=CompareNavigation(observations,item=>item.CorpusKey,
            item=>item.PairingKey,item=>item.SemanticSignature,item=>item.SerializedSha256,
            "pc-pristine","ps2-pristine");
        if (pc!=new NavigationPairing(9,9,9) ||
            cross!=new NavigationPairing(9,9,0))
            throw new InvalidDataException("Animation texture pairing changed.");
        Dictionary<(int Frames,int Inline),int> expected=new()
        { [(38,9)]=16,[(38,0)]=9,[(8,2)]=3 };
        Dictionary<(int,int),int> actual=observations.GroupBy(item=>
                (item.Data.Frames.Count,item.Data.Frames.Count(frame=>
                    frame.Texture.Encoding==SmoNodeRelationshipEncoding.InlineObject)))
            .ToDictionary(group=>group.Key,group=>group.Count());
        if (expected.Any(pair=>!actual.TryGetValue(pair.Key,out int count)||count!=pair.Value))
            throw new InvalidDataException("Animation texture storage variants changed.");

        ExecutableIdentity pcExecutable=FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable=FindExecutable(connection,"ps2");
        string[] tokens=["spAnimTexController","spAnimTexControllerSerializer",
            "esfAnimTexControllerBase","pLayerTexture->GetAnimTextureController"];
        RequireAsciiTokens(pcExecutable.Path,tokens);RequireAsciiTokens(ps2Executable.Path,tokens);
        string now=DateTime.UtcNow.ToString("O");
        using SqliteTransaction transaction=connection.BeginTransaction();
        ClearNavigationAnalysis(connection,transaction,report.TypeHash,"anim_texture_%");
        UpsertNavigationDefinitions(connection,transaction,report.TypeHash,
            observations.SelectMany(item=>item.Fields),
            [(0,0,0,"anim_texture.frames","Animation texture frames","texture_frame_track",
                "UInt32 count, count little-endian Single times, then count sized/inline spTextureData relationships",
                false,"Required. Counts/times/textures are one-to-one; times strictly increase.")],
            includeNode:false,includeRenderNode:false);
        AnnotateNavigationFields(connection,transaction,observations.SelectMany(item=>
            item.Fields.Select(field=>(item.FileId,item.ObjectIndex,field))));
        var variants=new Dictionary<(int,int),int>();
        foreach (var pair in expected.Keys)
        {
            AnimTextureObservation[] members=observations.Where(item=>
                item.Data.Frames.Count==pair.Frames && item.Data.Frames.Count(frame=>
                    frame.Texture.Encoding==SmoNodeRelationshipEncoding.InlineObject)==pair.Inline).ToArray();
            string key=$"anim_texture_{pair.Frames}_frames_{pair.Inline}_inline";
            variants[pair]=InsertNavigationVariant(connection,transaction,report.TypeHash,key,
                $"{pair.Frames} frames, {pair.Inline} inline textures",
                JsonSerializer.Serialize(new { frameCount=pair.Frames,inlineTextures=pair.Inline,
                    objects=members.Length,corpora=members.GroupBy(item=>item.CorpusKey)
                        .ToDictionary(item=>item.Key,item=>item.Count()) }),
                "Frame semantics are shared; inline relationships establish texture ownership.",now);
        }
        foreach (AnimTextureObservation item in observations)
        {
            var key=(item.Data.Frames.Count,item.Data.Frames.Count(frame=>
                frame.Texture.Encoding==SmoNodeRelationshipEncoding.InlineObject));
            AssignNavigationVariant(connection,transaction,item.FileId,item.ObjectIndex,
                variants[key],"strict frame/time/texture decode");
        }
        int evidenceRows=0;int primary=variants[(38,9)];
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "spAnimTexControllerSerializer and esfAnimTexControllerBase x86 writer/reader",
            "The x86 serializer identifies one base field containing a frame count, " +
            "time sequence and texture sequence. Layer/render-target call sites identify " +
            "the controller as the material texture-frame source.",pcExecutable.Sha256,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "independent MIPS class/serializer/base-field and layer call-site identity",
            "The PS2 executable contains the same controller, field and layer access; " +
            "PS2 objects reproduce identical counts and frame times.",ps2Executable.Sha256,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            null,null,"class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload and 974 resolved texture-frame relationships",
            "All 28 controllers decode. Tracks contain either 8 frames through 0.266667 s " +
            "or 38 frames through 1.266667 s at approximately 30 Hz. All 974 times are " +
            "strictly increasing and map one-to-one to spTextureData; 150 relationships " +
            "are inline and 824 are sized references.",null,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            null,null,"class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical resource path plus controller ordinal",
            $"PC working/pristine: {pc.EqualBytes}/{pc.Paired} exact. All " +
            $"{cross.Paired} shared PC/PS2 tracks have identical times and texture-class " +
            "sequences; complete bytes differ because inline texture representation is " +
            "platform-native. PS2 adds one MusaX 38-frame controller.",null,now);
        UpdateNavigationClass(connection,transaction,report.TypeHash,
            "animated_texture_controller",
            "Material texture animation track pairing strictly increasing frame times with spTextureData relationships",
            "Complete PC/PS2 read-only decode of esfAnimTexControllerBase. Frame count, " +
            "time count and texture count must match; inline texture ownership and frame " +
            "order must be preserved for coordinated editing.");
        transaction.Commit();Checkpoint(connection);
        return new SmoResearchClassAnalysisResult(report.TypeHash,
            report.EngineName??"<unknown>","confirmed_read_only",report.Profiles.Count,
            observations.Count,report.Profiles.Sum(item=>item.UniqueResourceCount),
            observations.Select(item=>item.SerializedSha256).Distinct().Count(),
            observations.Count,evidenceRows,
            "Three frame-count/ownership variants; all 974 timed texture frames decoded.");
    }

    private static List<AnimTextureObservation> LoadAnimTextureObservations(
        SqliteConnection connection)
    {
        var result=new List<AnimTextureObservation>();
        foreach (MeshNavigationFileLocation location in
                 LoadNavigationLocations(connection,SmoClassIds.AnimTextureController))
        {
            SmoDocument document=SmoDocument.Parse(
                ReadMeshNavigationResource(location),location.RelativePath);int ordinal=0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item=>
                         item.TypeHash==SmoClassIds.AnimTextureController))
            {
                if (!SmoAnimTextureControllerDecoder.TryDecode(
                        document,entry,out var data,out string error)||data is null)
                    throw new InvalidDataException($"Could not decode animation texture: {error}");
                IReadOnlyList<SmoObjectField> direct=SmoObjectFieldReader.Read(document,entry);
                SmoObjectField field=direct.Single(item=>item.PayloadSize>0);
                var annotation=new MeshNavigationFieldAnnotation(0,"anim_texture.frames",
                    SmoSerializedFieldRegistry.GetOwnFieldDefinitions(entry.TypeHash)[0].PayloadLayout,
                    JsonSerializer.Serialize(new
                    {
                        FrameCount=data.Frames.Count,data.Duration,
                        Frames=data.Frames.Select(frame=>new { frame.Time,
                            Texture=RelationshipValue(frame.Texture) })
                    }));
                string canonical=GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant();
                string semantic=JsonSerializer.Serialize(data.Frames.Select(frame=>new
                    { frame.Time,frame.Texture.TargetTypeHash }));
                result.Add(new AnimTextureObservation(location.FileId,entry.Index,
                    location.CorpusKey,$"{canonical}|{ordinal++}",data,
                    ObjectHash(document,entry),semantic,[annotation]));
            }
        }
        return result;
    }

    private sealed record AnimTextureObservation(
        int FileId,int ObjectIndex,string CorpusKey,string PairingKey,
        SmoAnimTextureControllerData Data,string SerializedSha256,string SemanticSignature,
        IReadOnlyList<MeshNavigationFieldAnnotation> Fields);
}
