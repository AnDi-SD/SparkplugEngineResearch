namespace SmoViewer.Core;

/// <summary>
/// A read-only name for one serializer field confirmed from WinxClub.exe.
/// This registry does not make a field editable: payload layout and mutation
/// safety are separate questions from recovering the serializer enum name.
/// </summary>
public sealed record SmoSerializedFieldDescriptor(
    int FieldType,
    string Key,
    string DisplayName,
    string PayloadLayout);

/// <summary>
/// Names fields in confirmed serializer sections. Sparkplug reuses field
/// numbers in base-class sections, separated by empty field-zero terminators,
/// so a concrete class ID, section and field number are all significant.
/// </summary>
public static class SmoSerializedFieldRegistry
{
    private static readonly IReadOnlyDictionary<uint,
        IReadOnlyDictionary<int, SmoSerializedFieldDescriptor>> Classes =
        BuildClasses();
    private static readonly IReadOnlyDictionary<(uint ClassId,int SectionFromEnd),
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>> InheritedSections =
        BuildInheritedSections();

    public static bool HasOwnFieldDefinitions(uint classId) =>
        Classes.ContainsKey(classId);

    public static bool HasFieldDefinitions(uint classId) =>
        Classes.ContainsKey(classId) ||
        InheritedSections.Keys.Any(key => key.ClassId == classId);

    public static IEnumerable<uint> KnownClassIds => Classes.Keys;

    internal static bool HasNodeSection(uint classId) => classId == SmoClassIds.Node ||
        InheritedSections.Any(item => item.Key.ClassId == classId && ReferenceEquals(item.Value, Classes[SmoClassIds.Node]));

    internal static bool TryGetNodeSectionRange(uint classId, IReadOnlyList<SmoObjectField> fields,
        out int first, out int terminal)
    {
        first = terminal = 0;
        if (!HasNodeSection(classId)) return false;
        int fromEnd = classId == SmoClassIds.Node ? 0 : InheritedSections.Single(item =>
            item.Key.ClassId == classId && ReferenceEquals(item.Value, Classes[SmoClassIds.Node])).Key.SectionFromEnd;
        int[] terminators = fields.Select((field,index) => (field,index))
            .Where(item => IsSectionTerminator(item.field)).Select(item => item.index).ToArray();
        int ordinal = terminators.Length - fromEnd - 1;
        if (ordinal != 0) return false;
        terminal = terminators[ordinal]; first = ordinal == 0 ? 0 : terminators[ordinal - 1] + 1;
        return true;
    }

    public static IReadOnlyDictionary<int, SmoSerializedFieldDescriptor>
        GetOwnFieldDefinitions(uint classId) =>
        Classes.TryGetValue(classId, out var fields)
            ? fields
            : EmptyFields;

    private static readonly IReadOnlyDictionary<int, SmoSerializedFieldDescriptor>
        EmptyFields = new Dictionary<int, SmoSerializedFieldDescriptor>();

    public static bool TryDescribeOwnField(
        uint classId,
        IReadOnlyList<SmoObjectField> fields,
        int fieldIndex,
        out SmoSerializedFieldDescriptor? descriptor)
    {
        ArgumentNullException.ThrowIfNull(fields);
        descriptor = null;
        if ((uint)fieldIndex >= (uint)fields.Count ||
            !Classes.TryGetValue(classId, out var classFields))
        {
            return false;
        }

        if (!TryGetSectionFromEnd(fields,fieldIndex,out int sectionFromEnd) ||
            sectionFromEnd != 0)
            return false;

        return classFields.TryGetValue(fields[fieldIndex].FieldType, out descriptor);
    }

    /// <summary>
    /// Describes a field in either the concrete serializer section or a
    /// confirmed inherited section of that concrete class.
    /// </summary>
    public static bool TryDescribeField(
        uint classId,
        IReadOnlyList<SmoObjectField> fields,
        int fieldIndex,
        out SmoSerializedFieldDescriptor? descriptor)
    {
        ArgumentNullException.ThrowIfNull(fields);
        descriptor = null;
        if ((uint)fieldIndex >= (uint)fields.Count ||
            !TryGetSectionFromEnd(fields,fieldIndex,out int sectionFromEnd))
        {
            return false;
        }

        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>? definitions = null;
        if (sectionFromEnd == 0)
            Classes.TryGetValue(classId,out definitions);
        else
        {
            InheritedSections.TryGetValue(
                (classId,sectionFromEnd),out definitions);
        }
        return definitions is not null &&
               definitions.TryGetValue(fields[fieldIndex].FieldType,out descriptor);
    }

