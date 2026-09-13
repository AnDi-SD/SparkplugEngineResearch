using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text.Json;
using SMOTextureTool.Core;
using SmoViewer.Core;
using CatalogDocument = SmoViewer.Core.SmoDocument;
using TextureDocument = SMOTextureTool.Core.SmoDocument;

// Deliberately uses only pre-existing TextureTool DTO members. The same test
// can run against the frozen old ToolCore DLL while ViewerCore stays current.
internal static class MaterialInspectionRegression
{
    private sealed record Getter(string Name, uint? Value, string? Error);
    private static string Hash(ReadOnlySpan<byte> bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static string HashFile(string path)
    {
        using var input = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(input));
    }
    private static Getter[] ReadGetters(MaterialReferenceInfo value)
    {
        (string Name, Func<uint> Read)[] calls = [
            (nameof(value.UnknownLayerState0), () => value.UnknownLayerState0),
            (nameof(value.ColorOperation), () => value.ColorOperation),
            (nameof(value.AlphaOperation), () => value.AlphaOperation),
            (nameof(value.AddressU), () => value.AddressU),
            (nameof(value.AddressV), () => value.AddressV),
            (nameof(value.BorderColor), () => value.BorderColor),
            (nameof(value.Filter), () => value.Filter),
            (nameof(value.TextureCoordinateIndex), () => value.TextureCoordinateIndex),
            (nameof(value.TextureTransformFlags), () => value.TextureTransformFlags)];
        return calls.Select(call =>
        {
            try { return new Getter(call.Name, call.Read(), null); }
            catch (Exception error) { return new Getter(call.Name, null, $"{error.GetType().Name}: {error.Message}"); }
        }).ToArray();
    }

