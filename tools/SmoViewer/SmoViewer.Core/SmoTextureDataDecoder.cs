using System.Buffers.Binary;
using System.Diagnostics.CodeAnalysis;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

public enum SmoTextureSourceKind
{
    LegacyCrossPlatform,
    Embedded,
    SourceNone
}

public enum SmoTextureRepresentationKind
{
    CrossPlatformBgra32,
    Direct3DBgra32,
    Ps2Indexed4,
    Ps2Indexed8,
    Ps2Rgba32,
    CrossPlatformRaw,
    Direct3DDxt1,
    Direct3DDxt3,
    Direct3DDxt5,
    CrossPlatformBgrx32,
    /// <summary>BGRA upload projection of the actual loaded PC texture.</summary>
    RuntimeBgra32,
    Ps2Raw
}

public sealed record SmoTextureMipLevelData(
    int Index,
    int Width,
    int Height,
    uint Descriptor0,
    uint Descriptor1,
    uint Descriptor2,
    ReadOnlyMemory<byte> PixelData)
{
    /// <summary>False for PS2 raw mip records: Width/Height are zero placeholders,
    /// not inferred dimensions. Descriptor0/1/2 preserve the exact wire words.</summary>
    public bool DimensionsKnown { get; internal init; } = true;
}

public sealed record SmoTextureRepresentationData(
    SmoTextureRepresentationKind Kind,
    int Width,
    int Height,
    uint FormatValue,
    uint AuxiliaryValue,
    uint BitsPerPixel,
    bool PixelDataPresent,
    ReadOnlyMemory<byte> Palette,
    IReadOnlyList<SmoTextureMipLevelData> MipLevels)
{
    /// <summary>Optional BGRA projection by the reconstructed codec; original mip bytes stay in MipLevels.</summary>
    public ReadOnlyMemory<byte> BgraPreview { get; internal init; }
    /// <summary>Observed original PC prefix byte, kept exactly for editing; its full meaning is not inferred.</summary>
    public byte NativeField1C { get; internal init; }
    /// <summary>Raw PC/PS2 native prefix byte, when observed; no shared meaning is inferred.</summary>
    public byte? NativeDataFlag { get; internal init; }
}

public sealed record SmoTextureDataInfo(
    SmoTextureSourceKind SourceKind,
    uint? PlatformType,
    SmoTextureRepresentationData? CrossPlatform,
    SmoTextureRepresentationData? PlatformSpecific)
{
    /// <summary>Representation exposed for preview. UsesRuntimeSourceSelection
    /// distinguishes actual PC selection from legacy/PS2 metadata projection.
    /// The two positional properties are metadata summaries, not priority rules.</summary>
    public SmoTextureRepresentationData? SelectedRepresentation { get; internal init; }
    public bool UsesRuntimeSourceSelection { get; internal init; }
    /// <summary>Successful stored representation reads in execution order.
    /// Skipped fields have no invented decoded representation.</summary>
    public IReadOnlyList<SmoTextureRepresentationData> ObservedRepresentations { get; internal init; } = [];
    public bool HasOpaqueRepresentations { get; internal init; }
    /// <summary>Host restriction for the existing lossless editor operation.</summary>
    public string? EditingIssue { get; internal init; } = "Texture source editing path has not been verified.";
    public uint? RuntimeMipLevelCount { get; internal init; }
    public int MipLevelCount => Math.Max(
        CrossPlatform?.MipLevels.Count ?? 0,
        PlatformSpecific?.MipLevels.Count ?? 0);
}

/// <summary>
/// Metadata adapter for texture representations. Cross pixels and PC mip
/// records use the reconstructed native readers. PC source selection executes
/// the actual shared reader. Legacy-common and PS2 source metadata remain
/// explicit inspector boundaries below; they do not claim a runtime load.
/// </summary>
public static class SmoTextureDataDecoder
{
    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoTextureDataInfo? data,
        out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        data = null;
        error = string.Empty;

