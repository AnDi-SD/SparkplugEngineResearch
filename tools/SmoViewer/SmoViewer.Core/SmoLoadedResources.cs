using System.Collections.ObjectModel;
using System.Numerics;
using System.Runtime.CompilerServices;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

public sealed record SmoLoadedTexture(uint ObjectId, int ObjectIndex, SmoTexture? Texture, string? Issue)
{
    public bool UsesHostCompatibility { get; init; }
}
public sealed record SmoLoadedTextureKey(float Time, SmoLoadedTexture? Texture);
public sealed record SmoLoadedTextureAnimation(uint ObjectId, int ObjectIndex, float Duration,
    IReadOnlyList<SmoLoadedTextureKey> Keys);
public sealed record SmoLoadedMaterialLayer(uint ClassId, SmoLoadedTexture? Texture,
    SmoLoadedTextureAnimation? Animation, uint UvControllerId, bool UvEnabled,
    IReadOnlyList<float> UvMatrix, IReadOnlyList<uint> TextureStates,
    bool AnimationBoundHere, bool UvBoundHere);
public sealed record SmoLoadedMaterialPass(uint Blend, IReadOnlyList<SmoLoadedMaterialLayer> Layers);
public sealed record SmoLoadedMaterial(uint ObjectId, int ObjectIndex, IReadOnlyList<uint> RenderStates,
    byte VertexAlpha, IReadOnlyList<Vector4> Colors, float? SpecularPower, uint ColorControllerId,
    IReadOnlyList<SmoLoadedMaterialPass> Passes);
public sealed record SmoLoadedModel(int ObjectIndex, uint MeshId, int? MeshObjectIndex,
    uint MaterialId, SmoLoadedMaterial? Material, uint FogId, bool AlphaSort,
    uint Priority, uint Projection, string? Issue)
{
    public SmoFogDraw? FogDraw { get; init; }
    public uint? SkinWeightCount { get; init; }
    public IReadOnlyList<Matrix4x4>? InitialSkinPalette { get; init; }
    public string? SkinIssue { get; init; }
    public SmoAlphaSortData? AlphaSortData { get; init; }
}
public sealed record SmoLoadedNode(uint ObjectId, int ObjectIndex, int? ParentObjectIndex, Matrix4x4 World);
public enum SmoRenderContainerKind { RenderNode, StaticRenderObject, PartitionRenderable, SkyBox }
/// <summary>Actual ordered support members; not a visibility result or frame draw order.</summary>
public sealed record SmoLoadedRenderContainer(uint ObjectId, int ObjectIndex, SmoRenderContainerKind Kind,
    Matrix4x4? World, Matrix4x4? Inverse, IReadOnlyList<int> RenderableObjectIndices)
{
    public SmoSkyBoxPose? SkyPose {get;init;}
}
/// <summary>Host identity of a reference slot; it is not a serialized game object.</summary>
public readonly record struct SmoRenderOccurrenceKey(int ContainerObjectIndex, int MemberSlot);
public sealed record SmoLoadedRenderOccurrence(SmoRenderOccurrenceKey Key, int RenderableObjectIndex,
    Matrix4x4? InputWorld, int? RigidNodeObjectIndex, string? Issue);