    public static int Run(string book, string current, string output)
    {
        output = Path.GetFullPath(output);
        if (Directory.Exists(output)) throw new InvalidOperationException("Use a new output directory to preserve baseline evidence.");
        Directory.CreateDirectory(output);
        int checks = 0;
        var failures = new List<string>();
        var observations = new List<object>();
        void Check(bool condition, string message) { ++checks; if (!condition) failures.Add(message); }
        void Probe(string label, byte[] source, int expectedStatesField)
        {
            string before = Hash(source);
            var rows = new List<object>();
            var rawFields = new List<object>();
            string? failure = null;
            try
            {
                var catalog = CatalogDocument.ParseOwned(source);
                var document = TextureDocument.Parse(source);
                Check(!catalog.HasErrors, $"{label}: shared catalog is valid");
                Check(document.Textures.Count > 0, $"{label}: selected input exposes textures");
                foreach (var entry in catalog.Objects.Where(entry => entry.TypeHash == SmoClassIds.MaterialData))
                    foreach (var field in SmoObjectFieldReader.Read(catalog, entry).Where(field => field.FieldType is 8 or 17))
                    {
                        // Raw observation only: the generic shared reader supplies
                        // the exact field extent; native inspection supplies meaning.
                        Check(field.PayloadSize == 36, $"{label}: state field has exactly nine raw UInt32 words");
                        uint[] words = field.PayloadSize == 36 ? Enumerable.Range(0, 9)
                            .Select(index => BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span.Slice(index * 4, 4))).ToArray() : [];
                        rawFields.Add(new { material = entry.Index, field = field.FieldType, offset = field.AbsolutePayloadOffset,
                            bytes = field.PayloadSize, words, sha256 = Hash(field.Payload.Span) });
                    }
                int represented = 0;
                foreach (var texture in document.Textures)
                {
                    var dto = texture.Material;
                    Check(dto is not null, $"{label}: texture [{texture.ObjectIndex}] has material metadata");
                    if (dto is null) continue;
                    var getters = ReadGetters(dto);
                    // Save the actual arrays and exceptions before asserting;
                    // the old legacy reader must report LTS length0 honestly.
                    Check(dto.MaterialRenderStates.Count == 11, $"{label}: MRS count is11");
                    Check(dto.LayerTextureStates.Count == 9, $"{label}: LTS count is9, observed{dto.LayerTextureStates.Count}");
                    Check(getters.All(getter => getter.Error is null), $"{label}: all nine getters succeed; " +
                        string.Join("; ", getters.Where(getter => getter.Error is not null).Select(getter => $"{getter.Name}: {getter.Error}")));
                    var entry = catalog.Objects.Single(item => item.TypeHash == SmoClassIds.MaterialData && item.PhysicalOffset == dto.BlockOffset);
                    bool inspected = SmoMaterialInspection.TryInspect(catalog, entry, out var native, out string error);
                    Check(inspected, $"{label}: shared material inspector: {error}");
                    SmoMaterialInspection.Pass? pass = null;
                    SmoMaterialInspection.Layer? layer = null;
                    if (inspected)
                    {
                        Check(dto.MaterialRenderStates.SequenceEqual(native!.RenderStates), $"{label}: actual11 MRS values match native state");
                        pass = native.Passes.SingleOrDefault(item => item.Index + 1 == dto.PassIndex);
                        layer = pass?.Layers.SingleOrDefault(item => item.Index + 1 == dto.LayerIndex);
                        Check(pass is not null && layer is not null, $"{label}: pass/layer indices are one-based native ordinals");
                        if (pass is not null && layer is not null)
                        {
                            if (layer.TextureStatesFieldType == expectedStatesField) ++represented;
                            Check(dto.LayerClassId == layer.ClassId && dto.FinalBlendOperation == pass.FinalBlendOperation,
                                $"{label}: layer class and final pass blend match native state");
                            Check(dto.LayerTextureStates.SequenceEqual(layer.TextureStates), $"{label}: actual9 LTS values match native state");
                            Check(getters.Select(getter => getter.Value).SequenceEqual(layer.TextureStates.Select(value => (uint?)value)),
                                $"{label}: getters expose exact native words");
                            var container = SmoObjectFieldReader.Read(catalog, entry)
                                .SingleOrDefault(field => field.AbsoluteHeaderOffset == dto.TextureContainerOffset);
                            var textureEntry = catalog.Objects[texture.ObjectIndex];
                            Check(container is not null && container.FieldType == 10 && layer.TextureReference is not null,
                                $"{label}: texture extent belongs to an actual observed reference field");
                            if (container is not null && layer.TextureReference is { } reference)
                                Check(container.AbsolutePayloadOffset == reference.AbsolutePayloadOffset && container.PayloadSize == reference.Size
                                    && dto.TextureContainerSize == reference.Size && container.AbsolutePayloadOffset <= textureEntry.PhysicalOffset
                                    && container.AbsoluteEnd >= textureEntry.PhysicalEnd,
                                    $"{label}: DTO/shared field/native reference extents agree and contain the actual TextureData");
                        }
                    }
                    rows.Add(new { texture = texture.ObjectIndex, dto.Index, dto.BlockOffset, dto.TextureContainerOffset,
                        dto.TextureContainerSize, dto.PassIndex, dto.LayerIndex, dto.LayerClassId, dto.FinalBlendOperation,
                        render_states = dto.MaterialRenderStates, texture_states = dto.LayerTextureStates, getters,
                        native_state = native, native_layer = layer, native_error = error });
                }
                Check(represented > 0, $"{label}: selected texture demonstrates state field{expectedStatesField} (minus1 means actual constructor defaults)");
            }
            catch (Exception error) { failure = $"{error.GetType().Name}: {error.Message}"; Check(false, $"{label}: {failure}"); }
            finally
            {
                Check(Hash(source) == before, $"{label}: input array remains byte-identical");
                observations.Add(new { label, source_bytes = source.Length, source_sha256 = before,
                    source_sha256_after = Hash(source), raw_state_fields = rawFields, textures = rows, failure });
            }
        }

