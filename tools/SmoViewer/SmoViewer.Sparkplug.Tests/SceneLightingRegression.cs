using System.Diagnostics;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;
using SmoViewer.Sparkplug;

internal static class SceneLightingRegression
{
    internal static unsafe int Run(string modelPath, string animationPath, string reportPath)
    {
        if (File.Exists(reportPath)) throw new IOException("Use a fresh lighting report path.");
        var watch = Stopwatch.StartNew();
        int checks = 0;
        void Check(bool value, string message) { ++checks; if (!value) throw new InvalidDataException(message); }
        byte[] input = File.ReadAllBytes(modelPath);
        string inputHash = Convert.ToHexString(SHA256.HashData(input));
        Check(inputHash == "744EEDD682725C5712789E13598FB478AB7190CBC022CDB98B291E2E900D485C", "This expected cache belongs to pristine PC Icy.smo");
        var document = SmoDocument.Parse(input);
        var skins = document.Objects.Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .Select(entry => SmoSkinDecoder.TryDecode(document, entry, out var skin, out var error) ? skin! : throw new InvalidDataException(error))
            .ToDictionary(skin => skin.ObjectIndex);
        int lightIndex = document.Objects.Single(entry => entry.Id == 119).Index;
        int renderIndex = document.Objects.Single(entry => entry.Id == 3).Index;
        using var plain = new SparkplugSceneRuntime(document, skins);
        using var lit = new SparkplugSceneRuntime(document, skins, enableDocumentLighting: true);
        var materials=SmoLoadedResources.Get(document).Models.Values.Select(model=>model.Material)
            .Where(material=>material is not null).Select(material=>material!).DistinctBy(material=>material.ObjectIndex).ToArray();
        Check(materials.Length==3,"Pinned Icy has three material owners");
        Check(SmoObjectFieldReader.TryRead(document,document.Objects[lightIndex],out var lightFields,out var lightError),lightError);
        Check(SmoLightDataDecoder.TryDecode(lightFields,out var lightData,out lightError),lightError);
        var shaderFrames=new List<object>();
        void ExpectedShader(bool active)
        {
            foreach(var material in materials)
            {
                var captured=lit.CaptureShaderLighting(renderIndex,material.ObjectIndex,Matrix4x4.Identity,0xcc336699);
                Check(captured.KnownMask==15&&captured.Lights.Count==0,"Ambient shader projection retains actual empty ordinary cache");
                Check(captured.ColorMode==material.RenderStates[8]&&captured.Diffuse==material.Colors[1],"Current material coloring and diffuse are original producer values");
                Check(captured.Ambient==(active?lightData!.ColorRgba*material.Colors[0]:Vector4.Zero),"Selected ambient uses native color multiplied by original material ambient");
                Check(new[]{captured.ConstantColor.X,captured.ConstantColor.Y,captured.ConstantColor.Z,captured.ConstantColor.W}
                    .Select(BitConverter.SingleToUInt32Bits).SequenceEqual(new uint[]{0x3e4cccce,0x3eccccce,0x3f19999a,0x3f4cccce}),
                    "Native packed constant-color conversion retains PC float32 reciprocal rounding");
                Check(captured.Power==material.SpecularPower&&captured.UseSpecular==(material.SpecularPower>0),"Original positive-power shader branch");
                shaderFrames.Add(new {material.ObjectId,active,colorMode=captured.ColorMode,
                    ambient=new[]{captured.Ambient.X,captured.Ambient.Y,captured.Ambient.Z,captured.Ambient.W},
                    diffuse=new[]{captured.Diffuse.X,captured.Diffuse.Y,captured.Diffuse.Z,captured.Diffuse.W},captured.Power});
            }
        }
        void ExpectedAmbient(bool active)
        {
            Check(lit.LightSelections.TryGetValue(renderIndex, out var selected), "Cache belongs to actual RenderNode ID3, not Skin ID4");
            Check(selected!.OrdinaryObjectIndices.Count == 0 && selected.AmbientObjectIndex == (active ? lightIndex : null), "Icy ambient selection uses actual loaded Light ID119");
        }
        Check(!plain.LightingConfigured && plain.LightSelections.Count == 0, "Existing constructor keeps lighting explicit");
        Check(lit.AvailableLightObjectIndices.SequenceEqual(new[] { lightIndex }), "Actual Light discovery, without managed wire-class guesses");
        ExpectedAmbient(true);
        Check(plain.Worlds.All(pair => lit.Worlds[pair.Key] == pair.Value), "Lighting activation preserves original Node world matrices");
        using var clip = SparkplugAnimationClip.Load(animationPath);
        plain.Bind(clip); lit.Bind(clip);
        var frames = new List<object>();
        string? previousMiddle = null;
        foreach (float time in new[] { 0f, clip.Duration * .5f, clip.Duration, 0f, clip.Duration * .5f })
        {
            plain.Sample(time); lit.Sample(time); ExpectedAmbient(true); ExpectedShader(true);
            Check(plain.Worlds.All(pair => lit.Worlds[pair.Key] == pair.Value), "Lighting does not change animated world matrices");
            Check(skins.Keys.All(index => plain.Palette(index).SequenceEqual(lit.Palette(index))), "Lighting does not change Skin palettes");
            var matrices = lit.Worlds.OrderBy(pair => pair.Key).Select(pair => pair.Value).ToArray();
            string hash = Convert.ToHexString(SHA256.HashData(MemoryMarshal.AsBytes(matrices.AsSpan())));
            if (time == clip.Duration * .5f)
            {
                if (previousMiddle != null) Check(previousMiddle == hash, "Backward seeking keeps pose independent of frame history");
                previousMiddle = hash;
            }
            frames.Add(new { time, worldMatrixSha256 = hash, ambientObjectId = 119, ordinaryCount = 0 });
        }
        lit.SetLightingActive(false); ExpectedAmbient(false); ExpectedShader(false);
        lit.SetLightingActive(true); ExpectedAmbient(true);
        try { lit.EnableDocumentLighting(); throw new Exception("Implicit replacement accepted"); }
        catch (InvalidOperationException) { Check(true, "Managed reconfiguration requires explicit clearing"); }
        ExpectedAmbient(true);
        lit.ClearLighting(); Check(!lit.LightingConfigured && lit.LightSelections.Count == 0, "Managed cache clears with native owner");
        lit.ConfigureLighting([], true); ExpectedAmbient(false);
        lit.ClearLighting(); lit.ClearLighting();
        lit.EnableDocumentLighting(); ExpectedAmbient(true);

        Check(Marshal.SizeOf<NativeMethods.SceneLightCache>() == 44, "Lighting C ABI row size");
        Check(Marshal.SizeOf<NativeMethods.ShaderLight>()==96&&Marshal.SizeOf<NativeMethods.ShaderLighting>()==836,"Shader constant ABI sizes");
        GraphHandle graph;
        fixed (byte* bytes = input) graph = new(NativeMethods.Check(NativeMethods.spv_graph_load(bytes, (uint)input.Length)));
        using (graph)
        using (var owner = new SceneHandle(NativeMethods.Check(NativeMethods.spv_graph_scene_all(graph))))
        using (var contender = new SceneHandle(NativeMethods.Check(NativeMethods.spv_graph_scene_all(graph))))
        {
            NativeMethods.Check(NativeMethods.spv_scene_node_count(owner, out uint nodeCount));
            var nodeIds = new uint[nodeCount];
            fixed (uint* output = nodeIds) NativeMethods.Check(NativeMethods.spv_scene_graph_node_ids(owner, output, nodeCount));
            uint[] hierarchy = nodeIds.Select(id => { NativeMethods.Check(NativeMethods.spv_graph_node(graph, id, out var node)); return node.Flags & 0x100; }).ToArray();
            void HierarchyRestored()
            {
                Check(nodeIds.Select(id => { NativeMethods.Check(NativeMethods.spv_graph_node(graph, id, out var node)); return node.Flags & 0x100; }).SequenceEqual(hierarchy), "Context teardown restores original hierarchy bits");
            }
            uint* ids = stackalloc uint[2]; ids[0] = 119; ids[1] = 119;
            Check(NativeMethods.spv_scene_lighting_configure(owner, ids, 2, 1) == 0, "Repeated Light rejected before graph mutation");
            HierarchyRestored();
            ids[0] = 4;
            Check(NativeMethods.spv_scene_lighting_configure(owner, ids, 1, 1) == 0, "Skin is not a Light");
            ids[0] = uint.MaxValue;
            Check(NativeMethods.spv_scene_lighting_configure(owner, ids, 1, 1) == 0, "Unknown ID rejected");
            ids[0] = 119;
            Check(NativeMethods.spv_scene_lighting_configure(owner, null, 1, 1) == 0 && NativeMethods.spv_scene_lighting_configure(owner, ids, 1, 2) == 0, "Null input and non-Boolean activation rejected");
            Check(NativeMethods.spv_scene_lighting_caches(owner, null, 0, out _) == 0, "Unconfigured cache cannot masquerade as a scene result");
            NativeMethods.Check(NativeMethods.spv_scene_lighting_configure(owner, ids, 1, 1));
            var shaderView=Matrix4x4.Identity;
            NativeMethods.ShaderLighting shaderRow=default;shaderRow.Known=0x12345678;
            Check(NativeMethods.spv_scene_shader_lighting(owner,4,5,(float*)&shaderView,0,out shaderRow)==0&&shaderRow.Known==0x12345678,"Skin cannot replace RenderNode cache; output stays atomic");
            Check(NativeMethods.spv_scene_shader_lighting(owner,3,4,(float*)&shaderView,0,out shaderRow)==0,"Skin cannot replace Material");
            Check(NativeMethods.spv_scene_shader_lighting(owner,3,5,null,0,out shaderRow)==0,"Null shader view rejected");
            shaderView.M11=float.NaN;
            Check(NativeMethods.spv_scene_shader_lighting(owner,3,5,(float*)&shaderView,0,out shaderRow)==0,"Non-finite shader view rejected");
            shaderView=Matrix4x4.Identity;
            NativeMethods.Check(NativeMethods.spv_scene_shader_lighting(owner,3,5,(float*)&shaderView,0,out shaderRow));
            Check(shaderRow.Known==15&&shaderRow.Count==0,"Original RenderNode cache captured");
            Check(NativeMethods.spv_scene_lighting_configure(contender, ids, 1, 1) == 0, "Two contexts cannot own the same graph's borrowed links");
            Check(NativeMethods.spv_scene_lighting_configure(owner, ids, 1, 1) == 0, "Native reconfiguration preserves the existing owner");
            var worlds = new Matrix4x4[nodeCount];
            fixed (Matrix4x4* output = worlds) NativeMethods.Check(NativeMethods.spv_scene_sample(owner, 0, output, nodeCount * 16));
            NativeMethods.Check(NativeMethods.spv_scene_lighting_caches(owner, null, 0, out uint caches));
            var rows = new NativeMethods.SceneLightCache[caches + 1];
            fixed (NativeMethods.SceneLightCache* output = rows)
            {
                output[0].RenderNode = 0xDEADBEEF; output[caches].RenderNode = 0xC0FFEE;
                Check(NativeMethods.spv_scene_lighting_caches(owner, output, caches - 1, out _) == 0 && output[0].RenderNode == 0xDEADBEEF, "Short output rejected without partial write");
                NativeMethods.Check(NativeMethods.spv_scene_lighting_caches(owner, output, caches, out _));
                Check(output[caches].RenderNode == 0xC0FFEE, "Cache read stays within caller capacity");
            }
            var actual = rows.Take((int)caches).Single(row => row.RenderNode == 3);
            Check(actual.Count == 0 && actual.Ambient == 119, "Native cache preserves original ambient/ordinary separation");
            bool zeroUnused = true; for (int i = 0; i < 8; ++i) zeroUnused &= actual.Lights[i] == 0;
            Check(zeroUnused, "C ABI does not expose unused stale native slots");
            NativeMethods.Check(NativeMethods.spv_scene_lighting_clear(owner)); HierarchyRestored();
            NativeMethods.Check(NativeMethods.spv_scene_lighting_configure(contender, ids, 1, 1));
            contender.Dispose(); HierarchyRestored();
            NativeMethods.Check(NativeMethods.spv_scene_lighting_configure(owner, ids, 1, 1));
            graph.Dispose(); // scene retains the graph and all borrowed targets
            fixed (Matrix4x4* output = worlds) NativeMethods.Check(NativeMethods.spv_scene_sample(owner, 0, output, nodeCount * 16));
            NativeMethods.Check(NativeMethods.spv_scene_lighting_caches(owner, null, 0, out uint retainedCaches));
            Check(caches == retainedCaches, "Scene remains usable after outer graph handle disposal");
            NativeMethods.Check(NativeMethods.spv_scene_shader_lighting(owner,3,5,(float*)&shaderView,0,out shaderRow));
            Check(shaderRow.Known==15,"Shader projection retains graph ownership after outer handle disposal");
        }
        Check(Convert.ToHexString(SHA256.HashData(input)) == inputHash, "Graph and scene preserve source bytes");
        var binaries = new[] { typeof(SparkplugSceneRuntime).Assembly.Location, typeof(NativeMethods).Assembly.Location, typeof(SceneLightingRegression).Assembly.Location, Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll") }
            .Distinct().Select(path => new { path, sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))) }).ToArray();
        var report = new { checks, elapsedSeconds = watch.Elapsed.TotalSeconds, peakWorkingSet = Process.GetCurrentProcess().PeakWorkingSet64,
            model = new { path = Path.GetFullPath(modelPath), sha256 = inputHash }, animation = new { path = Path.GetFullPath(animationPath), sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(animationPath))) },
            frames, shaderFrames, binaries, scope = "HOST lighting ownership/scheduling and shader constant transport; reconstructed selection/producers. No full original Scene frame or GPU-lighting equivalence claim." };
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(reportPath))!);
        File.WriteAllText(reportPath, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"PASS scene lighting: {checks} checks, {watch.Elapsed.TotalSeconds:F3}s, peak {report.peakWorkingSet} bytes");
        return 0;
    }
}
