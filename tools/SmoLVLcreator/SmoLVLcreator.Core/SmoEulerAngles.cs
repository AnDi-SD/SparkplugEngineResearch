using System.Numerics;

namespace SmoLVLcreator.Core;

/// <summary>
/// Stable editor conversion for the X (pitch), Y (yaw), Z (roll) convention
/// used by <see cref="Quaternion.CreateFromYawPitchRoll(float, float, float)"/>.
/// SMO still stores quaternions/matrices; Euler angles are only an input view.
/// </summary>
public static class SmoEulerAngles
{
    public static Quaternion FromDegrees(Vector3 degrees)
    {
        const float radiansPerDegree = MathF.PI / 180f;
        return Quaternion.Normalize(Quaternion.CreateFromYawPitchRoll(
            degrees.Y * radiansPerDegree,
            degrees.X * radiansPerDegree,
            degrees.Z * radiansPerDegree));
    }

    public static Vector3 ToDegrees(Quaternion value)
    {
        if (value.LengthSquared() < 1e-12f)
            return Vector3.Zero;
        value = Quaternion.Normalize(value);

        double pitchTerm = 2 * (value.W * value.X - value.Y * value.Z);
        float pitch = (float)Math.Asin(Math.Clamp(pitchTerm, -1, 1));
        float yaw = MathF.Atan2(
            2 * (value.W * value.Y + value.X * value.Z),
            1 - 2 * (value.X * value.X + value.Y * value.Y));
        float roll = MathF.Atan2(
            2 * (value.W * value.Z + value.X * value.Y),
            1 - 2 * (value.X * value.X + value.Z * value.Z));
        const float degreesPerRadian = 180f / MathF.PI;
        return new Vector3(pitch, yaw, roll) * degreesPerRadian;
    }

    public static bool TryGetAxisAngle(
        Quaternion value,
        out Vector3 axis,
        out float radians)
    {
        axis = Vector3.UnitY;
        radians = 0;
        if (!float.IsFinite(value.X) || !float.IsFinite(value.Y) ||
            !float.IsFinite(value.Z) || !float.IsFinite(value.W) ||
            value.LengthSquared() < 1e-12f)
        {
            return false;
        }
        value = Quaternion.Normalize(value);
        if (value.W < 0)
            value = new Quaternion(-value.X, -value.Y, -value.Z, -value.W);
        radians = 2 * MathF.Acos(Math.Clamp(value.W, -1, 1));
        float sine = MathF.Sqrt(MathF.Max(0, 1 - value.W * value.W));
        if (sine < 1e-6f || radians < 1e-6f)
        {
            radians = 0;
            return true;
        }
        axis = Vector3.Normalize(new Vector3(value.X, value.Y, value.Z) / sine);
        return true;
    }
}