/// <summary>
/// Immutable upload/inspection snapshot copied from one actual loaded Sparkplug
/// resource graph. The graph is disposed after copying; this is not an animation
/// runtime. No file grammar, reference resolution or representation selection is
/// implemented here. A separate edited document receives a separate snapshot.
/// </summary>
public sealed class SmoLoadedResources
{
    private static readonly ConditionalWeakTable<SmoDocument, Lazy<SmoLoadedResources>> Cache = new();
    public IReadOnlyDictionary<int, SmoLoadedModel> Models { get; }
    public string? LoadIssue { get; }
    public string? SceneIssue { get; }
    public IReadOnlyList<string> CompatibilityIssues { get; } = Array.Empty<string>();
    public IReadOnlyDictionary<int,SmoLoadedFont> Fonts { get; } = new ReadOnlyDictionary<int,SmoLoadedFont>(new Dictionary<int,SmoLoadedFont>());
    public IReadOnlyDictionary<int,SmoLoadedText> Texts { get; } = new ReadOnlyDictionary<int,SmoLoadedText>(new Dictionary<int,SmoLoadedText>());
    public IReadOnlyDictionary<int,int?> TextNodeCachedTexts { get; } = new ReadOnlyDictionary<int,int?>(new Dictionary<int,int?>());
    // Explicit frame1 preview operation, captured after immutable inspection.
    // Reuses the same native graph and uploaded texture DTOs; no second load.
    public IReadOnlyDictionary<int, SmoMaterialDraw> PreviewMaterialDraws { get; }
    public IReadOnlyList<string> PreviewMaterialIssues { get; }
    public SmoFileReferenceTrace? ReferenceTrace { get; }
    public string? ReferenceTraceIssue { get; }
    internal NavigationSnapshot? Navigation { get; }
    internal SpatialSnapshot? Spatial { get; }
    public IReadOnlyDictionary<int,SmoLensFlareData> LensFlares { get; } = new ReadOnlyDictionary<int,SmoLensFlareData>(new Dictionary<int,SmoLensFlareData>());
    public IReadOnlyDictionary<int,SmoParticleSystemData> Particles { get; } = new ReadOnlyDictionary<int,SmoParticleSystemData>(new Dictionary<int,SmoParticleSystemData>());
    public IReadOnlyDictionary<int, SmoLoadedNode> Nodes { get; } = new ReadOnlyDictionary<int, SmoLoadedNode>(new Dictionary<int, SmoLoadedNode>());
    public IReadOnlyDictionary<int, Matrix4x4> NodeWorlds { get; } = new ReadOnlyDictionary<int, Matrix4x4>(new Dictionary<int, Matrix4x4>());
    public IReadOnlyList<SmoLoadedRenderContainer> RenderContainers { get; } = Array.Empty<SmoLoadedRenderContainer>();
    public IReadOnlyList<SmoLoadedRenderOccurrence> RenderOccurrences { get; } = Array.Empty<SmoLoadedRenderOccurrence>();
    public IReadOnlyDictionary<int, SmoLoadedRenderContainer> RenderContainersByObjectIndex { get; } = new ReadOnlyDictionary<int, SmoLoadedRenderContainer>(new Dictionary<int, SmoLoadedRenderContainer>());
    public IReadOnlyDictionary<int, IReadOnlyList<SmoLoadedRenderContainer>> RenderContainersByRenderable { get; } = new ReadOnlyDictionary<int, IReadOnlyList<SmoLoadedRenderContainer>>(new Dictionary<int, IReadOnlyList<SmoLoadedRenderContainer>>());

    public static SmoLoadedResources Get(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        return Cache.GetValue(document, static value => new Lazy<SmoLoadedResources>(() => new SmoLoadedResources(value))).Value;
    }

