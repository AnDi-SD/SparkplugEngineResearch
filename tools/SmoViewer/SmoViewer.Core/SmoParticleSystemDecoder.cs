using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

public sealed record SmoSingleRange(float Begin,float End);
public sealed record SmoParticlePointRegion(Vector3 Position);
public sealed record SmoParticlePlaneRegion(
    Vector3 Position,Vector3 Normal,float SizeX,float SizeY);
public sealed record SmoParticleBoxRegion(Vector3 Position,Vector3 Size);
public sealed record SmoParticleSphereRegion(Vector3 Position,float Radius);
public sealed record SmoParticleDiskRegion(Vector3 Position,float Radius);
public sealed record SmoParticleCylinderRegion(Vector3 Position,float Radius,float Height);
public sealed record SmoParticleConeRegion(
    Vector3 Position,float Radius1,float Radius2,float Height);
/// <summary>Initial CPU record. Unwritten original allocator bytes are exposed as zero with Written=false.</summary>
public sealed record SmoParticleRecord(Vector3 Position,Vector3 Velocity,float BirthTime,float Lifetime,
    bool Written,uint Previous,uint Next);
public sealed record SmoParticlePoolSnapshot(uint First,uint Boundary,IReadOnlyList<SmoParticleRecord> Records);

/// <summary>Actual loaded ParticleSystem state, including constructor defaults and raw mode bytes.</summary>
public sealed record SmoParticleSystemData(
    SmoLoadedRenderableData Renderable,Vector3 AccelerationBegin,Vector3 AccelerationEnd,
    Vector3 EmissionDirection,SmoSingleRange Velocity,SmoSingleRange Angle,SmoSingleRange Scale,
    (uint Begin,uint End) Color,SmoSingleRange Time,byte LoopAnimation,byte WorldSpace,byte IterativeMode,
    float EmissionRate,float BoundingSphere,uint RegionType,int RegionFieldType,object Region,SmoObjectEntry? RenderNode,
    IReadOnlyList<uint> InitialPool)
{
    public SmoParticlePoolSnapshot? CpuPool { get; init; }
}

/// <summary>Projection of the shared loaded class and CPU initialization; frame simulation is not included.</summary>
public static class SmoParticleSystemDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoParticleSystemData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        if(entry.TypeHash!=SmoClassIds.ParticleSystem){error="Object is not spParticleSystem.";return false;}
        var loaded=SmoLoadedResources.Get(document);
        if(loaded.LoadIssue is not null){error=loaded.LoadIssue;return false;}
        if(!loaded.Particles.TryGetValue(entry.Index,out value)){error="Actual ParticleSystem is absent from the loaded graph.";return false;}
        return true;
    }
}
