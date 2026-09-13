using System.Diagnostics;
using System.IO;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Windows.Media.Media3D;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using SmoViewer.Core;
using SmoViewer.Scene;
using SmoViewer.Sparkplug;
using SmoViewer.Rendering.Wpf;
using Colors=System.Windows.Media.Colors;
using Color=System.Windows.Media.Color;
internal static class GpuSkyRegression
{
    internal static int Run(string source,string output)
    {
        if(Directory.Exists(output))throw new IOException("Fresh sky output required.");Directory.CreateDirectory(output);
        var timer=Stopwatch.StartNew();int checks=0;string failure="",device="";object? detail=null;
        void Check(bool ok,string message){++checks;if(!ok)throw new InvalidDataException(message);}
        static string Hash(string path)=>Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
        try
        {
            var sourceHash=Hash(source);var document=SmoDocument.Load(source);var scene=SmoSceneBuilder.Build(document);
            var skies=scene.Meshes.Where(m=>m.ContainerKind==SmoRenderContainerKind.SkyBox).ToArray();
            Check(skies.Length>0&&skies.All(m=>m.SkyPose is not null&&m.MaterialDraw is not null),"Actual SkyBox has local pose and common material passes");
            Check(!scene.TextureIssues.Any(s=>s.StartsWith("SKY_PASS_PENDING")),"Prepared scene no longer reports the disconnected sky pass");
            const int width=128,height=128;
            using var window=new NativeWindow(new NativeWindowSettings{ClientSize=new Vector2i(width,height),StartVisible=false,StartFocused=false,
                API=ContextAPI.OpenGL,APIVersion=new Version(3,3),Profile=ContextProfile.Core,Flags=ContextFlags.ForwardCompatible,AutoLoadBindings=true});
            window.MakeCurrent();device=GL.GetString(StringName.Renderer);
            var renderer=new SmoGpuSceneRenderer();renderer.SetAntialiasingSamples(0);
            foreach(var mesh in skies)renderer.Add(mesh,new(0,mesh.SceneObjectIndex,mesh.OccurrenceKey),Colors.White);
            var camera=new PerspectiveCamera(new Point3D(),new Vector3D(0,0,-1),new Vector3D(0,1,0),60){NearPlaneDistance=.01,FarPlaneDistance=100000};
            byte[] Render()
            {
                renderer.Render(width,height,camera,Color.FromRgb(10,20,30),0,new Point3D(),false,Colors.Gray,0,100,100);
                GL.Finish();var pixels=new byte[width*height*4];GL.ReadPixels(0,0,width,height,PixelFormat.Rgba,PixelType.UnsignedByte,pixels);return pixels;
            }
            var baseline=Render();var upload=renderer.ConsumeUploadReport();
            Check(renderer.RenderIssues.Count==0,string.Join("; ",renderer.RenderIssues));
            Check(renderer.LastSkyPlacementCount==skies.Length,"All enabled sky placements use the separate pass");
            Check(renderer.LastAlphaOrder.Count==0,"Sky alpha is excluded from the ordinary queue");
            renderer.ShowSky=false;var empty=Render();int covered=baseline.Zip(empty).Where((p,i)=>i%4!=3).Count(p=>p.First!=p.Second);
            Check(covered>100,"Actual sky changes framebuffer components");
            Check(renderer.LastSkyPlacementCount==0&&Enumerable.Range(0,width*height).All(i=>empty[i*4]==10&&empty[i*4+1]==20&&empty[i*4+2]==30),"Disabled sky leaves only the clear color");
            renderer.ShowSky=true;Check(baseline.SequenceEqual(Render()),"Sky toggle restores the exact frame");
            var firstKey=new SmoRenderObjectKey(0,skies[0].SceneObjectIndex,skies[0].OccurrenceKey);
            renderer.SetModelTransform(firstKey,skies[0].WorldTransform*Matrix4x4.CreateTranslation(1,0,0));Render();
            Check(renderer.RenderIssues.Any(s=>s.Contains("SKY_EDIT_POSE")),"World-only editor change is explicitly rejected rather than silently discarded");
            renderer.SetModelTransform(firstKey,skies[0].WorldTransform);Render();
            Check(renderer.RenderIssues.Count==0,"Restoring authored placement clears the partial-editor diagnostic");
            camera.Position=new Point3D(16,8,-32);var translated=Render();
            int translationDifferences=baseline.Zip(translated).Count(p=>p.First!=p.Second);
            Check(translationDifferences<baseline.Length/100,"Camera translation leaves the surrounding sky image stable within raster precision");
            camera.Position=new Point3D();camera.LookDirection=new Vector3D(0,-.5,-1);var rotated=Render();
            int rotationDifferences=baseline.Zip(rotated).Count(p=>p.First!=p.Second);
            Check(rotationDifferences>100,"Camera rotation changes the sky view");
            using var runtime=new SparkplugSceneRuntime(document,scene.Skins);
            renderer.SetMaterialRuntime(0,runtime.Materials);Render();
            Check(renderer.RenderIssues.Count==0,"Live actual graph supplies sky pose and material controllers");
            foreach(var mesh in scene.Meshes.Except(skies))renderer.Add(mesh,new(0,mesh.SceneObjectIndex,mesh.OccurrenceKey),Colors.White);
            foreach(var text in scene.Texts)renderer.AddText(text,new(0,text.Text.ObjectIndex,text.OccurrenceKey));
            var mixed=Render();var alpha=renderer.LastAlphaOrder.ToArray();var skyAlpha=renderer.LastSkyAlphaOrder.ToArray();
            Check(renderer.LastSkyPlacementCount==skies.Length,"Mixed scene retains its separate sky pass");
            var skyKeys=skies.Select(m=>m.OccurrenceKey).ToHashSet();
            Check(alpha.All(a=>!skyKeys.Contains(a.Key.OccurrenceKey))&&skyAlpha.All(a=>skyKeys.Contains(a.Key.OccurrenceKey)),"Sky and ordinary alpha phases stay separate");
            Check(renderer.RenderIssues.Count==0,string.Join("; ",renderer.RenderIssues.Take(8)));
            Check(GL.GetError()==ErrorCode.NoError,"Sky and mixed passes have no GL errors");
            Check(sourceHash==Hash(source),"Source SMO remains unchanged");
            File.WriteAllBytes(Path.Combine(output,"sky.rgba"),baseline);File.WriteAllBytes(Path.Combine(output,"mixed.rgba"),mixed);
            void SavePng(byte[] pixels,string name)
            {
                var bgra=new byte[pixels.Length];
                for(int y=0;y<height;++y)for(int x=0;x<width;++x)
                {int a=(y*width+x)*4,b=((height-1-y)*width+x)*4;bgra[b]=pixels[a+2];bgra[b+1]=pixels[a+1];bgra[b+2]=pixels[a];bgra[b+3]=pixels[a+3];}
                var bitmap=System.Windows.Media.Imaging.BitmapSource.Create(width,height,96,96,System.Windows.Media.PixelFormats.Bgra32,null,bgra,width*4);
                var png=new System.Windows.Media.Imaging.PngBitmapEncoder();png.Frames.Add(System.Windows.Media.Imaging.BitmapFrame.Create(bitmap));
                using var stream=File.Create(Path.Combine(output,name));png.Save(stream);
            }
            SavePng(baseline,"sky.png");SavePng(mixed,"mixed.png");
            detail=new{sourceHash,skyMeshes=skies.Length,sceneMeshes=scene.Meshes.Count,covered,translationDifferences,rotationDifferences,
                ordinaryAlpha=alpha.Length,skyAlpha=skyAlpha.Length,upload,decodeIssues=scene.DecodeErrors,textureIssues=scene.TextureIssues,
                native=Hash(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")),renderer=Hash(typeof(SmoGpuSceneRenderer).Assembly.Location)};
            renderer.Clear();Render();Check(renderer.LastSkyPlacementCount==0&&renderer.LastSkyAlphaOrder.Count==0,"Clear releases sky resources and diagnostics");
            Console.WriteLine($"PASS GPU sky: {skies.Length} sky / {scene.Meshes.Count} scene meshes, {checks} checks");return 0;
        }
        catch(Exception error){failure=error.ToString();throw;}
        finally{File.WriteAllText(Path.Combine(output,"report.json"),JsonSerializer.Serialize(new{source,checks,failure,device,detail,
            seconds=timer.Elapsed.TotalSeconds,peakBytes=Process.GetCurrentProcess().PeakWorkingSet64},new JsonSerializerOptions{WriteIndented=true}));}
    }
}
