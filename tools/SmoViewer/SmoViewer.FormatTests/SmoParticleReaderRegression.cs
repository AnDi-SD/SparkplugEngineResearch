using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;
internal static class SmoParticleReaderRegression
{
    private static byte[] State(SmoParticleSystemData p)
    {
        using var stream=new MemoryStream();using var writer=new BinaryWriter(stream);
        void V(Vector3 value){writer.Write(value.X);writer.Write(value.Y);writer.Write(value.Z);}
        void R(SmoSingleRange value){writer.Write(value.Begin);writer.Write(value.End);}
        writer.Write(checked((byte)p.Renderable.AlphaSortEnable));writer.Write(p.Renderable.Priority);
        writer.Write(p.LoopAnimation);writer.Write(p.WorldSpace);writer.Write(p.IterativeMode);writer.Write(p.EmissionRate);
        R(p.Time);writer.Write(p.Color.Begin);writer.Write(p.Color.End);V(p.AccelerationBegin);V(p.AccelerationEnd);V(p.EmissionDirection);
        R(p.Velocity);R(p.Angle);R(p.Scale);V(Vector3.Zero);writer.Write(p.BoundingSphere);writer.Write(p.RegionType);
        switch(p.Region)
        {
            case SmoParticlePointRegion region:V(region.Position);break;
            case SmoParticleBoxRegion region:V(region.Position);V(region.Size);break;
            case SmoParticleSphereRegion region:V(region.Position);writer.Write(region.Radius);break;
            case SmoParticlePlaneRegion region:V(region.Position);V(region.Normal);writer.Write(region.SizeX);writer.Write(region.SizeY);break;
            case SmoParticleDiskRegion region:V(region.Position);writer.Write(region.Radius);break;
            case SmoParticleCylinderRegion region:V(region.Position);writer.Write(region.Height);writer.Write(region.Radius);break;
            case SmoParticleConeRegion region:V(region.Position);writer.Write(region.Height);writer.Write(region.Radius1);writer.Write(region.Radius2);break;
            default:throw new InvalidDataException("Unknown projected region.");
        }
        foreach(uint value in p.InitialPool)writer.Write(value);return stream.ToArray();
    }
    internal static int Run(string folder,string output)
    {
        int checks=0;var cases=new List<object>();var real=new List<object>();
        void Check(bool value,string message){if(!value)throw new InvalidDataException(message);++checks;}
        var expected=JsonNode.Parse(File.ReadAllText(Path.Combine(folder,"abi-1.json")))!;
        foreach(var row in expected["rows"]!.AsArray())
        {
            var document=SmoDocument.Load(Path.Combine(folder,row!["file"]!.GetValue<string>()));
            var entry=document.Objects.Single(item=>item.TypeHash==SmoClassIds.ParticleSystem);
            Check(SmoParticleSystemDecoder.TryDecode(document,entry,out var data,out var error),error);
            Check(Convert.ToHexString(State(data!)).Equals(row["state_hex"]!.GetValue<string>(),StringComparison.OrdinalIgnoreCase),"Managed projection equals original-PC/ABI state bytes");
            Check(SmoParticleSystemDecoder.TryDecode(document,entry,out var again,out _)&&ReferenceEquals(data,again),"immutable graph snapshot reused");
            string mode=row["case"]!.GetValue<string>();
            if(mode=="defaults-12")Check(data!.Scale==new SmoSingleRange(1,1)&&data.AccelerationBegin==new Vector3(0,0,1)&&data.InitialPool[1]==100,"constructor defaults and actual non-looping pool");
            if(mode=="direction-tiny-12")Check(data!.EmissionDirection==Vector3.Zero,"original normalization threshold");
            if(mode.StartsWith("custom-",StringComparison.Ordinal))
            {
                Check(data!.IterativeMode==3,"raw mode byte retained");
                var inspection=SmoSerializedFieldInspector.Inspect(document,entry);
                Check(inspection.Single(field=>field.Descriptor.Key=="particle.iterative").DisplayValue.Contains("raw 3",StringComparison.Ordinal),"inspector preserves non-Boolean byte");
                if(mode is "custom-17" or "custom-18")
                {
                    string key=mode=="custom-17"?"particle.region.cylinder":"particle.region.cone";
                    Check(inspection.Single(field=>field.Descriptor.Key==key).DisplayValue.Contains("height=3.25",StringComparison.Ordinal),"inspector takes actual height before radius members");
                }
            }
            cases.Add(new{mode,objects=document.Objects.Count});
        }
        string root=Path.GetFullPath(Path.Combine(folder,"../../../.."));
        foreach(var row in expected["real"]!.AsArray())
        {
            string relative=row!["path"]!.GetValue<string>();var document=SmoDocument.Load(Path.Combine(root,relative));
            var loaded=SmoLoadedResources.Get(document);Check(loaded.LoadIssue is null,loaded.LoadIssue??"whole common graph");
            Check(loaded.Particles.Count==row["particles"]!.AsArray().Count,"all actual particles represented");
            foreach(var item in row["particles"]!.AsArray())
            {
                uint id=item!["id"]!.GetValue<uint>();var entry=document.Objects.Single(value=>value.Id==id);
                Check(SmoParticleSystemDecoder.TryDecode(document,entry,out var data,out var error),error);
                Check(Convert.ToHexString(State(data!)).Equals(item["state_hex"]!.GetValue<string>(),StringComparison.OrdinalIgnoreCase),"real particle snapshot equals C ABI");
                Check((data!.RenderNode?.Id??0)==item["node"]!.GetValue<uint>()&&(data.Renderable.Material?.Id??0)==item["material"]!.GetValue<uint>(),"actual loaded reference IDs");
                if(data.RenderNode is not null)Check(ReferenceEquals(data.RenderNode,document.Objects.Single(value=>value.Id==data.RenderNode.Id)),"canonical parent identity");
            }
            real.Add(new{relative,objects=document.Objects.Count,particles=loaded.Particles.Count});
        }
        var looping=SmoDocument.Load(Path.Combine(folder,"fixture-looping.smo"));
        Check(!SmoParticleSystemDecoder.TryDecode(looping,looping.Objects[0],out _,out var issue)&&issue.Contains("RenderNode",StringComparison.Ordinal),"looping initialization without its required RenderNode remains visible");
        var report=new{status="passed",checks,cases,real,native_dll_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll"))))};
        File.WriteAllText(output,JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true})+"\n");Console.WriteLine($"Particle: {cases.Count} fixtures, {checks} checks");return 0;
    }
    internal static int RunInitialization(string source,string original,string output)
    {
        int checks=0;void Check(bool value,string message){if(!value)throw new InvalidDataException(message);++checks;}
        var watch=System.Diagnostics.Stopwatch.StartNew();var document=SmoDocument.Load(source);
        var resources=SmoLoadedResources.Get(document);Check(resources.LoadIssue is null,resources.LoadIssue??"Loaded particle graph");
        var expected=JsonNode.Parse(File.ReadAllText(original))!;var state=expected["pools"]!.AsArray()[1]!;
        var p=resources.Particles.Single().Value;var pool=p.CpuPool??throw new InvalidDataException("Missing native CPU pool");
        Check(Convert.ToHexString(State(p)).Equals(state["parametersHex"]!.GetValue<string>(),StringComparison.OrdinalIgnoreCase),"Managed original Particle parameter/pool bytes");
        Check(pool.First==state["first"]!.GetValue<uint>()&&pool.Boundary==state["boundary"]!.GetValue<uint>(),"Original ring boundaries");
        Check(pool.Records.Count==state["count"]!.GetValue<int>(),"Original physical particle count");
        using var stream=new MemoryStream();using var writer=new BinaryWriter(stream);
        void V(Vector3 v){writer.Write(v.X);writer.Write(v.Y);writer.Write(v.Z);}
        for(int i=0;i<pool.Records.Count;++i)
        {
            var record=pool.Records[i];Check(record.Written,"Real looping menu emitted every record");
            V(record.Position);V(record.Velocity);writer.Write(record.BirthTime);writer.Write(record.Lifetime);
            var link=state["links"]!.AsArray()[i]!;
            Check(record.Previous==link[1]!.GetValue<uint>()&&record.Next==link[2]!.GetValue<uint>(),"Original physical record links");
        }
        Check(Convert.ToHexString(stream.ToArray()).Equals(state["recordsHex"]!.GetValue<string>(),StringComparison.OrdinalIgnoreCase),"Every original particle CPU record bit matches through C#");
        var report=new{status="passed",checks,objects=document.Objects.Count,nodes=resources.Nodes.Count,particles=resources.Particles.Count,records=pool.Records.Count,
            seconds=watch.Elapsed.TotalSeconds,source_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))),
            original_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(original))),
            native_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll"))))};
        File.WriteAllText(output,JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true})+"\n");
        Console.WriteLine($"Particle Init: {pool.Records.Count} exact original records; {checks} checks");return 0;
    }
}
