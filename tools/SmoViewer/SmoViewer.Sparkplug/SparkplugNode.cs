using System.Numerics;

namespace SmoViewer.Sparkplug;

/// <summary>Thin access to the same spNode transform used by the native scene runtime.</summary>
public static class SparkplugNode
{
    public static Matrix4x4 LocalMatrix(Vector3 position, Quaternion rotation, Vector3 scale)
    {
        NativeMethods.Node node = new() { Parent = -1, Position = position, Rotation = rotation, Scale = scale };
        NativeMethods.Check(NativeMethods.spv_node_local(in node, out var matrix, 16));
        return matrix;
    }
}
