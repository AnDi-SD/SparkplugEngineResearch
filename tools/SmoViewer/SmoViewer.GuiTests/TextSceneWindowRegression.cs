using System.Collections;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text.Json;
using SmoLVLcreator.Core;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Scene;

internal static class TextSceneWindowRegression
{
    private const BindingFlags Hidden=BindingFlags.Instance|BindingFlags.NonPublic;
    private static object Get(object owner,string name)=>owner.GetType().GetField(name,Hidden)!.GetValue(owner)!;
    private static object? Call(object owner,string name,params object?[] args)=>owner.GetType().GetMethod(name,Hidden)!.Invoke(owner,args);
    private static T Property<T>(object owner,string name)=>(T)owner.GetType().GetProperty(name)!.GetValue(owner)!;

    internal static async Task<int> Run(string source,string output,bool sky=false)
    {
        if(File.Exists(output))throw new IOException("Fresh Text window report required.");
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        var timer=Stopwatch.StartNew();int checks=0;string? failure=null;object? detail=null;
        void Check(bool value,string message){++checks;if(!value)throw new InvalidDataException(message);}
        var viewer=new SmoViewer.MainWindow{ShowInTaskbar=false};
        var levelWindow=new SmoLVLcreator.Gui.MainWindow{ShowInTaskbar=false};
        object Inspect(SmoGpuSceneRenderer renderer,SmoPreparedScene scene,string host)
        {
            var items=((IEnumerable)Get(renderer,"_items")).Cast<object>().ToArray();
            var keys=items.Select(item=>Property<SmoRenderObjectKey>(item,"Key")).ToHashSet();
            Check(items.Length==scene.Meshes.Count+scene.Texts.Count&&keys.Count==items.Length,host+" keeps all Mesh/Text occurrence identities");
            foreach(var text in scene.Texts)
            {
                var key=new SmoRenderObjectKey(0,text.Text.ObjectIndex,text.OccurrenceKey);
                var item=items.Single(value=>Property<SmoRenderObjectKey>(value,"Key")==key);
                Check(Property<int?>(item,"TextObjectIndex")==text.Text.ObjectIndex&&Property<bool>(item,"OriginalAlphaSphere"),host+" connects actual Text source and alpha sphere");
                Check(Property<SmoMaterialDraw>(item,"MaterialDraw").MaterialObjectIndex==text.Text.Renderable.Material!.Index,host+" connects actual custom material");
            }
            foreach(var mesh in scene.Meshes.Where(m=>m.ContainerKind==SmoRenderContainerKind.SkyBox))
            {
                var key=new SmoRenderObjectKey(0,mesh.SceneObjectIndex,mesh.OccurrenceKey);
                var item=items.Single(value=>Property<SmoRenderObjectKey>(value,"Key")==key);
                Check(Property<bool>(item,"IsSky")&&Property<SmoSkyBoxPose>(item,"SkyPose") is not null,host+" connects the separate common SkyBox camera pose");
            }
            return new{host,placements=items.Length,texts=scene.Texts.Count,meshes=scene.Meshes.Count};
        }
        try
        {
            Check((bool)Get(viewer,"_gpuRendererAvailable"),"Actual Viewer OpenGL host initialized");
            await (Task)Call(viewer,"LoadSmoFilesAsync",(object)new[]{Path.GetFullPath(source)})!;
            Check((int)Get(viewer,"_loadedFileCount")==1&&(int)Get(viewer,"_failedFileCount")==0,"Actual Viewer async loader succeeds");
            var files=(IDictionary)Get(viewer,"_treeFiles");var decoded=files[0]!;
            if(!sky)Check(Property<IReadOnlyList<SmoSceneText>>(decoded,"Texts").Count==10,"Viewer retains ten prepared Text occurrences");
            var workspace=SmoLevelWorkspace.Load(source);var scene=SmoSceneBuilder.Build(workspace.Document);
            Check(scene.DecodeErrors.Count==0&&(sky?scene.Meshes.Any(m=>m.ContainerKind==SmoRenderContainerKind.SkyBox):scene.Meshes.Count==41&&scene.Texts.Count==10),"Selected actual scene has no requested decode gap");
            var viewerReport=Inspect((SmoGpuSceneRenderer)Get(viewer,"_gpuRenderer"),scene,"Viewer");
            Call(levelWindow,"AttachWorkspace",workspace,null,false);
            var level=(SmoLevelDocument)Get(levelWindow,"_document");
            Check(level.Placements.Count==scene.Meshes.Count,"Every actual Mesh placement remains in the editor catalog");
            var levelReport=Inspect((SmoGpuSceneRenderer)Get(levelWindow,"_gpuRenderer"),scene,"LVLcreator");
            detail=new{viewer=viewerReport,level=levelReport,editablePlacements=level.Placements.Count};
        }
        catch(Exception error){failure=error.ToString();}
        finally{viewer.Close();levelWindow.Close();}
        File.WriteAllText(output,JsonSerializer.Serialize(new{source,checks,failure,detail,seconds=timer.Elapsed.TotalSeconds,
            peakBytes=Process.GetCurrentProcess().PeakWorkingSet64},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine(JsonSerializer.Serialize(new{checks,failure,seconds=timer.Elapsed.TotalSeconds}));return failure is null?0:1;
    }
}
