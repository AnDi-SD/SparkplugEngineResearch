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

/// <summary>
/// Vertex attributes confirmed independently by the corpus and texture-tool
/// experiments. Unknown layouts remain position-only.
/// </summary>
public static class SmoVertexLayoutRegistry
{
    private static readonly IReadOnlyDictionary<uint, SmoVertexLayout> KnownLayouts =
        new ReadOnlyDictionary<uint, SmoVertexLayout>(
            new Dictionary<uint, SmoVertexLayout>
            {
                [0x0800] = new(
                    Format: 0x0800,
                    SerializedStride: 20,
                    DiffuseArgbOffset: null,
                    TextureCoordinate0Offset: 12),
                [0x0840] = new(
                    Format: 0x0840,
                    SerializedStride: 32,
                    DiffuseArgbOffset: null,
                    TextureCoordinate0Offset: 24,
                    NormalOffset: 12),
                [0x0100] = new(
                    Format: 0x0100,
                    SerializedStride: 16,
                    DiffuseArgbOffset: 12,
                    TextureCoordinate0Offset: null),
                [0x0900] = new(
                    Format: 0x0900,
                    SerializedStride: 24,
                    DiffuseArgbOffset: 12,
                    TextureCoordinate0Offset: 16),
                [0x093E] = new(
                    Format: 0x093E,
                    SerializedStride: 44,
                    DiffuseArgbOffset: 32,
                    TextureCoordinate0Offset: 36,
                    BlendWeightsOffset: 12,
                    BlendIndicesOffset: 28),
                [0x0940] = new(
                    Format: 0x0940,
                    SerializedStride: 36,
                    DiffuseArgbOffset: 24,
                    TextureCoordinate0Offset: 28,
                    NormalOffset: 12),
                [0x097E] = new(
                    Format: 0x097E,
                    SerializedStride: 56,
                    DiffuseArgbOffset: 44,
                    TextureCoordinate0Offset: 48,
                    BlendWeightsOffset: 12,
                    BlendIndicesOffset: 28,
                    NormalOffset: 32),
                [0x1900] = new(
                    Format: 0x1900,
                    SerializedStride: 32,
                    DiffuseArgbOffset: 12,
                    TextureCoordinate0Offset: 16,
                    TextureCoordinate1Offset: 24),
                [0x1940] = new(
                    Format: 0x1940,
                    SerializedStride: 44,
                    DiffuseArgbOffset: 24,
                    TextureCoordinate0Offset: 28,
                    NormalOffset: 12,
                    TextureCoordinate1Offset: 36),
                [0x197E] = new(
                    Format: 0x197E,
                    SerializedStride: 64,
                    DiffuseArgbOffset: 44,
                    TextureCoordinate0Offset: 48,
                    BlendWeightsOffset: 12,
                    BlendIndicesOffset: 28,
                    NormalOffset: 32,
                    TextureCoordinate1Offset: 56)
            });

    public static IReadOnlyDictionary<uint, SmoVertexLayout> All => KnownLayouts;

    public static bool TryGet(uint format, out SmoVertexLayout? layout) =>
        KnownLayouts.TryGetValue(format, out layout);
}
