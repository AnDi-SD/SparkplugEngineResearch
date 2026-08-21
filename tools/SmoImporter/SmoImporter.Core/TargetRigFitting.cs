using System.Collections.ObjectModel;
using System.Numerics;
using System.Security.Cryptography;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// One node in the immutable target fitting rig. Matrices use the same external
/// right-handed space as the importer/exporter (the SMO Z axis is reflected).
/// </summary>
public sealed record TargetRigJoint(
    int JointIndex,
    string Name,
    int ObjectIndex,
    int ParentJointIndex,
    bool IsDeformJoint,
    Matrix4x4 BindWorldMatrix,
    Matrix4x4 BindLocalMatrix,
    Vector3 BindLocalScale,
    Quaternion BindLocalRotation,
    Vector3 BindLocalTranslation,
    float BindLengthFromParent);

/// <summary>
/// Immutable, read-only definition of the target game's deformation rig. It is
/// derived from confirmed skin palettes and uses the same parent convention as
/// <c>SmoSceneBuilder</c>: one unambiguous logical <c>esfNodeChild</c> parent wins,
/// otherwise the validated serializer parent is used as the compatibility fallback.
/// </summary>
public sealed class TargetRigDefinition
{
    private const float MatrixTolerance = 0.0001f;
    private const float ScaleUniformityTolerance = 0.0001f;
    private static readonly Matrix4x4 ExternalSpaceReflection =
        Matrix4x4.CreateScale(1, 1, -1);

    private readonly IReadOnlyDictionary<string, int> _jointIndicesByName;
    private readonly IReadOnlyDictionary<int, int> _jointIndicesByObjectIndex;
    private readonly Guid _instanceIdentity = Guid.NewGuid();

    private TargetRigDefinition(
        IReadOnlyList<TargetRigJoint> joints,
        IReadOnlyDictionary<string, int> jointIndicesByName,
        IReadOnlyDictionary<int, int> jointIndicesByObjectIndex,
        string sourceFingerprint)
    {
        Joints = joints;
        _jointIndicesByName = jointIndicesByName;
        _jointIndicesByObjectIndex = jointIndicesByObjectIndex;
        SourceFingerprint = sourceFingerprint;
    }

    public IReadOnlyList<TargetRigJoint> Joints { get; }

    /// <summary>
    /// SHA-256 of the immutable source SMO bytes used to build this rig. It lets
    /// pose consumers reject a valid pose captured for another target model.
    /// </summary>
    public string SourceFingerprint { get; }

    public int DeformJointCount => Joints.Count(joint => joint.IsDeformJoint);

    internal Guid InstanceIdentity => _instanceIdentity;

