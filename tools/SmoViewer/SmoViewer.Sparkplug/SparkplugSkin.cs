using System.Numerics;

namespace SmoViewer.Sparkplug;

/// <summary>Shared spSkin palette composition for static tool poses.</summary>
public static class SparkplugSkin
{
    public static Matrix4x4 ComposeMatrix(Matrix4x4 inverseBind, Matrix4x4 world)
    {
        NativeMethods.Check(NativeMethods.spv_skin_matrix(in inverseBind, in world, out var matrix, 16));
        return matrix;
    }
}
