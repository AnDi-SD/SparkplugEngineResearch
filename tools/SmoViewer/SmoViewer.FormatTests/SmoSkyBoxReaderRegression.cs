using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;
using SmoViewer.Scene;

namespace SmoViewer.FormatTests;

internal static class SmoSkyBoxReaderRegression
{
    internal static int Run(string source,string output)
    {
        var document=SmoDocument.Load(source);var loaded=SmoLoadedResources.Get(document);
        int checks=0;
        void Check(bool value,string message){if(!value)throw new InvalidDataException(message);++checks;}
        Check(loaded.LoadIssue is null,loaded.LoadIssue ?? "Full graph loaded");
        Check(loaded.SceneIssue is null,loaded.SceneIssue ?? "Actual derived Node world updated");
        var skyEntries=document.Objects.Where(entry=>entry.TypeHash==SmoClassIds.SkyBox).ToArray();
        Check(skyEntries.Length>0,"Selected real scene contains SkyBox");
        var scene=SmoSceneBuilder.Build(document);
        int members=0;
        foreach(var entry in skyEntries)
        {
            Check(SmoSkyBoxDecoder.TryDecode(document,entry,out var metadata,out var error),error);
            var container=loaded.RenderContainersByObjectIndex[entry.Index];
            Check(container.Kind==SmoRenderContainerKind.SkyBox,"SkyBox is separate pass kind");
            Check(container.RenderableObjectIndices.SequenceEqual(metadata!.Models.Select(value=>value.TargetObjectIndex!.Value)),"Actual ordered members match shared metadata references");
            var occurrences=loaded.RenderOccurrences.Where(value=>value.Key.ContainerObjectIndex==entry.Index).ToArray();
            Check(occurrences.Length==metadata.Models.Count&&occurrences.All(value=>value.Issue is null),"All actual sky support occurrences retained");
            foreach(var occurrence in occurrences)
            {
                var mesh=scene.Meshes.Single(value=>value.OccurrenceKey==occurrence.Key);
                Check(mesh.ContainerKind==SmoRenderContainerKind.SkyBox,"Scene preserves special pass identity");
                Check(mesh.WorldTransform==occurrence.InputWorld&&mesh.RigidNodeObjectIndex==entry.Index,"Sky placement comes from actual derived Node");
            }
            members+=occurrences.Length;
        }
        var report=new{status="passed",source=Path.GetFullPath(source),source_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))),
            native_dll_sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")))),
            objects=document.Objects.Count,skyboxes=skyEntries.Length,sky_members=members,scene_meshes=scene.Meshes.Count,checks,
            scope="Actual resource graph, world and ordered support view; scene manager/camera-follow and full renderer scheduling remain separate."};
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        File.WriteAllText(output,JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true})+"\n");
        Console.WriteLine($"SkyBox: {skyEntries.Length} nodes, {members} members, {scene.Meshes.Count} scene meshes, {checks} checks");return 0;
    }
}
