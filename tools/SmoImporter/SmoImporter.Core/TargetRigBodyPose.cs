using System.Numerics;

namespace SmoImporter.Core;

/// <summary>
/// Absolute, symmetric controls for the small set of body motions needed to
/// fit the immutable game rig inside another humanoid. Angles describe limb
/// endpoint directions rather than Euler rotations of implementation-specific
/// bones: zero arm elevation is a horizontal T pose, zero leg spread is a
/// vertical leg, and zero bend is a straight two-bone chain. NeckForward is a
/// sagittal rotation relative to the neck direction inherited from the torso;
/// positive values move the head forward along external-space +Z while a Head
/// counter-rotation preserves its previous world-facing direction.
/// </summary>
public sealed record TargetRigBodyPoseParameters(
    float ArmElevationDegrees,
    float ArmForwardDegrees,
    float ElbowBendDegrees,
    float LegSpreadDegrees,
    float KneeBendDegrees,
    float TorsoPitchDegrees,
    float NeckForward = 0)
{
    public static TargetRigBodyPoseParameters Neutral { get; } =
        new(0, 0, 0, 0, 0, 0, 0);
}

/// <summary>
/// Stable editor conversion for the X/Y/Z sliders used by the joint mode.
/// The quaternion convention is the same as
/// <see cref="Quaternion.CreateFromYawPitchRoll(float, float, float)"/>:
/// Y is yaw, X is pitch, and Z is roll. At gimbal lock the equivalent
/// representation with zero roll is chosen so that a UI round-trip preserves
/// the rotation instead of jumping to an unrelated orientation.
/// </summary>
public static class TargetRigEulerAngles
{
    private const float DegreesToRadians = MathF.PI / 180;
    private const float RadiansToDegrees = 180 / MathF.PI;
    private const float GimbalLockThreshold = 0.999999f;

    public static Quaternion ToQuaternion(Vector3 degrees)
    {
        if (!IsFinite(degrees))
            throw new ArgumentException("Euler angles must be finite.", nameof(degrees));
        Quaternion value = Quaternion.CreateFromYawPitchRoll(
            degrees.Y * DegreesToRadians,
            degrees.X * DegreesToRadians,
            degrees.Z * DegreesToRadians);
        return Normalize(value);
    }

    public static Vector3 FromQuaternion(Quaternion value)
    {
        Quaternion rotation = Normalize(value);
        Matrix4x4 matrix = Matrix4x4.CreateFromQuaternion(rotation);
        float sinPitch = Math.Clamp(-matrix.M32, -1f, 1f);
        float pitch = MathF.Asin(sinPitch);
        float yaw;
        float roll;
        if (MathF.Abs(sinPitch) < GimbalLockThreshold)
        {
            yaw = MathF.Atan2(matrix.M31, matrix.M33);
            roll = MathF.Atan2(matrix.M12, matrix.M22);
        }
        else
        {
            // At +/-90 degrees only yaw-roll (or yaw+roll) is observable.
            // Choosing zero roll and recovering the combined yaw from M11/M13
            // produces an equivalent rotation for both signs of pitch.
            yaw = MathF.Atan2(-matrix.M13, matrix.M11);
            roll = 0;
        }
        return new Vector3(
            NormalizeDegrees(pitch * RadiansToDegrees),
            NormalizeDegrees(yaw * RadiansToDegrees),
            NormalizeDegrees(roll * RadiansToDegrees));
    }

    private static Quaternion Normalize(Quaternion value)
    {
        if (!TargetRigDefinition.IsFinite(value) ||
            value.LengthSquared() <= 0.000001f)
        {
            throw new ArgumentException(
                "Joint rotation must be finite and non-degenerate.",
                nameof(value));
        }
        return Quaternion.Normalize(value);
    }

    private static float NormalizeDegrees(float value)
    {
        float normalized = MathF.IEEERemainder(value, 360f);
        return normalized == -180f ? 180f : normalized;
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) &&
        float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);
}