    public static TargetRigDefinition FromSmoDocument(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        if (document.HasErrors)
            throw new InvalidDataException(
                "Target SMO contains structural errors and cannot define a fitting rig.");

        HashSet<int> deformObjectIndices = ReadDeformObjectIndices(document);
        SmoNodeHierarchy hierarchy = SmoNodeHierarchy.Decode(document);
        Dictionary<int, int?> logicalParents = BuildRequiredNodeForest(
            document, hierarchy, deformObjectIndices);
        int[] orderedObjectIndices = TopologicalOrder(logicalParents);

        ValidateUniqueNames(document, orderedObjectIndices);
        IReadOnlyDictionary<int, Matrix4x4> canonicalSmoBindWorld =
            SmoSkinBindingResolver.ResolveBindWorldMatrices(document);
        foreach (int objectIndex in deformObjectIndices)
        {
            if (!canonicalSmoBindWorld.TryGetValue(
                    objectIndex, out Matrix4x4 bindWorld) || !IsFinite(bindWorld))
            {
                throw new InvalidDataException(
                    $"Deform node [{objectIndex}] {document.Objects[objectIndex].Name} " +
                    "has no unique finite canonical bind-world matrix.");
            }
        }

        Dictionary<int, Matrix4x4> smoWorld = ResolveSmoWorldMatrices(
            document, orderedObjectIndices, logicalParents, canonicalSmoBindWorld);
        Dictionary<int, Matrix4x4> externalWorld = smoWorld.ToDictionary(
            pair => pair.Key,
            pair => ToExternalSpace(pair.Value));
        var jointIndexByObject = orderedObjectIndices
            .Select((objectIndex, jointIndex) => (objectIndex, jointIndex))
            .ToDictionary(pair => pair.objectIndex, pair => pair.jointIndex);

        var joints = new TargetRigJoint[orderedObjectIndices.Length];
        for (int jointIndex = 0; jointIndex < joints.Length; jointIndex++)
        {
            int objectIndex = orderedObjectIndices[jointIndex];
            int parentJointIndex = logicalParents[objectIndex] is int parentObjectIndex
                ? jointIndexByObject[parentObjectIndex]
                : -1;
            Matrix4x4 bindWorld = externalWorld[objectIndex];
            Matrix4x4 bindLocal = bindWorld;
            if (parentJointIndex >= 0)
            {
                Matrix4x4 parentWorld = joints[parentJointIndex].BindWorldMatrix;
                if (!Matrix4x4.Invert(parentWorld, out Matrix4x4 inverseParent) ||
                    !IsFinite(inverseParent))
                {
                    throw new InvalidDataException(
                        $"Parent bind matrix for node [{objectIndex}] cannot be inverted.");
                }
                bindLocal = bindWorld * inverseParent;
            }

            if (!Matrix4x4.Decompose(
                    bindLocal,
                    out Vector3 bindScale,
                    out Quaternion bindRotation,
                    out Vector3 bindTranslation) ||
                !IsFinite(bindScale) || !IsFinite(bindRotation) ||
                !IsFinite(bindTranslation) ||
                bindRotation.LengthSquared() <= 0.000001f)
            {
                throw new InvalidDataException(
                    $"Bind-local matrix for node [{objectIndex}] " +
                    $"{document.Objects[objectIndex].Name} cannot be decomposed safely.");
            }
            bindRotation = Quaternion.Normalize(bindRotation);
            if (!IsUniformScale(bindScale))
            {
                throw new InvalidDataException(
                    $"Bind-local matrix for node [{objectIndex}] " +
                    $"{document.Objects[objectIndex].Name} has non-uniform scale " +
                    $"({bindScale.X:G9}, {bindScale.Y:G9}, {bindScale.Z:G9}). " +
                    "A rotation-only fitting pose cannot preserve world-space " +
                    "bone lengths through a non-uniformly scaled hierarchy.");
            }
            Matrix4x4 reconstructed = ComposeLocal(
                bindScale, bindRotation, Quaternion.Identity, bindTranslation);
            if (!ApproximatelyEqual(bindLocal, reconstructed, MatrixTolerance))
            {
                throw new InvalidDataException(
                    $"Bind-local matrix for node [{objectIndex}] " +
                    $"{document.Objects[objectIndex].Name} contains shear or another " +
                    "component that a rotation-only fitting pose cannot preserve.");
            }

            float bindLength = parentJointIndex < 0
                ? 0
                : Vector3.Distance(
                    Translation(joints[parentJointIndex].BindWorldMatrix),
                    Translation(bindWorld));
            if (!float.IsFinite(bindLength))
                throw new InvalidDataException(
                    $"Bind length for node [{objectIndex}] is not finite.");
            joints[jointIndex] = new TargetRigJoint(
                jointIndex,
                document.Objects[objectIndex].Name,
                objectIndex,
                parentJointIndex,
                deformObjectIndices.Contains(objectIndex),
                bindWorld,
                bindLocal,
                bindScale,
                bindRotation,
                bindTranslation,
                bindLength);
        }

        var byName = joints.ToDictionary(
            joint => joint.Name,
            joint => joint.JointIndex,
            StringComparer.OrdinalIgnoreCase);
        return new TargetRigDefinition(
            Array.AsReadOnly(joints),
            new ReadOnlyDictionary<string, int>(byName),
            new ReadOnlyDictionary<int, int>(jointIndexByObject),
            ComputeSourceFingerprint(document));
    }

