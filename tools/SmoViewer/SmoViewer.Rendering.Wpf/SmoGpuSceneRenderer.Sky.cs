using System.IO;
using System.Numerics;
using OpenTK.Graphics.OpenGL4;
namespace SmoViewer.Rendering.Wpf;
public sealed partial class SmoGpuSceneRenderer
{
    private readonly List<string> _skyIssues=[];
    private readonly List<SmoAlphaDrawOrder> _lastSkyAlphaOrder=[];
    public IReadOnlyList<SmoAlphaDrawOrder> LastSkyAlphaOrder=>_lastSkyAlphaOrder;
    public int LastSkyPlacementCount {get;private set;}
    public bool ShowSky {get;set;}=true;
    private void DrawSky(Matrix4x4 turntable,Matrix4x4 view)
    {
        _skyIssues.Clear();_lastSkyAlphaOrder.Clear();LastSkyPlacementCount=0;
        if(!_items.Any(item=>item.IsSky))return;
        var groups=_items.Where(item=>item.IsSky).GroupBy(item=>(item.Key.FileIndex,item.Key.OccurrenceKey?.ContainerObjectIndex));
        // Inverse of game-world -> LH camera view supplies the actual modern
        // camera parent. Turntable participates in the coordinate conversion.
        bool cameraValid=Matrix4x4.Invert(_gameWorldToShaderView,out var cameraWorld);
        foreach(var group in groups)
        {
            foreach(var item in group)item.SkyDrawEnabled=false;
            if(!ShowSky)continue;
            try
            {
                if(!cameraValid)throw new InvalidDataException("Camera view is not invertible.");
                var pose=group.First().SkyPose;
                if(group.Key.ContainerObjectIndex is int index&&_materialRuntimes.TryGetValue(group.Key.FileIndex,out var runtime))
                    pose=runtime.Runtime.ReadSkyPose(index);
                else if(group.Any(item=>item.SkyWorldEdited))
                    throw new InvalidDataException("SKY_EDIT_POSE: world-only editor change needs a refreshed original local SkyBox pose.");
                if(pose is null)throw new InvalidDataException("Missing original local/cached SkyBox pose.");
                var world=pose.CameraWorld(cameraWorld);
                foreach(var item in group)
                {
                    item.Model=world*Matrix4x4.CreateScale(1,1,-1);item.AlphaSupportWorld=world;
                    item.SkyDrawEnabled=pose.Enabled;
                    if(pose.Enabled&&_appearances.TryGetValue(item.Key,out var a)&&a.Visible&&a.Opacity>0)++LastSkyPlacementCount;
                }
            }
            catch(Exception error) when(error is InvalidDataException or ArgumentOutOfRangeException or ObjectDisposedException)
            {_skyIssues.Add("SKY_WORLD_UNAVAILABLE: "+error.Message);}
        }
        // PC Scene45EC70 runs the sky pass and its alpha flush before ordinary
        // supports. Materials retain their own depth/blend states; no fabricated
        // far-plane geometry or unconditional depth clearing is introduced.
        foreach(var item in _items.Where(item=>item.IsSky&&!UsesTransparentPass(item)))Draw(item,turntable);
        DrawTransparent(turntable,view,sky:true);
        GL.DepthMask(true);GL.Disable(EnableCap.Blend);
    }
}
