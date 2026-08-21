using System.Collections.ObjectModel;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using SmoExporter.Core;

namespace SmoImporter.Core;

public enum TargetRigBodyComponentRole
{
    WholeBody,
    LowerBody,
    TorsoAndArms
}

/// <summary>
/// Original donor vertex membership for one selected connected body surface.
/// Indices refer to the immutable input <see cref="ImportedScene"/> and can be
/// reused later by generated-skinning preparation.
/// </summary>
public sealed record TargetRigBodyVertexMembership(
    int MeshIndex,
    string MeshName,
    IReadOnlyList<int> VertexIndices);

public sealed record TargetRigSelectedBodyComponent(
    int ComponentIndex,
    TargetRigBodyComponentRole Role,
    IReadOnlyList<TargetRigBodyVertexMembership> VerticesByMesh,
    int UniquePositionCount,
    int TriangleCount,
    float SurfaceArea,
    Vector3 AlignedMinimum,
    Vector3 AlignedMaximum);

public sealed record TargetRigBodySelection(
    IReadOnlyList<TargetRigSelectedBodyComponent> Components,
    int TotalComponentCount,
    int ExcludedComponentCount,
    string TargetRigFingerprint,
    string DonorGeometryFingerprint,
    ReplacementTransform DonorAlignment);

public sealed record TargetRigAutomaticPoseFitOptions(
    int OptimizationPasses = 4,
    int MaximumTargetSurfaceSamples = 256,
    float TrimmedSurfaceFraction = 0.85f);

public sealed record TargetRigAutomaticPoseFitResult(
    TargetRigBodyPoseParameters Parameters,
    TargetRigFittingPoseSnapshot Pose,
    TargetRigBodySelection BodySelection,
    float ScoreBefore,
    float ScoreAfter,
    IReadOnlyList<string> Diagnostics);

/// <summary>
/// Deterministic, conservative humanoid fit for an aligned unskinned donor.
/// It selects graph-connected lower-body and torso/arm surfaces by geometry,
/// explicitly excludes disconnected one-sided sheets such as wings, extracts
/// bilateral limb directions, and optimizes the six whole-body controls
/// exposed by <see cref="TargetRigBodyPoseMapper"/>. The optional manual neck
/// control remains neutral during automatic fitting.
/// </summary>
public static class TargetRigAutomaticPoseFitter
{
    private const float PositionEpsilon = 0.000001f;
    private const float MinimumCentralFraction = 0.02f;
    private const float MinimumDirectionDotImprovement = 0.000001f;

    private readonly record struct VertexRef(int MeshIndex, int VertexIndex);

    private sealed record GeometryComponent(
        int ComponentIndex,
        VertexRef[] Vertices,
        Vector3[] UniqueAlignedPositions,
        int TriangleCount,
        float Area,
        Vector3 Minimum,
        Vector3 Maximum)
    {
        public Vector3 Size => Maximum - Minimum;
    }

    private sealed record LimbTargets(
        Vector3 LeftArmDirection,
        Vector3 RightArmDirection,
        Vector3 LeftLegDirection,
        Vector3 RightLegDirection,
        Vector3 TorsoDirection);

    private sealed record FitContext(
        TargetRigDefinition Rig,
        LimbTargets Targets,
        IReadOnlyList<Vector3> TargetSurfaceSamples,
        IReadOnlyList<Vector3> DonorBodyPositions,
        float NormalizationLength);

    /// <summary>
    /// Selects the donor surfaces that represent the continuous humanoid body
    /// without changing or optimizing the target-rig pose. This is the manual
    /// fitting counterpart of <see cref="Fit"/>: callers can prepare generated
    /// weights from user-authored joint rotations without first accepting an
    /// automatically fitted pose.
    /// </summary>
    public static TargetRigBodySelection SelectBody(
        TargetRigDefinition targetRig,
        ImportedScene donor,
        ReplacementTransform donorAlignment)
    {
        ArgumentNullException.ThrowIfNull(targetRig);
        ArgumentNullException.ThrowIfNull(donor);
        ValidateBodySelectionInputs(donor, donorAlignment);

        GeometryComponent[] components = BuildComponents(
            donor,
            donorAlignment.Matrix);
        (GeometryComponent lower, GeometryComponent upper) =
            SelectBodyComponents(targetRig, components);
        return BuildPublicSelection(
            donor,
            components,
            lower,
            upper,
            targetRig.SourceFingerprint,
            ComputeDonorGeometryFingerprint(donor),
            donorAlignment);
    }

