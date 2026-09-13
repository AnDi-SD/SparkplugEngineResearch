using System.Security.Cryptography;
using System.Text.Json;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Avalonia.VisualTree;
using SixLabors.ImageSharp.PixelFormats;
using SMOTextureTool;
using TextureDocument = SMOTextureTool.Core.SmoDocument;

internal static class Program
{
    [STAThread]
    public static int Main(string[] args)
    {
        if (args.Length != 2)
            throw new ArgumentException("GuiTests SOURCE_SMO OUTPUT_DIRECTORY");
        AppBuilder.Configure<App>().UsePlatformDetect().WithInterFont().SetupWithoutStarting();
        using var lifetime = new CancellationTokenSource();
        int result = 1;
        Dispatcher.UIThread.Post(async () =>
        {
            try { await Run(args[0], args[1]); result = 0; }
            catch (Exception error) { Console.Error.WriteLine(error); }
            finally { lifetime.Cancel(); }
        });
        Dispatcher.UIThread.MainLoop(lifetime.Token);
        return result;
    }

    private static async Task Run(string source, string outputDirectory)
    {
        Directory.CreateDirectory(outputDirectory);
        byte[] original = File.ReadAllBytes(source);
        int checks = 0;
        void Check(bool condition, string message)
        {
            checks++;
            if (!condition) throw new InvalidDataException(message);
        }
        var window = new MainWindow();
        window.LoadDocument(Path.GetFullPath(source));
        Check(window.Rows.Count > 0 && window.HasDocument && !window.HasReplacements, "GUI opens supported texture slots");
        var row = window.Rows.First(item => item.Texture.CanReplace);
        foreach (string mode in window.PreviewModes)
        {
            window.SelectedPreviewMode = mode;
            Check(row.OriginalPreview.PixelSize.Width > 0, $"GUI preview mode: {mode}");
        }
        window.SelectedPreviewMode = "RGBA на шахматном фоне";
        string fixedPng = Path.GetFullPath(Path.Combine(outputDirectory, "replacement-fixed.png"));
        using (var fixedImage = new SixLabors.ImageSharp.Image<Rgba32>(row.Texture.Width, row.Texture.Height, new Rgba32(80, 110, 160, 90)))
        using (var png = File.Create(fixedPng))
            fixedImage.Save(png, new SixLabors.ImageSharp.Formats.Png.PngEncoder());
        window.SelectReplacement(row, fixedPng);
        string fixedCopy = Path.GetFullPath(Path.Combine(outputDirectory, "gui-fixed.smo"));
        await window.SaveCopyAsync(fixedCopy);
        byte[] fixedBytes = File.ReadAllBytes(fixedCopy);
        var fixedDoc = TextureDocument.Parse(fixedBytes);
        using (var fixedPixels = fixedDoc.Decode(fixedDoc.Textures.Single(item => item.ObjectIndex == row.Texture.ObjectIndex)))
            Check(fixedBytes.Length == original.Length && fixedPixels.Width == row.Texture.Width &&
                fixedPixels.Height == row.Texture.Height && fixedPixels[0, 0] == new Rgba32(80, 110, 160, 90),
                "GUI fixed-size replacement preserves dimensions and copies RGBA");
        string replacement = Path.GetFullPath(Path.Combine(outputDirectory, "replacement-13x7.png"));
        using (var image = new SixLabors.ImageSharp.Image<Rgba32>(13, 7, new Rgba32(50, 130, 180, 70)))
        using (var png = File.Create(replacement))
            image.Save(png, new SixLabors.ImageSharp.Formats.Png.PngEncoder());
        window.SelectReplacement(row, replacement);
        Check(window.HasReplacements && row.HasReplacement && row.ReplacementPreview is not null, "GUI selects replacement and preview");
        window.Measure(new Size(1280, 820));
        window.Arrange(new Rect(0, 0, 1280, 820));
        window.UpdateLayout();
        var content = (Control)window.Content!;
        content.Width = 1280;
        content.Height = 820;
        content.Measure(new Size(1280, 820));
        content.Arrange(new Rect(0, 0, 1280, 820));
        content.UpdateLayout();
        Button[] buttons = window.GetVisualDescendants().OfType<Button>().ToArray();
        Check(buttons.Any(button => Equals(button.Content, "Сохранить копию SMO") && button.IsEnabled), "save button binding is enabled");
        Check(buttons.Any(button => Equals(button.Content, "Папка с заменами") && button.IsEnabled), "replacement-folder button binding is enabled");
        using (var capture = new RenderTargetBitmap(new PixelSize(1280, 820), new Vector(96, 96)))
        {
            capture.Render(content);
            capture.Save(Path.Combine(outputDirectory, "texture-tool-window.png"));
        }
        using (var rendered = SixLabors.ImageSharp.Image.Load<Rgba32>(Path.Combine(outputDirectory, "texture-tool-window.png")))
        {
            var colors = new HashSet<Rgba32>();
            for (int y = 0; y < rendered.Height; y += 8)
                for (int x = 0; x < rendered.Width; x += 8)
                    colors.Add(rendered[x, y]);
            Check(colors.Count > 8, "offscreen GUI capture contains rendered controls");
        }
        string output = Path.GetFullPath(Path.Combine(outputDirectory, "gui-copy.smo"));
        var installed = await window.SaveCopyAsync(output);
        byte[] saved = File.ReadAllBytes(output);
        Check(Hash(saved) == installed.Sha256 && window.IsIdle, "GUI save completes verified atomic output");
        var doc = TextureDocument.Parse(saved);
        var texture = doc.Textures.Single(item => item.ObjectIndex == row.Texture.ObjectIndex);
        using (var decoded = doc.Decode(texture))
            Check(decoded.Width == 13 && decoded.Height == 7 && decoded[0, 0] == new Rgba32(50, 130, 180, 70), "GUI output dimensions and RGBA");
        var again = await window.SaveCopyAsync(output);
        Check(again.BackupPath is not null && File.ReadAllBytes(again.BackupPath).AsSpan().SequenceEqual(saved), "saving an existing copy retains an exact backup");
        bool protectedSource = false;
        try { await window.SaveCopyAsync(source); }
        catch (InvalidOperationException) { protectedSource = true; }
        Check(protectedSource && File.ReadAllBytes(source).AsSpan().SequenceEqual(original) && window.IsIdle, "GUI refuses source overwrite and recovers busy state");
        window.LoadDocument(output);
        Check(window.Rows.Any(item => item.Texture.Width == 13 && item.Texture.Height == 7) && !window.HasReplacements, "GUI reopens its saved copy");
        File.WriteAllText(Path.Combine(outputDirectory, "report.json"), JsonSerializer.Serialize(new
        {
            kind = "texture-tool-gui-workflow-regression", schemaVersion = 1, checks,
            scope = "In-process Avalonia controls, previews and save workflow; OS file-picker dialogs are not automated.",
            sourcePath = Path.GetFullPath(source), sourceSha256 = Hash(original),
            outputPath = output, outputSha256 = Hash(saved), objectIndex = row.Texture.ObjectIndex,
            fixedOutputPath = fixedCopy, fixedOutputSha256 = Hash(fixedBytes)
        }, new JsonSerializerOptions { WriteIndented = true }));
        foreach (var item in window.Rows) item.Dispose();
        Console.WriteLine($"PASS GUI workflow: {checks} assertions; output {output}");
    }

    private static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
}
