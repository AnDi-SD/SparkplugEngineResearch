using System.Diagnostics;
using System.IO;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Scene;
using SmoViewer.Sparkplug;
using Vector3 = System.Numerics.Vector3;

internal static class GpuMaterialSceneRegression
{
    internal static int Run(string source, string output, bool documentLighting = false)
    {
        if (Directory.Exists(output)) throw new IOException("Fresh GPU scene report folder required.");
        Directory.CreateDirectory(output);
        int checks=0; var timer=Stopwatch.StartNew(); string failure="",device="";
        object? detail=null;
        void Check(bool ok,string message) { ++checks; if(!ok)throw new InvalidDataException(message); }
        try
        {
            var document=SmoDocument.Load(source);var scene=SmoSceneBuilder.Build(document);
            var meshes=scene.Meshes.Where(m=>m.ContainerKind!=SmoRenderContainerKind.SkyBox).ToArray();
            Check(meshes.Length>0&&(documentLighting ? meshes.Any(m=>m.Mesh.HasSkinningData) : meshes.All(m=>!m.Mesh.HasSkinningData)),
                "Selected scene matches the rigid-level or actual skinned-lighting slice.");
            using var runtime=new SparkplugSceneRuntime(document,scene.Skins,enableDocumentLighting:documentLighting);
            detail = new { missing = meshes.Where(m=>m.MaterialDraw is null).Select(m=>new {m.SceneObjectIndex,material=m.LoadedMaterial?.ObjectIndex}).ToArray(),
                issues = scene.TextureIssues.Where(i=>i.StartsWith("MATERIAL_DRAW_UNAVAILABLE:")).ToArray() };
            Check(meshes.All(m=>m.LoadedMaterial is null || m.MaterialDraw is not null),"Every selected material has a common draw: "+JsonSerializer.Serialize(detail));
            var minimum=new Vector3(float.PositiveInfinity);var maximum=new Vector3(float.NegativeInfinity);
            foreach(var m in meshes)foreach(var p in m.Mesh.Positions)
            {
                var point=Vector3.Transform(p,m.WorldTransform*Matrix4x4.CreateScale(1,1,-1));
                minimum=Vector3.Min(minimum,point);maximum=Vector3.Max(maximum,point);
            }
            var center=(minimum+maximum)*.5f;float extent=(maximum-minimum).Length();
            const int width=192,height=192;
            using var window=new NativeWindow(new NativeWindowSettings { ClientSize=new Vector2i(width,height),
                StartVisible=false,StartFocused=false,API=ContextAPI.OpenGL,APIVersion=new Version(3,3),
                Profile=ContextProfile.Core,Flags=ContextFlags.ForwardCompatible,AutoLoadBindings=true });
            window.MakeCurrent();device=GL.GetString(StringName.Renderer);
            var renderer=new SmoGpuSceneRenderer();renderer.SetAntialiasingSamples(0);
            foreach(var mesh in meshes)renderer.Add(mesh,new(0,mesh.SceneObjectIndex,mesh.OccurrenceKey),Colors.White);
            foreach(var text in scene.Texts)renderer.AddText(text,new(0,text.Text.ObjectIndex,text.OccurrenceKey));
            if(documentLighting){renderer.SetMaterialRuntime(0,runtime.Materials);renderer.SetLightingRuntime(0,runtime);}
            var camera=new PerspectiveCamera(new Point3D(center.X,center.Y+extent*.5,center.Z+extent*.8),
                new Vector3D(0,-extent*.5,-extent*.8),new Vector3D(0,1,0),45)
                {NearPlaneDistance=.01,FarPlaneDistance=extent*4};
            void Render() => renderer.Render(width,height,camera,Color.FromRgb(10,20,30),0,
                new Point3D(center.X,center.Y,center.Z),false,Colors.Gray,minimum.Y,extent,extent);
            Render();GL.Finish();var upload=renderer.ConsumeUploadReport();
            var mipReadback=GpuMipChainRegression.Verify(renderer,scene,Check);
            Check(upload is not null&&upload.CopiedTextureBytes==0,"Owned texture arrays upload without extra managed pixel copies");
            Check(upload!.GeneratedMipTextureCount==0&&upload.RuntimeMipTextureCount==upload.TextureCount,
                "Selected actual material textures use their complete runtime mip chains");
            var staticMs=new List<double>();
            for(int i=0;i<3;++i){var t=Stopwatch.StartNew();Render();GL.Finish();staticMs.Add(t.Elapsed.TotalMilliseconds);}
            Check(renderer.RenderIssues.Count==0,string.Join("; ",renderer.RenderIssues.Take(8)));
            int expectedQueued=meshes.Where(mesh=>mesh.RequiresTransparentOrdering).Select(mesh=>new SmoRenderObjectKey(0,mesh.SceneObjectIndex,mesh.OccurrenceKey)).Distinct().Count()
                +scene.Texts.Count(text=>text.Text.Geometry!.Vertices.Count>0&&text.Text.AlphaSortData?.RequiresQueue==true);
            Check(renderer.LastAlphaOrder.Count==expectedQueued,"All actual eligible occurrence units enter native alpha order");
            Check(renderer.LastAlphaOrder.All(item=>item.OriginalSphere),"Actual scene alpha uses loaded sphere metadata");
            var alphaSummary=new {queued=expectedQueued,rawAlphaPlacements=meshes.Count(mesh=>mesh.SourceAlphaSort==true),
                priorityBands=renderer.LastAlphaOrder.GroupBy(item=>item.Priority).Select(group=>new{priority=group.Key,count=group.Count()}).ToArray()};
            var pixels=new byte[width*height*4];GL.ReadPixels(0,0,width,height,OpenTK.Graphics.OpenGL4.PixelFormat.Rgba,PixelType.UnsignedByte,pixels);
            int textChangedComponents=0;
            if(scene.Texts.Count>0)
            {
                foreach(var text in scene.Texts)renderer.SetAppearance(new(0,text.Text.ObjectIndex,text.OccurrenceKey),false,false,1);
                Render();GL.Finish();var withoutText=new byte[pixels.Length];GL.ReadPixels(0,0,width,height,OpenTK.Graphics.OpenGL4.PixelFormat.Rgba,PixelType.UnsignedByte,withoutText);
                textChangedComponents=pixels.Zip(withoutText).Count(pair=>pair.First!=pair.Second);
                Check(textChangedComponents>0,"Actual Text glyphs change framebuffer pixels");
                foreach(var text in scene.Texts)renderer.SetAppearance(new(0,text.Text.ObjectIndex,text.OccurrenceKey),true,false,1);
                Render();GL.Finish();var restoredText=new byte[pixels.Length];GL.ReadPixels(0,0,width,height,OpenTK.Graphics.OpenGL4.PixelFormat.Rgba,PixelType.UnsignedByte,restoredText);
                Check(pixels.SequenceEqual(restoredText),"Re-enabling Text restores exact framebuffer");
            }
            int covered=0;
            for(int i=0;i<pixels.Length;i+=4)if(pixels[i]!=10||pixels[i+1]!=20||pixels[i+2]!=30)++covered;
            Check(covered>20,"Actual scene produces non-background pixels");
            var bgra=new byte[pixels.Length];
            for(int y=0;y<height;++y)for(int x=0;x<width;++x)
            {int a=(y*width+x)*4,b=((height-1-y)*width+x)*4;bgra[b]=pixels[a+2];bgra[b+1]=pixels[a+1];bgra[b+2]=pixels[a];bgra[b+3]=pixels[a+3];}
            var bitmap=BitmapSource.Create(width,height,96,96,PixelFormats.Bgra32,null,bgra,width*4);
            var png=new PngBitmapEncoder();png.Frames.Add(BitmapFrame.Create(bitmap));
            using(var stream=File.Create(Path.Combine(output,"scene.png")))png.Save(stream);
            renderer.SetMaterialRuntime(0,runtime.Materials);
            Render();GL.Finish();var liveMs=new List<double>();
            for(int i=0;i<3;++i){var t=Stopwatch.StartNew();Render();GL.Finish();liveMs.Add(t.Elapsed.TotalMilliseconds);}
            Check(renderer.RenderIssues.Count==0,string.Join("; ",renderer.RenderIssues.Take(8)));
            Check(GL.GetError()==ErrorCode.NoError,"No GL errors after live material frames");
            object? lightingEffect=null;
            if(documentLighting)
            {
                var before=new byte[pixels.Length];GL.ReadPixels(0,0,width,height,OpenTK.Graphics.OpenGL4.PixelFormat.Rgba,PixelType.UnsignedByte,before);
                var shaderInputs=meshes.Where(mesh=>mesh.SkinObjectIndex is not null&&mesh.LoadedMaterial is not null&&mesh.OccurrenceKey is not null)
                    .Select(mesh=>runtime.CaptureShaderLighting(mesh.OccurrenceKey!.Value.ContainerObjectIndex,mesh.LoadedMaterial!.ObjectIndex,Matrix4x4.Identity)).ToArray();
                bool contributes=shaderInputs.Any(input=>input.ColorMode is not (0 or 1 or 2 or 6)&&
                    (input.Lights.Count>0||input.Ambient.X!=0||input.Ambient.Y!=0||input.Ambient.Z!=0));
                runtime.SetLightingActive(false);Render();GL.Finish();
                var after=new byte[pixels.Length];GL.ReadPixels(0,0,width,height,OpenTK.Graphics.OpenGL4.PixelFormat.Rgba,PixelType.UnsignedByte,after);
                int changed=before.Zip(after).Count(pair=>pair.First!=pair.Second);
                Check(contributes ? changed>20 : changed==0,
                    "Activating actual lights follows the loaded shader colors; a black ambient has no RGB contribution");
                Check(renderer.RenderIssues.Count==0,"Empty native light cache is a valid shader input");
                runtime.SetLightingActive(true);Render();GL.Finish();
                var restored=new byte[pixels.Length];GL.ReadPixels(0,0,width,height,OpenTK.Graphics.OpenGL4.PixelFormat.Rgba,PixelType.UnsignedByte,restored);
                Check(before.SequenceEqual(restored),"Restoring actual scene light activation restores the rendered pixels");
                lightingEffect=new {contributes,changedComponents=changed,shaderInputs=shaderInputs.Select(input=>new {input.ColorMode,input.UseSpecular,
                    ambient=new[]{input.Ambient.X,input.Ambient.Y,input.Ambient.Z,input.Ambient.W},lights=input.Lights.Count}).ToArray(),selected=runtime.LightSelections.Select(pair=>new {container=pair.Key,
                    ambient=pair.Value.AmbientObjectIndex,ordinary=pair.Value.OrdinaryObjectIndices}).ToArray()};
            }
            var mipUpgrade=GpuMipChainRegression.VerifyLateUpgrade(scene,Check);
            detail=new { meshes=meshes.Length,texts=scene.Texts.Count,textChangedComponents,passes=meshes.Sum(m=>m.MaterialDraw?.Passes.Count ?? 1)+scene.Texts.Sum(t=>t.Text.Geometry!.MaterialDraw.Passes.Count),withoutMaterial=meshes.Count(m=>m.LoadedMaterial is null),covered,upload,mipUpgrade,
                staticMs,liveMs,alphaSummary,lightingEffect,mipReadback,decodeIssues=scene.DecodeErrors,textureIssues=scene.TextureIssues,
                native=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")))),
                renderer=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(typeof(SmoGpuSceneRenderer).Assembly.Location))) };
            renderer.Clear();Render();Console.WriteLine($"PASS GPU level materials: {meshes.Length} placements, {covered} pixels, live average {liveMs.Average():F2} ms");return 0;
        }
        catch(Exception error){failure=error.ToString();throw;}
        finally{File.WriteAllText(Path.Combine(output,"report.json"),JsonSerializer.Serialize(new {source,checks,device,failure,
            seconds=timer.Elapsed.TotalSeconds,peakBytes=Process.GetCurrentProcess().PeakWorkingSet64,detail},new JsonSerializerOptions{WriteIndented=true}));}
    }
}