    public static TargetRigAutomaticPoseFitResult Fit(
        TargetRigDefinition targetRig,
        SmoExportScene targetScene,
        ImportedScene donor,
        ReplacementTransform donorAlignment,
        TargetRigAutomaticPoseFitOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(targetRig);
        ArgumentNullException.ThrowIfNull(targetScene);
        ArgumentNullException.ThrowIfNull(donor);
        options ??= new TargetRigAutomaticPoseFitOptions();
        ValidateOptions(options);
        ValidateInputs(targetRig, targetScene, donor, donorAlignment);

        Matrix4x4 alignment = donorAlignment.Matrix;
        GeometryComponent[] components = BuildComponents(donor, alignment);
        (GeometryComponent lower, GeometryComponent upper) =
            SelectBodyComponents(targetRig, components);
        GeometryComponent[] selected = lower.ComponentIndex == upper.ComponentIndex
            ? [lower]
            : [lower, upper];
        TargetRigBodySelection publicSelection = BuildPublicSelection(
            donor,
            components,
            lower,
            upper,
            targetRig.SourceFingerprint,
            ComputeDonorGeometryFingerprint(donor),
            donorAlignment);

        LimbTargets targets = ExtractTargets(targetRig, lower, upper);
        Vector3[] donorBodyPositions = selected
            .SelectMany(component => component.UniqueAlignedPositions)
            .Distinct()
            .ToArray();
        Vector3[] targetSamples = BuildTargetSurfaceSamples(
            targetRig,
            targetScene,
            options.MaximumTargetSurfaceSamples);
        float normalizationLength = ComputeNormalizationLength(targetRig);
        var context = new FitContext(
            targetRig,
            targets,
            Array.AsReadOnly(targetSamples),
            Array.AsReadOnly(donorBodyPositions),
            normalizationLength);

        TargetRigFittingPoseSnapshot identity = targetRig.CreateFittingPose().Capture();
        float scoreBefore = Score(identity, context, options.TrimmedSurfaceFraction);
        TargetRigBodyPoseParameters seed = DeriveSeed(targetRig, targets);
        (TargetRigBodyPoseParameters parameters,
            TargetRigFittingPoseSnapshot pose,
            float scoreAfter) = Optimize(seed, context, options);
        if (!float.IsFinite(scoreBefore) || !float.IsFinite(scoreAfter))
            throw new InvalidOperationException("Automatic target-rig fitting produced a non-finite score.");
        if (scoreAfter > scoreBefore - MinimumDirectionDotImprovement)
        {
            throw new InvalidDataException(
                "Automatic target-rig fitting could not improve the canonical bind pose " +
                "for the selected donor body surfaces.");
        }

        float bindHandY = 0.5f * (
            Translation(targetRig.Joints[targetRig.GetJointIndex("L_Hand")].BindWorldMatrix).Y +
            Translation(targetRig.Joints[targetRig.GetJointIndex("R_Hand")].BindWorldMatrix).Y);
        float posedHandY = 0.5f * (
            Translation(pose.WorldMatrices[targetRig.GetJointIndex("L_Hand")]).Y +
            Translation(pose.WorldMatrices[targetRig.GetJointIndex("R_Hand")]).Y);
        float bindAnkleAbsX = 0.5f * (
            MathF.Abs(Translation(targetRig.Joints[targetRig.GetJointIndex("L_Ankle")].BindWorldMatrix).X) +
            MathF.Abs(Translation(targetRig.Joints[targetRig.GetJointIndex("R_Ankle")].BindWorldMatrix).X));
        float posedAnkleAbsX = 0.5f * (
            MathF.Abs(Translation(pose.WorldMatrices[targetRig.GetJointIndex("L_Ankle")]).X) +
            MathF.Abs(Translation(pose.WorldMatrices[targetRig.GetJointIndex("R_Ankle")]).X));

        var diagnostics = new List<string>
        {
            $"Selected {selected.Length} body component(s) from {components.Length}; " +
            $"excluded {components.Length - selected.Length} disconnected component(s).",
            $"Body selection contains {donorBodyPositions.Length} unique aligned positions; " +
            $"objective sampled {targetSamples.Length} target body vertices.",
            $"Score improved from {scoreBefore:G9} to {scoreAfter:G9}.",
            $"Average hand Y changed from {bindHandY:G9} to {posedHandY:G9}; " +
            $"average absolute ankle X changed from {bindAnkleAbsX:G9} to " +
            $"{posedAnkleAbsX:G9}.",
            $"Parameters: arm elevation={parameters.ArmElevationDegrees:G6}°, " +
            $"arm forward={parameters.ArmForwardDegrees:G6}°, " +
            $"elbow bend={parameters.ElbowBendDegrees:G6}°, " +
            $"leg spread={parameters.LegSpreadDegrees:G6}°, " +
            $"knee bend={parameters.KneeBendDegrees:G6}°, " +
            $"torso pitch={parameters.TorsoPitchDegrees:G6}°."
        };
        return new TargetRigAutomaticPoseFitResult(
            parameters,
            pose,
            publicSelection,
            scoreBefore,
            scoreAfter,
            new ReadOnlyCollection<string>(diagnostics));
    }

    private static void ValidateInputs(
        TargetRigDefinition rig,
        SmoExportScene targetScene,
        ImportedScene donor,
        ReplacementTransform alignment)
    {
        if (!string.Equals(
                rig.SourceFingerprint,
                targetScene.SourceSha256,
                StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException(
                "Target export scene and target fitting rig come from different SMO data.");
        }
        if (targetScene.Meshes.Count == 0 || targetScene.Skins.Count == 0)
            throw new InvalidDataException(
                "Automatic fitting requires target meshes and skeleton resources.");
        ValidateBodySelectionInputs(donor, alignment);
    }

    private static void ValidateBodySelectionInputs(
        ImportedScene donor,
        ReplacementTransform alignment)
    {
        if (donor.Meshes.Count == 0)
            throw new InvalidDataException("Automatic fitting donor contains no meshes.");
        if (donor.Meshes.Any(mesh => mesh.Skinning is not null))
            throw new InvalidDataException(
                "Automatic generated-rig fitting accepts only an unskinned donor.");
        if (!float.IsFinite(alignment.Scale) || alignment.Scale <= 0 ||
            !IsFinite(alignment.RotationDegrees) ||
            alignment.RotationDegrees != Vector3.Zero ||
            !IsFinite(alignment.Translation) ||
            !TargetRigDefinition.IsFinite(alignment.Matrix) ||
            !Matrix4x4.Invert(alignment.Matrix, out _))
        {
            throw new ArgumentException(
                "Automatic generated-rig fitting requires an invertible positive " +
                "uniform scale and translation, with zero donor rotation.",
                nameof(alignment));
        }
    }

    private static void ValidateOptions(TargetRigAutomaticPoseFitOptions options)
    {
        if (options.OptimizationPasses is < 1 or > 8)
            throw new ArgumentOutOfRangeException(nameof(options.OptimizationPasses));
        if (options.MaximumTargetSurfaceSamples is < 32 or > 4096)
            throw new ArgumentOutOfRangeException(nameof(options.MaximumTargetSurfaceSamples));
        if (!float.IsFinite(options.TrimmedSurfaceFraction) ||
            options.TrimmedSurfaceFraction is < 0.5f or > 1)
            throw new ArgumentOutOfRangeException(nameof(options.TrimmedSurfaceFraction));
    }

    private static GeometryComponent[] BuildComponents(
        ImportedScene donor,
        Matrix4x4 alignment)
    {
        int[] offsets = new int[donor.Meshes.Count];
        int totalVertices = 0;
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            offsets[meshIndex] = totalVertices;
            totalVertices = checked(totalVertices + donor.Meshes[meshIndex].Positions.Length);
        }
        if (totalVertices == 0)
            throw new InvalidDataException("Automatic fitting donor has no vertices.");

        var refs = new VertexRef[totalVertices];
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = donor.Meshes[meshIndex];
            if (mesh.Positions.Any(position => !IsFinite(position)))
                throw new InvalidDataException(
                    $"Donor mesh [{meshIndex}] {mesh.Name} has non-finite positions.");
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
                refs[offsets[meshIndex] + vertex] = new VertexRef(meshIndex, vertex);
        }

