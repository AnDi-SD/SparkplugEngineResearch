namespace SMOTextureTool.Core;

public enum TextureLayout
{
    Abgr,
    Bgra
}

public enum TextureContentKind
{
    Color,
    Monochrome
}

public sealed record TextureInfo(
    int Index,
    int BlockOffset,
    int PixelDataOffset,
    int Width,
    int Height,
    ushort FormatCode,
    TextureLayout Layout,
    TextureChannelInfo Channels)
{
    public const int MaximumCurrentHeaderDimension = 16384;

    public int PixelDataSize => checked(Width * Height * 4);
    public bool CanResize => true;
    public MaterialReferenceInfo? Material { get; init; }
    public TextureContentKind ContentKind => Channels.RgbChannelsIdentical
        ? TextureContentKind.Monochrome
        : TextureContentKind.Color;
    public bool HasVariableAlpha => Channels.AlphaMin != Channels.AlphaMax;
    public bool HasTransparency => Channels.AlphaMin < 255;

    public string FileName =>
        $"tex_{Index:D3}_{Width}x{Height}_fmt{FormatCode:X4}.png";
}

public sealed record MaterialReferenceInfo(
    int Index,
    int BlockOffset,
    int TextureContainerOffset,
    uint TextureContainerSize,
    int PassIndex,
    int LayerIndex,
    uint LayerClassId,
    string LayerClassName,
    uint FinalBlendOperation,
    IReadOnlyList<uint> MaterialRenderStates,
    IReadOnlyList<uint> LayerTextureStates)
{
    public uint UnknownLayerState0 => LayerTextureStates[0];
    public uint ColorOperation => LayerTextureStates[1];
    public uint AlphaOperation => LayerTextureStates[2];
    public uint AddressU => LayerTextureStates[3];
    public uint AddressV => LayerTextureStates[4];
    public uint BorderColor => LayerTextureStates[5];
    public uint Filter => LayerTextureStates[6];
    public uint TextureCoordinateIndex => LayerTextureStates[7];
    public uint TextureTransformFlags => LayerTextureStates[8];
    public bool UsesColorModulation => ColorOperation == 3;
}

public sealed record TextureChannelInfo(
    byte RedMin,
    byte RedMax,
    byte GreenMin,
    byte GreenMax,
    byte BlueMin,
    byte BlueMax,
    byte AlphaMin,
    byte AlphaMax,
    int AlphaValueCount,
    bool RgbChannelsIdentical);

public sealed record VertexColorBindingInfo(
    int ModelOffset,
    int MeshOffset,
    int VertexCount,
    int TriangleCount,
    IReadOnlyList<int> InfluencingVertexIndices,
    int OverlappingPixelWrites,
    int ConflictingPixelWrites);
