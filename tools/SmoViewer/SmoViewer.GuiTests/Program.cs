using System.Collections;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media.Media3D;
using SmoViewer;
using SmoViewer.Core;
using SmoViewer.Scene;
using SmoViewer.Sparkplug;

// Exercises the actual window's loading, selection and slider handlers. No
// duplicated production sampler, file picker automation or visible window.
internal static class Program
{
    private static readonly BindingFlags Private = BindingFlags.Instance | BindingFlags.NonPublic;
    private static object? Field(object owner, string name) => owner.GetType().GetField(name, Private)!.GetValue(owner);
    private static object? Property(object owner, string name) => owner.GetType().GetProperty(name)!.GetValue(owner);
    private static object? Call(object owner, string name, params object?[] args) => owner.GetType().GetMethod(name, Private)!.Invoke(owner, args);
    private static string Hash(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));

    [STAThread]
    private static int Main(string[] args)
    {
        if (args is ["--native-transfer-window", var transferTarget, var transferDonor, var transferReference, var transferOutput])
        {
            var transferApp = new Application { ShutdownMode = ShutdownMode.OnExplicitShutdown }; int transferCode = 1;
            transferApp.Dispatcher.BeginInvoke(async () => {
                try { transferCode = await NativeTransferWindowRegression.Run(transferTarget,transferDonor,transferReference,transferOutput); }
                catch (Exception error) { Console.Error.WriteLine(error); }
                finally { transferApp.Shutdown(); }
            });
            transferApp.Run(); Environment.Exit(transferCode); return transferCode;
        }
        if (args is ["--gpu-fog", var fogSource, var fogSky, var fogOutput])
        {
            try { return GpuFogRegression.Run(fogSource,fogSky,fogOutput); }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
        if(args is ["--gpu-sky",var skySource,var skyOutput])
        {
            try{return GpuSkyRegression.Run(skySource,skyOutput);}
            catch(Exception error){Console.Error.WriteLine(error);return 1;}
        }
        if(args.Length==3&&args[0] is "--text-windows" or "--sky-windows")
        {
            var textApp=new Application{ShutdownMode=ShutdownMode.OnExplicitShutdown};int textCode=1;
            textApp.Dispatcher.BeginInvoke(async()=>{
                try{textCode=await TextSceneWindowRegression.Run(args[1],args[2],sky:args[0]=="--sky-windows");}
                catch(Exception error){Console.Error.WriteLine(error);}
                finally{textApp.Shutdown();}
            });
            textApp.Run();Environment.Exit(textCode);return textCode;
        }
        if (args is ["--gpu-alpha", var alphaSource, var alphaOutput])
        {
            try { return GpuAlphaRegression.Run(alphaSource, alphaOutput); }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
        if (args is ["--gpu-lighting", var lightingOutput])
        {
            try { return GpuLightingRegression.Run(lightingOutput); }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
        if (args is ["--gpu-lighting-scene", var lightingSource, var lightingSceneOutput])
        {
            try { return GpuMaterialSceneRegression.Run(lightingSource, lightingSceneOutput, documentLighting:true); }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
        if (args.Length == 3 && args[0] == "--lvl-render-occurrences")
        {
            int result = 1;
            try { result = LvlOccurrenceRendererRegression.Run(args[1], args[2]); }
            catch (Exception error) { Console.Error.WriteLine(error); }
            // GLWpfControl can retain a foreground event thread without a shown
            // Dispatcher loop. This bounded probe owns the entire process.
            Environment.Exit(result);
            return result;
        }
        if (args.Length == 3 && args[0] == "--gpu-material")
        {
            try { return GpuMaterialRegression.Run(args[1], args[2]); }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
        if (args.Length == 3 && args[0] == "--gpu-material-scene")
        {
            try { return GpuMaterialSceneRegression.Run(args[1], args[2]); }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
        if (args.Length == 3 && args[0] == "--gpu-readback")
        {
            try { return GpuReadbackRegression.Run(args[1], args[2]); }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
        if (args.Length == 3 && args[0] == "--importer-fitting-gpu")
        {
            try { return ImporterFittingGpuRegression.Run(args[1], args[2]); }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
        if (args.Length is 2 or 3 && args[0] == "--gpu-skin")
        {
            try { return GpuSkinningRegression.Run(args[1], args.Length == 3 ? args[2] : null); }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
        if (args.Length != 2) throw new ArgumentException("GuiTests EXPORT_INPUT_JSON OUTPUT_DIRECTORY");
        var app = new Application { ShutdownMode = ShutdownMode.OnExplicitShutdown };
        int code = 1;
        app.Dispatcher.BeginInvoke(async () =>
        {
            try { await Run(args[0], args[1]); code = 0; }
            catch (Exception error) { Console.Error.WriteLine(error); }
            finally { app.Shutdown(); }
        });
        app.Run();
        return code;
    }

    private static async Task Run(string inputPath, string output)
    {
        output = Path.GetFullPath(output);
        Directory.CreateDirectory(output);
        using JsonDocument input = JsonDocument.Parse(File.ReadAllBytes(inputPath));
        var timer = Stopwatch.StartNew();
        int checks = 0;
        void Check(bool value, string message)
        {
            checks++;
            if (!value) throw new InvalidDataException(message);
        }
        var window = new MainWindow { ShowInTaskbar = false };
        try
        {
            Check((bool)Field(window, "_gpuRendererAvailable")!, "Actual OpenGL host initialized.");
            using var gpuCapture = new GpuAnimationCapture(window, Check);
            var list = (ListBox)window.FindName("AnimationList");
            var slider = (Slider)window.FindName("AnimationSlider");
            var play = (Button)window.FindName("AnimationPlayPauseButton");
            void Select(string san)
            {
                Call(window, "AddAnimationFile", san, null, "Regression");
                Call(window, "RefreshAnimationList");
                list.SelectedItem = list.Items.Cast<object>().Single(item => string.Equals((string)Property(item, "Path")!,
                    Path.GetFullPath(san), StringComparison.OrdinalIgnoreCase));
            }
            string? loaded = null;
            var results = new List<object>();
            foreach (JsonElement row in input.RootElement.GetProperty("cases").EnumerateArray())
            {
                string referencePath = Path.Combine(Path.GetDirectoryName(inputPath)!, row.GetProperty("reference").GetString()!);
                Check(Hash(referencePath) == row.GetProperty("referenceSha256").GetString(), "Reference hash.");
                using JsonDocument reference = JsonDocument.Parse(File.ReadAllBytes(referencePath));
                string smo = reference.RootElement.GetProperty("smo").GetString()!;
                if (loaded != smo)
                {
                    Check(Hash(smo) == reference.RootElement.GetProperty("smoSha256").GetString(), "SMO hash.");
                    await (Task)Call(window, "LoadSmoFilesAsync", (object)new[] { smo })!;
                    Check((int)Field(window, "_loadedFileCount")! == 1 && (int)Field(window, "_failedFileCount")! == 0,
                        "Actual async model loader completed.");
                    loaded = smo;
                }
                string san = row.GetProperty("san").GetString()!;
                Check(Hash(san) == row.GetProperty("sanSha256").GetString(), "SAN hash.");
                Select(san); // Raises the real selection event.
                Check(Field(window, "_selectedAnimation") is SparkplugAnimationClip clip &&
                    string.Equals(clip.Path, san, StringComparison.OrdinalIgnoreCase), "Selection installed requested SAN.");
                var placementMetadata = new Dictionary<string, object>();
                foreach (DictionaryEntry entry in (IDictionary)Field(window, "_sceneGeometry")!)
                {
                    int sceneIndex = (int)Property(entry.Key, "ObjectIndex")!;
                    var renderMesh = (SmoSceneMesh)Property(entry.Value!, "RenderMesh")!;
                    placementMetadata.Add("placement_" + sceneIndex, new
                    {
                        fileIndex = (int)Property(entry.Key, "FileIndex")!,
                        meshObjectIndex = renderMesh.Mesh.ObjectIndex,
                        sceneObjectIndex = renderMesh.SceneObjectIndex,
                        renderableObjectIndex = renderMesh.RenderableObjectIndex,
                        skinObjectIndex = renderMesh.SkinObjectIndex
                    });
                }
                var samples = new List<object>();
                JsonElement[] all = row.GetProperty("samples").EnumerateArray().ToArray();
                int[] indices = all.Length <= 8 ? Enumerable.Range(0, all.Length).ToArray() :
                    new[] { 0, 1, all.Length/4, all.Length/2, 3*all.Length/4, all.Length-2, all.Length-1 }.Distinct().ToArray();
                foreach (int index in indices)
                {
                    double seconds = all[index].GetProperty("seconds").GetDouble();
                    slider.Value = seconds; // Raises the real slider event.
                    Check(Math.Abs((double)Field(window, "_animationTime")!-seconds) < 1e-6,
                        "Slider updated animation time.");
                    gpuCapture.RefreshCompanions();
                    var geometries = (IDictionary)Field(window, "_sceneGeometry")!;
                    var meshes = new Dictionary<string, double[][]>();
                    foreach (DictionaryEntry entry in geometries)
                    {
                        int sceneIndex = (int)Property(entry.Key, "ObjectIndex")!;
                        var geometry = (MeshGeometry3D)Property(entry.Value!, "Geometry")!;
                        meshes.Add("placement_"+sceneIndex, geometry.Positions.Select(p => new[] { p.X, -p.Z, p.Y }).ToArray());
                    }
                    Check(meshes.Count > 0, "Animation produced actual window mesh positions.");
                    Check(meshes.Keys.ToHashSet().SetEquals(placementMetadata.Keys),
                        "Every captured placement has declared physical-mesh metadata.");
                    AnimationVisualRegression.Check(window, Check);
                    samples.Add(new { seconds, referenceSampleIndex = index, meshes });
                }
                results.Add(new { name = row.GetProperty("name").GetString(), placementMetadata, samples });
                AnimationVisualRegression.CheckVisibilityAndSelection(window, Check);
                Console.WriteLine($"Captured Viewer {row.GetProperty("name").GetString()}: {samples.Count} poses.");
            }

            string previousSan = ((SparkplugAnimationClip)Field(window, "_selectedAnimation")!).Path;
            byte[] staticBytes = File.ReadAllBytes(previousSan);
            SmoDocument staticDocument = SmoDocument.Parse(staticBytes);
            Check(SmoDataBlockReader.TryReadHeader(staticBytes, (int)staticDocument.Objects.Single().PhysicalOffset+8,
                out var durationField) && durationField.FieldType == 0 && durationField.PayloadSize == 4,
                "Synthetic zero-duration fixture locates the real duration field.");
            Array.Clear(staticBytes, durationField.PayloadOffset, 4);
            string staticPath = Path.Combine(output, "static-duration.san");
            File.WriteAllBytes(staticPath, staticBytes);
            Select(staticPath);
            Call(window, "AnimationPlayPause_Click", play, new RoutedEventArgs());
            Check(Field(window, "_selectedAnimation") is SparkplugAnimationClip { Duration: 0 } &&
                !play.IsEnabled && !(bool)Field(window, "_animationPlaying")! && slider.Maximum == 0,
                "Zero-duration clip remains a static pose with playback disabled.");
            Select(previousSan);
            // Invalid selection must not keep playing the previously selected clip.
            Call(window, "AnimationPlayPause_Click", play, new RoutedEventArgs());
            Check((bool)Field(window, "_animationPlaying")!, "Play button starts a nonzero clip.");
            string invalid = Path.Combine(output, "invalid.san");
            File.WriteAllBytes(invalid, "invalid SAN"u8.ToArray());
            Select(invalid);
            Check(Field(window, "_selectedAnimation") is null && !(bool)Field(window, "_animationPlaying")!,
                "Invalid selection clears active playback and reports its error.");
            Check(((FrameworkElement)window.FindName("AnimationTimeline")).Visibility == Visibility.Collapsed,
                "Invalid clip cannot retain an active timeline.");
            JsonElement firstCase = input.RootElement.GetProperty("cases")[0];
            JsonElement lastCase = input.RootElement.GetProperty("cases").EnumerateArray().Last();
            string Model(JsonElement row)
            {
                using JsonDocument reference = JsonDocument.Parse(File.ReadAllBytes(Path.Combine(
                    Path.GetDirectoryName(inputPath)!, row.GetProperty("reference").GetString()!)));
                return reference.RootElement.GetProperty("smo").GetString()!;
            }
            await (Task)Call(window, "LoadSmoFilesAsync", (object)new[] { Model(firstCase), Model(lastCase) })!;
            Check((int)Field(window, "_loadedFileCount")! == 2, "Both models loaded for the cross-file selection regression.");
            Select(firstCase.GetProperty("san").GetString()!);
            AnimationVisualRegression.CheckForeignSelection(window, Check);
            timer.Stop();
            File.WriteAllText(Path.Combine(output, "capture.json"), JsonSerializer.Serialize(new
            {
                status = "captured", checks, inputSha256 = Hash(inputPath), cases = results,
                elapsedSeconds = timer.Elapsed.TotalSeconds, peakWorkingSet = Process.GetCurrentProcess().PeakWorkingSet64,
                gpu = new { gpuCapture.Device, gpuCapture.Version, gpuCapture.Frames, gpuCapture.RefreshedMeshes },
                scope = "Actual WPF model-load, SAN selection and slider handlers; production hidden OpenGL frames and shader readback refresh skinned companions before the unchanged external-reference capture. No OS dialogs or GPU pixels asserted."
            }));
            Console.WriteLine($"PASS Viewer handlers: {checks} checks, {timer.Elapsed.TotalSeconds:F3}s.");
        }
        finally { window.Close(); }
    }
}