    public int GetJointIndex(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        return _jointIndicesByName.TryGetValue(name, out int jointIndex)
            ? jointIndex
            : throw new KeyNotFoundException($"Target rig has no joint named {name}.");
    }

    public int GetJointIndexByObjectIndex(int objectIndex) =>
        _jointIndicesByObjectIndex.TryGetValue(objectIndex, out int jointIndex)
            ? jointIndex
            : throw new KeyNotFoundException(
                $"Target object [{objectIndex}] is not part of the fitting rig.");

    public TargetRigFittingPose CreateFittingPose() => new(this);

    internal static string ComputeSourceFingerprint(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        return Convert.ToHexString(SHA256.HashData(document.Data.Span));
    }

    internal static Matrix4x4 ComposeLocal(
        Vector3 bindScale,
        Quaternion bindRotation,
        Quaternion boneLocalRotationDelta,
        Vector3 bindTranslation) =>
        Matrix4x4.CreateScale(bindScale) *
        Matrix4x4.CreateFromQuaternion(boneLocalRotationDelta) *
        Matrix4x4.CreateFromQuaternion(bindRotation) *
        Matrix4x4.CreateTranslation(bindTranslation);

    internal static bool IsFinite(Quaternion value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z) && float.IsFinite(value.W);

    internal static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);

    internal static bool IsFinite(Matrix4x4 value) =>
        float.IsFinite(value.M11) && float.IsFinite(value.M12) &&
        float.IsFinite(value.M13) && float.IsFinite(value.M14) &&
        float.IsFinite(value.M21) && float.IsFinite(value.M22) &&
        float.IsFinite(value.M23) && float.IsFinite(value.M24) &&
        float.IsFinite(value.M31) && float.IsFinite(value.M32) &&
        float.IsFinite(value.M33) && float.IsFinite(value.M34) &&
        float.IsFinite(value.M41) && float.IsFinite(value.M42) &&
        float.IsFinite(value.M43) && float.IsFinite(value.M44);

    internal static Vector3 Translation(Matrix4x4 value) =>
        new(value.M41, value.M42, value.M43);

    private static bool IsUniformScale(Vector3 value)
    {
        Vector3 magnitude = Vector3.Abs(value);
        float largest = MathF.Max(magnitude.X, MathF.Max(magnitude.Y, magnitude.Z));
        float smallest = MathF.Min(magnitude.X, MathF.Min(magnitude.Y, magnitude.Z));
        float tolerance = ScaleUniformityTolerance * MathF.Max(1, largest);
        return float.IsFinite(largest) && float.IsFinite(smallest) &&
               largest - smallest <= tolerance;
    }

    private static HashSet<int> ReadDeformObjectIndices(SmoDocument document)
    {
        var result = new HashSet<int>();
        SmoObjectEntry[] skins = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .ToArray();
        if (skins.Length == 0)
            throw new InvalidDataException("Target SMO contains no skin palettes.");

        foreach (SmoObjectEntry skinEntry in skins)
        {
            if (!SmoSkinDecoder.TryDecode(
                    document, skinEntry, out SmoSkin? skin, out string error) ||
                skin is null)
            {
                throw new InvalidDataException(
                    $"Target skin [{skinEntry.Index}] cannot be decoded: {error}");
            }
            foreach (SmoSkinBone bone in skin.Bones)
            {
                if ((uint)bone.NodeObjectIndex >= (uint)document.Objects.Count ||
                    document.Objects[bone.NodeObjectIndex].TypeHash != SmoClassIds.Node)
                {
                    throw new InvalidDataException(
                        $"Target skin [{skinEntry.Index}] references a non-node bone " +
                        $"object [{bone.NodeObjectIndex}].");
                }
                result.Add(bone.NodeObjectIndex);
            }
        }
        if (result.Count == 0)
            throw new InvalidDataException("Target skin palettes contain no deform joints.");
        return result;
    }

    private static Dictionary<int, int?> BuildRequiredNodeForest(
        SmoDocument document,
        SmoNodeHierarchy hierarchy,
        IReadOnlySet<int> deformObjectIndices)
    {
        var included = new HashSet<int>();
        foreach (int deformObjectIndex in deformObjectIndices)
        {
            var path = new HashSet<int>();
            int cursor = deformObjectIndex;
            while (true)
            {
                if (!path.Add(cursor))
                    throw new InvalidDataException(
                        $"Target node hierarchy contains a cycle through object [{cursor}].");
                included.Add(cursor);
                int? parent = ResolveLogicalParent(document, hierarchy, cursor);
                if (parent is null)
                    break;
                cursor = parent.Value;
            }
        }

        var result = new Dictionary<int, int?>();
        foreach (int objectIndex in included)
        {
            int? parent = ResolveLogicalParent(document, hierarchy, objectIndex);
            if (parent is int parentObjectIndex && !included.Contains(parentObjectIndex))
            {
                throw new InvalidDataException(
                    $"Required ancestor [{parentObjectIndex}] of node [{objectIndex}] was omitted.");
            }
            result.Add(objectIndex, parent);
        }
        return result;
    }

    private static int? ResolveLogicalParent(
        SmoDocument document,
        SmoNodeHierarchy hierarchy,
        int objectIndex)
    {
        if ((uint)objectIndex >= (uint)document.Objects.Count ||
            document.Objects[objectIndex].TypeHash != SmoClassIds.Node)
        {
            throw new InvalidDataException(
                $"Target rig object [{objectIndex}] is not a valid spNode.");
        }

        int? parent = hierarchy.ParentsByChild.TryGetValue(
                objectIndex, out IReadOnlyList<int>? logicalParents) &&
            logicalParents.Count == 1
                ? logicalParents[0]
                : document.Objects[objectIndex].ParentIndex;
        if (parent is not int parentObjectIndex)
            return null;
        if ((uint)parentObjectIndex >= (uint)document.Objects.Count)
        {
            throw new InvalidDataException(
                $"Target rig node [{objectIndex}] references parent " +
                $"[{parentObjectIndex}] outside the object catalog.");
        }
        if (document.Objects[parentObjectIndex].TypeHash != SmoClassIds.Node)
        {
            throw new InvalidDataException(
                $"Target rig node [{objectIndex}] references non-node parent " +
                $"[{parentObjectIndex}] {document.Objects[parentObjectIndex].Name}.");
        }
        return parentObjectIndex;
    }

    private static int[] TopologicalOrder(IReadOnlyDictionary<int, int?> parents)
    {
        var states = new Dictionary<int, byte>();
        var result = new List<int>(parents.Count);
        foreach (int objectIndex in parents.Keys.Order())
            Visit(objectIndex);
        return result.ToArray();

        void Visit(int objectIndex)
        {
            byte state = states.GetValueOrDefault(objectIndex);
            if (state == 2)
                return;
            if (state == 1)
                throw new InvalidDataException(
                    $"Target node hierarchy contains a cycle through object [{objectIndex}].");
            states[objectIndex] = 1;
            if (parents[objectIndex] is int parentObjectIndex)
            {
                if (!parents.ContainsKey(parentObjectIndex))
                    throw new InvalidDataException(
                        $"Target node [{objectIndex}] references missing ancestor " +
                        $"[{parentObjectIndex}].");
                Visit(parentObjectIndex);
            }
            states[objectIndex] = 2;
            result.Add(objectIndex);
        }
    }

    private static void ValidateUniqueNames(
        SmoDocument document,
        IReadOnlyList<int> objectIndices)
    {
        var names = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        foreach (int objectIndex in objectIndices)
        {
            string name = document.Objects[objectIndex].Name;
            if (string.IsNullOrWhiteSpace(name))
                throw new InvalidDataException(
                    $"Target rig node [{objectIndex}] has no stable name.");
            if (names.TryGetValue(name, out int previousObjectIndex))
            {
                throw new InvalidDataException(
                    $"Target rig contains duplicate node name {name} at objects " +
                    $"[{previousObjectIndex}] and [{objectIndex}].");
            }
            names.Add(name, objectIndex);
        }
    }

    private static Dictionary<int, Matrix4x4> ResolveSmoWorldMatrices(
        SmoDocument document,
        IReadOnlyList<int> orderedObjectIndices,
        IReadOnlyDictionary<int, int?> logicalParents,
        IReadOnlyDictionary<int, Matrix4x4> canonicalBindWorld)
    {
        var result = new Dictionary<int, Matrix4x4>();
        foreach (int objectIndex in orderedObjectIndices)
        {
            Matrix4x4 world;
            if (canonicalBindWorld.TryGetValue(objectIndex, out Matrix4x4 canonical))
            {
                world = canonical;
            }
            else
            {
                Matrix4x4 local = SmoNodeTransformDecoder.TryDecode(
                        document,
                        document.Objects[objectIndex],
                        out SmoNodeTransform? transform) && transform is not null
                    ? transform.LocalMatrix
                    : Matrix4x4.Identity;
                world = logicalParents[objectIndex] is int parentObjectIndex
                    ? local * result[parentObjectIndex]
                    : local;
            }
            if (!IsFinite(world))
            {
                throw new InvalidDataException(
                    $"Target rig node [{objectIndex}] has a non-finite bind-world matrix.");
            }
            result.Add(objectIndex, world);
        }
        return result;
    }

    private static Matrix4x4 ToExternalSpace(Matrix4x4 value) =>
        ExternalSpaceReflection * value * ExternalSpaceReflection;

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

