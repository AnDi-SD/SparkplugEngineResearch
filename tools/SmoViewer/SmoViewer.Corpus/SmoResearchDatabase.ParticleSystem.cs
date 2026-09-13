using System.Numerics;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static readonly IReadOnlyDictionary<int,string> ParticleRegionNames =
        new Dictionary<int,string>
        {
            [12]="point",[13]="plane",[14]="box",[15]="sphere",
            [16]="disk",[17]="cylinder",[18]="cone"
        };

    // Historical complete-corpus profile verifier. Its original counts are not
    // current shared-loader coverage: LoadParticleObservations now refuses any
    // unsupported whole graph before this method can write an analysis result.
    private static SmoResearchClassAnalysisResult AnalyzeParticleSystem(
        string databasePath,SmoResearchClassReport report)
    {
        using SqliteConnection connection=OpenResearch(
            Path.GetFullPath(databasePath),readOnly:false);
        List<ParticleObservation> observations=LoadParticleObservations(connection);
        if (observations.Count!=1745 || observations.Sum(item=>item.Fields.Count)!=26673)
            throw new InvalidDataException("spParticleSystem corpus profile changed.");
        RequireCounts(observations,item=>item.CorpusKey,new Dictionary<string,int>
        { ["pc-working"]=619,["pc-pristine"]=619,["ps2-pristine"]=507 });
        NavigationPairing pc=CompareNavigation(observations,item=>item.CorpusKey,
            item=>item.PairingKey,item=>item.SemanticSignature,
            item=>item.SerializedSha256,"pc-working","pc-pristine");
        NavigationPairing cross=CompareNavigation(observations,item=>item.CorpusKey,
            item=>item.PairingKey,item=>item.SemanticSignature,
            item=>item.SerializedSha256,"pc-pristine","ps2-pristine");
        if (pc!=new NavigationPairing(619,619,619) ||
            cross!=new NavigationPairing(507,455,304))
            throw new InvalidDataException("spParticleSystem corpus pairing changed: "+
                JsonSerializer.Serialize(new { pc,cross }));
        Dictionary<int,int> expectedRegions=new()
        { [12]=480,[13]=117,[14]=15,[15]=646,[16]=110,[17]=5,[18]=372 };
        Dictionary<int,int> actualRegions=observations.GroupBy(item=>item.Data.RegionFieldType)
            .ToDictionary(group=>group.Key,group=>group.Count());
        if (expectedRegions.Any(pair=>!actualRegions.TryGetValue(pair.Key,out int count) ||
                count!=pair.Value) || actualRegions.Count!=expectedRegions.Count)
            throw new InvalidDataException("Particle region variants changed.");

        ExecutableIdentity pcExecutable=FindExecutable(connection,"pc");
        ExecutableIdentity ps2Executable=FindExecutable(connection,"ps2");
        string[] tokens=
        [
            "spParticleSystem","spParticleSystemSerializer","esfParticleAccel",
            "esfParticleDirection","esfParticleRenderNode","esfParticleRegionPoint",
            "esfParticleRegionPlane","esfParticleRegionBox","esfParticleRegionSphere",
            "esfParticleRegionDisk","esfParticleRegionCylinder","esfParticleRegionCone",
            "pPlane->m_vNormal","pCylinder->m_fHeight","pCone->m_fRadius2"
        ];
        RequireAsciiTokens(pcExecutable.Path,tokens);
        RequireAsciiTokens(ps2Executable.Path,tokens);
        string now=DateTime.UtcNow.ToString("O");
        using SqliteTransaction transaction=connection.BeginTransaction();
        ClearNavigationAnalysis(connection,transaction,report.TypeHash,"particle_system_%");
        UpsertNavigationDefinitions(connection,transaction,report.TypeHash,
            observations.SelectMany(item=>item.Fields),ParticleDefinitions(),
            includeNode:false,includeRenderNode:false);
        AnnotateNavigationFields(connection,transaction,observations.SelectMany(item=>
            item.Fields.Select(field=>(item.FileId,item.ObjectIndex,field))));
        var variants=new Dictionary<int,int>();
        foreach (int region in ParticleRegionNames.Keys)
        {
            ParticleObservation[] members=observations.Where(item=>
                item.Data.RegionFieldType==region).ToArray();
            variants[region]=InsertNavigationVariant(connection,transaction,
                report.TypeHash,$"particle_system_{ParticleRegionNames[region]}",
                $"{ParticleRegionNames[region]} emission region",
                JsonSerializer.Serialize(new
                {
                    regionField=region,region=ParticleRegionNames[region],
                    objects=members.Length,corpora=members.GroupBy(item=>item.CorpusKey)
                        .ToDictionary(item=>item.Key,item=>item.Count())
                }),"Exactly one region field is serialized per particle system.",now);
        }
        foreach (ParticleObservation item in observations)
            AssignNavigationVariant(connection,transaction,item.FileId,item.ObjectIndex,
                variants[item.Data.RegionFieldType],
                "strict renderable/particle decode and exclusive region discriminator");

        int evidenceRows=0;int primary=variants[15];
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            pcExecutable.PlatformId,pcExecutable.CorpusId,
            "class_analysis:pc_executable",pcExecutable.Path,
            "spParticleSystemSerializer field enums/getter assertions and region-member strings",
            "The x86 executable names all fields and confirms acceleration begin/end, " +
            "velocity/angle minima/maxima, scale/color begin/end, emission/lifetime, " +
            "Boolean modes, rate, bounding sphere and render node. Region assertions " +
            "prove exact position/normal/size/radius/height member order.",
            pcExecutable.Sha256,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            ps2Executable.PlatformId,ps2Executable.CorpusId,
            "class_analysis:ps2_executable",ps2Executable.Path,
            "independent MIPS serializer enums/getters and matching region-member strings",
            "The PS2 executable independently contains the same 20 enum names, getter " +
            "assertions and region struct members; scalar widths/order match PC.",
            ps2Executable.Sha256,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            null,null,"class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict source reload of inherited renderable and all particle fields",
            "All 1,745 objects decode with required acceleration, scale, bounding sphere, " +
            "render node and exactly one valid region. The seven region totals are point " +
            "480, plane 117, box 15, sphere 646, disk 110, cylinder 5 and cone 372. " +
            "All material/fog/render-node relationships resolve; one PC root render-node " +
            "relationship uses the legacy four-byte ID-only form.",null,now);
        evidenceRows+=InsertEvidence(connection,transaction,report.TypeHash,primary,
            null,null,"class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "canonical path, particle name and ordinal comparison",
            $"PC working/pristine: {pc.EqualBytes}/{pc.Paired} exact. PC/PS2: " +
            $"{cross.EqualSemantic}/{cross.Paired} exact field-semantic matches and " +
            $"{cross.EqualBytes}/{cross.Paired} complete bytes; 112 PC particles have no " +
            "PS2 resource counterpart.",null,now);
        UpdateNavigationClass(connection,transaction,report.TypeHash,
            "particle_renderable",
            "spRenderable-derived particle emitter with ranges, simulation flags, one of seven regions and a render-node relationship",
            "Loaded shared ParticleSystem parameters and inherited renderable references; " +
            "typed values include constructor defaults. One supported emission region is required; " +
            "raw fields retain serialization presence separately. Looping initialization remains unsupported.");
        transaction.Commit();Checkpoint(connection);
        return new SmoResearchClassAnalysisResult(report.TypeHash,
            report.EngineName??"<unknown>","confirmed_read_only",report.Profiles.Count,
            observations.Count,report.Profiles.Sum(item=>item.UniqueResourceCount),
            observations.Select(item=>item.SerializedSha256).Distinct().Count(),
            observations.Count,evidenceRows,
            "Seven confirmed emission-region variants and complete 20-field PC/PS2 decode.");
    }

    private static IReadOnlyList<(int Section,int Type,int Occurrence,string Semantic,
        string Display,string Kind,string Layout,bool Repeated,string Notes)>
        ParticleDefinitions() =>
    [
        (1,0,0,"renderable.material","Material","object_relationship","relationship to spMaterialData",false,"Present and resolved in all objects."),
        (1,1,0,"renderable.fog","Fog","object_relationship","relationship to spFog",false,"Present and resolved in all objects."),
        (1,2,0,"renderable.alpha_sort","Alpha sort","uint32_boolean","little-endian UInt32 0/1",false,"Paired with priority; omitted in one object per PC corpus."),
        (1,3,0,"renderable.priority","Priority","uint32","little-endian UInt32",false,"Paired with alpha sort."),
        (0,0,0,"particle.acceleration","Acceleration range","vector3_pair","Vector3 begin and Vector3 end",false,"Required."),
        (0,1,0,"particle.direction","Emission direction","vector3","Vector3",false,"Optional."),
        (0,2,0,"particle.velocity","Velocity range","single_pair","Single min/max",false,"Optional."),
        (0,3,0,"particle.angle","Angle range","single_pair","Single min/max",false,"Optional."),
        (0,4,0,"particle.scale","Scale range","single_pair","Single begin/end",false,"Required."),
        (0,5,0,"particle.color","Color range","argb_pair","ARGB UInt32 begin/end",false,"Optional."),
        (0,6,0,"particle.time","Emission/lifetime","single_pair","Single emission time/lifetime",false,"Optional."),
        (0,7,0,"particle.loop","Loop animation","boolean","one byte 0/1",false,"Optional."),
        (0,8,0,"particle.world_space","World-space simulation","boolean","one byte 0/1",false,"Optional."),
        (0,9,0,"particle.iterative","Iterative mode","boolean","one byte 0/1",false,"Optional."),
        (0,10,0,"particle.rate","Emission rate","single","little-endian Single",false,"Optional."),
        (0,11,0,"particle.bounding_sphere","Bounding sphere","single","little-endian Single radius",false,"Required."),
        (0,12,0,"particle.region.point","Point region","region_point","Vector3 position",false,"Exclusive region variant."),
        (0,13,0,"particle.region.plane","Plane region","region_plane","Vector3 position/normal; Single sizeX/sizeY",false,"Exclusive region variant."),
        (0,14,0,"particle.region.box","Box region","region_box","Vector3 position/size",false,"Exclusive region variant."),
        (0,15,0,"particle.region.sphere","Sphere region","region_sphere","Vector3 position; Single radius",false,"Exclusive region variant."),
        (0,16,0,"particle.region.disk","Disk region","region_disk","Vector3 position; Single radius",false,"Exclusive region variant."),
        (0,17,0,"particle.region.cylinder","Cylinder region","region_cylinder","Vector3 position; Single height/radius",false,"Exclusive region variant."),
        (0,18,0,"particle.region.cone","Cone region","region_cone","Vector3 position; Single height/radius1/radius2",false,"Exclusive region variant."),
        (0,19,0,"particle.render_node","Render node","object_relationship","relationship to spRenderNode",false,"Required; sized reference except one PC ID-only root object.")
    ];

    private static List<ParticleObservation> LoadParticleObservations(
        SqliteConnection connection)
    {
        var result=new List<ParticleObservation>();
        foreach (MeshNavigationFileLocation location in
                 LoadNavigationLocations(connection,SmoClassIds.ParticleSystem))
        {
            SmoDocument document=SmoDocument.Parse(
                ReadMeshNavigationResource(location),location.RelativePath);
            var nameOrdinals=new Dictionary<string,int>(StringComparer.OrdinalIgnoreCase);
            foreach (SmoObjectEntry entry in document.Objects.Where(item=>
                         item.TypeHash==SmoClassIds.ParticleSystem))
            {
                if (!SmoParticleSystemDecoder.TryDecode(
                        document,entry,out var data,out string error) || data is null)
                    throw new InvalidDataException($"Could not decode particle: {error}");
                IReadOnlyList<SmoObjectField> fields=SmoObjectFieldReader.Read(document,entry);
                var annotations=new List<MeshNavigationFieldAnnotation>();
                foreach ((SmoObjectField field,int index) in fields.Select((field,index)=>(field,index)))
                {
                    if (field.FieldType==0 && field.PayloadSize==0) continue;
                    if (!SmoSerializedFieldRegistry.TryDescribeField(
                            entry.TypeHash,fields,index,out var descriptor) || descriptor is null)
                        throw new InvalidDataException("Unregistered particle field.");
                    annotations.Add(new MeshNavigationFieldAnnotation(index,descriptor.Key,
                        descriptor.PayloadLayout,JsonSerializer.Serialize(
                            ParticleDecodedValue(data,descriptor.Key))));
                }
                string name=entry.Name.TrimEnd('\0').ToLowerInvariant();
                nameOrdinals.TryGetValue(name,out int ordinal);nameOrdinals[name]=ordinal+1;
                string canonical=GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant();
                string pairing=$"{canonical}|{name}|{ordinal}";
                string semantic=JsonSerializer.Serialize(new
                {
                    data.Renderable.AlphaSortEnable,data.Renderable.Priority,
                    MaterialType=data.Renderable.Material?.TypeHash,
                    FogType=data.Renderable.Fog?.TypeHash,
                    Own=fields.Where(field=>field.RelativeHeaderOffset>
                            fields.First(item=>item.FieldType==0&&item.PayloadSize==0).RelativeHeaderOffset &&
                            field.FieldType!=19 && field.PayloadSize!=0)
                        .Select(field=>new { field.FieldType,
                            Hex=Convert.ToHexString(field.Payload.Span) }),
                    data.RegionFieldType,RenderType=data.RenderNode?.TypeHash
                });
                result.Add(new ParticleObservation(location.FileId,entry.Index,
                    location.CorpusKey,pairing,data,ObjectHash(document,entry),semantic,
                    annotations.AsReadOnly()));
            }
        }
        return result;
    }

    private static object ParticleDecodedValue(SmoParticleSystemData data,string key) =>
        key switch
        {
            "renderable.material"=>ParticleResourceValue(data.Renderable.Material),
            "renderable.fog"=>ParticleResourceValue(data.Renderable.Fog),
            "renderable.alpha_sort"=>data.Renderable.AlphaSortEnable!,
            "renderable.priority"=>data.Renderable.Priority!,
            "particle.acceleration"=>new
                { Begin=VectorValue(data.AccelerationBegin),End=VectorValue(data.AccelerationEnd) },
            "particle.direction"=>VectorValue(data.EmissionDirection),
            "particle.velocity"=>RangeValue(data.Velocity!),
            "particle.angle"=>RangeValue(data.Angle!),
            "particle.scale"=>RangeValue(data.Scale),
            "particle.color"=>new { data.Color.Begin,data.Color.End },
            "particle.time"=>RangeValue(data.Time!),
            "particle.loop"=>data.LoopAnimation!,
            "particle.world_space"=>data.WorldSpace!,
            "particle.iterative"=>data.IterativeMode!,
            "particle.rate"=>data.EmissionRate!,
            "particle.bounding_sphere"=>data.BoundingSphere,
            "particle.region.point" or "particle.region.plane" or
            "particle.region.box" or "particle.region.sphere" or
            "particle.region.disk" or "particle.region.cylinder" or
            "particle.region.cone"=>ParticleRegionValue(data.Region),
            "particle.render_node"=>ParticleResourceValue(data.RenderNode),
            _=>throw new InvalidDataException($"Unexpected particle semantic {key}.")
        };

    private static object ParticleResourceValue(SmoObjectEntry? entry)=>new { ObjectId=entry?.Id??0,ObjectIndex=entry?.Index,TypeHash=entry?.TypeHash,Name=entry?.Name };

    private static object ParticleRegionValue(object region) => region switch
    {
        SmoParticlePointRegion value=>new { Position=VectorValue(value.Position) },
        SmoParticlePlaneRegion value=>new { Position=VectorValue(value.Position),
            Normal=VectorValue(value.Normal),value.SizeX,value.SizeY },
        SmoParticleBoxRegion value=>new { Position=VectorValue(value.Position),
            Size=VectorValue(value.Size) },
        SmoParticleSphereRegion value=>new { Position=VectorValue(value.Position),value.Radius },
        SmoParticleDiskRegion value=>new { Position=VectorValue(value.Position),value.Radius },
        SmoParticleCylinderRegion value=>new { Position=VectorValue(value.Position),
            value.Radius,value.Height },
        SmoParticleConeRegion value=>new { Position=VectorValue(value.Position),
            value.Radius1,value.Radius2,value.Height },
        _=>throw new InvalidDataException("Unknown particle region.")
    };

    private static object VectorValue(Vector3 value)=>new { value.X,value.Y,value.Z };
    private static object RangeValue(SmoSingleRange value)=>new { value.Begin,value.End };

    private sealed record ParticleObservation(
        int FileId,int ObjectIndex,string CorpusKey,string PairingKey,
        SmoParticleSystemData Data,string SerializedSha256,string SemanticSignature,
        IReadOnlyList<MeshNavigationFieldAnnotation> Fields);
}
