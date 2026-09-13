using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using System.Windows.Controls;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class NativeTransferWindowRegression
{
    private const BindingFlags Hidden = BindingFlags.Instance | BindingFlags.NonPublic;
    private static object? Get(object owner,string name) => owner.GetType().GetField(name,Hidden)!.GetValue(owner);
    private static object? Call(object owner,string name,params object?[] args) => owner.GetType().GetMethod(name,Hidden)!.Invoke(owner,args);
    private static T Property<T>(object owner,string name) => (T)owner.GetType().GetProperty(name,Hidden)!.GetValue(owner)!;
    private static string Hash(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));

    internal static async Task<int> Run(string targetPath,string donorPath,string referencePath,string output)
    {
        if (Directory.Exists(output)) throw new IOException("Fresh native transfer window output required.");
        Directory.CreateDirectory(output);
        int checks = 0; string? failure = null; object? detail = null; var watch = Stopwatch.StartNew();
        void Check(bool value,string message) { ++checks; if (!value) throw new InvalidDataException(message); }
        var window = new SmoImporter.Gui.MainWindow { ShowInTaskbar = false };
        try
        {
            string targetHash = Hash(targetPath), donorHash = Hash(donorPath), referenceHash = Hash(referencePath);
            var donorResources = SmoLoadedResources.Get(SmoDocument.Load(donorPath));
            bool layered = donorResources.Models.Values.Any(model => model.Material is {} material &&
                (material.Passes.Count > 1 || material.Passes.Any(pass => pass.Layers.Count > 1)));
            bool layeredRejected = false; ImportedScene? strict = null;
            try { strict = SmoModelReader.Read(donorPath); }
            catch (InvalidDataException error) when (error.Message.StartsWith("MATERIAL_IMPORT_SHAPE")) { layeredRejected = true; }
            Check(layeredRejected == layered, "Flat authoring reader retains its actual material-shape boundary");
            Call(window,"LoadSource",targetPath);
            var target = Get(window,"_document") as SmoDocument;
            Check(target is not null, "Actual window target loader succeeds");
            await (Task)Call(window,"LoadExternalReplacementAsync",donorPath)!;
            var preview = (ImportedScene)Get(window,"_replacementScene")!;
            Check(preview is { Meshes.Count: > 0, HasSkinning: true }, "Actual native donor loader produces geometry and skeleton preview");
            Check(preview.Materials.Count == 0 && preview.Textures.Count == 0 && preview.Meshes.All(mesh => mesh.MaterialIndex == -1),
                "Geometry preview has no misleading flattened material or decoded PNG catalog");
            Check(preview.ImportWarnings.Any(w => w.StartsWith("NATIVE_GEOMETRY_PREVIEW")), "Preview scope is explicit");
            if (strict is not null)
            {
                Check(strict.Meshes.Count == preview.Meshes.Count && strict.Meshes.Zip(preview.Meshes).All(pair =>
                    pair.First.Positions.AsSpan().SequenceEqual(pair.Second.Positions) &&
                    pair.First.Normals.AsSpan().SequenceEqual(pair.Second.Normals) &&
                    pair.First.TriangleIndices.AsSpan().SequenceEqual(pair.Second.TriangleIndices) &&
                    pair.First.TextureCoordinates.AsSpan().SequenceEqual(pair.Second.TextureCoordinates) &&
                    pair.First.SecondaryTextureCoordinates.AsSpan().SequenceEqual(pair.Second.SecondaryTextureCoordinates)),
                    "Ordinary donor geometry matches the established full reader exactly");
                Check(strict.Meshes.Zip(preview.Meshes).All(pair => pair.First.Skinning is {} before && pair.Second.Skinning is {} after &&
                    before.Weights.AsSpan().SequenceEqual(after.Weights) && before.JointIndices.AsSpan().SequenceEqual(after.JointIndices) &&
                    before.Skeleton.JointNames.SequenceEqual(after.Skeleton.JointNames) &&
                    before.Skeleton.InverseBindMatrices.SequenceEqual(after.Skeleton.InverseBindMatrices)),
                    "Ordinary donor skin mapping and raw weights stay unchanged");
            }
            var plan = (SmoNativeVisualGraphPlan)Get(window,"_nativeSmoVisualPlan")!;
            Check(plan is { CanReplace: true }, "Actual window reaches its native forest transfer plan");
            Check(Call(window,"GetPortingModeBlockMessage") is null && Property<bool>(window,"CanRunSelectedPortingPipeline"),
                "Material shape no longer blocks the native operation");
            Call(window,"Plan_Click",Get(window,"PlanButton"),new System.Windows.RoutedEventArgs());
            Check(((Button)Get(window,"SaveButton")!).IsEnabled, "Actual Save control is enabled after successful native planning");
            Check(!Property<bool>(window,"CanShowFinalTexturedPreview"), "Native donor preview remains explicitly geometry-only");
            string previewLabel = ((TextBlock)Get(window,"PortingPreviewStatusText")!).Text;
            Check(previewLabel.Contains("без материалов"), "Window describes the material-free preview");
            // Use the same core operation as Save, without invoking a file picker.
            string saved = Path.Combine(output,"transferred.smo");
            SmoNativeVisualGraphReplacer.Replace(target!,SmoDocument.Load(donorPath),saved);
            Check(Hash(saved) == referenceHash, "Window donor selection yields the already-qualified full native output byte-for-byte");
            Check(SmoLoadedResources.Get(SmoDocument.Load(saved)).LoadIssue is null, "Saved graph loads through the shared reader");
            Check(targetHash == Hash(targetPath) && donorHash == Hash(donorPath) && referenceHash == Hash(referencePath), "All input files remain unchanged");
            detail = new { targetHash, donorHash, referenceHash, layered, previewMeshes = preview.Meshes.Count,
                previewVertices = preview.Meshes.Sum(mesh => mesh.Positions.Length), plan.MaterialCount, plan.MeshCount,
                previewLabel, importerCore = Hash(typeof(SmoModelReader).Assembly.Location),
                importerGui = Hash(typeof(SmoImporter.Gui.MainWindow).Assembly.Location),
                native = Hash(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")) };
        }
        catch (Exception error) { failure = error.ToString(); }
        finally { window.Close(); }
        File.WriteAllText(Path.Combine(output,"report.json"),JsonSerializer.Serialize(new { targetPath, donorPath, referencePath,
            checks, failure, detail, seconds = watch.Elapsed.TotalSeconds, peakBytes = Process.GetCurrentProcess().PeakWorkingSet64 },
            new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine(JsonSerializer.Serialize(new { checks, failure, seconds = watch.Elapsed.TotalSeconds }));
        return failure is null ? 0 : 1;
    }
}