/// <summary>
/// Immutable, defensive capture of a validated fitting pose. The world matrices
/// use the importer's external row-vector space and can only be created by
/// <see cref="TargetRigFittingPose.Capture"/> after hierarchy and length checks.
/// </summary>
public sealed class TargetRigFittingPoseSnapshot
{
    private readonly ReadOnlyCollection<Matrix4x4> _worldMatrices;
    private readonly ReadOnlyCollection<Quaternion> _localRotationDeltas;

    internal TargetRigFittingPoseSnapshot(
        TargetRigDefinition definition,
        IReadOnlyList<Matrix4x4> worldMatrices,
        IReadOnlyList<Quaternion> localRotationDeltas,
        Quaternion rootRotation,
        Vector3 rootTranslation,
        bool isIdentityPose)
    {
        Definition = definition ?? throw new ArgumentNullException(nameof(definition));
        if (worldMatrices.Count != definition.Joints.Count ||
            localRotationDeltas.Count != definition.Joints.Count)
        {
            throw new ArgumentException(
                "A fitting-pose capture must contain one matrix and rotation " +
                "for every target-rig joint.");
        }

        Matrix4x4[] checkedWorld = worldMatrices.ToArray();
        if (checkedWorld.Any(matrix => !TargetRigDefinition.IsFinite(matrix)))
            throw new ArgumentException(
                "A fitting-pose capture contains a non-finite world matrix.",
                nameof(worldMatrices));
        Quaternion[] checkedRotations = localRotationDeltas.ToArray();
        if (checkedRotations.Any(rotation =>
                !TargetRigDefinition.IsFinite(rotation) ||
                rotation.LengthSquared() <= 0.000001f))
        {
            throw new ArgumentException(
                "A fitting-pose capture contains an invalid local rotation.",
                nameof(localRotationDeltas));
        }
        if (!TargetRigDefinition.IsFinite(rootRotation) ||
            rootRotation.LengthSquared() <= 0.000001f ||
            !TargetRigDefinition.IsFinite(rootTranslation))
        {
            throw new ArgumentException(
                "A fitting-pose capture contains an invalid root transform.");
        }

        _worldMatrices = Array.AsReadOnly(checkedWorld);
        _localRotationDeltas = Array.AsReadOnly(checkedRotations);
        RootRotation = Quaternion.Normalize(rootRotation);
        RootTranslation = rootTranslation;
        IsIdentityPose = isIdentityPose;
        TargetRigFingerprint = definition.SourceFingerprint;
        DefinitionIdentity = definition.InstanceIdentity;
    }

