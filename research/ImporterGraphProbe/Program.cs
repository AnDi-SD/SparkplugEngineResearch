using System.Diagnostics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;

if(args.Length is <2 or >11)throw new ArgumentException("OUTPUT_JSON SMO [at most10 files]");
var names=typeof(SmoClassIds).GetFields(BindingFlags.Public|BindingFlags.Static).Where(f=>f.FieldType==typeof(uint))
    .GroupBy(f=>(uint)f.GetValue(null)!).ToDictionary(g=>g.Key,g=>g.First().Name);
var files=new List<object>();var watch=Stopwatch.StartNew();int missingTotal=0;
foreach(string path in args.Skip(1))
{
    if(new FileInfo(path).Length>8*1024*1024)throw new InvalidDataException("8MiB input cap");
    byte[] raw=File.ReadAllBytes(path);var doc=SmoDocument.ParseOwned(raw);
    if(doc.HasErrors || doc.Objects.Count>4096)throw new InvalidDataException("Invalid or oversized catalog");
    var bindings=SmoTextureBindingResolver.ResolveAll(doc);var rows=new List<object>();int missing=0;
    foreach(var entry in doc.Objects.Where(e=>e.TypeHash==SmoClassIds.MeshData))
    {
        bindings.TryGetValue(entry.Index,out var binding);
        if(binding?.Texture is not null && binding.Issue is null)continue;
        missing++;var parents=new List<object>();var current=entry;
        while(current.ParentIndex is int index)
        {
            if(parents.Count>=64)throw new InvalidDataException("Parent depth cap");
            current=doc.Objects[index];parents.Add(new {current.Index,current.Name,type=names.GetValueOrDefault(current.TypeHash,current.TypeHash.ToString("X8"))});
        }
        var mesh=SmoMeshDecoder.Decode(doc,entry);
        rows.Add(new {entry.Index,entry.Name,mesh.VertexCount,mesh.HasSkinningData,binding?.Issue,parents});
    }
    missingTotal+=missing;
    files.Add(new {path=Path.GetFullPath(path),sha256=Convert.ToHexString(SHA256.HashData(raw)),objects=doc.Objects.Count,
        textures=doc.Objects.Count(e=>e.TypeHash==SmoClassIds.TextureData),missing,rows});
    Console.WriteLine($"{Path.GetFileName(path)}: {missing} meshes without an unambiguous texture binding");
}
string output=Path.GetFullPath(args[0]);Directory.CreateDirectory(Path.GetDirectoryName(output)!);
File.WriteAllText(output,JsonSerializer.Serialize(new {kind="importer-target-binding-survey",status="observed",files,missingTotal,
    elapsedSeconds=watch.Elapsed.TotalSeconds,peakWorkingSet=Process.GetCurrentProcess().PeakWorkingSet64,
    scope="Bounded source survey, not native material inheritance evidence. Rigid and skinned meshes are reported separately."},new JsonSerializerOptions {WriteIndented=true}));
