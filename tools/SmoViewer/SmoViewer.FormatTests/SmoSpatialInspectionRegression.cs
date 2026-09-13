using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;
internal static class SmoSpatialInspectionRegression
{
    internal static int Run(string folder,string output)
    {
        int checks=0,objects=0;void Check(bool value,string label){if(!value)throw new InvalidDataException(label);++checks;}
        static uint U(JsonNode value,string key)=>value[key]!.GetValue<uint>();
        static uint[] A(JsonNode value,string key)=>value[key]!.AsArray().Select(v=>v!.GetValue<uint>()).ToArray();
        static uint[] Bits(Vector3 v)=>[BitConverter.SingleToUInt32Bits(v.X),BitConverter.SingleToUInt32Bits(v.Y),BitConverter.SingleToUInt32Bits(v.Z)];
        static uint[] PlaneBits(SmoBspPlane p)=>[..Bits(p.Normal),BitConverter.SingleToUInt32Bits(p.Constant)];
        string root=Path.GetFullPath(Path.Combine(folder,"../../../.."));
        var proof=JsonNode.Parse(File.ReadAllText(Path.Combine(folder,"abi-2.json")))!;
        var rows=new List<object>();
        foreach(var row in proof["rows"]!.AsArray())
        {
            string path=Path.Combine(root,row!["source"]!.GetValue<string>());
            Check(Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)))==row["sha256"]!.GetValue<string>(),"same fixture/file bytes as ABI proof");
            var document=SmoDocument.Load(path);
            var expected=JsonNode.Parse(File.ReadAllText(Path.Combine(folder,row["snapshot"]!.GetValue<string>())))!;
            var loaded=SmoLoadedResources.Get(document);
            Check(loaded.LoadIssue is null,loaded.LoadIssue??"shared graph loaded");
            Check(ReferenceEquals(loaded,SmoLoadedResources.Get(document)),"one immutable graph projection per document");
            var byId=document.Objects.ToDictionary(e=>e.Id);
            void Reference(SmoLoadedReference reference,uint id)
            {
                Check(reference.ObjectId==id,"actual referenced ID");
                Check(id==0?reference.Target is null:ReferenceEquals(reference.Target,byId[id]),"canonical target identity, no wire encoding invented");
            }
            void References(IReadOnlyList<SmoLoadedReference> references,uint[] ids)
            {
                Check(references.Count==ids.Length,"all ordered reference slots retained");
                for(int i=0;i<ids.Length;++i)Reference(references[i],ids[i]);
            }
            void Plane(SmoBspPlane? plane,JsonNode? bits)
            {
                Check((plane is null)==(bits is null),"unknown original plane stays explicit");
                if(plane is not null)Check(PlaneBits(plane).SequenceEqual(bits!.AsArray().Select(b=>b!.GetValue<uint>())),"plane bits preserved without normalization");
            }
            void Polygon(IReadOnlyList<Vector3> points,JsonArray expectedPoints)
            {
                Check(points.Count==expectedPoints.Count,"all polygon vertices retained");
                for(int i=0;i<points.Count;++i)Check(Bits(points[i]).SequenceEqual(expectedPoints[i]!.AsArray().Select(b=>b!.GetValue<uint>())) ,"polygon bits preserved");
            }
            void Partition(SmoPartitionNodeData p,JsonNode e)
            {
                Check(p.DebugColorArgb==U(e,"Color"),"actual current partition color");
                Reference(p.PartitionSystem,U(e,"System"));Reference(p.Zone,U(e,"Zone"));Reference(p.Parent,U(e,"Parent"));Reference(p.PartitionRenderable,U(e,"Payload"));
                References(p.CollisionInfos,A(e,"Collisions"));References(p.ZonePortals,A(e,"Portals"));References(p.StaticRenderObjects,A(e,"Statics"));
                var children=A(e,"Children");Check(p.ChildSlotCount==children.Length,"actual factory slot count");
                var nonnull=children.Select((id,slot)=>(id,slot)).Where(v=>v.id!=0).ToArray();
                Check(p.Children.Count==nonnull.Length,"nonnull child list agrees");
                for(int i=0;i<nonnull.Length;++i){Check(p.Children[i].SlotIndex==nonnull[i].slot,"child slot index retained");Reference(p.Children[i].Relationship,nonnull[i].id);}
            }
            foreach(var e in expected["Partitions"]!.AsArray())
            {
                var entry=byId[U(e!,"Id")];++objects;
                if(entry.TypeHash==SmoClassIds.PartitionNode){Check(SmoPartitionNodeDecoder.TryDecode(document,entry,out var p,out var error),error);Partition(p!,e!);}
                else if(entry.TypeHash==SmoClassIds.BspNode){Check(SmoBspNodeDecoder.TryDecode(document,entry,out var p,out var error),error);Partition(p!.PartitionNode,e!);Plane(p.Plane,e!["PlaneBits"]);Polygon(p.Polygon,e["PolygonBits"]!.AsArray());}
                else
                {
                    bool success=SmoOctreeNodeDecoder.TryDecode(document,entry,out var p,out var error);
                    if(e!["PivotBits"] is null){Check(!success&&error.Contains("uninitialized pivot",StringComparison.Ordinal),"unknown pivot explicitly refused");continue;}
                    Check(success,error);Partition(p!.PartitionNode,e);
                    Check(Bits(p.Pivot).SequenceEqual(A(e,"PivotBits"))&&Bits(p.Minimum).SequenceEqual(A(e,"MinimumBits"))&&Bits(p.Maximum).SequenceEqual(A(e,"MaximumBits")),"Octree raw values preserved");
                }
            }
            foreach(var e in expected["Systems"]!.AsArray())
            {
                var entry=byId[U(e!,"Id")];++objects;Check(SmoPartitionSystemDecoder.TryDecode(document,entry,out var p,out var error),error);
                Reference(p!.PartitionRoot,U(e!,"Root"));References(p.Renderables,A(e!,"Renderables"));
            }
            foreach(var e in expected["Payloads"]!.AsArray())
            {
                var entry=byId[U(e!,"Id")];++objects;Check(SmoPartitionRenderableDecoder.TryDecode(document,entry,out var p,out var error),error);
                Check(p!.DebugColorArgb==U(e!,"Color"),"payload current color");References(p.Renderables,A(e!,"Renderables"));
            }
            foreach(var e in expected["Zones"]!.AsArray())
            {
                var entry=byId[U(e!,"Id")];++objects;Check(SmoZoneDecoder.TryDecode(document,entry,out var p,out var error),error);References(p!.LocalPartitionRoots,A(e!,"Roots"));
            }
            foreach(var e in expected["Portals"]!.AsArray())
            {
                var entry=byId[U(e!,"Id")];++objects;Check(SmoZonePortalDecoder.TryDecode(document,entry,out var p,out var error),error);
                Reference(p!.DestinationZone,U(e!,"Destination"));Check(p.OpenByte==U(e!,"Open")&&p.IsOpen==(p.OpenByte!=0),"raw open byte and nonzero interpretation");
                Plane(p.Plane,e!["PlaneBits"]);Polygon(p.Polygon,e["PolygonBits"]!.AsArray());
            }
            foreach(var e in expected["PortalNodes"]!.AsArray())
            {
                var entry=byId[U(e!,"Id")];++objects;Check(SmoZonePortalNodeDecoder.TryDecode(document,entry,out var p,out var error),error);References(p!.ZonePortals,A(e!,"Portals"));
            }
            rows.Add(new{source=row["source"]!.GetValue<string>(),objects=document.Objects.Count});
        }
        var report=new{status="passed",checks,spatialObjects=objects,files=rows,
            native_dll_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll"))))};
        File.WriteAllText(output,JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true})+"\n");Console.WriteLine($"Spatial inspection: {checks} checks, {objects} spatial objects");return 0;
    }
}
