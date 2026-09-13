using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

public static class SmoLoader
{
    public static ModelData LoadFromFile(string smoPath)
    {
        List<MeshPart> parts = LoadMeshPartsFromFile(smoPath);

        if (parts.Count == 0)
            return LoadTestCube();

        return BuildCombinedModel(parts);
    }
    private const uint MeshTypeHash = 0x33C34CF0;

    public static List<MeshPart> LoadMeshPartsFromFile(string smoPath)
    {
        Console.WriteLine("========================================");
        Console.WriteLine("SMO LOAD REQUEST");
        Console.WriteLine("========================================");
        Console.WriteLine($"Requested path : {smoPath}");
        Console.WriteLine($"File exists    : {File.Exists(smoPath)}");
        Console.WriteLine();

        if (!File.Exists(smoPath))
            throw new FileNotFoundException($"SMO file not found: {smoPath}", smoPath);

        byte[] data = File.ReadAllBytes(smoPath);

        SmoHeader header = ParseHeader(data);
        List<SmoEntry> entries = ParseEntries(data, header);
        ComputeAbsoluteOffsetsAndSizes(entries, header);

        DumpHeader(header);
        DumpEntries(entries);

        return TryLoadAllMeshes(data, header, entries);
    }

    public static ModelData BuildCombinedModel(IEnumerable<MeshPart> meshParts)
    {
        List<float> allVertices = new();
        List<uint> allIndices = new();

        foreach (MeshPart part in meshParts)
        {
            uint baseVertex = (uint)(allVertices.Count / 3);

            allVertices.AddRange(part.Vertices);

            foreach (uint index in part.Indices)
                allIndices.Add(index + baseVertex);
        }

        ModelData model = new ModelData
        {
            Vertices = allVertices.ToArray(),
            Indices = allIndices.ToArray()
        };

        model.CenterAndScaleSafe();
        return model;
    }

    private static List<MeshPart> TryLoadAllMeshes(byte[] data, SmoHeader header, List<SmoEntry> entries)
    {
        Console.WriteLine("========================================");
        Console.WriteLine("STRUCTURED MESH PARSE");
        Console.WriteLine("========================================");

        List<SmoEntry> meshEntries = entries
            .Where(e => e.TypeHash == MeshTypeHash)
            .OrderBy(e => e.Index)
            .ToList();

        Console.WriteLine($"Mesh entries found: {meshEntries.Count}");
        Console.WriteLine();

        List<MeshPart> result = new();

        foreach (SmoEntry meshEntry in meshEntries)
        {
            Console.WriteLine("----------------------------------------");
            Console.WriteLine($"Mesh entry [{meshEntry.Index:D2}] ID={meshEntry.Id} Name=\"{meshEntry.Name}\"");
            Console.WriteLine($"TableAbs(SBOO) = 0x{meshEntry.AbsoluteOffset:X8}");
            Console.WriteLine($"BlockSize      = 0x{meshEntry.BlockSize:X} ({meshEntry.BlockSize})");

            int meshRawOffset = checked((int)header.DataStartOffset + (int)meshEntry.FieldA);
            Console.WriteLine($"MeshRawOffset   = 0x{meshRawOffset:X8}");
            Console.WriteLine();

            MeshPart? part = TryParseSingleMesh(data, meshRawOffset, meshEntry.Name);
            if (part == null)
            {
                Console.WriteLine("Mesh parse failed.");
                Console.WriteLine();
                continue;
            }

            Console.WriteLine($"Accepted mesh: vertices={part.VertexCount}, indices={part.IndexCount}");
            Console.WriteLine();

            result.Add(part);
        }

        return result;
    }

