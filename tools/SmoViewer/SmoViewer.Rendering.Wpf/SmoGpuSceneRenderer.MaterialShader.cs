namespace SmoViewer.Rendering.Wpf;

public sealed partial class SmoGpuSceneRenderer
{
    // Modern GL implementation of the mapped texture-stage device contract.
    // No original D3D device/runtime is loaded. Texture argument defaults are
    // TEXTURE/CURRENT; NULL texture or COLOROP_DISABLE terminates the chain.
    // https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop
    // https://learn.microsoft.com/en-us/windows/win32/direct3d9/texture-blending
    private const string MaterialFragmentShaderSource = """
        #version 330 core
        in vec2 vUv;
        in vec2 vUv1;
        in vec4 vColor;
        in vec3 vNormal;
        in vec4 vStageUV[8];
        in vec4 vFixedColor;
        uniform int uFixedLightingEnabled;
        uniform sampler2D uTexture;
        uniform sampler2D uBaseTexture;
        uniform int uHasTexture;
        uniform int uHasBaseTexture;
        uniform int uHighlight;
        uniform int uLuminanceCoverage;
        uniform float uOpacity;
        uniform int uRenderMode;
        uniform int uMaterialEnabled;
        uniform int uMaterialStageCount;
        uniform int uMaterialColorMode;
        uniform vec4 uMaterialDiffuse;
        uniform vec4 uMaterialEmissive;
        uniform int uMaterialAlphaTest;
        uniform float uMaterialAlphaReference;
        uniform int uMaterialAlphaFunction;
        uniform sampler2D uStageTexture0;
        uniform sampler2D uStageTexture1;
        uniform sampler2D uStageTexture2;
        uniform sampler2D uStageTexture3;
        uniform sampler2D uStageTexture4;
        uniform sampler2D uStageTexture5;
        uniform sampler2D uStageTexture6;
        uniform sampler2D uStageTexture7;
        uniform int uStageColor[8];
        uniform int uStageAlpha[8];
        uniform int uStagePresent[8];
        uniform int uStageTransformFlags[8];
        out vec4 FragColor;
        """ + "\n" + FogFragmentSource + "\n" + """

        vec4 StageTexture(int stage, vec2 uv)
        {
            // GLSL330 sampler array indexing requires a constant expression.
            if (stage == 0) return texture(uStageTexture0, uv);
            if (stage == 1) return texture(uStageTexture1, uv);
            if (stage == 2) return texture(uStageTexture2, uv);
            if (stage == 3) return texture(uStageTexture3, uv);
            if (stage == 4) return texture(uStageTexture4, uv);
            if (stage == 5) return texture(uStageTexture5, uv);
            if (stage == 6) return texture(uStageTexture6, uv);
            return texture(uStageTexture7, uv);
        }

        vec4 Combine(int operation, vec4 a, vec4 b, vec4 diffuse)
        {
            if (operation == 2) return a;
            if (operation == 3) return b;
            if (operation == 4) return a * b;
            if (operation == 5) return 2.0 * a * b;
            if (operation == 6) return 4.0 * a * b;
            if (operation == 7) return a + b;
            if (operation == 10) return a - b;
            if (operation == 12) return mix(b, a, diffuse.a);
            if (operation == 13) return mix(b, a, a.a);
            if (operation == 15) return a + b * (1.0 - a.a);
            if (operation == 16) return mix(b, a, b.a);
            if (operation == 18) return a + b * a.a;
            if (operation == 19) return a * b + a.a;
            if (operation == 20) return a + b * (1.0 - a.a);
            if (operation == 21) return (1.0 - a) * b + a.a;
            return b; // unsupported operations are refused before drawing
        }

        bool AlphaPass(float a)
        {
            if (uMaterialAlphaFunction == 1) return false;
            if (uMaterialAlphaFunction == 2) return a < uMaterialAlphaReference;
            if (uMaterialAlphaFunction == 3) return a == uMaterialAlphaReference;
            if (uMaterialAlphaFunction == 4) return a <= uMaterialAlphaReference;
            if (uMaterialAlphaFunction == 5) return a > uMaterialAlphaReference;
            if (uMaterialAlphaFunction == 6) return a != uMaterialAlphaReference;
            if (uMaterialAlphaFunction == 7) return a >= uMaterialAlphaReference;
            return true;
        }

        float PreviewLighting()
        {
            // Existing editor light, pending the actual scene-light shader
            // consumer. Unlit/wireframe deliberately omit preview lighting.
            if (uRenderMode != 0) return 1.0;
            return 0.38 + 0.62 * abs(dot(normalize(vNormal), normalize(vec3(0.35, 0.80, -0.48))));
        }

        void main()
        {
            vec4 result;
            if (uMaterialEnabled != 0)
            {
                vec4 base = vec4(vec3(PreviewLighting()), uMaterialDiffuse.a);
                // Coloring modes follow the supplied original raw mode.
                // The lighting term itself is still the explicit editor light.
                if (uMaterialColorMode == 0) base = vec4(1.0);
                else if (uMaterialColorMode == 1 || uMaterialColorMode == 6) base = uMaterialEmissive;
                else if (uMaterialColorMode == 2) base = vColor;
                else if (uMaterialColorMode == 4) { base.rgb += vColor.rgb; base.a = uMaterialDiffuse.a; }
                else if (uMaterialColorMode == 5) base *= vColor;
                if (uFixedLightingEnabled != 0) base = vFixedColor;
                base = clamp(base, 0.0, 1.0);
                result = base;
                for (int s = 0; s < uMaterialStageCount; ++s)
                {
                    if (uStageColor[s] == 1 || uStagePresent[s] == 0) break;
                    vec4 coordinate = vStageUV[s];
                    vec2 uv = coordinate.xy;
                    if ((uStageTransformFlags[s] & 256) != 0)
                    {
                        int count = uStageTransformFlags[s] & 7;
                        float divisor = count == 2 ? coordinate.y : count == 3 ? coordinate.z : coordinate.w;
                        uv /= divisor;
                    }
                    vec4 texel = StageTexture(s, uv);
                    vec4 color = Combine(uStageColor[s], texel, result, base);
                    vec4 alpha = Combine(uStageAlpha[s], texel, result, base);
                    result = clamp(vec4(color.rgb, alpha.a), 0.0, 1.0);
                }
                if (uMaterialAlphaTest != 0 && !AlphaPass(result.a)) discard;
            }
            else
            {
                // Host-created geometry/overlays without a game material.
                vec4 source;
                if (uHasBaseTexture != 0 && uHasTexture != 0)
                {
                    vec4 base = texture(uBaseTexture, vUv);
                    vec4 effect = texture(uTexture, vUv1);
                    source = vec4(min(vec3(1.0), base.rgb + effect.rgb * effect.a), base.a);
                }
                else source = uHasTexture != 0 ? texture(uTexture, vUv) : vec4(1.0);
                result = source * vColor;
                result.rgb *= PreviewLighting();
                if (uLuminanceCoverage != 0) result.a *= max(result.r, max(result.g, result.b));
                if (result.a <= 0.001) discard;
            }
            result = ApplyPixelFog(result);
            result.a *= uOpacity;
            if (uHighlight != 0) result.rgb = mix(result.rgb, vec3(1.0, 0.55, 0.10), 0.62);
            FragColor = result;
        }
        """;
}