    private static bool TryGetSectionFromEnd(
        IReadOnlyList<SmoObjectField> fields,
        int fieldIndex,
        out int sectionFromEnd)
    {
        sectionFromEnd = 0;
        if ((uint)fieldIndex >= (uint)fields.Count ||
            IsSectionTerminator(fields[fieldIndex]))
        {
            return false;
        }

        int followingTerminators = 0;
        for (int index = fieldIndex + 1; index < fields.Count; index++)
        {
            if (IsSectionTerminator(fields[index]))
                followingTerminators++;
        }
        if (followingTerminators == 0)
            return true;
        sectionFromEnd = followingTerminators - 1;
        return true;
    }

    private static bool IsSectionTerminator(SmoObjectField field) =>
        field.FieldType == 0 && field.PayloadSize == 0;

    private static IReadOnlyDictionary<uint,
        IReadOnlyDictionary<int, SmoSerializedFieldDescriptor>> BuildClasses()
    {
        var result = new Dictionary<uint,
            IReadOnlyDictionary<int, SmoSerializedFieldDescriptor>>
        {
            [SmoClassIds.Node] = Fields(
                Field(0, "node.position", "Local position", "Vector3"),
                Field(1, "node.rotation", "Local rotation",
                    "Quaternion (X, Y, Z, W)"),
                Field(2, "node.scale", "Local scale", "Vector3"),
                Field(3, "node.is_bone", "Bone node", "Boolean"),
                Field(4, "node.is_static", "Static node", "Boolean"),
                Field(5, "node.child", "Child node",
                    "UInt32 object ID; optional UInt32 inline size and inline SBOO"),
                Field(6, "node.billboard_axis", "Billboard axis", "UInt32 enum"),
                Field(7, "node.collision", "Collision info",
                    "UInt32 object ID; optional UInt32 inline size and inline SBOO"),
                Field(8, "node.is_animated", "Animated node", "Boolean")),
            [SmoClassIds.RenderNode] = Fields(
                Field(0, "render_node.renderable", "Renderable",
                    "UInt32 object ID; optional UInt32 inline size and inline SBOO")),
            [SmoClassIds.MeshData] = Fields(
                Field(0,"mesh.cross_platform","Cross-platform mesh",
                    "primitive/index/vertex buffers in portable E0 representation"),
                Field(1,"mesh.platform_specific","Platform-specific mesh",
                    "Direct3D E1 buffers or PS2 sphere/count/format/DMA representation"),
                Field(2,"mesh.bounding_box","Mesh bounding box",
                    "Vector3 minimum plus Vector3 maximum")),
            [SmoClassIds.Model] = Fields(
                Field(0,"model.base_mesh","Base mesh",
                    "object relationship to spMeshData"),
                Field(1,"model.projection_group","Projection group","UInt32")),
            [SmoClassIds.Skin] = Fields(
                Field(0,"skin.palette","Bone palette",
                    "UInt32 blend-influence count hint, UInt32 slot count, repeated spNode " +
                    "relationship plus inverse-bind Matrix4x4")),
            [SmoClassIds.CollisionInfo] = Fields(
                Field(0,"collision_info.primitive","Collision primitive",
                    "object relationship to spBoundingVolume"),
                Field(1,"collision_info.group","Collision group","UInt32"),
                Field(2,"collision_info.transform","Collision world transform",
                    "Vector3 position, Quaternion X/Y/Z/W, Vector3 scale")),
            [SmoClassIds.MeshBoundingVolume] = Fields(
                Field(0,"mesh_bv.geometry","Collision triangle geometry",
                    "UInt32 version 2, triangle count/reserved, UInt16 indices, " +
                    "index tail, vertex count/reserved, Vector3 positions"),
                Field(1,"mesh_bv.face_data","Per-triangle surface data",
                    "wxFaceData class ID 0x313C4C17, face count, repeated sparse " +
                    "UInt8 surface type, UInt16 flags and UInt8 surface ID")),
            [SmoClassIds.PartitionRenderable] = Fields(
                Field(0,"partition_renderable.renderable","Renderable",
                    "inline object relationship to spModel"),
                Field(1,"partition_renderable.debug_color","Debug color",
                    "ARGB UInt32")),
            [SmoClassIds.PartitionNode] = Fields(
                Field(0,"partition_node.collision_info","Collision info",
                    "object relationship to spCollisionInfo"),
                Field(1,"partition_node.debug_color","Debug color",
                    "ARGB UInt32"),
                Field(2,"partition_node.child","Child partition node",
                    "UInt32 octant slot followed by an inline object relationship"),
                Field(3,"partition_node.zone","Zone",
                    "sized-reference relationship to spZone"),
                Field(4,"partition_node.zone_portal","Zone portal",
                    "inline object relationship to spZonePortal"),
                Field(5,"partition_node.partition_system","Partition system",
                    "sized-reference relationship to spPartitionSystem"),
                Field(6,"partition_node.partition_renderable",
                    "Partition renderable",
                    "null ID or inline relationship to spPartitionRenderable"),
                Field(7,"partition_node.static_render_object",
                    "Static render object",
                    "object relationship to spStaticRenderObject")),
            [SmoClassIds.OctreeNode] = Fields(
                Field(0,"octree_node.pivot","Partition pivot","Vector3"),
                Field(1,"octree_node.minimum","Bounds minimum","Vector3"),
                Field(2,"octree_node.maximum","Bounds maximum","Vector3")),
            [SmoClassIds.PartitionSystem] = Fields(
                Field(0,"partition_system.partition_root","Partition root",
                    "required relationship to spBSPNode or spOctreeNode")),
            [SmoClassIds.Zone] = Fields(
                Field(0,"zone.local_partition_root","Local partition root",
                    "owned inline relationship to spPartitionNode or spOctreeNode")),
            [SmoClassIds.ZonePortal] = Fields(
                Field(0,"zone_portal.destination_zone","Destination zone",
                    "sized-reference or owned inline relationship to spZone"),
                Field(1,"zone_portal.polygon","Portal polygon",
                    "UInt32 vertex count followed by Vector3 vertices"),
                Field(2,"zone_portal.open","Open portal","Boolean")),
            [SmoClassIds.ZonePortalNode] = Fields(
                Field(0,"zone_portal_node.zone_portal","Zone portal",
                    "sized-reference relationship to spZonePortal")),
            [SmoClassIds.StaticRenderObject] = Fields(
                Field(0,"static_render_object.renderable","Renderable",
                    "object relationship to spModel"),
                Field(1,"static_render_object.transform","World transform",
                    "row-vector affine Matrix4x4"),
                Field(2,"static_render_object.inverse_transform",
                    "Engine inverse transform",
                    "transposed 3x3 basis plus -T*A^T affine Matrix4x4")),
            [SmoClassIds.TextureData] = Fields(
                Field(0, "texture.cross_platform", "Cross-platform texture",
                    "field 5: width, height, auxiliary value, bytes/pixel, BGRA32"),
                Field(2, "texture.source_none", "No texture source",
                    "Boolean source-state payload"),
                Field(3, "texture.source_embedded", "Embedded texture source",
                    "base source section plus platform type and cross/native representations"),
                Field(4, "texture.source_reference", "Referenced texture source",
                    "serializer reference payload (not observed in the validated corpora)"),
                Field(6, "texture.platform_type", "Texture platform type",
                    "UInt32 enum")),
            [SmoClassIds.MaterialData] = Fields(
                Field(0, "material.render_states", "Material render states",
                    "11 UInt32 values (engine eRenderState indices)"),
                Field(1, "material.vertex_alpha", "Use vertex alpha", "Boolean"),
                Field(2, "material.color", "Material colors",
                    "ambient/diffuse/specular/emissive ARGB plus Single specular power"),
                Field(3, "material.pass", "Material pass",
                    "UInt32 FinalBlendOp; begins a repeated pass"),
                Field(4, "material.layer", "Material layer class", "UInt32 class ID"),
                Field(6, "material.color_controller", "Material color controller",
                    "object relationship to spMaterialColorController"),
                Field(8, "material.texture_states_legacy", "Legacy texture states",
                    "9 UInt32 values (legacy spTextureStateBlockOld form)"),
                Field(9, "material.static_uv_transform", "Static UV transform",
                    "Boolean plus row-major 3x3 Single matrix"),
                Field(10, "material.texture", "Layer texture",
                    "object relationship to spTextureData"),
                Field(11, "material.animation_controller",
                    "Layer animation controller",
                    "object relationship to spAnimTexController"),
                Field(12, "material.uv_controller", "Layer UV controller",
                    "object relationship to spUVController"),
                Field(17, "material.texture_states", "Texture states",
                    "9 UInt32 values")),
            [SmoClassIds.UvController] = Fields(
                Field(0, "uv_controller.transform_evaluators",
                    "UV transform evaluators",
                    "spTransFunctionEval: translation XYZ, scale XYZ, rotation, " +
                    "UV pivot and rotation axis")),
            [SmoClassIds.MaterialColorController] = Fields(
                Field(0, "material_color_controller.evaluators",
                    "Material color evaluators",
                    "ambient/diffuse/specular/emissive ColorFunctionalEvaluator + " +
                    "alpha FunctionalEvaluator")),
            [SmoClassIds.Fog] = Fields(
                Field(0, "fog.parameters", "Fog parameters",
                    "UInt32 fog type, ARGB color, Single start/end/density")),
            [SmoClassIds.OrientedBoxBoundingVolume] = Fields(
                Field(0, "obb.position", "Local OBB position", "Vector3"),
                Field(1, "obb.size", "Full OBB size", "Vector3 full dimensions"),
                Field(2, "obb.rotation", "Local OBB rotation",
                    "Quaternion (X, Y, Z, W)")),
            [SmoClassIds.BoxBoundingVolume] = Fields(
                Field(0, "box_bv.position", "Local box position", "Vector3"),
                Field(1, "box_bv.size", "Full box size",
                    "Vector3 full dimensions")),
            [SmoClassIds.SphereBoundingVolume] = Fields(
                Field(0, "sphere_bv.position", "Local sphere position", "Vector3"),
                Field(1, "sphere_bv.radius", "Sphere radius", "Single")),
            [SmoClassIds.LightData] = Fields(
                Field(0, "light.type", "Light type", "UInt32"),
                Field(1, "light.project_shadow", "Project shadow volume", "Boolean"),
                Field(2, "light.color", "Light color", "ARGB UInt32"),
                Field(3, "light.attenuation", "Attenuation enabled", "Boolean"),
                Field(4, "light.intensity", "Intensity", "Single"),
                Field(5, "light.range", "Range", "Single"),
                Field(6, "light.hotspot_angle", "Hotspot angle", "Single radians"),
                Field(7, "light.falloff_angle", "Falloff angle", "Single radians"),
                Field(8, "light.enabled", "Enabled", "Boolean")),
            [SmoClassIds.NavigationGraph] = Fields(
                Field(0, "navigation_graph.set", "Navigation set",
                    "sized relationship to spMeshNavigationSet"),
                Field(1, "navigation_graph.portal", "Navigation portal",
                    "sized relationship to spNavigationPortal"),
                Field(2, "navigation_graph.path_table_size", "Path table size", "UInt32"),
                Field(3, "navigation_graph.path", "Navigation path",
                    "UInt32 source/destination set, UInt8 next portal/count, " +
                    "then count pairs of UInt8 first portal/reserved")),
            [SmoClassIds.NavigationSet] = Fields(
                Field(0,"navigation_set.node_count","Navigation node count","UInt32"),
                Field(1,"navigation_set.transition_table","Node transition table",
                    "UInt32 rows/columns followed by UInt32 link selectors/terminal 3"),
                Field(2,"navigation_set.portal_transition_table",
                    "Portal transition table",
                    "UInt32 portal/node dimensions followed by UInt32 link selectors/terminal 3"),
                Field(3,"navigation_set.links","Navigation links",
                    "UInt32 node count, then UInt8 node ID, UInt32 link count and UInt8 link IDs"),
                Field(4,"navigation_set.portal","Navigation portal",
                    "object relationship to spNavigationPortal"),
                Field(5,"navigation_set.enabled","Navigation enabled","Boolean")),
            [SmoClassIds.MeshNavigationSet] = Fields(
                Field(0, "navigation_set.mesh", "Navigation mesh", "object relationship")),
            [SmoClassIds.NavigationPortal] = Fields(
                Field(0, "navigation_portal.graph", "Navigation graph",
                    "sized relationship to spNavigationGraph"),
                Field(1, "navigation_portal.sets", "Navigation sets",
                    "two concatenated sized-reference/inline spMeshNavigationSet relationships"),
                Field(2, "navigation_portal.nodes", "Navigation node pair",
                    "UInt8 node ID in endpoint set 0 and UInt8 node ID in endpoint set 1"),
                Field(3, "navigation_portal.path", "Path membership",
                    "UInt8 source set, destination set and alternative-path index")),
            [SmoClassIds.SkyBox] = Fields(
                Field(0,"sky_box.model","Sky model",
                    "required repeated inline relationship to spModel")),
            [SmoClassIds.AnimTextureController] = Fields(
                Field(0,"anim_texture.frames","Animation texture frames",
                    "UInt32 count, count Single times, then count sized/inline spTextureData relationships")),
            [SmoClassIds.LensFlare] = Fields(
                Field(0,"lens_flare.element","Flare element",
                    "material relationship, ARGB UInt32 color, Single relative distance/scale"),
                Field(1,"lens_flare.glare","Additional elements",
                    "UInt32 count followed by material/color/distance/scale records"),
                Field(2,"lens_flare.occlusion","Occlusion",
                    "Single sphere radius and Single speed"),
                Field(3,"lens_flare.render_node","Render node",
                    "relationship to spRenderNode")),
            [SmoClassIds.Font] = Fields(
                Field(0,"font.data","Font data",
                    "spTextureData relationship, UInt32 height/baseline, then 224 packed UInt8 width + Vector2 UV0/UV1 glyphs for 0x20..0xFF")),
            [SmoClassIds.TextRenderable] = Fields(
                Field(0,"text_renderable.text","Text","UInt16 byte length and raw byte string"),
                Field(1,"text_renderable.color","Text color","ARGB UInt32"),
                Field(2,"text_renderable.wrap_width","Wrap width","UInt32 pixels"),
                Field(3,"text_renderable.alignment","Alignment","UInt32 enum"),
                Field(4,"text_renderable.font","Font","NULL, reference or inline relationship to spFont")),
            [SmoClassIds.TextNode] = Fields(
                Field(0,"render_node.renderable","Text renderable",
                    "RenderNode relationship; the menu profile uses inline spTextRenderable")),
            [SmoClassIds.BspNode] = Fields(
                Field(0, "bsp.plane", "BSP plane", "Vector3 normal, Single constant"),
                Field(1, "bsp.polygon", "BSP polygon", "UInt32 count, Vector3 vertices")),
            [SmoClassIds.OcclusionVolume] = Fields(
                Field(0,"occlusion_volume.index_buffer","Occlusion index buffer",
                    "UInt32 primitive type/count/index format followed by UInt16 triangle indices"),
                Field(1,"occlusion_volume.vertex_buffer","Occlusion vertex buffer",
                    "UInt32 declaration/count/flags followed by position-only Vector3 vertices")),
            [SmoClassIds.ParticleSystem] = Fields(
                Field(0, "particle.acceleration", "Acceleration range", "Vector3 begin and Vector3 end"),
                Field(1, "particle.direction", "Emission direction", "Vector3"),
                Field(2, "particle.velocity", "Velocity range", "two Single values"),
                Field(3, "particle.angle", "Angle range", "two Single values"),
                Field(4, "particle.scale", "Scale range", "two Single values"),
                Field(5, "particle.color", "Color range", "two ARGB UInt32 values"),
                Field(6, "particle.time", "Emission/lifetime range", "two Single values"),
                Field(7, "particle.loop", "Loop animation", "Boolean"),
                Field(8, "particle.world_space", "World-space simulation", "Boolean"),
                Field(9, "particle.iterative", "Iterative mode", "Boolean"),
                Field(10, "particle.rate", "Emission rate", "Single"),
                Field(11, "particle.bounding_sphere", "Bounding-sphere radius", "Single"),
                Field(12, "particle.region.point", "Point emission region", "Vector3"),
                Field(13, "particle.region.plane", "Plane emission region", "Vector3 position/normal and Single size X/Y"),
                Field(14, "particle.region.box", "Box emission region", "Vector3 position and Vector3 size"),
                Field(15, "particle.region.sphere", "Sphere emission region", "Vector3 center, Single radius"),
                Field(16, "particle.region.disk", "Disk emission region", "Vector3 position and Single radius"),
                Field(17, "particle.region.cylinder", "Cylinder emission region", "Vector3 position and Single height/radius"),
                Field(18, "particle.region.cone", "Cone emission region", "Vector3 position and Single height/radius1/radius2"),
                Field(19, "particle.render_node", "Render node", "object relationship"))
        };
        return result;
    }