    private static MeshPart? TryParseSingleMesh(byte[] data, int meshRawOffset, string meshName)
    {
        if (meshRawOffset < 0 || meshRawOffset + 64 > data.Length)
            return null;

        uint typeHashAtStart = ReadUInt32LE(data, meshRawOffset + 0x00);
        string tag = Encoding.ASCII.GetString(data, meshRawOffset + 0x04, 4);

        Console.WriteLine($"StartTypeHash = 0x{typeHashAtStart:X8}");
        Console.WriteLine($"StartTag      = {tag}");
        Console.WriteLine();

        if (typeHashAtStart != MeshTypeHash || tag != "SBOO")
            return null;

        int p = meshRawOffset + 8;

        byte typeInfo = data[p];
        p += 1;

        Console.WriteLine($"typeInfo = 0x{typeInfo:X2} ({typeInfo})");

        if (typeInfo != 0xE1)
        {
            Console.WriteLine("Unsupported mesh typeInfo.");
            return null;
        }

        uint b0 = ReadUInt32LE(data, p + 0);
        uint b1 = ReadUInt32LE(data, p + 4);
        uint b2 = ReadUInt32LE(data, p + 8);
        uint b3 = ReadUInt32LE(data, p + 12);
        uint b4 = ReadUInt32LE(data, p + 16);
        p += 20;

        byte unknownByte = data[p];
        p += 1;

        uint b5 = ReadUInt32LE(data, p + 0);
        uint b6 = ReadUInt32LE(data, p + 4);
        uint b7 = ReadUInt32LE(data, p + 8);
        p += 12;

        Console.WriteLine("Header fields:");
        Console.WriteLine($"  b0 = 0x{b0:X8} ({b0})");
        Console.WriteLine($"  b1 = 0x{b1:X8} ({b1})");
        Console.WriteLine($"  b2 = 0x{b2:X8} ({b2})");
        Console.WriteLine($"  b3 = 0x{b3:X8} ({b3})");
        Console.WriteLine($"  b4 = 0x{b4:X8} ({b4})");
        Console.WriteLine($"  unknownByte = 0x{unknownByte:X2} ({unknownByte})");
        Console.WriteLine($"  b5 = 0x{b5:X8} ({b5})");
        Console.WriteLine($"  b6 = 0x{b6:X8} ({b6})");
        Console.WriteLine($"  b7 = 0x{b7:X8} ({b7})");
        Console.WriteLine();

        int vertexCount = checked((int)b2);
        int vertexDataSize = checked((int)b3);
        int faceSize = checked((int)b4);

        if (vertexCount <= 0 || vertexCount > 500000)
            return null;

        if (faceSize <= 0 || p + faceSize > data.Length)
            return null;

        byte[] faceBytes = new byte[faceSize];
        Buffer.BlockCopy(data, p, faceBytes, 0, faceSize);
        p += faceSize;

        uint strideWord2 = ReadUInt32LE(data, p + 0);
        uint vertexCount2 = ReadUInt32LE(data, p + 4);
        uint skipWord = ReadUInt32LE(data, p + 8);
        p += 12;

        Console.WriteLine("Post-face fields:");
        Console.WriteLine($"  strideWord2  = 0x{strideWord2:X8} ({strideWord2})");
        Console.WriteLine($"  vertexCount2 = 0x{vertexCount2:X8} ({vertexCount2})");
        Console.WriteLine($"  skipWord     = 0x{skipWord:X8} ({skipWord})");
        Console.WriteLine($"  vertexBufferOffset = 0x{p:X8}");
        Console.WriteLine();

        if ((int)vertexCount2 != vertexCount)
            return null;

        uint layoutKey = strideWord2 & 0x0FFF;
        int baseStride = GetBaseStride(layoutKey);

        int actualStride = 0;
        if (vertexCount > 0 && vertexDataSize % vertexCount == 0)
            actualStride = vertexDataSize / vertexCount;

        if (actualStride <= 0)
            actualStride = baseStride;

        Console.WriteLine($"layoutKey      = 0x{layoutKey:X} ({layoutKey})");
        Console.WriteLine($"baseStride     = {baseStride}");
        Console.WriteLine($"actualStride   = {actualStride}");
        Console.WriteLine($"vertexDataSize = {vertexDataSize}");
        Console.WriteLine();

        int vertexBufferOffset = p;
        if (vertexBufferOffset + vertexDataSize > data.Length)
            return null;

        PositionGuess? guess = FindBestPositionGuess(data, vertexBufferOffset, vertexCount, actualStride);
        if (guess == null)
            return null;

        Console.WriteLine($"Chosen positionOffset = {guess.PositionOffset}");
        Console.WriteLine($"Position score        = {guess.Score}");
        Console.WriteLine();

        float[] vertices = guess.Vertices;
        DumpVertexPreview(vertices, 8);

        ushort[] strip = ReadIndexStrip(faceBytes);
        uint[] triangles = BuildTrianglesFromStrip(strip);

        Console.WriteLine($"Strip index count    : {strip.Length}");
        Console.WriteLine($"Triangle index count : {triangles.Length}");
        DumpIndexTriples(strip, 12);

        bool[] validVertices = BuildValidVertexMask(vertices);
        uint[] filteredTriangles = FilterTrianglesByValidVertices(triangles, validVertices);

        Console.WriteLine($"Valid vertices       : {validVertices.Count(v => v)} / {validVertices.Length}");
        Console.WriteLine($"Filtered indices     : {filteredTriangles.Length}");
        Console.WriteLine();

        if (filteredTriangles.Length == 0)
            return null;

        return new MeshPart
        {
            Name = meshName,
            Vertices = vertices,
            Indices = filteredTriangles
        };
    }

