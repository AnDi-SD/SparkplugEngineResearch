using System.IO;
using System.Numerics;
using OpenTK.Graphics.OpenGL4;
using SmoViewer.Sparkplug;

namespace SmoViewer.Rendering.Wpf;

public sealed partial class SmoGpuSceneRenderer
{
    private readonly Dictionary<int,SparkplugSceneRuntime> _lightingRuntimes = [];
    private readonly Dictionary<(int File,int Container,int Material),SparkplugShaderLighting> _shaderLightingCaptures = [];
    private Matrix4x4 _gameWorldToShaderView;
    private int _fixedLightingEnabled, _fixedLightingView, _fixedLightingMode, _fixedLightCount,
        _fixedUseSpecular, _fixedAmbient, _fixedDiffuse, _fixedConstant, _fixedPower;
    private readonly int[,] _fixedLightLocations = new int[8,7];

    /// <summary>Borrow the application's scene, including its actual selected
    /// light caches. Clear drops this reference; ownership stays with the host.</summary>
    public void SetLightingRuntime(int fileIndex, SparkplugSceneRuntime runtime)
    {
        ArgumentNullException.ThrowIfNull(runtime);
        if (!runtime.LightingConfigured) throw new InvalidDataException("Configure scene lights before binding the renderer.");
        _lightingRuntimes[fileIndex] = runtime;
    }

    private void InitializeLightingUniforms()
    {
        int Location(string name) => GL.GetUniformLocation(_program,name);
        _fixedLightingEnabled=Location("uFixedLightingEnabled"); _fixedLightingView=Location("uFixedLightingView");
        _fixedLightingMode=Location("uFixedLightingMode"); _fixedLightCount=Location("uFixedLightCount");
        _fixedUseSpecular=Location("uFixedUseSpecular"); _fixedAmbient=Location("uFixedAmbient");
        _fixedDiffuse=Location("uFixedDiffuse"); _fixedConstant=Location("uFixedConstant"); _fixedPower=Location("uFixedPower");
        string[] fields=["Type","Diffuse","Specular","Position","Direction","Attenuation","Cone"];
        for (int i=0;i<8;++i) for (int f=0;f<fields.Length;++f)
            _fixedLightLocations[i,f]=Location($"uFixedLight{fields[f]}[{i}]");
    }

    private bool BindShaderLighting(GpuRenderItem item)
    {
        GL.Uniform1(_fixedLightingEnabled,0);
        if (item.BoneMatrices.Length==0) return true; // original rigid path is fixed-function
        if (item.Material is null || item.Key.OccurrenceKey is not { } occurrence ||
            !_lightingRuntimes.TryGetValue(item.Key.FileIndex,out var runtime)) return false;
        var key=(item.Key.FileIndex,occurrence.ContainerObjectIndex,item.Material.ObjectIndex);
        if (!_shaderLightingCaptures.TryGetValue(key,out var lighting))
        {
            lighting=runtime.CaptureShaderLighting(occurrence.ContainerObjectIndex,
                item.Material.ObjectIndex,_gameWorldToShaderView);
            _shaderLightingCaptures.Add(key,lighting);
        }
        ApplyShaderLighting(lighting);
        return true;
    }

    private void ApplyShaderLighting(SparkplugShaderLighting lighting)
    {
        if (lighting.KnownMask!=15 || lighting.Lights.Count>8)
            throw new InvalidDataException("Incomplete original shader constants.");
        bool lit=lighting.ColorMode is not (0 or 1 or 2 or 6);
        foreach (var light in lighting.Lights)
        {
            uint needed=lit ? 1u | (lighting.UseSpecular?2u:0u) |
                (light.Type!=0?4u:0u) | (light.Type!=1?8u:0u) |
                (light.Type==1&&!lighting.UseSpecular?16u:0u) | (light.Type==2?96u:0u) : 0;
            if (light.Type>2 || (light.KnownMask&needed)!=needed)
                throw new InvalidDataException("Selected shader light has unproduced constants.");
        }
        static void Vector(int location,Vector4 value) => GL.Uniform4(location,value.X,value.Y,value.Z,value.W);
        GL.Uniform1(_fixedLightingMode,(int)lighting.ColorMode); GL.Uniform1(_fixedLightCount,lighting.Lights.Count);
        GL.Uniform1(_fixedUseSpecular,lighting.UseSpecular?1:0); GL.Uniform1(_fixedPower,lighting.Power);
        Vector(_fixedAmbient,lighting.Ambient); Vector(_fixedDiffuse,lighting.Diffuse); Vector(_fixedConstant,lighting.ConstantColor);
        for (int i=0;i<lighting.Lights.Count;++i)
        {
            var light=lighting.Lights[i];
            GL.Uniform1(_fixedLightLocations[i,0],(int)light.Type);
            Vector(_fixedLightLocations[i,1],light.Diffuse); Vector(_fixedLightLocations[i,2],light.Specular);
            Vector(_fixedLightLocations[i,3],light.Position); Vector(_fixedLightLocations[i,4],light.Direction);
            Vector(_fixedLightLocations[i,5],light.Attenuation);
            GL.Uniform2(_fixedLightLocations[i,6],light.Inner,light.Outer);
        }
        GL.Uniform1(_fixedLightingEnabled,1);
    }

