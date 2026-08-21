using System.Numerics;
using SmoExporter.Core;

namespace SmoImporter.Core;

/// <summary>
/// Read-only result of posing the target SMO for the fitting preview. The scene
/// retains the target graph, palettes and bind data; only copies of skinned
/// vertex positions and normals may differ from the supplied target scene.
/// </summary>
public sealed record TargetRigFittingPreviewResult(
    SmoExportScene Scene,
    int SkinnedMeshCount,
    int SkinnedVertexCount,
    bool IsIdentityPose);

/// <summary>
/// Produces a transient, external-space view of the original target model in a
/// <see cref="TargetRigFittingPoseSnapshot"/>. It does not mutate an SMO
/// document, the supplied export scene, the target hierarchy, or inverse-bind
/// matrices.
/// </summary>
public static class TargetRigFittingPreviewBuilder
{
    private const float WeightEpsilon = 0.000001f;
    private const float JointIndexTolerance = 0.0001f;
    private const float MatrixTolerance = 0.0001f;

    /// <summary>
    /// Applies ordinary linear-blend skinning in the importer's external,
    /// row-vector space. For palette joint j the vertex transform is
    /// <c>inverseBind[j] * fittingWorld[j]</c>.
    /// </summary>
    public static TargetRigFittingPreviewResult Build(
        SmoExportScene targetScene,
        TargetRigFittingPoseSnapshot fittingPose)
    {
        ArgumentNullException.ThrowIfNull(targetScene);
        ArgumentNullException.ThrowIfNull(fittingPose);

        if (!string.Equals(
                targetScene.SourceSha256,
                fittingPose.TargetRigFingerprint,
                StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                "The fitting pose was captured from a different target SMO scene.");
        }
        if ((targetScene.Resources & SmoExportResourceTypes.Skeleton) == 0)
        {
            throw new InvalidDataException(
                "Target fitting preview requires a scene exported with Skeleton data.");
        }

        Dictionary<int, SmoExportSkin> skinsByObjectIndex = targetScene.Skins
            .GroupBy(skin => skin.ObjectIndex)
            .ToDictionary(
                group => group.Key,
                group => group.Count() == 1
                    ? group.Single()
                    : throw new InvalidDataException(
                        $"Target scene contains duplicate skin object [{group.Key}]."));
        var transformsBySkinObjectIndex = new Dictionary<int, Matrix4x4[]>();
        var previewMeshes = new SmoExportMesh[targetScene.Meshes.Count];
        int skinnedMeshCount = 0;
        int skinnedVertexCount = 0;

        for (int meshIndex = 0; meshIndex < targetScene.Meshes.Count; meshIndex++)
        {
            SmoExportMesh mesh = targetScene.Meshes[meshIndex];
            if (mesh.SkinObjectIndex is not int skinObjectIndex)
            {
                if (mesh.BlendWeights.Length != 0 || mesh.JointIndices.Length != 0)
                {
                    throw new InvalidDataException(
                        $"Target mesh [{mesh.ObjectIndex}] '{mesh.Name}' has skin arrays " +
                        "but no owning skin palette.");
                }
                previewMeshes[meshIndex] = CloneGeometry(mesh);
                continue;
            }

            if (!skinsByObjectIndex.TryGetValue(
                    skinObjectIndex, out SmoExportSkin? skin))
            {
                throw new InvalidDataException(
                    $"Target mesh [{mesh.ObjectIndex}] '{mesh.Name}' references missing " +
                    $"skin [{skinObjectIndex}].");
            }
            if (!transformsBySkinObjectIndex.TryGetValue(
                    skinObjectIndex, out Matrix4x4[]? paletteTransforms))
            {
                paletteTransforms = BuildPaletteTransforms(skin, fittingPose);
                transformsBySkinObjectIndex.Add(skinObjectIndex, paletteTransforms);
            }

            ValidateMeshSkinArrays(mesh);
            // Identity must reproduce the source scene bit-for-bit. Palette and
            // weight validation still runs, but no floating-point skinning does.
            if (fittingPose.IsIdentityPose)
            {
                ValidateMeshInfluences(mesh, paletteTransforms.Length);
                previewMeshes[meshIndex] = CloneGeometry(mesh);
            }
            else
            {
                previewMeshes[meshIndex] = PoseMesh(mesh, paletteTransforms);
            }
            skinnedMeshCount++;
            skinnedVertexCount = checked(skinnedVertexCount + mesh.Positions.Length);
        }

        var previewScene = targetScene with
        {
            Meshes = Array.AsReadOnly(previewMeshes)
        };
        return new TargetRigFittingPreviewResult(
            previewScene,
            skinnedMeshCount,
            skinnedVertexCount,
            fittingPose.IsIdentityPose);
    }