    private unsafe SmoLoadedResources(SmoDocument document)
    {
        var models = new Dictionary<int, SmoLoadedModel>();
        Models = new ReadOnlyDictionary<int, SmoLoadedModel>(models);
        var draws = new Dictionary<int, SmoMaterialDraw>();
        var drawIssues = new List<string>();
        PreviewMaterialDraws = new ReadOnlyDictionary<int, SmoMaterialDraw>(draws);
        PreviewMaterialIssues = drawIssues.AsReadOnly();
        try
        {
            IntPtr pointer;
            fixed (byte* bytes = document.Data.Span)
                pointer = NativeMethods.Check(NativeMethods.spv_graph_load_for_tools(bytes, checked((uint)document.Data.Length),1));
            using var graph = new GraphHandle(pointer);
            var legacyTextureIds=SparkplugGraphCompatibility.ReadTextureIds(graph);
            CompatibilityIssues=Array.AsReadOnly(legacyTextureIds.Select(SparkplugGraphCompatibility.Describe).ToArray());
            try { ReferenceTrace = SmoFileReferenceTrace.Read(document, graph); }
            catch (InvalidDataException exception) { ReferenceTraceIssue = exception.Message; }
            Navigation = NavigationSnapshot.Read(graph);
            Spatial = SpatialSnapshot.Read(graph);
            var entries = document.Objects.ToDictionary(entry => entry.Id);
            var materialView = new SparkplugMaterialView(document, graph,legacyTextureIds);
            var fogDraws = new Dictionary<uint, SmoFogDraw>();
            SmoFogDraw Fog(uint id)
            {
                if (!fogDraws.TryGetValue(id, out var value)) fogDraws.Add(id, value = SmoFogDraw.Read(graph, id));
                return value;
            }

            SmoObjectEntry Entry(uint id) => entries.TryGetValue(id, out var value) ? value
                : throw new InvalidDataException($"Loaded resource ID {id} is absent from this document's FAT.");
            SmoLoadedRenderableData Renderable(SmoObjectEntry entry)
            {
                NativeMethods.Check(NativeMethods.spv_graph_renderable(graph,entry.Id,out var inherited));
                return new(inherited.Material==0?null:Entry(inherited.Material),inherited.Fog==0?null:Entry(inherited.Fog),inherited.Alpha,inherited.Priority);
            }

            var fonts=new Dictionary<int,SmoLoadedFont>();
            foreach(var entry in document.Objects.Where(entry=>entry.TypeHash==SmoClassIds.Font))
            {
                var nativeGlyphs=new NativeMethods.FontGlyph[224];NativeMethods.GraphFont info;
                fixed(NativeMethods.FontGlyph* output=nativeGlyphs)NativeMethods.Check(NativeMethods.spv_graph_font(graph,entry.Id,out info,output,224));
                var glyphs=nativeGlyphs.Select((glyph,index)=>new SmoFontGlyph(checked((byte)(index+32)),checked((byte)glyph.Width),glyph.Uv0,glyph.Uv1)).ToArray();
                fonts.Add(entry.Index,new(entry.Id,entry.Index,info.Height,info.BaselinePresent!=0?info.Baseline:null,materialView.Texture(info.Image),Array.AsReadOnly(glyphs)));
            }
            Fonts=new ReadOnlyDictionary<int,SmoLoadedFont>(fonts);
            var texts=new Dictionary<int,SmoLoadedText>();
            foreach(var entry in document.Objects.Where(entry=>entry.TypeHash==SmoClassIds.TextRenderable))
            {
                NativeMethods.Check(NativeMethods.spv_graph_text(graph,entry.Id,out var info));
                if(info.TextBytes>65535||info.TextPresent>1||info.BoundsMask>3)throw new InvalidDataException("Invalid loaded Text output.");
                var bytes=new byte[checked((int)info.TextBytes)];
                fixed(byte* output=bytes)NativeMethods.Check(NativeMethods.spv_graph_text_bytes(graph,entry.Id,output,info.TextBytes));
                NativeMethods.Check(NativeMethods.spv_graph_alpha_info(graph,entry.Id,out var alpha));
                var textRenderable = Renderable(entry);
                texts.Add(entry.Index,new(entry.Id,entry.Index,textRenderable,info.Font==0?null:fonts[Entry(info.Font).Index],info.TextPresent!=0,bytes,
                    info.Color,info.Wrap,info.Alignment,info.Width,info.Sphere,(info.BoundsMask&1)!=0?info.Minimum:null,(info.BoundsMask&2)!=0?info.Maximum:null)
                    {AlphaSortData=new(alpha.Queued!=0,alpha.Priority,alpha.Particle!=0,alpha.Sphere),FogDraw=Fog(textRenderable.Fog?.Id ?? 0)});
            }
            Texts=new ReadOnlyDictionary<int,SmoLoadedText>(texts);
            var textNodes=new Dictionary<int,int?>();
            foreach(var entry in document.Objects.Where(entry=>entry.TypeHash==SmoClassIds.TextNode))
            {
                NativeMethods.Check(NativeMethods.spv_graph_text_node(graph,entry.Id,out uint text));
                textNodes.Add(entry.Index,text==0?null:Entry(text).Index);
            }
            TextNodeCachedTexts=new ReadOnlyDictionary<int,int?>(textNodes);

            var flares=new Dictionary<int,SmoLensFlareData>();
            foreach(var entry in document.Objects.Where(entry=>entry.TypeHash==SmoClassIds.LensFlare))
            {
                NativeMethods.Check(NativeMethods.spv_graph_lens_flare(graph,entry.Id,out var state));
                SmoLensFlareElement Element(uint ordinal)
                {
                    NativeMethods.Check(NativeMethods.spv_graph_lens_flare_element(graph,entry.Id,ordinal,out var value));
                    return new(value.Material==0?null:Entry(value.Material),value.Color,value.Distance,value.Scale);
                }
                flares.Add(entry.Index,new(Renderable(entry),Element(uint.MaxValue),
                    Array.AsReadOnly(Enumerable.Range(0,checked((int)state.Elements)).Select(index=>Element((uint)index)).ToArray()),
                    state.Radius,state.Speed,state.RenderNode==0?null:Entry(state.RenderNode)));
            }
            LensFlares=new ReadOnlyDictionary<int,SmoLensFlareData>(flares);

            var particles=new Dictionary<int,SmoParticleSystemData>();
            foreach(var entry in document.Objects.Where(entry=>entry.TypeHash==SmoClassIds.ParticleSystem))
            {
                NativeMethods.Check(NativeMethods.spv_graph_particle(graph,entry.Id,out var p));
                NativeMethods.Check(NativeMethods.spv_graph_particle_pool(graph,entry.Id,out var poolInfo,null,0));
                if(poolInfo.Initialized!=1||poolInfo.Count>1024)throw new InvalidDataException("Particle CPU pool is not initialized within its supported bounds.");
                var nativeRecords=new NativeMethods.ParticleRecord[checked((int)poolInfo.Count)];
                fixed(NativeMethods.ParticleRecord* records=nativeRecords)
                    NativeMethods.Check(NativeMethods.spv_graph_particle_pool(graph,entry.Id,out poolInfo,records,poolInfo.Count));
                var poolSnapshot=new SmoParticlePoolSnapshot(poolInfo.First,poolInfo.Boundary,
                    Array.AsReadOnly(nativeRecords.Select(record=>new SmoParticleRecord(record.Position,record.Velocity,record.Birth,record.Lifetime,
                        record.Written!=0,record.Previous,record.Next)).ToArray()));
                Vector3 position=new(p.Region[0],p.Region[1],p.Region[2]);
                object region=p.RegionType switch {
                    1 when p.RegionValues==3=>new SmoParticlePointRegion(position),
                    2 when p.RegionValues==6=>new SmoParticleBoxRegion(position,new(p.Region[3],p.Region[4],p.Region[5])),
                    3 when p.RegionValues==4=>new SmoParticleSphereRegion(position,p.Region[3]),
                    4 when p.RegionValues==8=>new SmoParticlePlaneRegion(position,new(p.Region[3],p.Region[4],p.Region[5]),p.Region[6],p.Region[7]),
                    5 when p.RegionValues==4=>new SmoParticleDiskRegion(position,p.Region[3]),
                    6 when p.RegionValues==5=>new SmoParticleCylinderRegion(position,p.Region[4],p.Region[3]),
                    7 when p.RegionValues==6=>new SmoParticleConeRegion(position,p.Region[4],p.Region[5],p.Region[3]),
                    _=>throw new InvalidDataException("Unknown loaded ParticleSystem region shape.") };
                int field=p.RegionType switch {1=>12,2=>14,3=>15,4=>13,5=>16,6=>17,7=>18,_=>throw new InvalidDataException("Unknown particle region.")};
                particles.Add(entry.Index,new(Renderable(entry),p.AccelerationBegin,p.AccelerationEnd,p.Direction,
                    new(p.Velocity.X,p.Velocity.Y),new(p.Angle.X,p.Angle.Y),new(p.Scale.X,p.Scale.Y),(p.ColorBegin,p.ColorEnd),new(p.Times.X,p.Times.Y),
                    checked((byte)p.Loop),checked((byte)p.WorldSpace),checked((byte)p.Iterative),p.Rate,p.Sphere.W,p.RegionType,field,region,p.RenderNode==0?null:Entry(p.RenderNode),
                    Array.AsReadOnly(new[]{p.Pool[0],p.Pool[1],p.Pool[2],p.Pool[3],p.Pool[4]})){CpuPool=poolSnapshot});
            }
            Particles=new ReadOnlyDictionary<int,SmoParticleSystemData>(particles);

            foreach (var entry in document.Objects.Where(entry => entry.TypeHash is SmoClassIds.Model or SmoClassIds.Skin))
            {
                NativeMethods.Check(NativeMethods.spv_graph_model(graph, entry.Id, out var model));
                var material = materialView.Material(model.Material);
                NativeMethods.Check(NativeMethods.spv_graph_alpha_info(graph,entry.Id,out var alpha));
                models.Add(entry.Index, new(entry.Index, model.Mesh, model.Mesh == 0 ? null : Entry(model.Mesh).Index,
                    model.Material, material.Value, model.Fog, model.Alpha != 0, model.Priority, model.Projection, material.Issue)
                    {AlphaSortData=new(alpha.Queued!=0,alpha.Priority,alpha.Particle!=0,alpha.Sphere),FogDraw=Fog(model.Fog)});
            }
            try
            {
                using var scene = new SceneHandle(NativeMethods.Check(NativeMethods.spv_graph_scene_all(graph)));
                NativeMethods.Check(NativeMethods.spv_scene_node_count(scene, out uint count));
                var ids = new uint[checked((int)count)];
                var worlds = new Matrix4x4[ids.Length];
                fixed (uint* output = ids) NativeMethods.Check(NativeMethods.spv_scene_graph_node_ids(scene, output, count));
                fixed (Matrix4x4* output = worlds) NativeMethods.Check(NativeMethods.spv_scene_sample(scene, 0, output, count * 16));
                var nodes = new Dictionary<int, SmoLoadedNode>();
                for (int i = 0; i < ids.Length; ++i)
                {
                    NativeMethods.Check(NativeMethods.spv_graph_node(graph, ids[i], out var node));
                    var entry = Entry(ids[i]);
                    nodes.Add(entry.Index, new(ids[i], entry.Index, node.ParentId == 0 ? null : Entry(node.ParentId).Index, worlds[i]));
                }
                Nodes = new ReadOnlyDictionary<int, SmoLoadedNode>(nodes);
                NodeWorlds = new ReadOnlyDictionary<int, Matrix4x4>(nodes.ToDictionary(item => item.Key, item => item.Value.World));
                foreach (var entry in document.Objects.Where(entry => entry.TypeHash == SmoClassIds.Skin))
                {
                    try
                    {
                        NativeMethods.Check(NativeMethods.spv_scene_graph_skin_info(scene, entry.Id, out uint weights, out uint bones));
                        var matrices = new Matrix4x4[checked((int)bones)];
                        fixed (Matrix4x4* output = matrices)
                            NativeMethods.Check(NativeMethods.spv_scene_graph_skin_palette(scene, entry.Id, output, bones * 16));
                        models[entry.Index] = models[entry.Index] with { SkinWeightCount = weights, InitialSkinPalette = Array.AsReadOnly(matrices) };
                    }
                    catch (InvalidDataException exception)
                    { models[entry.Index] = models[entry.Index] with { SkinIssue = $"SPARKPLUG_SKIN_RUNTIME: [{entry.Index}] {exception.Message}" }; }
                }
            }
            catch (InvalidDataException exception) { SceneIssue = $"SPARKPLUG_NODE_RUNTIME: {exception.Message}"; }

            NativeMethods.Check(NativeMethods.spv_graph_render_containers(graph, null, 0, out uint containersCount));
            var nativeContainers = new NativeMethods.GraphRenderContainer[checked((int)containersCount)];
            fixed (NativeMethods.GraphRenderContainer* output = nativeContainers)
                NativeMethods.Check(NativeMethods.spv_graph_render_containers(graph, output, containersCount, out _));
            var containers = new List<SmoLoadedRenderContainer>(nativeContainers.Length);
            var occurrences = new List<SmoLoadedRenderOccurrence>();
            foreach (var container in nativeContainers)
            {
                var ids = new uint[checked((int)container.Renderables)];
                fixed (uint* output = ids)
                    NativeMethods.Check(NativeMethods.spv_graph_render_members(graph, container.Id, output, container.Renderables));
                bool worldAvailable = container.Kind is not (0 or 3) || SceneIssue is null;
                containers.Add(new(container.Id, Entry(container.Id).Index, (SmoRenderContainerKind)container.Kind,
                    worldAvailable ? container.World : null, worldAvailable ? container.Inverse : null,
                    Array.AsReadOnly(ids.Select(id => Entry(id).Index).ToArray()))
                    {SkyPose=container.Kind==3?SmoSkyBoxPose.Read(graph,container.Id):null});
                for (int slot = 0; slot < ids.Length; ++slot)
                {
                    var key = new SmoRenderOccurrenceKey(Entry(container.Id).Index, slot);
                    int renderableIndex = Entry(ids[slot]).Index;
                    try
                    {
                        if (!worldAvailable) throw new InvalidDataException(SceneIssue);
                        NativeMethods.Check(NativeMethods.spv_graph_render_occurrence(graph, container.Id, (uint)slot, out var occurrence));
                        if (occurrence.Renderable != ids[slot]) throw new InvalidDataException("Loaded occurrence/member identity differs.");
                        occurrences.Add(new(key, renderableIndex, occurrence.World,
                            occurrence.RigidNode == 0 ? null : Entry(occurrence.RigidNode).Index, null));
                    }
                    catch (InvalidDataException exception)
                    {
                        occurrences.Add(new(key, renderableIndex, null, null,
                            $"RENDER_OCCURRENCE: container [{key.ContainerObjectIndex}] slot {slot}, renderable [{renderableIndex}]: {exception.Message}"));
                    }
                }
            }
            RenderContainers = containers.AsReadOnly();
            RenderOccurrences = occurrences.AsReadOnly();
            RenderContainersByObjectIndex = new ReadOnlyDictionary<int, SmoLoadedRenderContainer>(containers.ToDictionary(container => container.ObjectIndex));
            RenderContainersByRenderable = new ReadOnlyDictionary<int, IReadOnlyList<SmoLoadedRenderContainer>>(
                containers.SelectMany(container => container.RenderableObjectIndices.Select(index => (index, container)))
                    .GroupBy(item => item.index).ToDictionary(group => group.Key,
                        group => (IReadOnlyList<SmoLoadedRenderContainer>)Array.AsReadOnly(group.Select(item => item.container).Distinct().ToArray())));
            // This is a preview draw schedule, after all read-only snapshots.
            // The temporary graph may mutate; original file bytes/DTOs do not.
            using var materialRuntime = new SparkplugMaterialRuntime(document, graph, materialView);
            foreach(var entry in texts.ToArray())
            {
                try {texts[entry.Key]=entry.Value with{Geometry=SmoTextGeometry.Read(graph,entry.Value.ObjectId,materialView)};}
                catch(InvalidDataException exception){texts[entry.Key]=entry.Value with{GeometryIssue=$"TEXT_GEOMETRY: [{entry.Key}]: {exception.Message}"};}
            }
            foreach (var material in models.Values.Select(model => model.Material)
                         .OfType<SmoLoadedMaterial>().DistinctBy(material => material.ObjectIndex))
            {
                try { draws.Add(material.ObjectIndex, materialRuntime.CaptureDraw(material.ObjectIndex, 1)); }
                catch (InvalidDataException exception)
                { drawIssues.Add($"MATERIAL_DRAW_UNAVAILABLE: [{material.ObjectIndex}]: {exception.Message}"); }
            }
        }
        catch (InvalidDataException exception)
        {
            models.Clear();
            LoadIssue = $"SPARKPLUG_RESOURCE_GRAPH: {exception.Message}";
        }
    }
}
