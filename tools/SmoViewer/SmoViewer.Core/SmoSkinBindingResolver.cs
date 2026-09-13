using System.Collections.ObjectModel;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Resolves node bind-world matrices from confirmed skin palettes.</summary>
public static class SmoSkinBindingResolver
{
    public static IReadOnlyDictionary<int, Matrix4x4> ResolveBindWorldMatrices(
        SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        var candidates = new Dictionary<int, List<Matrix4x4>>();
        foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                     item.TypeHash == SmoClassIds.Skin))
        {
            if (!SmoSkinDecoder.TryDecode(document, entry, out SmoSkin? skin, out _) ||
                skin is null)
                continue;

            foreach (SmoSkinBone bone in skin.Bones)
            {
                if (!Matrix4x4.Invert(bone.InverseBindMatrix, out Matrix4x4 bindWorld) ||
                    !IsFinite(bindWorld))
                    continue;
                if (!candidates.TryGetValue(
                        bone.NodeObjectIndex, out List<Matrix4x4>? matrices))
                {
                    matrices = [];
                    candidates.Add(bone.NodeObjectIndex, matrices);
                }
                matrices.Add(bindWorld);
            }
        }

        // The same bone may occur in many 16-entry hardware palettes. Accept a
        // shared bind matrix only when every serialized inverse agrees.
        Dictionary<int, Matrix4x4> result = candidates
            .Where(item => item.Value.Skip(1).All(matrix =>
                ApproximatelyEqual(item.Value[0], matrix)))
            .ToDictionary(item => item.Key, item => item.Value[0]);
        return new ReadOnlyDictionary<int, Matrix4x4>(result);
    }

    private static bool ApproximatelyEqual(Matrix4x4 left, Matrix4x4 right)
    {
        const float epsilon = 0.001f;
        return
            MathF.Abs(left.M11 - right.M11) <= epsilon &&
            MathF.Abs(left.M12 - right.M12) <= epsilon &&
            MathF.Abs(left.M13 - right.M13) <= epsilon &&
            MathF.Abs(left.M14 - right.M14) <= epsilon &&
            MathF.Abs(left.M21 - right.M21) <= epsilon &&
            MathF.Abs(left.M22 - right.M22) <= epsilon &&
            MathF.Abs(left.M23 - right.M23) <= epsilon &&
            MathF.Abs(left.M24 - right.M24) <= epsilon &&
            MathF.Abs(left.M31 - right.M31) <= epsilon &&
            MathF.Abs(left.M32 - right.M32) <= epsilon &&
            MathF.Abs(left.M33 - right.M33) <= epsilon &&
            MathF.Abs(left.M34 - right.M34) <= epsilon &&
            MathF.Abs(left.M41 - right.M41) <= epsilon &&
            MathF.Abs(left.M42 - right.M42) <= epsilon &&
            MathF.Abs(left.M43 - right.M43) <= epsilon &&
            MathF.Abs(left.M44 - right.M44) <= epsilon;
    }

    private static bool IsFinite(Matrix4x4 value) =>
        float.IsFinite(value.M11) && float.IsFinite(value.M12) &&
        float.IsFinite(value.M13) && float.IsFinite(value.M14) &&
        float.IsFinite(value.M21) && float.IsFinite(value.M22) &&
        float.IsFinite(value.M23) && float.IsFinite(value.M24) &&
        float.IsFinite(value.M31) && float.IsFinite(value.M32) &&
        float.IsFinite(value.M33) && float.IsFinite(value.M34) &&
        float.IsFinite(value.M41) && float.IsFinite(value.M42) &&
        float.IsFinite(value.M43) && float.IsFinite(value.M44);
}