    public TargetRigDefinition Definition { get; }

    public string TargetRigFingerprint { get; }

    public IReadOnlyList<Matrix4x4> WorldMatrices => _worldMatrices;

    public IReadOnlyList<Quaternion> LocalRotationDeltas => _localRotationDeltas;

    public Quaternion RootRotation { get; }

    public Vector3 RootTranslation { get; }

    public bool IsIdentityPose { get; }

    internal Guid DefinitionIdentity { get; }

    internal void ValidateForTarget(SmoDocument target)
    {
        ArgumentNullException.ThrowIfNull(target);
        string targetFingerprint = TargetRigDefinition.ComputeSourceFingerprint(target);
        if (!string.Equals(
                TargetRigFingerprint,
                targetFingerprint,
                StringComparison.Ordinal))
        {
            throw new InvalidOperationException(
                "The fitting pose was captured from a different target SMO rig.");
        }
        if (DefinitionIdentity != Definition.InstanceIdentity ||
            WorldMatrices.Count != Definition.Joints.Count)
        {
            throw new InvalidOperationException(
                "The fitting-pose definition identity is inconsistent.");
        }
    }
}

/// <summary>
/// Transient rotation-only pose used to fit the immutable target rig to donor
/// geometry. It never owns or writes an <see cref="SmoDocument"/>.
/// </summary>
public sealed class TargetRigFittingPose
{
    private const float LengthTolerance = 0.0001f;
    private readonly Quaternion[] _localRotationDeltas;

