using System.Collections.ObjectModel;

namespace SmoViewer.Core;

public sealed record SmoVertexLayout(
    uint Format,
    int SerializedStride,
    int? DiffuseArgbOffset,
    int? TextureCoordinate0Offset,
    int? BlendWeightsOffset = null,
    int? BlendIndicesOffset = null,
    int? NormalOffset = null,
    int? TextureCoordinate1Offset = null);

/// <summary>Editor presets whose byte offsets come from the original component-layout class.</summary>
public static class SmoVertexLayoutRegistry
{
    private static readonly IReadOnlyDictionary<uint, SmoVertexLayout> KnownLayouts =
        new ReadOnlyDictionary<uint, SmoVertexLayout>(new uint[]
        { 0x0000, 0x0040, 0x0800, 0x0840, 0x0100, 0x0140, 0x0900,
          0x093E, 0x0940, 0x097E, 0x1900, 0x1940, 0x197E }
            .ToDictionary(format => format, ReadLayout));

    private static SmoVertexLayout ReadLayout(uint format)
    {
        SmoViewer.Sparkplug.NativeMethods.Check(
            SmoViewer.Sparkplug.NativeMethods.spv_vertex_layout(format, out var native));
        static int? Optional(int value) => value < 0 ? null : value;
        return new SmoVertexLayout(format, checked((int)native.Stride),
            Optional(native.Color), Optional(native.Uv0), Optional(native.Weights),
            Optional(native.Bones), Optional(native.Normal), Optional(native.Uv1));
    }
    public static IReadOnlyDictionary<uint, SmoVertexLayout> All => KnownLayouts;
    public static bool TryGet(uint format, out SmoVertexLayout? layout) =>
        KnownLayouts.TryGetValue(format, out layout);
}
