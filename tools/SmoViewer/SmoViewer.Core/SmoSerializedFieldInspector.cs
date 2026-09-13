using System.Buffers.Binary;
using System.Globalization;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>
/// A read-only value recovered from a confirmed own or inherited serializer
/// section. Complex payloads remain visible as bounded hexadecimal data when
/// their complete layout has not yet been proven.
/// </summary>
public sealed record SmoSerializedFieldValue(
    SmoSerializedFieldDescriptor Descriptor,
    int Occurrence,
    SmoDataBlockSizeCode SizeKind,
    uint PayloadSize,
    int AbsolutePayloadOffset,
    string DisplayValue,
    bool IsDecoded,
    string HexPreview);

/// <summary>
/// Produces safe, read-only field values for the confirmed class definitions.
/// It never scans for fields and never treats a size mismatch as a decoded
/// value.
/// </summary>
public static class SmoSerializedFieldInspector
{
    private const int HexPreviewBytes = 48;

    public static IReadOnlyList<SmoSerializedFieldValue> Inspect(
        SmoDocument document,
        SmoObjectEntry entry)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        if (!SmoSerializedFieldRegistry.HasFieldDefinitions(entry.TypeHash) ||
            !SmoObjectFieldReader.TryRead(
                document, entry, out IReadOnlyList<SmoObjectField>? fields, out _))
        {
            return Array.Empty<SmoSerializedFieldValue>();
        }

