using System.Numerics;

namespace SmoViewer.Core;

public readonly record struct SmoBlendIndices(byte X, byte Y, byte Z, byte W);

/// <summary>
/// A decoded <c>spMeshData</c> object. The current decoder deliberately exposes
/// only the fields whose binary layout is confirmed.
/// </summary>
public sealed class SmoMesh
{
    public static SmoMesh CreateTransient(
        SmoMesh template,
        Vector3[] positions,
        Vector3[] normals,
        Vector2[] textureCoordinates,
        uint[] diffuseColorsArgb,
        Vector4[] blendWeights,
        SmoBlendIndices[] blendIndices,
        uint[] triangleIndices,
        int? objectIndex = null) => CreateTransient(
            template,
            positions,
            normals,
            textureCoordinates,
            [],
            diffuseColorsArgb,
            blendWeights,
            blendIndices,
            triangleIndices,
            objectIndex);

    public static SmoMesh CreateTransient(
        SmoMesh template,
        Vector3[] positions,
        Vector3[] normals,
        Vector2[] textureCoordinates,
        Vector2[] textureCoordinates1,
        uint[] diffuseColorsArgb,
        Vector4[] blendWeights,
        SmoBlendIndices[] blendIndices,
        uint[] triangleIndices,
        int? objectIndex = null)
    {
        ArgumentNullException.ThrowIfNull(template);
        ArgumentNullException.ThrowIfNull(positions);
        ArgumentNullException.ThrowIfNull(normals);
        ArgumentNullException.ThrowIfNull(textureCoordinates);
        ArgumentNullException.ThrowIfNull(textureCoordinates1);
        ArgumentNullException.ThrowIfNull(diffuseColorsArgb);
        ArgumentNullException.ThrowIfNull(blendWeights);
        ArgumentNullException.ThrowIfNull(blendIndices);
        ArgumentNullException.ThrowIfNull(triangleIndices);
        if (triangleIndices.Length % 3 != 0 ||
            triangleIndices.Any(index => index >= positions.Length))
        {
            throw new ArgumentException(
                "Transient mesh triangle indices are invalid.",
                nameof(triangleIndices));
        }
        ushort[] storedIndices = triangleIndices
            .Select(index => checked((ushort)index))
            .ToArray();
        return new SmoMesh(
            objectIndex ?? template.ObjectIndex,
            template.Name,
            template.Marker,
            SmoMeshDecoder.TriangleListPrimitive,
            template.VertexFormat,
            template.Stride,
            template.RuntimeStride,
            0,
            checked((uint)(positions.Length * template.RuntimeStride)),
            -1,
            -1,
            -1,
            -1,
            positions,
            normals,
            textureCoordinates,
            textureCoordinates1,
            diffuseColorsArgb,
            blendWeights,
            blendIndices,
            storedIndices,
            triangleIndices) { PrimitiveCount = checked((uint)(triangleIndices.Length / 3)) };
    }

    internal SmoMesh(
        int objectIndex,
        string name,
        byte marker,
        uint primitiveType,
        uint vertexFormat,
        int stride,
        int runtimeStride,
        int indexTrailerSize,
        uint runtimeVertexBufferSize,
        long physicalOffset,
        long physicalEnd,
        long indexDataOffset,
        long vertexDataOffset,
        Vector3[] positions,
        Vector3[] normals,
        Vector2[] textureCoordinates,
        Vector2[] textureCoordinates1,
        uint[] diffuseColorsArgb,
        Vector4[] blendWeights,
        SmoBlendIndices[] blendIndices,
        ushort[] stripIndices,
        uint[] triangleIndices)
    {
        ObjectIndex = objectIndex;
        Name = name;
        Marker = marker;
        PrimitiveType = primitiveType;
        VertexFormat = vertexFormat;
        Stride = stride;
        RuntimeStride = runtimeStride;
        IndexTrailerSize = indexTrailerSize;
        RuntimeVertexBufferSize = runtimeVertexBufferSize;
        PhysicalOffset = physicalOffset;
        PhysicalEnd = physicalEnd;
        IndexDataOffset = indexDataOffset;
        VertexDataOffset = vertexDataOffset;
        Positions = positions;
        Normals = normals;
        TextureCoordinates = textureCoordinates;
        TextureCoordinates1 = textureCoordinates1;
        DiffuseColorsArgb = diffuseColorsArgb;
        BlendWeights = blendWeights;
        BlendIndices = blendIndices;
        StripIndices = stripIndices;
        TriangleIndices = triangleIndices;
    }

