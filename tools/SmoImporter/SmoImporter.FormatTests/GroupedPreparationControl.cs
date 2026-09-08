using System.Diagnostics;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class GroupedPreparationControl
{
    internal static int Run(string targetPath, string donorPath, string report)
    {
        if (File.Exists(report)) throw new InvalidOperationException("Use a new report path");
        var target = SmoDocument.Load(targetPath);
        var donor = ImportedModelReader.ReadGeometryOnly(donorPath);
        var rig = TargetRigDefinition.FromSmoDocument(target);
        var alignment = new ReplacementTransform(83f, Vector3.Zero, new Vector3(0, 13.5f, -4));
        var pose = TargetRigBodyPoseMapper.CreatePose(rig, new TargetRigBodyPoseParameters(
            ArmElevationDegrees: -2.7f, ArmForwardDegrees: 7.4f, ElbowBendDegrees: 0,
            LegSpreadDegrees: -7.3f, KneeBendDegrees: 0, TorsoPitchDegrees: 1f, NeckForward: 0)).Capture();
        var body = TargetRigAutomaticPoseFitter.SelectBody(rig, donor, alignment);
        var watch = Stopwatch.StartNew();
        var preparation = GeneratedSkinningPreparer.Prepare(target, donor, pose, alignment, body);
        var options = new JsonSerializerOptions { IncludeFields = true, WriteIndented = true };
        byte[] serialized = JsonSerializer.SerializeToUtf8Bytes(preparation, options);
        string fingerprint = Convert.ToHexString(SHA256.HashData(serialized));
        File.WriteAllText(report, JsonSerializer.Serialize(new
        {
            kind = "grouped-preparation-control", status = "captured", publicPreparationSha256 = fingerprint,
            targetSha256 = Hash(targetPath), donorSha256 = Hash(donorPath),
            selectedComponents = body.Components.Count, body.TotalComponentCount,
            preparation.Analysis.InternalPreparationPassCount, preparation.Analysis.SemanticResolutionPassCount,
            analysis = preparation.Analysis, elapsedSeconds = watch.Elapsed.TotalSeconds,
            coreAssemblySha256 = Hash(typeof(GeneratedSkinningPreparer).Assembly.Location)
        }, options));
        Console.WriteLine($"CAPTURE grouped pose, selected={body.Components.Count}, passes={preparation.Analysis.InternalPreparationPassCount}, SHA={fingerprint}");
        return 0;
    }

    private static string Hash(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
}
