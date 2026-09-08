using System.Diagnostics;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class CleanSkinnedTargetRegression
{
    internal static int Run(string[] args)
    {
        bool probe=args[0]=="--probe";
        if(probe)args=args.Skip(1).ToArray();
        if(args.Length is <2 or >9)throw new ArgumentException("[--probe] NEW_OUTPUT_DIR TARGET [up to eight targets]");
        string directory=Path.GetFullPath(args[0]);
        if(Directory.Exists(directory))throw new InvalidOperationException("Use a new output directory to preserve prior evidence");
        Directory.CreateDirectory(directory);
        var watch=Stopwatch.StartNew();var rows=new List<object>();int checks=0,failures=0;
        foreach(string input in args.Skip(1))
        {
            string targetPath=Path.GetFullPath(input);byte[] original=File.ReadAllBytes(targetPath);
            string output=Path.Combine(directory,Path.GetFileNameWithoutExtension(input)+"-clean.smo");
            string inputSha=Convert.ToHexString(SHA256.HashData(original));
            try
            {
                var target=SmoDocument.ParseOwned(original);var donor=MakeDonor(target);
                var plan=SmoSkinnedGlbReplacer.Analyze(target,donor);
                Check(plan.CanReplace,"analysis: "+string.Join(" | ",plan.Messages));
                var result=SmoSkinnedGlbReplacer.Replace(target,donor,ReplacementTransform.Identity,output,SkinnedGeometryTransferMode.PreservePreparedGeometry);
                var after=SmoDocument.Load(output);
                Check(!after.HasErrors && result.TriangleCount==2,"strict installed output and both donor triangles");
                var originalVisualIds=target.Objects.Where(e=>e.TypeHash is SmoClassIds.MeshData or SmoClassIds.MaterialData or SmoClassIds.TextureData).Select(e=>e.Id).ToHashSet();
                Check(after.Objects.All(e=>!originalVisualIds.Contains(e.Id)),"all original visual resources removed");
                var textures=after.Objects.Where(e=>e.TypeHash==SmoClassIds.TextureData).ToArray();
                Check(textures.Length==2,"two independent donor textures");
                var dimensions=textures.Select(e=>SmoTextureDecoder.TryDecode(after,e,out var texture,out var error) && texture is not null?texture:throw new InvalidDataException(error)).Select(t=>(t.Width,t.Height)).OrderBy(s=>s.Width).ToArray();
                Check(dimensions.SequenceEqual(new[] {(8,8),(17,9)}),"native donor dimensions including NPOT");
                var nodes=target.Objects.Where(e=>e.TypeHash==SmoClassIds.Node).ToArray();
                Check(nodes.All(n=>after.Objects.Any(e=>e.Id==n.Id && e.TypeHash==n.TypeHash && e.Name==n.Name)),"all original skeleton nodes retained");
                Check(nodes.All(n=>
                {
                    var other=after.Objects.Single(e=>e.Id==n.Id);
                    bool beforeHas=SmoNodeTransformDecoder.TryDecode(target,n,out var beforeTransform);
                    bool afterHas=SmoNodeTransformDecoder.TryDecode(after,other,out var afterTransform);
                    return beforeHas==afterHas && (!beforeHas || beforeTransform!.LocalMatrix==afterTransform!.LocalMatrix);
                }),"all original node local transforms unchanged");
                Check(NodeLinks(target).SetEquals(NodeLinks(after)),"logical skeleton hierarchy unchanged");
                Check(target.Objects.Where(e=>e.TypeHash is 0x47A97C0E or 0x4DA04889).All(e=>
                {
                    var other=after.Objects.Single(o=>o.Id==e.Id && o.TypeHash==e.TypeHash);
                    return target.Data.Span.Slice((int)e.PhysicalOffset,(int)e.SerializedSize).SequenceEqual(
                        after.Data.Span.Slice((int)other.PhysicalOffset,(int)other.SerializedSize));
                }),"preserved collision info and OBB payloads byte-identical");
                var bindings=SmoTextureBindingResolver.ResolveAll(after);
                var visualMeshes=after.Objects.Where(e=>e.TypeHash==SmoClassIds.MeshData).Where(e=>SmoMeshDecoder.Decode(after,e).VertexCount>1).ToArray();
                Check(visualMeshes.Length==2 && visualMeshes.All(e=>bindings.TryGetValue(e.Index,out var b) && b.Issue is null && b.Texture is not null),"explicit generated texture bindings");
                Check(File.ReadAllBytes(targetPath).AsSpan().SequenceEqual(original),"source file unchanged");
                int guards=VerifyNoTemplateRejection(target,donor,Path.Combine(directory,Path.GetFileNameWithoutExtension(input)+"-rejected.smo"));
                checks+=guards;
                rows.Add(new {input=targetPath,inputSha256=inputSha,status="passed",output,outputSha256=result.Sha256,objects=after.Objects.Count,nodes=nodes.Length,result.VertexCount,result.TriangleCount,result.PaletteCount,bytes=after.Data.Length,guards});
                Console.WriteLine($"PASS {Path.GetFileName(input)}: {after.Objects.Count} objects, {after.Data.Length} bytes");
            }
            catch(Exception error)
            {
                failures++;rows.Add(new {input=targetPath,inputSha256=inputSha,status="failed",error=error.ToString(),outputExists=File.Exists(output)});
                Console.WriteLine($"FAIL {Path.GetFileName(input)}: {error.Message}");
            }
        }
        File.WriteAllText(Path.Combine(directory,"report.json"),JsonSerializer.Serialize(new {kind="clean-skinned-target-regression",status=failures==0?"passed":"failed",checks,failures,rows,elapsedSeconds=watch.Elapsed.TotalSeconds,peakWorkingSet=Process.GetCurrentProcess().PeakWorkingSet64},new JsonSerializerOptions {WriteIndented=true}));
        return failures==0 || probe?0:1;
        void Check(bool value,string message){checks++;if(!value)throw new InvalidDataException(message);}
    }

    private static HashSet<(uint,uint)> NodeLinks(SmoDocument document)=>SmoNodeHierarchy.Decode(document).Links
        .Where(l=>document.Objects[l.ParentObjectIndex].TypeHash==SmoClassIds.Node && document.Objects[l.ChildObjectIndex].TypeHash==SmoClassIds.Node)
        .Select(l=>(document.Objects[l.ParentObjectIndex].Id,l.ChildObjectId)).ToHashSet();

    private static int VerifyNoTemplateRejection(SmoDocument target,ImportedScene donor,string output)
    {
        var current=target;
        foreach(var material in target.Objects.Where(e=>e.TypeHash==SmoClassIds.MaterialData).OrderByDescending(e=>e.PhysicalOffset))
        {
            var entry=current.Objects.SingleOrDefault(e=>e.Id==material.Id);
            if(entry?.ParentIndex is not int parent)continue;
            current=SmoDocument.Parse(SmoVisualForestInjector.RemoveInlineBranch(current,current.Objects[parent].Id,entry.Id));
        }
        if(current.HasErrors)throw new InvalidDataException("No-template guard fixture must remain structurally valid");
        var plan=SmoSkinnedGlbReplacer.Analyze(current,donor);
        if(plan.CanReplace)throw new InvalidDataException("A target without an explicit material template must be rejected");
        byte[] sentinel="previous user output"u8.ToArray();File.WriteAllBytes(output,sentinel);
        bool rejected=false;
        try {SmoSkinnedGlbReplacer.Replace(current,donor,ReplacementTransform.Identity,output,SkinnedGeometryTransferMode.PreservePreparedGeometry);}
        catch(InvalidOperationException){rejected=true;}
        if(!rejected || !File.ReadAllBytes(output).AsSpan().SequenceEqual(sentinel))throw new InvalidDataException("Rejected target must preserve the prior output");
        return 2;
    }

    private static ImportedScene MakeDonor(SmoDocument target)
    {
        var rig=TargetRigDefinition.FromSmoDocument(target);
        var inverse=rig.Joints.Select(j=>Matrix4x4.Invert(j.BindWorldMatrix,out var m)?m:throw new InvalidDataException("Singular bind matrix")).ToArray();
        var skeleton=new ImportedSkeleton("target-template-regression",rig.Joints.Select(j=>j.Name).ToArray(),inverse)
        {
            ParentJointIndices=rig.Joints.Select(j=>j.ParentJointIndex).ToArray(),
            BindWorldMatrices=rig.Joints.Select(j=>j.BindWorldMatrix).ToArray(),
            BindLocalMatrices=rig.Joints.Select(j=>j.BindLocalMatrix).ToArray()
        };
        var meshes=new List<ImportedMesh>();var textures=new List<ImportedTexture>();var materials=new List<ImportedMaterial>();
        foreach(var item in new[] {(Name:"Pelvis",Width:17,Height:9,Color:new Rgba32(17,71,139,255)),(Name:"Head",Width:8,Height:8,Color:new Rgba32(221,11,31,255))})
        {
            int index=meshes.Count,joint=rig.GetJointIndex(item.Name);
            using var image=new Image<Rgba32>(item.Width,item.Height,item.Color);using var stream=new MemoryStream();image.SaveAsPng(stream);
            textures.Add(new ImportedTexture(item.Name,"image/png",item.Width,item.Height,stream.ToArray()));
            materials.Add(new ImportedMaterial(item.Name,item.Name,index));
            Vector3 offset=new(index*2,0,0);
            meshes.Add(new ImportedMesh(item.Name,new[] {offset,offset+Vector3.UnitX,offset+Vector3.UnitY},
                new[] {Vector3.UnitZ,Vector3.UnitZ,Vector3.UnitZ},new[] {Vector2.Zero,Vector2.UnitX,Vector2.UnitY},
                new uint[] {0,1,2},new uint[] {0xffffffff,0xffffffff,0xffffffff},index)
                {Skinning=new ImportedSkinning(skeleton,Enumerable.Repeat(new ImportedJointIndices(checked((ushort)joint),0,0,0),3).ToArray(),Enumerable.Repeat(Vector4.UnitX,3).ToArray())});
        }
        return new ImportedScene(meshes,textures,materials);
    }
}
