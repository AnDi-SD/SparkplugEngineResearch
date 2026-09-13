using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;
namespace SmoViewer.FormatTests;
internal static class SmoFontInspectionRegression
{
    internal static int Run(string source,string output)
    {
        int checks=0;
        void Check(bool ok,string text) { if(!ok)throw new InvalidDataException(text);++checks; }
        var document=SmoDocument.Load(source);
        var entries=document.Objects.Where(entry=>entry.TypeHash==SmoClassIds.Font).ToArray();
        Check(entries.Length>0,"selected menu contains Font objects");
        var copy=SmoDocument.Parse(document.Data);
        Check(!SmoFontDecoder.TryDecode(copy,entries[0],out _,out _),"foreign catalog entry rejected before native parsing");
        byte[] malformed=document.Data.ToArray();
        int ownOffset=checked((int)entries[0].PhysicalOffset+8);
        malformed[ownOffset]=0; // premature terminator, remaining original body follows
        var invalid=SmoDocument.ParseOwned(malformed);
        Check(!SmoFontDecoder.TryDecode(invalid,invalid.Objects[entries[0].Index],out _,out var invalidError)&&
            invalidError.Contains("Trailing derived section bytes",StringComparison.Ordinal),"stable malformed-input diagnostic");
        var snapshots=new List<object>();
        foreach(var entry in entries)
        {
            Check(SmoFontDecoder.TryDecode(document,entry,out var font,out var error),error);
            Check(font!.Height==32&&font.Baseline==6&&font.Glyphs.Count==224,"original menu metrics and glyph count");
            Check(font.Image.TargetTypeHash==SmoClassIds.TextureData&&font.Image.TargetObjectIndex.HasValue,"atlas catalog binding");
            Check(font.Glyphs[0].Character==0x20&&font.Glyphs[^1].Character==0xFF,"full byte character range");
            using var stream=new MemoryStream();using var writer=new BinaryWriter(stream);
            foreach(var glyph in font.Glyphs)
            {
                writer.Write(glyph.Width);writer.Write(glyph.Uv0.X);writer.Write(glyph.Uv0.Y);writer.Write(glyph.Uv1.X);writer.Write(glyph.Uv1.Y);
            }
            var fields=SmoSerializedFieldInspector.Inspect(document,entry);
            Check(fields.Any(field=>field.DisplayValue.Contains("height=32",StringComparison.Ordinal)),"existing inspector consumes common Font");
            snapshots.Add(new {entry.Id,font.Height,font.Baseline,atlas=font.Image.ObjectId,
                glyphSha256=Convert.ToHexString(SHA256.HashData(stream.ToArray()))});
        }
        Check(!SmoFontDecoder.TryDecode(document,document.Objects.First(entry=>entry.TypeHash!=SmoClassIds.Font),out _,out _),"wrong type rejected");
        File.WriteAllText(output,JsonSerializer.Serialize(new {status="passed",checks,source,fonts=snapshots,
            inputSha256=Convert.ToHexString(SHA256.HashData(document.Data.Span)),
            nativeSha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll"))))},
            new JsonSerializerOptions{WriteIndented=true})+"\n");
        Console.WriteLine($"Font: {checks} checks, {entries.Length} objects");return 0;
    }
}
