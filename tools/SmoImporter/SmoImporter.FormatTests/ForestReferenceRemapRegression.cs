using System.Buffers.Binary;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class ForestReferenceRemapRegression
{
    public static int Run(string templatePath, string output)
    {
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        void Reject(Action action, string message)
        {
            bool rejected = false;
            try { action(); }
            catch (Exception error) when (error is ArgumentException or InvalidDataException or NotSupportedException)
            { rejected = true; }
            Check(rejected, message);
        }
        byte[] original = File.ReadAllBytes(templatePath);
        var templateDocument = SmoDocument.ParseOwned(original, templatePath);
        SmoMesh? template = null;
        foreach (var entry in templateDocument.Objects.Where(value => value.TypeHash == SmoClassIds.MeshData))
        {
            if (!SmoMeshDecoder.TryDecode(templateDocument, entry, out var candidate, out _)) continue;
            try { _ = SmoMeshResourceReplacer.ValidateWritableLayout(entry, candidate); }
            catch (InvalidOperationException) { continue; }
            template = candidate; break;
        }
        if (template is null) throw new InvalidDataException("Regression requires one writable mesh template.");
        byte[] meshBytes = SmoMeshDataWriter.CreateTriangleList(SmoMesh.CreateTransient(template,
            [Vector3.Zero, Vector3.UnitX, Vector3.UnitY], [Vector3.UnitZ, Vector3.UnitZ, Vector3.UnitZ],
            [Vector2.Zero, Vector2.UnitX, Vector2.UnitY], [0xffffffff, 0xffffffff, 0xffffffff], [], [], [0, 1, 2]));
        var emptyRoot = new FixtureObject(1, SmoClassIds.Node); emptyRoot.End();
        var source = SmoDocument.ParseOwned(Container(emptyRoot), "remap-source");
        var root = new FixtureObject(1, SmoClassIds.Node);
        var render = new FixtureObject(2, SmoClassIds.RenderNode);
        var bone = new FixtureObject(7, SmoClassIds.Node);
        bone.Field(3, [1]);
        bone.Field(30, [..BitConverter.GetBytes(7u), ..new byte[4]]);
        bone.End();
        render.Reference(5, bone, inline: true); render.End();
        var skin = new FixtureObject(3, SmoClassIds.Skin);
        skin.End();
        skin.Reference(0, new FixtureObject(4, SmoClassIds.MeshData, meshBytes), inline: true);
        skin.End();
        var palette = new List<byte>([..BitConverter.GetBytes(4u), ..BitConverter.GetBytes(3u)]);
        for (int index = 0; index < 3; ++index)
        {
            palette.AddRange(BitConverter.GetBytes(index == 2 ? 1u : 7u));
            palette.AddRange(new byte[4]);
            Matrix4x4 matrix = Matrix4x4.Identity; matrix.M41 = index;
            byte[] bytes = new byte[64]; MemoryMarshal.Write(bytes.AsSpan(), in matrix);
            palette.AddRange(bytes);
        }
        skin.Field(0, palette.ToArray()); skin.End();
        render.Reference(0, skin, inline: true); render.End();
        root.Reference(5, render, inline: true); root.End();
        byte[] additiveBytes = Container(root);
        var additive = SmoDocument.ParseOwned(additiveBytes, "remap-additive");
        var loaded = SmoLoadedResources.Get(additive);
        Check(!source.HasErrors && !additive.HasErrors && loaded.LoadIssue is null,
            loaded.LoadIssue ?? "Directed additive fixture must load actual Node/RenderNode/Skin/Mesh objects.");
        var plan = SmoAdditiveForestPlanner.Create(source, additive);
        Check(plan.Operations.Count == 1 && plan.ReferenceRanges?.Count == 1 &&
            plan.GeneratedObjectIds.Order().SequenceEqual(new uint[] { 2, 3, 4, 7 }),
            "One generated branch has sealed actual-reader provenance for every generated object.");
        byte[] originalField = plan.Operations[0].Attachment.FieldData.ToArray();
        var idMap = plan.GeneratedObjectIds.ToDictionary(id => id, id => id + 100);
        var mapped = SmoAdditiveForestPlanner.RemapObjectIds(plan, idMap);
        Check(plan.Operations[0].Attachment.FieldData.AsSpan().SequenceEqual(originalField),
            "Remapping does not mutate the original sealed plan.");
        Check(mapped.ReferenceRanges?.Count == 1 && mapped.GeneratedObjectIds.Order().SequenceEqual(new uint[] { 102, 103, 104, 107 }),
            "Mapped IDs and sealed provenance are retained together.");
        Check(mapped.ReferenceRanges![0].Sites.Any(site => site.ObjectId == 107 && site.ConsumerId == 103),
            "Nested reference consumer identities follow the same ID mapping.");
        // Our private worker transports existing observations. JSON decoding
        // is not another reader execution or an independent source of proof.
        var exported = plan.ReferenceRanges![0].ExportWorkerTransport();
        string workerJson = JsonSerializer.Serialize(exported);
        var workerTransport = JsonSerializer.Deserialize<SmoFileReferenceRangeTransport>(workerJson) ??
            throw new InvalidDataException("Worker JSON did not retain its reference observations.");
        var workerRange = SmoFileReferenceRange.RestoreWorkerTransport(originalField, workerTransport);
        var workerPlan = plan with { ReferenceRanges = [workerRange] };
        var exportContext = new SmoFileReferenceWorkerContext();
        var firstTransport = plan.ReferenceRanges[0].ExportWorkerTransport(exportContext);
        var nextTransport = plan.ReferenceRanges[0].ExportWorkerTransport(exportContext);
        Check(firstTransport.CatalogIds.Length != 0 && nextTransport.CatalogIds.Length == 0 &&
            firstTransport.CatalogSha256 == nextTransport.CatalogSha256,
            "Worker transmits a shared catalog once for repeated ranges.");
        Reject(() => SmoFileReferenceRange.RestoreWorkerTransport(originalField, nextTransport),
            "A worker catalog reference requires its prior definition.");
        var restoreContext = new SmoFileReferenceWorkerContext();
        _ = SmoFileReferenceRange.RestoreWorkerTransport(originalField, firstTransport, restoreContext);
        var restoredNext = SmoFileReferenceRange.RestoreWorkerTransport(originalField, nextTransport, restoreContext);
        Check(restoredNext.Remap(originalField, idMap).Data.AsSpan().SequenceEqual(mapped.Operations[0].Attachment.FieldData),
            "Repeated worker catalog references preserve the exact ID map.");
        var workerMapped = SmoAdditiveForestPlanner.RemapObjectIds(workerPlan, idMap);
        Check(workerMapped.Operations[0].Attachment.FieldData.AsSpan().SequenceEqual(mapped.Operations[0].Attachment.FieldData) &&
            workerMapped.ReferenceRanges![0].Sites.SequenceEqual(mapped.ReferenceRanges[0].Sites) &&
            workerMapped.ReferenceRanges[0].Objects.SequenceEqual(mapped.ReferenceRanges[0].Objects),
            "Worker JSON roundtrip retains exact bytes, object metadata and reference observations.");
        Reject(() => SmoFileReferenceRange.RestoreWorkerTransport(originalField,
            workerTransport with { ContentSha256 = new string('0', 64) }),
            "Worker transport with an incorrect attachment hash is rejected.");
        var invalidSites = workerTransport.Sites.ToArray();
        invalidSites[0] = invalidSites[0] with { Offset = originalField.Length };
        Reject(() => SmoFileReferenceRange.RestoreWorkerTransport(originalField,
            workerTransport with { Sites = invalidSites }),
            "Worker transport cannot name an out-of-range reference site.");
        byte[] zeroBytes = new byte[8];
        Reject(() => SmoFileReferenceRange.RestoreWorkerTransport(zeroBytes,
            workerTransport with { ContentSha256 = Convert.ToHexString(SHA256.HashData(zeroBytes)),
                Sites = [new(0, 0), new(1, 0)], Objects = [] }),
            "Worker transport cannot name overlapping four-byte ID sites.");
        invalidSites = workerTransport.Sites.ToArray();
        invalidSites[0] = invalidSites[0] with { InlineSize = invalidSites[0].InlineSize - 1 };
        Reject(() => SmoFileReferenceRange.RestoreWorkerTransport(originalField,
            workerTransport with { Sites = invalidSites }),
            "Worker inline size must match the actual size word, not only fit the range.");
        byte originalLast = additiveBytes[^1];
        additiveBytes[^1] ^= 1;
        Reject(() => loaded.ReferenceTrace!.CaptureRange(additive, 0, additiveBytes.Length),
            "Underlying ParseOwned array mutation cannot obtain fresh provenance from a stale graph.");
        additiveBytes[^1] = originalLast;
        var once = Install(mapped, "remap-once");
        Verify(once, 100);
        var twice = SmoAdditiveForestPlanner.RemapObjectIds(mapped,
            mapped.GeneratedObjectIds.ToDictionary(id => id, id => id + 100));
        var repeated = Install(twice, "remap-twice");
        Verify(repeated, 200);
        var manual = new SmoAdditiveForestPlan(plan.Operations, plan.GeneratedObjectIds);
        Reject(() => SmoAdditiveForestPlanner.RemapObjectIds(manual, idMap),
            "Manual plans without provenance cannot remap IDs.");
        Check(Install(manual, "manual-insert").Objects.Count == additive.Objects.Count,
            "Ordinary insertion of an unchanged manual plan still works.");
        Reject(() => SmoAdditiveForestPlanner.RemapObjectIds(plan with { ReferenceRanges = [] }, idMap),
            "Mismatched operation/provenance counts are rejected.");
        var missing = new Dictionary<uint, uint>(idMap); missing.Remove(7);
        Reject(() => SmoAdditiveForestPlanner.RemapObjectIds(plan, missing), "Incomplete ID maps are rejected.");
        var duplicate = new Dictionary<uint, uint>(idMap) { [7] = 102 };
        Reject(() => SmoAdditiveForestPlanner.RemapObjectIds(plan, duplicate), "Many-to-one ID maps are rejected.");
        var zero = new Dictionary<uint, uint>(idMap) { [7] = 0 };
        Reject(() => SmoAdditiveForestPlanner.RemapObjectIds(plan, zero), "Zero destination IDs are rejected.");
        var retainedCollision = new Dictionary<uint, uint>(idMap) { [7] = 1 };
        Reject(() => SmoAdditiveForestPlanner.RemapObjectIds(plan, retainedCollision),
            "Remapped IDs cannot collide with the retained imported ancestor.");
        byte[] changed = originalField.ToArray(); changed[^2] ^= 1;
        var operation = plan.Operations[0];
        var wrongEntries = operation.Attachment.Entries.ToArray();
        wrongEntries[0] = wrongEntries[0] with { RelativeOffset = wrongEntries[0].RelativeOffset + 1 };
        var wrongPrefix = plan with { Operations = [operation with
            { Attachment = operation.Attachment with { Entries = wrongEntries } }] };
        Reject(() => SmoAdditiveForestPlanner.RemapObjectIds(wrongPrefix, idMap),
            "Declared object prefixes must coincide with actual traced reference sites.");
        foreach (var changedEntry in new[]
        {
            operation.Attachment.Entries[0] with { TypeHash = SmoClassIds.Node },
            operation.Attachment.Entries[0] with { SerializedSize = operation.Attachment.Entries[0].SerializedSize + 1 },
            operation.Attachment.Entries[0] with { RawName = "different-name\0"u8.ToArray() }
        })
        {
            var wrongCatalog = operation.Attachment.Entries.ToArray(); wrongCatalog[0] = changedEntry;
            var invalid = plan with { Operations = [operation with
                { Attachment = operation.Attachment with { Entries = wrongCatalog } }] };
            Reject(() => SmoAdditiveForestPlanner.RemapObjectIds(invalid, idMap),
                "Entry type, size and raw name must match the canonical observed FAT metadata.");
        }
        var wrongOwner = plan with { Operations = [operation with
            { Attachment = operation.Attachment with { TargetOwnerId = 7 } }] };
        Reject(() => SmoAdditiveForestPlanner.RemapObjectIds(wrongOwner, idMap),
            "Root inline reference consumer must match the declared attachment owner.");
        var tampered = plan with { Operations = [operation with
            { Attachment = operation.Attachment with { FieldData = changed } }] };
        Reject(() => SmoAdditiveForestPlanner.RemapObjectIds(tampered, idMap),
            "Changed bytes cannot reuse provenance sealed for another range.");
        Check(File.ReadAllBytes(templatePath).AsSpan().SequenceEqual(original), "Original mesh template is unchanged.");
        Directory.CreateDirectory(output);
        File.WriteAllBytes(Path.Combine(output, "additive.smo"), additiveBytes);
        File.WriteAllBytes(Path.Combine(output, "remapped.smo"), once.Data.ToArray());
        File.WriteAllBytes(Path.Combine(output, "remapped-twice.smo"), repeated.Data.ToArray());
        File.WriteAllText(Path.Combine(output, "worker-reference-transport.json"), workerJson + "\n");
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(new
        { status = "passed", checks, template = Path.GetFullPath(templatePath),
            template_sha256 = Convert.ToHexString(SHA256.HashData(original)),
            input_sha256 = Convert.ToHexString(SHA256.HashData(additiveBytes)),
            output_sha256 = Convert.ToHexString(SHA256.HashData(once.Data.Span)),
            repeated_sha256 = Convert.ToHexString(SHA256.HashData(repeated.Data.Span)) },
            new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"PASS exact forest reference remap: {checks} checks");
        return 0;

        SmoDocument Install(SmoAdditiveForestPlan value, string name) => SmoDocument.ParseOwned(
            SmoVisualForestInjector.Inject(source, 1, value.Operations.Select(item => item.Attachment).ToArray()), name);
        void Verify(SmoDocument document, uint shift)
        {
            var actual = SmoLoadedResources.Get(document);
            Check(!document.HasErrors && actual.LoadIssue is null, actual.LoadIssue ?? "Remapped graph loads actual objects.");
            var skinEntry = document.Objects.Single(entry => entry.Id == 3 + shift);
            Check(actual.Models[skinEntry.Index].MeshId == 4 + shift, "Actual Skin points to its remapped Mesh.");
            Check(SmoSkinDecoder.TryDecode(document, skinEntry, out var decoded, out string error), error);
            Check(decoded!.Bones.Select(value => value.NodeObjectId).SequenceEqual(new uint[] { 7 + shift, 7 + shift, 1 }),
                "Both nested palette references change; the imported ancestor reference stays unchanged.");
            Check(decoded.Bones.Select(value => value.InverseBindMatrix.M41).SequenceEqual(new float[] { 0, 1, 2 }),
                "Palette matrix values are not interpreted as IDs.");
            var boneEntry = document.Objects.Single(entry => entry.Id == 7 + shift);
            ReadOnlySpan<byte> bytes = document.Data.Span.Slice(checked((int)boneEntry.PhysicalOffset), checked((int)boneEntry.SerializedSize));
            int cursor = 8; bool found = false;
            while (cursor < bytes.Length && SmoDataBlockReader.TryReadHeader(bytes, cursor, out var field))
            {
                if (field.FieldType == 30)
                {
                    Check(field.PayloadSize == 8 && BinaryPrimitives.ReadUInt32LittleEndian(bytes[field.PayloadOffset..]) == 7 &&
                        BinaryPrimitives.ReadUInt32LittleEndian(bytes[(field.PayloadOffset + 4)..]) == 0,
                        "Unknown eight-byte ID-shaped payload remains byte-identical.");
                    found = true;
                }
                cursor = checked((int)field.PayloadEnd);
            }
            Check(found, "Collision field survives remapping.");
        }
    }

    // Explicit directed wire fixture; production reference discovery is solely
    // the actual reader's trace, not these test construction helpers.
    private static byte[] Container(FixtureObject root)
    {
        var entries = root.Entries(0).ToArray();
        byte[][] names = entries.Select(entry => Encoding.ASCII.GetBytes($"remap-{entry.Object.Id}\0")).ToArray();
        int dataStart = SmoHeader.Size + entries.Select((_, index) => 18 + names[index].Length).Sum() + 4;
        byte[] result = new byte[dataStart + root.Bytes.Count];
        "FFPS"u8.CopyTo(result);
        void Word(int offset, uint value) => BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(offset), value);
        Word(4, 0x26); Word(12, checked((uint)result.Length)); Word(16, 2);
        Word(20, checked((uint)dataStart)); Word(24, checked((uint)root.Bytes.Count)); Word(28, checked((uint)entries.Length));
        int cursor = SmoHeader.Size;
        for (int index = 0; index < entries.Length; ++index)
        {
            var entry = entries[index]; Word(cursor, entry.Object.Id); cursor += 4;
            BinaryPrimitives.WriteUInt16LittleEndian(result.AsSpan(cursor), checked((ushort)names[index].Length)); cursor += 2;
            names[index].CopyTo(result, cursor); cursor += names[index].Length;
            Word(cursor, entry.Object.Type); Word(cursor + 4, checked((uint)entry.Offset));
            Word(cursor + 8, checked((uint)entry.Object.Bytes.Count)); cursor += 12;
        }
        root.Bytes.CopyTo(result, dataStart); return result;
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
