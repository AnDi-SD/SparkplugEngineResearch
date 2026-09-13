using System.Numerics;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Mapped graphics states produced by the original PC material routines.
/// Values retain the device enum numbers; KnownMask distinguishes untouched inputs.</summary>
public sealed record SmoMaterialRasterState(uint KnownMask, uint DepthEnable,
    uint FillMode, uint ShadeMode, uint DepthWrite, uint AlphaTest,
    uint SourceBlend, uint DestinationBlend, uint CullMode, uint DepthFunction,
    uint AlphaReference, uint AlphaFunction, uint BlendEnable, uint SpecularEnable,
    uint LightingEnable, uint DiffuseSource, uint AmbientSource);

public sealed record SmoMaterialTextureStage(SmoLoadedTexture? Texture, uint KnownMask,
    uint ColorOperation, uint AlphaOperation, uint AddressU, uint AddressV,
    uint BorderColor, uint Magnification, uint Minification, uint MipFilter,
    uint Coordinates, uint TransformFlags, Matrix4x4? UVTransform);

public sealed record SmoMaterialDrawPass(uint Ordinal, byte VertexAlpha,
    SmoMaterialRasterState Raster, Vector4 Diffuse, Vector4 Ambient,
    Vector4 Specular, Vector4 Emissive, float? SpecularPower,
    IReadOnlyList<SmoMaterialTextureStage> Stages);

/// <summary>Host draw-order snapshot, not a reconstructed visibility/frame scheduler.</summary>
public sealed record SmoMaterialDraw(int MaterialObjectIndex, uint Frame,
    IReadOnlyList<SmoMaterialDrawPass> Passes);

public sealed partial class SparkplugMaterialRuntime
{
    private MaterialSubmissionHandle? _submission;
    private bool _disposed;
    private readonly NativeMethods.MaterialDrawPass[] _drawBuffer = new NativeMethods.MaterialDrawPass[8];
    private readonly Dictionary<int,(TextViewHandle Handle,NativeMethods.TextViewInfo Info)> _textViews=[];

    /// <summary>Text binds its actual Font atlas before each draw. This must
    /// precede the shared pass/controller evaluation even for a reused material.</summary>
    public unsafe SmoMaterialDraw CaptureTextDraw(int textObjectIndex,uint frame)
    {
        uint id=Id(textObjectIndex);
        if(!_textViews.TryGetValue(textObjectIndex,out var view))
        {
            var handle=new TextViewHandle(NativeMethods.Check(NativeMethods.spv_text_view_create(_graph,id)));
            try {
                NativeMethods.Check(NativeMethods.spv_text_view_info(handle,out var info));
                if(info.Passes>8||info.PowerAssigned>1)throw new InvalidDataException("Invalid Text material output.");
                view=(handle,info);_textViews.Add(textObjectIndex,view);
            }catch{handle.Dispose();throw;}
        }
        fixed(NativeMethods.MaterialDrawPass* output=_drawBuffer)
            NativeMethods.Check(NativeMethods.spv_text_view_capture(view.Handle,frame,output,view.Info.Passes));
        int materialIndex=view.Info.Material==0?-1:_indices[view.Info.Material];
        return ProjectDraw(_drawBuffer,view.Info.Passes,_view,materialIndex,frame,view.Info.PowerAssigned!=0);
    }

    /// <summary>Apply the actual material/pass routines in draw order. The
    /// renderer cache is retained across calls. Native graph mutations remain
    /// if an original update fails; no partially filled snapshot is published.</summary>
    public unsafe SmoMaterialDraw CaptureDraw(int materialObjectIndex, uint frame)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        uint id = Id(materialObjectIndex);
        _submission ??= new(NativeMethods.Check(NativeMethods.spv_material_submission_create(_graph)));
        uint count;
        fixed (NativeMethods.MaterialDrawPass* output = _drawBuffer)
            NativeMethods.Check(NativeMethods.spv_material_submission_capture(_submission, id, frame, output, 8, out count));
        return ProjectDraw(_drawBuffer,count,_view,materialObjectIndex,frame,true);
    }

    internal static unsafe SmoMaterialDraw ProjectDraw(NativeMethods.MaterialDrawPass[] rows,uint count,
        SparkplugMaterialView view,int materialObjectIndex,uint frame,bool powerAssigned)
    {
        var passes = new SmoMaterialDrawPass[checked((int)count)];
        for (int p = 0; p < passes.Length; ++p)
        {
            var row = rows[p];
            uint* r = row.Render;
            var raster = new SmoMaterialRasterState(row.KnownRender,
                r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
                r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
            var stages = new SmoMaterialTextureStage[8];
            for (int s = 0; s < stages.Length; ++s)
            {
                uint* t = row.Stages + s * 10;
                float* m = row.UV + s * 16;
                Matrix4x4? matrix = (row.KnownUV & (1u << s)) == 0 ? null : new Matrix4x4(
                    m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                    m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
                stages[s] = new(view.Texture(row.Textures[s]), row.KnownStages[s],
                    t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9], matrix);
            }
            float* c = row.Colors;
            passes[p] = new(row.Pass, checked((byte)row.VertexAlpha), raster,
                new(c[0], c[1], c[2], c[3]), new(c[4], c[5], c[6], c[7]),
                new(c[8], c[9], c[10], c[11]), new(c[12], c[13], c[14], c[15]),
                powerAssigned?c[16]:null, Array.AsReadOnly(stages));
        }
        return new(materialObjectIndex, frame, Array.AsReadOnly(passes));
    }

    public void Dispose()
    {
        foreach(var view in _textViews.Values)view.Handle.Dispose();
        _textViews.Clear();
        _submission?.Dispose();
        _disposed = true;
    }
}