/// <summary>
/// Maps high-level symmetric humanoid controls to the exact Bloom target rig.
/// The mapper uses world-space two-bone IK and writes only local rotation
/// deltas. Bind translations, scales, hierarchy, and bone lengths remain
/// immutable and are revalidated by <see cref="TargetRigFittingPose.Capture"/>.
/// </summary>
public static class TargetRigBodyPoseMapper
{
    private const float DirectionEpsilon = 0.000001f;
    private const float SymmetryTolerance = 0.001f;

    private sealed record LimbChain(
        int Root,
        int Middle,
        int End,
        float FirstLength,
        float SecondLength);

    public static TargetRigFittingPose CreatePose(
        TargetRigDefinition rig,
        TargetRigBodyPoseParameters parameters)
    {
        ArgumentNullException.ThrowIfNull(rig);
        ArgumentNullException.ThrowIfNull(parameters);
        ValidateParameters(parameters);

        LimbChain leftArm = ResolveChain(
            rig, "L_Bicep", "L_UpperArm", "L_Hand");
        LimbChain rightArm = ResolveChain(
            rig, "R_Bicep", "R_UpperArm", "R_Hand");
        LimbChain leftLeg = ResolveChain(
            rig, "L_Thigh", "L_calf", "L_Ankle");
        LimbChain rightLeg = ResolveChain(
            rig, "R_Thigh", "R_calf", "R_Ankle");
        ValidateSymmetricLengths(leftArm, rightArm, "arms");
        ValidateSymmetricLengths(leftLeg, rightLeg, "legs");
        ValidateSegment(rig, "Spine_01", "Spine_02");

        TargetRigFittingPose pose = rig.CreateFittingPose();

        float torsoPitch = DegreesToRadians(parameters.TorsoPitchDegrees);
        Vector3 torsoDirection = Vector3.Normalize(
            Vector3.UnitY * MathF.Cos(torsoPitch) +
            Vector3.UnitZ * MathF.Sin(torsoPitch));
        AimSegment(pose, rig.GetJointIndex("Spine_01"),
            rig.GetJointIndex("Spine_02"), torsoDirection);

        PoseNeck(pose, rig, DegreesToRadians(parameters.NeckForward));

        float armElevation = DegreesToRadians(parameters.ArmElevationDegrees);
        float armForward = DegreesToRadians(parameters.ArmForwardDegrees);
        float elbowBend = DegreesToRadians(parameters.ElbowBendDegrees);
        PoseArm(pose, leftArm, side: 1, armElevation, armForward, elbowBend);
        PoseArm(pose, rightArm, side: -1, armElevation, armForward, elbowBend);

        float legSpread = DegreesToRadians(parameters.LegSpreadDegrees);
        float kneeBend = DegreesToRadians(parameters.KneeBendDegrees);
        PoseLeg(pose, leftLeg, side: 1, legSpread, kneeBend);
        PoseLeg(pose, rightLeg, side: -1, legSpread, kneeBend);

        // Capture performs the authoritative hierarchy and fixed-length check.
        _ = pose.Capture();
        return pose;
    }

    public static TargetRigFittingPoseSnapshot CreateSnapshot(
        TargetRigDefinition rig,
        TargetRigBodyPoseParameters parameters) =>
        CreatePose(rig, parameters).Capture();

