using System.Numerics;

namespace SmoImporter.Core;

internal static class SmoBloomXWingProfile
{
    // PC pristine Media/Characters/BloomX/bloomx.smo, SHA-256
    // 49E2C2D22FAE3211C20D9A3767EBB43934048507606ABD0C32B16B093823C012.
    // WingL node 26 / mesh 29 and WingR node 22 / mesh 25 are rigid children of Spine_03.
    // The plane axes and inner-edge origins are measured from the referenced triangle vertices.
    private static readonly Matrix4x4 LeftNode = new(
        8.555287e-8f, 1, 7.823112e-7f, 0,
        0.97490263f, 1.00510725e-7f, -0.22263156f, 0,
        -0.22263156f, 7.821841e-7f, -0.9749026f, 0,
        5.5333633f, -2.7716078e-6f, 5.322155f, 1);

    private static readonly Matrix4x4 RightNode = LeftNode with
    {
        Translation = new Vector3(5.541374f, -0.08188906f, 5.3910046f)
    };

    private static readonly Matrix4x4 LeftPlane = new(
        0.5703713f, 0.618686f, 0.5402815f, 0,
        0.82053727f, -0.45908043f, -0.3405342f, 0,
        0.03734891f, 0.6375522f, -0.7695013f, 0,
        8.27755f, 0.58072853f, 5.949541f, 1);

    private static readonly Matrix4x4 RightPlane = new(
        0.57036996f, -0.6186915f, 0.54027665f, 0,
        -0.8205383f, -0.45908174f, 0.34053022f, 0,
        0.037348002f, -0.63754594f, -0.76950645f, 0,
        8.277557f, -0.58434814f, 5.9495335f, 1);

    internal static SmoWingReferences Resolve(Matrix4x4 spine) =>
        new(
            new SmoWingReference("WingL", null, [], LeftNode, LeftPlane, LeftPlane * spine),
            new SmoWingReference("WingR", null, [], RightNode, RightPlane, RightPlane * spine));
}
