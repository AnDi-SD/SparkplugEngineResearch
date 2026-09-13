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
    public const uint CollisionManager = 0x0A316ECE;
    public const uint PhysicsManager = 0x436BFF01;
    public const uint BoundingVolume = 0x21CC76AF;
    public const uint MeshBoundingVolume = 0x3F453DE7;
    public const uint OrientedBoxBoundingVolume = 0x4DA04889;
    public const uint BoxBoundingVolume = 0x7B4C0876;
    public const uint CapsuleBoundingVolume = 0x312FABC0;
    public const uint ConvexBoundingVolume = 0x1BCC5322;
    public const uint CollisionMesh = 0x36432CFF;
    public const uint PartitionSystem = 0x912CC341;
    public const uint PartitionNode = 0x67672341;
    public const uint PartitionRenderable = 0x94BBCA2A;
    public const uint OctreeNode = 0x21A70829;
    public const uint Zone = 0x61254AB3;
    public const uint ZonePortal = 0x6523AC37;
    public const uint ZonePortalNode = 0xABB5AB2C;
    public const uint UvController = 0x1C0053D6;
    public const uint StandardLayer = 0x234C576B;
    public const uint MaterialTextureLayer = 0x7F577C6D;
    public const uint EnvironmentMapLayer = 0x427C7480;
    public const uint ShadowVolumeManager = 0x63FEA321;
    public const uint DxShadowVolumeManager = 0x04680BC1;
    public const uint DxShadowMeshSerializer = 0x774E52E3;
    public const uint TextNode = 0x52E86EFE;
    public const uint TextRenderable = 0x19A745D7;
    public const uint Font = 0x4693490A;
    public const uint Fog = 0x7AC95AEC;
    public const uint LightData = 0x5E6402DF;
    public const uint NavigationGraph = 0x188A161F;
    public const uint NavigationSet = 0x74F9013E;
    public const uint MeshNavigationSet = 0x7297173C;
    public const uint NavigationPortal = 0x385662AA;
    public const uint BspNode = 0x7362AB22;
    public const uint ParticleSystem = 0x5AFA1A4F;
    public const uint Animation = 0x56EE563A;
    public const uint MaterialColorController = 0x4C633E85;
    public const uint SkyBox = 0x7A7124AF;
    public const uint OcclusionVolume = 0x43D24430;
    public const uint LensFlare = 0x435370B5;
    public const uint AnimTextureController = 0x16FB0E47;
    public const uint SphereBoundingVolume = 0x390946D2;
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
                [SmoClassIds.CollisionManager] = "spCollisionManager",
                [SmoClassIds.PhysicsManager] = "spPhysicsManager",
                [SmoClassIds.BoundingVolume] = "spBoundingVolume",
                [SmoClassIds.MeshBoundingVolume] = "spMeshBV",
                [SmoClassIds.OrientedBoxBoundingVolume] = "spOBBBV",
                [SmoClassIds.BoxBoundingVolume] = "spBoxBV",
                [SmoClassIds.CapsuleBoundingVolume] = "spCapsuleBV",
                [SmoClassIds.ConvexBoundingVolume] = "spConvexBV",
                [SmoClassIds.CollisionMesh] = "spCollisionMesh",
                [SmoClassIds.PartitionSystem] = "spPartitionSystem",
                [SmoClassIds.PartitionNode] = "spPartitionNode",
                [SmoClassIds.PartitionRenderable] = "spPartitionRenderable",
                [SmoClassIds.OctreeNode] = "spOctreeNode",
                [SmoClassIds.Zone] = "spZone",
                [SmoClassIds.ZonePortal] = "spZonePortal",
                [SmoClassIds.ZonePortalNode] = "spZonePortalNode",
                [SmoClassIds.UvController] = "spUVController",
                [SmoClassIds.StandardLayer] = "spStdLayer",
                [SmoClassIds.MaterialTextureLayer] = "spMaterialTextureLayer",
                [SmoClassIds.EnvironmentMapLayer] = "spEnvironmentMapLayer",
                [SmoClassIds.ShadowVolumeManager] = "spShadowVolumeManager",
                [SmoClassIds.DxShadowVolumeManager] = "spDXShadowVolumeManager",
                [SmoClassIds.DxShadowMeshSerializer] = "spDXShadowMeshSerializer",
                [SmoClassIds.TextNode] = "spTextNode",
                [SmoClassIds.TextRenderable] = "spTextRenderable",
                [SmoClassIds.Font] = "spFont",
                [SmoClassIds.Fog] = "spFog",
                [SmoClassIds.LightData] = "spLightData",
                [SmoClassIds.NavigationGraph] = "spNavigationGraph",
                [SmoClassIds.NavigationSet] = "spNavigationSet",
                [SmoClassIds.MeshNavigationSet] = "spMeshNavigationSet",
                [SmoClassIds.NavigationPortal] = "spNavigationPortal",
                [SmoClassIds.BspNode] = "spBSPNode",
                [SmoClassIds.ParticleSystem] = "spParticleSystem",
                [SmoClassIds.Animation] = "spAnimation",
                [SmoClassIds.MaterialColorController] = "spMaterialColorController",
                [SmoClassIds.SkyBox] = "spSkyBox",
                [SmoClassIds.OcclusionVolume] = "spOcclusionVolume",
                [SmoClassIds.LensFlare] = "spLensFlare",
                [SmoClassIds.AnimTextureController] = "spAnimTexController",
                [SmoClassIds.SphereBoundingVolume] = "spSphereBV"
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
