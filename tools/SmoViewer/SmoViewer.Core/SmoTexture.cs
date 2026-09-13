namespace SmoViewer.Core;

public enum SmoTextureLayout
{
    Abgr,
    Bgra
}

public sealed record SmoTextureMip(int Width,int Height,ReadOnlyMemory<byte> Bgra32Pixels);

/// <summary>
/// A confirmed <c>spTextureData</c> pixel buffer normalized to BGRA32.
/// </summary>
public sealed class SmoTexture
{
    public static SmoTexture CreateTransient(
        string name,
        int width,
        int height,
        byte[] bgra32Pixels,
        int objectIndex = -1)
    {
        ArgumentNullException.ThrowIfNull(bgra32Pixels);
        if (width < 1 || height < 1 ||
            bgra32Pixels.Length != checked(width * height * 4))
            throw new ArgumentException(
                "Transient texture dimensions do not match its BGRA32 payload.",
                nameof(bgra32Pixels));
        return new SmoTexture(
            objectIndex,
            name,
            0,
            width,
            height,
            SmoTextureLayout.Bgra,
            bgra32Pixels.ToArray(),
            SmoTextureRepresentationKind.CrossPlatformBgra32,
            1,
            null);
    }

    internal SmoTexture(
        int objectIndex,
        string name,
        ushort formatCode,
        int width,
        int height,
        SmoTextureLayout sourceLayout,
        byte[] bgra32Pixels,
        SmoTextureRepresentationKind representationKind =
            SmoTextureRepresentationKind.Direct3DBgra32,
        int mipLevelCount = 1,
        uint? platformType = null,
        IReadOnlyList<SmoTextureMip>? runtimeMipLevels = null)
    {
        ObjectIndex = objectIndex;
        Name = name;
        FormatCode = formatCode;
        Width = width;
        Height = height;
        SourceLayout = sourceLayout;
        Bgra32Pixels = bgra32Pixels;
        RepresentationKind = representationKind;
        MipLevelCount = mipLevelCount;
        PlatformType = platformType;
        HasRuntimeMipChain=runtimeMipLevels is not null;
        if(runtimeMipLevels is not null)
        {
            if(runtimeMipLevels.Count!=mipLevelCount||mipLevelCount is <1 or >16)
                throw new ArgumentException("Runtime mip count is inconsistent.",nameof(runtimeMipLevels));
            int w=width,h=height;
            foreach(var mip in runtimeMipLevels)
            {
                if(mip.Width!=w||mip.Height!=h||mip.Bgra32Pixels.Length!=checked(w*h*4))
                    throw new ArgumentException("Runtime mip dimensions/pixels are inconsistent.",nameof(runtimeMipLevels));
                w=Math.Max(1,w/2);h=Math.Max(1,h/2);
            }
            if(!runtimeMipLevels[0].Bgra32Pixels.Equals(Bgra32Pixels))
                throw new ArgumentException("Base pixels must share the first runtime mip buffer.",nameof(runtimeMipLevels));
            MipLevels=Array.AsReadOnly(runtimeMipLevels.ToArray());
        }
        else MipLevels=Array.AsReadOnly(new[]{new SmoTextureMip(width,height,Bgra32Pixels)});
    }

    public int ObjectIndex { get; }
    public string Name { get; }
    /// <summary>
    /// Legacy diagnostic signature made from the first two bytes of the
    /// direct field header. Its high byte is part of a payload length and is
    /// not a texture pixel-format code.
    /// </summary>
    public ushort FormatCode { get; }
    public int Width { get; }
    public int Height { get; }
    public SmoTextureLayout SourceLayout { get; }
    public SmoTextureRepresentationKind RepresentationKind { get; }
    public int MipLevelCount { get; }
    public uint? PlatformType { get; }
    public ReadOnlyMemory<byte> Bgra32Pixels { get; }
    /// <summary>Available CPU pixels. Metadata-only decoders may expose just
    /// the base level even when MipLevelCount describes more serialized levels.</summary>
    public IReadOnlyList<SmoTextureMip> MipLevels {get;}
    public bool HasRuntimeMipChain {get;}
    public int PixelStride => checked(Width * 4);
}
