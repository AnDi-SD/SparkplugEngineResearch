using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeSkyBox(
        string databasePath,SmoResearchClassReport report)
    {
        using SqliteConnection connection=OpenResearch(
            Path.GetFullPath(databasePath),readOnly:false);
        List<SkyBoxObservation> observations=LoadSkyBoxObservations(connection);
        if (observations.Count!=126 || observations.Sum(item=>item.Fields.Count)!=478 ||
            observations.Sum(item=>item.Data.Models.Count)!=159 ||
            observations.SelectMany(item=>item.Data.Models).Any(model=>
                model.Encoding!=SmoNodeRelationshipEncoding.InlineObject))
        {
            throw new InvalidDataException("spSkyBox corpus profile changed.");
        }
        RequireCounts(observations,item=>item.CorpusKey,new Dictionary<string,int>
        { ["pc-working"]=43,["pc-pristine"]=43,["ps2-pristine"]=40 });
        NavigationPairing pc=CompareNavigation(
            observations,item=>item.CorpusKey,item=>item.PairingKey,
            item=>item.SemanticSignature,item=>item.SerializedSha256,
            "pc-working","pc-pristine");
        NavigationPairing cross=CompareNavigation(
            observations,item=>item.CorpusKey,item=>item.PairingKey,
            item=>item.SemanticSignature,item=>item.SerializedSha256,
            "pc-pristine","ps2-pristine");
        if (pc!=new NavigationPairing(43,43,43) ||
            cross!=new NavigationPairing(40,39,0))
            throw new InvalidDataException("spSkyBox PC/PS2 pairing changed: "+
                JsonSerializer.Serialize(new { pc,cross }));

        ExecutableIdentity pcExecutable=FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable=FindExecutable(connection,"ps2");
        RequireAsciiTokens(pcExecutable.Path,["spSkyBox","spSkyBoxManager"]);
        RequireAsciiTokens(ps2Executable.Path,["spSkyBox","spSkyBoxManager"]);
        string now=DateTime.UtcNow.ToString("O");
        using SqliteTransaction transaction=connection.BeginTransaction();
        ClearNavigationAnalysis(connection,transaction,report.TypeHash,"sky_box_%");
        UpsertNavigationDefinitions(
            connection,transaction,report.TypeHash,
            observations.SelectMany(item=>item.Fields),
            [(0,0,-1,"sky_box.model","Sky model","object_relationship",
                "one physically inline relationship to spModel",true,
                "Required/repeated 1..3; all 159 model relationships are inline-owned.")],
            includeNode:true,includeRenderNode:false);
        AnnotateNavigationFields(connection,transaction,observations.SelectMany(item=>
            item.Fields.Select(field=>(item.FileId,item.ObjectIndex,field))));
        var variants=new Dictionary<int,int>();
        foreach (IGrouping<int,SkyBoxObservation> group in observations.GroupBy(item=>
                     item.Data.Models.Count).OrderBy(group=>group.Key))
        {
            variants[group.Key]=InsertNavigationVariant(
                connection,transaction,report.TypeHash,$"sky_box_{group.Key}_models",
                $"{group.Key} inline sky model"+(group.Key==1 ? "" : "s"),
                JsonSerializer.Serialize(new
                {
                    inlineModelCount=group.Key,objects=group.Count(),
                    corpora=group.GroupBy(item=>item.CorpusKey)
                        .ToDictionary(item=>item.Key,item=>item.Count())
                }),"Serializer-level multiplicity variant; geometry/materials define " +
                   "whether the object is sky, cloud, moon, sun or fog shell.",now);
        }
        foreach (SkyBoxObservation item in observations)
            AssignNavigationVariant(connection,transaction,item.FileId,item.ObjectIndex,
                variants[item.Data.Models.Count],
                "strict two-section decode and inline spModel validation");

        int evidenceRows=0;
        int primary=variants[1];
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "spSkyBox class/manager code and inherited spNode serializer",
            "The x86 executable identifies spSkyBox and its manager. Object layout " +
            "and registration establish direct spNode inheritance; the own section " +
            "is an ordered repeated model relationship.",pcExecutable.Sha256,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "independent MIPS spSkyBox/spSkyBoxManager identity and spNode layout",
            "The PS2 executable independently contains the same class and manager; " +
            "all PS2 objects use the same node-plus-model relationship structure.",
            ps2Executable.Sha256,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            null,null,"class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload, relationship resolution and nested model decode",
            "All 126 objects decode with 1..3 models. Every one of 159 relationships " +
            "is physically inline, resolves to spModel and passes the full model/" +
            "renderable/mesh/material decoder. No face enum is serialized.",null,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            null,null,"class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical path, object name and ordinal comparison",
            $"PC working/pristine: {pc.EqualBytes}/{pc.Paired} exact. PC/PS2: " +
            $"{cross.EqualSemantic}/{cross.Paired} node/model-count semantic matches; " +
            "native nested mesh data makes complete bytes platform-specific. The only " +
            "node-semantic difference is a non-zero PC-only position on DatingAssets/" +
            "mini_level_date_02 sky; three PC sky boxes have no PS2 resource.",null,now);
        UpdateNavigationClass(connection,transaction,report.TypeHash,"sky_render_node",
            "spNode-derived ordered owner of one to three inline spModel sky/cloud/moon geometries",
            "Complete PC/PS2 read-only decode. Own field 0 repeats inline models; model " +
            "geometry and materials carry visual role. The serializer stores no cube-face " +
            "labels, so model order must be preserved during coordinated edits.");
        transaction.Commit();
        Checkpoint(connection);
        return new SmoResearchClassAnalysisResult(
            report.TypeHash,report.EngineName??"<unknown>","confirmed_read_only",
            report.Profiles.Count,observations.Count,
            report.Profiles.Sum(item=>item.UniqueResourceCount),
            observations.Select(item=>item.SerializedSha256).Distinct().Count(),
            observations.Count,evidenceRows,
            "Three confirmed model-count variants; 159/159 inline models fully decoded " +
            "across PC and PS2.");
    }

    private static List<SkyBoxObservation> LoadSkyBoxObservations(
        SqliteConnection connection)
    {
        var result=new List<SkyBoxObservation>();
        foreach (MeshNavigationFileLocation location in
                 LoadNavigationLocations(connection,SmoClassIds.SkyBox))
        {
            SmoDocument document=SmoDocument.Parse(
                ReadMeshNavigationResource(location),location.RelativePath);
            int ordinal=0;
            foreach (SmoObjectEntry entry in document.Objects.Where(item=>
                         item.TypeHash==SmoClassIds.SkyBox))
            {
                if (!SmoSkyBoxDecoder.TryDecode(document,entry,out var data,out string error) ||
                    data is null)
                    throw new InvalidDataException($"Could not decode sky box: {error}");
                IReadOnlyList<SmoObjectField> fields=SmoObjectFieldReader.Read(document,entry);
                var annotations=new List<MeshNavigationFieldAnnotation>();
                int model=0;
                foreach ((SmoObjectField field,int index) in fields.Select((field,index)=>(field,index)))
                {
                    if (field.FieldType==0 && field.PayloadSize==0) continue;
                    if (!SmoSerializedFieldRegistry.TryDescribeField(
                            entry.TypeHash,fields,index,out var descriptor) || descriptor is null)
                        throw new InvalidDataException("Unregistered spSkyBox field.");
                    object decoded=descriptor.Key switch
                    {
                        "node.position"=>data.Node.Position,
                        "node.rotation"=>data.Node.Rotation,
                        "node.scale"=>data.Node.Scale,
                        "node.is_static"=>data.Node.IsStatic,
                        "node.is_animated"=>data.Node.IsAnimated,
                        "sky_box.model"=>RelationshipValue(data.Models[model++]),
                        _=>throw new InvalidDataException(
                            $"Unexpected sky semantic {descriptor.Key}.")
                    };
                    annotations.Add(new MeshNavigationFieldAnnotation(index,descriptor.Key,
                        descriptor.PayloadLayout,JsonSerializer.Serialize(decoded)));
                }
                string canonical=GetCanonicalResourcePath(location.RelativePath)
                    .ToLowerInvariant();
                string pairing=$"{canonical}|{entry.Name.TrimEnd('\0').ToLowerInvariant()}|{ordinal}";
                string semantic=JsonSerializer.Serialize(new
                {
                    Position=new[] { data.Node.Position.X,data.Node.Position.Y,
                        data.Node.Position.Z },
                    Rotation=new[] { data.Node.Rotation.X,data.Node.Rotation.Y,
                        data.Node.Rotation.Z,data.Node.Rotation.W },
                    Scale=new[] { data.Node.Scale.X,data.Node.Scale.Y,data.Node.Scale.Z },
                    data.Node.IsStatic,data.Node.IsAnimated,ModelCount=data.Models.Count,
                    Types=data.Models.Select(item=>item.TargetTypeHash)
                });
                result.Add(new SkyBoxObservation(location.FileId,entry.Index,
                    location.CorpusKey,pairing,data,ObjectHash(document,entry),semantic,
                    annotations.AsReadOnly()));
                ordinal++;
            }
        }
        return result;
    }

    private sealed record SkyBoxObservation(
        int FileId,int ObjectIndex,string CorpusKey,string PairingKey,
        SmoSkyBoxData Data,string SerializedSha256,string SemanticSignature,
        IReadOnlyList<MeshNavigationFieldAnnotation> Fields);
}