    private static PositionGuess? FindBestPositionGuess(byte[] data, int vertexBufferOffset, int vertexCount, int stride)
    {
        PositionGuess? best = null;

        for (int posOffset = 0; posOffset <= stride - 12; posOffset += 4)
        {
            float[] verts = ReadVerticesXYZFloat(data, vertexBufferOffset, vertexCount, stride, posOffset);
            int score = ScoreVertices(verts);

            if (best == null || score > best.Score)
            {
                best = new PositionGuess
                {
                    PositionOffset = posOffset,
                    Vertices = verts,
                    Score = score
                };
            }
        }

        return best;
    }

    private static int ScoreVertices(float[] vertices)
    {
        int valid = 0;
        int huge = 0;
        int tiny = 0;

        float minX = 0, minY = 0, minZ = 0;
        float maxX = 0, maxY = 0, maxZ = 0;
        bool first = true;

        for (int i = 0; i < vertices.Length; i += 3)
        {
            float x = vertices[i + 0];
            float y = vertices[i + 1];
            float z = vertices[i + 2];

            if (!IsReasonableVertex(x, y, z))
            {
                huge++;
                continue;
            }

            valid++;

            if (MathF.Abs(x) < 0.00001f && MathF.Abs(y) < 0.00001f && MathF.Abs(z) < 0.00001f)
                tiny++;

            if (first)
            {
                minX = maxX = x;
                minY = maxY = y;
                minZ = maxZ = z;
                first = false;
            }
            else
            {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                if (z < minZ) minZ = z;
                if (z > maxZ) maxZ = z;
            }
        }

        if (valid == 0)
            return int.MinValue;

        float sizeX = maxX - minX;
        float sizeY = maxY - minY;
        float sizeZ = maxZ - minZ;

        int score = 0;
        score += valid * 10;
        score -= huge * 100;
        score -= tiny * 2;

        if (sizeX > 0.01f) score += 50;
        if (sizeY > 0.01f) score += 50;
        if (sizeZ > 0.01f) score += 50;

        if (sizeX > 10000 || sizeY > 10000 || sizeZ > 10000)
            score -= 1000;

        return score;
    }

    private static int GetBaseStride(uint layoutKey)
    {
        return layoutKey switch
        {
            2430 => 56,
            2368 => 36,
            2304 => 24,
            2048 => 12,
            _ => 0
        };
    }

    private static float[] ReadVerticesXYZFloat(byte[] data, int vertexBufferOffset, int vertexCount, int stride, int positionOffset)
    {
        float[] vertices = new float[vertexCount * 3];

        for (int i = 0; i < vertexCount; i++)
        {
            int src = vertexBufferOffset + i * stride + positionOffset;
            int dst = i * 3;

            vertices[dst + 0] = ReadSingleLE(data, src + 0);
            vertices[dst + 1] = ReadSingleLE(data, src + 4);
            vertices[dst + 2] = ReadSingleLE(data, src + 8);
        }

        return vertices;
    }

    private static ushort[] ReadIndexStrip(byte[] faceBytes)
    {
        int count = faceBytes.Length / 2;
        ushort[] result = new ushort[count];

        for (int i = 0; i < count; i++)
            result[i] = BitConverter.ToUInt16(faceBytes, i * 2);

        return result;
    }

    private static uint[] BuildTrianglesFromStrip(ushort[] strip)
    {
        List<uint> result = new();
        bool flip = false;

        for (int i = 0; i < strip.Length - 2; i++)
        {
            ushort a = strip[i + 0];
            ushort b = strip[i + 1];
            ushort c = strip[i + 2];

            if (a == b || b == c || a == c)
            {
                flip = false;
                continue;
            }

            if (!flip)
            {
                result.Add(a);
                result.Add(b);
                result.Add(c);
            }
            else
            {
                result.Add(b);
                result.Add(a);
                result.Add(c);
            }

            flip = !flip;
        }

        return result.ToArray();
    }

    private static bool[] BuildValidVertexMask(float[] vertices)
    {
        int vertexCount = vertices.Length / 3;
        bool[] valid = new bool[vertexCount];

        for (int i = 0; i < vertexCount; i++)
        {
            float x = vertices[i * 3 + 0];
            float y = vertices[i * 3 + 1];
            float z = vertices[i * 3 + 2];

            valid[i] = IsReasonableVertex(x, y, z);
        }

        return valid;
    }

