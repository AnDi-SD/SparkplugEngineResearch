using System.Diagnostics;
using System.IO;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Windows.Media.Media3D;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using SmoViewer.Core;
using SmoViewer.Scene;
using SmoViewer.Rendering.Wpf;
using Vector2 = System.Numerics.Vector2;
using Vector3 = System.Numerics.Vector3;
using Vector4 = System.Numerics.Vector4;
using Colors = System.Windows.Media.Colors;

// Modern device-contract fixtures consume actual loaded material/Fog snapshots.
// Expected colors follow documented pixel fog, not a second production shader.
internal static class GpuFogRegression
{
    internal static int Run(string source, string skySource, string output)
    {
        if (Directory.Exists(output)) throw new IOException("Fresh fog output required.");
        Directory.CreateDirectory(output);
        var timer = Stopwatch.StartNew(); int checks = 0; string failure = "", device = "";
        var observations = new List<object>(); object? detail = null;
        void Check(bool ok, string message) { ++checks; if (!ok) throw new InvalidDataException(message); }
        static string Hash(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
        try
        {
            string sourceHash = Hash(source), skyHash = Hash(skySource);
            var document = SmoDocument.Load(source); var scene = SmoSceneBuilder.Build(document);
            var template = scene.Meshes.First(mesh => mesh.FogDraw is { Enabled: 1, Mode: 3 } && mesh.MaterialDraw?.Passes.Count > 0);
            var fog = template.FogDraw!;
            Check(fog.KnownMask == 31 && fog.Issue is null && fog.Start < fog.End, "Actual loaded linear Fog has exactly its submitted words");
            Check(SmoLoadedResources.Get(document).Models[template.RenderableObjectIndex!.Value].FogId == fog.ObjectId,
                "Fog identity follows the actual Model reference");
            var native = template.MaterialDraw!.Passes[0];
            var raster = native.Raster with { DepthEnable = 1, DepthWrite = 1, DepthFunction = 4,
                CullMode = 1, AlphaTest = 0, BlendEnable = 0, SourceBlend = 2, DestinationBlend = 1 };
            var pass = native with { Raster = raster, Stages = Array.AsReadOnly(Enumerable.Repeat(
                native.Stages[0] with { Texture = null, ColorOperation = 1, AlphaOperation = 1 }, 8).ToArray()) };
            var material = template.LoadedMaterial! with { RenderStates = Array.AsReadOnly(new uint[] { 0,0,1,0,1,1,3,0,2,0,7 }) };
            int nextId = 900000;
            SmoSceneMesh Quad(float depth, SmoFogDraw? value, bool weighted = false)
            {
                int id = ++nextId;
                Vector3[] positions = [new(-10000,-10000,depth),new(10000,-10000,depth),new(10000,10000,depth),new(-10000,10000,depth)];
                var mesh = SmoMesh.CreateTransient(template.Mesh, positions, Enumerable.Repeat(Vector3.UnitZ,4).ToArray(),
                    Enumerable.Repeat(Vector2.Zero,4).ToArray(), Enumerable.Repeat(0x804020c0u,4).ToArray(),
                    weighted ? Enumerable.Repeat(new Vector4(1,0,0,0),4).ToArray() : [],
                    weighted ? Enumerable.Repeat(new SmoBlendIndices(0,0,0,0),4).ToArray() : [], [0,1,2,0,2,3], id);
                return template with { Mesh = mesh, SceneObjectIndex = id, WorldTransform = Matrix4x4.Identity,
                    SkinObjectIndex = weighted ? id : null, InitialSkinMatrices = weighted ? new[] { Matrix4x4.Identity } : null,
                    RigidNodeObjectIndex = null, LoadedMaterial = material, MaterialDraw = new(material.ObjectIndex,1,[pass]),
                    FogDraw = value, ContainerKind = SmoRenderContainerKind.RenderNode, OccurrenceKey = new(id,0),
                    MaterialRenderState = null, UsesAlphaBlend = false, Texture = null, BaseTexture = null,
                    SourceAlphaSort = false, AlphaSortData = null };
            }
            const int size = 32;
            using var window = new NativeWindow(new NativeWindowSettings { ClientSize = new Vector2i(size,size),
                StartVisible = false, StartFocused = false, API = ContextAPI.OpenGL, APIVersion = new Version(3,3),
                Profile = ContextProfile.Core, Flags = ContextFlags.ForwardCompatible, AutoLoadBindings = true });
            window.MakeCurrent(); device = GL.GetString(StringName.Renderer);
            int fbo = GL.GenFramebuffer(), color = GL.GenTexture(), depthBuffer = GL.GenRenderbuffer();
            GL.BindTexture(TextureTarget.Texture2D,color);
            GL.TexImage2D(TextureTarget.Texture2D,0,PixelInternalFormat.Rgba8,size,size,0,PixelFormat.Rgba,PixelType.UnsignedByte,IntPtr.Zero);
            GL.BindRenderbuffer(RenderbufferTarget.Renderbuffer,depthBuffer);
            GL.RenderbufferStorage(RenderbufferTarget.Renderbuffer,RenderbufferStorage.DepthComponent24,size,size);
            GL.BindFramebuffer(FramebufferTarget.Framebuffer,fbo);
            GL.FramebufferTexture2D(FramebufferTarget.Framebuffer,FramebufferAttachment.ColorAttachment0,TextureTarget.Texture2D,color,0);
            GL.FramebufferRenderbuffer(FramebufferTarget.Framebuffer,FramebufferAttachment.DepthAttachment,RenderbufferTarget.Renderbuffer,depthBuffer);
            Check(GL.CheckFramebufferStatus(FramebufferTarget.Framebuffer) == FramebufferErrorCode.FramebufferComplete, "RGBA8 fog framebuffer");
            var renderer = new SmoGpuSceneRenderer { RenderMode = SmoSceneRenderMode.Lit }; renderer.SetAntialiasingSamples(0);
            ProjectionCamera camera = new PerspectiveCamera(new Point3D(),new Vector3D(0,0,-1),new Vector3D(0,1,0),60)
                { NearPlaneDistance = 1, FarPlaneDistance = 100000 };
            byte[] Render()
            {
                renderer.Render(size,size,camera,Colors.Black,0,new Point3D(),false,Colors.Gray,0,2,2);
                GL.Finish(); var pixels = new byte[size*size*4];
                GL.ReadPixels(0,0,size,size,PixelFormat.Rgba,PixelType.UnsignedByte,pixels);
                Check(GL.GetError() == ErrorCode.NoError, "Fog draw has no GL errors"); return pixels;
            }
            void Add(SmoSceneMesh mesh) => renderer.Add(mesh,new(0,mesh.SceneObjectIndex,mesh.OccurrenceKey),Colors.White);
            byte[] Pixel(string name, float depth, SmoFogDraw? value)
            {
                renderer.Clear(); Add(Quad(depth,value)); var pixels = Render();
                Check(renderer.RenderIssues.Count == 0, name + ": " + string.Join("; ",renderer.RenderIssues));
                var sample = pixels.AsSpan((size/2*size+size/2)*4,4).ToArray();
                Check(new[] { (2*size+2)*4, (2*size+size-3)*4, ((size-3)*size+2)*4 }
                    .All(index => pixels.AsSpan(index,4).SequenceEqual(sample)), "Constant eye depth has equal fog off-axis");
                observations.Add(new { name, depth, sample, fog = value, placements = renderer.LastFogPlacementCount }); return sample;
            }
            void Expect(byte[] actual, double factor, uint colorArgb)
            {
                int[] sourceColor = [64,32,192]; int[] fogColor = [(int)((colorArgb>>16)&255),(int)((colorArgb>>8)&255),(int)(colorArgb&255)];
                Check(Enumerable.Range(0,3).All(i => Math.Abs(actual[i] - (sourceColor[i]*factor + fogColor[i]*(1-factor))) <= 2),
                    "GPU RGB matches documented fog factor");
                Check(actual[3] == 128, "Fog preserves fragment alpha");
            }
            float middle = (fog.Start + fog.End)/2;
            foreach (var pair in new[] { (fog.Start/2,1d), (fog.Start,1d), (middle,.5), (fog.End,0d), (fog.End*1.5f,0d) })
                Expect(Pixel("linear",pair.Item1,fog),pair.Item2,fog.Color);
            var half = Pixel("alpha-byte-ignored",middle,fog with { Color = fog.Color ^ 0xff000000 }); Expect(half,.5,fog.Color);
            renderer.ShowFog = false; var noFog = Render();
            Expect(noFog.AsSpan((size/2*size+size/2)*4,4).ToArray(),1,fog.Color);
            Check(renderer.LastFogPlacementCount == 0, "Fog toggle resets active state");
            renderer.ShowFog = true; Check(Render().AsSpan((size/2*size+size/2)*4,4).SequenceEqual(half), "Fog toggle restores exact pixels");
            renderer.RenderMode = SmoSceneRenderMode.Unlit;
            Check(Render().AsSpan((size/2*size+size/2)*4,4).SequenceEqual(half), "Unlit changes illumination, while Fog remains independently enabled");
            renderer.RenderMode = SmoSceneRenderMode.Lit;
            foreach (uint mode in new uint[] {1,2})
            {
                var exponential = fog with { KnownMask = 39, Mode = mode, Density = .001f, Start = float.NaN, End = float.NaN };
                double product = .001f * 1000;
                Expect(Pixel("exponential-"+mode,1000,exponential),Math.Exp(mode == 1 ? -product : -product*product),fog.Color);
            }
            var disabled = fog with { KnownMask = 1, Enabled = 0, Start = float.NaN, End = float.NaN, Density = float.NaN };
            Expect(Pixel("disabled-unused-NaN",middle,disabled),1,fog.Color);
            camera = new OrthographicCamera(new Point3D(),new Vector3D(0,0,-1),new Vector3D(0,1,0),20000)
                { NearPlaneDistance = 1, FarPlaneDistance = 10000 };
            var affine = fog with { Start = .25f, End = .75f };
            Expect(Pixel("affine-device-depth",5000,affine),(.75-(5000d-1)/9999)/.5,fog.Color);
            camera = new PerspectiveCamera(new Point3D(),new Vector3D(0,0,-1),new Vector3D(0,1,0),60)
                { NearPlaneDistance = 1, FarPlaneDistance = 100000 };
            renderer.Clear(); Add(Quad(middle,fog)); Add(Quad(fog.Start/2,disabled));
            Expect(Render().AsSpan((size/2*size+size/2)*4,4).ToArray(),1,fog.Color);
            Check(renderer.LastFogPlacementCount == 1, "Fog does not leak into the following disabled placement");
            var tested = Quad(middle,fog);
            renderer.Clear(); Add(tested with { MaterialDraw = new(material.ObjectIndex,1,
                [pass with { Raster = raster with { AlphaTest = 1, AlphaFunction = 5, AlphaReference = 200 } }]) });
            Check(Render().AsSpan((size/2*size+size/2)*4,4).SequenceEqual(new byte[] {0,0,0,255}),
                "Fog does not revive a fragment rejected by material alpha test");
            renderer.Clear(); Add(Quad(middle,fog with { End = fog.Start })); Render();
            Check(renderer.RenderIssues.Any(issue => issue.StartsWith("FOG_GPU_UNAVAILABLE")), "Degenerate consumed linear range is explicit");
            renderer.Clear(); Add(Quad(middle,fog,weighted:true)); Render();
            Check(renderer.RenderIssues.Any(issue => issue.StartsWith("FOG_SHADER_UNVERIFIED")) && renderer.LastFogPlacementCount == 0,
                "Weighted shader fog remains an explicit unverified boundary");
            renderer.Clear();
            var skyScene = SmoSceneBuilder.Build(SmoDocument.Load(skySource));
            var skies = skyScene.Meshes.Where(mesh => mesh.ContainerKind == SmoRenderContainerKind.SkyBox).ToArray();
            foreach (var sky in skies) Add(sky with { FogDraw = fog });
            Render(); Check(skies.Length > 0 && renderer.LastSkyPlacementCount == skies.Length && renderer.LastFogPlacementCount == 0 &&
                renderer.RenderIssues.Count == 0, "Separate real SkyBox pass ignores attached Fog snapshots");
            renderer.Clear();
            var boundsMin = new Vector3(float.PositiveInfinity); var boundsMax = new Vector3(float.NegativeInfinity);
            foreach (var real in scene.Meshes)
            {
                Add(real);
                foreach (var position in real.Mesh.Positions)
                { var world = Vector3.Transform(position,real.WorldTransform); boundsMin = Vector3.Min(boundsMin,world); boundsMax = Vector3.Max(boundsMax,world); }
            }
            var center = (boundsMin+ boundsMax)/2; var extent = boundsMax-boundsMin;
            double fieldOfView = Math.Clamp(2*Math.Atan(Math.Max(extent.X,extent.Y)*.8/middle)*180/Math.PI,.05,90);
            camera = new PerspectiveCamera(new Point3D(center.X,center.Y,-center.Z+middle),new Vector3D(0,0,-1),new Vector3D(0,1,0),fieldOfView)
                { NearPlaneDistance = 1, FarPlaneDistance = 100000 };
            var realFog = Render(); int realFogPlacements = renderer.LastFogPlacementCount;
            Check(realFogPlacements > 0 && renderer.RenderIssues.Count == 0, "Actual source geometry consumes its own loaded Fog and material");
            renderer.ShowFog = false; var realClear = Render();
            int changedComponents = realFog.Zip(realClear).Count(pair => pair.First != pair.Second);
            Check(changedComponents > 16, "Actual SMO fog changes covered image components");
            Check(Enumerable.Range(0,size*size).All(i => realFog[i*4+3] == realClear[i*4+3]), "Actual SMO alpha remains unchanged");
            renderer.ShowFog = true; Check(Render().SequenceEqual(realFog), "Actual SMO toggle restores exact pixels");
            File.WriteAllBytes(Path.Combine(output,"source-fog.rgba"),realFog);
            File.WriteAllBytes(Path.Combine(output,"source-clear.rgba"),realClear);
            Check(sourceHash == Hash(source) && skyHash == Hash(skySource), "Game source files remain unchanged");
            detail = new { sourceHash, skyHash, originalFog = fog, sceneMeshes = scene.Meshes.Count, skyMeshes = skies.Length,
                realFogPlacements, changedComponents, fieldOfView,
                native = Hash(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")), renderer = Hash(typeof(SmoGpuSceneRenderer).Assembly.Location) };
            renderer.Clear(); Render(); Check(renderer.LastFogPlacementCount == 0 && renderer.RenderIssues.Count == 0, "Clear resets fog diagnostics and draw count");
            GL.BindFramebuffer(FramebufferTarget.Framebuffer,0); GL.DeleteRenderbuffer(depthBuffer); GL.DeleteTexture(color); GL.DeleteFramebuffer(fbo);
            Console.WriteLine($"PASS GPU fog: {checks} checks, {device}"); return 0;
        }
        catch (Exception error) { failure = error.ToString(); throw; }
        finally { File.WriteAllText(Path.Combine(output,"report.json"),JsonSerializer.Serialize(new { source,skySource,checks,failure,device,detail,
            seconds = timer.Elapsed.TotalSeconds, peakBytes = Process.GetCurrentProcess().PeakWorkingSet64, observations },new JsonSerializerOptions
                { WriteIndented = true, NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals })); }
    }
}