        var result = new List<SmoSerializedFieldValue>();
        SmoTextureDataInfo? textureData = null;
        if (entry.TypeHash == SmoClassIds.TextureData)
            SmoTextureDataDecoder.TryDecode(document, entry, out textureData, out _);
        SmoMaterialDataInfo? materialData = null;
        if (entry.TypeHash == SmoClassIds.MaterialData)
            SmoMaterialDataDecoder.TryDecode(document, entry, out materialData, out _);
        SmoMeshDataInfo? meshData = null;
        if (entry.TypeHash == SmoClassIds.MeshData)
            SmoMeshDataDecoder.TryDecode(document,entry,out meshData,out _);
        SmoModelData? modelData = null;
        if (entry.TypeHash == SmoClassIds.Model)
            SmoModelDecoder.TryDecode(document,entry,out modelData,out _);
        SmoSkin? skinData = null;
        if (entry.TypeHash == SmoClassIds.Skin)
            SmoSkinDecoder.TryDecode(document,entry,out skinData,out _);
        SmoStaticRenderObjectData? staticRenderObject = null;
        if (entry.TypeHash == SmoClassIds.StaticRenderObject)
            SmoStaticRenderObjectDecoder.TryDecode(
                document,entry,out staticRenderObject,out _);
        SmoCollisionInfoData? collisionInfo = null;
        if (entry.TypeHash == SmoClassIds.CollisionInfo)
            SmoCollisionInfoDecoder.TryDecode(
                document,entry,out collisionInfo,out _);
        SmoMeshBoundingVolumeData? meshBoundingVolume = null;
        if (entry.TypeHash == SmoClassIds.MeshBoundingVolume)
            SmoMeshBoundingVolumeDecoder.TryDecode(
                document,entry,out meshBoundingVolume,out _);
        SmoPartitionRenderableData? partitionRenderable = null;
        if (entry.TypeHash == SmoClassIds.PartitionRenderable)
            SmoPartitionRenderableDecoder.TryDecode(
                document,entry,out partitionRenderable,out _);
        SmoPartitionNodeData? partitionNode = null;
        if (entry.TypeHash == SmoClassIds.PartitionNode)
            SmoPartitionNodeDecoder.TryDecode(
                document,entry,out partitionNode,out _);
        SmoOctreeNodeData? octreeNode = null;
        if (entry.TypeHash == SmoClassIds.OctreeNode &&
            SmoOctreeNodeDecoder.TryDecode(document,entry,out octreeNode,out _))
        {
            partitionNode = octreeNode.PartitionNode;
        }
        SmoBspNodeData? bspNode = null;
        if (entry.TypeHash == SmoClassIds.BspNode &&
            SmoBspNodeDecoder.TryDecode(
                document,entry,out bspNode,out _))
        {
            partitionNode = bspNode.PartitionNode;
        }
        SmoPartitionSystemData? partitionSystem = null;
        if (entry.TypeHash == SmoClassIds.PartitionSystem)
            SmoPartitionSystemDecoder.TryDecode(
                document,entry,out partitionSystem,out _);
        SmoZoneData? zone = null;
        if (entry.TypeHash == SmoClassIds.Zone)
            SmoZoneDecoder.TryDecode(document,entry,out zone,out _);
        SmoZonePortalData? zonePortal = null;
        if (entry.TypeHash == SmoClassIds.ZonePortal)
            SmoZonePortalDecoder.TryDecode(document,entry,out zonePortal,out _);
        SmoZonePortalNodeData? zonePortalNode = null;
        if (entry.TypeHash == SmoClassIds.ZonePortalNode)
            SmoZonePortalNodeDecoder.TryDecode(
                document,entry,out zonePortalNode,out _);
        SmoOcclusionVolumeData? occlusionVolume = null;
        if (entry.TypeHash == SmoClassIds.OcclusionVolume)
            SmoOcclusionVolumeDecoder.TryDecode(
                document,entry,out occlusionVolume,out _);
        SmoMeshNavigationSetData? meshNavigationSet = null;
        if (entry.TypeHash == SmoClassIds.MeshNavigationSet)
            SmoMeshNavigationSetDecoder.TryDecode(
                document,entry,out meshNavigationSet,out _);
        SmoNavigationPortalData? navigationPortal = null;
        if (entry.TypeHash == SmoClassIds.NavigationPortal)
            SmoNavigationPortalDecoder.TryDecode(
                document,entry,out navigationPortal,out _);
        SmoNavigationGraphData? navigationGraph = null;
        if (entry.TypeHash == SmoClassIds.NavigationGraph)
            SmoNavigationGraphDecoder.TryDecode(
                document,entry,out navigationGraph,out _);
        SmoSkyBoxData? skyBox = null;
        if (entry.TypeHash == SmoClassIds.SkyBox)
            SmoSkyBoxDecoder.TryDecode(document,entry,out skyBox,out _);
        SmoParticleSystemData? particleSystem = null;
        if (entry.TypeHash == SmoClassIds.ParticleSystem)
            SmoParticleSystemDecoder.TryDecode(document,entry,out particleSystem,out _);
        SmoAnimTextureControllerData? animTextureController = null;
        if (entry.TypeHash == SmoClassIds.AnimTextureController)
            SmoAnimTextureControllerDecoder.TryDecode(
                document,entry,out animTextureController,out _);
        SmoLensFlareData? lensFlare = null;
        if (entry.TypeHash == SmoClassIds.LensFlare)
            SmoLensFlareDecoder.TryDecode(document,entry,out lensFlare,out _);
        SmoLightData? lightData = null;
        if (entry.TypeHash == SmoClassIds.LightData)
            SmoLightDataDecoder.TryDecode(document,entry,out lightData,out _);
        SmoFontData? fontData = null;
        if (entry.TypeHash == SmoClassIds.Font)
            SmoFontDecoder.TryDecode(document,entry,out fontData,out _);
        SmoTextRenderableData? textRenderable = null;
        if (entry.TypeHash == SmoClassIds.TextRenderable)
            SmoTextRenderableDecoder.TryDecode(document,entry,out textRenderable,out _);
        SmoTextNodeData? textNode = null;
        if (entry.TypeHash == SmoClassIds.TextNode)
            SmoTextNodeDecoder.TryDecode(document,entry,out textNode,out _);
        for (int fieldIndex = 0; fieldIndex < fields.Count; fieldIndex++)
        {
            SmoObjectField field = fields[fieldIndex];
            if (!SmoSerializedFieldRegistry.TryDescribeField(
                    entry.TypeHash,
                    fields,
                    fieldIndex,
                    out SmoSerializedFieldDescriptor? descriptor) ||
                descriptor is null)
            {
                continue;
            }

            bool decoded = TryFormatValue(
                document, descriptor.Key, field, textureData, materialData,meshData,
                modelData,skinData,staticRenderObject,collisionInfo,
                meshBoundingVolume,partitionRenderable,partitionNode,
                octreeNode,bspNode,partitionSystem,zone,zonePortal,zonePortalNode,
                occlusionVolume,meshNavigationSet,
                navigationPortal,navigationGraph,
                skyBox,
                particleSystem,
                animTextureController,
                lensFlare,
                lightData,
                fontData,textRenderable,textNode,
                out string value);
            result.Add(new SmoSerializedFieldValue(
                descriptor,
                field.Occurrence,
                field.SizeKind,
                field.PayloadSize,
                field.AbsolutePayloadOffset,
                decoded ? value : "complex payload; hexadecimal preview only",
                decoded,
                FormatHexPreview(field.Payload.Span)));
        }
        return result.AsReadOnly();
    }

    private static string FormatLoadedLensElement(SmoLensFlareElement element)
    {
        string material=element.Material is { } target?$"[{target.Index}] {target.Name.TrimEnd('\0')}":"NULL";
        return $"material={material}; color={FormatArgb(element.Color)}; relative distance="+
            $"{element.RelativeDistance.ToString("G9",CultureInfo.InvariantCulture)}; "+
            $"scale={element.Scale.ToString("G9",CultureInfo.InvariantCulture)}";
    }

    private static bool TryFormatValue(
        SmoDocument document,
        string key,
        SmoObjectField field,
        SmoTextureDataInfo? textureData,
        SmoMaterialDataInfo? materialData,
        SmoMeshDataInfo? meshData,
        SmoModelData? modelData,
        SmoSkin? skinData,
        SmoStaticRenderObjectData? staticRenderObject,
        SmoCollisionInfoData? collisionInfo,
        SmoMeshBoundingVolumeData? meshBoundingVolume,
        SmoPartitionRenderableData? partitionRenderable,
        SmoPartitionNodeData? partitionNode,
        SmoOctreeNodeData? octreeNode,
        SmoBspNodeData? bspNode,
        SmoPartitionSystemData? partitionSystem,
        SmoZoneData? zone,
        SmoZonePortalData? zonePortal,
        SmoZonePortalNodeData? zonePortalNode,
        SmoOcclusionVolumeData? occlusionVolume,
        SmoMeshNavigationSetData? meshNavigationSet,
        SmoNavigationPortalData? navigationPortal,
        SmoNavigationGraphData? navigationGraph,
        SmoSkyBoxData? skyBox,
        SmoParticleSystemData? particleSystem,
        SmoAnimTextureControllerData? animTextureController,
        SmoLensFlareData? lensFlare,
        SmoLightData? lightData,
        SmoFontData? fontData,
        SmoTextRenderableData? textRenderable,
        SmoTextNodeData? textNode,
        out string value)
    {
        value = string.Empty;
        ReadOnlySpan<byte> payload = field.Payload.Span;
        switch (key)
        {
            case "node.position":
            case "node.scale":
                return TryFormatVectors(payload, 1, out value);
            case "node.rotation":
                if (payload.Length != SmoNodeDecoder.QuaternionPayloadSize)
                    return false;
                value =
                    $"(X={FormatSingle(payload, 0)}, " +
                    $"Y={FormatSingle(payload, 4)}, " +
                    $"Z={FormatSingle(payload, 8)}, " +
                    $"W={FormatSingle(payload, 12)})";
                return true;
            case "node.is_bone":
            case "node.is_static":
            case "node.is_animated":
                return TryFormatBoolean(payload, out value);
            case "node.billboard_axis":
                if (payload.Length != sizeof(uint))
                    return false;
                uint billboardAxis = ReadUInt32(payload, 0);
                if (billboardAxis is not 1 and not 2)
                    return false;
                value = $"{billboardAxis} (engine axis {billboardAxis})";
                return true;
            case "node.child":
            case "node.collision":
            case "render_node.renderable":
                return TryFormatNodeRelationship(document, payload, out value);

            case "mesh.cross_platform":
                if (meshData?.CrossPlatform is null)
                    return false;
                value = FormatMeshRepresentation(meshData.CrossPlatform);
                return true;
            case "mesh.platform_specific":
                if (meshData?.PlatformSpecific is null)
                    return false;
                value = FormatMeshRepresentation(meshData.PlatformSpecific);
                return true;
            case "mesh.bounding_box":
                if (meshData?.BoundingBox is null)
                    return false;
                value = $"minimum={FormatVector3(meshData.BoundingBox.Minimum)}; " +
                        $"maximum={FormatVector3(meshData.BoundingBox.Maximum)}";
                return true;

            case "renderable.material":
            case "renderable.fog":
                return (modelData is not null || skinData is not null ||
                        particleSystem is not null || lensFlare is not null ||
                        textRenderable is not null) &&
                       TryFormatNodeRelationship(document,payload,out value);
            case "model.base_mesh":
                return (modelData is not null || skinData is not null) &&
                       TryFormatNodeRelationship(document,payload,out value);
            case "renderable.alpha_sort":
                uint? alphaSort = modelData?.Renderable.AlphaSortEnable ??
                                  skinData?.Renderable.AlphaSortEnable ??
                                  particleSystem?.Renderable.AlphaSortEnable ??
                                  lensFlare?.Renderable.AlphaSortEnable ??
                                  textRenderable?.Renderable.AlphaSortEnable;
                if (alphaSort is not (0 or 1))
                    return false;
                value = alphaSort == 0
                    ? "false (0)"
                    : "true (1)";
                return true;
            case "renderable.priority":
                return (modelData is not null || skinData is not null ||
                        particleSystem is not null || lensFlare is not null ||
                        textRenderable is not null) &&
                       TryFormatUInt32(payload,out value);
            case "model.projection_group":
                return (modelData is not null || skinData is not null) &&
                       TryFormatUInt32(payload,out value);
            case "skin.palette":
                if (skinData is null)
                    return false;
                int inlineBones = skinData.Bones.Count(item =>
                    item.Encoding == SmoNodeRelationshipEncoding.InlineObject);
                string[] previewBones = skinData.Bones.Take(8).Select(item =>
                {
                    SmoObjectEntry node = document.Objects[item.NodeObjectIndex];
                    return $"{item.PaletteIndex}:[{node.Index}] " +
                           $"\"{node.Name.TrimEnd('\0')}\"";
                }).ToArray();
                string suffix = skinData.Bones.Count > previewBones.Length
                    ? ", …"
                    : string.Empty;
                value = $"blend-influence hint={skinData.BlendInfluenceCountHint}; " +
                        $"slots={skinData.Bones.Count}; inline={inlineBones}; " +
                        $"references={skinData.Bones.Count - inlineBones}; " +
                        $"nodes=[{string.Join(", ",previewBones)}{suffix}]";
                return true;

            case "static_render_object.renderable":
                return staticRenderObject is not null &&
                       TryFormatNodeRelationship(document,payload,out value);
            case "static_render_object.transform":
                if (staticRenderObject is null)
                    return false;
                value = FormatMatrix(staticRenderObject.Transform);
                return true;
            case "static_render_object.inverse_transform":
                if (staticRenderObject is null)
                    return false;
                value = FormatMatrix(staticRenderObject.EngineInverseTransform);
                return true;

            case "collision_info.primitive":
                return collisionInfo is not null &&
                       TryFormatNodeRelationship(document,payload,out value);
            case "collision_info.group":
                return collisionInfo is not null &&
                       TryFormatUInt32(payload,out value);
            case "collision_info.transform":
                if (collisionInfo?.Transform is not SmoCollisionInfoTransform transform)
                    return false;
                value = $"position={FormatVector3(transform.Position)}; " +
                        $"rotation=({FormatSingle(transform.Rotation.X)}, " +
                        $"{FormatSingle(transform.Rotation.Y)}, " +
                        $"{FormatSingle(transform.Rotation.Z)}, " +
                        $"{FormatSingle(transform.Rotation.W)}); " +
                        $"scale={FormatVector3(transform.Scale)}";
                return true;

            case "mesh_bv.geometry":
                if (meshBoundingVolume is null)
                    return false;
                Vector3 minimum = Vector3.Zero;
                Vector3 maximum = Vector3.Zero;
                if (meshBoundingVolume.Positions.Count > 0)
                {
                    minimum = meshBoundingVolume.Positions[0];
                    maximum = minimum;
                    for (int index = 1;
                         index < meshBoundingVolume.Positions.Count;
                         index++)
                    {
                        minimum = Vector3.Min(
                            minimum,meshBoundingVolume.Positions[index]);
                        maximum = Vector3.Max(
                            maximum,meshBoundingVolume.Positions[index]);
                    }
                }
                int degenerateTriangles = 0;
                for (int triangle = 0;
                     triangle < meshBoundingVolume.TriangleIndices.Count;
                     triangle += 3)
                {
                    Vector3 a = meshBoundingVolume.Positions[
                        meshBoundingVolume.TriangleIndices[triangle]];
                    Vector3 b = meshBoundingVolume.Positions[
                        meshBoundingVolume.TriangleIndices[triangle + 1]];
                    Vector3 c = meshBoundingVolume.Positions[
                        meshBoundingVolume.TriangleIndices[triangle + 2]];
                    if (Vector3.Cross(b - a,c - a).LengthSquared() <= 1e-12f)
                        degenerateTriangles++;
                }
                value = $"version={meshBoundingVolume.Version}; " +
                        $"triangles={meshBoundingVolume.TriangleCount}; " +
                        $"vertices={meshBoundingVolume.VertexCount}; " +
                        $"bounds={FormatVector3(minimum)}..{FormatVector3(maximum)}; " +
                        $"degenerate triangles={degenerateTriangles}";
                return true;

            case "mesh_bv.face_data":
                if (meshBoundingVolume?.FaceData is not { } faces)
                    return false;
                value = $"wxFaceData=0x{SmoMeshBoundingVolumeDecoder.FaceDataClassId:X8}; " +
                        $"faces={faces.Count}; surface types=" +
                        FormatSurfaceTypeHistogram(faces) +
                        "; flags=" +
                        FormatValueHistogram(faces.Select(item => (uint)item.Flags)) +
                        "; surface IDs=" +
                        FormatValueHistogram(faces.Select(item => (uint)item.SurfaceId));
                return true;

            case "partition_renderable.debug_color":
                if (partitionRenderable is null || payload.Length != sizeof(uint))
                    return false;
                uint debugColor = partitionRenderable.DebugColorArgb;
                value = $"{FormatArgb(debugColor)} " +
                        $"(A={(debugColor >> 24) & 0xFF}, " +
                        $"R={(debugColor >> 16) & 0xFF}, " +
                        $"G={(debugColor >> 8) & 0xFF}, B={debugColor & 0xFF})";
                return true;
            case "partition_renderable.renderable":
                return partitionRenderable is not null &&
                       TryFormatNodeRelationship(document,payload,out value);

            case "partition_node.debug_color":
                if (partitionNode is null || payload.Length != sizeof(uint))
                    return false;
                uint partitionColor = partitionNode.DebugColorArgb;
                value = $"{FormatArgb(partitionColor)} " +
                        $"(A={(partitionColor >> 24) & 0xFF}, " +
                        $"R={(partitionColor >> 16) & 0xFF}, " +
                        $"G={(partitionColor >> 8) & 0xFF}, " +
                        $"B={partitionColor & 0xFF})";
                return true;
            case "partition_node.partition_system":
            case "partition_node.zone":
            case "partition_node.collision_info":
            case "partition_node.zone_portal":
            case "partition_node.static_render_object":
            case "partition_node.partition_renderable":
                return partitionNode is not null &&
                       TryFormatNodeRelationship(document,payload,out value);
            case "partition_node.child":
                return partitionNode is not null &&
                       TryFormatIndexedPartitionChild(
                           document,field,payload,out value);

            case "octree_node.pivot":
                if (octreeNode is null)
                    return false;
                value = FormatVector3(octreeNode.Pivot);
                return true;
            case "octree_node.minimum":
                if (octreeNode is null)
                    return false;
                value = FormatVector3(octreeNode.Minimum);
                return true;
            case "octree_node.maximum":
                if (octreeNode is null)
                    return false;
                value = FormatVector3(octreeNode.Maximum);
                return true;

            case "partition_system.partition_root":
                return partitionSystem is not null &&
                       TryFormatNodeRelationship(document,payload,out value);

            case "zone.local_partition_root":
                return zone is not null &&
                       TryFormatNodeRelationship(document,payload,out value);

            case "zone_portal.destination_zone":
                return zonePortal is not null &&
                       TryFormatNodeRelationship(document,payload,out value);
            case "zone_portal.polygon":
                if (zonePortal is null)
                    return false;
                value = FormatLoadedPolygon(zonePortal.Polygon);
                return true;
            case "zone_portal.open":
                if (zonePortal is null)
                    return false;
                value = $"loaded={zonePortal.IsOpen}; raw byte={zonePortal.OpenByte}";
                return true;

            case "zone_portal_node.zone_portal":
                return zonePortalNode is not null &&
                       TryFormatNodeRelationship(document,payload,out value);

            case "occlusion_volume.index_buffer":
                if (occlusionVolume is null)
                    return false;
                value = $"triangle list; triangles=" +
                        $"{occlusionVolume.IndexBuffer.PrimitiveCount}; " +
                        $"indices={occlusionVolume.IndexBuffer.TriangleIndices.Count}; " +
                        "format=UInt16";
                return true;
            case "occlusion_volume.vertex_buffer":
                if (occlusionVolume is null)
                    return false;
                IReadOnlyList<Vector3> occlusionPositions =
                    occlusionVolume.VertexBuffer.Positions;
                Vector3 occlusionMinimum = occlusionPositions[0];
                Vector3 occlusionMaximum = occlusionPositions[0];
                for (int index = 1;index < occlusionPositions.Count;index++)
                {
                    occlusionMinimum = Vector3.Min(
                        occlusionMinimum,occlusionPositions[index]);
                    occlusionMaximum = Vector3.Max(
                        occlusionMaximum,occlusionPositions[index]);
                }
                value = $"position-only; vertices={occlusionPositions.Count}; " +
                        $"bounds={FormatVector3(occlusionMinimum)}.." +
                        FormatVector3(occlusionMaximum);
                return true;

            case "texture.cross_platform":
            case "texture.source_embedded":
                if (textureData is null)
                    return false;
                value = FormatTextureData(textureData);
                return true;
            case "texture.platform_type":
                if (payload.Length != sizeof(uint))
                    return false;
                uint platformType = ReadUInt32(payload, 0);
                value = $"{platformType} ({GetTexturePlatformTypeName(platformType)})";
                return true;
            case "texture.source_none":
                return TryFormatBoolean(payload, out value);
            case "texture.source_reference":
                return false;

            case "material.render_states":
                if (materialData is null)
                    return false;
                value = $"[{string.Join(", ", materialData.RenderStates)}]";
                return true;
            case "material.vertex_alpha":
                if (materialData is null)
                    return false;
                value = $"{materialData.UsesVertexAlpha.ToString().ToLowerInvariant()} " +
                        $"({(materialData.UsesVertexAlpha ? 1 : 0)})";
                return true;
            case "material.color":
                if (materialData is null)
                    return false;
                value = $"ambient={FormatArgb(materialData.Color.AmbientArgb)}, " +
                        $"diffuse={FormatArgb(materialData.Color.DiffuseArgb)}, " +
                        $"specular={FormatArgb(materialData.Color.SpecularArgb)}, " +
                        $"emissive={FormatArgb(materialData.Color.EmissiveArgb)}, " +
                        $"power={FormatSingle(materialData.Color.SpecularPower)}";
                return true;
            case "material.pass":
                if (!TryGetMaterialPass(materialData, field.FieldType, field.Occurrence,
                        out SmoMaterialPassData? pass))
                    return false;
                SmoMaterialRenderStateInfo classification =
                    SmoMaterialRenderState.Classify(
                        pass!.FinalBlendOperation, materialData!.RenderStates);
                value = $"pass {pass.Index}; FinalBlendOp={pass.FinalBlendOperation} " +
                        $"(0x{pass.FinalBlendOperation:X}, {classification.BlendMode})";
                return true;
            case "material.layer":
                if (!TryGetMaterialPass(materialData, field.FieldType, field.Occurrence,
                        out pass))
                    return false;
                value = $"pass {pass!.Index}; 0x{pass.LayerClassId:X8} " +
                        $"({SmoClassRegistry.GetDisplayName(pass.LayerClassId)})";
                return true;
            case "material.texture_states_legacy":
            case "material.texture_states":
                if (!TryGetMaterialPass(materialData, field.FieldType, field.Occurrence,
                        out pass))
                    return false;
                value = $"pass {pass!.Index}; " +
                        $"[{string.Join(", ", pass.TextureStates)}]; " +
                        (pass.TextureStatesFieldType ==
                            SmoMaterialDataDecoder.LegacyTextureStatesField
                                ? "legacy field 8"
                                : "current field 17");
                return true;
            case "material.static_uv_transform":
                if (!TryGetMaterialPass(materialData, field.FieldType, field.Occurrence,
                        out pass) || pass!.StaticUvTransform is null)
                    return false;
                value = $"pass {pass.Index}; enabled=" +
                        $"{pass.StaticUvTransform.Enabled.ToString().ToLowerInvariant()}; " +
                        $"matrix3x3=[{string.Join(", ",
                            pass.StaticUvTransform.Matrix3x3.Select(FormatSingle))}]";
                return true;
            case "material.texture":
                if (!TryGetMaterialPass(materialData, field.FieldType, field.Occurrence,
                        out pass) || pass!.Texture is null)
                    return false;
                value = $"pass {pass.Index}; " +
                        FormatMaterialRelationship(document, pass.Texture);
                return true;
            case "material.animation_controller":
                if (!TryGetMaterialPass(materialData, field.FieldType, field.Occurrence,
                        out pass) || pass!.AnimationController is null)
                    return false;
                value = $"pass {pass.Index}; " +
                        FormatMaterialRelationship(document, pass.AnimationController);
                return true;
            case "material.uv_controller":
                if (!TryGetMaterialPass(materialData, field.FieldType, field.Occurrence,
                        out pass) || pass!.UvController is null)
                    return false;
                value = $"pass {pass.Index}; " +
                        FormatMaterialRelationship(document, pass.UvController);
                return true;
            case "material.color_controller":
                if (materialData is null)
                    return false;
                value = FormatMaterialRelationship(
                    document, materialData.ColorController);
                return true;

            case "uv_controller.transform_evaluators":
                if (!SmoUvControllerDecoder.TryDecode(
                        payload, out SmoUvControllerData? uv) || uv is null)
                {
                    return false;
                }
                value =
                    $"translation=[X={FormatEvaluator(uv.TranslationX)}, " +
                    $"Y={FormatEvaluator(uv.TranslationY)}, " +
                    $"Z={FormatEvaluator(uv.TranslationZ)}]; " +
                    $"scale=[X={FormatEvaluator(uv.ScaleX)}, " +
                    $"Y={FormatEvaluator(uv.ScaleY)}, " +
                    $"Z={FormatEvaluator(uv.ScaleZ)}]; " +
                    $"rotation={FormatEvaluator(uv.Rotation)}; " +
                    $"UV pivot={FormatVector3(uv.UvPivot)}; " +
                    $"rotation axis={FormatVector3(uv.RotationAxis)}";
                return true;

            case "material_color_controller.evaluators":
                if (!SmoMaterialColorControllerDecoder.TryDecode(
                        payload, out SmoMaterialColorControllerData? controller) ||
                    controller is null)
                {
                    return false;
                }
                value =
                    $"ambient={FormatColorEvaluator(controller.Ambient)}; " +
                    $"diffuse={FormatColorEvaluator(controller.Diffuse)}; " +
                    $"specular={FormatColorEvaluator(controller.Specular)}; " +
                    $"emissive={FormatColorEvaluator(controller.Emissive)}; " +
                    $"alpha={FormatEvaluator(controller.Alpha)}";
                return true;

            case "fog.parameters":
                if (!SmoFogDecoder.TryDecode(payload, out SmoFogData? fog) ||
                    fog is null)
                {
                    return false;
                }
                value =
                    $"type={fog.Type} ({SmoFogDecoder.GetTypeName(fog.Type)}), " +
                    $"color={FormatArgb(fog.Color)}, " +
                    $"start={FormatSingle(fog.Start)}, " +
                    $"end={FormatSingle(fog.End)}, " +
                    $"density={FormatSingle(fog.Density)}";
                return true;

            case "obb.position":
                if (!SmoOrientedBoxBoundingVolumeDecoder.TryDecodePosition(
                        payload, out Vector3 position))
                {
                    return false;
                }
                value = FormatVector3(position);
                return true;

            case "obb.size":
                if (!SmoOrientedBoxBoundingVolumeDecoder.TryDecodeSize(
                        payload, out SmoOrientedBoxSizeData? size) || size is null)
                {
                    return false;
                }
                value = $"full={FormatVector3(size.FullSize)}, " +
                        $"half-extents={FormatVector3(size.HalfExtents)}, " +
                        $"sphere radius={FormatSingle(size.BoundingSphereRadius)}";
                return true;

            case "obb.rotation":
                if (!SmoOrientedBoxBoundingVolumeDecoder.TryDecodeRotation(
                        payload, out Quaternion rotation))
                {
                    return false;
                }
                value = $"(X={FormatSingle(rotation.X)}, " +
                        $"Y={FormatSingle(rotation.Y)}, " +
                        $"Z={FormatSingle(rotation.Z)}, " +
                        $"W={FormatSingle(rotation.W)})";
                return true;

            case "box_bv.position":
                if (!SmoBoxBoundingVolumeDecoder.TryDecodePosition(
                        payload, out Vector3 boxPosition))
                {
                    return false;
                }
                value = FormatVector3(boxPosition);
                return true;

            case "box_bv.size":
                if (!SmoBoxBoundingVolumeDecoder.TryDecodeSize(
                        payload, out SmoBoxSizeData? boxSize) || boxSize is null)
                {
                    return false;
                }
                value = $"full={FormatVector3(boxSize.FullSize)}, " +
                        $"half-extents={FormatVector3(boxSize.HalfExtents)}, " +
                        $"sphere radius={FormatSingle(boxSize.BoundingSphereRadius)}";
                return true;

            case "sphere_bv.position":
                if (!SmoSphereBoundingVolumeDecoder.TryDecodePosition(
                        payload,out Vector3 spherePosition))
                {
                    return false;
                }
                value = FormatVector3(spherePosition);
                return true;

            case "sphere_bv.radius":
                if (!SmoSphereBoundingVolumeDecoder.TryDecodeRadius(
                        payload,out float sphereRadius))
                {
                    return false;
                }
                value = FormatSingle(sphereRadius);
                return true;

            case "light.type":
                if (lightData is null) return false;
                value = $"{lightData.Type} ({SmoLightDataDecoder.GetTypeName(lightData.Type)}); effective state";
                return true;
            case "navigation_graph.path_table_size":
                return navigationGraph is not null &&
                       TryFormatUInt32(payload, out value);
            case "light.project_shadow":
            case "light.attenuation":
            case "light.enabled":
                if (lightData is null) return false;
                bool lightFlag = key == "light.project_shadow" ? lightData.ProjectShadowVolume :
                    key == "light.attenuation" ? lightData.AttenuationEnabled : lightData.Enabled;
                value = $"{lightFlag}; effective state";
                return true;
            case "particle.loop":
            case "particle.world_space":
            case "particle.iterative":
                if(particleSystem is null)return false;
                byte mode=key=="particle.loop"?particleSystem.LoopAnimation:key=="particle.world_space"?particleSystem.WorldSpace:particleSystem.IterativeMode;
                value=$"{mode!=0} (raw {mode})";return true;
            case "light.color":
                if (lightData is null) return false;
                var lightColor = lightData.ColorRgba;
                value = $"RGBA=({FormatSingle(lightColor.X)}, {FormatSingle(lightColor.Y)}, " +
                    $"{FormatSingle(lightColor.Z)}, {FormatSingle(lightColor.W)}); effective state";
                return true;
            case "light.intensity":
            case "light.range":
            case "light.hotspot_angle":
            case "light.falloff_angle":
                if (lightData is null) return false;
                float lightScalar = key switch
                {
                    "light.intensity" => lightData.Intensity,
                    "light.range" => lightData.Range,
                    "light.hotspot_angle" => lightData.HotspotAngle,
                    _ => lightData.FalloffAngle
                };
                value = $"{FormatSingle(lightScalar)}; effective state";
                return true;
            case "particle.rate":
            case "particle.bounding_sphere":
                if(particleSystem is null)return false;
                value=FormatSingle(key=="particle.rate"?particleSystem.EmissionRate:particleSystem.BoundingSphere);return true;
            case "particle.acceleration":
                if(particleSystem is null)return false;
                value=$"{FormatVector3(particleSystem.AccelerationBegin)} .. {FormatVector3(particleSystem.AccelerationEnd)}";return true;
            case "particle.direction":
                if(particleSystem is null)return false;
                value=FormatVector3(particleSystem.EmissionDirection);return true;
            case "particle.velocity":
            case "particle.angle":
            case "particle.scale":
            case "particle.time":
                if(particleSystem is null)return false;
                SmoSingleRange range=key=="particle.velocity"?particleSystem.Velocity:key=="particle.angle"?particleSystem.Angle:key=="particle.scale"?particleSystem.Scale:particleSystem.Time;
                value=$"{FormatSingle(range.Begin)} .. {FormatSingle(range.End)}";return true;
            case "particle.color":
                if(particleSystem is null)return false;
                value=$"[{FormatArgb(particleSystem.Color.Begin)}, {FormatArgb(particleSystem.Color.End)}]";return true;
            case "particle.region.point":
            case "particle.region.sphere":
            case "particle.region.disk":
            case "particle.region.plane":
            case "particle.region.box":
            case "particle.region.cylinder":
            case "particle.region.cone":
                if(particleSystem is null)return false;
                value=particleSystem.Region switch {
                    SmoParticlePointRegion p=>$"position={FormatVector3(p.Position)}",
                    SmoParticlePlaneRegion p=>$"position={FormatVector3(p.Position)}; normal={FormatVector3(p.Normal)}; size=({FormatSingle(p.SizeX)}, {FormatSingle(p.SizeY)})",
                    SmoParticleBoxRegion p=>$"position={FormatVector3(p.Position)}; size={FormatVector3(p.Size)}",
                    SmoParticleSphereRegion p=>$"center={FormatVector3(p.Position)}; radius={FormatSingle(p.Radius)}",
                    SmoParticleDiskRegion p=>$"center={FormatVector3(p.Position)}; radius={FormatSingle(p.Radius)}",
                    SmoParticleCylinderRegion p=>$"position={FormatVector3(p.Position)}; radius={FormatSingle(p.Radius)}; height={FormatSingle(p.Height)}",
                    SmoParticleConeRegion p=>$"position={FormatVector3(p.Position)}; radii=({FormatSingle(p.Radius1)}, {FormatSingle(p.Radius2)}); height={FormatSingle(p.Height)}",
                    _=>string.Empty };return value.Length>0;

            case "bsp.plane":
                if (bspNode?.Plane is not { } plane)
                    return false;
                value =
                    $"loaded normal={FormatVector3(plane.Normal)}, " +
                    $"constant={FormatSingle(plane.Constant)}";
                return true;
            case "bsp.polygon":
                if (bspNode is null)
                    return false;
                value = FormatLoadedPolygon(bspNode.Polygon);
                return true;

            case "navigation_graph.set":
            case "navigation_graph.portal":
            case "particle.render_node":
                return (navigationGraph is not null ||
                        (key == "particle.render_node" && particleSystem is not null)) &&
                       TryFormatRelationships(document, payload, 1, out value);
            case "navigation_graph.path":
                if (navigationGraph is null || payload.Length < SmoNavigationGraphDecoder.PathHeaderSize)
                {
                    return false;
                }
                uint sourceSet=BinaryPrimitives.ReadUInt32LittleEndian(payload);
                uint destinationSet=BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]);
                if(sourceSet>=navigationGraph.PathTableSize||destinationSet>=navigationGraph.PathTableSize)return false;
                SmoNavigationGraphPath graphPath = navigationGraph[(int)sourceSet,(int)destinationSet];
                string alternatives = string.Join(", ",graphPath.Alternatives
                    .Take(16).Select((item,index) =>
                        $"{index}:portal {item.FirstPortalIndex}/reserved {item.Reserved}"));
                if (graphPath.Alternatives.Count > 16)
                    alternatives += ", …";
                value = $"set {graphPath.SourceSetIndex} -> " +
                        $"{graphPath.DestinationSetIndex}; next portal=" +
                        (graphPath.IsReachable
                            ? graphPath.NextPortalIndex.ToString(CultureInfo.InvariantCulture)
                            : "255 (unreachable/self)") +
                        $"; alternatives={graphPath.Alternatives.Count}; [{alternatives}]";
                return true;
            case "navigation_portal.graph":
                return navigationPortal is not null &&
                       TryFormatRelationships(document,payload,1,out value);
            case "navigation_set.node_count":
                return meshNavigationSet is not null &&
                       TryFormatUInt32(payload,out value);
            case "navigation_set.transition_table":
                if (meshNavigationSet is null)
                    return false;
                value = FormatNavigationTransitionTable(
                    meshNavigationSet.NodeTransitions,"nodes");
                return true;
            case "navigation_set.portal_transition_table":
                if (meshNavigationSet is null)
                    return false;
                value = FormatNavigationTransitionTable(
                    meshNavigationSet.PortalTransitions,"portals x nodes");
                return true;
            case "navigation_set.links":
                if (meshNavigationSet is null)
                    return false;
                value = FormatNavigationLinks(meshNavigationSet);
                return true;
            case "navigation_set.portal":
            case "navigation_set.mesh":
                return meshNavigationSet is not null &&
                       TryFormatNodeRelationship(document,payload,out value);
            case "navigation_set.enabled":
                return meshNavigationSet is not null &&
                       TryFormatBoolean(payload,out value);
            case "navigation_portal.sets":
                return navigationPortal is not null &&
                       TryFormatRelationships(document, payload, 2, out value);
            case "navigation_portal.nodes":
                if (navigationPortal is null || payload.Length != 2)
                    return false;
                value = $"endpoint node IDs=[{payload[0]}, {payload[1]}]";
                return true;
            case "navigation_portal.path":
                if (navigationPortal is null || payload.Length != 3)
                    return false;
                value = $"set {payload[0]} -> {payload[1]}; " +
                        $"member of alternative path {payload[2]}";
                return true;
            case "sky_box.model":
                return skyBox is not null &&
                       TryFormatRelationships(document,payload,1,out value);
            case "anim_texture.frames":
                if (animTextureController is null)
                    return false;
                int inlineTextures=animTextureController.Frames.Count(frame=>
                    frame.Texture.Encoding==SmoNodeRelationshipEncoding.InlineObject);
                string framePreview=string.Join(", ",animTextureController.Frames
                    .Take(12).Select((frame,index)=>
                        $"{index}:{frame.Time.ToString("G7",CultureInfo.InvariantCulture)}s"));
                if (animTextureController.Frames.Count>12) framePreview+=", …";
                value=$"frames={animTextureController.Frames.Count}; duration="+
                      $"{animTextureController.Duration.ToString("G7",CultureInfo.InvariantCulture)}s; "+
                      $"inline textures={inlineTextures}; references="+
                      $"{animTextureController.Frames.Count-inlineTextures}; [{framePreview}]";
                return true;
            case "lens_flare.element":
                if(lensFlare is null)return false;
                value=FormatLoadedLensElement(lensFlare.Primary);return true;
            case "lens_flare.glare":
                if(lensFlare is null)return false;
                value=$"elements={lensFlare.Elements.Count}; ["+
                    string.Join("; ",lensFlare.Elements.Take(12).Select(FormatLoadedLensElement))+"]";
                return true;
            case "lens_flare.occlusion":
                if (lensFlare is null) return false;
                value=$"sphere radius={lensFlare.OcclusionSphereRadius.ToString("G9",CultureInfo.InvariantCulture)}; "+
                      $"speed={lensFlare.OcclusionSpeed.ToString("G9",CultureInfo.InvariantCulture)}";
                return true;
            case "lens_flare.render_node":
                return lensFlare is not null&&
                       TryFormatRelationships(document,payload,1,out value);
            case "font.data":
                if (fontData is null) return false;
                string glyphPreview=string.Join(", ",fontData.Glyphs.Take(12)
                    .Select(glyph=>$"0x{glyph.Character:X2}:{glyph.Width}px"));
                value=$"image={FormatDecodedRelationship(document,fontData.Image)}; "+
                      $"height={fontData.Height}; baseline={fontData.Baseline}; "+
                      $"glyphs={fontData.Glyphs.Count}; [{glyphPreview}, …]";
                return true;
            case "text_renderable.font":
                return textRenderable is not null&&
                       TryFormatRelationships(document,payload,1,out value);
            case "text_renderable.text":
                if (textRenderable is null) return false;
                value=$"\"{textRenderable.Text}\" (byte string)";return true;
            case "text_renderable.color":
                if (textRenderable is null) return false;
                value=FormatArgb(textRenderable.Color);return true;
            case "text_renderable.wrap_width":
                if (textRenderable?.WrapWidth is not uint wrapWidth) return false;
                value=wrapWidth.ToString(CultureInfo.InvariantCulture);return true;
            case "text_renderable.alignment":
                if (textRenderable?.Alignment is not uint alignment) return false;
                value=alignment.ToString(CultureInfo.InvariantCulture);return true;
            default:
                return false;
        }
    }

    private static string FormatNavigationTransitionTable(
        SmoNavigationTransitionTable table,string dimensions)
    {
        string histogram = string.Join(", ",table.LinkSelectors
            .GroupBy(value => value)
            .OrderBy(group => group.Key)
            .Select(group => $"{group.Key}:{group.Count()}"));
        string preview = string.Join(", ",table.LinkSelectors.Take(16));
        if (table.LinkSelectors.Count > 16)
            preview += ", …";
        return $"{dimensions}={table.RowCount}x{table.ColumnCount}; " +
               $"selectors={table.LinkSelectors.Count}; histogram=[{histogram}]; " +
               $"preview=[{preview}]";
    }

    private static string FormatNavigationLinks(SmoMeshNavigationSetData value)
    {
        string preview = string.Join("; ",value.Links.Take(12).Select(item =>
            $"{item.NodeId}->[{string.Join(",",item.NeighbourNodeIds)}]"));
        if (value.Links.Count > 12)
            preview += "; …";
        int minimumDegree = value.Links.Min(item => item.NeighbourNodeIds.Count);
        int maximumDegree = value.Links.Max(item => item.NeighbourNodeIds.Count);
        return $"nodes={value.NodeCount}; directed links={value.LinkCount}; " +
               $"degree={minimumDegree}..{maximumDegree}; " +
               $"reciprocal={value.HasReciprocalLinks}; [{preview}]";
    }

    private static string FormatMeshRepresentation(
        SmoMeshRepresentationData representation)
    {
        if (representation.Geometry is SmoMesh mesh)
        {
            string primitive = mesh.PrimitiveType switch
            {
                SmoMeshDecoder.TriangleListPrimitive => "triangle list",
                SmoMeshDecoder.TriangleStripPrimitive => "triangle strip",
                _ => $"primitive {mesh.PrimitiveType}"
            };
            return $"{representation.Kind}; format=0x{mesh.VertexFormat:X4}; " +
                   $"{primitive}; vertices={mesh.VertexCount}; " +
                   $"stored indices={mesh.IndexCount}; triangles={mesh.TriangleCount}; " +
                   $"stride={mesh.Stride}/{mesh.RuntimeStride}; " +
                   $"payload={representation.PayloadSize} bytes";
        }
        if (representation.Pc is SmoPcMeshData pc)
        {
            string primitive = pc.PrimitiveType == SmoMeshDecoder.TriangleListPrimitive
                ? "triangle list"
                : "triangle strip";
            return $"{representation.Kind}; format=0x{pc.VertexFormat:X4}; " +
                   $"{primitive}; primitive count={pc.PrimitiveCount}; " +
                   $"vertices={pc.VertexCount}; stride=" +
                   $"{pc.SerializedStride}/{pc.RuntimeStride}; " +
                   $"payload={representation.PayloadSize} bytes; " +
                   "geometry attributes rejected by strict preview decoder";
        }
        if (representation.Ps2Native is SmoPs2NativeMeshData ps2)
        {
            return $"PS2 native; sphere={FormatVector4(ps2.BoundingSphere)}; " +
                   $"primitive count={ps2.PrimitiveCount}; vertices={ps2.VertexCount}; " +
                   $"format=0x{ps2.VertexFormat:X4}; DMA={ps2.DmaQwordCount} qwords/" +
                   $"{ps2.DmaPayloadSize} bytes; additional UV channels=" +
                   $"{ps2.AdditionalTextureCoordinateCount}; blend weights=" +
                   $"{ps2.BlendWeightCount}";
        }
        return $"{representation.Kind}; payload={representation.PayloadSize} bytes";
    }

    private static string FormatMatrix(Matrix4x4 value)
    {
        Vector3 axisX = new(value.M11,value.M12,value.M13);
        Vector3 axisY = new(value.M21,value.M22,value.M23);
        Vector3 axisZ = new(value.M31,value.M32,value.M33);
        Vector3 translation = new(value.M41,value.M42,value.M43);
        return $"translation={FormatVector3(translation)}; " +
               $"basis X={FormatVector3(axisX)}; " +
               $"Y={FormatVector3(axisY)}; Z={FormatVector3(axisZ)}; " +
               $"axis lengths=({FormatSingle(axisX.Length())}, " +
               $"{FormatSingle(axisY.Length())}, {FormatSingle(axisZ.Length())})";
    }

    private static bool TryGetMaterialPass(
        SmoMaterialDataInfo? material,
        int fieldType,
        int occurrence,
        out SmoMaterialPassData? pass)
    {
        pass = null;
        if (material is null || occurrence < 0)
            return false;
        IEnumerable<SmoMaterialPassData> candidates = fieldType switch
        {
            3 or 4 => material.Passes,
            8 or 17 => material.Passes.Where(
                item => item.TextureStatesFieldType == fieldType),
            9 => material.Passes.Where(item => item.StaticUvTransform is not null),
            10 => material.Passes.Where(item => item.Texture is not null),
            11 => material.Passes.Where(item => item.AnimationController is not null),
            12 => material.Passes.Where(item => item.UvController is not null),
            _ => []
        };
        pass = candidates.ElementAtOrDefault(occurrence);
        return pass is not null;
    }

    private static string FormatMaterialRelationship(
        SmoDocument document,
        SmoMaterialRelationshipData relationship)
    {
        if (relationship.StorageKind == SmoMaterialRelationshipStorageKind.NullId)
            return "null object ID";
        SmoObjectEntry? target = document.Objects.FirstOrDefault(item =>
            item.Id == relationship.ObjectId && item.IsWithinDataSection &&
            item.SignatureMatches);
        string storage = relationship.StorageKind switch
        {
            SmoMaterialRelationshipStorageKind.LegacyIdOnly =>
                "legacy ID-only reference",
            SmoMaterialRelationshipStorageKind.Reference => "reference",
            SmoMaterialRelationshipStorageKind.InlineObject =>
                $"inline SBOO ({relationship.InlineSize} bytes)",
            _ => relationship.StorageKind.ToString()
        };
        string targetText = target is null
            ? "unresolved"
            : $"[{target.Index}] {SmoClassRegistry.GetDisplayName(target.TypeHash)} " +
              $"\"{target.Name.TrimEnd('\0')}\"";
        return $"id=0x{relationship.ObjectId:X8}; {storage}; {targetText}";
    }

    private static bool TryFormatUInt32(
        ReadOnlySpan<byte> payload,
        out string value)
    {
        value = string.Empty;
        if (payload.Length != sizeof(uint))
            return false;
        uint number = ReadUInt32(payload, 0);
        value = $"{number} (0x{number:X8})";
        return true;
    }

    private static bool TryFormatBoolean(
        ReadOnlySpan<byte> payload,
        out string value)
    {
        value = string.Empty;
        ulong number = payload.Length switch
        {
            1 => payload[0],
            2 => BinaryPrimitives.ReadUInt16LittleEndian(payload),
            4 => BinaryPrimitives.ReadUInt32LittleEndian(payload),
            _ => ulong.MaxValue
        };
        if (number == ulong.MaxValue)
            return false;
        value = $"{(number == 0 ? "false" : "true")} ({number})";
        return true;
    }

    private static bool TryFormatArgb(
        ReadOnlySpan<byte> payload,
        out string value)
    {
        value = string.Empty;
        if (payload.Length != sizeof(uint))
            return false;
        value = FormatArgb(ReadUInt32(payload, 0));
        return true;
    }

    private static bool TryFormatSingles(
        ReadOnlySpan<byte> payload,
        int count,
        out string value)
    {
        value = string.Empty;
        if (payload.Length != count * sizeof(float))
            return false;
        var values = new string[count];
        for (int index = 0; index < count; index++)
            values[index] = FormatSingle(payload, index * sizeof(float));
        value = count == 1 ? values[0] : $"[{string.Join(", ", values)}]";
        return true;
    }

    private static bool TryFormatVectors(
        ReadOnlySpan<byte> payload,
        int count,
        out string value)
    {
        value = string.Empty;
        if (payload.Length != count * 3 * sizeof(float))
            return false;
        var values = new string[count];
        for (int index = 0; index < count; index++)
            values[index] = FormatVector3(payload, index * 3 * sizeof(float));
        value = count == 1 ? values[0] : $"[{string.Join(", ", values)}]";
        return true;
    }

    private static string FormatLoadedPolygon(IReadOnlyList<Vector3> polygon)
    {
        var vertices = polygon.Take(4).Select(FormatVector3);
        string suffix = polygon.Count > 4 ? ", …" : string.Empty;
        return $"loaded count={polygon.Count}; vertices=[{string.Join(", ", vertices)}{suffix}]";
    }

    private static string FormatDecodedRelationship(
        SmoDocument document,SmoNodeRelationship relationship)
    {
        string target=relationship.TargetObjectIndex is int index
            ? $"[{index}] {SmoClassRegistry.GetDisplayName(
                relationship.TargetTypeHash??0)} \"{document.Objects[index].Name.TrimEnd('\0')}\""
            : "unresolved";
        return $"id=0x{relationship.ObjectId:X8}, {relationship.Encoding}, {target}";
    }

    private static bool TryFormatRelationships(
        SmoDocument document,
        ReadOnlySpan<byte> payload,
        int count,
        out string value)
    {
        value = string.Empty;
        if (count < 0 || count > payload.Length / sizeof(uint))
            return false;
        Dictionary<uint, SmoObjectEntry> objectsById = document.Objects
            .GroupBy(entry => entry.Id)
            .Where(group => group.Key != 0 && group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single());
        var relationships = new string[count];
        int cursor = 0;
        for (int index = 0; index < count; index++)
        {
            ReadOnlySpan<byte> remaining = payload[cursor..];
            // The common serializer observes one bounded sequence prefix;
            // NULL consumes only its ID. The exact slice is then resolved by
            // the existing metadata adapter, including inline class/size checks.
            if (!SmoNodeDecoder.TryReadReference(remaining,
                    checked((uint)remaining.Length), 2, out var prefix))
                return false;
            int length = prefix.Id == 0 ? sizeof(uint) : checked((int)prefix.InlineSize + 2 * sizeof(uint));
            if (!SmoNodeDecoder.TryDecodeRelationship(remaining[..length], objectsById,
                    out var relationship) || relationship is null)
                return false;
            string target = relationship.TargetObjectIndex is int targetIndex
                ? $"[{targetIndex}] {SmoClassRegistry.GetDisplayName(relationship.TargetTypeHash ?? 0)} " +
                  $"\"{relationship.TargetName}\""
                : "unresolved";
            relationships[index] =
                $"id=0x{relationship.ObjectId:X8}, inline={relationship.InlineSerializedSize}, {target}";
            cursor += length;
        }
        if (cursor != payload.Length)
            return false;
        value = count == 1
            ? relationships[0]
            : $"[{string.Join("; ", relationships)}]";
        return true;
    }

    private static bool TryFormatNodeRelationship(
        SmoDocument document,
        ReadOnlySpan<byte> payload,
        out string value)
    {
        value = string.Empty;
        if (!SmoNodeDecoder.TryDecodeRelationship(
                payload, out uint objectId, out uint inlineSize,
                out SmoNodeRelationshipEncoding encoding))
        {
            return false;
        }

        SmoObjectEntry? target = document.Objects
            .GroupBy(entry => entry.Id)
            .Where(group => group.Count() == 1 && group.Key == objectId)
            .Select(group => group.Single())
            .SingleOrDefault();
        if (target is not null && inlineSize > 0 &&
            target.SerializedSize != inlineSize)
        {
            return false;
        }
        string encodingName = encoding switch
        {
            SmoNodeRelationshipEncoding.IdOnly => "ID-only reference",
            SmoNodeRelationshipEncoding.SizedReference => "sized reference",
            SmoNodeRelationshipEncoding.InlineObject =>
                $"inline SBOO ({inlineSize} bytes)",
            _ => encoding.ToString()
        };
        string targetName = target is null
            ? "unresolved"
            : $"[{target.Index}] {SmoClassRegistry.GetDisplayName(target.TypeHash)} " +
              $"\"{target.Name.TrimEnd('\0')}\"";
        value = $"id=0x{objectId:X8}, {encodingName}, {targetName}";
        return true;
    }

    private static bool TryFormatIndexedPartitionChild(
        SmoDocument document,
        SmoObjectField field,
        ReadOnlySpan<byte> payload,
        out string value)
    {
        value = string.Empty;
        if (payload.Length <= sizeof(uint))
            return false;
        uint slot = ReadUInt32(payload,0);
        if (slot >= 8 || !TryFormatNodeRelationship(
                document,payload[sizeof(uint)..],out string relationship))
        {
            return false;
        }
        string slotKind = document.Objects[field.ObjectIndex].TypeHash ==
                          SmoClassIds.BspNode
            ? "BSP branch slot"
            : "octant slot";
        value = $"{slotKind}={slot}; {relationship}";
        return true;
    }

    private static string FormatTextureData(SmoTextureDataInfo data)
    {
        static string FormatRepresentation(SmoTextureRepresentationData? item)
        {
            if (item is null)
                return "none";
            int paletteEntries = item.Palette.Length / 4;
            string palette = paletteEntries == 0
                ? string.Empty
                : $", palette={paletteEntries}";
            return $"{SmoTextureDataDecoder.GetRepresentationName(item.Kind)} " +
                   $"{item.Width}x{item.Height}, format={item.FormatValue}, " +
                   $"aux={item.AuxiliaryValue}, bpp={item.BitsPerPixel}, " +
                   $"mips={item.MipLevels.Count}{palette}";
        }

        string platform = data.PlatformType.HasValue
            ? $"{data.PlatformType.Value} " +
              $"({GetTexturePlatformTypeName(data.PlatformType.Value)})"
            : "legacy/omitted";
        return $"source={data.SourceKind}; platform={platform}; " +
               $"selected=[{FormatRepresentation(data.SelectedRepresentation)}]; " +
               $"cross=[{FormatRepresentation(data.CrossPlatform)}]; " +
               $"native=[{FormatRepresentation(data.PlatformSpecific)}]" +
               (data.HasOpaqueRepresentations ? "; skipped representation fields remain opaque" : string.Empty);
    }

    private static string GetTexturePlatformTypeName(uint value) => value switch
    {
        1 => "legacy cross-platform",
        6 => "Direct3D",
        7 => "Direct3D alternate",
        8 => "PS2 native",
        9 => "PS2 native plus cross-platform",
        _ => "unknown engine value"
    };

    private static string FormatHexPreview(ReadOnlySpan<byte> payload)
    {
        int count = Math.Min(payload.Length, HexPreviewBytes);
        string preview = string.Join(
            " ", payload[..count].ToArray().Select(value => value.ToString("X2")));
        return payload.Length > count ? preview + " …" : preview;
    }

    private static string FormatValueHistogram(IEnumerable<uint> values)
    {
        var groups = values.GroupBy(value => value)
            .OrderByDescending(group => group.Count())
            .ThenBy(group => group.Key)
            .Take(8)
            .Select(group => $"{group.Key}x{group.Count()}")
            .ToArray();
        return $"[{string.Join(", ",groups)}]";
    }

    private static string FormatSurfaceTypeHistogram(
        IEnumerable<SmoMeshBoundingVolumeFaceData> faces)
    {
        string[] groups = faces.GroupBy(item => item.SurfaceType)
            .OrderByDescending(group => group.Count())
            .ThenBy(group => group.Key)
            .Take(10)
            .Select(group =>
                $"{group.Key} ({SmoMeshBoundingVolumeDecoder.GetSurfaceTypeName(group.Key)})" +
                $"x{group.Count()}")
            .ToArray();
        return $"[{string.Join(", ",groups)}]";
    }

    private static uint ReadUInt32(ReadOnlySpan<byte> payload, int offset) =>
        BinaryPrimitives.ReadUInt32LittleEndian(
            payload.Slice(offset, sizeof(uint)));

    private static string FormatSingle(ReadOnlySpan<byte> payload, int offset) =>
        BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(
                payload.Slice(offset, sizeof(float))))
            .ToString("G9", CultureInfo.InvariantCulture);

    private static string FormatVector3(ReadOnlySpan<byte> payload, int offset)
    {
        var value = new Vector3(
            BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(
                payload.Slice(offset, sizeof(float)))),
            BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(
                payload.Slice(offset + sizeof(float), sizeof(float)))),
            BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(
                payload.Slice(offset + 2 * sizeof(float), sizeof(float)))));
        return FormatVector3(value);
    }

    private static string FormatVector3(Vector3 value) =>
        $"({FormatSingle(value.X)}, {FormatSingle(value.Y)}, " +
        $"{FormatSingle(value.Z)})";

    private static string FormatVector4(Vector4 value) =>
        $"({FormatSingle(value.X)}, {FormatSingle(value.Y)}, " +
        $"{FormatSingle(value.Z)}, {FormatSingle(value.W)})";

    private static string FormatColorEvaluator(
        SmoColorFunctionalEvaluatorData value) =>
        $"{{color1=#{value.Color1:X8}, color2=#{value.Color2:X8}, " +
        $"type={value.FunctionType}, frequency={FormatSingle(value.Frequency)}, " +
        $"amplitude={FormatSingle(value.Amplitude)}, " +
        $"xOffset={FormatSingle(value.XOffset)}, " +
        $"yOffset={FormatSingle(value.YOffset)}, pitch={FormatSingle(value.Pitch)}}}";

    private static string FormatEvaluator(SmoFunctionalEvaluatorData value) =>
        $"{{type={value.FunctionType}, frequency={FormatSingle(value.Frequency)}, " +
        $"amplitude={FormatSingle(value.Amplitude)}, " +
        $"xOffset={FormatSingle(value.XOffset)}, " +
        $"yOffset={FormatSingle(value.YOffset)}, pitch={FormatSingle(value.Pitch)}}}";

    private static string FormatSingle(float value) =>
        value.ToString("G9", CultureInfo.InvariantCulture);

    private static string FormatArgb(uint value) => $"#{value:X8}";
}