    private static IReadOnlyDictionary<(uint ClassId,int SectionFromEnd),
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>>
        BuildInheritedSections() =>
        new Dictionary<(uint ClassId,int SectionFromEnd),
            IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>>
        {
            [(SmoClassIds.RenderNode,1)] = Classes[SmoClassIds.Node],
            [(SmoClassIds.OctreeNode,1)] = Classes[SmoClassIds.PartitionNode],
            [(SmoClassIds.BspNode,1)] = Classes[SmoClassIds.PartitionNode],
            [(SmoClassIds.OcclusionVolume,1)] = Classes[SmoClassIds.Node],
            [(SmoClassIds.MeshNavigationSet,2)] = Classes[SmoClassIds.Node],
            [(SmoClassIds.MeshNavigationSet,1)] = Classes[SmoClassIds.NavigationSet],
            [(SmoClassIds.NavigationPortal,1)] = Classes[SmoClassIds.Node],
            [(SmoClassIds.NavigationGraph,2)] = Classes[SmoClassIds.Node],
            [(SmoClassIds.NavigationGraph,1)] = Classes[SmoClassIds.RenderNode],
            [(SmoClassIds.SkyBox,1)] = Classes[SmoClassIds.Node],
            [(SmoClassIds.ParticleSystem,1)] = Fields(
                Field(0,"renderable.material","Material",
                    "object relationship to spMaterialData"),
                Field(1,"renderable.fog","Fog",
                    "object relationship to spFog"),
                Field(2,"renderable.alpha_sort","Alpha sort enabled","UInt32 Boolean"),
                Field(3,"renderable.priority","Render priority","UInt32")),
            [(SmoClassIds.LensFlare,1)] = Fields(
                Field(0,"renderable.material","Material",
                    "object relationship to spMaterialData"),
                Field(1,"renderable.fog","Fog","object relationship to spFog"),
                Field(2,"renderable.alpha_sort","Alpha sort enabled","UInt32 Boolean"),
                Field(3,"renderable.priority","Render priority","UInt32")),
            [(SmoClassIds.TextRenderable,1)] = Fields(
                Field(0,"renderable.material","Material","object relationship to spMaterialData"),
                Field(1,"renderable.fog","Fog","object relationship to spFog"),
                Field(2,"renderable.alpha_sort","Alpha sort enabled","UInt32 Boolean"),
                Field(3,"renderable.priority","Render priority","UInt32")),
            [(SmoClassIds.TextNode,1)] = Classes[SmoClassIds.Node],
            [(SmoClassIds.PartitionSystem,2)] = Classes[SmoClassIds.Node],
            [(SmoClassIds.PartitionSystem,1)] = Classes[SmoClassIds.RenderNode],
            [(SmoClassIds.Zone,1)] = Classes[SmoClassIds.Node],
            [(SmoClassIds.ZonePortalNode,1)] = Classes[SmoClassIds.Node],
            [(SmoClassIds.Model,1)] = Fields(
                Field(0,"renderable.material","Material",
                    "object relationship to spMaterialData"),
                Field(1,"renderable.fog","Fog",
                    "object relationship to spFog"),
                Field(2,"renderable.alpha_sort","Alpha sort enabled","UInt32 Boolean"),
                Field(3,"renderable.priority","Render priority","UInt32")),
            [(SmoClassIds.Skin,2)] = Fields(
                Field(0,"renderable.material","Material",
                    "object relationship to spMaterialData"),
                Field(1,"renderable.fog","Fog",
                    "object relationship to spFog"),
                Field(2,"renderable.alpha_sort","Alpha sort enabled","UInt32 Boolean"),
                Field(3,"renderable.priority","Render priority","UInt32")),
            [(SmoClassIds.Skin,1)] = Fields(
                Field(0,"model.base_mesh","Base mesh",
                    "object relationship to spMeshData"),
                Field(1,"model.projection_group","Projection group","UInt32"))
        };

    private static IReadOnlyDictionary<int, SmoSerializedFieldDescriptor> Fields(
        params SmoSerializedFieldDescriptor[] fields) =>
        fields.ToDictionary(field => field.FieldType);

    private static SmoSerializedFieldDescriptor Field(
        int fieldType,
        string key,
        string displayName,
        string payloadLayout) =>
        new(fieldType, key, displayName, payloadLayout);
}
