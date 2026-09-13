using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;
internal static class SmoFogInspectionRegression
{
    internal static int Run(string folder,string output)
    {
        int checks=0;void Check(bool value,string label){if(!value)throw new InvalidDataException(label);++checks;}
        var proof=JsonNode.Parse(File.ReadAllText(Path.Combine(folder,"abi-1.json")))!;
        foreach(var row in proof["rows"]!.AsArray())
        {
            byte[] payload=Convert.FromHexString(row!["payload_hex"]!.GetValue<string>());
            Check(SmoFogDecoder.TryDecode(payload,out var fog)&&fog is not null,"common Fog payload accepted");
            using var stream=new MemoryStream();using var writer=new BinaryWriter(stream);
            writer.Write(fog!.Type);writer.Write(fog.Color);writer.Write(fog.Start);writer.Write(fog.End);writer.Write(fog.Density);
            Check(Convert.ToHexString(stream.ToArray()).Equals(row["state_hex"]!.GetValue<string>(),StringComparison.OrdinalIgnoreCase),"all original PC state bits survive managed projection");
        }
        foreach(int length in new[]{0,4,19,21,24})Check(!SmoFogDecoder.TryDecode(new byte[length],out _),"wrong extent rejected");
        string root=Path.GetFullPath(Path.Combine(folder,"../../../.."));
        var document=SmoDocument.Load(Path.Combine(root,"local-data/pc-pristine/Media/Menus/logo_screen.smo"));
        var entry=document.Objects.Single(value=>value.TypeHash==SmoClassIds.Fog);
        Check(SmoObjectFieldReader.TryRead(document,entry,out var fields,out var error),error);
        var field=fields.Single(value=>value.FieldType==0&&value.PayloadSize==20);
        Check(SmoFogDecoder.TryDecode(field.Payload.Span,out var actual),"actual original SMO Fog field");
        Check(actual!.Type==0&&actual.Color==0xff000000&&actual.Start==0&&actual.End==1000,"actual logo Fog values");
        var inspected=SmoSerializedFieldInspector.Inspect(document,entry);
        Check(inspected.Any(value=>value.DisplayValue.Contains("1000",StringComparison.Ordinal)),"existing inspector consumes the common projection");
        var report=new{status="passed",checks,originalCases=3,realFiles=1,native_dll_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll"))))};
        File.WriteAllText(output,JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true})+"\n");Console.WriteLine($"Fog: {checks} checks");return 0;
    }
}
