using System.Diagnostics;
using System.IO;
using System.Numerics;
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
using Vector2 = System.Numerics.Vector2;
using Vector3 = System.Numerics.Vector3;
using Vector4 = System.Numerics.Vector4;
using PixelFormat = OpenTK.Graphics.OpenGL4.PixelFormat;

// Device-contract checks: production renderer + actual GPU + explicit mapped
// state fixtures. No second production texture compositor or skin evaluator.
internal static class GpuMaterialRegression
{
    internal static int Run(string source, string output)
    {
        if (Directory.Exists(output)) throw new IOException("Fresh GPU report folder required.");
        Directory.CreateDirectory(output);
        var timer = Stopwatch.StartNew(); var observations = new List<object>();
        int checks = 0; string device = "", failure = "";
        void Check(bool ok, string message) { ++checks; if (!ok) throw new InvalidDataException(message); }
        try
        {
            var scene = SmoSceneBuilder.Build(SmoDocument.Load(source));
            var template = scene.Meshes.First(m => m.MaterialDraw?.Passes.Count > 0);
            var native = template.MaterialDraw!.Passes[0];
            var raster = native.Raster with { DepthEnable = 1, DepthWrite = 1, DepthFunction = 4,
                CullMode = 1, AlphaTest = 0, BlendEnable = 1, SourceBlend = 2, DestinationBlend = 1 };
            var material = template.LoadedMaterial! with { RenderStates = Array.AsReadOnly(new uint[] { 0,0,1,0,1,1,3,0,2,0,7 }) };
            Vector3[] positions = [new(-1,-1,0),new(1,-1,0),new(1,1,0),new(-1,1,0)];
            var mesh = SmoMesh.CreateTransient(template.Mesh, positions, Enumerable.Repeat(Vector3.UnitZ,4).ToArray(),
                Enumerable.Repeat(new Vector2(.25f,.5f),4).ToArray(), Enumerable.Repeat(new Vector2(.75f,.5f),4).ToArray(),
                Enumerable.Repeat(0x8080c040u,4).ToArray(), [], [], [0,1,2,0,2,3], 900001);
            var input = template with { Mesh = mesh, SceneObjectIndex = 900000, WorldTransform = Matrix4x4.Identity,
                SkinObjectIndex = null, InitialSkinMatrices = null, RigidNodeObjectIndex = null, LoadedMaterial = material,
                ContainerKind = SmoRenderContainerKind.RenderNode, OccurrenceKey = new(900000,0),
                MaterialRenderState = null, UsesAlphaBlend = false, Texture = null, BaseTexture = null };
            SmoLoadedTexture Texture(int id, int width, byte[] bgra) => new((uint)id,id,
                SmoTexture.CreateTransient("pixel fixture",width,1,bgra,id),null);
            var a = Texture(900010,1,[192,128,64,128]);
            var b = Texture(900011,1,[32,64,128,192]);
            var rg = Texture(900012,2,[0,0,255,255,0,255,0,255]);
            SmoMaterialTextureStage Stage(SmoLoadedTexture? texture, uint color = 4, uint alpha = 4) =>
                native.Stages[0] with { Texture = texture, ColorOperation = color, AlphaOperation = alpha,
                    Coordinates = 0, TransformFlags = 0, UVTransform = null, AddressU = 1, AddressV = 1,
                    Magnification = 1, Minification = 1, MipFilter = 0 };
            SmoMaterialDrawPass Pass(params SmoMaterialTextureStage[] active)
            {
                var stages = Enumerable.Repeat(Stage(null,1,2),8).ToArray(); active.CopyTo(stages,0);
                return native with { Raster = raster, Stages = Array.AsReadOnly(stages), Diffuse = Vector4.One };
            }
            using var window = new NativeWindow(new NativeWindowSettings { ClientSize = new Vector2i(16,16),
                StartVisible = false, StartFocused = false, API = ContextAPI.OpenGL, APIVersion = new Version(3,3),
                Profile = ContextProfile.Core, Flags = ContextFlags.ForwardCompatible, AutoLoadBindings = true });
            window.MakeCurrent(); device = GL.GetString(StringName.Renderer);
            int target = GL.GenFramebuffer(), colorBuffer = GL.GenTexture(), depthBuffer = GL.GenRenderbuffer();
            GL.BindTexture(TextureTarget.Texture2D,colorBuffer);
            GL.TexImage2D(TextureTarget.Texture2D,0,PixelInternalFormat.Rgba8,16,16,0,PixelFormat.Rgba,PixelType.UnsignedByte,IntPtr.Zero);
            GL.BindRenderbuffer(RenderbufferTarget.Renderbuffer,depthBuffer);
            GL.RenderbufferStorage(RenderbufferTarget.Renderbuffer,RenderbufferStorage.DepthComponent24,16,16);
            GL.BindFramebuffer(FramebufferTarget.Framebuffer,target);
            GL.FramebufferTexture2D(FramebufferTarget.Framebuffer,FramebufferAttachment.ColorAttachment0,TextureTarget.Texture2D,colorBuffer,0);
            GL.FramebufferRenderbuffer(FramebufferTarget.Framebuffer,FramebufferAttachment.DepthAttachment,RenderbufferTarget.Renderbuffer,depthBuffer);
            Check(GL.CheckFramebufferStatus(FramebufferTarget.Framebuffer)==FramebufferErrorCode.FramebufferComplete,"RGBA8 target");
            var renderer = new SmoGpuSceneRenderer(); renderer.SetAntialiasingSamples(0);
            var camera = new OrthographicCamera(new Point3D(0,0,5),new Vector3D(0,0,-1),new Vector3D(0,1,0),2)
                { NearPlaneDistance = .01, FarPlaneDistance = 100 };
            byte[] Pixel(string name, SmoSceneMesh model, params SmoMaterialDrawPass[] passes)
            {
                renderer.Clear();
                renderer.Add(model with { MaterialDraw = new(material.ObjectIndex,1,Array.AsReadOnly(passes)) },
                    new(0,900000,new(900000,0)),Colors.White);
                renderer.Render(16,16,camera,Colors.Black,0,new Point3D(),false,Colors.Gray,0,2,2);
                Check(renderer.MaterialIssues.Count==0,name+": "+string.Join("; ",renderer.MaterialIssues));
                var pixel = new byte[4]; GL.ReadPixels(8,8,1,1,PixelFormat.Rgba,PixelType.UnsignedByte,pixel);
                Check(GL.GetError()==ErrorCode.NoError,name+" GL state"); observations.Add(new { name,pixel });return pixel;
            }
            void Expect(string name, byte[] actual, int[] expected)
            { Check(actual.Select((v,i)=>Math.Abs(v-expected[i])).Max()<=2,name+": "+string.Join(",",actual)); }
            Expect("authored vertex color",Pixel("vertex",input,Pass()),[128,192,64,128]);
            Expect("native modulate",Pixel("modulate",input,Pass(Stage(a))),[32,96,48,64]);
            Expect("two texture stages",Pixel("two-stage",input,Pass(Stage(a,2,2),Stage(b,5,4))),[64,64,48,96]);
            var first = Pass(Stage(a,2,2));
            var second = Pass(Stage(b,2,2)) with { Ordinal = 1, Raster = raster with { SourceBlend = 5, DestinationBlend = 6 } };
            Expect("two actual draw passes",Pixel("two-pass",input,first,second),[112,80,72,176]);
            Expect("alpha test discard",Pixel("alpha-cut",input,first with { Raster = raster with { AlphaTest = 1, AlphaReference = 200, AlphaFunction = 7 } }),[0,0,0,255]);
            Expect("alpha test equality",Pixel("alpha-equal",input,first with { Raster = raster with { AlphaTest = 1, AlphaReference = 128, AlphaFunction = 3 } }),[64,128,192,128]);
            Expect("second UV channel",Pixel("uv1",input,Pass(Stage(rg,2,2),Stage(rg,7,2) with { Coordinates = 1 })),[255,255,0,255]);
            var translate = Matrix4x4.Identity; translate.M31=.5f;
            Expect("native UV matrix layout",Pixel("uv-transform",input,Pass(Stage(rg,2,2) with { TransformFlags = 2, UVTransform = translate })),[0,255,0,255]);
            var outside = SmoMesh.CreateTransient(mesh,positions,mesh.Normals,
                Enumerable.Repeat(new Vector2(1.25f,.5f),4).ToArray(),Enumerable.Repeat(new Vector2(1.25f,.5f),4).ToArray(),
                mesh.DiffuseColorsArgb,[],[],mesh.TriangleIndices,900002);
            Expect("independent sampler objects",Pixel("sampler-alias",input with { Mesh = outside },
                Pass(Stage(rg,2,2),Stage(rg,7,2) with { Coordinates = 1, AddressU = 3 })),[255,255,0,255]);
            Expect("mapped depth never",Pixel("depth-never",input,first with { Raster = raster with { DepthFunction = 1 } }),[0,0,0,255]);
            renderer.Clear(); renderer.Render(16,16,camera,Colors.Black,0,new Point3D(),false,Colors.Gray,0,2,2);
            GL.BindFramebuffer(FramebufferTarget.Framebuffer,0);GL.DeleteRenderbuffer(depthBuffer);GL.DeleteTexture(colorBuffer);GL.DeleteFramebuffer(target);
            Console.WriteLine($"PASS GPU material: {checks} checks, {device}");return 0;
        }
        catch(Exception error) { failure=error.ToString(); throw; }
        finally { File.WriteAllText(Path.Combine(output,"report.json"),JsonSerializer.Serialize(new { checks,device,failure,
            seconds=timer.Elapsed.TotalSeconds,observations },new JsonSerializerOptions { WriteIndented=true })); }
    }
}