        if (entry.TypeHash != SmoClassIds.TextureData)
            return Fail(out data, out error, "NOT_TEXTURE_DATA",
                $"Object [{entry.Index}] is not spTextureData.");
        if ((document.Header.PlatformMask & 2) != 0)
            return TryDecodePcSource(document, entry, out data, out error);
        if (!SmoObjectFieldReader.TryRead(
                document, entry, out IReadOnlyList<SmoObjectField>? fields,
                out string fieldError))
        {
            return Fail(out data, out error, "TEXTURE_FIELD_STREAM_INVALID",
                fieldError);
        }
        if (fields.Count < 2 || !IsTerminator(fields[^1]) ||
            fields.Take(fields.Count - 1).Any(IsTerminator))
        {
            return Fail(out data, out error, "TEXTURE_TOP_LEVEL_SHAPE_INVALID",
                $"Object [{entry.Index}] has an unexpected direct field shape.");
        }

        IReadOnlyList<SmoObjectField> values = fields.Take(fields.Count - 1).ToArray();
        uint? platformType = null;
        int valueIndex = 0;
        if (values[0].FieldType == 6)
        {
            if (values[0].PayloadSize != sizeof(uint))
                return Fail(out data, out error, "TEXTURE_PLATFORM_TYPE_INVALID",
                    "The platform-type field is not a UInt32.");
            platformType = BinaryPrimitives.ReadUInt32LittleEndian(
                values[0].Payload.Span);
            valueIndex++;
        }
        if (valueIndex != values.Count - 1)
            return Fail(out data, out error, "TEXTURE_TOP_LEVEL_SHAPE_INVALID",
                "The texture contains more than one source field.");

        SmoObjectField source = values[valueIndex];
        if (source.FieldType == 0)
        {
            if (!TryParseCrossPlatform(
                    source.Payload, out SmoTextureRepresentationData? cross,
                    out error))
            {
                data = null;
                return false;
            }
            data = new SmoTextureDataInfo(
                SmoTextureSourceKind.LegacyCrossPlatform, platformType, cross, null)
                { SelectedRepresentation = cross, ObservedRepresentations = Array.AsReadOnly(new[] { cross }) };
            return true;
        }
        if (source.FieldType != 3)
            return Fail(out data, out error, "TEXTURE_SOURCE_UNSUPPORTED",
                $"Source field {source.FieldType} is not present in the validated corpora.");

