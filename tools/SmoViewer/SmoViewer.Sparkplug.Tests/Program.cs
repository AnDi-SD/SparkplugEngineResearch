using System.Numerics;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Diagnostics;
using System.Security.Cryptography;
using SmoViewer.Core;
using SmoViewer.Sparkplug;

internal static class Program
{
    private static int _checks;
    private static void Check(bool value, string message) { _checks++; if (!value) throw new InvalidDataException(message); }
    private static unsafe void AbiAndForest()
    {
        Check(Marshal.SizeOf<NativeMethods.Node>() == 48 && Marshal.SizeOf<NativeMethods.Bone>() == 68 && Marshal.SizeOf<NativeMethods.Sample>() == 44, "C ABI layouts");
        Check(Marshal.SizeOf<NativeMethods.ChannelInfo>() == 24 && Marshal.SizeOf<NativeMethods.LinearChannel>() == 24 &&
            Marshal.SizeOf<NativeMethods.FieldHeader>() == 12, "Channel/key/header C ABI layouts");
        var local = SparkplugNode.LocalMatrix(new(10,20,30), new(0,0,1,0), new(2,3,4));
        Check(local == new Matrix4x4(-2,0,0,0, 0,-3,0,0, 0,0,4,0, 10,20,30,1), "Native local matrix output layout");
        Check(SparkplugSkin.ComposeMatrix(Matrix4x4.CreateTranslation(-1,-2,-3),
            Matrix4x4.CreateTranslation(12,26,42)).Translation == new Vector3(11,24,39), "Static native skin matrix ABI");
        float[] linearTimes = [0, 0, 1]; Vector3[] linearValues = [new(0,1,2),new(3,4,5),new(6,7,8)];
        var linear = SparkplugAnimationClip.CreateLinear(linearTimes, linearValues, [], [], [], []);
        var channel = linear.Channel(0,0);
        linearTimes[0] = 99; linearValues[1] = Vector3.Zero;
        Check(channel.SampleVector(0, Vector3.Zero) == new Vector3(3,4,5), "Linear bridge owns keys and uses original equal-time result");
        Check(channel.SampleVector(.25f, Vector3.Zero) == new Vector3(3.75f,4.75f,5.75f), "Linear bridge native interpolation");
        linear.Dispose();
        try { channel.SampleVector(.25f, Vector3.Zero); Check(false,"Disposed channel cache remained usable"); }
        catch (ObjectDisposedException) { Check(true,"Retained channel cannot bypass disposed owner through sample cache"); }
        NativeMethods.Node[] nodes = [
            new() { Parent=-1, Position=new(10,20,30), Rotation=Quaternion.Identity, Scale=new(2,3,4) },
            new() { Parent=0, Position=new(1,2,3), Rotation=Quaternion.Identity, Scale=Vector3.One }
        ];
        SceneHandle handle;
        fixed (NativeMethods.Node* input = nodes) handle = new(NativeMethods.Check(NativeMethods.spv_scene_create(input, 2)));
        using (handle)
        {
            Matrix4x4[] worlds = new Matrix4x4[2];
            fixed (Matrix4x4* output = worlds)
            {
                NativeMethods.Check(NativeMethods.spv_scene_sample(handle, 0, output, 32));
                Check(worlds[1].Translation == new Vector3(12,26,42), "Native parent translation and inherited scale");
                Check(worlds[1].M11 == 2 && worlds[1].M22 == 3 && worlds[1].M33 == 4, "Native world scale");
                Check(NativeMethods.spv_scene_sample(handle, float.NaN, output, 32) == 0, "Nonfinite time rejected");
                Check(NativeMethods.spv_scene_sample(handle, 1, output, 16) == 0, "Short world output rejected");
                NativeMethods.Check(NativeMethods.spv_scene_sample(handle, -2, output, 32));
                Check(worlds[1].Translation == new Vector3(12,26,42), "Failed call leaves runtime reusable");
            }
            NativeMethods.Bone bone = new() { Node=1, InverseBind=Matrix4x4.CreateTranslation(-1,-2,-3) };
            Matrix4x4 palette;
            NativeMethods.Check(NativeMethods.spv_scene_palette(handle, &bone, 1, &palette, 16));
            Check(palette.Translation == new Vector3(10,20,30), "spSkin inverseBind times boneWorld matrix order");
            bone.Node=99;
            Check(NativeMethods.spv_scene_palette(handle, &bone, 1, &palette, 16) == 0, "Missing palette node rejected");
        }
        nodes[0].Parent=1;
        fixed (NativeMethods.Node* input = nodes) Check(NativeMethods.spv_scene_create(input, 2) == IntPtr.Zero, "Cycle rejected before owned graph creation");
        nodes[0].Parent=-1; nodes[1].Parent=5;
        fixed (NativeMethods.Node* input = nodes) Check(NativeMethods.spv_scene_create(input, 2) == IntPtr.Zero, "Missing parent rejected");
        try { using var invalid = SparkplugAnimationClip.Load("invalid"u8); Check(false,"Invalid SAN accepted"); }
        catch (InvalidDataException) { Check(true,"Invalid SAN reports a managed exception"); }
    }
    private static void OriginalPrs(string input)
    {
        using var source = JsonDocument.Parse(File.ReadAllBytes(input));
        Check(Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source.RootElement.GetProperty("original_report_path").GetString()!))) == source.RootElement.GetProperty("original_report_sha256").GetString(), "Original PC evidence hash");
        foreach (var row in source.RootElement.GetProperty("cases").EnumerateArray())
        {
            string path = row.GetProperty("path").GetString()!;
            Check(Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))) == row.GetProperty("sha256").GetString(), "Exact native fixture bytes");
            using var clip = SparkplugAnimationClip.Load(path);
            Check(clip.Tracks.Count == 2 && clip.Tracks[0].NodeName == "Pelvis", "Original PRS fixture binds its first Pelvis track");
            foreach (var sample in row.GetProperty("samples").EnumerateArray())
            {
                NativeMethods.Check(NativeMethods.spv_clip_sample(clip.Handle, 0, sample.GetProperty("seconds").GetSingle(), out var actual));
                float[][] values = [[actual.Position.X,actual.Position.Y,actual.Position.Z], [actual.Rotation.X,actual.Rotation.Y,actual.Rotation.Z,actual.Rotation.W], [actual.Scale.X,actual.Scale.Y,actual.Scale.Z]];
                int role=0;
                foreach (var expected in sample.GetProperty("prs").EnumerateArray())
                {
                    bool valid=sample.GetProperty("validity")[role].GetInt32()!=0;
                    Check(((actual.ValidRoles & (1u<<role))!=0)==valid,"Native validity flags");
                    if(valid) for(int c=0;c<values[role].Length;c++)
                        Check(MathF.Abs(values[role][c]-expected[c].GetSingle())<4e-5f,$"Original PC PRS: {row.GetProperty("name").GetString()} role {role} component {c}");
                    role++;
                }
            }
        }
        byte[] invalid = File.ReadAllBytes(source.RootElement.GetProperty("cases")[0].GetProperty("path").GetString()!);
        var document = SmoDocument.Parse(invalid);
        int offset = (int)document.Objects.Single().PhysicalOffset + 8;
        while (SmoDataBlockReader.TryReadHeader(invalid, offset, out var field))
        {
            if (field.FieldType == 8 && field.PayloadSize == 4)
            {
                Array.Clear(invalid, field.PayloadOffset, 4);
                try { using var rejected = SparkplugAnimationClip.Load(invalid); throw new Exception("Explicit zero pool accepted"); }
                catch (InvalidDataException) { Check(true, "Optional pool support still enforces explicitly declared capacities"); }
                break;
            }
            if (field.SizeKind == SmoDataBlockSizeCode.Empty) throw new InvalidDataException("Missing fixture pool field");
            offset = (int)field.PayloadEnd;
        }
    }
    private static void Models(string media)
    {
        (string Model,string San)[] cases = [
            ("Characters/Bloom/bloom_jeans.smo","Characters/Bloom/blwalk.san"),
            ("Characters/Icy/Icy.smo","Characters/Icy/xiwa.san"),
            ("Characters/Knut/knut.smo","Characters/Knut/Knwa.san")
        ];
        foreach(var (model,san) in cases)
        {
            var document=SmoDocument.Load(Path.Combine(media,model));
            var skins=document.Objects.Where(e=>e.TypeHash==SmoClassIds.Skin).Select(e=>SmoSkinDecoder.TryDecode(document,e,out var s,out var error)?s!:throw new InvalidDataException(error)).ToDictionary(s=>s.ObjectIndex);
            using var scene=new SparkplugSceneRuntime(document,skins);
            var clip=SparkplugAnimationClip.Load(Path.Combine(media,san));
            scene.Bind(clip); float duration=clip.Duration;
            clip.Dispose(); // native scene must retain its own animation ownership
            scene.Sample(duration*.5f); var middle=scene.Worlds.ToDictionary(p=>p.Key,p=>p.Value);
            scene.Sample(duration); scene.Sample(0); scene.Sample(duration*.5f);
            Check(middle.All(p=>p.Value==scene.Worlds[p.Key]),"Seeking is independent of frame history and disposed clip facade");
            foreach(var skin in skins.Values) Check(scene.Palette(skin.ObjectIndex).Length==skin.Bones.Count,"Native palette length");
            using var replacement=SparkplugAnimationClip.Load(Path.Combine(media,san));
            scene.Bind(replacement); scene.Sample(duration*.5f);
            Check(middle.All(p=>p.Value==scene.Worlds[p.Key]),"Rebind does not retain old key caches");
        }
        foreach(string model in new[]{"Characters/Flora/Flora.smo","Characters/Tecna/Tecna.smo","Menus/igmenu_opt_pc.smo"})
        {
            var document=SmoDocument.Load(Path.Combine(media,model));
            var skins=document.Objects.Where(e=>e.TypeHash==SmoClassIds.Skin).Select(e=>SmoSkinDecoder.TryDecode(document,e,out var s,out var error)?s!:throw new InvalidDataException(error)).ToDictionary(s=>s.ObjectIndex);
            using var scene=new SparkplugSceneRuntime(document,skins);
            scene.Sample(0);
            Check(scene.Worlds.Values.All(m=>float.IsFinite(m.M41)&&float.IsFinite(m.M42)&&float.IsFinite(m.M43)),"Static representative native node world positions");
        }
    }
    private static int Main(string[] args)
    {
        try
        {
            if (args is ["--buffer-inspection"]) return BufferInspectionRegression.Run();
            if (args is ["--scene-lighting", var model, var animation, var report]) return SceneLightingRegression.Run(model, animation, report);
            var timer=Stopwatch.StartNew(); AbiAndForest(); OriginalPrs(args[0]); Models(args[1]);
            Console.WriteLine($"PASS Sparkplug interop: {_checks} checks, {timer.Elapsed.TotalSeconds:F3}s, peak {Process.GetCurrentProcess().PeakWorkingSet64} bytes"); return 0;
        }
        catch(Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
