using System.Buffers.Binary;
using System.Diagnostics;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;

internal static class SmoAnimationRegression
{
    internal static int Run(string inputPath, string outputPath)
    {
        var timer = Stopwatch.StartNew();
        using JsonDocument input = JsonDocument.Parse(File.ReadAllBytes(inputPath));
        JsonElement root = input.RootElement;
        int checks = 0, poses = 0;
        double maximumError = 0;
        void Check(bool value, string message)
        {
            checks++;
            if (!value) throw new InvalidDataException(message);
        }
        var cases = new List<object>();
        foreach (JsonElement row in root.GetProperty("cases").EnumerateArray())
        {
            string path = row.GetProperty("path").GetString()!;
            byte[] bytes = File.ReadAllBytes(path);
            Check(Convert.ToHexString(SHA256.HashData(bytes)) == row.GetProperty("sha256").GetString(), "Fixture hash changed.");
            Check(SmoAnimationDecoder.TryDecode(bytes, path, out var clip, out string error), path+": "+error);
            SmoAnimationTrack track = clip!.Tracks.Single(track => track.NodeName == "Pelvis");
            SmoAnimationPose before = track.Sample(.5f, Vector3.Zero, Quaternion.Identity, Vector3.One);
            bytes.AsSpan().Clear(); // Decoder owns its snapshot, not the caller's buffer.
            Check(track.Sample(.5f, Vector3.Zero, Quaternion.Identity, Vector3.One) == before, "Caller mutation changed decoded keys.");
            double caseError = 0;
            foreach (JsonElement sample in row.GetProperty("samples").EnumerateArray())
            {
                float time = sample.GetProperty("seconds").GetSingle();
                SmoAnimationPose actual = track.Sample(time, Vector3.Zero, Quaternion.Identity, Vector3.One);
                float[][] values = [[actual.Position.X, actual.Position.Y, actual.Position.Z],
                    [actual.Rotation.X, actual.Rotation.Y, actual.Rotation.Z, actual.Rotation.W],
                    [actual.Scale.X, actual.Scale.Y, actual.Scale.Z]];
                bool[] validity = [track.PositionChannel?.HasKeys == true, track.RotationChannel?.HasKeys == true,
                                    track.ScaleChannel?.HasKeys == true];
                for (int role = 0; role < 3; role++)
                {
                    Check(validity[role] == (sample.GetProperty("validity")[role].GetInt32() != 0), "Channel validity mismatch.");
                    double[] expected = sample.GetProperty("prs")[role].EnumerateArray().Select(v => v.GetDouble()).ToArray();
                    for (int c = 0; c < expected.Length; c++)
                    {
                        double difference = Math.Abs(values[role][c]-expected[c]);
                        caseError = Math.Max(caseError, difference);
                        Check(difference <= 4e-5 + 4e-5*Math.Abs(expected[c]),
                            $"{row.GetProperty("name").GetString()}, role {role}, t={time:G9}: {values[role][c]:G9} != {expected[c]:G9}");
                    }
                }
                poses++;
            }
            maximumError = Math.Max(maximumError, caseError);
            cases.Add(new { name = row.GetProperty("name").GetString(), maximumError = caseError });
        }
        foreach (JsonElement row in root.GetProperty("real_sans").EnumerateArray())
        {
            string path = row.GetProperty("path").GetString()!;
            Check(Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))) == row.GetProperty("sha256").GetString(), "Real SAN hash changed.");
            Check(SmoAnimationDecoder.TryDecode(path, out var clip, out string error), path+": "+error);
            Check(clip!.Tracks.Count == row.GetProperty("tracks").GetInt32(), "Actual named track count.");
            if (Path.GetFileName(path) == "bbush.san")
            {
                SmoAnimationTrack plane = clip.Tracks.Single(track => track.NodeName == "Plane01");
                Check(plane.ScaleChannel?.IsCubic == true && plane.Scales.Count == 3, "Real packed cubic scale was dropped.");
                Check(plane.Sample(.25f, Vector3.Zero, Quaternion.Identity, Vector3.One).Scale != Vector3.One,
                    "Viewer supports the actual non-unit scale which VMD must reject.");
            }
        }

        byte[] Build(uint representation, float[] times, float[] values)
        {
            using var wire = new MemoryStream(); using var writer = new BinaryWriter(wire);
            writer.Write(representation); writer.Write((uint)times.Length);
            foreach (float value in times) writer.Write(value);
            foreach (float value in values) writer.Write(value);
            return Container(SmoDataBlockWriter.BuildField(2, wire.ToArray()));
        }
        void Reject(byte[] data, string message)
        {
            Check(!SmoAnimationDecoder.TryDecode(data, "malformed.san", out var clip, out string error) &&
                  clip is null && !string.IsNullOrWhiteSpace(error), message);
        }
        Reject(Build(99, [0], [1, 2, 3]), "Unknown representation was silently emptied.");
        Reject(Build(2, [], []), "Unsafe empty cubic accepted.");
        Reject(Build(3, [0], [1]), "Missing scalar axes accepted.");
        // Original PC 43DB90/479290 accepts equal times. Recorded by
        // research/probe_viewer_san_equal_times.py, Viewer audit V-02.
        foreach (var repeated in new (float[] Times, float[] ExpectedX)[] {
            ([0, 0], [0, 0, 0, 0, 0]),
            ([0, 0, 1], [0, 3, 3.75f, 6, 6]),
            ([0, 1, 1], [0, 0, .75f, 6, 6]) })
        {
            float[] values = Enumerable.Range(0, repeated.Times.Length*3).Select(i => (float)i).ToArray();
            Check(SmoAnimationDecoder.TryDecode(Build(1, repeated.Times, values), "equal-times.san",
                out var equalTimes, out string equalError), equalError);
            float[] samples = [-1, 0, .25f, 1, 2];
            for (int i = 0; i < samples.Length; i++)
            {
                var actual = equalTimes!.Tracks.Single().Sample(samples[i], Vector3.Zero, Quaternion.Identity, Vector3.One);
                float x = repeated.ExpectedX[i];
                Check(actual.Position == new Vector3(x, x+1, x+2), "Original PC repeated-time pose.");
            }
        }
        Reject(Build(1, [0], [float.NaN, 0, 0]), "Non-finite used value accepted.");
        Reject(Build(1, [float.PositiveInfinity], [0, 0, 0]), "Non-finite time accepted.");
        byte[] emptyLinear = [1, 0, 0, 0, 0, 0, 0, 0];
        byte[] unclosed = Container(SmoDataBlockWriter.BuildField(2, emptyLinear), includeName: false);
        Reject(unclosed, "Unnamed trailing channel accepted.");
        byte[] platform = Build(1, [0], [1, 2, 3]);
        BinaryPrimitives.WriteUInt32LittleEndian(platform.AsSpan(16), 4);
        Reject(platform, "PS2 file incorrectly claimed as PC.");
        byte[] zero = emptyLinear;
        // Original whole field reader 43ECC0 accepts repeated PRS fields;
        // the last field supplies this role. See probe_viewer_san_repeated_role.py.
        Check(SmoAnimationDecoder.TryDecode(Container([..SmoDataBlockWriter.BuildField(2, zero),
            ..SmoDataBlockWriter.BuildField(2, zero)]), "repeated-empty.san", out var repeatedEmpty, out _)
            && repeatedEmpty!.Tracks.Single().PositionChannel?.HasKeys == false, "Native empty repeated role.");
        byte[] firstRole = [..BitConverter.GetBytes(1u), ..BitConverter.GetBytes(1u), ..BitConverter.GetBytes(0f),
            ..BitConverter.GetBytes(1f), ..BitConverter.GetBytes(2f), ..BitConverter.GetBytes(3f)];
        byte[] lastRole = [..BitConverter.GetBytes(1u), ..BitConverter.GetBytes(1u), ..BitConverter.GetBytes(0f),
            ..BitConverter.GetBytes(4f), ..BitConverter.GetBytes(5f), ..BitConverter.GetBytes(6f)];
        Check(SmoAnimationDecoder.TryDecode(Container([..SmoDataBlockWriter.BuildField(2, firstRole),
            ..SmoDataBlockWriter.BuildField(2, lastRole)]), "repeated-role.san", out var repeatedRole, out _)
            && repeatedRole!.Tracks.Single().Sample(0, Vector3.Zero, Quaternion.Identity, Vector3.One).Position == new Vector3(4,5,6),
            "Original PC last repeated role wins.");
        byte[] garbageCoefficients = Build(2, [0], [1, 2, 3, 0, 0, 0, 0, 0, 0,
            float.NaN, float.NaN, float.NaN, float.NaN, float.NaN, float.NaN]);
        // Original 43ECC0/479290 accepts this input but produces NaN positions
        // (audit V-05, probe_viewer_san_repeated_role.py --unused-coefficients).
        // Keep the existing portable finite-input boundary; do not claim the PC rejects it.
        Reject(garbageCoefficients, "Portable Sparkplug finite-input boundary.");
        Check(SmoAnimationDecoder.TryDecode(Build(1, [-1, 0], [0, 0, 0, 10, 0, 0]),
            "negative.san", out var negative, out _), "Negative source times are supported.");
        int keyBudget = SmoAnimationBaker.MaximumOutputKeys;
        var baked = SmoAnimationBaker.Bake(negative!.Tracks.Single(), 1, ref keyBudget, timeOffset: 1);
        Check(MathF.BitDecrement(0f)+1 == 1f, "Reproduces the old shift-after-baking collision.");
        Check(baked.Positions.Zip(baked.Positions.Skip(1)).All(pair => pair.First.Time < pair.Second.Time),
            "Shifted target times must be strictly increasing.");
        Check(Math.Abs(baked.Positions.Single(key => key.Time == MathF.BitDecrement(1f)).Value.X-10) < 1e-5 &&
            baked.Positions.Single(key => key.Time == 1).Value.X == 0,
            "Shifted two-key endpoint retains the pre-jump and native endpoint poses.");
        float guardTime = 1-SmoAnimationBaker.BoundaryGuardSeconds;
        Check(Math.Abs(baked.Positions.Single(key => key.Time == guardTime).Value.X-guardTime*10) < 1e-5,
            "Receiver boundary guard is an actual source sample, not a repeated/fabricated value.");
        int smallBudget = 1;
        bool budgetRejected = false;
        try { SmoAnimationBaker.Bake(negative.Tracks.Single(), 1, ref smallBudget, 1); }
        catch (SmoFormatException) { budgetRejected = true; }
        Check(budgetRejected, "Sampling must enforce the shared key budget.");
        Check(SmoAnimationDecoder.TryDecode(Build(1, [-1, 0, float.Epsilon], [0, 0, 0, 1, 0, 0, 2, 0, 0]),
            "collapsed.san", out var collapsed, out _), "Distinct source times are accepted.");
        bool collapseRejected = false;
        try { SmoAnimationBaker.Bake(collapsed!.Tracks.Single(), 1, ref keyBudget, 1); }
        catch (SmoFormatException) { collapseRejected = true; }
        Check(collapseRejected, "Unrepresentable shifted source times must be rejected explicitly.");
        var bindingWarnings = new List<string>();
        SmoAnimationTrack positionTrack = negative.Tracks.Single();
        var rotationTrack = new SmoAnimationTrack("Pelvis", [], [new(0, Quaternion.Identity)], []);
        var lowerCaseTrack = positionTrack with { NodeName = "pelvis" };
        var bindings = SmoAnimationBinding.BindByName([positionTrack, rotationTrack, lowerCaseTrack, positionTrack], bindingWarnings);
        Check(bindings.Count == 2 && bindings["Pelvis"].PositionChannel == positionTrack.PositionChannel &&
            bindings["Pelvis"].Rotations.Count == 1 && bindings["pelvis"].Rotations.Count == 0,
            "Case-sensitive binding merges disjoint properties without changing another node.");
        Check(bindingWarnings.Count == 1 && bindingWarnings[0].Contains("position"),
            "Overlapping duplicate property retains the first with a warning.");
        timer.Stop();
        var report = new { status = "passed", checks, poses, maximumError, cases,
            inputSha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(inputPath))),
            elapsedSeconds = timer.Elapsed.TotalSeconds, peakWorkingSet = Process.GetCurrentProcess().PeakWorkingSet64 };
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outputPath))!);
        File.WriteAllText(outputPath, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"PASS shared SAN: {checks} checks, {poses} original PRS poses, max error {maximumError:G5}; {timer.Elapsed.TotalSeconds:F3}s");
        return 0;
    }

    private static byte[] Container(byte[] prs, bool includeName = true)
    {
        byte[] name = "Pelvis\0"u8.ToArray();
        byte[] body = [..BitConverter.GetBytes(SmoClassIds.Animation), .."SBOO"u8,
            ..SmoDataBlockWriter.BuildField(0, BitConverter.GetBytes(1f)), ..prs,
            ..(includeName ? SmoDataBlockWriter.BuildField(1, [..BitConverter.GetBytes((ushort)name.Length), ..name]) : []), 0];
        using var stream = new MemoryStream(); using var writer = new BinaryWriter(stream, Encoding.UTF8, true);
        int start = 32+6+name.Length+12+4;
        writer.Write("FFPS"u8); writer.Write(0x26u); writer.Write(0u); writer.Write((uint)(start+body.Length));
        writer.Write(2u); writer.Write((uint)start); writer.Write((uint)body.Length); writer.Write(1u);
        writer.Write(1u); writer.Write((ushort)name.Length); writer.Write(name); writer.Write(SmoClassIds.Animation);
        writer.Write(0u); writer.Write((uint)body.Length); writer.Write(0u); writer.Write(body);
        return stream.ToArray();
    }
}