    /// <summary>
    /// Replaces the high-level humanoid contribution while preserving every
    /// per-joint correction already present in <paramref name="effectivePose"/>.
    /// This is the shared state transition used when the editor switches from
    /// individual joints back to humanoid controls: switching modes alone does
    /// not mutate the pose, and changing a humanoid control keeps the manual
    /// correction relative to the previous humanoid pose.
    /// </summary>
    public static TargetRigFittingPoseSnapshot RebasePreservingCorrections(
        TargetRigDefinition rig,
        TargetRigFittingPoseSnapshot effectivePose,
        TargetRigBodyPoseParameters oldParameters,
        TargetRigBodyPoseParameters newParameters,
        TargetRigFittingPoseSnapshot? exactNewHumanPose = null)
    {
        ArgumentNullException.ThrowIfNull(rig);
        ArgumentNullException.ThrowIfNull(effectivePose);
        ArgumentNullException.ThrowIfNull(oldParameters);
        ArgumentNullException.ThrowIfNull(newParameters);

        if (!ReferenceEquals(effectivePose.Definition, rig))
        {
            throw new InvalidOperationException(
                "The effective pose belongs to another target rig.");
        }
        if (exactNewHumanPose is null && oldParameters == newParameters)
            return effectivePose;

        TargetRigFittingPoseSnapshot oldHumanPose =
            CreateSnapshot(rig, oldParameters);
        TargetRigFittingPoseSnapshot newHumanPose = exactNewHumanPose ??
            CreateSnapshot(rig, newParameters);
        if (!ReferenceEquals(newHumanPose.Definition, rig) ||
            effectivePose.LocalRotationDeltas.Count != rig.Joints.Count ||
            oldHumanPose.LocalRotationDeltas.Count != rig.Joints.Count ||
            newHumanPose.LocalRotationDeltas.Count != rig.Joints.Count)
        {
            throw new InvalidOperationException(
                "A humanoid pose belongs to another target rig or has an invalid joint count.");
        }

        TargetRigFittingPose rebased = rig.CreateFittingPose();
        for (int jointIndex = 0; jointIndex < rig.Joints.Count; jointIndex++)
        {
            // Effective = correction * oldHuman. Replacing oldHuman with
            // newHuman must therefore retain correction on the left.
            Quaternion correction = NormalizeRotation(
                effectivePose.LocalRotationDeltas[jointIndex] *
                Quaternion.Inverse(oldHumanPose.LocalRotationDeltas[jointIndex]));
            rebased.SetLocalRotationDelta(
                jointIndex,
                NormalizeRotation(
                    correction * newHumanPose.LocalRotationDeltas[jointIndex]));
        }
        rebased.SetRootTransform(
            effectivePose.RootRotation,
            effectivePose.RootTranslation);
        return rebased.Capture();
    }

    private static void PoseArm(
        TargetRigFittingPose pose,
        LimbChain chain,
        int side,
        float elevation,
        float forward,
        float bend)
    {
        Vector3 lateral = side > 0 ? Vector3.UnitX : -Vector3.UnitX;
        Vector3 horizontal = Vector3.Normalize(
            lateral * MathF.Cos(forward) + Vector3.UnitZ * MathF.Sin(forward));
        Vector3 direction = Vector3.Normalize(
            horizontal * MathF.Cos(elevation) + Vector3.UnitY * MathF.Sin(elevation));
        PoseTwoBoneChain(
            pose,
            chain,
            direction,
            bend,
            -Vector3.UnitZ);
    }

    private static void PoseLeg(
        TargetRigFittingPose pose,
        LimbChain chain,
        int side,
        float spread,
        float bend)
    {
        Vector3 lateral = side > 0 ? Vector3.UnitX : -Vector3.UnitX;
        Vector3 direction = Vector3.Normalize(
            -Vector3.UnitY * MathF.Cos(spread) + lateral * MathF.Sin(spread));
        PoseTwoBoneChain(
            pose,
            chain,
            direction,
            bend,
            Vector3.UnitZ);
    }

