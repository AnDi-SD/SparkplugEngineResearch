using System.Diagnostics;
using System.IO;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Scene;
using Float4=System.Numerics.Vector4;

// Declared alpha metadata fixtures on actual decoded geometry. They test the
// production renderer's grouping, support matrices, camera conversion and
// native-order consumption; they are not original whole-game pixel captures.
internal static class GpuAlphaRegression
{
    internal static int Run(string source,string output)
    {
        if(File.Exists(output))throw new IOException("Fresh alpha GPU report required.");
        int checks=0;string failure="",device="";var timer=Stopwatch.StartNew();var observations=new List<object>();
        void Check(bool ok,string message){++checks;if(!ok)throw new InvalidDataException(message);}
        try
        {
            var scene=SmoSceneBuilder.Build(SmoDocument.Load(source));var template=scene.Meshes.First(mesh=>mesh.MaterialDraw is not null);
            Check(template.Mesh.PhysicalOffset>=0,"Fixture uses original decoded geometry rather than a transient mesh");
            using var window=new NativeWindow(new NativeWindowSettings{ClientSize=new Vector2i(16,16),StartVisible=false,StartFocused=false,
                API=ContextAPI.OpenGL,APIVersion=new Version(3,3),Profile=ContextProfile.Core,Flags=ContextFlags.ForwardCompatible,AutoLoadBindings=true});
            window.MakeCurrent();device=GL.GetString(StringName.Renderer);
            var renderer=new SmoGpuSceneRenderer();renderer.SetAntialiasingSamples(0);
            var keys=new[]{new SmoRenderObjectKey(0,template.SceneObjectIndex,new(500,0)),
                new SmoRenderObjectKey(0,template.SceneObjectIndex,new(500,1)),new SmoRenderObjectKey(0,template.SceneObjectIndex,new(600,0))};
            SmoSceneMesh Item(int i,Float4 sphere,uint priority,float z)=>template with
            {OccurrenceKey=keys[i].OccurrenceKey,WorldTransform=Matrix4x4.Identity,SkinObjectIndex=null,InitialSkinMatrices=null,
                RigidNodeObjectIndex=null,AlphaSortData=new(true,priority,false,sphere),AlphaSupportWorld=Matrix4x4.CreateTranslation(0,0,z),SourcePriority=0};
            renderer.Add(Item(0,new(10,0,0,1),1,8),keys[0],Colors.White);
            renderer.Add(Item(1,new(0,0,0,1),2,0),keys[1],Colors.White);
            renderer.Add(Item(2,new(0,0,0,1),1,9),keys[2],Colors.White);
            var camera=new OrthographicCamera(new Point3D(0,0,10),new Vector3D(0,0,-1),new Vector3D(0,1,0),10)
                {NearPlaneDistance=.1,FarPlaneDistance=1000};
            void Expect(string name,int[] expected,float[] distances)
            {
                renderer.Render(16,16,camera,Colors.Black,0,new Point3D(),false,Colors.Gray,0,10,10);
                Check(renderer.RenderIssues.Count==0,name+": "+string.Join("; ",renderer.RenderIssues));
                Check(renderer.LastAlphaOrder.Select(row=>row.Key).SequenceEqual(expected.Select(i=>keys[i])),name+" actual placement order");
                Check(renderer.LastAlphaOrder.All(row=>row.OriginalSphere),name+" supplied sphere branch, not preview geometry centroid");
                Check(renderer.LastAlphaOrder.Select(row=>row.DistanceSquared).Zip(distances).All(pair=>Math.Abs(pair.First-pair.Second)<1e-4f),name+" actual support/view metric");
                observations.Add(new{name,order=renderer.LastAlphaOrder.ToArray()});
            }
            Expect("priority then native sphere",[1,0,2],[100,424,361]);
            Check(renderer.ConsumeUploadReport() is {GeometryCount:1,PlacementCount:3},"Three occurrence slots retain one shared geometry upload");
            renderer.DepthOnlyAlphaMetric=true;Expect("explicit byte231",[1,2,0],[100,361,324]);
            renderer.DepthOnlyAlphaMetric=false;renderer.SetAppearance(keys[1],false,false,1);
            Expect("hidden occurrence omitted",[0,2],[424,361]);
            renderer.SetModelTransform(keys[0],Matrix4x4.Identity);
            Expect("editor support transform",[2,0],[361,200]);
            renderer.Clear();Check(renderer.LastAlphaOrder.Count==0&&renderer.AlphaIssue is null,"Clear removes ordering and diagnostics");
            renderer.Render(16,16,camera,Colors.Black,0,new Point3D(),false,Colors.Gray,0,10,10);
            Check(GL.GetError()==ErrorCode.NoError,"GL renderer alpha slice completed");
            Console.WriteLine($"PASS GPU alpha ordering: {checks} checks on {device}");return 0;
        }
        catch(Exception error){failure=error.ToString();throw;}
        finally{Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);File.WriteAllText(output,
            JsonSerializer.Serialize(new{source,checks,failure,device,seconds=timer.Elapsed.TotalSeconds,peakBytes=Process.GetCurrentProcess().PeakWorkingSet64,
                native=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")))),
                renderer=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(typeof(SmoGpuSceneRenderer).Assembly.Location))),observations},new JsonSerializerOptions{WriteIndented=true}));}
    }
}