    private static Matrix4x4[] BuildPaletteTransforms(
        SmoExportSkin skin,
        TargetRigFittingPoseSnapshot fittingPose)
    {
        if (skin.JointObjectIndices.Count == 0 ||
            skin.InverseBindMatrices.Count != skin.JointObjectIndices.Count)
        {
            throw new InvalidDataException(
                $"Target skin [{skin.ObjectIndex}] '{skin.Name}' has incomplete " +
                "joint or inverse-bind data.");
        }

        var result = new Matrix4x4[skin.JointObjectIndices.Count];
        for (int paletteIndex = 0; paletteIndex < result.Length; paletteIndex++)
        {
            int objectIndex = skin.JointObjectIndices[paletteIndex];
            int rigJointIndex;
            try
            {
                rigJointIndex = fittingPose.Definition.GetJointIndexByObjectIndex(objectIndex);
            }
            catch (KeyNotFoundException exception)
            {
                throw new InvalidDataException(
                    $"Target skin [{skin.ObjectIndex}] '{skin.Name}' references object " +
                    $"[{objectIndex}] outside the captured fitting rig.", exception);
            }

            TargetRigJoint rigJoint = fittingPose.Definition.Joints[rigJointIndex];
            if (!rigJoint.IsDeformJoint)
            {
                throw new InvalidDataException(
                    $"Target skin [{skin.ObjectIndex}] '{skin.Name}' uses non-deform " +
                    $"fitting joint '{rigJoint.Name}'.");
            }
            Matrix4x4 inverseBind = skin.InverseBindMatrices[paletteIndex];
            if (!Matrix4x4.Invert(
                    rigJoint.BindWorldMatrix, out Matrix4x4 expectedInverseBind) ||
                !IsFinite(expectedInverseBind) ||
                !ApproximatelyEqual(
                    inverseBind, expectedInverseBind, MatrixTolerance))
            {
                throw new InvalidDataException(
                    $"Target skin [{skin.ObjectIndex}] '{skin.Name}' inverse bind " +
                    $"[{paletteIndex}] does not match fitting joint '{rigJoint.Name}'.");
            }

            Matrix4x4 transform = inverseBind *
                fittingPose.WorldMatrices[rigJointIndex];
            if (!IsFinite(transform))
            {
                throw new InvalidDataException(
                    $"Target fitting transform for joint '{rigJoint.Name}' is non-finite.");
            }
            result[paletteIndex] = transform;
        }
        return result;
    }

    private static void ValidateMeshSkinArrays(SmoExportMesh mesh)
    {
        if (mesh.BlendWeights.Length != mesh.Positions.Length ||
            mesh.JointIndices.Length != mesh.Positions.Length)
        {
            throw new InvalidDataException(
                $"Target mesh [{mesh.ObjectIndex}] '{mesh.Name}' has incomplete skin arrays.");
        }
        if (mesh.Normals.Length != 0 &&
            mesh.Normals.Length != mesh.Positions.Length)
        {
            throw new InvalidDataException(
                $"Target mesh [{mesh.ObjectIndex}] '{mesh.Name}' has an incomplete normal array.");
        }
    }

