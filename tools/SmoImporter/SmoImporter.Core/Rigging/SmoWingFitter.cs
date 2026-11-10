using System.Numerics;
using SmoExporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>One native wing attachment and its measured fitting plane. A null node identifies the BloomX placement profile.</summary>
public sealed record SmoWingReference(
    string NodeName,
    int? NodeObjectIndex,
    IReadOnlyList<int> MeshObjectIndices,
    Matrix4x4 NodeLocalToSpine,
    Matrix4x4 PlaneLocalToSpine,
    Matrix4x4 PlaneInFittingSpace)
{
    public bool UsesBloomXProfile => NodeObjectIndex is null;
    public Vector3 Pivot => PlaneInFittingSpace.Translation;
    public Vector3 PlaneNormal => Vector3.Normalize(Vector3.TransformNormal(Vector3.UnitZ, PlaneInFittingSpace));
}

public sealed record SmoWingReferences(SmoWingReference Left, SmoWingReference Right);

public sealed record SmoWingSideFit(
    IReadOnlyList<int> Components,
    SmoWingReference Reference,
    Matrix4x4 PlaneBeforeFit,
    Matrix4x4 Transform);

public sealed record SmoWingFitResult(ImportedScene Scene, SmoWingSideFit Left, SmoWingSideFit Right);

/// <summary>Fits explicitly selected wing surfaces to the original rigid WingL/WingR attachments in external coordinates.</summary>
public static class SmoWingFitter
{
    private const string LeftWing = "WingL";
    private const string RightWing = "WingR";
    private const float MinimumExtent = 0.0001f;

    private sealed record SelectedWing(int ComponentIndex, IReadOnlyList<TargetRigBodyVertexMembership> Members, Vector3[] Points);

    /// <summary>Resolves the active target meshes first and uses BloomX only when both wing attachments are absent.</summary>
    public static SmoWingReferences ResolveReferences(SmoDocument target, TargetRigFittingPoseSnapshot pose)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(pose);
        pose.ValidateForTarget(target);
        int spine = pose.Definition.GetJointIndex("Spine_03");
        TargetRigJoint spineJoint = pose.Definition.Joints[spine];
        Matrix4x4 fittingSpine = pose.WorldMatrices[spine];
        bool hasLeftNode = target.Objects.Any(entry => string.Equals(entry.Name, LeftWing, StringComparison.OrdinalIgnoreCase));
        bool hasRightNode = target.Objects.Any(entry => string.Equals(entry.Name, RightWing, StringComparison.OrdinalIgnoreCase));
        if (!hasLeftNode && !hasRightNode)
            return SmoBloomXWingProfile.Resolve(fittingSpine);
        if (!hasLeftNode || !hasRightNode)
            throw new InvalidDataException("The target has an incomplete native WingL/WingR pair.");

