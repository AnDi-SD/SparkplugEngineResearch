using System.Numerics;
using SmoExporter.Core;
using SmoViewer.Sparkplug;

namespace SmoImporter.Core;

/// <summary>
/// Read-only result of posing the target SMO for the fitting preview. The scene
/// retains the target graph, palettes and bind data; only copies of skinned
/// vertex positions differ. Normals are empty: this is a positions-only preview.
/// </summary>
public sealed record TargetRigFittingPreviewResult(
    SmoExportScene Scene,
    int SkinnedMeshCount,
    int SkinnedVertexCount,
    bool IsIdentityPose);

/// <summary>
/// Stable original mesh and read-only authored fitting palette for a positions
/// provider. Preparing this request does not evaluate vertex deformation.
/// </summary>
public sealed record TargetRigFittingPreviewRequest(
    SmoExportMesh Mesh,
    IReadOnlyList<Matrix4x4> PaletteTransforms);

/// <summary>Builds an owned positions-only snapshot without changing target or bind data.</summary>
public static class TargetRigFittingPreviewBuilder
{
    private const float MatrixTolerance = 0.0001f;

    /// <summary>
    /// Prepares authored fitting palettes in external row-vector space and
    /// delegates every skinned mesh, including identity poses, to the rendering
    /// backend. The provider receives original weights unchanged.
    /// </summary>
    public static TargetRigFittingPreviewResult Build(
        SmoExportScene targetScene,
        TargetRigFittingPoseSnapshot fittingPose,
        Func<TargetRigFittingPreviewRequest, Vector3[]> readPositions)
    {
        ArgumentNullException.ThrowIfNull(readPositions);
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
            Vector3[] positions = readPositions(new TargetRigFittingPreviewRequest(
                mesh, Array.AsReadOnly(paletteTransforms))) ?? throw new InvalidDataException(
                    "The target preview backend returned no positions.");
            if (positions.Length != mesh.Positions.Length || positions.Any(value => !IsFinite(value)))
                throw new InvalidDataException(
                    "The target preview backend must return all finite positions for the selected mesh.");
            previewMeshes[meshIndex] = mesh with
            {
                Positions = positions.ToArray(),
                Normals = []
            };
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

            Matrix4x4 transform = SparkplugSkin.ComposeMatrix(
                inverseBind, fittingPose.WorldMatrices[rigJointIndex]);
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

    private static SmoExportMesh CloneGeometry(SmoExportMesh mesh) => mesh with
    {
        Positions = mesh.Positions.ToArray(),
        Normals = []
    };

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
