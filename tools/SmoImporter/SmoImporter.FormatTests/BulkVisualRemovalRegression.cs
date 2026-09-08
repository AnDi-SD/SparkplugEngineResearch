using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class BulkVisualRemovalRegression
{
    internal static int Run(string[] args)
    {
        if(args.Length is <2 or >9)throw new ArgumentException("NEW_REPORT_JSON SMO [up to eight files]");
        string report=Path.GetFullPath(args[0]);if(File.Exists(report))throw new InvalidOperationException("Use a new report path");
        var watch=Stopwatch.StartNew();var rows=new List<object>();int checks=0;
        foreach(string path in args.Skip(1))
        {
            byte[] source=File.ReadAllBytes(path);var document=SmoDocument.ParseOwned(source);
            var visuals=document.Objects.Where(e=>e.TypeHash is SmoClassIds.MeshData or SmoClassIds.MaterialData or SmoClassIds.TextureData).ToArray();
            var selections=new List<uint[]> {Array.Empty<uint>(),visuals.Take(1).Select(e=>e.Id).ToArray(),visuals.Select(e=>e.Id).ToArray(),
                visuals.Where(e=>e.TypeHash is SmoClassIds.MaterialData or SmoClassIds.TextureData).Select(e=>e.Id).ToArray(),
                document.Objects.Where(e=>e.TypeHash==SmoClassIds.Skin).Take(2).Select(e=>e.Id).ToArray(),
                visuals.Where((_,i)=>i%2==0).Select(e=>e.Id).ToArray()};
            foreach(uint[] selected in selections)
            {
                var sequential=document;
                foreach(var original in document.Objects.Where(e=>selected.Contains(e.Id)).OrderByDescending(e=>e.LogicalOffset))
                {
                    var current=sequential.Objects.SingleOrDefault(e=>e.Id==original.Id);
                    if(current?.ParentIndex is not int owner)continue;
                    sequential=SmoDocument.ParseOwned(SmoVisualForestInjector.RemoveInlineBranch(sequential,sequential.Objects[owner].Id,current.Id));
                }
                byte[] batch=SmoVisualForestInjector.RemoveInlineBranches(document,selected);
                Check(batch.AsSpan().SequenceEqual(sequential.Data.Span),"batch must equal sequential removal byte for byte");
                Check(!SmoDocument.ParseOwned(batch).HasErrors,"batch output strict container");
                byte[] repeated=SmoVisualForestInjector.RemoveInlineBranches(document,selected.Reverse().Concat(selected));
                Check(batch.AsSpan().SequenceEqual(repeated),"duplicates and selection order do not change the result");
            }
            uint absent=document.Objects.Max(e=>e.Id)+1;
            Check(Rejects(()=>SmoVisualForestInjector.RemoveInlineBranches(document,[absent])),"missing identity rejected");
            uint root=document.Objects.First(e=>e.ParentIndex is null).Id;
            Check(Rejects(()=>SmoVisualForestInjector.RemoveInlineBranches(document,[root])),"root without owner rejected");
            Check(File.ReadAllBytes(path).AsSpan().SequenceEqual(source),"source array and file unchanged");
            rows.Add(new {path=Path.GetFullPath(path),sha256=Convert.ToHexString(SHA256.HashData(source)),cases=selections.Count});
        }
        Directory.CreateDirectory(Path.GetDirectoryName(report)!);
        File.WriteAllText(report,JsonSerializer.Serialize(new {kind="bulk-visual-removal-regression",status="passed",checks,rows,
            elapsedSeconds=watch.Elapsed.TotalSeconds,peakWorkingSet=Process.GetCurrentProcess().PeakWorkingSet64},new JsonSerializerOptions {WriteIndented=true}));
        Console.WriteLine($"PASS {checks} bulk-removal checks");return 0;
        void Check(bool value,string reason){checks++;if(!value)throw new InvalidDataException(reason);}
    }
    private static bool Rejects(Action action){try{action();return false;}catch(InvalidOperationException){return true;}}
}