    private static SmoExportMesh PoseMesh(
        SmoExportMesh mesh,
        IReadOnlyList<Matrix4x4> paletteTransforms)
    {
        var positions = new Vector3[mesh.Positions.Length];
        Vector3[] normals = mesh.Normals.Length == mesh.Positions.Length
            ? new Vector3[mesh.Normals.Length]
            : [];
        Span<float> weights = stackalloc float[4];
        Span<float> jointValues = stackalloc float[4];
        Span<int> joints = stackalloc int[4];

        for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
        {
            Vector4 sourceWeights = mesh.BlendWeights[vertex];
            Vector4 sourceJoints = mesh.JointIndices[vertex];
            weights[0] = sourceWeights.X;
            weights[1] = sourceWeights.Y;
            weights[2] = sourceWeights.Z;
            weights[3] = sourceWeights.W;
            jointValues[0] = sourceJoints.X;
            jointValues[1] = sourceJoints.Y;
            jointValues[2] = sourceJoints.Z;
            jointValues[3] = sourceJoints.W;

            joints.Clear();
            float totalWeight = 0;
            for (int influence = 0; influence < 4; influence++)
            {
                float weight = weights[influence];
                if (!float.IsFinite(weight) || weight < 0)
                {
                    throw new InvalidDataException(
                        $"Target mesh [{mesh.ObjectIndex}] '{mesh.Name}' vertex " +
                        $"{vertex} has an invalid skin weight.");
                }
                if (weight <= WeightEpsilon)
                    continue;

                float jointValue = jointValues[influence];
                if (!float.IsFinite(jointValue) || jointValue < 0 ||
                    jointValue > int.MaxValue)
                {
                    throw InvalidJoint(mesh, vertex, jointValue);
                }
                int joint = checked((int)MathF.Round(jointValue));
                if (joint < 0 ||
                    joint >= paletteTransforms.Count ||
                    MathF.Abs(jointValue - joint) > JointIndexTolerance)
                {
                    throw InvalidJoint(mesh, vertex, jointValue);
                }
                joints[influence] = joint;
                totalWeight += weight;
            }
            if (!float.IsFinite(totalWeight) || totalWeight <= WeightEpsilon)
            {
                throw new InvalidDataException(
                    $"Target mesh [{mesh.ObjectIndex}] '{mesh.Name}' vertex " +
                    $"{vertex} has no usable skin influence.");
            }

            Matrix4x4 blended = default;
            for (int influence = 0; influence < 4; influence++)
            {
                if (weights[influence] <= WeightEpsilon)
                    continue;
                AddWeighted(
                    ref blended,
                    paletteTransforms[joints[influence]],
                    weights[influence] / totalWeight);
            }
            Vector3 posedPosition = Vector3.Transform(mesh.Positions[vertex], blended);
            if (!IsFinite(posedPosition))
            {
                throw new InvalidDataException(
                    $"Target fitting preview produced a non-finite position for mesh " +
                    $"[{mesh.ObjectIndex}] '{mesh.Name}' vertex {vertex}.");
            }
            positions[vertex] = posedPosition;

            if (normals.Length != 0)
            {
                if (!Matrix4x4.Invert(blended, out Matrix4x4 inverseBlend) ||
                    !IsFinite(inverseBlend))
                {
                    throw new InvalidDataException(
                        $"Target fitting normal transform is singular for mesh " +
                        $"[{mesh.ObjectIndex}] '{mesh.Name}' vertex {vertex}.");
                }
                Vector3 posedNormal = Vector3.TransformNormal(
                    mesh.Normals[vertex], Matrix4x4.Transpose(inverseBlend));
                if (!IsFinite(posedNormal) ||
                    posedNormal.LengthSquared() <= WeightEpsilon)
                {
                    throw new InvalidDataException(
                        $"Target fitting preview produced an invalid normal for mesh " +
                        $"[{mesh.ObjectIndex}] '{mesh.Name}' vertex {vertex}.");
                }
                normals[vertex] = Vector3.Normalize(posedNormal);
            }
        }

        return mesh with
        {
            Positions = positions,
            Normals = normals
        };
    }

    private static void ValidateMeshInfluences(
        SmoExportMesh mesh,
        int paletteSize)
    {
        for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
        {
            Vector4 weights = mesh.BlendWeights[vertex];
            Vector4 joints = mesh.JointIndices[vertex];
            float[] weightValues = [weights.X, weights.Y, weights.Z, weights.W];
            float[] jointValues = [joints.X, joints.Y, joints.Z, joints.W];
            float totalWeight = 0;
            for (int influence = 0; influence < 4; influence++)
            {
                float weight = weightValues[influence];
                if (!float.IsFinite(weight) || weight < 0)
                {
                    throw new InvalidDataException(
                        $"Target mesh [{mesh.ObjectIndex}] '{mesh.Name}' vertex " +
                        $"{vertex} has an invalid skin weight.");
                }
                if (weight <= WeightEpsilon)
                    continue;
                float jointValue = jointValues[influence];
                if (!float.IsFinite(jointValue) || jointValue < 0 ||
                    jointValue > int.MaxValue)
                {
                    throw InvalidJoint(mesh, vertex, jointValue);
                }
                int joint = checked((int)MathF.Round(jointValue));
                if (joint < 0 || joint >= paletteSize ||
                    MathF.Abs(jointValue - joint) > JointIndexTolerance)
                {
                    throw InvalidJoint(mesh, vertex, jointValue);
                }
                totalWeight += weight;
            }
            if (!float.IsFinite(totalWeight) || totalWeight <= WeightEpsilon)
            {
                throw new InvalidDataException(
                    $"Target mesh [{mesh.ObjectIndex}] '{mesh.Name}' vertex " +
                    $"{vertex} has no usable skin influence.");
            }
        }
    }

    private static InvalidDataException InvalidJoint(
        SmoExportMesh mesh,
        int vertex,
        float jointValue) =>
        new(
            $"Target mesh [{mesh.ObjectIndex}] '{mesh.Name}' vertex " +
            $"{vertex} references invalid palette joint {jointValue:G9}.");

    private static SmoExportMesh CloneGeometry(SmoExportMesh mesh) => mesh with
    {
        Positions = mesh.Positions.ToArray(),
        Normals = mesh.Normals.ToArray()
    };

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
