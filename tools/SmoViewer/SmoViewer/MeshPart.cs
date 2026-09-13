using System;

public class MeshPart
{
    public string Name { get; set; } = "";
    public float[] Vertices { get; set; } = Array.Empty<float>();
    public uint[] Indices { get; set; } = Array.Empty<uint>();

    public int VertexCount => Vertices.Length / 3;
    public int IndexCount => Indices.Length;
}