using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;
using SmoViewer.Sparkplug;
using SmoViewer.Scene;
namespace SmoViewer.FormatTests;
internal static class SmoTextRuntimeRegression
{
    internal static int Run(string source,string output)
    {
        if(File.Exists(output))throw new InvalidDataException("Use a fresh result path.");
        var watch=Stopwatch.StartNew();int checks=0;string? failure=null;object? detail=null;
        void Check(bool value,string message){++checks;if(!value)throw new InvalidDataException(message);}
        string Hash(ReadOnlySpan<byte> value)=>Convert.ToHexString(SHA256.HashData(value));
        var original=File.ReadAllBytes(source);string sourceHash=Hash(original);
        try
        {
            var document=SmoDocument.Parse(original);var loaded=SmoLoadedResources.Get(document);
            Check(loaded.LoadIssue is null,loaded.LoadIssue??"loaded");
            Check(loaded.SceneIssue is null,loaded.SceneIssue??"scene");
            Check(loaded.ReferenceTrace is not null&&loaded.ReferenceTraceIssue is null,"Original file reference trace remains available");
            Check(ReferenceEquals(loaded,SmoLoadedResources.Get(document)),"One immutable graph snapshot per document");
            Check(loaded.Texts.Count==document.Objects.Count(e=>e.TypeHash==SmoClassIds.TextRenderable)
                &&loaded.Fonts.Count==document.Objects.Count(e=>e.TypeHash==SmoClassIds.Font)
                &&loaded.TextNodeCachedTexts.Count==document.Objects.Count(e=>e.TypeHash==SmoClassIds.TextNode),"All actual Text/Font/TextNode objects projected");
            var atlasRows=new Dictionary<int,object>();var fontRows=new List<object>();var textRows=new List<object>();
            foreach(var font in loaded.Fonts.Values)
            {
                Check(SmoFontDecoder.TryDecode(document,document.Objects[font.ObjectIndex],out var wire,out var error),error);
                Check(wire!.Height==font.Height&&wire.Baseline==font.Baseline&&wire.Glyphs.SequenceEqual(font.Glyphs),"Actual Font scalars/glyphs equal raw serializer observation");
                Check(font.Atlas is {Texture:not null,Issue:null},font.Atlas?.Issue??"Font has decoded actual atlas");
                Check(wire.Image.TargetObjectIndex==font.Atlas!.ObjectIndex,"Original atlas reference resolves to canonical runtime texture");
                if(!atlasRows.ContainsKey(font.Atlas.ObjectIndex))
                {
                    Check(SmoTextureDecoder.TryDecode(document,document.Objects[font.Atlas.ObjectIndex],out var stored,out error),error);
                    Check(stored!.Width==font.Atlas.Texture!.Width&&stored.Height==font.Atlas.Texture.Height
                        &&stored.Bgra32Pixels.Span.SequenceEqual(font.Atlas.Texture.Bgra32Pixels.Span),"Compatibility atlas preserves stored base pixels exactly");
                    Check(SmoTextureDataDecoder.TryDecode(document,document.Objects[font.Atlas.ObjectIndex],out var sourceData,out error),error);
                    var sourceMips=sourceData!.SelectedRepresentation!;var runtimeTexture=font.Atlas.Texture;
                    Check(sourceMips.Kind is SmoTextureRepresentationKind.CrossPlatformBgra32 or SmoTextureRepresentationKind.CrossPlatformBgrx32&&
                        runtimeTexture.HasRuntimeMipChain&&runtimeTexture.MipLevels.Count>=sourceMips.MipLevels.Count,
                        $"Actual runtime chain covers stored levels: {sourceMips.Kind}, stored={sourceMips.MipLevels.Count}, runtime={runtimeTexture.MipLevels.Count}");
                    for(int level=0;level<sourceMips.MipLevels.Count;++level)
                    {
                        var sourceMip=sourceMips.MipLevels[level];var runtimeMip=runtimeTexture.MipLevels[level];
                        Check(runtimeMip.Width==sourceMip.Width&&runtimeMip.Height==sourceMip.Height,"Stored mip dimensions survive runtime transport");
                        if(sourceMips.Kind==SmoTextureRepresentationKind.CrossPlatformBgra32)
                            Check(runtimeMip.Bgra32Pixels.Span.SequenceEqual(sourceMip.PixelData.Span),"Authored BGRA mip survives common transport exactly");
                        else
                        {
                            bool equal=sourceMip.PixelData.Length==runtimeMip.Bgra32Pixels.Length;
                            for(int offset=0;equal&&offset<sourceMip.PixelData.Length;offset+=4)
                                equal=sourceMip.PixelData.Span.Slice(offset,3).SequenceEqual(runtimeMip.Bgra32Pixels.Span.Slice(offset,3))&&runtimeMip.Bgra32Pixels.Span[offset+3]==255;
                            Check(equal,"Authored BGRX color bytes survive and runtime X channel becomes opaque alpha");
                        }
                    }
                    atlasRows.Add(font.Atlas.ObjectIndex,new {font.Atlas.ObjectId,font.Atlas.UsesHostCompatibility,stored.Width,stored.Height,
                        basePixelSha256=Hash(stored.Bgra32Pixels.Span),runtimeMips=font.Atlas.Texture.MipLevelCount,
                        storedMips=sourceMips.MipLevels.Count,storedKind=sourceMips.Kind.ToString(),
                        mipHashes=runtimeTexture.MipLevels.Select(m=>Hash(m.Bgra32Pixels.Span)).ToArray()});
                }
                fontRows.Add(new {font.ObjectId,font.Height,font.Baseline,atlas=font.Atlas.ObjectId});
            }
            foreach(var text in loaded.Texts.Values)
            {
                Check(SmoTextRenderableDecoder.TryDecode(document,document.Objects[text.ObjectIndex],out var wire,out var error),error);
                Check(text.TextPresent&&text.TextBytes.Span.SequenceEqual(new byte[]{0x30})&&wire!.Text=="0", "Selected menu preserves raw digit text");
                Check(text.Font is not null&&ReferenceEquals(text.Font,loaded.Fonts[text.Font.ObjectIndex]),"Shared canonical Font DTO");
                Check(text.Color==wire!.Color&&text.WrapWidth==(wire.WrapWidth??0)&&text.Alignment==(wire.Alignment??0)
                    &&text.MeasuredWidth==text.Font!.Glyphs[16].Width,"Actual text reader/layout values");
                Check(text.Minimum is not null&&text.Maximum is not null&&text.Sphere.W>0,"Runtime bounds available independently of metadata inspector");
                Check(text.Geometry is not null&&text.GeometryIssue is null,text.GeometryIssue??"Text geometry available");
                var geometry=text.Geometry!;
                Check(geometry.Vertices.Count==4&&geometry.Indices.SequenceEqual(new ushort[]{0,1,2,1,3,2}),"One original digit glyph topology");
                Check(ReferenceEquals(geometry.Atlas,text.Font!.Atlas),"Text draw uses canonical Font atlas");
                Check(geometry.MaterialDraw.MaterialObjectIndex==text.Renderable.Material?.Index&&geometry.MaterialDraw.Passes.Count==1&&
                    geometry.MaterialDraw.Passes[0].SpecularPower==geometry.Material?.SpecularPower,"Text keeps actual custom material identity and power presence");
                Check(geometry.Vertices.All(v=>v.Color==text.Color)&&text.AlphaSortData?.RequiresQueue==true,"Original vertex ARGB and Text alpha gate");
                textRows.Add(new {text.ObjectId,font=text.Font!.ObjectId,text.MeasuredWidth,text.Color,text.WrapWidth,text.Alignment});
            }
            foreach(var node in loaded.TextNodeCachedTexts)
            {
                Check(node.Value is int index&&loaded.Texts.ContainsKey(index),"TextNode caches loaded Text identity");
                Check(loaded.RenderContainersByObjectIndex[node.Key].RenderableObjectIndices.Contains(node.Value!.Value),"Cached Text is an actual support member");
            }
            foreach(var group in loaded.Fonts.Values.GroupBy(font=>font.Atlas!.ObjectIndex))
                Check(group.All(font=>ReferenceEquals(font.Atlas,group.First().Atlas)),"Shared atlas is uploaded from one texture DTO");
            using(var runtime=new SparkplugSceneRuntime(document,new Dictionary<int,SmoSkin>()))
            {
                Check(runtime.CompatibilityIssues.SequenceEqual(loaded.CompatibilityIssues),"Live and immutable graph expose the same compatibility issues");
                foreach(float time in new[]{0f,1f,2f}){runtime.Sample(time);Check(runtime.Worlds.Count==loaded.Nodes.Count,"Live scene keeps all original Nodes");}
            }
            var prepared=SmoSceneBuilder.Build(document);
            Check(prepared.Texts.Count==loaded.RenderOccurrences.Count(o=>loaded.Texts.ContainsKey(o.RenderableObjectIndex)),"Every actual Text occurrence prepared separately from Mesh");
            Check(loaded.CompatibilityIssues.All(issue=>prepared.TextureIssues.Contains(issue)),"Prepared scene retains host compatibility diagnostics");
            Check(Hash(File.ReadAllBytes(source))==sourceHash,"Original source file unchanged");
            detail=new {nodes=loaded.Nodes.Count,models=loaded.Models.Count,containers=loaded.RenderContainers.Count,
                occurrences=loaded.RenderOccurrences.Count,compatibility=loaded.CompatibilityIssues,fonts=fontRows,texts=textRows,atlases=atlasRows.Values,
                meshes=prepared.Meshes.Count,skinnedMeshes=prepared.Meshes.Count(mesh=>mesh.Mesh.HasSkinningData),prepared.DecodeErrors,prepared.TextureIssues};
        }
        catch(Exception exception){failure=exception.ToString();}
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        var report=new {source=Path.GetFullPath(source),sourceSha256=sourceHash,checks,failure,detail,
            elapsedSeconds=watch.Elapsed.TotalSeconds,peakBytes=Process.GetCurrentProcess().PeakWorkingSet64,
            native=Hash(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll"))),
            core=Hash(File.ReadAllBytes(typeof(SmoLoadedResources).Assembly.Location))};
        File.WriteAllText(output,JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine(JsonSerializer.Serialize(new {checks,failure,report.elapsedSeconds,report.peakBytes}));return failure is null?0:1;
    }
}
