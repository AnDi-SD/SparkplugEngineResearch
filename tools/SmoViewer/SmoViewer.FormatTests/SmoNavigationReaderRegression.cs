using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;

internal static class SmoNavigationReaderRegression
{
    internal static int Run(string source,string output)
    {
        var document=SmoDocument.Load(source);var loaded=SmoLoadedResources.Get(document);int checks=0,sets=0,graphs=0,portals=0,cells=0;
        void Check(bool value,string message){if(!value)throw new InvalidDataException(message);++checks;}
        Check(loaded.LoadIssue is null,loaded.LoadIssue??"Actual graph loaded");
        Check(loaded.SceneIssue is null,loaded.SceneIssue??"Actual Node worlds available");
        static uint U32(ReadOnlySpan<byte> data,int offset=0)=>BinaryPrimitives.ReadUInt32LittleEndian(data[offset..]);
        foreach(var entry in document.Objects.Where(e=>e.TypeHash is SmoClassIds.MeshNavigationSet or SmoClassIds.NavigationGraph or SmoClassIds.NavigationPortal))
        {
            Check(SmoObjectFieldReader.TryRead(document,entry,out var fields,out var error),error);
            var byKey=fields.Select((field,index)=>(field,descriptor:SmoSerializedFieldRegistry.TryDescribeField(entry.TypeHash,fields,index,out var descriptor)?descriptor:null))
                .Where(item=>item.descriptor is not null).GroupBy(item=>item.descriptor!.Key)
                .ToDictionary(group=>group.Key,group=>group.Select(item=>item.field).ToArray());
            if(entry.TypeHash==SmoClassIds.MeshNavigationSet)
            {
                ++sets;Check(SmoMeshNavigationSetDecoder.TryDecode(document,entry,out var value,out error),error);
                Check(value!.NodeCount==U32(byKey["navigation_set.node_count"][^1].Payload.Span),"Node count matches wire");
                foreach(var (key,table) in new[]{("navigation_set.transition_table",value.NodeTransitions),("navigation_set.portal_transition_table",value.PortalTransitions)})
                {
                    var payload=byKey[key][^1].Payload.Span;
                    Check(table.RowCount==U32(payload)&&table.ColumnCount==U32(payload,4),"Actual matrix dimensions match wire");
                    for(int i=0;i<table.LinkSelectors.Count;++i){Check(table.LinkSelectors[i]==(U32(payload,8+4*i)&3u),"Actual two-bit matrix cell matches original truncation");++cells;}
                }
                var expected=Enumerable.Range(0,checked((int)value.NodeCount)).Select(_=>new List<byte>()).ToArray();
                foreach(var field in byKey["navigation_set.links"])
                {
                    var payload=field.Payload.Span;int cursor=4;
                    for(uint row=0;row<U32(payload);++row){byte node=payload[cursor++];uint count=U32(payload,cursor);cursor+=4;
                        for(uint i=0;i<count;++i)expected[node].Add(payload[cursor++]);}
                }
                for(int i=0;i<expected.Length;++i)Check(expected[i].SequenceEqual(value.Links[i].NeighbourNodeIds),"Actual neighbour order and multiplicity match wire");
                if(byKey.TryGetValue("navigation_set.enabled",out var enabled))Check(value.RawEnabled==enabled[^1].Payload.Span[0],"Raw enabled byte is retained");
                Check(value.NavigationMesh.TargetTypeHash==SmoClassIds.MeshBoundingVolume,"Mesh reference is actual MeshBV");
            }
            else if(entry.TypeHash==SmoClassIds.NavigationGraph)
            {
                ++graphs;Check(SmoNavigationGraphDecoder.TryDecode(document,entry,out var value,out error),error);
                Check(value!.PathTableSize==U32(byKey["navigation_graph.path_table_size"][^1].Payload.Span),"Graph table size matches wire");
                var expected=Enumerable.Range(0,value.Paths.Count).Select(_=>(next:(byte)255,alternatives:Array.Empty<byte>())).ToArray();
                foreach(var field in byKey["navigation_graph.path"])
                {
                    var p=field.Payload.Span;int index=checked((int)(U32(p)*value.PathTableSize+U32(p,4)));
                    expected[index]=(p[8],p[10..].ToArray());
                }
                for(int i=0;i<expected.Length;++i){var path=value.Paths[i];Check(path.NextPortalIndex==expected[i].next,"Graph next portal includes original defaults/last field");
                    Check(path.Alternatives.SelectMany(pair=>new[]{pair.FirstPortalIndex,pair.Reserved}).SequenceEqual(expected[i].alternatives),"Graph alternatives preserve raw ordered bytes");}
                Check(loaded.RenderContainersByObjectIndex.ContainsKey(entry.Index),"NavigationGraph is the same loaded RenderNode");
            }
            else
            {
                ++portals;Check(SmoNavigationPortalDecoder.TryDecode(document,entry,out var value,out error),error);
                var pairs=byKey.GetValueOrDefault("navigation_portal.nodes",[]).SelectMany(field=>field.Payload.ToArray()).ToArray();
                Check(value!.NodePairs.SelectMany(pair=>new[]{pair.FirstSetNodeId,pair.SecondSetNodeId}).SequenceEqual(pairs),"Portal node pairs match raw wire order");
                var paths=byKey.GetValueOrDefault("navigation_portal.path",[]).SelectMany(field=>field.Payload.ToArray()).ToArray();
                Check(value.PathMemberships.SelectMany(path=>new[]{path.SourceSetIndex,path.DestinationSetIndex,path.AlternativePathIndex}).SequenceEqual(paths),"Portal path memberships match raw wire order");
                Check(value.EndpointSets.Count==2&&value.Graph.TargetTypeHash==SmoClassIds.NavigationGraph,"Actual portal references are retained");
            }
        }
        Check(sets>0&&graphs>0,"Selected scene exercises navigation set and graph");
        var report=new{status="passed",source=Path.GetFullPath(source),source_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))),
            native_dll_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")))),
            objects=document.Objects.Count,sets,graphs,portals,cells,checks,scope="Actual common graph readers and immutable inspector projections; routing runtime and writers are not claimed."};
        File.WriteAllText(output,JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true})+"\n");
        Console.WriteLine($"Navigation: {sets} sets, {graphs} graphs, {portals} portals, {cells} cells, {checks} checks");return 0;
    }
}