    internal TargetRigFittingPose(TargetRigDefinition definition)
    {
        Definition = definition ?? throw new ArgumentNullException(nameof(definition));
        _localRotationDeltas = Enumerable.Repeat(
            Quaternion.Identity, definition.Joints.Count).ToArray();
    }

    public TargetRigDefinition Definition { get; }

    /// <summary>
    /// Global external-space alignment rotation applied about the coordinate
    /// origin after every rig-local transform.
    /// </summary>
    public Quaternion RootRotation { get; private set; } = Quaternion.Identity;

    /// <summary>
    /// Global external-space alignment translation applied after
    /// <see cref="RootRotation"/>.
    /// </summary>
    public Vector3 RootTranslation { get; private set; } = Vector3.Zero;

    public IReadOnlyList<Quaternion> LocalRotationDeltas =>
        Array.AsReadOnly(_localRotationDeltas);

    /// <summary>
    /// Sets the global fitting alignment. Rotation is about the external-space
    /// coordinate origin; this is not an in-place rotation about a root joint.
    /// </summary>
    public void SetRootTransform(Quaternion rotation, Vector3 translation)
    {
        RootRotation = NormalizeRotation(rotation, nameof(rotation));
        if (!TargetRigDefinition.IsFinite(translation))
            throw new ArgumentException("Root translation must be finite.", nameof(translation));
        RootTranslation = translation;
    }

    /// <summary>
    /// Sets a delta expressed in the selected bone's own bind-local axes.
    /// Its bind translation and scale remain unchanged.
    /// </summary>
    public void SetLocalRotationDelta(int jointIndex, Quaternion rotationDelta)
    {
        if ((uint)jointIndex >= (uint)_localRotationDeltas.Length)
            throw new ArgumentOutOfRangeException(nameof(jointIndex));
        _localRotationDeltas[jointIndex] = NormalizeRotation(
            rotationDelta, nameof(rotationDelta));
    }

    public void SetLocalRotationDelta(string jointName, Quaternion rotationDelta) =>
        SetLocalRotationDelta(Definition.GetJointIndex(jointName), rotationDelta);

    public void ResetLocalRotationDelta(int jointIndex)
    {
        if ((uint)jointIndex >= (uint)_localRotationDeltas.Length)
            throw new ArgumentOutOfRangeException(nameof(jointIndex));
        _localRotationDeltas[jointIndex] = Quaternion.Identity;
    }

    public void Reset()
    {
        Array.Fill(_localRotationDeltas, Quaternion.Identity);
        RootRotation = Quaternion.Identity;
        RootTranslation = Vector3.Zero;
    }

