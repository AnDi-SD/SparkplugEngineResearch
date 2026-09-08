using System.Diagnostics;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoExporter.Core;
using SmoViewer.Core;

internal static class FbxAlphaRegression
{
    public static int Run(string outputDirectory, string[] sources)
    {
        if (sources.Length > 5) throw new ArgumentException("Select at most five relevant SMOs.");
        string directory = Path.GetFullPath(outputDirectory);
        Directory.CreateDirectory(directory);
        var timer = Stopwatch.StartNew();
        int checks = 0;
        var exports = new List<object>();
        void Check(bool success, string message)
        {
            if (!success) throw new InvalidOperationException(message);
            checks++;
        }
        string Hash(ReadOnlySpan<byte> data) => Convert.ToHexString(SHA256.HashData(data));
        void Export(SmoExportScene scene, string name, string provenance)
        {
            string path = Path.Combine(directory, name + ".fbx");
            string[] before = scene.Meshes.Where(mesh => mesh.Texture is not null)
                .Select(mesh => Hash(mesh.Texture!.PngBytes)).ToArray();
            FbxExporter.Export(scene, path);
            Check(File.ReadAllBytes(path).AsSpan(0, 20).SequenceEqual("Kaydara FBX Binary  "u8),
                name + ": binary FBX written");
            Check(before.SequenceEqual(scene.Meshes.Where(mesh => mesh.Texture is not null)
                .Select(mesh => Hash(mesh.Texture!.PngBytes))), name + ": shared input PNGs unchanged");
            var meshes = new List<object>();
            foreach (var mesh in scene.Meshes)
            {
                bool includeTextures = (scene.Resources &
                    (SmoExportResourceTypes.Materials | SmoExportResourceTypes.Textures)) ==
                    (SmoExportResourceTypes.Materials | SmoExportResourceTypes.Textures);
                var texture = includeTextures ? mesh.Texture : null;
                string? pixels = null;
                if (texture is not null)
                {
                    Check(texture.Bgra32Pixels.Length == texture.Width * texture.Height * 4,
                        name + ": scene retains decoded texture pixels without a PNG round trip");
                    pixels = name + "-" + mesh.ObjectIndex + ".bgra";
                    File.WriteAllBytes(Path.Combine(directory, pixels), texture.Bgra32Pixels.ToArray());
                }
                object? skin = null;
                if (mesh.SkinObjectIndex is int skinIndex)
                {
                    var sourceSkin = scene.Skins.Single(item => item.ObjectIndex == skinIndex);
                    var seenJoints = new HashSet<int>();
                    string[] joints = sourceSkin.JointObjectIndices.Select((joint, slot) =>
                    {
                        string jointName = scene.Nodes.Single(node => node.ObjectIndex == joint).Name;
                        return seenJoints.Add(joint) ? jointName : jointName + "_palette_" + slot;
                    }).ToArray();
                    string weightsFile = name + "-" + mesh.ObjectIndex + ".weights";
                    using (var writer = new BinaryWriter(File.Create(Path.Combine(directory, weightsFile))))
                    {
                        for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
                        {
                            Vector4 weights = mesh.BlendWeights[vertex];
                            Vector4 indices = mesh.JointIndices[vertex];
                            foreach (float value in new[] { weights.X, weights.Y, weights.Z, weights.W }) writer.Write(value);
                            foreach (float value in new[] { indices.X, indices.Y, indices.Z, indices.W }) writer.Write(value);
                        }
                    }
                    skin = new { joints, file = weightsFile,
                        sha256 = Hash(File.ReadAllBytes(Path.Combine(directory, weightsFile))) };
                }
                meshes.Add(new
                {
                    name = mesh.Name, objectIndex = mesh.ObjectIndex,
                    materialAlpha = mesh.MaterialColor.W, usesTextureAlpha = mesh.UsesAlphaBlend,
                    materialRgb = new[] { mesh.MaterialColor.X, mesh.MaterialColor.Y, mesh.MaterialColor.Z },
                    vertexCount = mesh.Positions.Length, skin,
                    texture = texture is null ? null : new
                    {
                        objectIndex = texture.ObjectIndex, width = texture.Width, height = texture.Height,
                        pixels, sha256 = Hash(texture.Bgra32Pixels.Span),
                        hasAlpha = texture.OpacityMaskPngBytes is not null
                    }
                });
            }
            exports.Add(new { file = name + ".fbx", sha256 = Hash(File.ReadAllBytes(path)), provenance, meshes });
        }

        byte[] pixels = new byte[256 * 4];
        for (int index = 0; index < 256; index++)
        {
            pixels[index * 4] = (byte)index;
            pixels[index * 4 + 1] = (byte)(255 - index);
            pixels[index * 4 + 2] = (byte)((index * 17) % 256);
            pixels[index * 4 + 3] = (byte)index;
        }
        var texture = new SmoExportTexture(42, "alpha-ramp", 256, 1,
            PngEncoder.EncodeBgra32(256, 1, pixels),
            PngEncoder.EncodeOpacityMaskBgra32(256, 1, pixels),
            PngEncoder.EncodeBgr24(256, 1, pixels), pixels);
        var cases = new (string Name, float Factor, bool Blend, bool Texture)[]
        {
            ("alpha_zero", 0, true, true), ("alpha_small", 1f / 255, true, true),
            ("alpha_fraction", 0.1234567f, true, true), ("alpha_half", 0.5f, true, true),
            ("alpha_near_one", MathF.BitDecrement(1f), true, true), ("alpha_one", 1, true, true),
            ("alpha_ignored", 1, false, true), ("alpha_ignored_half", 0.5f, false, true),
            ("scalar_zero", 0, true, false), ("scalar_half", 0.5f, true, false),
            ("scalar_one", 1, false, false)
        };
        var syntheticMeshes = cases.Select((item, index) => new SmoExportMesh(
            100 + index, (uint)(100 + index), item.Name, 0, 4, 0, 0, 0,
            [new(0, 0, 0), new(256, 0, 0), new(256, 1, 0), new(0, 1, 0)],
            [Vector3.UnitZ, Vector3.UnitZ, Vector3.UnitZ, Vector3.UnitZ],
            [new(0, 0), new(1, 0), new(1, 1), new(0, 1)], [], [], [], [],
            [0, 1, 2, 0, 2, 3], item.Texture ? texture : null, null,
            new Vector4(1, 1, 1, item.Factor), item.Blend, null, null,
            Matrix4x4.Identity, Matrix4x4.Identity)).ToArray();
        var synthetic = new SmoExportScene("synthetic://fbx-alpha-ramp", "", 0,
            SmoExportResourceTypes.Meshes | SmoExportResourceTypes.Materials | SmoExportResourceTypes.Textures,
            SmoExportSceneMode.All, syntheticMeshes,
            syntheticMeshes.Select((mesh, index) => new SmoExportMeshPlacement(
                mesh.ObjectIndex, mesh.Name, mesh.ObjectIndex, false, null, null, null,
                Matrix4x4.CreateTranslation(0, index * 2, 0),
                Matrix4x4.CreateTranslation(0, index * 2, 0))).ToArray(), [], [], [], []);
        Export(synthetic, "alpha-ramp", "Synthetic full 0..255 alpha range; shared texture with distinct material factors.");
        Export(synthetic with { Resources = synthetic.Resources & ~SmoExportResourceTypes.Textures },
            "textures-disabled", "Resource filter must preserve scalar alpha and omit texture connections.");

        foreach (float invalid in new[] { -0.01f, 1.01f, float.NaN, float.PositiveInfinity })
        {
            string sentinel = Path.Combine(directory, "invalid-alpha.fbx");
            File.WriteAllText(sentinel, "existing output");
            try
            {
                FbxExporter.Export(synthetic with
                {
                    Meshes = [syntheticMeshes[0] with { MaterialColor = new Vector4(1, 1, 1, invalid) }]
                }, sentinel);
                throw new InvalidOperationException("Invalid alpha was accepted.");
            }
            catch (InvalidDataException)
            {
                Check(File.ReadAllText(sentinel) == "existing output", "Invalid alpha preserves existing output.");
            }
        }

        foreach (string source in sources)
        {
            byte[] original = File.ReadAllBytes(source);
            var scene = SmoSceneBuilder.Build(SmoDocument.Load(source));
            foreach (var group in scene.Meshes.SelectMany(mesh => new[] { mesh.Texture, mesh.EffectTexture })
                         .OfType<SmoExportTexture>().GroupBy(item => item.ObjectIndex))
                Check(group.All(item => ReferenceEquals(item, group.First())),
                    "Scene builder encodes each shared texture once.");
            // Stable unique names let the independent Blender reader identify every material.
            scene = scene with { Meshes = scene.Meshes.Select(mesh => mesh with
                { Name = "mesh_" + mesh.ObjectIndex }).ToArray() };
            string name = Path.GetFileNameWithoutExtension(source);
            Export(scene, name + "-original", Path.GetFullPath(source));
            if (scene.Meshes.Any(mesh => mesh.Texture?.OpacityMaskPngBytes is not null))
            {
                var changed = scene with { Meshes = scene.Meshes.Select(mesh =>
                    mesh.Texture?.OpacityMaskPngBytes is not null ? mesh with
                    {
                        MaterialColor = new Vector4(mesh.MaterialColor.X, mesh.MaterialColor.Y,
                            mesh.MaterialColor.Z, 0.5f), UsesAlphaBlend = true
                    } : mesh).ToArray() };
                Export(changed, name + "-combined", "Controlled material alpha=0.5 on original decoded SMO geometry/textures.");
            }
            Check(original.AsSpan().SequenceEqual(File.ReadAllBytes(source)), name + ": original SMO unchanged");
        }
        var report = new
        {
            schemaVersion = 1, createdUtc = DateTime.UtcNow, checks,
            elapsedSeconds = timer.Elapsed.TotalSeconds, peakWorkingSet = Process.GetCurrentProcess().PeakWorkingSet64,
            bridge = FbxExporter.FindNativeBridgeExecutable(),
            bridgeSha256 = Hash(File.ReadAllBytes(FbxExporter.FindNativeBridgeExecutable()!)),
            alphaTolerance = 0.5 / 65535 + 0.0000001, exports
        };
        File.WriteAllText(Path.Combine(directory, "expected.json"),
            JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"PASS: {checks} checks; {exports.Count} FBX files; {timer.Elapsed.TotalSeconds:F3}s; independent import pending.");
        return 0;
    }
}
