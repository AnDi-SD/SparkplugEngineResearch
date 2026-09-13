using System.Numerics;
using SmoViewer.Sparkplug;
namespace SmoViewer.Core;
/// <summary>Immutable projection of actual loaded Font/Text classes, independent of raw wire inspection.</summary>
public sealed record SmoLoadedFont(uint ObjectId,int ObjectIndex,uint Height,uint? Baseline,
    SmoLoadedTexture? Atlas,IReadOnlyList<SmoFontGlyph> Glyphs);
public sealed record SmoLoadedText(uint ObjectId,int ObjectIndex,SmoLoadedRenderableData Renderable,
    SmoLoadedFont? Font,bool TextPresent,ReadOnlyMemory<byte> TextBytes,uint Color,uint WrapWidth,
    uint Alignment,uint MeasuredWidth,Vector4 Sphere,Vector3? Minimum,Vector3? Maximum)
{
    public SmoTextGeometry? Geometry {get;init;}
    public string? GeometryIssue {get;init;}
    public SmoAlphaSortData? AlphaSortData {get;init;}
    public SmoFogDraw? FogDraw {get;init;}
}
public readonly record struct SmoTextVertex(Vector3 Position,uint Color,Vector2 UV);
/// <summary>Generated Text vertices, not a serialized Mesh resource.</summary>
public sealed record SmoTextGeometry(IReadOnlyList<SmoTextVertex> Vertices,
    IReadOnlyList<ushort> Indices,SmoLoadedTexture? Atlas,SmoMaterialDraw MaterialDraw,SmoLoadedMaterial? Material)
{
    internal static unsafe SmoTextGeometry Read(GraphHandle graph,uint text,SparkplugMaterialView materials)
    {
        using var handle=new TextViewHandle(NativeMethods.Check(NativeMethods.spv_text_view_create(graph,text)));
        NativeMethods.Check(NativeMethods.spv_text_view_info(handle,out var info));
        if(info.Vertices>16384||info.Indices>24576||info.Passes>8||info.PowerAssigned>1)
            throw new InvalidDataException("Invalid bounded Text geometry output.");
        var vertices=new NativeMethods.TextVertex[checked((int)info.Vertices)];
        var indices=new ushort[checked((int)info.Indices)];
        var draws=new NativeMethods.MaterialDrawPass[checked((int)info.Passes)];
        fixed(NativeMethods.TextVertex* output=vertices)NativeMethods.Check(NativeMethods.spv_text_view_vertices(handle,output,info.Vertices));
        fixed(ushort* output=indices)NativeMethods.Check(NativeMethods.spv_text_view_indices(handle,output,info.Indices));
        fixed(NativeMethods.MaterialDrawPass* output=draws)NativeMethods.Check(NativeMethods.spv_text_view_draws(handle,output,info.Passes));
        var material=materials.Material(info.Material,refresh:true);
        if(material.Issue is not null)throw new InvalidDataException(material.Issue);
        var atlas=materials.Texture(info.Atlas);
        if(info.Vertices>0&&atlas is null)throw new InvalidDataException("Missing nonempty Text atlas.");
        return new(Array.AsReadOnly(vertices.Select(value=>new SmoTextVertex(value.Position,value.Color,value.UV)).ToArray()),
            Array.AsReadOnly(indices),atlas,
            SparkplugMaterialRuntime.ProjectDraw(draws,info.Passes,materials,material.Value?.ObjectIndex??-1,1,info.PowerAssigned!=0),material.Value);
    }
}
