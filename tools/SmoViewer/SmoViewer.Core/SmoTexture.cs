namespace SmoViewer.Core;

public enum SmoTextureLayout
{
    Abgr,
    Bgra
}

/// <summary>
/// A confirmed <c>spTextureData</c> pixel buffer normalized to BGRA32.
/// </summary>
public sealed class SmoTexture
{
    internal SmoTexture(
        int objectIndex,
        string name,
        ushort formatCode,
        int width,
        int height,
        SmoTextureLayout sourceLayout,
        byte[] bgra32Pixels)
    {
        ObjectIndex = objectIndex;
        Name = name;
        FormatCode = formatCode;
        Width = width;
        Height = height;
        SourceLayout = sourceLayout;
        Bgra32Pixels = bgra32Pixels;
    }

    public int ObjectIndex { get; }
    public string Name { get; }
    public ushort FormatCode { get; }
    public int Width { get; }
    public int Height { get; }
    public SmoTextureLayout SourceLayout { get; }
    public ReadOnlyMemory<byte> Bgra32Pixels { get; }
    public int PixelStride => checked(Width * 4);
}