    private static void PoseNeck(
        TargetRigFittingPose pose,
        TargetRigDefinition rig,
        float forward)
    {
        // Zero is deliberately a no-op. Besides avoiding needless floating-point
        // churn, this keeps six-argument callers bitwise compatible with the pose
        // they produced before the optional neck control was introduced.
        if (forward == 0)
            return;

        int neck = ResolveExactDeformJoint(rig, "Neck");
        int head = ResolveExactDeformJoint(rig, "Head");
        ValidateDirectParent(rig, neck, head);
        IReadOnlyList<Matrix4x4> currentWorld = pose.ComputeWorldMatrices();
        Matrix4x4 baselineHeadWorldRotation = ExtractRotation(
            currentWorld[head], "Head baseline world transform");
        Vector3 inheritedDirection =
            Translation(currentWorld[head]) - Translation(currentWorld[neck]);
        if (!IsFinite(inheritedDirection) ||
            inheritedDirection.LengthSquared() <= DirectionEpsilon * DirectionEpsilon)
        {
            throw new InvalidDataException(
                "Target neck has a degenerate inherited direction.");
        }

        // System.Numerics uses row vectors. A positive X rotation maps +Y toward
        // +Z, which is the mapper's established forward direction.
        Vector3 desiredDirection = Vector3.TransformNormal(
            Vector3.Normalize(inheritedDirection),
            Matrix4x4.CreateRotationX(forward));
        AimSegment(pose, neck, head, Vector3.Normalize(desiredDirection));

        // Rotating Neck moves the Head joint to the requested sagittal
        // position, but must not make the character look up or down. Head is a
        // direct child, so counter-rotate it in its bind-local axes until its
        // world orientation is exactly the one it had before NeckForward.
        //
        // System.Numerics matrices use row vectors here:
        //   headWorld = headDelta * headBind * neckWorld
        // therefore:
        //   headDelta = baselineHeadWorld * inverse(neckWorld) * inverse(headBind)
        IReadOnlyList<Matrix4x4> afterNeck = pose.ComputeWorldMatrices();
        Matrix4x4 neckWorldRotation = ExtractRotation(
            afterNeck[neck], "posed Neck world transform");
        Matrix4x4 bindHeadRotation = Matrix4x4.CreateFromQuaternion(
            rig.Joints[head].BindLocalRotation);
        Matrix4x4 counterRotationMatrix =
            baselineHeadWorldRotation *
            Matrix4x4.Transpose(neckWorldRotation) *
            Matrix4x4.Transpose(bindHeadRotation);
        Quaternion counterRotation = NormalizeRotation(
            Quaternion.CreateFromRotationMatrix(counterRotationMatrix));
        pose.SetLocalRotationDelta(head, counterRotation);
    }

    private static Matrix4x4 ExtractRotation(
        Matrix4x4 transform,
        string label)
    {
        if (!Matrix4x4.Decompose(
                transform,
                out Vector3 scale,
                out Quaternion rotation,
                out Vector3 translation) ||
            !IsFinite(scale) ||
            !TargetRigDefinition.IsFinite(rotation) ||
            !IsFinite(translation) ||
            rotation.LengthSquared() <= DirectionEpsilon * DirectionEpsilon)
        {
            throw new InvalidDataException(
                $"{label} cannot be decomposed into a finite rotation.");
        }
        return Matrix4x4.CreateFromQuaternion(Quaternion.Normalize(rotation));
    }

    private static void PoseTwoBoneChain(
        TargetRigFittingPose pose,
        LimbChain chain,
        Vector3 endpointDirection,
        float bend,
        Vector3 preferredBendDirection)
    {
        IReadOnlyList<Matrix4x4> initialWorld = pose.ComputeWorldMatrices();
        Vector3 root = Translation(initialWorld[chain.Root]);
        float first = chain.FirstLength;
        float second = chain.SecondLength;
        float reachSquared = first * first + second * second +
                             2 * first * second * MathF.Cos(bend);
        float reach = MathF.Sqrt(MathF.Max(reachSquared, DirectionEpsilon));
        float along = (first * first + reach * reach - second * second) /
                      (2 * reach);
        float perpendicular = MathF.Sqrt(MathF.Max(
            first * first - along * along, 0));
        Vector3 bendDirection = ProjectPerpendicular(
            preferredBendDirection, endpointDirection);
        Vector3 desiredMiddle = root + endpointDirection * along +
                                bendDirection * perpendicular;
        Vector3 desiredEnd = root + endpointDirection * reach;

        AimSegment(
            pose,
            chain.Root,
            chain.Middle,
            Vector3.Normalize(desiredMiddle - root));
        IReadOnlyList<Matrix4x4> afterRoot = pose.ComputeWorldMatrices();
        Vector3 actualMiddle = Translation(afterRoot[chain.Middle]);
        Vector3 middleToEnd = desiredEnd - actualMiddle;
        if (!IsFinite(middleToEnd) ||
            middleToEnd.LengthSquared() <= DirectionEpsilon * DirectionEpsilon)
        {
            throw new InvalidOperationException(
                "Two-bone IK produced a degenerate middle-to-end direction.");
        }
        AimSegment(
            pose,
            chain.Middle,
            chain.End,
            Vector3.Normalize(middleToEnd));
    }

