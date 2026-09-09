using SmoViewer.Core;
using SmoViewer.Sparkplug;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;

float[] Floats(Matrix4x4 m)=>[m.M11,m.M12,m.M13,m.M14,m.M21,m.M22,m.M23,m.M24,m.M31,m.M32,m.M33,m.M34,m.M41,m.M42,m.M43,m.M44];
string Hex(Matrix4x4 m)=>Convert.ToHexString(Floats(m).SelectMany(BitConverter.GetBytes).ToArray());
string Sha(string file)=>Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(file)));
var files=new List<object>();
foreach(string file in args.Skip(1))
{
    var document=SmoDocument.Load(file);
    using var scene=new SparkplugSceneRuntime(document,new Dictionary<int,SmoSkin>());
    var rows=new List<object>();
    foreach(var (index,world) in scene.Worlds)
    {
        var entry=document.Objects[index];
        if(!SmoNodeTransformDecoder.TryResolveNodeWorldMatrix(document,entry,out var resolved) || Hex(world)!=Hex(resolved))
            throw new InvalidDataException($"Placement/cache differs from shared scene: {file}, {entry.Id}");
        rows.Add(new {id=entry.Id,index,name=entry.Name,world_hex=Hex(world)});
    }
    files.Add(new {file=Path.GetFullPath(file),sha256=Sha(file),nodes=rows});
}
var report=new {status="passed",core_dll_sha256=Sha(typeof(SmoDocument).Assembly.Location),
    native_dll_sha256=Sha(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")),files};
File.WriteAllText(args[0],JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true}));
Console.WriteLine($"PASS common scene/placement cache: {files.Count} files");
