using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeLensFlare(
        string databasePath,SmoResearchClassReport report)
    {
        using SqliteConnection connection=OpenResearch(
            Path.GetFullPath(databasePath),readOnly:false);
        List<LensFlareObservation> observations=LoadLensFlareObservations(connection);
        if (observations.Count!=6 || observations.Sum(item=>item.Fields.Count)!=36 ||
            observations.Any(item=>item.Data.Primary.Material is null||item.Data.Elements.Count!=0||
                item.Data.OcclusionSphereRadius!=100f||item.Data.OcclusionSpeed!=5f))
            throw new InvalidDataException("spLensFlare profile changed.");
        RequireCounts(observations,item=>item.CorpusKey,new Dictionary<string,int>
        { ["pc-working"]=2,["pc-pristine"]=2,["ps2-pristine"]=2 });
        NavigationPairing pc=CompareNavigation(observations,item=>item.CorpusKey,
            item=>item.PairingKey,item=>item.SemanticSignature,item=>item.SerializedSha256,
            "pc-working","pc-pristine");
        NavigationPairing cross=CompareNavigation(observations,item=>item.CorpusKey,
            item=>item.PairingKey,item=>item.SemanticSignature,item=>item.SerializedSha256,
            "pc-pristine","ps2-pristine");
        if (pc!=new NavigationPairing(2,2,2)||cross!=new NavigationPairing(2,2,0))
            throw new InvalidDataException("Lens-flare pairing changed.");
        ExecutableIdentity pcExecutable=FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable=FindExecutable(connection,"ps2");
        string[] tokens=["spLensFlare","spLensFlareSerializer","esfLensFlareElements",
            "esfLensFlareGlare","esfLensFlareOcclusion","esfLensFlareRenderNode",
            "pLensFlare->GetElementColor","pLensFlare->GetElementRelativeDistance",
            "pLensFlare->GetOcclusionSphereRadius","pLensFlare->GetOcclusionSpeed"];
        RequireAsciiTokens(pcExecutable.Path,tokens);RequireAsciiTokens(ps2Executable.Path,tokens);
        string now=DateTime.UtcNow.ToString("O");
        using SqliteTransaction transaction=connection.BeginTransaction();
        ClearNavigationAnalysis(connection,transaction,report.TypeHash,"lens_flare_%");
        UpsertNavigationDefinitions(connection,transaction,report.TypeHash,
            observations.SelectMany(item=>item.Fields),LensFlareDefinitions(),
            includeNode:false,includeRenderNode:false);
        AnnotateNavigationFields(connection,transaction,observations.SelectMany(item=>
            item.Fields.Select(field=>(item.FileId,item.ObjectIndex,field))));
        int variant=InsertNavigationVariant(connection,transaction,report.TypeHash,
            "lens_flare_single_element_no_glare","Single element, no glare",
            JsonSerializer.Serialize(new { primaryElementCount=1,additionalElementCount=0,
                occlusionRadius=100,occlusionSpeed=5,objects=observations.Count }),
            "The sole observed layout is shared by both levels and both platforms.",now);
        foreach (LensFlareObservation item in observations)
            AssignNavigationVariant(connection,transaction,item.FileId,item.ObjectIndex,
                variant,"strict renderable/lens record and nested material decode");
        int evidenceRows=0;
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,variant,
            pcExecutable.PlatformId,pcExecutable.CorpusId,"class_analysis:pc_executable",
            pcExecutable.Path,"x86 spLensFlareSerializer enums and element/glare/occlusion getter assertions",
            "The PC executable names all four fields and independently confirms element/" +
            "glare material, color, relative distance and scale plus occlusion sphere " +
            "radius/speed and render node.",pcExecutable.Sha256,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,variant,
            ps2Executable.PlatformId,ps2Executable.CorpusId,"class_analysis:ps2_executable",
            ps2Executable.Path,"MIPS spLensFlareSerializer enums and matching getter assertions",
            "The PS2 executable independently exposes the same fields and getters; its " +
            "platform-specific manager consumes the same logical values.",ps2Executable.Sha256,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,variant,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload, compound-record boundaries and nested material decode",
            "All six corpus objects decode. Each contains one inline spMaterialData element " +
            "followed by white ARGB, relative distance 0 and scale 200; the additional element array has count zero; occlusion is radius 100/speed 5; render-node relationships resolve.",
            null,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,variant,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical battle_01/Gardenia03 resource pairing",
            $"PC working/pristine: {pc.EqualBytes}/{pc.Paired} exact. PC/PS2: all " +
            $"{cross.EqualSemantic}/{cross.Paired} logical values match; complete bytes " +
            "differ because the inline material contains platform-native texture/mesh data.",
            null,now);
        UpdateNavigationClass(connection,transaction,report.TypeHash,"lens_flare_renderable",
            "spRenderable-derived lens flare with material-backed elements/glare, occlusion parameters and render node",
            "Complete PC/PS2 read-only decode. Element/glare records are relationship + " +
            "ARGB + relative-distance + scale; the additional array starts with a UInt32 count. Editing " +
            "requires coordinated nested material and render-node ownership handling.");
        transaction.Commit();Checkpoint(connection);
        return new SmoResearchClassAnalysisResult(report.TypeHash,
            report.EngineName??"<unknown>","confirmed_read_only",report.Profiles.Count,
            observations.Count,report.Profiles.Sum(item=>item.UniqueResourceCount),
            observations.Select(item=>item.SerializedSha256).Distinct().Count(),
            observations.Count,evidenceRows,
            "One confirmed shared layout; all compound values and nested materials decoded.");
    }

    private static IReadOnlyList<(int Section,int Type,int Occurrence,string Semantic,
        string Display,string Kind,string Layout,bool Repeated,string Notes)>
        LensFlareDefinitions()=>
    [
        (1,0,0,"renderable.material","Inherited material","object_relationship","relationship to spMaterialData",false,"Optional inherited field; absent in corpus because element owns its material."),
        (1,1,0,"renderable.fog","Inherited fog","object_relationship","relationship to spFog",false,"Optional inherited field; absent in corpus."),
        (1,2,0,"renderable.alpha_sort","Alpha sort","uint32_boolean","little-endian UInt32 0/1",false,"Required and true in all observed objects."),
        (1,3,0,"renderable.priority","Priority","uint32","little-endian UInt32",false,"Required and zero in all observed objects."),
        (0,0,-1,"lens_flare.element","Primary element","lens_flare_element","material relationship; ARGB UInt32; Single relative distance/scale",true,"Primary element; repeated fields replace it, one observed."),
        (0,1,0,"lens_flare.glare","Additional elements","counted_lens_flare_elements","UInt32 count followed by material/color/distance/scale records",false,"Zero count in the observed corpus; not a null material ID."),
        (0,2,0,"lens_flare.occlusion","Occlusion","single_pair","Single sphere radius/speed",false,"Required; 100 and 5 observed."),
        (0,3,0,"lens_flare.render_node","Render node","object_relationship","relationship to spRenderNode",false,"Required and resolved.")
    ];

    private static List<LensFlareObservation> LoadLensFlareObservations(
        SqliteConnection connection)
    {
        var result=new List<LensFlareObservation>();
        foreach (MeshNavigationFileLocation location in
                 LoadNavigationLocations(connection,SmoClassIds.LensFlare))
        {
            SmoDocument document=SmoDocument.Parse(
                ReadMeshNavigationResource(location),location.RelativePath);
            foreach (SmoObjectEntry entry in document.Objects.Where(item=>
                         item.TypeHash==SmoClassIds.LensFlare))
            {
                if (!SmoLensFlareDecoder.TryDecode(document,entry,out var data,out string error)||
                    data is null) throw new InvalidDataException($"Could not decode lens flare: {error}");
                IReadOnlyList<SmoObjectField> direct=SmoObjectFieldReader.Read(document,entry);
                var fields=new List<MeshNavigationFieldAnnotation>();
                foreach ((SmoObjectField field,int index) in direct.Select((field,index)=>(field,index)))
                {
                    if (field.FieldType==0&&field.PayloadSize==0) continue;
                    if (!SmoSerializedFieldRegistry.TryDescribeField(
                            entry.TypeHash,direct,index,out var descriptor)||descriptor is null)
                        throw new InvalidDataException("Unregistered lens-flare field.");
                    object? decoded=descriptor.Key switch
                    {
                        "renderable.alpha_sort"=>data.Renderable.AlphaSortEnable!,
                        "renderable.priority"=>data.Renderable.Priority!,
                        "lens_flare.element"=>LensElementValue(data.Primary),
                        "lens_flare.glare"=>data.Elements.Select(LensElementValue).ToArray(),
                        "lens_flare.occlusion"=>new { data.OcclusionSphereRadius,data.OcclusionSpeed },
                        "lens_flare.render_node"=>LensObjectValue(data.RenderNode),
                        _=>throw new InvalidDataException($"Unexpected lens semantic {descriptor.Key}.")
                    };
                    fields.Add(new MeshNavigationFieldAnnotation(index,descriptor.Key,
                        descriptor.PayloadLayout,JsonSerializer.Serialize(decoded)));
                }
                string canonical=GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant();
                string semantic=JsonSerializer.Serialize(new
                {
                    data.Renderable.AlphaSortEnable,data.Renderable.Priority,
                    Primary=LensElementSemantic(data.Primary),Elements=data.Elements.Select(LensElementSemantic),
                    data.OcclusionSphereRadius,data.OcclusionSpeed,
                    RenderType=data.RenderNode?.TypeHash
                });
                result.Add(new LensFlareObservation(location.FileId,entry.Index,
                    location.CorpusKey,canonical,data,ObjectHash(document,entry),semantic,
                    fields.AsReadOnly()));
            }
        }
        return result;
    }

    private static object? LensObjectValue(SmoObjectEntry? value)=>value is null?null:new { value.Id,value.Index,value.TypeHash,value.Name };
    private static object LensElementValue(SmoLensFlareElement value)=>new
    { Material=LensObjectValue(value.Material),value.Color,value.RelativeDistance,value.Scale };
    private static object LensElementSemantic(SmoLensFlareElement value)=>new
    { MaterialType=value.Material?.TypeHash,value.Color,value.RelativeDistance,value.Scale };

    private sealed record LensFlareObservation(
        int FileId,int ObjectIndex,string CorpusKey,string PairingKey,SmoLensFlareData Data,
        string SerializedSha256,string SemanticSignature,
        IReadOnlyList<MeshNavigationFieldAnnotation> Fields);
}