    private static bool IsReasonableVertex(float x, float y, float z)
    {
        if (float.IsNaN(x) || float.IsNaN(y) || float.IsNaN(z))
            return false;

        if (float.IsInfinity(x) || float.IsInfinity(y) || float.IsInfinity(z))
            return false;

        if (MathF.Abs(x) > 100000f || MathF.Abs(y) > 100000f || MathF.Abs(z) > 100000f)
            return false;

        if (MathF.Abs(x) > 1e20f || MathF.Abs(y) > 1e20f || MathF.Abs(z) > 1e20f)
            return false;

        return true;
    }

    private static uint[] FilterTrianglesByValidVertices(uint[] indices, bool[] validVertices)
    {
        List<uint> result = new(indices.Length);

        for (int i = 0; i + 2 < indices.Length; i += 3)
        {
            uint a = indices[i + 0];
            uint b = indices[i + 1];
            uint c = indices[i + 2];

            if (a >= validVertices.Length || b >= validVertices.Length || c >= validVertices.Length)
                continue;

            if (!validVertices[a] || !validVertices[b] || !validVertices[c])
                continue;

            result.Add(a);
            result.Add(b);
            result.Add(c);
        }

        return result.ToArray();
    }

    private static SmoHeader ParseHeader(byte[] data)
    {
        if (data.Length < 0x20)
            throw new Exception("Файл слишком маленький, это невалидный SMO.");

        string signature = Encoding.ASCII.GetString(data, 0, 4);
        if (signature != "FFPS")
            throw new Exception($"Неизвестная сигнатура файла: {signature}");

        return new SmoHeader
        {
            Signature = signature,
            Unknown04 = ReadUInt32LE(data, 0x04),
            Unknown08 = ReadUInt32LE(data, 0x08),
            FileSize = ReadUInt32LE(data, 0x0C),
            Version = ReadUInt32LE(data, 0x10),
            DataStartOffset = ReadUInt32LE(data, 0x14),
            DataSize = ReadUInt32LE(data, 0x18),
            ObjectCount = ReadUInt32LE(data, 0x1C)
        };
    }

    private static List<SmoEntry> ParseEntries(byte[] data, SmoHeader header)
    {
        List<SmoEntry> entries = new();
        int offset = 0x20;

        for (int i = 0; i < header.ObjectCount; i++)
        {
            uint id = ReadUInt32LE(data, offset);
            offset += 4;

            ushort nameLength = ReadUInt16LE(data, offset);
            offset += 2;

            byte[] rawName = new byte[nameLength];
            Buffer.BlockCopy(data, offset, rawName, 0, nameLength);
            offset += nameLength;

            string name = DecodeName(rawName);

            uint typeHash = ReadUInt32LE(data, offset);
            offset += 4;

            uint fieldA = ReadUInt32LE(data, offset);
            offset += 4;

            uint fieldB = ReadUInt32LE(data, offset);
            offset += 4;

            entries.Add(new SmoEntry
            {
                Index = i,
                Id = id,
                Name = name,
                TypeHash = typeHash,
                FieldA = fieldA,
                FieldB = fieldB
            });
        }

        return entries;
    }

    private static void ComputeAbsoluteOffsetsAndSizes(List<SmoEntry> entries, SmoHeader header)
    {
        int payloadBase = checked((int)header.DataStartOffset + 4);
        int dataEnd = checked((int)header.DataStartOffset + (int)header.DataSize);

        foreach (SmoEntry e in entries)
            e.AbsoluteOffset = payloadBase + checked((int)e.FieldA);

        for (int i = 0; i < entries.Count; i++)
        {
            int start = entries[i].AbsoluteOffset;
            int end = (i < entries.Count - 1) ? entries[i + 1].AbsoluteOffset : dataEnd;

            if (end < start)
                end = start;

            entries[i].BlockSize = end - start;
        }
    }

    private static void DumpHeader(SmoHeader header)
    {
        Console.WriteLine("========================================");
        Console.WriteLine("SMO HEADER");
        Console.WriteLine("========================================");
        Console.WriteLine($"Signature       : {header.Signature}");
        Console.WriteLine($"Unknown04       : 0x{header.Unknown04:X8} ({header.Unknown04})");
        Console.WriteLine($"Unknown08       : 0x{header.Unknown08:X8} ({header.Unknown08})");
        Console.WriteLine($"FileSize        : 0x{header.FileSize:X8} ({header.FileSize})");
        Console.WriteLine($"Version         : 0x{header.Version:X8} ({header.Version})");
        Console.WriteLine($"DataStartOffset : 0x{header.DataStartOffset:X8} ({header.DataStartOffset})");
        Console.WriteLine($"DataSize        : 0x{header.DataSize:X8} ({header.DataSize})");
        Console.WriteLine($"ObjectCount     : {header.ObjectCount}");
        Console.WriteLine($"PayloadBase     : 0x{(header.DataStartOffset + 4):X8} ({header.DataStartOffset + 4})");
        Console.WriteLine();
    }

