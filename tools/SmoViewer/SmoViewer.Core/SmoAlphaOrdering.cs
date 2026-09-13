using System.Numerics;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Original loaded renderable fields and the PC material/pass alpha gate.</summary>
public sealed record SmoAlphaSortData(bool RequiresQueue,uint Priority,bool ExactParticle,Vector4 Sphere);
public readonly record struct SmoAlphaSortInput(uint Token,uint Priority,bool ExactParticle,Vector3 Center,Matrix4x4 SupportWorld);
public readonly record struct SmoAlphaSortResult(uint Token,uint Priority,bool ExactParticle,float DistanceSquared);

/// <summary>Reusable modern host batch. All metrics, comparison and CRT ordering
/// execute in common native code. This is not the original queue/frame scheduler.</summary>
public sealed class SmoAlphaOrdering
{
    private NativeMethods.AlphaInput[] _input=[];
    private NativeMethods.AlphaOutput[] _output=[];
    private SmoAlphaSortResult[] _results=[];

    /// <summary>The returned span is valid until this instance's next call.
    /// The host explicitly chooses camera byte231 behavior and priority bias.</summary>
    public unsafe ReadOnlySpan<SmoAlphaSortResult> Order(IReadOnlyList<SmoAlphaSortInput> entries,
        Matrix4x4 view,bool depthOnly=false,uint priorityBase=0)
    {
        ArgumentNullException.ThrowIfNull(entries);
        if(entries.Count>65536)throw new ArgumentOutOfRangeException(nameof(entries),"Host alpha batch exceeds 65536 entries.");
        if(_input.Length<entries.Count)
        {
            int capacity=Math.Min(65536,Math.Max(entries.Count,Math.Max(16,_input.Length*2)));
            _input=new NativeMethods.AlphaInput[capacity];_output=new NativeMethods.AlphaOutput[capacity];_results=new SmoAlphaSortResult[capacity];
        }
        for(int i=0;i<entries.Count;++i)
        {
            var item=entries[i];_input[i]=new(){Token=item.Token,Priority=item.Priority,Particle=item.ExactParticle?1u:0u,
                Center=item.Center,World=item.SupportWorld};
        }
        fixed(NativeMethods.AlphaInput* input=_input)
        fixed(NativeMethods.AlphaOutput* output=_output)
            NativeMethods.Check(NativeMethods.spv_alpha_order(input,(uint)entries.Count,(float*)&view,depthOnly?1u:0u,priorityBase,output));
        for(int i=0;i<entries.Count;++i)
        {var item=_output[i];_results[i]=new(item.Token,item.Priority,item.Particle!=0,item.DistanceSquared);}
        return _results.AsSpan(0,entries.Count);
    }
}
