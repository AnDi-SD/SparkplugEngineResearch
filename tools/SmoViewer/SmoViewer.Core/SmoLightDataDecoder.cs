using System.Numerics;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

public enum SmoLightType : uint
{
    Directional = 0,
    Point = 1,
    Spot = 2,
    Ambient = 3
}

/// <summary>
/// Actual shared spLightData scalar state. ColorRgba is the normalized runtime
/// color, not the authored ARGB word. SerializedFieldMask describes physical
/// presence separately from the class constructor and reader defaults.
/// </summary>
public sealed record SmoLightData(
    uint Type,
    bool ProjectShadowVolume,
    Vector4 ColorRgba,
    bool AttenuationEnabled,
    float Intensity,
    float Range,
    float HotspotAngle,
    float FalloffAngle,
    bool Enabled,
    ushort SerializedFieldMask)
{
    public bool IsFieldSerialized(int fieldType) =>
        fieldType is >= 0 and <= 8 &&
        (SerializedFieldMask & (1 << fieldType)) != 0;
}

/// <summary>
/// Thin transport to the common light reader and actual spLightData class.
/// This inspects the own light section; it does not load inherited Node links
/// or attach a light to a scene. No C# light defaults or scalar codec is used.
/// </summary>
public static class SmoLightDataDecoder
{
    // Schema/display constants retained for existing research consumers.
    public const uint DefaultType = (uint)SmoLightType.Directional;
    public const float DefaultRange = 200.0f;
    private const int MaximumSectionBytes = 16 * 1024 * 1024;

    /// <summary>Inspect original document bytes directly, including reader-only header forms.</summary>
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,out SmoLightData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        if(entry.TypeHash!=SmoClassIds.LightData)
        {error="Object is not LightData.";return false;}
        if(!SmoObjectFieldReader.TryRead(document,entry,out var fields,out error))return false;
        int start=OwnSectionStart(fields);
        if(start<0){error="Light inspection requires a final section terminator.";return false;}
        int offset=fields[start].AbsoluteHeaderOffset;
        return TryDecodeOwnSection(document.Data.Span.Slice(offset,fields[^1].AbsoluteEnd-offset),out value,out error);
    }

    public static bool TryDecode(
        IReadOnlyList<SmoObjectField> fields,
        out SmoLightData? value) => TryDecode(fields, out value, out _);

    /// <summary>
    /// Compatibility transport for database field observations without source
    /// offsets. The existing shared DataBlock writer envelopes their payloads;
    /// unsupported lossless header forms fail with its explicit diagnostic.
    /// </summary>
    public static unsafe bool TryDecode(
        IReadOnlyList<SmoObjectField> fields,
        out SmoLightData? value,
        out string error)
    {
        ArgumentNullException.ThrowIfNull(fields);
        value = null;
        error = string.Empty;
        int start=OwnSectionStart(fields);
        if (start<0)
        {
            error = "Light inspection requires a final section terminator.";
            return false;
        }
        ushort mask = 0;
        try
        {
            using var section = new MemoryStream();
            byte* header = stackalloc byte[6];
            for (int index = start; index < fields.Count; ++index)
            {
                var field = fields[index];
                if (field.FieldType is < 0 or > byte.MaxValue ||
                    field.PayloadSize != field.Payload.Length ||
                    field.SizeKind != (SmoDataBlockSizeCode)(field.RawHeader >> 5) ||
                    field.PayloadSize > MaximumSectionBytes ||
                    section.Length + 6 + field.PayloadSize > MaximumSectionBytes)
                {
                    error = "Invalid bounded Light field observation.";
                    return false;
                }
                NativeMethods.Check(NativeMethods.spv_write_field_header(
                    (uint)field.FieldType, field.PayloadSize, (uint)field.SizeKind,
                    (field.RawHeader & 31) == 31 ? 1u : 0u, header, 6, out uint headerSize));
                section.Write(new ReadOnlySpan<byte>(header, checked((int)headerSize)));
                section.Write(field.Payload.Span);
                if (!IsSectionTerminator(field) && field.FieldType <= 8)
                    mask |= (ushort)(1 << field.FieldType);
            }
            return TryReadNative(section.GetBuffer().AsSpan(0, checked((int)section.Length)), mask, out value, out error);
        }
        catch (InvalidDataException exception)
        {
            error = exception.Message;
            return false;
        }
    }

    /// <summary>
    /// Direct own-section inspection preserves the source framing and supports
    /// reader-valid headers even when the original writer cannot express them.
    /// </summary>
    public static bool TryDecodeOwnSection(
        ReadOnlySpan<byte> section,
        out SmoLightData? value,
        out string error)
    {
        value = null;
        error = string.Empty;
        if (section.Length is 0 or > MaximumSectionBytes)
        {
            error = "Invalid bounded Light inspection section.";
            return false;
        }
        ushort mask = 0;
        // Generic physical field descriptors, not a second light reader.
        // The common reader below decides semantic validity and state.
        int offset = 0;
        while (offset < section.Length)
        {
            if (!SmoDataBlockReader.TryReadHeader(section, offset, out var header) ||
                header.PayloadEnd > section.Length)
            {
                error = "Unreadable Light inspection field extent.";
                return false;
            }
            if (header.SizeKind != SmoDataBlockSizeCode.Empty && header.FieldType <= 8)
                mask |= (ushort)(1 << header.FieldType);
            offset = checked((int)header.PayloadEnd);
        }
        return TryReadNative(section, mask, out value, out error);
    }

    private static unsafe bool TryReadNative(ReadOnlySpan<byte> section, ushort mask,
        out SmoLightData? value, out string error)
    {
        value = null;
        error = string.Empty;
        try
        {
            fixed (byte* bytes = section)
            {
                NativeMethods.Check(NativeMethods.spv_light_fields_read(bytes, checked((uint)section.Length), out var light));
                value = new(light.Type, light.ProjectShadow != 0, light.ColorRgba,
                    light.Attenuation != 0, light.Intensity, light.Range, light.Hotspot,
                    light.Falloff, light.Enabled != 0, mask);
                return true;
            }
        }
        catch (InvalidDataException exception)
        {
            error = exception.Message;
            return false;
        }
    }

    public static string GetTypeName(uint type) => type switch
    {
        (uint)SmoLightType.Directional => "directional",
        (uint)SmoLightType.Point => "point",
        (uint)SmoLightType.Spot => "spot",
        (uint)SmoLightType.Ambient => "ambient",
        _ => "unknown"
    };

    private static bool IsSectionTerminator(SmoObjectField field) =>
        field.SizeKind == SmoDataBlockSizeCode.Empty && field.PayloadSize == 0;

    private static int OwnSectionStart(IReadOnlyList<SmoObjectField> fields)
    {
        if(fields.Count is 0 or > 65536||!IsSectionTerminator(fields[^1]))return -1;
        int start=fields.Count-2;
        while(start>=0&&!IsSectionTerminator(fields[start]))--start;
        return start+1;
    }
}