    /// <summary>
    /// Computes and validates the complete hierarchy, then returns an immutable
    /// defensive snapshot suitable for a porting preparation. Later edits to this
    /// pose do not change the captured matrices or rotations.
    /// </summary>
    public TargetRigFittingPoseSnapshot Capture()
    {
        IReadOnlyList<Matrix4x4> worldMatrices = ComputeWorldMatrices();
        // This flag controls whether the geometry bake may be skipped, so it
        // must describe the exact reset state. Even a tiny intentional edit
        // must pass through the bake instead of being swallowed by a tolerance.
        bool identityPose = RootRotation == Quaternion.Identity &&
                            RootTranslation == Vector3.Zero &&
                            _localRotationDeltas.All(
                                rotation => rotation == Quaternion.Identity);
        return new TargetRigFittingPoseSnapshot(
            Definition,
            worldMatrices,
            _localRotationDeltas,
            RootRotation,
            RootTranslation,
            identityPose);
    }

    public IReadOnlyList<Matrix4x4> ComputeWorldMatrices()
    {
        Matrix4x4 rootTransform =
            Matrix4x4.CreateFromQuaternion(RootRotation) *
            Matrix4x4.CreateTranslation(RootTranslation);
        var result = new Matrix4x4[Definition.Joints.Count];
        foreach (TargetRigJoint joint in Definition.Joints)
        {
            Matrix4x4 local = TargetRigDefinition.ComposeLocal(
                joint.BindLocalScale,
                joint.BindLocalRotation,
                _localRotationDeltas[joint.JointIndex],
                joint.BindLocalTranslation);
            Matrix4x4 world = joint.ParentJointIndex >= 0
                ? local * result[joint.ParentJointIndex]
                : local * rootTransform;
            if (!TargetRigDefinition.IsFinite(world))
                throw new InvalidOperationException(
                    $"Fitting pose produced a non-finite matrix for joint {joint.Name}.");
            result[joint.JointIndex] = world;
        }

        VerifyLengths(result);
        return Array.AsReadOnly(result);
    }

    public IReadOnlyDictionary<int, Matrix4x4> ComputeWorldMatricesByObjectIndex()
    {
        IReadOnlyList<Matrix4x4> matrices = ComputeWorldMatrices();
        Dictionary<int, Matrix4x4> result = Definition.Joints.ToDictionary(
            joint => joint.ObjectIndex,
            joint => matrices[joint.JointIndex]);
        return new ReadOnlyDictionary<int, Matrix4x4>(result);
    }

    private void VerifyLengths(IReadOnlyList<Matrix4x4> worldMatrices)
    {
        foreach (TargetRigJoint joint in Definition.Joints.Where(
                     joint => joint.ParentJointIndex >= 0))
        {
            float posedLength = Vector3.Distance(
                TargetRigDefinition.Translation(
                    worldMatrices[joint.ParentJointIndex]),
                TargetRigDefinition.Translation(
                    worldMatrices[joint.JointIndex]));
            float tolerance = LengthTolerance * MathF.Max(
                1, joint.BindLengthFromParent);
            if (!float.IsFinite(posedLength) ||
                MathF.Abs(posedLength - joint.BindLengthFromParent) > tolerance)
            {
                throw new InvalidOperationException(
                    $"Rotation-only fitting changed the length of {joint.Name}: " +
                    $"bind={joint.BindLengthFromParent}, pose={posedLength}.");
            }
        }
    }

    private static Quaternion NormalizeRotation(Quaternion value, string parameterName)
    {
        if (!TargetRigDefinition.IsFinite(value) ||
            value.LengthSquared() <= 0.000001f)
        {
            throw new ArgumentException(
                "Rotation must be finite and non-zero.", parameterName);
        }
        Quaternion normalized = Quaternion.Normalize(value);
        if (!TargetRigDefinition.IsFinite(normalized))
            throw new ArgumentException("Rotation cannot be normalized.", parameterName);
        return normalized;
    }

}
