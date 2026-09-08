using System.Collections;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using SmoViewer;
using SmoViewer.Core;
using SmoViewer.Sparkplug;

internal static class Program
{
    static readonly BindingFlags Flags=BindingFlags.Instance|BindingFlags.NonPublic;
    static object? Field(object owner,string name)=>owner.GetType().GetField(name,Flags)!.GetValue(owner);
    static object? Call(object owner,string name,params object?[] args)=>owner.GetType().GetMethod(name,Flags)!.Invoke(owner,args);
    [STAThread] static int Main(string[] args)
    {
        var app=new Application { ShutdownMode=ShutdownMode.OnExplicitShutdown };
        int code=1;
        app.Dispatcher.BeginInvoke(async()=>{
            try {await Run(args.Single());code=0;}
            catch(Exception error){Console.Error.WriteLine(error);}
            finally{app.Shutdown();}
        });
        app.Run();return code;
    }
    static async Task Run(string output)
    {
        var window=new MainWindow { ShowInTaskbar=false };
        var report=new List<object>();
        try
        {
            foreach(var (model,clip) in new[]{("Characters/Icy/Icy.smo","Characters/Icy/xiwa.san"),("Characters/Knut/knut.smo","Characters/Knut/Knwa.san")})
            {
                var root=Path.GetFullPath("local-data/pc-pristine/Media");
                await (Task)Call(window,"LoadSmoFilesAsync",(object)new[]{Path.Combine(root,model)})!;
                if((int)Field(window,"_loadedFileCount")! !=1)throw new Exception("Model load failed");
                string san=Path.GetFullPath(Path.Combine(root,clip));
                Call(window,"AddAnimationFile",san,null,"Benchmark");Call(window,"RefreshAnimationList");
                var list=(ListBox)window.FindName("AnimationList");
                list.SelectedItem=list.Items.Cast<object>().Single(item=>(string)item.GetType().GetProperty("Path")!.GetValue(item)! == san);
                var animation=(SparkplugAnimationClip)Field(window,"_selectedAnimation")!;
                var timeField=typeof(MainWindow).GetField("_animationTime",Flags)!;
                var pose=typeof(MainWindow).GetMethod("ApplyAnimationPoseCore",Flags)!;
                var geometries=(IDictionary)Field(window,"_sceneGeometry")!;
                var entries=new List<DictionaryEntry>();
                foreach(DictionaryEntry entry in geometries) entries.Add(entry);
                foreach(string mode in new[]{"default-full","hidden-bones","world-only"})
                {
                    ((CheckBox)window.FindName("ShowSkeletonCheck")).IsChecked=mode=="default-full";
                    ((CheckBox)window.FindName("ShowAttachmentsCheck")).IsChecked=mode=="default-full";
                    if(mode=="world-only")geometries.Clear();
                    void Frame(int index){timeField.SetValue(window,(double)(index%120)/120*animation.Duration);pose.Invoke(window,null);}
                    for(int i=0;i<60;i++)Frame(i);
                    var samples=new double[240];
                    long allocations=GC.GetAllocatedBytesForCurrentThread();int gen0=GC.CollectionCount(0);
                    for(int i=0;i<samples.Length;i++) {long begin=Stopwatch.GetTimestamp();Frame(i);samples[i]=Stopwatch.GetElapsedTime(begin).TotalMilliseconds;}
                    long allocated=GC.GetAllocatedBytesForCurrentThread()-allocations;
                    Array.Sort(samples);
                    var result=new {model,mode,frames=samples.Length,medianMilliseconds=samples[samples.Length/2],
                        p95Milliseconds=samples[(int)(samples.Length*.95)],meanMilliseconds=samples.Average(),
                        allocatedBytesPerFrame=(double)allocated/samples.Length,gen0Collections=GC.CollectionCount(0)-gen0};
                    report.Add(result);Console.WriteLine(JsonSerializer.Serialize(result));
                    if(mode=="world-only")foreach(var entry in entries)geometries.Add(entry.Key,entry.Value);
                }
            }
        }
        finally{window.Close();}
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        File.WriteAllText(output,JsonSerializer.Serialize(new {kind="Viewer-animation-cost-probe",configuration="Release",cases=report,
            peakWorkingSet=Process.GetCurrentProcess().PeakWorkingSet64,
            scope="Actual CPU pose path with initialized OpenGL host; no displayed/composited frame, upload or GPU timing. World-only temporarily excludes all scene geometry."},new JsonSerializerOptions {WriteIndented=true}));
    }
}
