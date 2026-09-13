using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Sparkplug;
using Float4=System.Numerics.Vector4;

// Numeric device fixtures for the production vertex shader. The distinctive
// point/spot/order expectations are read from shipped PC Fixed.rfx, not from a
// second CPU implementation. Actual selected-cache binding is checked separately.
internal static class GpuLightingRegression
{
    internal static int Run(string output)
    {
        if(Directory.Exists(output))throw new IOException("Fresh lighting GPU output required.");
        Directory.CreateDirectory(output);var timer=Stopwatch.StartNew();int checks=0;
        string failure="",device="";var observations=new List<object>();
        string source=(string)typeof(SmoGpuSceneRenderer).GetField("VertexShaderSource",BindingFlags.Static|BindingFlags.NonPublic)!.GetRawConstantValue()!;
        void Check(bool ok,string message){++checks;if(!ok)throw new InvalidDataException(message);}
        try
        {
            using var window=new NativeWindow(new NativeWindowSettings {ClientSize=new Vector2i(16,16),
                StartVisible=false,StartFocused=false,API=ContextAPI.OpenGL,APIVersion=new Version(3,3),
                Profile=ContextProfile.Core,Flags=ContextFlags.ForwardCompatible,AutoLoadBindings=true});
            window.MakeCurrent();device=GL.GetString(StringName.Renderer);
            int shader=GL.CreateShader(ShaderType.VertexShader),program=GL.CreateProgram(),vao=GL.GenVertexArray(),buffer=GL.GenBuffer();
            try
            {
                GL.ShaderSource(shader,source);GL.CompileShader(shader);GL.GetShader(shader,ShaderParameter.CompileStatus,out int compiled);
                Check(compiled!=0,GL.GetShaderInfoLog(shader));GL.AttachShader(program,shader);
                GL.TransformFeedbackVaryings(program,1,["vFixedColor"],TransformFeedbackMode.InterleavedAttribs);
                GL.LinkProgram(program);GL.GetProgram(program,GetProgramParameterName.LinkStatus,out int linked);
                Check(linked!=0,GL.GetProgramInfoLog(program));GL.UseProgram(program);
                int U(string name)=>GL.GetUniformLocation(program,name);
                var vectors=new Dictionary<string,Float4>();var integers=new Dictionary<string,int>();
                void V(string name,float x,float y,float z,float w=1)=>vectors[name]=new(x,y,z,w);
                void I(string name,int value)=>integers[name]=value;
                float power=0;float[] cone=[0,0];
                var publisher=new SmoGpuSceneRenderer();const BindingFlags hidden=BindingFlags.Instance|BindingFlags.NonPublic;
                typeof(SmoGpuSceneRenderer).GetField("_program",hidden)!.SetValue(publisher,program);
                typeof(SmoGpuSceneRenderer).GetMethod("InitializeLightingUniforms",hidden)!.Invoke(publisher,null);
                var publish=typeof(SmoGpuSceneRenderer).GetMethod("ApplyShaderLighting",hidden)!;
                float[] identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];
                foreach(string name in new[]{"uModel","uView","uProjection","uFixedLightingView"})
                    GL.UniformMatrix4(U(name),1,true,identity);
                I("uFixedLightingEnabled",1);I("uFixedLightCount",0);
                V("uFixedAmbient",.1f,.2f,.3f,.4f);V("uFixedDiffuse",.4f,.5f,.6f,.75f);V("uFixedConstant",.9f,.8f,.7f,.6f);
                GL.BindVertexArray(vao);GL.VertexAttrib3(0,0f,0f,2f);GL.VertexAttrib3(6,0f,0f,-1f);
                GL.VertexAttrib4(2,.2f,.3f,.4f,.5f);
                GL.BindBuffer(BufferTarget.TransformFeedbackBuffer,buffer);GL.BufferData(BufferTarget.TransformFeedbackBuffer,16,IntPtr.Zero,BufferUsageHint.StreamRead);
                GL.BindBufferBase(BufferRangeTarget.TransformFeedbackBuffer,0,buffer);GL.Enable(EnableCap.RasterizerDiscard);
                void Expect(string name,params float[] expected)
                {
                    var lights=Enumerable.Range(0,integers.GetValueOrDefault("uFixedLightCount")).Select(i=>new SparkplugShaderLight(
                        (uint)integers.GetValueOrDefault($"uFixedLightType[{i}]"),127,
                        vectors.GetValueOrDefault($"uFixedLightDiffuse[{i}]"),vectors.GetValueOrDefault($"uFixedLightSpecular[{i}]"),
                        vectors.GetValueOrDefault($"uFixedLightPosition[{i}]"),vectors.GetValueOrDefault($"uFixedLightDirection[{i}]"),
                        vectors.GetValueOrDefault($"uFixedLightAttenuation[{i}]"),cone[0],cone[1])).ToArray();
                    publish.Invoke(publisher,[new SparkplugShaderLighting((uint)integers["uFixedLightingMode"],
                        integers.GetValueOrDefault("uFixedUseSpecular")!=0,15,vectors["uFixedAmbient"],vectors["uFixedDiffuse"],
                        vectors["uFixedConstant"],power,Array.AsReadOnly(lights))]);
                    GL.BeginTransformFeedback(TransformFeedbackPrimitiveType.Points);GL.DrawArrays(PrimitiveType.Points,0,1);GL.EndTransformFeedback();
                    var actual=new float[4];GL.GetBufferSubData(BufferTarget.TransformFeedbackBuffer,IntPtr.Zero,16,actual);
                    float error=actual.Zip(expected).Max(p=>Math.Abs(p.First-p.Second));
                    observations.Add(new{name,expected,actual,error});Check(actual.All(float.IsFinite)&&error<=3e-6f,name+": "+string.Join(",",actual));
                }
                float[][] colors=[[1,1,1,1],[.4f,.5f,.6f,.75f],[.2f,.3f,.4f,.5f],[.1f,.2f,.3f,.75f],
                    [.3f,.5f,.7f,.75f],[.02f,.06f,.12f,.375f],[.9f,.8f,.7f,.6f]];
                for(int mode=0;mode<7;++mode){I("uFixedLightingMode",mode);Expect("color-mode-"+mode,colors[mode]);}
                I("uFixedLightingMode",3);I("uFixedLightCount",1);I("uFixedLightType[0]",0);
                V("uFixedLightDirection[0]",0,0,1,0);V("uFixedLightDiffuse[0]",.2f,.3f,.1f);
                V("uFixedLightSpecular[0]",.1f,.2f,.1f);
                Expect("directional-diffuse",.3f,.5f,.4f,.75f);
                I("uFixedLightingMode",7);Expect("fallback-color-mode-7",.3f,.5f,.4f,.75f);I("uFixedLightingMode",3);
                I("uFixedUseSpecular",1);power=4f;
                Expect("directional-specular",.4f,.7f,.5f,.75f);
                I("uFixedUseSpecular",0);I("uFixedLightType[0]",1);V("uFixedLightPosition[0]",0,0,0);
                V("uFixedLightAttenuation[0]",1,.5f,99,1000);
                Expect("point-attenuates-ambient-too",.15f,.25f,.2f,.75f);
                I("uFixedUseSpecular",1);
                Expect("point-specular-ignores-attenuation",.4f,.7f,.5f,.75f);
                I("uFixedUseSpecular",0);I("uFixedLightCount",2);
                I("uFixedLightType[1]",0);V("uFixedLightDirection[1]",0,0,1,0);V("uFixedLightDiffuse[1]",.1f,.1f,.2f);
                Expect("point-before-directional",.25f,.35f,.4f,.75f);
                I("uFixedLightType[0]",0);V("uFixedLightDiffuse[0]",.1f,.1f,.2f);
                I("uFixedLightType[1]",1);V("uFixedLightPosition[1]",0,0,0);V("uFixedLightDiffuse[1]",.2f,.3f,.1f);
                V("uFixedLightAttenuation[1]",1,.5f,99,1000);
                Expect("directional-before-point",.2f,.3f,.3f,.75f);
                I("uFixedLightCount",1);I("uFixedLightType[0]",2);V("uFixedLightDiffuse[0]",.2f,.3f,.1f);
                V("uFixedLightDirection[0]",0,0,.75f,0);cone=[1,.5f];
                Expect("spot-half-cone",.2f,.35f,.35f,.75f);
                GL.VertexAttrib3(6,0f,0f,1f);
                Expect("spot-keeps-negative-diffuse",0f,.05f,.25f,.75f);
                GL.VertexAttrib3(6,0f,0f,-1f);I("uFixedUseSpecular",1);
                Expect("spot-specular",.25f,.45f,.4f,.75f);
                I("uFixedLightType[0]",0);V("uFixedLightDirection[0]",1,0,0,0);
                Expect("lit-zero-diffuse-rejects-specular",.1f,.2f,.3f,.75f);
                // Shipped HLSL normalizes float4(-PosView), then truncates to
                // float3. At P=(0,0,1,1), L=(1,0,1,1), N=(1,0,0), power=4,
                // this yields (sqrt(2/3))^4=4/9, not the vec3 result 1/4.
                // Source compilation and the independent system D3D9 oracle
                // are retained in pc-shader-contract evidence (12 Sep 2026).
                GL.VertexAttrib3(0,0f,0f,1f);GL.VertexAttrib3(6,1f,0f,0f);
                I("uFixedLightType[0]",1);I("uFixedUseSpecular",1);power=4;
                V("uFixedAmbient",0,0,0,0);V("uFixedLightDiffuse[0]",0,0,0,0);
                V("uFixedLightSpecular[0]",1,1,1,1);V("uFixedLightPosition[0]",1,0,1,1);
                Expect("point-specular-normalizes-homogeneous-view",4f/9,4f/9,4f/9,.75f);
                I("uFixedLightType[0]",2);V("uFixedLightDirection[0]",-1,0,0,0);cone=[1,.5f];
                Expect("spot-specular-normalizes-homogeneous-view",4f/9,4f/9,4f/9,.75f);
                Check(GL.GetError()==ErrorCode.NoError,"GL numeric lighting completed");
            }
            finally {GL.Disable(EnableCap.RasterizerDiscard);GL.UseProgram(0);GL.BindVertexArray(0);
                GL.DeleteBuffer(buffer);GL.DeleteVertexArray(vao);GL.DeleteProgram(program);GL.DeleteShader(shader);}
            Console.WriteLine($"PASS GPU original lighting arithmetic: {checks} checks on {device}");return 0;
        }
        catch(Exception error){failure=error.ToString();throw;}
        finally {File.WriteAllText(Path.Combine(output,"report.json"),JsonSerializer.Serialize(new{checks,failure,device,
            seconds=timer.Elapsed.TotalSeconds,peakBytes=Process.GetCurrentProcess().PeakWorkingSet64,
            shaderSHA256=Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(source))),observations},new JsonSerializerOptions{WriteIndented=true}));}
    }
}