        return TryParseEmbedded(source.Payload, (document.Header.PlatformMask & 8) != 0, out data, out error);
    }

    private static bool TryDecodePcSource(SmoDocument document, SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoTextureDataInfo? data, out string error)
    {
        data = null;
        if (!SmoTextureSourceInspection.TryRead(document, entry, out var source, out error)) return false;
        if (source.Representations.Length == 0)
            return Fail(out data, out error, "TEXTURE_REPRESENTATION_MISSING", "The PC reader produced no stored representation.");
        var payload = document.Data.Slice(checked((int)entry.PhysicalOffset + 8), checked((int)entry.SerializedSize - 8));
        var decoded = new List<SmoTextureRepresentationData>();
        SmoTextureRepresentationData? cross = null, native = null;
        foreach (var representation in source.Representations)
        {
            var mips = source.Mips.AsSpan(checked((int)representation.FirstMip), checked((int)representation.Mips));
            var levels = new SmoTextureMipLevelData[mips.Length];
            for (int i = 0; i < levels.Length; ++i)
            {
                var mip = mips[i];
                levels[i] = new(i, checked((int)mip.Width), checked((int)mip.Height),
                    mip.Descriptor0, mip.Descriptor1, mip.Descriptor2,
                    payload.Slice(checked((int)mip.PixelOffset), checked((int)mip.PixelSize)));
            }
            var kind = RepresentationKind(representation.Kind, representation.Format);
            ReadOnlyMemory<byte> preview = default;
            if (kind == SmoTextureRepresentationKind.CrossPlatformBgrx32)
            {
                // Project this observation's pixels. Re-reading its entire
                // field would select the last field5 again when fields repeat.
                preview = ProjectXrgb(levels[0].PixelData.Span);
            }
            var value = new SmoTextureRepresentationData(kind, checked((int)representation.Width),
                checked((int)representation.Height), representation.Format, representation.Auxiliary,
                representation.BitsPerPixel, levels.Any(level => !level.PixelData.IsEmpty),
                ReadOnlyMemory<byte>.Empty, Array.AsReadOnly(levels))
                { NativeField1C = checked((byte)representation.Field1C), BgraPreview = preview,
                  NativeDataFlag = representation.Kind == 1 ? checked((byte)representation.NativeFlag) : null };
            decoded.Add(value);
            if (representation.Kind == 0) cross = value; else native = value;
        }
        var selectedField = source.Fields[checked((int)source.Representations[^1].FieldIndex)];
        var platform = source.Fields.Where(field => field.Scope == 1 && field.Outcome == 5 &&
            field.FrameOffset == selectedField.FrameOffset && field.Depth == selectedField.Depth).ToArray();
        var rootEnd = source.Fields.Last(field => field.Scope == 0 && field.Depth == 0 && field.Outcome == 8);
        bool opaque = source.Fields.Any(field => field.Outcome == 1 && field.PayloadSize != 0 && field.FieldId <= 1);
        data = new(rootEnd.HandledAfter != 0 ? SmoTextureSourceKind.Embedded : SmoTextureSourceKind.SourceNone,
            platform.Length != 0 ? platform[^1].PlatformAfter : null, cross, native)
        {
            SelectedRepresentation = decoded[^1], ObservedRepresentations = decoded.AsReadOnly(),
            UsesRuntimeSourceSelection = true,
            HasOpaqueRepresentations = opaque, RuntimeMipLevelCount = source.Info.RuntimeMips,
            EditingIssue = HasExistingEditorSourcePath(source) ? null :
                "Замена этой формы source-полей не проверена: данные доступны только для чтения."
        };
        return true;
    }

    private static bool HasExistingEditorSourcePath(SmoTextureSourceInspection.Snapshot source)
    {
        // This is the host editor's supported operation, not a second source
        // reader. Unknown/repeated/skipped forms remain readable, but do not
        // silently become writable through the old [3,1,0] replacement path.
        if (source.Representations.Length != 1 || source.Representations[0].Kind != 1) return false;
        var outer = source.Fields.Where(field => field.Depth == 0 && field.Scope == 0).ToArray();
        if (outer.Length != 2 || outer[0].Outcome != 3 || outer[1].Outcome != 8) return false;
        var nested = source.Fields.Where(field => field.Depth == 1 && field.FrameOffset == outer[0].PayloadOffset).ToArray();
        if (source.Fields.Length != nested.Length + 2) return false;
        var wrapper = nested.Where(field => field.Scope == 0).ToArray();
        if (wrapper.Length != 2 || wrapper[0].Outcome != 2 || wrapper[0].PayloadSize != 1 || wrapper[1].Outcome != 8) return false;
        var local = nested.Where(field => field.Scope == 1).ToArray();
        int index = local.Length != 0 && local[0].Outcome == 5 ? 1 : 0;
        return local.Length == index + 2 && local[index].Outcome == 7 && local[index + 1].Outcome == 8;
    }

    private static SmoTextureRepresentationKind RepresentationKind(uint kind, uint format) => kind == 0
        ? format == 0 ? SmoTextureRepresentationKind.CrossPlatformBgra32
            : format == 1 ? SmoTextureRepresentationKind.CrossPlatformBgrx32 : SmoTextureRepresentationKind.CrossPlatformRaw
        : format switch
        {
            0 => SmoTextureRepresentationKind.Direct3DBgra32,
            1 => SmoTextureRepresentationKind.Direct3DDxt1,
            2 => SmoTextureRepresentationKind.Direct3DDxt3,
            3 => SmoTextureRepresentationKind.Direct3DDxt5,
            _ => throw new InvalidDataException("Unsupported native texture format.")
        };

    private static unsafe byte[] ProjectXrgb(ReadOnlySpan<byte> pixels)
    {
        byte[] output = new byte[pixels.Length];
        fixed (byte* input = pixels)
        fixed (byte* destination = output)
            NativeMethods.Check(NativeMethods.spv_texture_xrgb_bgra(input, (uint)pixels.Length, destination, (uint)output.Length));
        return output;
    }

    public static bool TryParseCrossPlatform(ReadOnlyMemory<byte> payload,
        [NotNullWhen(true)] out SmoTextureRepresentationData? representation, out string error) =>
        TryReadCrossSection(payload, out representation, out error);

    public static string GetRepresentationName(SmoTextureRepresentationKind kind) =>
        kind switch
        {
            SmoTextureRepresentationKind.CrossPlatformBgra32 =>
                "cross-platform BGRA32",
            SmoTextureRepresentationKind.Direct3DBgra32 => "Direct3D BGRA32",
            SmoTextureRepresentationKind.Ps2Indexed4 => "PS2 indexed 4-bit",
            SmoTextureRepresentationKind.Ps2Indexed8 => "PS2 indexed 8-bit",
            SmoTextureRepresentationKind.Ps2Rgba32 => "PS2 32-bit",
            SmoTextureRepresentationKind.Ps2Raw => "PS2 raw metadata",
            SmoTextureRepresentationKind.CrossPlatformRaw => "cross-platform raw",
            SmoTextureRepresentationKind.Direct3DDxt1 => "Direct3D DXT1",
            SmoTextureRepresentationKind.Direct3DDxt3 => "Direct3D DXT3",
            SmoTextureRepresentationKind.Direct3DDxt5 => "Direct3D DXT5",
            SmoTextureRepresentationKind.CrossPlatformBgrx32 => "cross-platform XRGB",
            _ => kind.ToString()
        };

    private static bool TryParseEmbedded(
        ReadOnlyMemory<byte> payload,
        bool ps2,
        [NotNullWhen(true)] out SmoTextureDataInfo? data,
        out string error)
    {
        data = null;
        error = string.Empty;
        if (!TryReadNestedFields(payload, out List<NestedField>? fields, out error))
            return false;
        NestedField[] baseFields = fields.Where(item => item.Section == 0).ToArray();
        NestedField[] derived = fields.Where(item => item.Section == 1).ToArray();
        if (baseFields.Length != 2 || baseFields[0].FieldType != 2 ||
            baseFields[0].Payload.Length != 1 || baseFields[0].Payload.Span[0] != 0 ||
            !baseFields[1].IsTerminator || derived.Length < 2 ||
            !derived[^1].IsTerminator ||
            fields.Any(item => item.Section > 1))
        {
            return Fail(out data, out error, "TEXTURE_EMBEDDED_SHAPE_INVALID",
                "The embedded base/derived serializer sections are malformed.");
        }

        uint? platformType = null;
        SmoTextureRepresentationData? cross = null;
        SmoTextureRepresentationData? specific = null;
        int previousRank = -1;
        foreach (NestedField field in derived.Take(derived.Length - 1))
        {
            int rank = field.FieldType switch { 6 => 0, 0 => 1, 1 => 2, _ => -1 };
            if (rank < 0 || rank < previousRank)
                return Fail(out data, out error, "TEXTURE_DERIVED_FIELD_INVALID",
                    $"Unexpected derived texture field {field.FieldType}.");
            previousRank = rank;
            switch (field.FieldType)
            {
                case 6:
                    if (platformType.HasValue || field.Payload.Length != sizeof(uint))
                        return Fail(out data, out error,
                            "TEXTURE_PLATFORM_TYPE_INVALID",
                            "The embedded platform type is duplicated or malformed.");
                    platformType = ReadUInt32(field.Payload.Span, 0);
                    break;
                case 0:
                    if (cross is not null)
                        return Fail(out data, out error, "TEXTURE_METADATA_SHAPE_UNSUPPORTED", "Repeated cross fields require an ordered source metadata view.");
                    if (!TryParseCrossPlatform(
                            field.Payload, out cross, out error))
                    {
                        data = null;
                        return false;
                    }
                    break;
                case 1:
                    if (specific is not null)
                        return Fail(out data, out error, "TEXTURE_METADATA_SHAPE_UNSUPPORTED", "Repeated native fields require an ordered source metadata view.");
                    if (!TryParsePlatformSpecific(
                            field.Payload, ps2, out specific, out error))
                    {
                        data = null;
                        return false;
                    }
                    break;
            }
        }
        if (cross is null && specific is null)
            return Fail(out data, out error, "TEXTURE_REPRESENTATION_MISSING",
                "The embedded texture has no cross-platform or platform-specific data.");

        data = new SmoTextureDataInfo(
            SmoTextureSourceKind.Embedded, platformType, cross, specific)
            { SelectedRepresentation = cross ?? specific,
              ObservedRepresentations = Array.AsReadOnly(new[] { cross, specific }.OfType<SmoTextureRepresentationData>().ToArray()) };
        return true;
    }

    private static bool TryParsePlatformSpecific(
        ReadOnlyMemory<byte> payload,
        bool ps2,
        [NotNullWhen(true)] out SmoTextureRepresentationData? representation,
        out string error)
    {
        if (ps2)
            return TryParsePs2(payload, out representation, out error);
        representation = null;
        error = "TEXTURE_PLATFORM_SPECIFIC_INVALID: No confirmed metadata reader for this declared platform/source.";
        return false;
    }

    private static unsafe bool TryReadCrossSection(ReadOnlyMemory<byte> payload,
        [NotNullWhen(true)] out SmoTextureRepresentationData? representation, out string error)
    {
        representation = null; error = string.Empty;
        try
        {
            TextureSectionHandle owner;
            fixed (byte* input = payload.Span)
                owner = new TextureSectionHandle(NativeMethods.Check(
                    NativeMethods.spv_texture_section_read(input, checked((uint)payload.Length), 0)));
            using (owner)
            {
                NativeMethods.Check(NativeMethods.spv_texture_section_info(owner, out var info));
                var observed = new NativeMethods.TextureMip[checked((int)info.Mips)];
                fixed (NativeMethods.TextureMip* output = observed)
                    NativeMethods.Check(NativeMethods.spv_texture_section_mips(owner, output, info.Mips));
                var mips = observed.Select((mip, index) => new SmoTextureMipLevelData(
                    index, checked((int)mip.Width), checked((int)mip.Height),
                    mip.Descriptor0, mip.Descriptor1, mip.Descriptor2,
                    payload.Slice(checked((int)mip.PixelOffset), checked((int)mip.PixelSize)))).ToArray();
                // Display enum only. Byte layout/format validation and all
                // offsets come from the actual engine reader.
                var representationKind = RepresentationKind(0, info.Format);
                byte[] preview = [];
                if (representationKind == SmoTextureRepresentationKind.CrossPlatformBgrx32)
                {
                    preview = new byte[checked((int)(info.Width * info.Height * 4))];
                    fixed (byte* output = preview)
                        NativeMethods.Check(NativeMethods.spv_texture_section_bgra(owner, output, checked((uint)preview.Length)));
                }
                representation = new SmoTextureRepresentationData(representationKind,
                    checked((int)info.Width), checked((int)info.Height), info.Format, info.Auxiliary,
                    info.BitsPerPixel, info.PixelDataPresent != 0, ReadOnlyMemory<byte>.Empty, Array.AsReadOnly(mips))
                    { BgraPreview = preview };
                return true;
            }
        }
        catch (InvalidDataException exception) { error = exception.Message; return false; }
    }

    private static unsafe bool TryParsePs2(
        ReadOnlyMemory<byte> payload,
        [NotNullWhen(true)] out SmoTextureRepresentationData? representation,
        out string error)
    {
        representation = null; error = string.Empty;
        try
        {
            Ps2TextureHandle handle;
            fixed (byte* bytes = payload.Span)
                handle = new(NativeMethods.Check(NativeMethods.spv_ps2_texture_inspect(bytes, (uint)payload.Length)));
            using (handle)
            {
                NativeMethods.Check(NativeMethods.spv_ps2_texture_info(handle, out var info));
                // The common inspector preserves every image; this legacy DTO
                // exposes one image only and does not invent a PS2 assignment rule.
                if (info.Images != 1)
                {
                    error = "PS2_TEXTURE_METADATA_SHAPE_UNSUPPORTED: A single image is required by this metadata view.";
                    return false;
                }
                NativeMethods.Ps2TextureImage image;
                NativeMethods.Check(NativeMethods.spv_ps2_texture_images(handle, &image, 1));
                var nativeMips = new NativeMethods.Ps2TextureMip[checked((int)info.Mips)];
                fixed (NativeMethods.Ps2TextureMip* output = nativeMips)
                    NativeMethods.Check(NativeMethods.spv_ps2_texture_mips(handle, output, info.Mips));
                var mips = nativeMips.Select((mip, index) => new SmoTextureMipLevelData(index, 0, 0,
                    mip.Descriptor0, mip.Descriptor1, mip.Descriptor2,
                    payload.Slice(checked((int)mip.DataOffset), checked((int)mip.DataSize)))
                    { DimensionsKnown = false }).ToArray();
                var kind = image.Format switch
                {
                    0 => SmoTextureRepresentationKind.Ps2Indexed4,
                    1 => SmoTextureRepresentationKind.Ps2Indexed8,
                    3 => SmoTextureRepresentationKind.Ps2Rgba32,
                    _ => SmoTextureRepresentationKind.Ps2Raw
                };
                representation = new(kind, checked((int)image.Width), checked((int)image.Height),
                    image.Format, image.Auxiliary, image.Format switch { 0 => 4u, 1 => 8u, 3 => 32u, _ => 0u },
                    mips.Any(mip => !mip.PixelData.IsEmpty),
                    payload.Slice(checked((int)image.PaletteOffset), checked((int)image.PaletteSize)), Array.AsReadOnly(mips))
                    { NativeDataFlag = checked((byte)image.NativeFlag) };
                return true;
            }
        }
        catch (InvalidDataException exception) { error = exception.Message; return false; }
        catch (OverflowException) { error = "PS2_TEXTURE_METADATA_RANGE: Raw dimensions exceed the host display range."; return false; }
    }

    private static bool TryReadNestedFields(
        ReadOnlyMemory<byte> data,
        [NotNullWhen(true)] out List<NestedField>? fields,
        out string error)
    {
        fields = [];
        error = string.Empty;
        int offset = 0;
        int section = 0;
        while (offset < data.Length)
        {
            if (!SmoDataBlockReader.TryReadHeader(
                    data.Span, offset, out SmoDataBlockHeader header))
            {
                fields = null;
                error = $"TEXTURE_NESTED_FIELD_INVALID: unreadable header at 0x{offset:X}.";
                return false;
            }
            bool terminator = header.FieldType == 0 && header.PayloadSize == 0;
            fields.Add(new NestedField(
                section, header.FieldType, terminator,
                data.Slice(header.PayloadOffset, checked((int)header.PayloadSize))));
            int next = checked((int)header.PayloadEnd);
            if (next <= offset)
            {
                fields = null;
                error = "TEXTURE_NESTED_FIELD_INVALID: non-advancing field.";
                return false;
            }
            offset = next;
            if (terminator)
                section++;
        }
        return true;
    }

    private static bool IsTerminator(SmoObjectField field) =>
        field.FieldType == 0 && field.PayloadSize == 0;

    private static uint ReadUInt32(ReadOnlySpan<byte> data, int offset) =>
        BinaryPrimitives.ReadUInt32LittleEndian(data.Slice(offset, sizeof(uint)));

    private static bool Fail<T>(
        out T? value, out string error, string code, string message)
    {
        value = default;
        error = $"{code}: {message}";
        return false;
    }

    private sealed record NestedField(
        int Section,
        int FieldType,
        bool IsTerminator,
        ReadOnlyMemory<byte> Payload);
}