        var union = new UnionFind(totalVertices);
        var referenced = new bool[totalVertices];
        var triangles = new List<(int First, int Second, int Third, float Area)>();
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = donor.Meshes[meshIndex];
            if (mesh.TriangleIndices.Length % 3 != 0)
                throw new InvalidDataException(
                    $"Donor mesh [{meshIndex}] {mesh.Name} has an incomplete triangle index list.");
            if (mesh.TriangleIndices.Length == 0)
                continue;
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                int first = CheckedIndex(mesh, mesh.TriangleIndices[index], meshIndex);
                int second = CheckedIndex(mesh, mesh.TriangleIndices[index + 1], meshIndex);
                int third = CheckedIndex(mesh, mesh.TriangleIndices[index + 2], meshIndex);
                Vector3 a = mesh.Positions[first];
                Vector3 b = mesh.Positions[second];
                Vector3 c = mesh.Positions[third];
                float area = Vector3.Cross(b - a, c - a).Length() * 0.5f;
                if (!float.IsFinite(area))
                    throw new InvalidDataException(
                        $"Donor mesh [{meshIndex}] {mesh.Name} has a triangle with " +
                        "non-finite surface area.");
                if (first == second || second == third || first == third ||
                    area <= PositionEpsilon * PositionEpsilon)
                    continue;
                int ga = offsets[meshIndex] + first;
                int gb = offsets[meshIndex] + second;
                int gc = offsets[meshIndex] + third;
                referenced[ga] = referenced[gb] = referenced[gc] = true;
                union.Union(ga, gb);
                union.Union(ga, gc);
                triangles.Add((ga, gb, gc, area));
            }
        }
        var byPosition = new Dictionary<Vector3, List<int>>();
        for (int global = 0; global < refs.Length; global++)
        {
            // Attribute-seam expansion can retain source vertices which occur
            // only in zero-area faces. They have no rendered surface and must
            // not become singleton graph components. Original mesh/vertex
            // indices remain unchanged for the preparation stage.
            if (!referenced[global])
                continue;
            VertexRef vertex = refs[global];
            Vector3 position = donor.Meshes[vertex.MeshIndex].Positions[vertex.VertexIndex];
            if (!byPosition.TryGetValue(position, out List<int>? equal))
            {
                equal = [];
                byPosition.Add(position, equal);
            }
            equal.Add(global);
        }
        var shared = new Dictionary<(int First, int Second), int>();
        foreach (List<int> equal in byPosition.Values)
        {
            int[] roots = equal.Select(union.Find).Distinct().Order().ToArray();
            if (roots.Length > 32)
                throw new InvalidDataException(
                    "Automatic fitting donor has ambiguous overlapping graph components.");
            for (int first = 0; first < roots.Length; first++)
                for (int second = first + 1; second < roots.Length; second++)
                {
                    var key = (roots[first], roots[second]);
                    shared[key] = shared.GetValueOrDefault(key) + 1;
                }
        }
        foreach (KeyValuePair<(int First, int Second), int> pair in shared)
            if (pair.Value >= 2)
                union.Union(pair.Key.First, pair.Key.Second);

        Dictionary<int, List<int>> globalsByRoot = Enumerable.Range(0, refs.Length)
            .Where(global => referenced[global])
            .GroupBy(union.Find)
            .ToDictionary(group => group.Key, group => group.ToList());
        Dictionary<int, List<(int First, int Second, int Third, float Area)>> trianglesByRoot =
            triangles.GroupBy(triangle => union.Find(triangle.First))
                .ToDictionary(group => group.Key, group => group.ToList());
        var result = new List<GeometryComponent>();
        int componentIndex = 0;
        foreach (List<int> globals in globalsByRoot.Values.OrderBy(group => group.Min()))
        {
            int root = union.Find(globals[0]);
            List<(int First, int Second, int Third, float Area)> componentTriangles =
                trianglesByRoot.GetValueOrDefault(root) ?? [];
            double rawArea = componentTriangles.Sum(triangle => (double)triangle.Area);
            if (componentTriangles.Count == 0 || !double.IsFinite(rawArea) || rawArea <= 0)
                throw new InvalidDataException(
                    $"Donor graph component {componentIndex} has no usable surface.");
            VertexRef[] componentVertices = globals.Select(global => refs[global]).ToArray();
            Vector3[] positions = componentVertices
                .Select(vertex => Vector3.Transform(
                    donor.Meshes[vertex.MeshIndex].Positions[vertex.VertexIndex],
                    alignment))
                .Distinct()
                .ToArray();
            if (positions.Length < 3 || positions.Any(position => !IsFinite(position)))
                throw new InvalidDataException(
                    $"Donor graph component {componentIndex} has invalid aligned positions.");
            (Vector3 minimum, Vector3 maximum) = Bounds(positions);
            float alignedArea = checked((float)(rawArea *
                (double)(alignment.M11 * alignment.M11)));
            result.Add(new GeometryComponent(
                componentIndex++,
                componentVertices,
                positions,
                componentTriangles.Count,
                alignedArea,
                minimum,
                maximum));
        }
        if (result.Count == 0)
            throw new InvalidDataException("Automatic fitting donor has no graph components.");
        return result.ToArray();
    }

    private static (GeometryComponent Lower, GeometryComponent Upper) SelectBodyComponents(
        TargetRigDefinition rig,
        IReadOnlyList<GeometryComponent> components)
    {
        float centerX = BodyCenterX(rig);
        float targetHeight = ComputeNormalizationLength(rig);
        float centerTolerance = targetHeight * MinimumCentralFraction;
        GeometryComponent[] central = components
            .Where(component =>
                component.Minimum.X <= centerX + centerTolerance &&
                component.Maximum.X >= centerX - centerTolerance &&
                component.Size.Y >= targetHeight * 0.12f &&
                component.UniqueAlignedPositions.Length >= 24)
            .ToArray();
        if (central.Length == 0)
            throw new InvalidDataException(
                "Donor has no substantial connected surface crossing the target body center.");

        GeometryComponent upper = central
            .OrderByDescending(component => component.Size.X)
            .ThenByDescending(component => component.Area)
            .ThenByDescending(component => component.Size.Y)
            .ThenBy(component => component.ComponentIndex)
            .First();
        float armReach = ChainLength(rig, "L_Bicep", "L_UpperArm", "L_Hand");
        if (upper.Size.X < armReach * 1.25f)
            throw new InvalidDataException(
                "Donor has no unambiguous bilateral torso-and-arms component.");

        GeometryComponent lower = central
            .Where(component =>
                component.ComponentIndex == upper.ComponentIndex ||
                component.Minimum.Y <= upper.Minimum.Y + targetHeight * 0.12f)
            .OrderBy(component => component.Minimum.Y)
            .ThenByDescending(component => component.Size.Y)
            .ThenByDescending(component => component.Area)
            .ThenBy(component => component.ComponentIndex)
            .First();
        float legReach = ChainLength(rig, "L_Thigh", "L_calf", "L_Ankle");
        if (lower.Size.Y < legReach * 0.55f)
            throw new InvalidDataException(
                "Donor has no substantial centered lower-body component.");
        if (lower.ComponentIndex != upper.ComponentIndex)
        {
            float overlap = MathF.Min(lower.Maximum.Y, upper.Maximum.Y) -
                            MathF.Max(lower.Minimum.Y, upper.Minimum.Y);
            float allowedGap = targetHeight * 0.04f;
            if (overlap < -allowedGap)
                throw new InvalidDataException(
                    "Selected donor lower-body and torso surfaces do not meet vertically.");
        }
        return (lower, upper);
    }

    private static TargetRigBodySelection BuildPublicSelection(
        ImportedScene donor,
        IReadOnlyList<GeometryComponent> all,
        GeometryComponent lower,
        GeometryComponent upper,
        string targetRigFingerprint,
        string donorGeometryFingerprint,
        ReplacementTransform donorAlignment)
    {
        GeometryComponent[] selected = lower.ComponentIndex == upper.ComponentIndex
            ? [lower]
            : [lower, upper];
        TargetRigSelectedBodyComponent[] publicComponents = selected
            .Select(component =>
            {
                TargetRigBodyComponentRole role = lower.ComponentIndex == upper.ComponentIndex
                    ? TargetRigBodyComponentRole.WholeBody
                    : component.ComponentIndex == lower.ComponentIndex
                        ? TargetRigBodyComponentRole.LowerBody
                        : TargetRigBodyComponentRole.TorsoAndArms;
                TargetRigBodyVertexMembership[] membership = component.Vertices
                    .GroupBy(vertex => vertex.MeshIndex)
                    .OrderBy(group => group.Key)
                    .Select(group => new TargetRigBodyVertexMembership(
                        group.Key,
                        donor.Meshes[group.Key].Name,
                        Array.AsReadOnly(group
                            .Select(vertex => vertex.VertexIndex)
                            .Distinct()
                            .Order()
                            .ToArray())))
                    .ToArray();
                return new TargetRigSelectedBodyComponent(
                    component.ComponentIndex,
                    role,
                    Array.AsReadOnly(membership),
                    component.UniqueAlignedPositions.Length,
                    component.TriangleCount,
                    component.Area,
                    component.Minimum,
                    component.Maximum);
            })
            .ToArray();
        return new TargetRigBodySelection(
            Array.AsReadOnly(publicComponents),
            all.Count,
            all.Count - selected.Length,
            targetRigFingerprint,
            donorGeometryFingerprint,
            donorAlignment);
    }

    internal static string ComputeDonorGeometryFingerprint(ImportedScene donor)
    {
        ArgumentNullException.ThrowIfNull(donor);
        using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        AppendInt(hash, donor.Meshes.Count);
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = donor.Meshes[meshIndex];
            AppendInt(hash, meshIndex);
            AppendString(hash, mesh.Name);
            AppendInt(hash, mesh.MaterialIndex);
            AppendInt(hash, mesh.Positions.Length);
            foreach (Vector3 value in mesh.Positions)
            {
                AppendFloat(hash, value.X);
                AppendFloat(hash, value.Y);
                AppendFloat(hash, value.Z);
            }
            AppendInt(hash, mesh.Normals.Length);
            foreach (Vector3 value in mesh.Normals)
            {
                AppendFloat(hash, value.X);
                AppendFloat(hash, value.Y);
                AppendFloat(hash, value.Z);
            }
            AppendInt(hash, mesh.TextureCoordinates.Length);
            foreach (Vector2 value in mesh.TextureCoordinates)
            {
                AppendFloat(hash, value.X);
                AppendFloat(hash, value.Y);
            }
            AppendInt(hash, mesh.TriangleIndices.Length);
            foreach (uint value in mesh.TriangleIndices)
                AppendUInt(hash, value);
            AppendInt(hash, mesh.DiffuseColors.Length);
            foreach (uint value in mesh.DiffuseColors)
                AppendUInt(hash, value);
            // Generated-rig fitting accepts only unskinned scenes. Preserve the
            // state bit in the identity so a later scene substitution cannot
            // silently reuse this selection.
            AppendInt(hash, mesh.Skinning is null ? 0 : 1);
        }
        return Convert.ToHexString(hash.GetHashAndReset());
    }

    private static void AppendString(IncrementalHash hash, string value)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(value ?? string.Empty);
        AppendInt(hash, bytes.Length);
        hash.AppendData(bytes);
    }

    private static void AppendFloat(IncrementalHash hash, float value) =>
        AppendInt(hash, BitConverter.SingleToInt32Bits(value));

    private static void AppendUInt(IncrementalHash hash, uint value)
    {
        Span<byte> bytes = stackalloc byte[sizeof(uint)];
        System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(bytes, value);
        hash.AppendData(bytes);
    }

    private static void AppendInt(IncrementalHash hash, int value)
    {
        Span<byte> bytes = stackalloc byte[sizeof(int)];
        System.Buffers.Binary.BinaryPrimitives.WriteInt32LittleEndian(bytes, value);
        hash.AppendData(bytes);
    }

    private static LimbTargets ExtractTargets(
        TargetRigDefinition rig,
        GeometryComponent lower,
        GeometryComponent upper)
    {
        IReadOnlyList<Matrix4x4> bind = rig.Joints
            .Select(joint => joint.BindWorldMatrix)
            .ToArray();
        Vector3 leftShoulder = Translation(bind[rig.GetJointIndex("L_Bicep")]);
        Vector3 rightShoulder = Translation(bind[rig.GetJointIndex("R_Bicep")]);
        Vector3 leftHip = Translation(bind[rig.GetJointIndex("L_Thigh")]);
        Vector3 rightHip = Translation(bind[rig.GetJointIndex("R_Thigh")]);
        float armReach = ChainLength(rig, "L_Bicep", "L_UpperArm", "L_Hand");
        float legReach = ChainLength(rig, "L_Thigh", "L_calf", "L_Ankle");
        float centerX = BodyCenterX(rig);

        Vector3 leftArm = ExtractArmDirection(
            upper.UniqueAlignedPositions, centerX, leftShoulder, armReach, side: 1);
        Vector3 rightArm = ExtractArmDirection(
            upper.UniqueAlignedPositions, centerX, rightShoulder, armReach, side: -1);
        Vector3 leftLeg = ExtractLegDirection(
            lower.UniqueAlignedPositions, leftHip, legReach, side: 1);
        Vector3 rightLeg = ExtractLegDirection(
            lower.UniqueAlignedPositions, rightHip, legReach, side: -1);
        Vector3 torso = ExtractTorsoDirection(upper.UniqueAlignedPositions, centerX);
        return new LimbTargets(leftArm, rightArm, leftLeg, rightLeg, torso);
    }

    private static Vector3 ExtractArmDirection(
        IReadOnlyList<Vector3> positions,
        float centerX,
        Vector3 shoulder,
        float reach,
        int side)
    {
        float maximumLateral = positions.Max(position => side * (position.X - centerX));
        if (!float.IsFinite(maximumLateral) || maximumLateral <= PositionEpsilon)
            throw new InvalidDataException("Donor arm surface has no lateral extent.");
        float threshold = maximumLateral * 0.28f;
        Vector3[] arm = positions
            .Where(position => side * (position.X - centerX) >= threshold)
            .ToArray();
        if (arm.Length < 24)
            throw new InvalidDataException("Donor arm surface has too few bilateral landmarks.");

        (float yIntercept, float ySlope) = RobustLine(
            arm.Select(position => (
                side * (position.X - centerX),
                position.Y)));
        (float zIntercept, float zSlope) = RobustLine(
            arm.Select(position => (
                side * (position.X - centerX),
                position.Z)));
        float shoulderLateral = side * (shoulder.X - centerX);
        float desiredLateral = MathF.Min(
            Quantile(arm.Select(position => side * (position.X - centerX)), 0.92f),
            shoulderLateral + reach);
        if (desiredLateral <= shoulderLateral + reach * 0.35f)
            throw new InvalidDataException(
                "Donor torso-and-arms surface does not extend far enough from the target shoulder.");
        Vector3 desired = new(
            centerX + side * desiredLateral,
            yIntercept + ySlope * desiredLateral,
            zIntercept + zSlope * desiredLateral);
        Vector3 direction = desired - shoulder;
        if (!IsFinite(direction) || direction.LengthSquared() <= PositionEpsilon)
            throw new InvalidDataException("Donor arm landmarks produced a degenerate direction.");
        direction = Vector3.Normalize(direction);
        if (side * direction.X <= 0.4f)
            throw new InvalidDataException(
                "Donor arm landmarks do not describe a sufficiently lateral humanoid arm.");
        return direction;
    }

    private static Vector3 ExtractLegDirection(
        IReadOnlyList<Vector3> positions,
        Vector3 hip,
        float reach,
        int side)
    {
        (Vector3 minimum, Vector3 maximum) = Bounds(positions);
        float desiredY = Math.Clamp(
            hip.Y - reach,
            minimum.Y + (maximum.Y - minimum.Y) * 0.06f,
            maximum.Y - (maximum.Y - minimum.Y) * 0.18f);
        float band = MathF.Max((maximum.Y - minimum.Y) * 0.08f, reach * 0.035f);
        Vector3[] candidates = positions
            .Where(position =>
                side * (position.X - BodyCenterXFromHips(hip, side)) > 0 &&
                MathF.Abs(position.Y - desiredY) <= band)
            .ToArray();
        if (candidates.Length < 6)
        {
            candidates = positions
                .Where(position =>
                    side * (position.X - BodyCenterXFromHips(hip, side)) > 0 &&
                    position.Y <= minimum.Y + (maximum.Y - minimum.Y) * 0.35f)
                .ToArray();
        }
        if (candidates.Length < 6)
            throw new InvalidDataException("Donor lower body has too few leg landmarks.");
        Vector3 desired = new(
            Median(candidates.Select(position => position.X)),
            desiredY,
            Median(candidates.Select(position => position.Z)));
        Vector3 direction = desired - hip;
        if (!IsFinite(direction) || direction.LengthSquared() <= PositionEpsilon)
            throw new InvalidDataException("Donor leg landmarks produced a degenerate direction.");
        direction = Vector3.Normalize(direction);
        if (direction.Y >= -0.45f)
            throw new InvalidDataException(
                "Donor lower-body landmarks do not describe a sufficiently downward leg.");
        return direction;
    }

    // Bloom's symmetric hips make centerX = hip.X - side * abs(hip.X).
    private static float BodyCenterXFromHips(Vector3 hip, int side) =>
        hip.X - side * MathF.Abs(hip.X);

    private static Vector3 ExtractTorsoDirection(
        IReadOnlyList<Vector3> positions,
        float centerX)
    {
        (Vector3 minimum, Vector3 maximum) = Bounds(positions);
        float halfWidth = MathF.Max(
            MathF.Abs(maximum.X - centerX),
            MathF.Abs(minimum.X - centerX));
        Vector3[] center = positions
            .Where(position => MathF.Abs(position.X - centerX) <= halfWidth * 0.18f)
            .ToArray();
        if (center.Length < 12)
            return Vector3.UnitY;
        float lowerCut = Quantile(center.Select(position => position.Y), 0.25f);
        float upperCut = Quantile(center.Select(position => position.Y), 0.75f);
        Vector3 lowerCenter = MedianPoint(center.Where(position => position.Y <= lowerCut));
        Vector3 upperCenter = MedianPoint(center.Where(position => position.Y >= upperCut));
        Vector3 direction = upperCenter - lowerCenter;
        direction.X = 0;
        if (!IsFinite(direction) || direction.Y <= PositionEpsilon)
            return Vector3.UnitY;
        direction = Vector3.Normalize(direction);
        return direction.Y >= 0.7f ? direction : Vector3.UnitY;
    }

    private static TargetRigBodyPoseParameters DeriveSeed(
        TargetRigDefinition rig,
        LimbTargets targets)
    {
        float armElevation = AverageMirroredArmAngle(
            targets.LeftArmDirection,
            targets.RightArmDirection,
            elevation: true);
        float armForward = AverageMirroredArmAngle(
            targets.LeftArmDirection,
            targets.RightArmDirection,
            elevation: false);
        float legSpread = 0.5f * (
            LegSpreadDegrees(targets.LeftLegDirection, 1) +
            LegSpreadDegrees(targets.RightLegDirection, -1));
        float torsoPitch = RadiansToDegrees(MathF.Atan2(
            targets.TorsoDirection.Z,
            targets.TorsoDirection.Y));
        return Clamp(new TargetRigBodyPoseParameters(
            armElevation,
            armForward,
            0,
            legSpread,
            0,
            torsoPitch));
    }

    private static float AverageMirroredArmAngle(
        Vector3 left,
        Vector3 right,
        bool elevation)
    {
        float Angle(Vector3 direction, int side)
        {
            float lateral = side * direction.X;
            return elevation
                ? RadiansToDegrees(MathF.Atan2(
                    direction.Y,
                    MathF.Sqrt(lateral * lateral + direction.Z * direction.Z)))
                : RadiansToDegrees(MathF.Atan2(direction.Z, lateral));
        }
        return 0.5f * (Angle(left, 1) + Angle(right, -1));
    }

    private static float LegSpreadDegrees(Vector3 direction, int side) =>
        RadiansToDegrees(MathF.Atan2(side * direction.X, -direction.Y));

    private static (
        TargetRigBodyPoseParameters Parameters,
        TargetRigFittingPoseSnapshot Pose,
        float Score) Optimize(
        TargetRigBodyPoseParameters seed,
        FitContext context,
        TargetRigAutomaticPoseFitOptions options)
    {
        TargetRigBodyPoseParameters bestParameters = seed;
        TargetRigFittingPoseSnapshot bestPose =
            TargetRigBodyPoseMapper.CreateSnapshot(context.Rig, bestParameters);
        float bestScore = Score(bestPose, context, options.TrimmedSurfaceFraction);
        float[] steps = [8, 8, 12, 4, 10, 5];
        for (int pass = 0; pass < options.OptimizationPasses; pass++)
        {
            for (int parameter = 0; parameter < steps.Length; parameter++)
            {
                foreach (float direction in new[] { -1f, 1f })
                {
                    TargetRigBodyPoseParameters candidate = Clamp(Adjust(
                        bestParameters,
                        parameter,
                        steps[parameter] * direction));
                    if (candidate == bestParameters)
                        continue;
                    TargetRigFittingPoseSnapshot candidatePose =
                        TargetRigBodyPoseMapper.CreateSnapshot(context.Rig, candidate);
                    float candidateScore = Score(
                        candidatePose,
                        context,
                        options.TrimmedSurfaceFraction);
                    if (candidateScore < bestScore)
                    {
                        bestParameters = candidate;
                        bestPose = candidatePose;
                        bestScore = candidateScore;
                    }
                }
            }
            for (int index = 0; index < steps.Length; index++)
                steps[index] *= 0.5f;
        }
        return (bestParameters, bestPose, bestScore);
    }

    private static float Score(
        TargetRigFittingPoseSnapshot pose,
        FitContext context,
        float trimmedFraction)
    {
        TargetRigDefinition rig = context.Rig;
        IReadOnlyList<Matrix4x4> world = pose.WorldMatrices;
        float directional = 0;
        directional += DirectionLoss(world, rig, "L_Bicep", "L_Hand", context.Targets.LeftArmDirection);
        directional += DirectionLoss(world, rig, "R_Bicep", "R_Hand", context.Targets.RightArmDirection);
        directional += DirectionLoss(world, rig, "L_Thigh", "L_Ankle", context.Targets.LeftLegDirection);
        directional += DirectionLoss(world, rig, "R_Thigh", "R_Ankle", context.Targets.RightLegDirection);
        directional += 0.5f * DirectionLoss(
            world, rig, "Spine_01", "Spine_02", context.Targets.TorsoDirection);
        directional += 0.75f * BendLoss(world, rig, "L_Bicep", "L_UpperArm", "L_Hand");
        directional += 0.75f * BendLoss(world, rig, "R_Bicep", "R_UpperArm", "R_Hand");
        directional += 0.5f * BendLoss(world, rig, "L_Thigh", "L_calf", "L_Ankle");
        directional += 0.5f * BendLoss(world, rig, "R_Thigh", "R_calf", "R_Ankle");

        Vector3[] posedSurface = PoseTargetSamples(
            context.TargetSurfaceSamples,
            pose,
            rig);
        float normalizationSquared = context.NormalizationLength * context.NormalizationLength;
        float[] distances = posedSurface
            .Select(position => context.DonorBodyPositions.Min(donor =>
                Vector3.DistanceSquared(position, donor)) / normalizationSquared)
            .Order()
            .ToArray();
        int keep = Math.Clamp(
            (int)MathF.Ceiling(distances.Length * trimmedFraction),
            1,
            distances.Length);
        float surface = distances.Take(keep).Average();
        return directional + surface * 0.35f;
    }

    private static float DirectionLoss(
        IReadOnlyList<Matrix4x4> world,
        TargetRigDefinition rig,
        string rootName,
        string endName,
        Vector3 desired)
    {
        Vector3 root = Translation(world[rig.GetJointIndex(rootName)]);
        Vector3 end = Translation(world[rig.GetJointIndex(endName)]);
        Vector3 actual = end - root;
        if (!IsFinite(actual) || actual.LengthSquared() <= PositionEpsilon)
            throw new InvalidOperationException(
                $"Posed target chain {rootName}->{endName} is degenerate.");
        return 1 - Math.Clamp(Vector3.Dot(Vector3.Normalize(actual), desired), -1, 1);
    }

    private static float BendLoss(
        IReadOnlyList<Matrix4x4> world,
        TargetRigDefinition rig,
        string rootName,
        string middleName,
        string endName)
    {
        Vector3 root = Translation(world[rig.GetJointIndex(rootName)]);
        Vector3 middle = Translation(world[rig.GetJointIndex(middleName)]);
        Vector3 end = Translation(world[rig.GetJointIndex(endName)]);
        Vector3 first = middle - root;
        Vector3 second = end - middle;
        if (!IsFinite(first) || !IsFinite(second) ||
            first.LengthSquared() <= PositionEpsilon ||
            second.LengthSquared() <= PositionEpsilon)
            throw new InvalidOperationException(
                $"Posed target chain {rootName}->{middleName}->{endName} is degenerate.");
        return 1 - Math.Clamp(
            Vector3.Dot(Vector3.Normalize(first), Vector3.Normalize(second)),
            -1,
            1);
    }

    private static Vector3[] BuildTargetSurfaceSamples(
        TargetRigDefinition rig,
        SmoExportScene scene,
        int maximumSamples)
    {
        var activeNames = new HashSet<string>(StringComparer.Ordinal)
        {
            "Pelvis", "Spine_01", "Spine_02", "Spine_03",
            "L_Clavicle", "L_Bicep", "L_UpperArm", "L_Hand",
            "R_Clavicle", "R_Bicep", "R_UpperArm", "R_Hand",
            "L_Thigh", "L_calf", "L_Ankle",
            "R_Thigh", "R_calf", "R_Ankle"
        };
        var candidates = new List<Vector3>();
        foreach (SmoExportMesh mesh in scene.Meshes)
        {
            if (mesh.SkinObjectIndex is not int skinObjectIndex ||
                mesh.BlendWeights.Length != mesh.Positions.Length ||
                mesh.JointIndices.Length != mesh.Positions.Length)
                continue;
            SmoExportSkin? skin = scene.Skins.FirstOrDefault(item =>
                item.ObjectIndex == skinObjectIndex);
            if (skin is null)
                continue;
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
                Vector4 weights = mesh.BlendWeights[vertex];
                Vector4 indices = mesh.JointIndices[vertex];
                int dominantSlot = DominantSlot(weights);
                int paletteIndex = checked((int)Get(indices, dominantSlot));
                if ((uint)paletteIndex >= (uint)skin.JointObjectIndices.Count)
                    continue;
                int objectIndex = skin.JointObjectIndices[paletteIndex];
                int rigIndex;
                try
                {
                    rigIndex = rig.GetJointIndexByObjectIndex(objectIndex);
                }
                catch (KeyNotFoundException)
                {
                    continue;
                }
                if (activeNames.Contains(rig.Joints[rigIndex].Name))
                    candidates.Add(mesh.Positions[vertex]);
            }
        }
        if (candidates.Count < 32)
            throw new InvalidDataException(
                "Target scene has too few skinned body vertices for automatic fitting.");
        if (candidates.Count <= maximumSamples)
            return candidates.ToArray();
        var result = new Vector3[maximumSamples];
        for (int sample = 0; sample < maximumSamples; sample++)
        {
            int index = (int)((long)sample * candidates.Count / maximumSamples);
            result[sample] = candidates[index];
        }
        return result;
    }

    private static Vector3[] PoseTargetSamples(
        IReadOnlyList<Vector3> samples,
        TargetRigFittingPoseSnapshot pose,
        TargetRigDefinition rig)
    {
        // Surface samples are used only as a low-weight, trimmed shape term.
        // Assign each sample to its nearest deform joint in canonical space;
        // semantic chain directions remain the dominant objective and exact
        // SMO weights are not duplicated in the public fit result.
        TargetRigJoint[] deform = rig.Joints.Where(joint => joint.IsDeformJoint).ToArray();
        var transforms = new Dictionary<int, Matrix4x4>();
        foreach (TargetRigJoint joint in deform)
        {
            if (!Matrix4x4.Invert(joint.BindWorldMatrix, out Matrix4x4 inverseBind))
                throw new InvalidDataException(
                    $"Target bind matrix for {joint.Name} is singular.");
            transforms[joint.JointIndex] = inverseBind * pose.WorldMatrices[joint.JointIndex];
        }
        var result = new Vector3[samples.Count];
        for (int index = 0; index < samples.Count; index++)
        {
            Vector3 sample = samples[index];
            TargetRigJoint nearest = deform.MinBy(joint =>
                Vector3.DistanceSquared(sample, Translation(joint.BindWorldMatrix)))!;
            result[index] = Vector3.Transform(sample, transforms[nearest.JointIndex]);
        }
        return result;
    }

    private static TargetRigBodyPoseParameters Adjust(
        TargetRigBodyPoseParameters value,
        int index,
        float delta) => index switch
        {
            0 => value with { ArmElevationDegrees = value.ArmElevationDegrees + delta },
            1 => value with { ArmForwardDegrees = value.ArmForwardDegrees + delta },
            2 => value with { ElbowBendDegrees = value.ElbowBendDegrees + delta },
            3 => value with { LegSpreadDegrees = value.LegSpreadDegrees + delta },
            4 => value with { KneeBendDegrees = value.KneeBendDegrees + delta },
            5 => value with { TorsoPitchDegrees = value.TorsoPitchDegrees + delta },
            _ => throw new ArgumentOutOfRangeException(nameof(index))
        };

    private static TargetRigBodyPoseParameters Clamp(TargetRigBodyPoseParameters value) =>
        value with
        {
            ArmElevationDegrees = Math.Clamp(value.ArmElevationDegrees, -85, 85),
            ArmForwardDegrees = Math.Clamp(value.ArmForwardDegrees, -75, 75),
            ElbowBendDegrees = Math.Clamp(value.ElbowBendDegrees, 0, 145),
            LegSpreadDegrees = Math.Clamp(value.LegSpreadDegrees, -20, 45),
            KneeBendDegrees = Math.Clamp(value.KneeBendDegrees, 0, 135),
            TorsoPitchDegrees = Math.Clamp(value.TorsoPitchDegrees, -45, 45)
        };

    private static (float Intercept, float Slope) RobustLine(
        IEnumerable<(float X, float Y)> values)
    {
        (float X, float Y)[] points = values
            .Where(point => float.IsFinite(point.X) && float.IsFinite(point.Y))
            .OrderBy(point => point.X)
            .ToArray();
        if (points.Length < 8)
            throw new InvalidDataException("Donor landmarks are insufficient for a robust line.");
        int binCount = Math.Clamp(points.Length / 8, 4, 12);
        var medians = new List<(float X, float Y)>();
        for (int bin = 0; bin < binCount; bin++)
        {
            int first = bin * points.Length / binCount;
            int end = (bin + 1) * points.Length / binCount;
            if (end <= first)
                continue;
            (float X, float Y)[] slice = points[first..end];
            medians.Add((
                Median(slice.Select(point => point.X)),
                Median(slice.Select(point => point.Y))));
        }
        float meanX = medians.Average(point => point.X);
        float meanY = medians.Average(point => point.Y);
        double numerator = 0;
        double denominator = 0;
        foreach ((float x, float y) in medians)
        {
            numerator += (x - meanX) * (y - meanY);
            denominator += (x - meanX) * (x - meanX);
        }
        if (!double.IsFinite(denominator) || denominator <= PositionEpsilon)
            throw new InvalidDataException("Donor landmarks have no measurable axis extent.");
        float slope = (float)(numerator / denominator);
        float intercept = meanY - slope * meanX;
        if (!float.IsFinite(slope) || !float.IsFinite(intercept))
            throw new InvalidDataException("Donor landmark regression is non-finite.");
        return (intercept, slope);
    }

    private static float Quantile(IEnumerable<float> values, float probability)
    {
        float[] sorted = values.Where(float.IsFinite).Order().ToArray();
        if (sorted.Length == 0)
            throw new InvalidDataException("Cannot compute a donor landmark quantile.");
        float position = probability * (sorted.Length - 1);
        int lower = (int)MathF.Floor(position);
        int upper = Math.Min(lower + 1, sorted.Length - 1);
        return float.Lerp(sorted[lower], sorted[upper], position - lower);
    }

    private static float Median(IEnumerable<float> values) => Quantile(values, 0.5f);

    private static Vector3 MedianPoint(IEnumerable<Vector3> values)
    {
        Vector3[] points = values.ToArray();
        if (points.Length == 0)
            throw new InvalidDataException("Cannot compute an empty donor landmark center.");
        return new Vector3(
            Median(points.Select(point => point.X)),
            Median(points.Select(point => point.Y)),
            Median(points.Select(point => point.Z)));
    }

    private static float ChainLength(
        TargetRigDefinition rig,
        string root,
        string middle,
        string end)
    {
        int rootIndex = rig.GetJointIndex(root);
        int middleIndex = rig.GetJointIndex(middle);
        int endIndex = rig.GetJointIndex(end);
        if (rig.Joints[middleIndex].ParentJointIndex != rootIndex ||
            rig.Joints[endIndex].ParentJointIndex != middleIndex)
            throw new InvalidDataException(
                $"Target chain {root}->{middle}->{end} is not direct.");
        return rig.Joints[middleIndex].BindLengthFromParent +
               rig.Joints[endIndex].BindLengthFromParent;
    }

    private static float BodyCenterX(TargetRigDefinition rig)
    {
        float left = Translation(rig.Joints[rig.GetJointIndex("L_Thigh")].BindWorldMatrix).X;
        float right = Translation(rig.Joints[rig.GetJointIndex("R_Thigh")].BindWorldMatrix).X;
        return (left + right) * 0.5f;
    }

    private static float ComputeNormalizationLength(TargetRigDefinition rig)
    {
        float top = Translation(rig.Joints[rig.GetJointIndex("Head")].BindWorldMatrix).Y;
        float bottom = MathF.Min(
            Translation(rig.Joints[rig.GetJointIndex("L_Ankle")].BindWorldMatrix).Y,
            Translation(rig.Joints[rig.GetJointIndex("R_Ankle")].BindWorldMatrix).Y);
        float length = top - bottom;
        if (!float.IsFinite(length) || length <= PositionEpsilon)
            throw new InvalidDataException("Target rig has no measurable humanoid height.");
        return length;
    }

    private static (Vector3 Minimum, Vector3 Maximum) Bounds(
        IReadOnlyList<Vector3> positions)
    {
        if (positions.Count == 0)
            throw new InvalidDataException("Cannot compute empty geometry bounds.");
        Vector3 minimum = positions[0];
        Vector3 maximum = positions[0];
        for (int index = 1; index < positions.Count; index++)
        {
            minimum = Vector3.Min(minimum, positions[index]);
            maximum = Vector3.Max(maximum, positions[index]);
        }
        return (minimum, maximum);
    }

    private static int CheckedIndex(ImportedMesh mesh, uint value, int meshIndex)
    {
        if (value >= mesh.Positions.Length)
            throw new InvalidDataException(
                $"Donor mesh [{meshIndex}] {mesh.Name} has an out-of-range triangle index.");
        return checked((int)value);
    }

    private static int DominantSlot(Vector4 weights)
    {
        int slot = 0;
        float largest = weights.X;
        for (int index = 1; index < 4; index++)
        {
            float value = Get(weights, index);
            if (value > largest)
            {
                largest = value;
                slot = index;
            }
        }
        return slot;
    }

    private static float Get(Vector4 value, int index) => index switch
    {
        0 => value.X,
        1 => value.Y,
        2 => value.Z,
        3 => value.W,
        _ => throw new ArgumentOutOfRangeException(nameof(index))
    };

    private static float RadiansToDegrees(float value) => value * 180 / MathF.PI;

    private static Vector3 Translation(Matrix4x4 value) =>
        new(value.M41, value.M42, value.M43);

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);

    private sealed class UnionFind
    {
        private readonly int[] _parents;
        private readonly byte[] _ranks;

        public UnionFind(int count)
        {
            _parents = Enumerable.Range(0, count).ToArray();
            _ranks = new byte[count];
        }

        public int Find(int value)
        {
            int root = value;
            while (_parents[root] != root)
                root = _parents[root];
            while (_parents[value] != value)
            {
                int next = _parents[value];
                _parents[value] = root;
                value = next;
            }
            return root;
        }

        public void Union(int left, int right)
        {
            int leftRoot = Find(left);
            int rightRoot = Find(right);
            if (leftRoot == rightRoot)
                return;
            if (_ranks[leftRoot] < _ranks[rightRoot])
            {
                _parents[leftRoot] = rightRoot;
            }
            else
            {
                _parents[rightRoot] = leftRoot;
                if (_ranks[leftRoot] == _ranks[rightRoot])
                    _ranks[leftRoot]++;
            }
        }
    }
}
