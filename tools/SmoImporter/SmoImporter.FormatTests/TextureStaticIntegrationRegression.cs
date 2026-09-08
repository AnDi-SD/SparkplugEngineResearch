using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class TextureStaticIntegrationRegression
{
    internal static int Run(string input,string output,int parts=1,bool independentTextures=false)
    {
        try
        {
            input=Path.GetFullPath(input);output=Path.GetFullPath(output);
            if(parts is <1 or >2)throw new ArgumentOutOfRangeException(nameof(parts));
            if(independentTextures && parts!=2)throw new ArgumentException("Independent textures require two parts");
            if(input.Equals(output,StringComparison.OrdinalIgnoreCase) || File.Exists(output))
                throw new InvalidOperationException("A separate, new output is required");
            byte[] source=File.ReadAllBytes(input);var doc=SmoDocument.ParseOwned(source);
            using var image=new Image<Rgba32>(17,9,new Rgba32(17,71,139,53));
            using var stream=new MemoryStream();image.SaveAsPng(stream);
            var texture=new ImportedTexture("proof","image/png",17,9,stream.ToArray());
            var donor=new ImportedScene(new[] {new ImportedMesh("proof",
                new[] {Vector3.Zero,Vector3.UnitX,Vector3.UnitY},new[] {Vector3.UnitZ,Vector3.UnitZ,Vector3.UnitZ},
                new[] {Vector2.Zero,Vector2.UnitX,Vector2.UnitY},new uint[] {0,1,2},new uint[] {0xffffffff,0xffffffff,0xffffffff},0)},
                new[] {texture},new[] {new ImportedMaterial("proof","proof",0,ImportedMaterialAlphaMode.Opaque)});
            if(parts==2)donor=donor with {Meshes=new[] {donor.Meshes[0],donor.Meshes[0] with
                {Name="proof-second",Positions=donor.Meshes[0].Positions.Select(v=>v+new Vector3(2,0,0)).ToArray()}}};
            if(independentTextures)
            {
                using var secondImage=new Image<Rgba32>(8,8,new Rgba32(221,11,31,255));
                using var secondStream=new MemoryStream();secondImage.SaveAsPng(secondStream);
                donor=donor with {Meshes=new[] {donor.Meshes[0],donor.Meshes[1] with {MaterialIndex=1}},
                    EmbeddedTextures=new[] {texture,new ImportedTexture("second","image/png",8,8,secondStream.ToArray())},
                    SourceMaterials=new[] {donor.Materials[0],new ImportedMaterial("second","second",1)}};
            }
            int index=doc.Objects.First(e=>e.TypeHash==SmoClassIds.MeshData).Index;
            // Explicit opaque policy keeps this probe focused on texture graph
            // serialization; raw alpha is still preserved in the native pixels.
            Directory.CreateDirectory(Path.GetDirectoryName(output)!);
            var result=SmoLevelModelGraphReplacer.ReplaceFile(doc,index,donor,ReplacementTransform.Identity,output,SmoLevelModelGraphWriteOptions.StandaloneStatic);
            var after=SmoDocument.Load(output);int checks=0;
            void Check(bool ok,string reason){checks++;if(!ok)throw new InvalidDataException(reason);}
            int textureCount=independentTextures?2:1;
            Check(!after.HasErrors && result.MeshCount==parts && result.TextureCount==textureCount,"full static graph write");
            Check(File.ReadAllBytes(input).AsSpan().SequenceEqual(source),"source remains unchanged");
            var meshes=after.Objects.Where(e=>e.TypeHash==SmoClassIds.MeshData).ToArray();
            Check(meshes.Length==parts && meshes.All(m=>SmoMeshDecoder.Decode(after,m).TriangleCount==1),"actual donor triangles");
            var bindings=SmoTextureBindingResolver.ResolveAll(after);var binding=bindings[meshes[0].Index];
            Check(binding.Issue is null && binding.Texture is {Width:17,Height:9},"resolved new native texture");
            int textureIndex=binding.Texture!.ObjectIndex;
            Check(meshes.All(m=>bindings[m.Index].Texture is not null && bindings[m.Index].Issue is null) &&
                meshes.Select(m=>bindings[m.Index].Texture!.ObjectIndex).Distinct().Count()==textureCount,"shared/independent donor texture bindings");
            Check(SmoTextureDataDecoder.TryDecode(after,after.Objects[textureIndex],out var data,out _) &&
                data is {SourceKind:SmoTextureSourceKind.Embedded,CrossPlatform:null,PlatformSpecific.Kind:SmoTextureRepresentationKind.Direct3DBgra32},
                "native PC source wrapper in installed graph");
            File.WriteAllText(Path.ChangeExtension(output,"json"),JsonSerializer.Serialize(new {status="passed",checks,
                input,inputSha256=Convert.ToHexString(SHA256.HashData(source)),output,outputSha256=result.Sha256,
                objects=after.Objects.Count,textureIndex,result.MeshCount,result.TextureCount,result.TriangleCount},new JsonSerializerOptions {WriteIndented=true}));
            Console.WriteLine($"PASS {checks} full static checks: {after.Objects.Count} objects, texture[{textureIndex}]");return 0;
        }
        catch(Exception error){Console.Error.WriteLine(error);return 1;}
    }

    internal static int VerifyRejectedForwardReference(string input)
    {
        var doc=SmoDocument.Load(input);
        try {SmoLevelModelGraphReplacer.VerifyTextureReferenceOrder(doc,doc.Objects.Where(e=>e.TypeHash==SmoClassIds.TextureData).Select(e=>e.Id));}
        catch(InvalidDataException error){Console.WriteLine("PASS preserved broken output rejected: "+error.Message);return 0;}
        Console.Error.WriteLine("Expected a texture forward reference rejection");return 1;
    }
}