    // GLSL translation of the shipped PC Shaders/Fixed.rfx vertex lighting.
    // This is the modern GPU backend, not a second CPU engine implementation.
    // The vs_1_1 lit operation uses positive dot products and exponent clamp:
    // https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/lit---vs
    private const string FixedLightingVertexSource = """
        uniform int uFixedLightingEnabled;
        uniform mat4 uFixedLightingView;
        uniform int uFixedLightingMode;
        uniform int uFixedLightCount;
        uniform int uFixedUseSpecular;
        uniform vec4 uFixedAmbient;
        uniform vec4 uFixedDiffuse;
        uniform vec4 uFixedConstant;
        uniform float uFixedPower;
        uniform int uRenderMode;
        uniform int uFixedLightType[8];
        uniform vec4 uFixedLightDiffuse[8];
        uniform vec4 uFixedLightSpecular[8];
        uniform vec4 uFixedLightPosition[8];
        uniform vec4 uFixedLightDirection[8];
        uniform vec4 uFixedLightAttenuation[8];
        uniform vec2 uFixedLightCone[8];
        out vec4 vFixedColor;

        vec2 FixedLit(float diffuseDot, float halfDot)
        {
            float diffuse = max(diffuseDot, 0.0);
            float specular = 0.0;
            if (diffuseDot > 0.0 && halfDot > 0.0)
                specular = pow(halfDot, clamp(uFixedPower, -127.9961, 127.9961));
            return vec2(diffuse, specular);
        }

        vec4 FixedLighting(vec4 position, vec3 normal, vec4 vertexColor)
        {
            vec4 result = uFixedAmbient;
            // Host inspection modes deliberately suppress game illumination.
            if (uRenderMode != 0) result = vec4(1.0);
            bool lit = uFixedLightingMode != 0 && uFixedLightingMode != 1 && uFixedLightingMode != 2 && uFixedLightingMode != 6;
            lit = lit && uRenderMode == 0;
            for (int i = 0; lit && i < uFixedLightCount; ++i)
            {
                vec3 direction = uFixedLightDirection[i].xyz;
                vec3 diffuse = uFixedLightDiffuse[i].xyz;
                vec3 specular = uFixedLightSpecular[i].xyz;
                if (uFixedLightType[i] == 0)
                {
                    if (uFixedUseSpecular != 0)
                    {
                        vec3 halfVector = normalize(vec3(0.0, 0.0, -1.0) - direction);
                        vec2 coefficients = FixedLit(dot(normal, -direction), dot(normal, halfVector));
                        result.xyz += diffuse * coefficients.x + specular * coefficients.y;
                    }
                    else
                    {
                        float intensity = dot(normal, -direction);
                        if (intensity > 0.0) result.xyz += diffuse * intensity;
                    }
                }
                else
                {
                    // Fixed.rfx normalizes float4 values BEFORE assigning
                    // their xyz components to a float3. Preserve homogeneous
                    // w through normalization, including the view vector.
                    vec4 delta = uFixedLightPosition[i] - position;
                    vec3 lightDirection = normalize(delta).xyz;
                    float intensity = dot(normal, lightDirection);
                    float spot = 1.0;
                    if (uFixedLightType[i] == 2)
                        spot = clamp((dot(lightDirection, -direction) - uFixedLightCone[i].y) /
                            (uFixedLightCone[i].x - uFixedLightCone[i].y), 0.0, 1.0);
                    if (uFixedUseSpecular != 0)
                    {
                        vec3 halfVector = normalize(normalize(-position).xyz + lightDirection);
                        vec2 coefficients = FixedLit(intensity, dot(normal, halfVector));
                        // The original specular point branch has no attenuation.
                        result.xyz += spot * (diffuse * coefficients.x + specular * coefficients.y);
                    }
                    else if (uFixedLightType[i] == 1)
                    {
                        if (intensity > 0.0)
                        {
                            result.xyz += diffuse * intensity;
                            // Original ordering attenuates the entire accumulated
                            // color, including ambient and all preceding lights.
                            result.xyz *= 1.0 / (uFixedLightAttenuation[i].x +
                                uFixedLightAttenuation[i].y * length(delta.xyz));
                        }
                    }
                    else result.xyz += spot * diffuse * intensity;
                }
            }
            if (uFixedLightingMode == 0) result = vec4(1.0);
            else if (uFixedLightingMode == 1) result = uFixedDiffuse;
            else if (uFixedLightingMode == 2) result = vertexColor;
            else if (uFixedLightingMode == 4) { result += vertexColor; result.w = uFixedDiffuse.w; }
            else if (uFixedLightingMode == 5) { result.w = uFixedDiffuse.w; result *= vertexColor; }
            else if (uFixedLightingMode == 6) result = uFixedConstant;
            else result.w = uFixedDiffuse.w;
            return clamp(result, 0.0, 1.0);
        }
        """;
}