        var files = new[] { Path.GetFullPath(book), Path.GetFullPath(current) };
        byte[][] inputs = files.Select(File.ReadAllBytes).ToArray();
        string[] beforeFiles = inputs.Select(bytes => Hash(bytes)).ToArray();
        Probe("book-legacy-field8", inputs[0], 8);
        Probe("current-field17", inputs[1], 17);
        try
        {
            var catalog = CatalogDocument.ParseOwned(inputs[0]);
            var material = TextureDocument.Parse(inputs[0]).Textures.Single().Material
                ?? throw new InvalidDataException("Selected book texture has no material owner metadata.");
            var owner = catalog.Objects.Single(entry => entry.TypeHash == SmoClassIds.MaterialData
                && entry.PhysicalOffset == material.BlockOffset);
            var fields = SmoObjectFieldReader.Read(catalog, owner).Where(field => field.FieldType == 8).ToArray();
            Check(fields.Length == 1 && fields[0].PayloadSize == 36, "selected book texture owner has one exact nine-word legacy state field");
            if (fields.Length == 1)
            {
                var mutation = new SmoMutationTransaction(catalog);
                mutation.RemoveField(fields[0].ObjectIndex, fields[0].Selector);
                Probe("book-authored-states-removed-constructor-defaults", mutation.Commit().Data, -1);
            }
        }
        catch (Exception error) { Check(false, $"constructor-default fixture: {error.GetType().Name}: {error.Message}"); }
        for (int i = 0; i < files.Length; ++i)
            Check(HashFile(files[i]) == beforeFiles[i] && Hash(inputs[i]) == beforeFiles[i], $"{files[i]}: original file and array unchanged");
        string nativePath = Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll");
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(new
        {
            status = failures.Count == 0 ? "passed" : "failed", checks, failures,
            scope = "Two selected files and one removed authored-state field; Tool DTO versus shared native inspection, no replacement material reader.",
            source_files = files.Select((path, index) => new { path, sha256_before = beforeFiles[index], sha256_after = HashFile(path) }),
            texture_tool_assembly_sha256 = HashFile(typeof(TextureDocument).Assembly.Location),
            viewer_core_assembly_sha256 = HashFile(typeof(SmoMaterialInspection).Assembly.Location),
            test_assembly_sha256 = HashFile(typeof(MaterialInspectionRegression).Assembly.Location),
            native_dll_sha256 = File.Exists(nativePath) ? HashFile(nativePath) : null,
            cases = observations
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"TextureTool material inspection: {checks} checks, {failures.Count} failures.");
        return failures.Count == 0 ? 0 : 1;
    }

