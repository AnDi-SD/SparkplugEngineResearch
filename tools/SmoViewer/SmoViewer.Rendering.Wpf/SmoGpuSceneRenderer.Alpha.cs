using System.IO;
using System.Numerics;
using SmoViewer.Core;

namespace SmoViewer.Rendering.Wpf;

public sealed record SmoAlphaDrawOrder(SmoRenderObjectKey Key,float DistanceSquared,uint Priority,bool ExactParticle,bool OriginalSphere);

public sealed partial class SmoGpuSceneRenderer
{
    private readonly SmoAlphaOrdering _alphaOrdering=new();
    private readonly List<SmoAlphaSortInput> _alphaInputs=[];
    private readonly List<SmoAlphaDrawOrder> _lastAlphaOrder=[];
    public IReadOnlyList<SmoAlphaDrawOrder> LastAlphaOrder=>_lastAlphaOrder;
    public string? AlphaIssue {get;private set;}
    public IReadOnlyCollection<string> RenderIssues=>_skyIssues.Count==0&&_fogIssues.Count==0&&AlphaIssue is null?MaterialIssues:
        MaterialIssues.Concat(_skyIssues).Concat(_fogIssues.Values).Concat(AlphaIssue is null?Array.Empty<string>():new[]{AlphaIssue}).ToArray();
    /// <summary>Explicit host policy for original camera byte231. It is not
    /// inferred from WPF OrthographicCamera or serialized game Is2D.</summary>
    public bool DepthOnlyAlphaMetric {get;set;}

    private void DrawTransparent(Matrix4x4 turntable,Matrix4x4 view,bool sky=false)
    {
        var reportedOrder=sky?_lastSkyAlphaOrder:_lastAlphaOrder;
        _alphaInputs.Clear();reportedOrder.Clear();if(!sky)AlphaIssue=null;
        try
        {
            var units=_items.Where(item=>item.IsSky==sky&&(!sky||item.SkyDrawEnabled)&&UsesTransparentPass(item)&&_geometry.ContainsKey(item.GeometryKey)&&
                    (!_appearances.TryGetValue(item.Key,out var appearance)||appearance.Visible&&appearance.Opacity>0))
                .GroupBy(item=>item.Key).Select(group=>group.ToArray()).ToArray();
            var originalSpheres=new bool[units.Length];
            for(int i=0;i<units.Length;++i)
            {
                var item=units[i][0];var source=item.AlphaSortData;
                Vector3 center;Matrix4x4 world;
                if(source is not null&&item.OriginalAlphaSphere)
                {
                    if(item.AlphaSupportWorld is not Matrix4x4 originalWorld)
                        throw new InvalidDataException($"Missing original alpha support matrix: {item.Key}.");
                    world=originalWorld;center=new(source.Sphere.X,source.Sphere.Y,source.Sphere.Z);originalSpheres[i]=true;
                    // Animated alpha uses the actual container world, independent
                    // from the Skin palette's render matrix.
                    if(!sky&&item.Key.OccurrenceKey is { } occurrence&&_lightingRuntimes.TryGetValue(item.Key.FileIndex,out var runtime)&&
                        runtime.Worlds.TryGetValue(occurrence.ContainerObjectIndex,out var animatedWorld))world=animatedWorld;
                }
                else
                {
                    // Host-created geometry has no current original sphere.
                    // Its preview AABB center stays explicitly host metadata.
                    center=_geometry[item.GeometryKey].Center;
                    world=item.Model*Matrix4x4.CreateScale(1,1,-1);
                }
                _alphaInputs.Add(new((uint)i,source?.Priority??item.SourcePriority,source?.ExactParticle??false,center,world));
            }
            var order=_alphaOrdering.Order(_alphaInputs,Matrix4x4.CreateScale(1,1,-1)*turntable*view,DepthOnlyAlphaMetric);
            foreach(var entry in order)
            {
                var unit=units[checked((int)entry.Token)];
                reportedOrder.Add(new(unit[0].Key,entry.DistanceSquared,entry.Priority,entry.ExactParticle,originalSpheres[entry.Token]));
                foreach(var item in unit)Draw(item,turntable);
            }
        }
        catch(Exception error) when(error is InvalidDataException or ArgumentOutOfRangeException or ObjectDisposedException)
        {if(sky)_skyIssues.Add("SKY_ALPHA_UNAVAILABLE: "+error.Message);else AlphaIssue="ALPHA_ORDER_UNAVAILABLE: "+error.Message;}
    }
}
