using SmoViewer.Core;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;

byte[] bytes=File.ReadAllBytes(args[0]);
var warm=SmoDocument.Parse(bytes);
SmoNodeTransformDecoder.TryDecode(warm,warm.Objects.First(x=>x.TypeHash==SmoClassIds.Node),out _);
var samples=new List<object>();
for(int sample=0;sample<3;sample++)
{
    var document=SmoDocument.Parse(bytes);
    var nodes=document.Objects.Where(x=>x.TypeHash is SmoClassIds.Node or SmoClassIds.RenderNode).ToArray();
    long allocated=GC.GetAllocatedBytesForCurrentThread();var timer=Stopwatch.StartNew();int decoded=0;double sum=0;
    foreach(var node in nodes)
    {
        if(!SmoNodeTransformDecoder.TryDecode(document,node,out var value))throw new Exception("Decode failed "+node.Id);
        decoded++;sum+=value.Position.X+value.Position.Y+value.Position.Z;
    }
    timer.Stop();samples.Add(new { decoded, checksum=sum, milliseconds=timer.Elapsed.TotalMilliseconds,
        allocated_bytes=GC.GetAllocatedBytesForCurrentThread()-allocated });
}
string Sha(string path)=>Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
Console.WriteLine(JsonSerializer.Serialize(new { input_sha256=Convert.ToHexString(SHA256.HashData(bytes)),
    core_sha256=Sha(typeof(SmoDocument).Assembly.Location),native_sha256=Sha(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")), samples }));