    // Final-only guard. Keep all new MaterialIssue member references in this
    // separate method so Run remains executable with the frozen old Tool DLL.
    public static int RunBindingGuards(string sourcePath, string output)
    {
        sourcePath = Path.GetFullPath(sourcePath); output = Path.GetFullPath(output);
        if (Directory.Exists(output)) throw new InvalidOperationException("Use a new output directory to preserve prior evidence.");
        Directory.CreateDirectory(output);
        byte[] source = File.ReadAllBytes(sourcePath);
        string beforeHash = Hash(source);
        int checks = 0;
        var failures = new List<string>();
        void Check(bool condition, string message) { ++checks; if (!condition) failures.Add(message); }
        object? observation = null;
        try
        {
            var catalog = CatalogDocument.ParseOwned(source);
            var before = TextureDocument.Parse(source).Textures.Single();
            var material = before.Material ?? throw new InvalidDataException("Selected texture requires an existing material binding.");
            var owner = catalog.Objects.Single(entry => entry.TypeHash == SmoClassIds.MaterialData && entry.PhysicalOffset == material.BlockOffset);
            var textureEntry = catalog.Objects[before.ObjectIndex];
            var fields = SmoObjectFieldReader.Read(catalog, owner);
            var mutation = new SmoMutationTransaction(catalog);
            mutation.AddField(owner.Index, 10, new byte[4], SmoFieldInsertion.BeforeTerminal);
            byte[] changed = mutation.Commit().Data;
            string changedHash = Hash(changed);
            var updatedCatalog = CatalogDocument.ParseOwned(changed);
            Check(!updatedCatalog.HasErrors, "Shared mutation preserves a valid object catalog.");
            Check(updatedCatalog.Objects.Count == catalog.Objects.Count, "NULL reassignment does not create or discard a saved resource.");
            var updatedOwner = updatedCatalog.Objects.Single(entry => entry.Id == owner.Id);
            var updatedTexture = updatedCatalog.Objects.Single(entry => entry.Id == textureEntry.Id);
            Check(updatedTexture.TypeHash == textureEntry.TypeHash && updatedTexture.SerializedSize == textureEntry.SerializedSize,
                "The original saved TextureData identity and size remain present.");
            Check(catalog.Data.Span.Slice(checked((int)textureEntry.PhysicalOffset), checked((int)textureEntry.SerializedSize))
                .SequenceEqual(updatedCatalog.Data.Span.Slice(checked((int)updatedTexture.PhysicalOffset), checked((int)updatedTexture.SerializedSize))),
                "The complete saved TextureData bytes remain exact.");
            var updatedFields = SmoObjectFieldReader.Read(updatedCatalog, updatedOwner);
            var textureFields = updatedFields.Where(field => field.FieldType == 10).ToArray();
            Check(textureFields.Length == fields.Count(field => field.FieldType == 10) + 1, "One generic field10 was appended after the previous assignment.");
            var last = textureFields.Last();
            Check(last.PayloadSize == 4 && last.Payload.Span.SequenceEqual(new byte[4])
                && last.AbsoluteHeaderOffset >= updatedTexture.PhysicalEnd,
                "The final field10 contains the four-byte NULL reference after saved TextureData.");
            bool inspected = SmoMaterialInspection.TryInspect(updatedCatalog, updatedOwner, out var native, out string error);
            Check(inspected, $"Shared material reader accepts the repeated NULL assignment: {error}");
            var layer = native?.Passes.Single(pass => pass.Index + 1 == material.PassIndex)
                .Layers.Single(item => item.Index + 1 == material.LayerIndex);
            Check(layer?.TextureReference is { } reference && reference.AbsolutePayloadOffset == last.AbsolutePayloadOffset
                && reference.Size == last.PayloadSize, "Native final observed reference is the appended NULL field, not the saved inline texture.");
            var document = TextureDocument.Parse(changed);
            var after = document.Textures.Single(texture => updatedCatalog.Objects[texture.ObjectIndex].Id == textureEntry.Id);
            Check(after.Material is null, "Saved TextureData is not falsely bound to a superseded material reference.");
            Check(after.MaterialIssue?.StartsWith("MATERIAL_BINDING_NOT_CURRENT", StringComparison.Ordinal) == true,
                "The missing current binding is explicit.");
            Check(after.CanReplace == before.CanReplace && after.CanResize == before.CanResize
                && after.ReplacementIssue == before.ReplacementIssue,
                "Material binding diagnostics do not invent texture replacement restrictions.");
            Check(after.Width == before.Width && after.Height == before.Height && after.Layout == before.Layout
                && after.PixelDataSize == before.PixelDataSize && after.MipLevelCount == before.MipLevelCount,
                "The retained texture still exposes the same pixel metadata.");
            Check(Hash(changed) == changedHash, "Inspection preserves the synthetic input bytes.");
            observation = new { owner_id = owner.Id, texture_id = textureEntry.Id, changed_sha256 = changedHash,
                last_field_payload_offset = last.AbsolutePayloadOffset, last_field_payload_size = last.PayloadSize,
                native_layer = layer, material_is_null = after.Material is null, after.MaterialIssue,
                before_can_replace = before.CanReplace, after_can_replace = after.CanReplace,
                before_replacement_issue = before.ReplacementIssue, after_replacement_issue = after.ReplacementIssue };
        }
        catch (Exception error) { Check(false, $"{error.GetType().Name}: {error.Message}"); }
        Check(Hash(source) == beforeHash && HashFile(sourcePath) == beforeHash, "Original file and input array remain byte-identical.");
        string nativePath = Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll");
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(new
        {
            status = failures.Count == 0 ? "passed" : "failed", checks, failures, sourcePath,
            source_sha256_before = beforeHash, source_sha256_after = HashFile(sourcePath), observation,
            texture_tool_assembly_sha256 = HashFile(typeof(TextureDocument).Assembly.Location),
            viewer_core_assembly_sha256 = HashFile(typeof(SmoMaterialInspection).Assembly.Location),
            test_assembly_sha256 = HashFile(typeof(MaterialInspectionRegression).Assembly.Location),
            native_dll_sha256 = File.Exists(nativePath) ? HashFile(nativePath) : null,
            scope = "One repeated NULL texture assignment through the existing generic mutation API; no raw FAT fixture or replacement reader."
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"TextureTool material binding guards: {checks} checks, {failures.Count} failures.");
        return failures.Count == 0 ? 0 : 1;
    }
}
