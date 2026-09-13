using System.Collections;
using System.Diagnostics;
using System.IO;
using System.Numerics;
using System.Reflection;
using System.Text.Json;
using System.Windows;
using SmoLVLcreator.Core;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Scene;

internal static class LvlOccurrenceRendererRegression
{
    private const BindingFlags Private = BindingFlags.Instance | BindingFlags.NonPublic;
    private static object Get(object obj,string name) => obj.GetType().GetField(name,Private)!.GetValue(obj)!;
    private static object? Call(object obj,string name,params object?[] args) => obj.GetType().GetMethod(name,Private)!.Invoke(obj,args);
    private static T Property<T>(object obj,string name) => (T)obj.GetType().GetProperty(name)!.GetValue(obj)!;

    internal static int Run(string source,string output)
    {
        if(File.Exists(output))throw new IOException("Fresh LVL GUI report required.");
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        int checks=0;string failure="";var timer=Stopwatch.StartNew();
        void Check(bool ok,string message){++checks;if(!ok)throw new InvalidDataException(message);}
        var app = new Application { ShutdownMode=ShutdownMode.OnExplicitShutdown };
        var window = new SmoLVLcreator.Gui.MainWindow { ShowInTaskbar=false };
        try
        {
            var workspace=SmoLevelWorkspace.Load(source);
            Call(window,"AttachWorkspace",workspace,null,false);
            var level=(SmoLevelDocument)Get(window,"_document");
            var renderer=(SmoGpuSceneRenderer)Get(window,"_gpuRenderer");
            var items=((IEnumerable)Get(renderer,"_items")).Cast<object>().ToArray();
            var byKey=items.ToDictionary(item=>Property<SmoRenderObjectKey>(item,"Key"));
            var appearances=(IDictionary)Get(renderer,"_appearances");
            Check(items.Length==level.Placements.Count&&appearances.Count==items.Length,"Every real slot owns a renderer and appearance identity");
            foreach(var placement in level.Placements)
                Check(byKey.ContainsKey(new(0,placement.Id.SceneObjectIndex,placement.Id.OccurrenceKey)),"Actual window retains the slot in the GPU key");
            var loaded=SmoLoadedResources.Get(workspace.Document);
            var repeated=level.Placements.GroupBy(p=>(p.Id.MeshObjectIndex,p.Id.SceneObjectIndex)).Where(g=>g.Count()>1).SelectMany(g=>g);
            var selected=repeated.First(p=>level.CanPersistTransform(p.Entity.Id)&&
                workspace.Document.Objects[p.Entity.Id.SceneObjectIndex].TypeHash==SmoClassIds.RenderNode&&
                !loaded.Nodes.Values.Any(n=>n.ParentObjectIndex==p.Entity.Id.SceneObjectIndex));
            var selection=(HashSet<SmoLevelEntityId>)Get(window,"_selectedEntities");selection.Add(selected.Entity.Id);
            Call(window,"ApplyPlacementHighlights");
            foreach(var placement in level.Placements)
            {
                var key=new SmoRenderObjectKey(0,placement.Id.SceneObjectIndex,placement.Id.OccurrenceKey);
                Check(Property<bool>(appearances[key]!,"Highlighted")== (placement.Entity.Id==selected.Entity.Id),"Highlight reaches exactly the owning container's slots");
            }
            var baseline=level.Placements.ToDictionary(p=>p.Id,p=>p.WorldTransform);
            Check(level.SetPlacementTransform(selected.Id,selected.WorldTransform*Matrix4x4.CreateTranslation(1.25f,-.5f,.25f)),"Real command changes the chosen owner");
            foreach(var placement in level.Placements)
            {
                var key=new SmoRenderObjectKey(0,placement.Id.SceneObjectIndex,placement.Id.OccurrenceKey);
                var currentItems=((IEnumerable)Get(renderer,"_items")).Cast<object>();
                var current=currentItems.Single(item=>Property<SmoRenderObjectKey>(item,"Key")==key);
                Check(Property<Matrix4x4>(current,"Model")==placement.WorldTransform*Matrix4x4.CreateScale(1,1,-1),
                    $"Actual Changed handler world {key}: GPU={Property<Matrix4x4>(current,"Model")}, core={placement.WorldTransform}");
                var mesh=workspace.PreparedScene.Meshes.Single(m=>m.SceneObjectIndex==placement.Id.SceneObjectIndex&&m.OccurrenceKey==placement.Id.OccurrenceKey);
                var picking=typeof(SmoLVLcreator.Gui.MainWindow).GetMethod("ScenePickingKey",BindingFlags.Static|BindingFlags.NonPublic)!.Invoke(null,[mesh])!;
                Check(((ValueTuple<SmoRenderObjectKey,int>)picking).Item1==key,"Picking uses the same exact renderer key");
            }
            Check(level.Undo()&&level.Placements.All(p=>p.WorldTransform==baseline[p.Id]),"Undo restores editor and triggers renderer propagation");
            var hidden=(HashSet<SmoLevelEntityId>)Get(window,"_hiddenEntities");hidden.Add(selected.Entity.Id);Call(window,"ApplyPlacementHighlights");
            appearances=(IDictionary)Get(renderer,"_appearances");
            foreach(var placement in selected.Entity.Parts)
                Check(!Property<bool>(appearances[new SmoRenderObjectKey(0,placement.Id.SceneObjectIndex,placement.Id.OccurrenceKey)]!,"Visible"),"Hide applies to each repeated slot");
            Console.WriteLine($"PASS LVL actual render bindings: {level.Placements.Count} placements, {checks} checks");return 0;
        }
        catch(Exception error){failure=error.ToString();throw;}
        finally
        {
            window.Close();app.Shutdown();
            File.WriteAllText(output,JsonSerializer.Serialize(new{checks,failure,seconds=timer.Elapsed.TotalSeconds,
                peakBytes=Process.GetCurrentProcess().PeakWorkingSet64},new JsonSerializerOptions{WriteIndented=true}));
        }
    }
}
