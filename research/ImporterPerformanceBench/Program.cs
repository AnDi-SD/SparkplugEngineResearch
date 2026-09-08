using System.Diagnostics;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoImporter.Core;
using SmoViewer.Core;

if(args.Length!=4)throw new ArgumentException("TARGET_SMO DONOR_OBJ NEW_OUTPUT_DIR ITERATIONS_1_TO_9");
string targetPath=Path.GetFullPath(args[0]),donorPath=Path.GetFullPath(args[1]),directory=Path.GetFullPath(args[2]);
int iterations=int.Parse(args[3]);if(iterations is <1 or >9)throw new ArgumentOutOfRangeException(nameof(iterations));
if(Directory.Exists(directory))throw new InvalidOperationException("A new output directory is required");
Directory.CreateDirectory(directory);
try
{
    var target=SmoDocument.Load(targetPath);var source=ImportedModelReader.ReadGeometryOnly(donorPath);
    string sourceHash=Hash(donorPath),targetHash=Hash(targetPath);
    var textures=source.Materials.Select(m=>m.BaseColorTextureName).Where(n=>!string.IsNullOrWhiteSpace(n))
        .Select(n=>Path.Combine(Path.GetDirectoryName(donorPath)!,Path.GetFileName(n!)))
        .Distinct(StringComparer.OrdinalIgnoreCase).Where(File.Exists).Select(ImportedTextureFileReader.Read).ToArray();
    source=ImportedTextureCatalog.ResolveExternalOverrides(source,textures).EffectiveScene;
    var rig=TargetRigDefinition.FromSmoDocument(target);
    var inverse=rig.Joints.Select(j=>Matrix4x4.Invert(j.BindWorldMatrix,out var m)?m:throw new InvalidDataException("Singular bind")).ToArray();
    var skeleton=new ImportedSkeleton("importer_performance_target_rig",rig.Joints.Select(j=>j.Name).ToArray(),inverse)
    {
        ParentJointIndices=rig.Joints.Select(j=>j.ParentJointIndex).ToArray(),
        BindWorldMatrices=rig.Joints.Select(j=>j.BindWorldMatrix).ToArray(),
        BindLocalMatrices=rig.Joints.Select(j=>j.BindLocalMatrix).ToArray()
    };
    int pelvis=rig.GetJointIndex("Pelvis");
    // A declared benchmark rig isolates import/texture/graph processing from
    // the separate automatic weighting algorithm. Source geometry stays intact.
    var donor=source with {Meshes=source.Meshes.Select(m=>m with {Skinning=new ImportedSkinning(skeleton,
        Enumerable.Repeat(new ImportedJointIndices(checked((ushort)pelvis),0,0,0),m.Positions.Length).ToArray(),
        Enumerable.Repeat(Vector4.UnitX,m.Positions.Length).ToArray())}).ToArray()};
    using var process=Process.GetCurrentProcess();
    var rows=new List<object>();var times=new List<(double Analyze,double Preview,double Write)>();string? expected=null;
    for(int round=-2;round<iterations;round++)
    {
        long allocated=GC.GetTotalAllocatedBytes(true);TimeSpan cpu=process.TotalProcessorTime;var watch=Stopwatch.StartNew();
        var plan=SmoSkinnedGlbReplacer.Analyze(target,donor);double analyze=watch.Elapsed.TotalMilliseconds;
        if(!plan.CanReplace)throw new InvalidDataException(string.Join(" | ",plan.Messages));
        double analyzeCpuMs=(process.TotalProcessorTime-cpu).TotalMilliseconds;
        long analyzeBytes=GC.GetTotalAllocatedBytes(true)-allocated;
        allocated=GC.GetTotalAllocatedBytes(true);cpu=process.TotalProcessorTime;watch.Restart();
        var preview=SmoSkinnedGlbReplacer.PrepareGeometryPreview(target,donor,ReplacementTransform.Identity,SkinnedGeometryTransferMode.PreservePreparedGeometry);
        double previewMs=watch.Elapsed.TotalMilliseconds;long previewBytes=GC.GetTotalAllocatedBytes(true)-allocated;
        double previewCpuMs=(process.TotalProcessorTime-cpu).TotalMilliseconds;
        allocated=GC.GetTotalAllocatedBytes(true);cpu=process.TotalProcessorTime;watch.Restart();
        string output=Path.Combine(directory,$"round-{round+2:D2}.smo");
        var result=SmoSkinnedGlbReplacer.Replace(target,donor,ReplacementTransform.Identity,output,SkinnedGeometryTransferMode.PreservePreparedGeometry);
        double write=watch.Elapsed.TotalMilliseconds;long writeBytes=GC.GetTotalAllocatedBytes(true)-allocated;
        double writeCpuMs=(process.TotalProcessorTime-cpu).TotalMilliseconds;
        if(result.TriangleCount!=source.Meshes.Sum(m=>m.TriangleIndices.Length/3))throw new InvalidDataException("Lost donor triangles");
        expected??=result.Sha256;if(result.Sha256!=expected)throw new InvalidDataException("Nondeterministic output");
        if(preview.Meshes.Count!=donor.Meshes.Count || preview.Meshes.Any(m=>m.Skinning is not null))throw new InvalidDataException("Invalid preview");
        rows.Add(new {round,warmup=round<0,analyzeMs=analyze,previewMs,writeMs=write,analyzeCpuMs,previewCpuMs,writeCpuMs,analyzeBytes,previewBytes,writeBytes,output,sha256=result.Sha256,result.FileSize,result.TriangleCount,result.VertexCount,result.PaletteCount});
        if(round>=0)times.Add((analyze,previewMs,write));
        Console.WriteLine($"{round}: analyze {analyze:F2} ms, preview {previewMs:F2} ms, write {write:F2} ms");
    }
    if(Hash(targetPath)!=targetHash || Hash(donorPath)!=sourceHash)throw new InvalidDataException("Changed source files");
    double Median(IEnumerable<double> values){var a=values.Order().ToArray();return a[a.Length/2];}
    File.WriteAllText(Path.Combine(directory,"report.json"),JsonSerializer.Serialize(new {kind="importer-real-donor-benchmark",status="passed",targetPath,targetSha256=targetHash,donorPath,donorSha256=sourceHash,
        meshes=source.Meshes.Count,triangles=source.Meshes.Sum(m=>m.TriangleIndices.Length/3),textures=source.Textures.Select(t=>new {t.Name,t.Width,t.Height,sha256=Convert.ToHexString(SHA256.HashData(t.Data))}),
        warmups=2,iterations,medianAnalyzeMs=Median(times.Select(t=>t.Analyze)),medianPreviewMs=Median(times.Select(t=>t.Preview)),medianWriteMs=Median(times.Select(t=>t.Write)),
        peakWorkingSet=process.PeakWorkingSet64,coreAssemblySha256=Hash(typeof(SmoSkinnedGlbReplacer).Assembly.Location),benchmarkAssemblySha256=Hash(typeof(Program).Assembly.Location),
        rows,scope="Actual OBJ geometry and PNG inputs; declared synthetic Pelvis weighting; public analyze/preview/verified-write operations. Not auto-weight quality or native game-render proof."},new JsonSerializerOptions {WriteIndented=true}));
    return 0;
}
catch(Exception error)
{
    File.WriteAllText(Path.Combine(directory,"failure.txt"),error.ToString());Console.Error.WriteLine(error);return 1;
}

static string Hash(string path)=>Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