    public int ObjectIndex { get; }
    public string Name { get; }

    /// <summary>The outer Sparkplug field marker, currently E0 or E1.</summary>
    public byte Marker { get; }

    /// <summary>The stored primitive kind. Confirmed triangle strips use value 3.</summary>
    public uint PrimitiveType { get; }
    /// <summary>Original index-buffer primitive count, or the generated host triangle count.</summary>
    public uint PrimitiveCount { get; internal init; }

    public uint VertexFormat { get; }

    /// <summary>
    /// Size of one serialized vertex from the original component-layout class.
    /// </summary>
    public int Stride { get; }

    /// <summary>
    /// Size of one prepared PC vertex. Original packed-index expansion can
    /// increase this over Stride for either serialized representation.
    /// </summary>
    public int RuntimeStride { get; }

    /// <summary>
    /// Non-index bytes between the complete stored strip and the vertex-buffer
    /// header. Some E0 files contain a four-byte 0xCD padding word. When the
    /// corresponding four bytes are two final strip indices they are included
    /// in <see cref="StripIndices"/> and this property is zero.
    /// </summary>
    public int IndexTrailerSize { get; }

    public uint RuntimeVertexBufferSize { get; }
    public int SerializedVertexBufferSize => checked(VertexCount * Stride);

    public long PhysicalOffset { get; }
    public long PhysicalEnd { get; }
    public long IndexDataOffset { get; }
    public long VertexDataOffset { get; }

    /// <summary>
    /// Vertex positions decoded from the confirmed position offset zero.
    /// </summary>
    public Vector3[] Positions { get; }

    /// <summary>Stored per-vertex normals for exactly confirmed layouts.</summary>
    public Vector3[] Normals { get; }

    /// <summary>
    /// First UV channel for an exactly confirmed serialized vertex layout.
    /// Unknown layouts expose an empty array rather than guessed offsets.
    /// </summary>
    public Vector2[] TextureCoordinates { get; }

    /// <summary>Second UV channel used by confirmed 0x1xxx multi-stage layouts.</summary>
    public Vector2[] TextureCoordinates1 { get; }

    /// <summary>
    /// Stored ARGB diffuse colors for an exactly confirmed vertex layout.
    /// The OpenGL renderer passes these values as an interpolated vertex input.
    /// </summary>
    public uint[] DiffuseColorsArgb { get; }

    /// <summary>Four stored blend weights for confirmed skinned layouts.</summary>
    public Vector4[] BlendWeights { get; }

    /// <summary>Four raw packed blend-index bytes; palette mapping is format-specific.</summary>
    public SmoBlendIndices[] BlendIndices { get; }

    public bool HasTextureCoordinates =>
        VertexCount > 0 && TextureCoordinates.Length == VertexCount;
    public bool HasTextureCoordinates1 =>
        VertexCount > 0 && TextureCoordinates1.Length == VertexCount;
    public bool HasNormals => VertexCount > 0 && Normals.Length == VertexCount;
    public bool HasDiffuseColors =>
        VertexCount > 0 && DiffuseColorsArgb.Length == VertexCount;
    public bool HasSkinningData =>
        VertexCount > 0 && BlendWeights.Length == VertexCount &&
        BlendIndices.Length == VertexCount;

    /// <summary>The original triangle-strip indices.</summary>
    public ushort[] StripIndices { get; }

    /// <summary>
    /// Non-degenerate triangles with strip winding converted to an explicit
    /// triangle list.
    /// </summary>
    public uint[] TriangleIndices { get; }

    public int VertexCount => Positions.Length;
    public int IndexCount => StripIndices.Length;
    public int TriangleCount => TriangleIndices.Length / 3;
}