        var scene = SmoSceneBuilder.Build(target, new SmoExportOptions(Resources:
            SmoExportResourceTypes.Meshes | SmoExportResourceTypes.Skeleton | SmoExportResourceTypes.ServiceNodes));
        Matrix4x4 inverseSpine = Invert(spineJoint.BindWorldMatrix);
        BodyFrame bindFrame = GetBodyFrame(pose.Definition, spineJoint.BindWorldMatrix, spineJoint.BindWorldMatrix);
        return new SmoWingReferences(
            ReadActiveWing(scene, LeftWing, spineJoint.ObjectIndex, inverseSpine, fittingSpine, bindFrame),
            ReadActiveWing(scene, RightWing, spineJoint.ObjectIndex, inverseSpine, fittingSpine, bindFrame));
    }

    /// <summary>
    /// Returns a new geometry snapshot in the same aligned space. Other meshes, attributes and original inputs are preserved.
    /// The optional height correction is a percentage of the donor's non-wing height along the body's up axis;
    /// it moves both wings within their fitted planes, positive upward, without changing their animation pivots.
    /// Rotation is in degrees within each plane around its back-side root after the height correction;
    /// positive angles raise both outward spans. Native attachment and animation matrices remain unchanged.
    /// </summary>
    public static SmoWingFitResult Fit(SmoDocument target, TargetRigFittingPoseSnapshot pose, ImportedScene alignedDonor,
        IReadOnlyList<GeneratedSkinningComponentInfo> selectedComponents, float heightOffsetPercent = 0, float rotationDegrees = 0)
    {
        ArgumentNullException.ThrowIfNull(alignedDonor);
        ArgumentNullException.ThrowIfNull(selectedComponents);
        if (!float.IsFinite(heightOffsetPercent))
            throw new ArgumentOutOfRangeException(nameof(heightOffsetPercent), "The wing height correction must be finite.");
        if (!float.IsFinite(rotationDegrees))
            throw new ArgumentOutOfRangeException(nameof(rotationDegrees), "The wing rotation must be finite.");
        if (selectedComponents.Count < 2)
            throw new InvalidDataException("Select separate left and right wing surfaces.");

        SmoWingReferences references = ResolveReferences(target, pose);
        int spine = pose.Definition.GetJointIndex("Spine_03");
        BodyFrame body = GetBodyFrame(pose.Definition, pose.Definition.Joints[spine].BindWorldMatrix, pose.WorldMatrices[spine]);
        var left = new List<SelectedWing>();
        var right = new List<SelectedWing>();
        var usedVertices = new HashSet<(int Mesh, int Vertex)>();
        var usedComponents = new HashSet<int>();

        foreach (GeneratedSkinningComponentInfo component in selectedComponents.OrderBy(component => component.ComponentIndex))
        {
            if (!usedComponents.Add(component.ComponentIndex))
                throw new InvalidDataException("A wing surface is selected more than once.");
            var points = new List<Vector3>();
            foreach (TargetRigBodyVertexMembership member in component.VerticesByMesh)
            {
                if ((uint)member.MeshIndex >= (uint)alignedDonor.Meshes.Count)
                    throw new InvalidDataException("Wing membership refers to a missing donor mesh.");
                ImportedMesh mesh = alignedDonor.Meshes[member.MeshIndex];
                if (!string.Equals(member.MeshName, mesh.Name, StringComparison.Ordinal))
                    throw new InvalidDataException("Wing membership no longer matches the donor mesh.");
                foreach (int vertex in member.VertexIndices)
                {
                    if ((uint)vertex >= (uint)mesh.Positions.Length || !usedVertices.Add((member.MeshIndex, vertex)))
                        throw new InvalidDataException("Wing membership refers to a missing or overlapping vertex.");
                    Vector3 position = mesh.Positions[vertex];
                    if (!IsFinite(position))
                        throw new InvalidDataException("A donor wing position is not finite.");
                    points.Add(position);
                }
            }

            Vector3[] unique = points.Distinct().ToArray();
            if (unique.Length < 3)
                throw new InvalidDataException("A wing surface has fewer than three distinct positions.");
            var surface = new SelectedWing(component.ComponentIndex, component.VerticesByMesh, unique);
            float minimum = unique.Min(point => body.Lateral(point));
            float maximum = unique.Max(point => body.Lateral(point));
            float center = (minimum + maximum) * 0.5f;
            float tolerance = Math.Max(MinimumExtent, (maximum - minimum) * 0.1f);
            if (Math.Abs(center) <= tolerance || center > 0 && minimum < -tolerance || center < 0 && maximum > tolerance)
                throw new InvalidDataException("A selected wing crosses the body center. Select separate wing surfaces.");
            (center > 0 ? left : right).Add(surface);
        }

        if (left.Count == 0 || right.Count == 0)
            throw new InvalidDataException("Select a wing surface on both sides of the donor.");
        SmoWingSideFit leftFit = FitSide(left, references.Left, body, sign: 1);
        SmoWingSideFit rightFit = FitSide(right, references.Right, body, sign: -1);
        if (heightOffsetPercent != 0)
        {
            float distance = MeasureBodyHeight(alignedDonor, usedVertices, body) * (heightOffsetPercent / 100);
            if (!float.IsFinite(distance))
                throw new InvalidDataException("The wing height correction exceeds the measurable donor extent.");
            leftFit = OffsetHeight(leftFit, body.Up, distance);
            rightFit = OffsetHeight(rightFit, body.Up, distance);
        }
        if (rotationDegrees != 0)
        {
            leftFit = RotateInPlane(leftFit, body.Up, rotationDegrees, sign: 1);
            rightFit = RotateInPlane(rightFit, body.Up, rotationDegrees, sign: -1);
        }
        ImportedMesh[] fitted = alignedDonor.Meshes.ToArray();
        var cloned = new HashSet<int>();
        ApplySide(fitted, cloned, left, leftFit.Transform);
        ApplySide(fitted, cloned, right, rightFit.Transform);
        return new SmoWingFitResult(alignedDonor with { Meshes = Array.AsReadOnly(fitted) }, leftFit, rightFit);
    }

    private static float MeasureBodyHeight(ImportedScene scene, HashSet<(int Mesh, int Vertex)> wingVertices, BodyFrame body)
    {
        float lower = float.PositiveInfinity, upper = float.NegativeInfinity;
        for (int meshIndex = 0; meshIndex < scene.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = scene.Meshes[meshIndex];
            foreach (uint vertexIndex in mesh.TriangleIndices.Distinct())
            {
                if (vertexIndex >= mesh.Positions.Length)
                    throw new InvalidDataException("A donor triangle refers to a missing body vertex.");
                if (wingVertices.Contains((meshIndex, (int)vertexIndex)))
                    continue;
                Vector3 point = mesh.Positions[vertexIndex];
                if (!IsFinite(point))
                    throw new InvalidDataException("A donor body position is not finite.");
                float height = Vector3.Dot(point - body.Center, body.Up);
                lower = Math.Min(lower, height);
                upper = Math.Max(upper, height);
            }
        }
        float extent = upper - lower;
        if (!float.IsFinite(extent) || extent <= MinimumExtent)
            throw new InvalidDataException("The donor has no measurable body height outside the selected wings.");
        return extent;
    }

    private static SmoWingSideFit OffsetHeight(SmoWingSideFit side, Vector3 bodyUp, float distance)
    {
        Vector3 normal = side.Reference.PlaneNormal;
        Vector3 vertical = bodyUp - normal * Vector3.Dot(bodyUp, normal);
        if (!IsFinite(vertical) || vertical.LengthSquared() <= MinimumExtent * MinimumExtent)
            throw new InvalidDataException("A fitted wing plane has no measurable vertical direction.");
        Vector3 offset = Vector3.Normalize(vertical) * distance;
        return side with { Transform = side.Transform * Matrix4x4.CreateTranslation(offset) };
    }

    private static SmoWingSideFit RotateInPlane(SmoWingSideFit side, Vector3 bodyUp, float degrees, int sign)
    {
        Vector3 normal = side.Reference.PlaneNormal;
        Vector3 outward = Vector3.TransformNormal(Vector3.UnitX, side.Reference.PlaneInFittingSpace);
        float rise = Vector3.Dot(Vector3.Cross(normal, outward), bodyUp);
        // Mirrored wings share a consistently facing plane normal. Choose the handedness
        // that raises the outward span; retain the side sign for a horizontal plane.
        float handedness = Math.Abs(rise) > MinimumExtent ? MathF.Sign(rise) : sign;
        float radians = (degrees % 360) * (MathF.PI / 180) * handedness;
        Vector3 pivot = Vector3.Transform(side.PlaneBeforeFit.Translation, side.Transform);
        Matrix4x4 correction = Matrix4x4.CreateTranslation(-pivot) *
            Matrix4x4.CreateFromAxisAngle(normal, radians) * Matrix4x4.CreateTranslation(pivot);
        return side with { Transform = side.Transform * correction };
    }

    private static SmoWingReference ReadActiveWing(SmoExportScene scene, string name, int spineIndex,
        Matrix4x4 inverseSpine, Matrix4x4 fittingSpine, BodyFrame bindFrame)
    {
        var nodes = scene.Nodes.Where(node => string.Equals(node.Name, name, StringComparison.OrdinalIgnoreCase)).ToArray();
        if (nodes is not [var node] || node.ParentObjectIndex != spineIndex)
            throw new InvalidDataException($"Native {name} is not a unique child of Spine_03.");
        var placements = scene.MeshPlacements.Where(placement => placement.ParentNodeObjectIndex == node.ObjectIndex).ToArray();
        if (placements.Length == 0)
            throw new InvalidDataException($"Native {name} has no active rigid mesh.");

        var meshes = scene.Meshes.ToDictionary(mesh => mesh.VariantKey);
        var meshIndices = new HashSet<int>();
        var positions = new List<Vector3>();
        foreach (SmoExportMeshPlacement placement in placements)
        {
            SmoExportMesh mesh = meshes[placement.EffectiveMeshKey];
            if (mesh.SkinObjectIndex is not null || placement.IsSharedInstance || !meshIndices.Add(mesh.ObjectIndex))
                throw new InvalidDataException($"Native {name} does not have an unambiguous rigid mesh attachment.");
            positions.AddRange(mesh.TriangleIndices.Distinct().Select(index => Vector3.Transform(mesh.Positions[index], placement.WorldMatrix)));
        }

        Vector3[] points = positions.Distinct().ToArray();
        int sign = name == LeftWing ? 1 : -1;
        Matrix4x4 plane = MeasurePlane(points, MeasureInnerEdge(points, bindFrame, sign), bindFrame);
        Matrix4x4 localPlane = plane * inverseSpine;
        return new SmoWingReference(name, node.ObjectIndex, Array.AsReadOnly(meshIndices.Order().ToArray()),
            node.BindLocalMatrix, localPlane, localPlane * fittingSpine);
    }

    private static SmoWingSideFit FitSide(List<SelectedWing> surfaces, SmoWingReference reference, BodyFrame body, int sign)
    {
        Vector3[] points = surfaces.SelectMany(surface => surface.Points).Distinct().ToArray();
        Matrix4x4 sourcePlane = MeasurePlane(points, MeasureInnerEdge(points, body, sign), body);
        Matrix4x4 fit = Invert(sourcePlane) * Orthonormalize(reference.PlaneInFittingSpace);
        return new SmoWingSideFit(Array.AsReadOnly(surfaces.Select(surface => surface.ComponentIndex).ToArray()), reference, sourcePlane, fit);
    }

    private static Vector3 MeasureInnerEdge(Vector3[] points, BodyFrame body, int sign)
    {
        if (points.Length < 3 || points.Any(point => !IsFinite(point)))
            throw new InvalidDataException("The wing plane requires finite surface geometry.");
        float inner = points.Min(point => sign * body.Lateral(point));
        float outer = points.Max(point => sign * body.Lateral(point));
        if (outer - inner < MinimumExtent)
            throw new InvalidDataException("A wing has no measurable outward span.");
        // Use the same registration feature on the source and target surfaces. The native node has a separate animation pivot.
        return Center(points.Where(point => sign * body.Lateral(point) <= inner + (outer - inner) * 0.08f));
    }

    private static void ApplySide(ImportedMesh[] meshes, HashSet<int> cloned, List<SelectedWing> surfaces, Matrix4x4 fit)
    {
        foreach (SelectedWing surface in surfaces)
        foreach (TargetRigBodyVertexMembership member in surface.Members)
        {
            ImportedMesh mesh = meshes[member.MeshIndex];
            if (cloned.Add(member.MeshIndex))
            {
                if (mesh.Normals.Length != 0 && mesh.Normals.Length != mesh.Positions.Length)
                    throw new InvalidDataException("Wing normals do not match the donor vertices.");
                mesh = mesh with { Positions = (Vector3[])mesh.Positions.Clone(), Normals = (Vector3[])mesh.Normals.Clone() };
                meshes[member.MeshIndex] = mesh;
            }
            foreach (int index in member.VertexIndices)
            {
                mesh.Positions[index] = Vector3.Transform(mesh.Positions[index], fit);
                if (mesh.Normals.Length != 0)
                    mesh.Normals[index] = Vector3.TransformNormal(mesh.Normals[index], fit);
            }
        }
    }

    private readonly record struct BodyFrame(Vector3 Center, Vector3 LateralAxis, Vector3 Up, Vector3 Front)
    {
        public float Lateral(Vector3 point) => Vector3.Dot(point - Center, LateralAxis);
    }

    private static BodyFrame GetBodyFrame(TargetRigDefinition rig, Matrix4x4 spineBind, Matrix4x4 fittingSpine)
    {
        Vector3 left = rig.Joints[rig.GetJointIndex("L_Clavicle")].BindWorldMatrix.Translation;
        Vector3 right = rig.Joints[rig.GetJointIndex("R_Clavicle")].BindWorldMatrix.Translation;
        Vector3 lower = rig.Joints[rig.GetJointIndex("Spine_02")].BindWorldMatrix.Translation;
        Matrix4x4 toPose = Invert(spineBind) * fittingSpine;
        Vector3 lateral = Unit(Vector3.TransformNormal(left - right, toPose));
        Vector3 up = Vector3.TransformNormal(spineBind.Translation - lower, toPose);
        up = Unit(up - lateral * Vector3.Dot(up, lateral));
        return new BodyFrame(fittingSpine.Translation, lateral, up, Unit(Vector3.Cross(lateral, up)));
    }

    private static Matrix4x4 MeasurePlane(Vector3[] points, Vector3 pivot, BodyFrame body)
    {
        if (points.Length < 3 || !IsFinite(pivot) || points.Any(point => !IsFinite(point)))
            throw new InvalidDataException("The wing plane requires finite surface geometry.");
        Vector3 center = Center(points);
        Vector3 normal = LeastVarianceAxis(points, center);
        // Fix the sign without depending on mesh winding or mirroring. Near-horizontal planes face up.
        float front = Vector3.Dot(normal, body.Front);
        float up = Vector3.Dot(normal, body.Up);
        float facing = Math.Abs(front) > 0.15f ? front : Math.Abs(up) > 0.15f ? up : Vector3.Dot(normal, body.LateralAxis);
        if (facing < 0)
            normal = -normal;
        Vector3 span = center - pivot;
        return Frame(Unit(span - normal * Vector3.Dot(span, normal)), normal, pivot);
    }

    private static Matrix4x4 Orthonormalize(Matrix4x4 matrix)
    {
        Vector3 normal = Unit(Vector3.TransformNormal(Vector3.UnitZ, matrix));
        Vector3 span = Vector3.TransformNormal(Vector3.UnitX, matrix);
        return Frame(Unit(span - normal * Vector3.Dot(span, normal)), normal, matrix.Translation);
    }

    private static Matrix4x4 Frame(Vector3 span, Vector3 normal, Vector3 pivot)
    {
        Vector3 up = Unit(Vector3.Cross(normal, span));
        return new Matrix4x4(span.X, span.Y, span.Z, 0, up.X, up.Y, up.Z, 0,
            normal.X, normal.Y, normal.Z, 0, pivot.X, pivot.Y, pivot.Z, 1);
    }

    private static Vector3 LeastVarianceAxis(Vector3[] points, Vector3 center)
    {
        var covariance = new double[3, 3];
        foreach (Vector3 point in points)
        {
            Vector3 delta = point - center;
            for (int row = 0; row < 3; row++)
            for (int column = row; column < 3; column++)
                covariance[row, column] += (double)delta[row] * delta[column];
        }
        for (int row = 0; row < 3; row++)
        for (int column = row + 1; column < 3; column++)
            covariance[column, row] = covariance[row, column];

        var vectors = new double[,] { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
        // A symmetric 3x3 Jacobi solve makes the minimum-variance axis independent of the orientation and tessellation winding.
        for (int sweep = 0; sweep < 8; sweep++)
        {
            Rotate(0, 1);
            Rotate(0, 2);
            Rotate(1, 2);
        }

        int[] axes = Enumerable.Range(0, 3).OrderBy(index => covariance[index, index]).ToArray();
        double small = covariance[axes[0], axes[0]], middle = covariance[axes[1], axes[1]], large = covariance[axes[2], axes[2]];
        if (!double.IsFinite(large) || middle <= Math.Max(1e-8, large * 1e-5) || small > middle * 0.45)
            throw new InvalidDataException("A selected wing has no stable plane.");
        int least = axes[0];
        return Unit(new Vector3((float)vectors[0, least], (float)vectors[1, least], (float)vectors[2, least]));

        void Rotate(int first, int second)
        {
            double cross = covariance[first, second];
            if (Math.Abs(cross) <= 1e-12 * (Math.Abs(covariance[first, first]) + Math.Abs(covariance[second, second])))
                return;
            double angle = 0.5 * Math.Atan2(2 * cross, covariance[second, second] - covariance[first, first]);
            double cosine = Math.Cos(angle), sine = Math.Sin(angle);
            for (int index = 0; index < 3; index++)
            {
                (covariance[index, first], covariance[index, second]) =
                    (cosine * covariance[index, first] - sine * covariance[index, second],
                     sine * covariance[index, first] + cosine * covariance[index, second]);
                (vectors[index, first], vectors[index, second]) =
                    (cosine * vectors[index, first] - sine * vectors[index, second],
                     sine * vectors[index, first] + cosine * vectors[index, second]);
            }
            for (int index = 0; index < 3; index++)
                (covariance[first, index], covariance[second, index]) =
                    (cosine * covariance[first, index] - sine * covariance[second, index],
                     sine * covariance[first, index] + cosine * covariance[second, index]);
            covariance[first, second] = covariance[second, first] = 0;
        }
    }

    private static Vector3 Center(IEnumerable<Vector3> points)
    {
        double x = 0, y = 0, z = 0;
        int count = 0;
        foreach (Vector3 point in points)
        {
            x += point.X; y += point.Y; z += point.Z;
            count++;
        }
        if (count == 0)
            throw new InvalidDataException("A wing has no positions to measure.");
        return new Vector3((float)(x / count), (float)(y / count), (float)(z / count));
    }

    private static Vector3 Unit(Vector3 value)
    {
        if (!IsFinite(value) || value.LengthSquared() <= MinimumExtent * MinimumExtent)
            throw new InvalidDataException("The wing frame has a degenerate axis.");
        return Vector3.Normalize(value);
    }

    private static Matrix4x4 Invert(Matrix4x4 matrix) => Matrix4x4.Invert(matrix, out Matrix4x4 inverse)
        ? inverse : throw new InvalidDataException("The wing attachment frame is singular.");

    private static bool IsFinite(Vector3 point) => float.IsFinite(point.X) && float.IsFinite(point.Y) && float.IsFinite(point.Z);
}
