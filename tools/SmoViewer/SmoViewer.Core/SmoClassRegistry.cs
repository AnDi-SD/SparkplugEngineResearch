using System.Collections.ObjectModel;
using System.Diagnostics.CodeAnalysis;

namespace SmoViewer.Core;

/// <summary>Confirmed Sparkplug class identifiers found in WinxClub.exe.</summary>
public static class SmoClassIds
{
    public const uint Model = 0x763277DB;
    public const uint MaterialData = 0x6160348B;
    public const uint TextureData = 0x78EA082B;
    public const uint MeshData = 0x33C34CF0;
    public const uint StaticRenderObject = 0x56D67170;
    public const uint Node = 0x695C0F65;
    public const uint RenderNode = 0x603625D0;
    public const uint Skin = 0x681F2043;
    public const uint CollisionInfo = 0x47A97C0E;
    public const uint MeshBoundingVolume = 0x3F453DE7;
    public const uint UvController = 0x1C0053D6;
    public const uint StandardLayer = 0x234C576B;
    public const uint MaterialTextureLayer = 0x7F577C6D;
    public const uint EnvironmentMapLayer = 0x427C7480;
    public const uint ShadowVolume = 0x63FEA321;
    public const uint DxShadowVolume = 0x04680BC1;
    public const uint DxShadowMesh = 0x774E52E3;
    public const uint TextNode = 0x52E86EFE;
    public const uint TextRenderable = 0x19A745D7;
    public const uint Font = 0x4693490A;
}

/// <summary>
/// Maps confirmed Sparkplug class identifiers to their engine class names.
/// Unknown identifiers remain valid and are intentionally not guessed.
/// </summary>
public static class SmoClassRegistry
{
    private static readonly IReadOnlyDictionary<uint, string> Classes =
        new ReadOnlyDictionary<uint, string>(
            new Dictionary<uint, string>
            {
                [SmoClassIds.Model] = "spModel",
                [SmoClassIds.MaterialData] = "spMaterialData",
                [SmoClassIds.TextureData] = "spTextureData",
                [SmoClassIds.MeshData] = "spMeshData",
                [SmoClassIds.StaticRenderObject] = "spStaticRenderObject",
                [SmoClassIds.Node] = "spNode",
                [SmoClassIds.RenderNode] = "spRenderNode",
                [SmoClassIds.Skin] = "spSkin",
                [SmoClassIds.CollisionInfo] = "spCollisionInfo",
                [SmoClassIds.MeshBoundingVolume] = "spMeshBV",
                [SmoClassIds.UvController] = "spUVController",
                [SmoClassIds.StandardLayer] = "spStdLayer",
                [SmoClassIds.MaterialTextureLayer] = "spMaterialTextureLayer",
                [SmoClassIds.EnvironmentMapLayer] = "spEnvironmentMapLayer",
                [SmoClassIds.ShadowVolume] = "spShadowVolume",
                [SmoClassIds.DxShadowVolume] = "spDXShadowVolume",
                [SmoClassIds.DxShadowMesh] = "spDXShadowMesh",
                [SmoClassIds.TextNode] = "spTextNode",
                [SmoClassIds.TextRenderable] = "spTextRenderable",
                [SmoClassIds.Font] = "spFont"
            });

    public static IReadOnlyDictionary<uint, string> KnownClasses => Classes;

    public static bool TryGetName(
        uint classId,
        [NotNullWhen(true)] out string? className) =>
        Classes.TryGetValue(classId, out className);

    public static string GetDisplayName(uint classId) =>
        TryGetName(classId, out string? className)
            ? className
            : $"0x{classId:X8}";
}
