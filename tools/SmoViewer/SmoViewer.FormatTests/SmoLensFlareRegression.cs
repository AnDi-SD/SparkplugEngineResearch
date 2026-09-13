using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;
internal static class SmoLensFlareRegression
{
    internal static int Run(string folder,string output)
    {
        int checks=0;var rows=new List<object>();
        void Check(bool value,string message){if(!value)throw new InvalidDataException(message);++checks;}
        var capture=JsonNode.Parse(File.ReadAllText(Path.Combine(folder,"abi-1.json")))!;
        foreach(var row in capture["rows"]!.AsArray())
        {
            string mode=row!["case"]!.GetValue<string>();string path=Path.Combine(folder,$"fixture-{mode}.smo");
            var document=SmoDocument.Load(path);var entry=document.Objects.Single(value=>value.TypeHash==SmoClassIds.LensFlare);
            var loaded=SmoLoadedResources.Get(document);Check(loaded.LoadIssue is null,loaded.LoadIssue??"Loaded actual graph");
            Check(SmoLensFlareDecoder.TryDecode(document,entry,out var value,out var error),error);
            object Element(SmoLensFlareElement item)=>new { material=item.Material?.Id??0,tail=new[]{item.Color,BitConverter.SingleToUInt32Bits(item.RelativeDistance),BitConverter.SingleToUInt32Bits(item.Scale)} };
            var actual=JsonSerializer.SerializeToNode(new{occlusion_bits=new[]{BitConverter.SingleToUInt32Bits(value!.OcclusionSphereRadius),BitConverter.SingleToUInt32Bits(value.OcclusionSpeed)},
                render_node=value.RenderNode?.Id??0,primary=Element(value.Primary),counted=value.Elements.Select(Element).ToArray()});
            Check(JsonNode.DeepEquals(actual,row["state"]),"Managed projection equals independently checked original/ABI snapshot");
            Check(value.Renderable.AlphaSortEnable==1&&value.Renderable.Priority==0,"Inherited state comes from actual Renderable constructor");
            if(mode=="links")
            {
                Check(loaded.SceneIssue is null,loaded.SceneIssue??"Actual containing Node world");
                Check(value.Elements.Count==2&&ReferenceEquals(value.Primary.Material,value.Elements[0].Material)&&ReferenceEquals(value.Elements[0].Material,value.Elements[1].Material),"One shared material identity in all three element slots");
                var inspected=SmoSerializedFieldInspector.Inspect(document,entry);
                Check(inspected.Single(field=>field.Descriptor.Key=="lens_flare.glare").DisplayValue.Contains("elements=2",StringComparison.Ordinal),"Inspector shows counted array");
                Check(value.RenderNode!.Id==1&&loaded.RenderContainers.Single().RenderableObjectIndices.Single()==entry.Index,"LensFlare belongs to actual RenderNode support");
            }
            rows.Add(new{mode,objects=document.Objects.Count,state=actual,sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)))});
        }
        var report=new{status="passed",checks,rows,native_dll_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")))),
            scope="Actual shared LensFlare/Quad graph and inspection; original PC destructor and GPU flare rendering are not claimed."};
        File.WriteAllText(output,JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true})+"\n");Console.WriteLine($"LensFlare: {rows.Count} cases, {checks} checks");return 0;
    }
}