    private static void AimSegment(
        TargetRigFittingPose pose,
        int jointIndex,
        int childIndex,
        Vector3 desiredWorldDirection)
    {
        TargetRigDefinition rig = pose.Definition;
        TargetRigJoint joint = rig.Joints[jointIndex];
        TargetRigJoint child = rig.Joints[childIndex];
        if (child.ParentJointIndex != jointIndex)
        {
            throw new InvalidDataException(
                $"Target chain {joint.Name}->{child.Name} is not a direct hierarchy edge.");
        }
        if (!IsFinite(desiredWorldDirection) ||
            desiredWorldDirection.LengthSquared() <= DirectionEpsilon * DirectionEpsilon)
        {
            throw new ArgumentException(
                $"Desired direction for {joint.Name}->{child.Name} is invalid.",
                nameof(desiredWorldDirection));
        }

        IReadOnlyList<Matrix4x4> currentWorld = pose.ComputeWorldMatrices();
        Matrix4x4 parentWorld = joint.ParentJointIndex >= 0
            ? currentWorld[joint.ParentJointIndex]
            : Matrix4x4.Identity;
        Matrix4x4 bindThenParent =
            Matrix4x4.CreateFromQuaternion(joint.BindLocalRotation) * parentWorld;
        if (!Matrix4x4.Invert(bindThenParent, out Matrix4x4 inverseBasis) ||
            !TargetRigDefinition.IsFinite(inverseBasis))
        {
            throw new InvalidDataException(
                $"Target joint {joint.Name} has no invertible aiming basis.");
        }

        Vector3 from = child.BindLocalTranslation;
        Vector3 to = Vector3.TransformNormal(
            Vector3.Normalize(desiredWorldDirection), inverseBasis);
        if (!IsFinite(from) || !IsFinite(to) ||
            from.LengthSquared() <= DirectionEpsilon * DirectionEpsilon ||
            to.LengthSquared() <= DirectionEpsilon * DirectionEpsilon)
        {
            throw new InvalidDataException(
                $"Target segment {joint.Name}->{child.Name} has a degenerate aiming vector.");
        }
        pose.SetLocalRotationDelta(
            jointIndex,
            RotationBetween(Vector3.Normalize(from), Vector3.Normalize(to)));
    }

    private static Quaternion RotationBetween(Vector3 from, Vector3 to)
    {
        float dot = Math.Clamp(Vector3.Dot(from, to), -1, 1);
        if (dot >= 1 - 0.000001f)
            return Quaternion.Identity;
        if (dot <= -1 + 0.000001f)
        {
            Vector3 helper = MathF.Abs(from.X) < 0.8f
                ? Vector3.UnitX
                : Vector3.UnitY;
            Vector3 axis = Vector3.Normalize(Vector3.Cross(from, helper));
            return Quaternion.CreateFromAxisAngle(axis, MathF.PI);
        }
        Vector3 cross = Vector3.Cross(from, to);
        Quaternion result = Quaternion.Normalize(new Quaternion(
            cross,
            1 + dot));
        if (!TargetRigDefinition.IsFinite(result))
            throw new InvalidOperationException("A target IK rotation is non-finite.");
        return result;
    }

    private static Quaternion NormalizeRotation(Quaternion value)
    {
        if (!TargetRigDefinition.IsFinite(value) ||
            value.LengthSquared() <= DirectionEpsilon * DirectionEpsilon)
        {
            throw new InvalidOperationException(
                "A target joint correction is non-finite or degenerate.");
        }
        return Quaternion.Normalize(value);
    }

    private static Vector3 ProjectPerpendicular(Vector3 value, Vector3 normal)
    {
        Vector3 projected = value - normal * Vector3.Dot(value, normal);
        if (projected.LengthSquared() <= DirectionEpsilon * DirectionEpsilon)
        {
            Vector3 helper = MathF.Abs(normal.Y) < 0.8f
                ? Vector3.UnitY
                : Vector3.UnitX;
            projected = helper - normal * Vector3.Dot(helper, normal);
        }
        if (!IsFinite(projected) ||
            projected.LengthSquared() <= DirectionEpsilon * DirectionEpsilon)
            throw new InvalidOperationException("A target IK bend plane is degenerate.");
        return Vector3.Normalize(projected);
    }

