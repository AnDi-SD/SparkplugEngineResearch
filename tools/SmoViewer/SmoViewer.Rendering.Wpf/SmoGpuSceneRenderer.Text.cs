using System.Numerics;
using System.Windows.Media;
using SmoViewer.Core;
using SmoViewer.Scene;

namespace SmoViewer.Rendering.Wpf;

public sealed partial class SmoGpuSceneRenderer
{
    /// <summary>Upload actual generated Text through the shared graphics path.
    /// The transient buffer is local to this backend and never enters editing/export.</summary>
    public void AddText(SmoSceneText text,SmoRenderObjectKey key)
    {
        var geometry=text.Text.Geometry??throw new ArgumentException("Text geometry is unavailable.",nameof(text));
        if(geometry.Vertices.Count==0)return;
        var vertices=geometry.Vertices;
        var mesh=new SmoMesh(text.Text.ObjectIndex,"Generated Text",0,
            SmoMeshDecoder.TriangleListPrimitive,0x142,24,24,0,checked((uint)vertices.Count*24),-1,-1,-1,-1,
            vertices.Select(v=>v.Position).ToArray(),[],vertices.Select(v=>v.UV).ToArray(),[],
            vertices.Select(v=>v.Color).ToArray(),[],[],geometry.Indices.ToArray(),geometry.Indices.Select(i=>(uint)i).ToArray());
        var renderMesh=new SmoSceneMesh(mesh,text.Text.ObjectIndex,null,null,null,null,null,null,true,null,null,null,
            text.WorldTransform,null,null,text.RigidNodeObjectIndex,new Dictionary<int,float>())
        {
            OccurrenceKey=text.OccurrenceKey,RenderableObjectIndex=text.Text.ObjectIndex,
            MaterialDraw=geometry.MaterialDraw,LoadedMaterial=geometry.Material,AlphaSortData=text.Text.AlphaSortData,
            FogDraw=text.Text.FogDraw,
            SourcePriority=text.Text.Renderable.Priority,AlphaSupportWorld=text.WorldTransform,
            ContainerKind=text.ContainerKind,HasOriginalAlphaSphere=true,SkyPose=text.SkyPose
        };
        Add(renderMesh,key,Colors.White);
        _items[^1].TextRigidNode=text.RigidNodeObjectIndex;
        _items[^1].TextObjectIndex=text.Text.ObjectIndex;
    }

    private void UpdateTextWorlds()
    {
        foreach(var item in _items)
            if(item.TextRigidNode is int node&&_lightingRuntimes.TryGetValue(item.Key.FileIndex,out var runtime)&&
                runtime.Worlds.TryGetValue(node,out var world))
            {item.Model=world*Matrix4x4.CreateScale(1,1,-1);item.AlphaSupportWorld=world;}
    }
}
