using System.Numerics;

namespace SmoImporter.Core;

/// <summary>
/// Bakes geometry authored around a temporary target-rig fitting pose back into
/// the target's immutable canonical bind pose. System.Numerics uses row vectors:
/// for one vertex, M = sum(weight[j] * (inverse(T[j]) * F[j])) maps canonical
/// bind space T to fitting space F, so fittingPosition * inverse(M) is the
/// canonical writer position. No target node, bind matrix, or inverse bind matrix
/// is modified.
/// </summary>
internal static class TargetRigFittingSkinBaker
{
    private const float WeightEpsilon = 0.000001f;
    private const float MatrixTolerance = 0.0001f;

    public static ImportedScene BakeToCanonical(
        ImportedScene fittingScene,
        TargetRigFittingPoseSnapshot fittingPose)
    {
        ArgumentNullException.ThrowIfNull(fittingScene);
        ArgumentNullException.ThrowIfNull(fittingPose);

        var transformsBySkeleton = new Dictionary<ImportedSkeleton, Matrix4x4[]>(
            ReferenceEqualityComparer.Instance);
        foreach (ImportedSkeleton skeleton in fittingScene.Meshes
                     .Select(mesh => mesh.Skinning?.Skeleton)
                     .OfType<ImportedSkeleton>()
                     .Distinct<ImportedSkeleton>(ReferenceEqualityComparer.Instance))
        {
            transformsBySkeleton.Add(
                skeleton,
                BuildCanonicalToFittingTransforms(skeleton, fittingPose));
        }
        if (fittingScene.Meshes.Any(mesh => mesh.Skinning is null))
        {
            throw new InvalidDataException(
                "A fitting-pose bake requires skinning on every prepared mesh.");
        }

        // Preserve the exact legacy/generated output for a reset pose. The
        // validation above still proves that the scene uses this target rig.
        if (fittingPose.IsIdentityPose)
            return fittingScene;

        ImportedMesh[] canonicalMeshes = fittingScene.Meshes
            .Select(mesh => BakeMesh(
                mesh,
                transformsBySkeleton[mesh.Skinning!.Skeleton]))
            .ToArray();
        return new ImportedScene(
            Array.AsReadOnly(canonicalMeshes),
            fittingScene.Textures,
            fittingScene.Materials);
    }

    private static Matrix4x4[] BuildCanonicalToFittingTransforms(
        ImportedSkeleton skeleton,
        TargetRigFittingPoseSnapshot fittingPose)
    {
        int jointCount = skeleton.JointNames.Count;
        if (jointCount == 0 || skeleton.InverseBindMatrices.Count != jointCount)
        {
            throw new InvalidDataException(
                $"Prepared skeleton '{skeleton.Name}' has incomplete inverse-bind data.");
        }

        var seenNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var result = new Matrix4x4[jointCount];
        for (int skeletonJoint = 0; skeletonJoint < jointCount; skeletonJoint++)
        {
            string name = skeleton.JointNames[skeletonJoint];
            if (string.IsNullOrWhiteSpace(name) || !seenNames.Add(name))
            {
                throw new InvalidDataException(
                    $"Prepared skeleton '{skeleton.Name}' has an empty or duplicate " +
                    $"target joint name '{name}'.");
            }

            int rigJoint;
            try
            {
                rigJoint = fittingPose.Definition.GetJointIndex(name);
            }
            catch (KeyNotFoundException exception)
            {
                throw new InvalidDataException(
                    $"Prepared skeleton joint '{name}' does not belong to the " +
                    "captured target rig.", exception);
            }
            TargetRigJoint targetJoint = fittingPose.Definition.Joints[rigJoint];
            if (!targetJoint.IsDeformJoint)
            {
                throw new InvalidDataException(
                    $"Prepared skeleton joint '{name}' is not a target deform joint.");
            }
            if (!Matrix4x4.Invert(
                    targetJoint.BindWorldMatrix,
                    out Matrix4x4 canonicalInverse) ||
                !IsFinite(canonicalInverse))
            {
                throw new InvalidDataException(
                    $"Canonical target bind matrix for '{name}' is singular.");
            }
            if (!ApproximatelyEqual(
                    canonicalInverse,
                    skeleton.InverseBindMatrices[skeletonJoint],
                    MatrixTolerance))
            {
                throw new InvalidDataException(
                    $"Prepared skeleton inverse bind for '{name}' is not the " +
                    "canonical inverse bind of the captured target rig.");
            }

            Matrix4x4 canonicalToFitting = canonicalInverse *
                fittingPose.WorldMatrices[rigJoint];
            if (!IsFinite(canonicalToFitting))
            {
                throw new InvalidDataException(
                    $"Fitting transform for target joint '{name}' is non-finite.");
            }
            result[skeletonJoint] = canonicalToFitting;
        }
        return result;
    }