    private static LimbChain ResolveChain(
        TargetRigDefinition rig,
        string rootName,
        string middleName,
        string endName)
    {
        int root = ResolveExactDeformJoint(rig, rootName);
        int middle = ResolveExactDeformJoint(rig, middleName);
        int end = ResolveExactDeformJoint(rig, endName);
        ValidateDirectParent(rig, root, middle);
        ValidateDirectParent(rig, middle, end);
        float first = rig.Joints[middle].BindLengthFromParent;
        float second = rig.Joints[end].BindLengthFromParent;
        if (!float.IsFinite(first) || !float.IsFinite(second) ||
            first <= DirectionEpsilon || second <= DirectionEpsilon)
        {
            throw new InvalidDataException(
                $"Target chain {rootName}->{middleName}->{endName} has a zero or invalid length.");
        }
        return new LimbChain(root, middle, end, first, second);
    }

    private static void ValidateSegment(
        TargetRigDefinition rig,
        string jointName,
        string childName)
    {
        int joint = ResolveExactDeformJoint(rig, jointName);
        int child = ResolveExactDeformJoint(rig, childName);
        ValidateDirectParent(rig, joint, child);
    }

    private static int ResolveExactDeformJoint(
        TargetRigDefinition rig,
        string expectedName)
    {
        int index;
        try
        {
            index = rig.GetJointIndex(expectedName);
        }
        catch (KeyNotFoundException exception)
        {
            throw new InvalidDataException(
                $"Target rig does not contain required humanoid joint '{expectedName}'.",
                exception);
        }
        TargetRigJoint joint = rig.Joints[index];
        if (!string.Equals(joint.Name, expectedName, StringComparison.Ordinal) ||
            !joint.IsDeformJoint)
        {
            throw new InvalidDataException(
                $"Target joint '{expectedName}' is not an exact deform-joint match.");
        }
        return index;
    }

    private static void ValidateDirectParent(
        TargetRigDefinition rig,
        int parent,
        int child)
    {
        if (rig.Joints[child].ParentJointIndex != parent)
        {
            throw new InvalidDataException(
                $"Required target chain edge {rig.Joints[parent].Name}->" +
                $"{rig.Joints[child].Name} is not direct.");
        }
    }

    private static void ValidateSymmetricLengths(
        LimbChain left,
        LimbChain right,
        string label)
    {
        float scale = MathF.Max(
            1,
            MathF.Max(
                MathF.Max(left.FirstLength, right.FirstLength),
                MathF.Max(left.SecondLength, right.SecondLength)));
        if (MathF.Abs(left.FirstLength - right.FirstLength) >
                SymmetryTolerance * scale ||
            MathF.Abs(left.SecondLength - right.SecondLength) >
                SymmetryTolerance * scale)
        {
            throw new InvalidDataException(
                $"Target {label} are not length-symmetric enough for mirrored controls.");
        }
    }

    private static void ValidateParameters(TargetRigBodyPoseParameters value)
    {
        ValidateAngle(value.ArmElevationDegrees, -85, 85, nameof(value.ArmElevationDegrees));
        ValidateAngle(value.ArmForwardDegrees, -75, 75, nameof(value.ArmForwardDegrees));
        ValidateAngle(value.ElbowBendDegrees, 0, 145, nameof(value.ElbowBendDegrees));
        ValidateAngle(value.LegSpreadDegrees, -20, 45, nameof(value.LegSpreadDegrees));
        ValidateAngle(value.KneeBendDegrees, 0, 135, nameof(value.KneeBendDegrees));
        ValidateAngle(value.TorsoPitchDegrees, -45, 45, nameof(value.TorsoPitchDegrees));
        ValidateAngle(value.NeckForward, -45, 45, nameof(value.NeckForward));
    }

    private static void ValidateAngle(
        float value,
        float minimum,
        float maximum,
        string name)
    {
        if (!float.IsFinite(value) || value < minimum || value > maximum)
        {
            throw new ArgumentOutOfRangeException(
                name,
                value,
                $"Angle must be finite and inside [{minimum}, {maximum}] degrees.");
        }
    }

    private static float DegreesToRadians(float value) => value * MathF.PI / 180;

    private static Vector3 Translation(Matrix4x4 value) =>
        new(value.M41, value.M42, value.M43);

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);
}
