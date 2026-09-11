using System.Security.Cryptography;
using System.Text.Json;
using System.Numerics;
using SmoLVLcreator.Core;
using SmoViewer.Core;

namespace SmoLVLcreator.CoreTests;

internal static class SmoOccurrenceWorkspaceRegression
{
    internal static int RunEdit(string path,string output)
    {
        if(Directory.Exists(output))throw new InvalidDataException("Occurrence edit evidence requires a fresh output directory.");
        Directory.CreateDirectory(output);int checks=0;
        void Check(bool value,string message){if(!value)throw new InvalidDataException(message);++checks;}
        var watch=System.Diagnostics.Stopwatch.StartNew();byte[] original=File.ReadAllBytes(path);
        var workspace=SmoLevelWorkspace.Load(path);var level=new SmoLevelDocument(workspace);
        var loaded=SmoLoadedResources.Get(workspace.Document);
        var repeated=level.Placements.GroupBy(p=>(p.Id.MeshObjectIndex,p.Id.SceneObjectIndex))
            .Where(group=>group.Count()>1).SelectMany(group=>group);
        var candidates=repeated.Select(p=>new{p.Id,Owner=p.Entity.Id.SceneObjectIndex,
            Type=workspace.Document.Objects[p.Entity.Id.SceneObjectIndex].TypeHash,Writable=level.CanPersistTransform(p.Entity.Id),
            Children=loaded.Nodes.Values.Count(node=>node.ParentObjectIndex==p.Entity.Id.SceneObjectIndex)}).ToArray();
        File.WriteAllText(Path.Combine(output,"candidates.json"),JsonSerializer.Serialize(candidates,new JsonSerializerOptions{WriteIndented=true}));
        var selected=repeated.First(p=>level.CanPersistTransform(p.Entity.Id)
            &&workspace.Document.Objects[p.Entity.Id.SceneObjectIndex].TypeHash==SmoClassIds.RenderNode
            &&!loaded.Nodes.Values.Any(node=>node.ParentObjectIndex==p.Entity.Id.SceneObjectIndex));
        var legacy=new SmoPlacementId(selected.Id.MeshObjectIndex,selected.Id.SceneObjectIndex);
        Check(!level.TryGetPlacement(legacy,out _),"Ambiguous old command identity cannot silently pick a different instance");
        var baseline=level.Placements.ToDictionary(p=>p.Id,p=>p.WorldTransform);
        var desired=selected.WorldTransform*Matrix4x4.CreateTranslation(1.25f,-.5f,.25f);
        Check(level.SetPlacementTransform(selected.Id,desired),"Addressed repeated occurrence accepts transform command");
        foreach(var p in level.Placements)Check(p.WorldTransform==(p.Entity.Id==selected.Entity.Id?desired:baseline[p.Id]),"Transform changes only the actual owning container");
        Check(level.Undo()&&level.Placements.All(p=>p.WorldTransform==baseline[p.Id]),"Undo restores every occurrence exactly");
        Check(level.Redo()&&selected.WorldTransform==desired,"Redo targets the same occurrence");
        var export=SmoLevelExportService.BuildSelectionScene(level,[selected.Entity.Id]);
        var expectedKeys=selected.Entity.Parts.Select(p=>p.Id.OccurrenceKey).ToHashSet();
        Check(export.MeshPlacements.Count==expectedKeys.Count&&export.MeshPlacements.All(p=>expectedKeys.Contains(p.OccurrenceKey)),"Selection export keeps exact support slots");
        string outputPath=Path.Combine(output,"edited.smo");string jobPath=SmoLevelSaveJob.Write(level,outputPath,Path.Combine(output,"job"));
        var job=SmoLevelSaveJob.Read(jobPath);Check(job.EntityTransforms.Count==1&&job.EntityTransforms[0].EntityIndex==selected.Entity.Id.SceneObjectIndex,"Save job addresses the real container object");
        var saved=job.Execute();Check(saved.PatchedTransformCount==1,"Save patched one actual transform owner");
        var reopened=new SmoLevelDocument(SmoLevelWorkspace.Load(outputPath));
        Check(reopened.Placements.Count==level.Placements.Count,"Save preserves all original reference occurrences");
        float maximum=0;
        foreach(var p in reopened.Placements)
        {
            var expected=level.GetPlacement(p.Id).WorldTransform;float error=Difference(p.WorldTransform,expected);maximum=Math.Max(maximum,error);
            Check(error<=.0001f,"Reopened shared game graph has the requested occurrence worlds");
        }
        Check(original.AsSpan().SequenceEqual(File.ReadAllBytes(path)),"Original input file remains byte-identical");
        var report=new{status="passed",checks,seconds=watch.Elapsed.TotalSeconds,placements=level.Placements.Count,
            selected=selected.Id,owner=selected.Entity.Id.SceneObjectIndex,maximum_world_error=maximum,patched=saved.PatchedTransformCount,
            source_sha256=Convert.ToHexString(SHA256.HashData(original)),output_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(outputPath))),
            core_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(typeof(SmoLevelDocument).Assembly.Location))),
            native_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll"))))};
        File.WriteAllText(Path.Combine(output,"report.json"),JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true})+"\n");
        Console.WriteLine($"Occurrence edit/save: owner {selected.Entity.Id.SceneObjectIndex}; {checks} checks");return 0;
    }
    private static float Difference(Matrix4x4 a,Matrix4x4 b)
    {
        float[] x=[a.M11,a.M12,a.M13,a.M14,a.M21,a.M22,a.M23,a.M24,a.M31,a.M32,a.M33,a.M34,a.M41,a.M42,a.M43,a.M44];
        float[] y=[b.M11,b.M12,b.M13,b.M14,b.M21,b.M22,b.M23,b.M24,b.M31,b.M32,b.M33,b.M34,b.M41,b.M42,b.M43,b.M44];
        return x.Zip(y,(u,v)=>Math.Abs(u-v)).Max();
    }
    internal static int Run(string path, string output)
    {
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        var workspace = SmoLevelWorkspace.Load(path);
        var placements = workspace.Assets.SelectMany(asset => asset.Placements.Select(value => (asset, value))).ToArray();
        Check(placements.Length == workspace.PreparedScene.Meshes.Count, "Workspace retains every actual prepared occurrence");
        Check(placements.All(value => value.value.OccurrenceKey.HasValue), "Workspace preserves slot identity");
        Check(placements.Select(value => value.value.OccurrenceKey).Distinct().Count() == placements.Length, "Workspace does not merge repeated slots");
        foreach (var item in placements)
        {
            var entry = item.value;
            Check(entry.ModelObjectIndex == entry.SceneObjectIndex, "Placement names and selection use actual consuming Model/Skin");
            Check(entry.Name == workspace.Document.Objects[entry.ModelObjectIndex!.Value].Name, "Shared geometry does not borrow another Model name");
        }
        bool repeats = placements.GroupBy(value => (value.asset.ObjectIndex, value.value.SceneObjectIndex)).Any(group => group.Count() > 1);
        var editable = new SmoLevelDocument(workspace);
        int editablePlacements=editable.Placements.Count;
        Check(editablePlacements==placements.Length,"Every actual slot reaches the editor command model");
        foreach(var group in editable.Placements.GroupBy(p=>new SmoPlacementId(p.Id.MeshObjectIndex,p.Id.SceneObjectIndex)))
        {
            Check(editable.TryGetPlacement(group.Key,out _)==(group.Count()==1),"Legacy identity resolves only an unambiguous occurrence");
            foreach(var p in group)
            {
                Check(ReferenceEquals(editable.GetPlacement(p.Id),p),"Exact support slot resolves the canonical editor placement");
                Check(p.Id.OccurrenceKey==p.Source.OccurrenceKey,"Command ID retains actual occurrence identity");
                var container=SmoViewer.Core.SmoLoadedResources.Get(workspace.Document).RenderContainersByObjectIndex[p.Id.OccurrenceKey!.Value.ContainerObjectIndex];
                if(container.Kind is SmoViewer.Core.SmoRenderContainerKind.RenderNode or SmoViewer.Core.SmoRenderContainerKind.StaticRenderObject)
                    Check(p.Entity.Id.SceneObjectIndex==container.ObjectIndex,"Editor transform belongs to actual support, not physical Model storage ancestry");
            }
        }
        var report = new
        {
            status = "passed", path = Path.GetFullPath(path), checks, placements = placements.Length,
            actual_models = workspace.PreparedScene.Meshes.Select(value => value.RenderableObjectIndex).Distinct().Count(),
            repeats, authoring_issue = (string?)null, editable_placements = editablePlacements,
            native_dll_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll"))))
        };
        File.WriteAllText(output, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Occurrence workspace: {placements.Length} editable placements, repeats={repeats}, {checks} checks");
        return 0;
    }
}