    private static ImportedMesh BakeMesh(
        ImportedMesh source,
        IReadOnlyList<Matrix4x4> canonicalToFitting)
    {
        ImportedSkinning skinning = source.Skinning ?? throw new InvalidDataException(
            $"Prepared fitting mesh '{source.Name}' has no target skinning.");
        if (skinning.JointIndices.Length != source.Positions.Length ||
            skinning.Weights.Length != source.Positions.Length)
        {
            throw new InvalidDataException(
                $"Prepared fitting mesh '{source.Name}' has incomplete skin arrays.");
        }
        bool transformNormals = source.Normals.Length == source.Positions.Length;
        var positions = new Vector3[source.Positions.Length];
        Vector3[] normals = transformNormals
            ? new Vector3[source.Normals.Length]
            : source.Normals.ToArray();

        Span<ushort> joints = stackalloc ushort[4];
        Span<float> weights = stackalloc float[4];
        for (int vertex = 0; vertex < source.Positions.Length; vertex++)
        {
            ImportedJointIndices sourceJoints = skinning.JointIndices[vertex];
            Vector4 sourceWeights = skinning.Weights[vertex];
            joints[0] = sourceJoints.X;
            joints[1] = sourceJoints.Y;
            joints[2] = sourceJoints.Z;
            joints[3] = sourceJoints.W;
            weights[0] = sourceWeights.X;
            weights[1] = sourceWeights.Y;
            weights[2] = sourceWeights.Z;
            weights[3] = sourceWeights.W;

            float total = 0;
            for (int influence = 0; influence < 4; influence++)
            {
                float weight = weights[influence];
                if (!float.IsFinite(weight) || weight < 0)
                {
                    throw new InvalidDataException(
                        $"Fitting mesh '{source.Name}' vertex {vertex} has an " +
                        "invalid target weight.");
                }
                if (weight <= WeightEpsilon)
                    continue;
                if (joints[influence] >= canonicalToFitting.Count)
                {
                    throw new InvalidDataException(
                        $"Fitting mesh '{source.Name}' vertex {vertex} references " +
                        $"target joint {joints[influence]} outside its skeleton.");
                }
                total += weight;
            }
            if (!float.IsFinite(total) || total <= WeightEpsilon)
            {
                throw new InvalidDataException(
                    $"Fitting mesh '{source.Name}' vertex {vertex} has no usable " +
                    "target weights.");
            }

            Matrix4x4 blended = default;
            for (int influence = 0; influence < 4; influence++)
            {
                if (weights[influence] <= WeightEpsilon)
                    continue;
                AddWeighted(
                    ref blended,
                    canonicalToFitting[joints[influence]],
                    weights[influence] / total);
            }
            if (!IsFinite(blended) ||
                !Matrix4x4.Invert(blended, out Matrix4x4 fittingToCanonical) ||
                !IsFinite(fittingToCanonical))
            {
                throw new InvalidDataException(
                    $"Fitting-pose skin matrix for mesh '{source.Name}' vertex " +
                    $"{vertex} is singular or non-finite.");
            }

            Vector3 canonicalPosition = Vector3.Transform(
                source.Positions[vertex], fittingToCanonical);
            if (!IsFinite(canonicalPosition))
            {
                throw new InvalidDataException(
                    $"Fitting-pose bake produced a non-finite position for mesh " +
                    $"'{source.Name}' vertex {vertex}.");
            }
            positions[vertex] = canonicalPosition;

            if (transformNormals)
            {
                // fitting = canonical * M. A row-vector normal therefore maps
                // back with transpose(M), the inverse-transpose of inverse(M).
                Vector3 canonicalNormal = Vector3.TransformNormal(
                    source.Normals[vertex], Matrix4x4.Transpose(blended));
                if (!IsFinite(canonicalNormal) ||
                    canonicalNormal.LengthSquared() <= WeightEpsilon)
                {
                    throw new InvalidDataException(
                        $"Fitting-pose bake produced an invalid normal for mesh " +
                        $"'{source.Name}' vertex {vertex}.");
                }
                normals[vertex] = Vector3.Normalize(canonicalNormal);
            }
        }

        return source with
        {
            Positions = positions,
            Normals = normals
        };
    }

    private static void AddWeighted(
        ref Matrix4x4 target,
        Matrix4x4 value,
        float weight)
    {
        target.M11 += value.M11 * weight;
        target.M12 += value.M12 * weight;
        target.M13 += value.M13 * weight;
        target.M14 += value.M14 * weight;
        target.M21 += value.M21 * weight;
        target.M22 += value.M22 * weight;
        target.M23 += value.M23 * weight;
        target.M24 += value.M24 * weight;
        target.M31 += value.M31 * weight;
        target.M32 += value.M32 * weight;
        target.M33 += value.M33 * weight;
        target.M34 += value.M34 * weight;
        target.M41 += value.M41 * weight;
        target.M42 += value.M42 * weight;
        target.M43 += value.M43 * weight;
        target.M44 += value.M44 * weight;
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);

    private static bool IsFinite(Matrix4x4 value) =>
        float.IsFinite(value.M11) && float.IsFinite(value.M12) &&
        float.IsFinite(value.M13) && float.IsFinite(value.M14) &&
        float.IsFinite(value.M21) && float.IsFinite(value.M22) &&
        float.IsFinite(value.M23) && float.IsFinite(value.M24) &&
        float.IsFinite(value.M31) && float.IsFinite(value.M32) &&
        float.IsFinite(value.M33) && float.IsFinite(value.M34) &&
        float.IsFinite(value.M41) && float.IsFinite(value.M42) &&
        float.IsFinite(value.M43) && float.IsFinite(value.M44);

    private static bool ApproximatelyEqual(
        Matrix4x4 left,
        Matrix4x4 right,
        float epsilon) =>
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
