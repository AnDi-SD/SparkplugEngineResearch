using System.Numerics;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Host projection shared by immutable document snapshots and live
/// scene material views. It never parses fields or evaluates controllers.</summary>
internal sealed unsafe class SparkplugMaterialView
{
    private readonly GraphHandle graph;
    private readonly IReadOnlyDictionary<uint, SmoObjectEntry> entries;
    private readonly Dictionary<uint, SmoLoadedTexture> textures = [];
    private readonly Dictionary<uint, SmoLoadedTextureAnimation> animations = [];
    private readonly Dictionary<uint, (SmoLoadedMaterial? Value, string? Issue)> materials = [];
    private readonly HashSet<uint> legacyTextures;
    internal SparkplugMaterialView(SmoDocument document, GraphHandle graph,IEnumerable<uint>? legacyTextureIds=null)
    {
        this.graph = graph;
        entries = document.Objects.ToDictionary(entry => entry.Id);
        legacyTextures=(legacyTextureIds??SparkplugGraphCompatibility.ReadTextureIds(graph)).ToHashSet();
    }
    private SmoObjectEntry Entry(uint id) => entries.TryGetValue(id, out var value) ? value
        : throw new InvalidDataException($"Loaded resource ID {id} is absent from this document's FAT.");

    internal SmoLoadedTexture? Texture(uint id)
    {
        if (id == 0) return null;
        if (textures.TryGetValue(id, out var cached)) return cached;
        var entry = Entry(id);
        SmoLoadedTexture value;
        try
        {
            NativeMethods.Check(NativeMethods.spv_graph_texture(graph, id, out var info));
            if(info.Mips is <1 or >16)throw new InvalidDataException("Runtime texture mip count exceeds the host bound.");
            var mips=new SmoTextureMip[checked((int)info.Mips)];byte[]? pixels=null;long totalBytes=0;
            for(uint level=0;level<info.Mips;++level)
            {
                NativeMethods.Check(NativeMethods.spv_graph_texture_mip_info(graph,id,level,out var mip));
                totalBytes+=mip.Bytes;
                if(mip.Bytes>16u*1024u*1024u||totalBytes>32L*1024*1024||mip.Bytes!=(ulong)mip.Width*mip.Height*4)
                    throw new InvalidDataException("Runtime texture pixels exceed the bounded mip chain.");
                var bytes=new byte[checked((int)mip.Bytes)];
                fixed(byte* output=bytes)NativeMethods.Check(NativeMethods.spv_graph_texture_mip_bgra(graph,id,level,output,mip.Bytes));
                if(level==0)pixels=bytes;
                mips[level]=new(checked((int)mip.Width),checked((int)mip.Height),bytes);
            }
            value = new(id, entry.Index, new SmoTexture(entry.Index, entry.Name, 0,
                checked((int)info.Width), checked((int)info.Height), SmoTextureLayout.Bgra, pixels!,
                SmoTextureRepresentationKind.RuntimeBgra32, checked((int)info.Mips), null,mips), null);
        }
        catch (InvalidDataException exception)
        { value = new(id, entry.Index, null, $"RUNTIME_TEXTURE_UPLOAD: [{entry.Index}] {entry.Name}: {exception.Message}"); }
        value=value with {UsesHostCompatibility=legacyTextures.Contains(id)};
        textures.Add(id, value);
        return value;
    }

    SmoLoadedTextureAnimation? Animation(uint id)
    {
        if (id == 0) return null;
        if (animations.TryGetValue(id, out var cached)) return cached;
        NativeMethods.Check(NativeMethods.spv_graph_texture_track(graph, id, out uint count, out float duration));
        var native = new NativeMethods.GraphTextureKey[checked((int)count)];
        fixed (NativeMethods.GraphTextureKey* output = native)
            NativeMethods.Check(NativeMethods.spv_graph_texture_keys(graph, id, output, count));
        var keys = native.Select(key => new SmoLoadedTextureKey(key.Time, Texture(key.Texture))).ToArray();
        var value = new SmoLoadedTextureAnimation(id, Entry(id).Index, duration, Array.AsReadOnly(keys));
        animations.Add(id, value);
        return value;
    }

    internal (SmoLoadedMaterial? Value, string? Issue) Material(uint id, bool refresh = false)
    {
        if (id == 0) return (null, null);
        if (!refresh && materials.TryGetValue(id, out var cached)) return cached;
        (SmoLoadedMaterial? Value, string? Issue) value;
        try
        {
            NativeMethods.Check(NativeMethods.spv_graph_material(graph, id, out var info));
            var passes = new List<SmoLoadedMaterialPass>();
            for (uint p = 0; p < info.Passes; ++p)
            {
                NativeMethods.Check(NativeMethods.spv_graph_pass(graph, id, p, out var pass));
                var layers = new List<SmoLoadedMaterialLayer>();
                for (uint l = 0; l < pass.Layers; ++l)
                {
                    NativeMethods.Check(NativeMethods.spv_graph_layer(graph, id, p, l, out var layer));
                    layers.Add(new(layer.ClassId, Texture(layer.Texture), Animation(layer.Animation),
                        layer.UvController, layer.UvEnabled != 0,
                        Array.AsReadOnly(new ReadOnlySpan<float>(layer.Uv, 9).ToArray()),
                        Array.AsReadOnly(new ReadOnlySpan<uint>(layer.States, 12).ToArray()),
                        layer.AnimationBoundHere != 0, layer.UvBoundHere != 0));
                }
                passes.Add(new(pass.Blend, layers.AsReadOnly()));
            }
            var colors = new Vector4[4];
            for (int i = 0; i < colors.Length; ++i)
                colors[i] = new(info.Colors[i * 4], info.Colors[i * 4 + 1], info.Colors[i * 4 + 2], info.Colors[i * 4 + 3]);
            value = (new(id, Entry(id).Index,
                Array.AsReadOnly(new ReadOnlySpan<uint>(info.States, 11).ToArray()), checked((byte)info.VertexAlpha),
                Array.AsReadOnly(colors), info.PowerInitialized == 0 ? null : info.Power,
                info.ColorController, passes.AsReadOnly()), null);
        }
        catch (InvalidDataException exception)
        { value = (null, $"RUNTIME_MATERIAL_VIEW: ID {id}: {exception.Message}"); }
        materials[id] = value;
        return value;
    }

}
