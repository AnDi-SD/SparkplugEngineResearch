using System;

public class ModelData
{
    public float[] Vertices { get; set; } = Array.Empty<float>();
    public uint[] Indices { get; set; } = Array.Empty<uint>();

    public void CenterAndScaleSafe(float targetSize = 2.0f)
    {
        if (Vertices.Length < 3)
            return;

        bool found = false;

        float minX = 0, minY = 0, minZ = 0;
        float maxX = 0, maxY = 0, maxZ = 0;

        for (int i = 0; i < Vertices.Length; i += 3)
        {
            float x = Vertices[i + 0];
            float y = Vertices[i + 1];
            float z = Vertices[i + 2];

            if (!IsReasonable(x, y, z))
                continue;

            if (!found)
            {
                minX = maxX = x;
                minY = maxY = y;
                minZ = maxZ = z;
                found = true;
                continue;
            }

            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
            if (z < minZ) minZ = z;
            if (z > maxZ) maxZ = z;
        }

        if (!found)
            return;

        float centerX = (minX + maxX) * 0.5f;
        float centerY = (minY + maxY) * 0.5f;
        float centerZ = (minZ + maxZ) * 0.5f;

        float sizeX = maxX - minX;
        float sizeY = maxY - minY;
        float sizeZ = maxZ - minZ;

        float maxSize = MathF.Max(sizeX, MathF.Max(sizeY, sizeZ));
        if (maxSize < 0.0001f)
            maxSize = 1.0f;

        float scale = targetSize / maxSize;

        for (int i = 0; i < Vertices.Length; i += 3)
        {
            float x = Vertices[i + 0];
            float y = Vertices[i + 1];
            float z = Vertices[i + 2];

            if (!IsReasonable(x, y, z))
            {
                Vertices[i + 0] = 0f;
                Vertices[i + 1] = 0f;
                Vertices[i + 2] = 0f;
                continue;
            }

            Vertices[i + 0] = (x - centerX) * scale;
            Vertices[i + 1] = (y - centerY) * scale;
            Vertices[i + 2] = (z - centerZ) * scale;
        }
    }

    private static bool IsReasonable(float x, float y, float z)
    {
        return IsReasonableFloat(x) &&
               IsReasonableFloat(y) &&
               IsReasonableFloat(z);
    }

    private static bool IsReasonableFloat(float v)
    {
        if (float.IsNaN(v) || float.IsInfinity(v))
            return false;

        if (MathF.Abs(v) > 100000f)
            return false;

        if (MathF.Abs(v) > 1e20f)
            return false;

        return true;
    }
}