    private static void DumpEntries(List<SmoEntry> entries)
    {
        Console.WriteLine("========================================");
        Console.WriteLine("SMO OBJECT TABLE (EXTENDED)");
        Console.WriteLine("========================================");

        foreach (SmoEntry e in entries)
        {
            Console.WriteLine(
                $"[{e.Index:D2}] " +
                $"ID={e.Id,-3} " +
                $"Name=\"{e.Name}\" " +
                $"Type=0x{e.TypeHash:X8} " +
                $"A=0x{e.FieldA:X8} " +
                $"B=0x{e.FieldB:X8} " +
                $"Abs=0x{e.AbsoluteOffset:X8} " +
                $"Size=0x{e.BlockSize:X} ({e.BlockSize})");
        }

        Console.WriteLine();
    }

    private static void DumpVertexPreview(float[] vertices, int count)
    {
        Console.WriteLine("First vertices:");

        int vertexCount = vertices.Length / 3;
        int n = Math.Min(count, vertexCount);

        for (int i = 0; i < n; i++)
        {
            float x = vertices[i * 3 + 0];
            float y = vertices[i * 3 + 1];
            float z = vertices[i * 3 + 2];

            Console.WriteLine($"  V[{i:D3}] = ({x}, {y}, {z})");
        }

        Console.WriteLine();
    }

    private static void DumpIndexTriples(ushort[] strip, int count)
    {
        int n = Math.Min(count, Math.Max(0, strip.Length - 2));

        for (int i = 0; i < n; i++)
            Console.WriteLine($"  Strip[{i:D2}] = ({strip[i]}, {strip[i + 1]}, {strip[i + 2]})");

        Console.WriteLine();
    }

    private static string DecodeName(byte[] raw)
    {
        int zeroIndex = Array.IndexOf(raw, (byte)0);
        int length = zeroIndex >= 0 ? zeroIndex : raw.Length;
        return Encoding.ASCII.GetString(raw, 0, length);
    }

    private static ushort ReadUInt16LE(byte[] data, int offset) =>
        BitConverter.ToUInt16(data, offset);

    private static uint ReadUInt32LE(byte[] data, int offset) =>
        BitConverter.ToUInt32(data, offset);

    private static float ReadSingleLE(byte[] data, int offset) =>
        BitConverter.ToSingle(data, offset);

    public static ModelData LoadTestCube()
    {
        float[] vertices =
        {
            -0.5f, -0.5f,  0.5f,
             0.5f, -0.5f,  0.5f,
             0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f,

            -0.5f, -0.5f, -0.5f,
             0.5f, -0.5f, -0.5f,
             0.5f,  0.5f, -0.5f,
            -0.5f,  0.5f, -0.5f,
        };

        uint[] indices =
        {
            0, 1, 2, 2, 3, 0,
            1, 5, 6, 6, 2, 1,
            5, 4, 7, 7, 6, 5,
            4, 0, 3, 3, 7, 4,
            3, 2, 6, 6, 7, 3,
            4, 5, 1, 1, 0, 4
        };

        return new ModelData
        {
            Vertices = vertices,
            Indices = indices
        };
    }

    private sealed class PositionGuess
    {
        public int PositionOffset { get; set; }
        public float[] Vertices { get; set; } = Array.Empty<float>();
        public int Score { get; set; }
    }

    private sealed class SmoHeader
    {
        public string Signature { get; set; } = "";
        public uint Unknown04 { get; set; }
        public uint Unknown08 { get; set; }
        public uint FileSize { get; set; }
        public uint Version { get; set; }
        public uint DataStartOffset { get; set; }
        public uint DataSize { get; set; }
        public uint ObjectCount { get; set; }
    }

    private sealed class SmoEntry
    {
        public int Index { get; set; }
        public uint Id { get; set; }
        public string Name { get; set; } = "";
        public uint TypeHash { get; set; }
        public uint FieldA { get; set; }
        public uint FieldB { get; set; }
        public int AbsoluteOffset { get; set; }
        public int BlockSize { get; set; }
    }
}