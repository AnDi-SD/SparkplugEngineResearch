using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class ModelGraphLinkRegression
{
    public static int Run(string templatePath, string output)
    {
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        byte[] source = File.ReadAllBytes(templatePath);
        var templateDocument = SmoDocument.ParseOwned(source, templatePath);
        SmoMesh? template = null;
        foreach (var entry in templateDocument.Objects.Where(value => value.TypeHash == SmoClassIds.MeshData))
        {
            if (!SmoMeshDecoder.TryDecode(templateDocument, entry, out var candidate, out _) ||
                candidate.HasSkinningData) continue;
            try { _ = SmoMeshResourceReplacer.ValidateWritableLayout(entry, candidate); }
            catch (InvalidOperationException) { continue; }
            template = candidate;
            break;
        }
        if (template is null) throw new InvalidDataException("Regression requires one writable rigid mesh template.");
        var triangle = new ImportedMesh("link-proof", [Vector3.Zero, Vector3.UnitX, Vector3.UnitY],
            [Vector3.UnitZ, Vector3.UnitZ, Vector3.UnitZ], [Vector2.Zero, Vector2.UnitX, Vector2.UnitY],
            [0, 1, 2], [0xffffffff, 0xffffffff, 0xffffffff], 0);
        byte[] mesh = SmoMeshDataWriter.CreateTriangleList(SmoMesh.CreateTransient(template,
            triangle.Positions, triangle.Normals, triangle.TextureCoordinates,
            triangle.DiffuseColors, [], [], triangle.TriangleIndices));
        var donor = new ImportedScene([triangle], [],
            [new ImportedMaterial("alpha-proof", AlphaMode: ImportedMaterialAlphaMode.Blend)]);
        Directory.CreateDirectory(output);
        var cases = new List<object>();
        foreach ((bool referenced, bool redirectPlacement) in new[] { (false, false), (true, false), (true, true) })
        {
            string name = redirectPlacement ? "superseded-mesh-reference" : referenced ? "referenced-materials" : "inline-materials";
            byte[] before = CreateFixture(mesh, referenced, redirectPlacementMesh: redirectPlacement);
            byte[] snapshot = before.ToArray();
            var document = SmoDocument.ParseOwned(before, name);
            SmoObjectEntry Entry(uint id) => document.Objects.Single(value => value.Id == id);
            var loaded = SmoLoadedResources.Get(document);
            Check(!document.HasErrors && loaded.LoadIssue is null, loaded.LoadIssue ?? "Fixture must load actual objects.");
            Check(loaded.Models[Entry(3).Index].MeshId == 10 &&
                loaded.Models[Entry(4).Index].MeshId == (redirectPlacement ? 11u : 10u),
                "Actual Models retain the final shared/other Mesh assignments.");
            Check(loaded.Models[Entry(3).Index].MaterialId == 20 && loaded.Models[Entry(4).Index].MaterialId == 21,
                "Shared geometry retains each actual Model's distinct material.");
            if (referenced)
            {
                Check(Entry(20).ParentIndex != Entry(3).Index && Entry(21).ParentIndex != Entry(4).Index,
                    "Both selected materials are reference-only in their consuming Models.");
                Check(Entry(99).ParentIndex == Entry(3).Index && Entry(98).ParentIndex == Entry(4).Index,
                    "Earlier inline materials are byte owners, not the final assigned materials.");
            }
            var plan = SmoLevelModelGraphReplacer.ResolveWritablePlan(document, Entry(10).Index);
            Check(plan.RootObjectId == 1 && plan.Components.Count == 1,
                "Shared Mesh retains one physical replacement component.");
            Check(plan.Components[0] == new SmoLevelModelGraphComponent(3, 20, 10, Entry(10).Index),
                "Plan combines the physical Mesh carrier with its actual material link.");
            if (redirectPlacement)
            {
                Check(Entry(11).ParentIndex == Entry(5).Index && Entry(5).ParentIndex == Entry(6).Index &&
                    Entry(6).ParentIndex == Entry(1).ParentIndex && Entry(6).PhysicalEnd <= Entry(1).PhysicalOffset,
                    "Other Mesh has a complete earlier physical carrier outside the selected RenderNode.");
                Check(loaded.Models[Entry(5).Index].MeshId == 11,
                    "Other Mesh is owned by an actual Model in the sibling RenderNode.");
            }
            var result = SmoLevelModelGraphReplacer.Replace(document, Entry(10).Index, donor,
                ReplacementTransform.Identity, Matrix4x4.Identity);
            var after = SmoDocument.ParseOwned(result.Data, name + "-replaced");
            var actual = SmoLoadedResources.Get(after);
            SmoObjectEntry After(uint id) => after.Objects.Single(value => value.Id == id);
            Check(!after.HasErrors && actual.LoadIssue is null, actual.LoadIssue ?? "Edited graph must load actual objects.");
            Check(result.MeshObjectIds.Count == 1 && !after.Objects.Any(value => value.Id == 10),
                "Replacement creates one shared Mesh and removes the old resource.");
            foreach (uint modelId in new uint[] { 3, 4 })
            {
                var model = actual.Models[After(modelId).Index];
                bool active = modelId == 3 || !redirectPlacement;
                Check(model.MeshId == (active ? result.MeshObjectIds[10] : 11u) && model.MaterialId == (modelId == 3 ? 20u : 21u),
                    "Replacement redirects each Model without exchanging its material.");
                Check(model.Material is not null && model.Material.Passes.Count == 1 &&
                    model.Material.Passes[0].Blend == (active ? 2u : 0u) &&
                    model.Material.RenderStates.SequenceEqual(active
                        ? new uint[] { 0, 0, 1, 0, 1, 0, 3, 0, 2, 0, 6 } : new uint[11]),
                    "Authoring alpha policy changes only actual consumers of the replaced Mesh.");
            }
            if (referenced)
                foreach (uint unused in new uint[] { 98, 99 })
                    Check(ObjectBytes(document, Entry(unused)).SequenceEqual(ObjectBytes(after, After(unused))),
                        "Inactive inline material bytes remain unchanged.");
            if (redirectPlacement)
                Check(!result.MeshObjectIds.ContainsKey(11) &&
                    ObjectBytes(document, Entry(6)).SequenceEqual(ObjectBytes(after, After(6))) &&
                    ObjectBytes(document, Entry(11)).SequenceEqual(ObjectBytes(after, After(11))),
                    "Sibling RenderNode and its Mesh remain byte-identical outside the replacement scope.");
            Check(document.Data.Span.SequenceEqual(snapshot), "Replacement preserves the input document.");
            File.WriteAllBytes(Path.Combine(output, name + ".smo"), before);
            File.WriteAllBytes(Path.Combine(output, name + "-replaced.smo"), result.Data);
            cases.Add(new { name, input_sha256 = Convert.ToHexString(SHA256.HashData(before)),
                output_sha256 = Convert.ToHexString(SHA256.HashData(result.Data)), objects = after.Objects.Count });
        }
        var inactive = SmoDocument.ParseOwned(CreateFixture(mesh, true, redirectPrimaryMesh: true));
        var inactiveLoaded = SmoLoadedResources.Get(inactive);
        Check(inactiveLoaded.LoadIssue is null &&
            inactiveLoaded.Models[inactive.Objects.Single(value => value.Id == 3).Index].MeshId == 11,
            "Inactive carrier fixture loads its later reference to another actual Mesh.");
        bool rejected = false;
        try { SmoLevelModelGraphReplacer.ResolvePlan(inactive, inactive.Objects.Single(value => value.Id == 10).Index); }
        catch (InvalidOperationException error) when (error.Message.Contains("not the active mesh", StringComparison.Ordinal))
        { rejected = true; }
        Check(rejected, "An inactive physical Mesh carrier is not silently treated as the Model's active mesh.");
        var boneSlotCases = new List<object>();
        foreach (bool redirected in new[] { false, true })
        {
            string name = redirected ? "superseded-skin-mesh" : "stored-skin-mesh";
            byte[] bytes = CreateFixture(mesh, true, redirectPrimaryMesh: redirected, primarySkin: true);
            byte[] snapshot = bytes.ToArray();
            var document = SmoDocument.ParseOwned(bytes, name);
            SmoObjectEntry Entry(uint id) => document.Objects.Single(value => value.Id == id);
            Check(!document.HasErrors, name + ": directed fixture has a complete catalog.");
            Check(SmoSkinDecoder.TryDecode(document, Entry(3), out var skin, out _),
                name + ": actual Skin metadata accepts the directed fixture.");
            Check(skin!.BaseMesh.ObjectId == (redirected ? 11u : 10u) && skin.Bones.Count == 2,
                name + ": final BaseMesh assignment and both palette entries are observed.");
            Check(Entry(10).ParentIndex == Entry(3).Index,
                name + ": the earlier Mesh remains physically inside Skin.");
            IReadOnlyList<SmoBoneSlot> slots = SmoMeshReplacer.GetBoneSlots(document, Entry(10));
            Check(redirected ? slots.Count == 0 : slots.SequenceEqual(new SmoBoneSlot[]
                    { new(0, 30, "link-30"), new(1, 31, "link-31") }),
                name + ": only the active stored mesh receives the unchanged palette order, IDs and names.");
            if (redirected)
                Check(SmoMeshReplacer.GetBoneSlots(document, Entry(11)).Count == 0,
                    "A reference-only Skin consumer does not replace the other mesh's physical Model owner.");
            else
            {
                var foreign = SmoDocument.ParseOwned(snapshot.ToArray(), name + "-foreign");
                SmoObjectEntry foreignMesh = foreign.Objects.Single(value => value.Id == 10);
                Check(foreignMesh.Index == Entry(10).Index && !ReferenceEquals(foreignMesh, Entry(10)) &&
                    SmoMeshReplacer.GetBoneSlots(document, foreignMesh).Count == 0,
                    "A foreign document entry cannot acquire the same-index Skin palette.");
            }
            Check(document.Data.Span.SequenceEqual(snapshot), name + ": bone-slot lookup preserves source bytes.");
            File.WriteAllBytes(Path.Combine(output, name + ".smo"), bytes);
            boneSlotCases.Add(new { name, input_sha256 = Convert.ToHexString(SHA256.HashData(bytes)),
                slots, metadataOnly = true });
        }
        Check(File.ReadAllBytes(templatePath).AsSpan().SequenceEqual(source), "Original template file is unchanged.");
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(new
        { status = "passed", checks, template = Path.GetFullPath(templatePath),
            template_sha256 = Convert.ToHexString(SHA256.HashData(source)), cases, boneSlotCases },
            new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"PASS Model authoring links: {cases.Count} complete replacements, {checks} checks");
        return 0;
    }

    private static ReadOnlySpan<byte> ObjectBytes(SmoDocument document, SmoObjectEntry entry) =>
        document.Data.Span.Slice(checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));

    // Directed test input only: these explicit wire relationships are the
    // independent expectation; production reads use actual Sparkplug objects.
    private static byte[] CreateFixture(byte[] meshBytes, bool referenced,
        bool redirectPrimaryMesh = false, bool redirectPlacementMesh = false, bool primarySkin = false)
    {
        FixtureObject Material(uint id)
        {
            var material = new FixtureObject(id, SmoClassIds.MaterialData);
            material.Field(0, new byte[44]);
            material.Field(3, new byte[4]);
            material.End();
            return material;
        }
        var firstMaterial = Material(20);
        var secondMaterial = Material(21);
        var otherMesh = new FixtureObject(11, SmoClassIds.MeshData, meshBytes);
        var root = new FixtureObject(1, SmoClassIds.RenderNode);
        root.End(); // Node section
        if (referenced)
        {
            var carrier = new FixtureObject(2, SmoClassIds.Model);
            carrier.Reference(0, firstMaterial, inline: true);
            carrier.Reference(0, secondMaterial, inline: true);
            carrier.End(); carrier.End();
            root.Reference(0, carrier, inline: true);
        }
        var primary = new FixtureObject(3, primarySkin ? SmoClassIds.Skin : SmoClassIds.Model);
        if (referenced) primary.Reference(0, Material(99), inline: true);
        primary.Reference(0, firstMaterial, inline: !referenced);
        primary.End(); // Renderable section
        var mesh = new FixtureObject(10, SmoClassIds.MeshData, meshBytes);
        primary.Reference(0, mesh, inline: true);
        if (redirectPrimaryMesh) primary.Reference(0, otherMesh, inline: false);
        primary.End();
        if (primarySkin)
        {
            // Explicit test palette: two reference-only Nodes, identity inverse
            // binds. The existing shared Skin reader is the production parser.
            byte[] identity = new byte[64];
            foreach (int component in new[] { 0, 5, 10, 15 })
                BinaryPrimitives.WriteUInt32LittleEndian(identity.AsSpan(component * 4), 0x3f800000);
            primary.Field(0, [..BitConverter.GetBytes(4u), ..BitConverter.GetBytes(2u),
                ..BitConverter.GetBytes(30u), ..BitConverter.GetBytes(0u), ..identity,
                ..BitConverter.GetBytes(31u), ..BitConverter.GetBytes(0u), ..identity]);
            primary.End();
        }
        root.Reference(0, primary, inline: true);
        var placement = new FixtureObject(4, SmoClassIds.Model);
        if (referenced) placement.Reference(0, Material(98), inline: true);
        placement.Reference(0, secondMaterial, inline: !referenced);
        placement.End();
        placement.Reference(0, mesh, inline: false);
        if (redirectPlacementMesh) placement.Reference(0, otherMesh, inline: false);
        placement.End();
        root.Reference(0, placement, inline: true);
        root.End();

        if (redirectPrimaryMesh || redirectPlacementMesh || primarySkin)
        {
            var scene = new FixtureObject(100, SmoClassIds.Node);
            if (primarySkin)
                foreach (uint boneId in new uint[] { 30, 31 })
                {
                    var bone = new FixtureObject(boneId, SmoClassIds.Node);
                    bone.End();
                    scene.Reference(5, bone, inline: true);
                }
            // Node field 5 is an actual child reference. Serialize the sibling
            // first so every later Mesh 11 reference follows its inline body.
            if (redirectPrimaryMesh || redirectPlacementMesh)
            {
                var otherModel = new FixtureObject(5, SmoClassIds.Model);
                otherModel.End();
                otherModel.Reference(0, otherMesh, inline: true);
                otherModel.End();
                var sibling = new FixtureObject(6, SmoClassIds.RenderNode);
                sibling.End();
                sibling.Reference(0, otherModel, inline: true);
                sibling.End();
                scene.Reference(5, sibling, inline: true);
            }
            scene.Reference(5, root, inline: true);
            scene.End();
            root = scene;
        }

        var entries = root.Entries(0).ToArray();
        byte[][] names = entries.Select(entry => Encoding.ASCII.GetBytes($"link-{entry.Object.Id}\0")).ToArray();
        int dataStart = SmoHeader.Size + entries.Select((_, index) => 18 + names[index].Length).Sum() + 4;
        byte[] result = new byte[dataStart + root.Bytes.Count];
        "FFPS"u8.CopyTo(result);
        void Word(int offset, uint value) => BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(offset), value);
        Word(4, 0x26); Word(12, checked((uint)result.Length)); Word(16, 2);
        Word(20, checked((uint)dataStart)); Word(24, checked((uint)root.Bytes.Count)); Word(28, checked((uint)entries.Length));
        int cursor = SmoHeader.Size;
        for (int index = 0; index < entries.Length; ++index)
        {
            var entry = entries[index];
            Word(cursor, entry.Object.Id); cursor += 4;
            BinaryPrimitives.WriteUInt16LittleEndian(result.AsSpan(cursor), checked((ushort)names[index].Length)); cursor += 2;
            names[index].CopyTo(result, cursor); cursor += names[index].Length;
            Word(cursor, entry.Object.Type); Word(cursor + 4, checked((uint)entry.Offset));
            Word(cursor + 8, checked((uint)entry.Object.Bytes.Count)); cursor += 12;
        }
        root.Bytes.CopyTo(result, dataStart);
        return result;
    }

    private sealed class FixtureObject(uint id, uint type, byte[]? body = null)
    {
        public uint Id { get; } = id;
        public uint Type { get; } = type;
        public List<byte> Bytes { get; } = body?.ToList() ?? [..BitConverter.GetBytes(type), .."SBOO"u8];
        private readonly List<(FixtureObject Object, int Offset)> children = [];
        public void End() => Bytes.Add(0);
        public void Field(int type, byte[] payload) => Bytes.AddRange(SmoDataBlockWriter.BuildField(type, payload));
        public void Reference(int type, FixtureObject target, bool inline)
        {
            byte[] payload = [..BitConverter.GetBytes(target.Id),
                ..BitConverter.GetBytes(inline ? checked((uint)target.Bytes.Count) : 0u),
                ..(inline ? target.Bytes : Enumerable.Empty<byte>())];
            byte[] field = SmoDataBlockWriter.BuildField(type, payload);
            if (inline) children.Add((target, Bytes.Count + field.Length - payload.Length + 8));
            Bytes.AddRange(field);
        }
        public IEnumerable<(FixtureObject Object, int Offset)> Entries(int offset)
        {
            yield return (this, offset);
            foreach (var child in children)
                foreach (var entry in child.Object.Entries(offset + child.Offset)) yield return entry;
        }
    }
}
