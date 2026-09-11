#pragma once
#include <cstdint>
#include "LightInspection.h"
#include "BufferInspection.h"
#ifdef _WIN32
#define SPV_API extern "C" __declspec(dllexport)
#else
#define SPV_API extern "C" __attribute__((visibility("default")))
#endif

// Application interop ABI, NOT the original Sparkplug C++ ABI. All arrays are
// borrowed for one call; runtime/clip handles own reconstructed engine objects.
struct SpvNode { std::int32_t parent; float position[3], rotation[4], scale[3]; std::uint32_t billboard; };
struct SpvBone { std::int32_t node; float inverseBind[16]; };
struct SpvTrackInfo { std::uint32_t positionKeys, rotationKeys, scaleKeys; };
struct SpvSample { float position[3], rotation[4], scale[3]; std::uint32_t validRoles; };
struct SpvChannelInfo { std::uint32_t sourceKeys, axes, uniqueTimes, representations[3]; };
struct SpvAxisInfo { std::uint32_t representation, keys, stride, values; };
struct SpvLinearChannel { const float* times; const float* values; std::uint32_t count; };
struct SpvFieldHeader { std::uint32_t field, payloadSize, headerSize; };
struct SpvGraphObject {std::uint32_t id,wireClassID,runtimeClassID,offset,size,isNode;};
struct SpvGraphNode {std::uint32_t parentID,flags,children,collisions;float position[3],orientation[9],scale[3],rotation[4];};
// Raw container inspection through the original header/FAT stream readers.
// No runtime resource factories are invoked or substituted for unknown types.
struct SpvContainerInfo {std::uint32_t signature,version,exportTag,fileSize,platformMask,dataOffset,dataSize,objectCount,headerStatus;};
struct SpvContainerEntry {std::uint32_t tableOffset,id,nameOffset,nameBytes,classID,offset,size,signatureClassID,signatureFlags;};
// HOST lossless envelope encoder; not an original whole-file writer.
struct SpvEnvelopeEntry {std::uint32_t id,classID,offset,size,nameOffset,nameSize;};
struct SpvEnvelopeLayout {std::uint32_t dataOffset,fileSize;};
struct SpvEnvelopeHeader {std::uint32_t signature,version,exportTag,platformMask;};
// IDs are canonical graph object IDs; ordinary cache slots retain original order.
struct SpvSceneLightCache {std::uint32_t renderNode,count,ambient,lights[8];};
// HOST transport for the consumed Fixed.rfx vertex-shader constants. A zero
// in an unmarked field is ABI padding, never an inferred original register.
struct SpvShaderLight {
    std::uint32_t type,known;
    float diffuse[4],specular[4],position[4],direction[4],attenuation[4],inner,outer;
};
struct SpvShaderLighting {
    std::uint32_t colorMode,count,specular,known;
    float ambient[4],diffuse[4],constantColor[4],power;
    SpvShaderLight lights[8];
};
SPV_API int spv_scene_shader_lighting(void*,std::uint32_t,std::uint32_t,const float*,std::uint32_t,SpvShaderLighting*) noexcept;
SPV_API int spv_scene_light_ids(void*,std::uint32_t*,std::uint32_t,std::uint32_t*) noexcept;
SPV_API int spv_scene_lighting_configure(void*,const std::uint32_t*,std::uint32_t,std::uint32_t) noexcept;
SPV_API int spv_scene_lighting_active(void*,std::uint32_t) noexcept;
SPV_API int spv_scene_lighting_clear(void*) noexcept;
SPV_API int spv_scene_lighting_caches(void*,SpvSceneLightCache*,std::uint32_t,std::uint32_t*) noexcept;
SPV_API int spv_envelope_measure(const SpvEnvelopeEntry*,std::uint32_t,
    const std::uint8_t*,std::uint32_t,std::uint32_t,SpvEnvelopeLayout*) noexcept;
SPV_API int spv_envelope_write(const SpvEnvelopeHeader*,const SpvEnvelopeEntry*,
    std::uint32_t,const std::uint8_t*,std::uint32_t,std::uint32_t,
    std::uint8_t*,std::uint32_t) noexcept;
struct SpvMeshBVInfo {std::uint32_t primitiveType,vertices,indices,faces,hasFaces,fieldMask,vertexPayloadOffset,faceClassID;};
// Standalone CPU buffer metadata. Input is immutable and borrowed for one
// synchronous read; each handle owns its actual shared buffer until destroy.
// These reads impose no occluder geometry policy or runtime initialization.
SPV_API void* spv_index_buffer_read(const std::uint8_t*,std::uint32_t) noexcept;
SPV_API void spv_index_buffer_destroy(void*) noexcept;
SPV_API int spv_index_buffer_info(void*,spvhost::IndexBufferInfo*) noexcept;
SPV_API int spv_index_buffer_indices(void*,std::uint32_t*,std::uint32_t count) noexcept;
SPV_API void* spv_vertex_buffer_read(const std::uint8_t*,std::uint32_t) noexcept;
SPV_API void spv_vertex_buffer_destroy(void*) noexcept;
SPV_API int spv_vertex_buffer_info(void*,spvhost::VertexBufferInfo*) noexcept;
SPV_API int spv_vertex_buffer_positions(void*,float*,std::uint32_t floats) noexcept;
struct SpvFaceData {std::uint32_t surfaceType,flags,surfaceID,fieldMask;};
struct SpvVertexLayout {std::uint32_t stride;std::int32_t normal,color,uv0,uv1,weights,bones;};
struct SpvMeshInfo {
    std::uint32_t fieldID,fieldPayloadOffset,indexPayloadOffset,vertexPayloadOffset;
    std::uint32_t primitiveType,primitives,vertices,indices,indexElementSize;
    std::uint32_t serializedStride,runtimeStride,runtimeVBSize,componentFlags,attributes;
    std::uint32_t planningWords[4],planningByte;
};
struct SpvMeshVertex {float position[3],normal[3],uv0[2],uv1[2],weights[4];std::uint32_t color,bones;};
struct SpvPs2MeshHeader {float sphere[4];std::uint32_t primitives,vertices,componentFlags,packetQwords,additionalUVCount,weightCount;};
struct SpvTextureSectionInfo {std::uint32_t kind,width,height,format,auxiliary,bitsPerPixel,pixelDataPresent,mips;};
struct SpvTextureMip {std::uint32_t width,height,descriptor0,descriptor1,descriptor2,pixelOffset,pixelSize;};
struct SpvTextureSourceInfo {std::uint32_t fields,representations,mips,finalPosition,runtimeWidth,runtimeHeight,runtimeMips;};
struct SpvTextureSourceField {
    std::uint32_t scope,outcome,frameOffset,frameSize,depth,fieldID,headerOffset,payloadOffset,payloadSize;
    std::uint32_t platformBefore,platformAfter,inputStream,complete,handledBefore,handledAfter;
};
struct SpvTextureSourceRepresentation {
    std::uint32_t kind,fieldIndex,width,height,format,auxiliary,bitsPerPixel,nativeFlag,field1C,firstMip,mips;
};
// Complete PC source dispatch on a real spDXTexture CPU owner. These arrays
// observe the original reader; skipped fields remain opaque. Stored mip slices
// precede generated runtime mips. No retained pointer into the borrowed input.
SPV_API void* spv_texture_source_read(const std::uint8_t*,std::uint32_t,std::uint32_t platformMask) noexcept;
SPV_API void spv_texture_source_destroy(void*) noexcept;
SPV_API int spv_texture_source_info(void*,SpvTextureSourceInfo*) noexcept;
SPV_API int spv_texture_source_fields(void*,SpvTextureSourceField*,std::uint32_t) noexcept;
SPV_API int spv_texture_source_representations(void*,SpvTextureSourceRepresentation*,std::uint32_t) noexcept;
SPV_API int spv_texture_source_mips(void*,SpvTextureMip*,std::uint32_t) noexcept;
struct SpvPs2TextureInfo {std::uint32_t fields,images,mips,finalPosition;};
struct SpvPs2TextureField {std::uint32_t fieldID,headerOffset,payloadOffset,payloadSize,imageIndex,terminator;};
struct SpvPs2TextureImage {std::uint32_t fieldIndex,nativeFlag,format,width,height,auxiliary,paletteOffset,paletteSize,firstMip,mips;};
struct SpvPs2TextureMip {std::uint32_t descriptor0,descriptor1,descriptor2,dataSize,descriptorOffset,dataOffset;};
// Serialized PS2 native-section metadata only. No source dispatch, target
// initialization, inferred mip dimensions, pixel decoding or GPU operation.
SPV_API void* spv_ps2_texture_inspect(const std::uint8_t*,std::uint32_t) noexcept;
SPV_API void spv_ps2_texture_destroy(void*) noexcept;
SPV_API int spv_ps2_texture_info(void*,SpvPs2TextureInfo*) noexcept;
SPV_API int spv_ps2_texture_fields(void*,SpvPs2TextureField*,std::uint32_t) noexcept;
SPV_API int spv_ps2_texture_images(void*,SpvPs2TextureImage*,std::uint32_t) noexcept;
SPV_API int spv_ps2_texture_mips(void*,SpvPs2TextureMip*,std::uint32_t) noexcept;
// Explicit serialized representation inspection: kind0 cross field, kind1 PC
// native field. Stored levels/offsets only; runtime attachment generates mips
// separately through the same engine classes. Input is borrowed for one call.
SPV_API void* spv_texture_section_read(const std::uint8_t*,std::uint32_t,std::uint32_t kind) noexcept;
SPV_API void spv_texture_section_destroy(void*) noexcept;
SPV_API int spv_texture_section_info(void*,SpvTextureSectionInfo*) noexcept;
SPV_API int spv_texture_section_mips(void*,SpvTextureMip*,std::uint32_t) noexcept;
// XRGB inspector preview through the restored raw decoder/encoder. Stored
// bytes above remain untouched; this is an explicit BGRA output projection.
SPV_API int spv_texture_section_bgra(void*,std::uint8_t*,std::uint32_t) noexcept;
// Explicit stored XRGB pixel projection through the same restored codec used
// for section previews and graph uploads. Input/output lengths must match.
SPV_API int spv_texture_xrgb_bgra(const std::uint8_t*,std::uint32_t,std::uint8_t*,std::uint32_t) noexcept;
SPV_API int spv_texture_section_field1c(void*,std::uint32_t*) noexcept;
// One supplied BGRA base mip. kind0: whole embedded CPU TextureData object;
// kind1: raw first-mip record for a lossless editor field replacement.
SPV_API void* spv_texture_write_bgra(const std::uint8_t*,std::uint32_t bytes,std::uint32_t width,
    std::uint32_t height,std::uint32_t field1C,std::uint32_t kind) noexcept;
SPV_API void spv_serialized_bytes_destroy(void*) noexcept;
SPV_API int spv_serialized_bytes_size(void*,std::uint32_t*) noexcept;
SPV_API int spv_serialized_bytes_copy(void*,std::uint8_t*,std::uint32_t) noexcept;
// Bounded inspection only. Header reads the original prefix and checks the
// opaque packet extent; it neither materializes nor executes a DMA packet.
SPV_API int spv_ps2_mesh_header(const std::uint8_t*,std::uint32_t,SpvPs2MeshHeader*) noexcept;
SPV_API int spv_mesh_bounds(const std::uint8_t*,std::uint32_t,float* minimumMaximum) noexcept;
// kind0: complete MeshData field stream; kind1: portable field; kind2: PC field.
// kind3/4: same portable/PC reader, metadata only (no host attribute validation).
// platformMask controls the original whole-field selector, without fallback.
SPV_API void* spv_mesh_read(const std::uint8_t*,std::uint32_t,std::uint32_t kind,std::uint32_t platformMask) noexcept;
// Typed host input -> original CPU buffers/mesh serializers. kind0 portable,
// kind1 PC native-only. UInt16 triangle lists; returned SerializedBytes owner.
SPV_API void* spv_mesh_write_triangles(const SpvMeshVertex*,std::uint32_t vertices,
    const std::uint32_t* indices,std::uint32_t indexCount,std::uint32_t componentFlags,std::uint32_t kind) noexcept;
// Same typed input, positions only, original MeshBV geometry-only writer.
SPV_API void* spv_mesh_bv_write_triangles(const SpvMeshVertex*,std::uint32_t vertices,
    const std::uint32_t* indices,std::uint32_t indexCount) noexcept;
SPV_API void spv_mesh_destroy(void*) noexcept;
SPV_API int spv_mesh_info(void*,SpvMeshInfo*) noexcept;
SPV_API int spv_mesh_vertices(void*,SpvMeshVertex*,std::uint32_t) noexcept;
SPV_API int spv_mesh_indices(void*,std::uint32_t*,std::uint32_t) noexcept;
SPV_API int spv_vertex_layout(std::uint32_t componentFlags,SpvVertexLayout*) noexcept;
// kind 0: MeshBV field stream (no SBOO header), 1: geometry field, 2: face field.
// Leaf readers invoke the same actual serializers as whole resource loading.
SPV_API void* spv_mesh_bv_read(const std::uint8_t*,std::uint32_t,std::uint32_t kind) noexcept;
SPV_API void spv_mesh_bv_destroy(void*) noexcept;
SPV_API int spv_mesh_bv_info(void*,SpvMeshBVInfo*) noexcept;
SPV_API int spv_mesh_bv_geometry(void*,float* positions,std::uint32_t floats,std::int32_t* indices,std::uint32_t count) noexcept;
SPV_API int spv_mesh_bv_faces(void*,SpvFaceData*,std::uint32_t count) noexcept;
SPV_API void* spv_container_inspect(const std::uint8_t*,std::uint32_t) noexcept;
SPV_API void spv_container_destroy(void*) noexcept;
SPV_API int spv_container_info(void*,SpvContainerInfo*) noexcept;
SPV_API int spv_container_entries(void*,SpvContainerEntry*,std::uint32_t count) noexcept;
SPV_API void* spv_graph_load(const std::uint8_t*,std::uint32_t) noexcept;
// Optional host observations from the same original readers. This is not an
// alternative loader or a second field/reference parser.
SPV_API void* spv_graph_load_with_trace(const std::uint8_t*,std::uint32_t) noexcept;
struct SpvReferenceRead {
    std::uint32_t consumerId,id,inlineSize,idPhysicalOffset,sizePhysicalOffset,resolution,success;
};
struct SpvPayloadRead {
    std::uint32_t objectId,wireClassId,physicalOffset,size,kind,complete;
};
SPV_API int spv_graph_reference_trace_info(void*,std::uint32_t* references,std::uint32_t* payloads,std::uint32_t* dataPhysicalOrigin) noexcept;
SPV_API int spv_graph_reference_reads(void*,SpvReferenceRead*,std::uint32_t count) noexcept;
SPV_API int spv_graph_payload_reads(void*,SpvPayloadRead*,std::uint32_t count) noexcept;
SPV_API void spv_graph_destroy(void*) noexcept;
SPV_API int spv_graph_info(void*,std::uint32_t* objects,std::uint32_t* nodes,std::uint32_t* rootID) noexcept;
SPV_API int spv_graph_object(void*,std::uint32_t ordinal,char* name,std::uint32_t capacity,SpvGraphObject*) noexcept;
SPV_API int spv_graph_node(void*,std::uint32_t id,SpvGraphNode*) noexcept;
struct SpvOctreeFields {std::uint32_t pivotKnown;float pivot[3],mins[3],maxs[3];};
struct SpvGraphOctree {std::uint32_t parent,children[8];SpvOctreeFields fields;};
SPV_API int spv_octree_fields_read(const std::uint8_t*,std::uint32_t,SpvOctreeFields*) noexcept;
SPV_API int spv_graph_octree(void*,std::uint32_t id,SpvGraphOctree*) noexcept;
SPV_API int spv_graph_navigation_json(void*,std::uint8_t*,std::uint32_t capacity,std::uint32_t* size) noexcept;
SPV_API int spv_graph_spatial_json(void*,std::uint8_t*,std::uint32_t capacity,std::uint32_t* size) noexcept;
using SpvLightFields=spvhost::LightInspection;
SPV_API int spv_light_fields_read(const std::uint8_t*,std::uint32_t,SpvLightFields*) noexcept;
// Font inspection DTOs. An absent baseline is explicitly unknown; image is an
// observed wire extent, not a loaded texture. Glyph layout is host-only.
struct SpvFontInfo {std::uint32_t height,baseline,hasBaseline,hasImage,imageOffset,imageSize;};
struct SpvFontGlyph {std::uint32_t width;float uv0[2],uv1[2];};
SPV_API int spv_font_read(const std::uint8_t*,std::uint32_t,SpvFontInfo*,SpvFontGlyph*,std::uint32_t) noexcept;
// Scalar observation from an actual BV. Values contain the requested field;
// halfExtents/boundingRadius describe size fields only, not extra game members.
struct SpvBVField {float values[4],halfExtents[3],boundingRadius;};
SPV_API int spv_bv_scalar_read(std::uint32_t classID,std::uint32_t field,
    const std::uint8_t*,std::uint32_t,SpvBVField*) noexcept;
struct SpvLensFlareInfo {std::uint32_t elements,renderNode;float radius,speed;};
struct SpvGraphRenderable {std::uint32_t material,fog,alpha,priority;};
SPV_API int spv_graph_renderable(void*,std::uint32_t id,SpvGraphRenderable*) noexcept;
struct SpvLensFlareElement {std::uint32_t material,color;float distance,scale;};
SPV_API int spv_graph_lens_flare(void*,std::uint32_t id,SpvLensFlareInfo*) noexcept;
SPV_API int spv_graph_lens_flare_element(void*,std::uint32_t id,std::uint32_t ordinal,SpvLensFlareElement*) noexcept;
struct SpvParticleInfo {
    float acceleration[6],direction[3],velocity[2],angle[2],scale[2];
    std::uint32_t colors[2];float times[2],sphere[4],rate;
    std::uint32_t flags[3],regionType,regionValues,renderNode;
    float region[8];std::uint32_t pool[5];
};
SPV_API int spv_graph_particle(void*,std::uint32_t id,SpvParticleInfo*) noexcept;
// Initial CPU pool only, not a sampled frame or GPU particle payload.
struct SpvParticlePoolInfo {std::uint32_t count,first,boundary,initialized;};
struct SpvParticleRecord {float values[8];std::uint32_t written,previous,next;};
SPV_API int spv_graph_particle_pool(void*,std::uint32_t id,SpvParticlePoolInfo*,SpvParticleRecord*,std::uint32_t capacity) noexcept;
struct SpvFogFields {std::uint32_t type,color;float start,end,density;};
SPV_API int spv_fog_payload_read(const std::uint8_t*,std::uint32_t,SpvFogFields*) noexcept;
// Read-only projections of actual loaded resources. IDs preserve canonical
// loader/cache identity; zero means NULL, never an inferred default resource.
struct SpvGraphModel {std::uint32_t mesh,material,fog,alpha,priority,projection;};
struct SpvGraphMaterial {
    std::uint32_t states[11],vertexAlpha,powerInitialized;
    float colors[16],power;std::uint32_t colorController,passes;
};
struct SpvGraphPass {std::uint32_t blend,layers;};
struct SpvGraphLayer {
    std::uint32_t classID,texture,animation,uvController,uvEnabled,animationBoundHere,uvBoundHere;
    std::uint32_t states[12];float uv[9];
};
struct SpvGraphTexture {std::uint32_t width,height,surfaceFormat,mips;};
struct SpvGraphTextureKey {float time;std::uint32_t texture;};
SPV_API int spv_graph_model(void*,std::uint32_t,SpvGraphModel*) noexcept;
SPV_API int spv_graph_material(void*,std::uint32_t,SpvGraphMaterial*) noexcept;
SPV_API int spv_graph_pass(void*,std::uint32_t material,std::uint32_t pass,SpvGraphPass*) noexcept;
SPV_API int spv_graph_layer(void*,std::uint32_t material,std::uint32_t pass,std::uint32_t layer,SpvGraphLayer*) noexcept;
SPV_API int spv_graph_texture(void*,std::uint32_t,SpvGraphTexture*) noexcept;
// BGRA host upload projection of an already selected/initialized CPU texture.
// Does not inspect a different serialized representation as a fallback.
SPV_API int spv_graph_texture_bgra(void*,std::uint32_t,std::uint8_t*,std::uint32_t) noexcept;
SPV_API int spv_graph_texture_track(void*,std::uint32_t,std::uint32_t* keys,float* duration) noexcept;
SPV_API int spv_graph_texture_keys(void*,std::uint32_t,SpvGraphTextureKey*,std::uint32_t) noexcept;
// Explicit tool inputs to the actual controller and pass methods. These calls
// do not invent an AnimationManager schedule, enabled gate or visibility order.
struct SpvGraphControllerClock {std::uint32_t classID;float accumulated,applied,playback;std::uint32_t hasPlayback,enabled;};
struct SpvGraphUVSubmission {std::uint32_t stage;float matrix[9];};
SPV_API int spv_graph_controller_clock(void*,std::uint32_t,SpvGraphControllerClock*) noexcept;
SPV_API int spv_graph_apply_controllers(void*,const std::uint32_t* ids,std::uint32_t count,float elapsed) noexcept;
SPV_API int spv_graph_update_material_color(void*,std::uint32_t material,std::uint32_t frame,std::uint32_t force,std::uint32_t* evaluated) noexcept;
// Immutable per-pass transport from common material submission. Device indices
// render={7,8,9,14,15,19,20,22,23,24,25,27,29,137,145,148};
// stage={colorOp,alphaOp,addressU,addressV,border,mag,min,mip,coordinates,transform}.
// Known masks distinguish untouched startup state from valid zero values.
struct SpvMaterialDrawPass {
    std::uint32_t pass,vertexAlpha,knownRender,knownUV;
    std::uint32_t render[16];float colors[17];std::uint32_t textures[8];
    std::uint32_t stages[80],knownStages[8];float uv[128];
};
SPV_API void* spv_material_submission_create(void* graph) noexcept;
SPV_API void spv_material_submission_destroy(void*) noexcept;
SPV_API int spv_material_submission_capture(void*,std::uint32_t material,std::uint32_t frame,
    SpvMaterialDrawPass*,std::uint32_t capacity,std::uint32_t* count) noexcept;
// Output capacity must cover the pass's layer count. Output consists only of
// actual UV submissions in original call order. Failure can retain mutations
// performed by preceding layers; original updates are not transactional.
SPV_API int spv_graph_update_material_pass(void*,std::uint32_t material,std::uint32_t pass,SpvGraphUVSubmission*,std::uint32_t capacity,std::uint32_t* count) noexcept;
// Select actual loaded Node objects, including every parent of the selection.
// The scene retains their whole resource graph and preserves authored matrices.
SPV_API void* spv_graph_scene(void*,const std::uint32_t* ids,std::uint32_t count) noexcept;
// Select all canonical loaded Nodes (including actual derived classes). Scene
// owns the graph after the external graph handle is destroyed.
SPV_API void* spv_graph_scene_all(void*) noexcept;
SPV_API int spv_scene_node_count(void*,std::uint32_t*) noexcept;
SPV_API int spv_scene_graph_node_ids(void*,std::uint32_t*,std::uint32_t) noexcept;
SPV_API int spv_scene_graph_skin_info(void*,std::uint32_t skin,std::uint32_t* weights,std::uint32_t* bones) noexcept;
SPV_API int spv_scene_graph_skin_palette(void*,std::uint32_t skin,float*,std::uint32_t floats) noexcept;
// Actual support membership, not visibility traversal or a frame draw order.
// kind0 RenderNode,1 StaticRenderObject,2 PartitionRenderable. Query with null
// output/capacity0 first; matrices reflect the current world/cache state.
// kind: 0 RenderNode, 1 StaticRenderObject, 2 PartitionRenderable, 3 SkyBox.
// Membership is an inspection view, not a normal/special pass schedule.
struct SpvGraphRenderContainer {std::uint32_t id,kind,renderables;float world[16],inverse[16];};
SPV_API int spv_graph_render_containers(void*,SpvGraphRenderContainer*,std::uint32_t capacity,std::uint32_t* count) noexcept;
SPV_API int spv_graph_render_members(void*,std::uint32_t container,std::uint32_t* ids,std::uint32_t count) noexcept;
// One actual Model/Skin occurrence. This projects the confirmed input matrix
// (support for Model, original Skin render-world getter for Skin), not a draw.
struct SpvGraphRenderOccurrence {std::uint32_t renderable,rigidNode;float world[16];};
SPV_API int spv_graph_render_occurrence(void*,std::uint32_t container,std::uint32_t slot,SpvGraphRenderOccurrence*) noexcept;
SPV_API std::uint32_t spv_abi_version() noexcept;
SPV_API const char* spv_last_error() noexcept;
SPV_API int spv_read_field(const std::uint8_t*, std::uint32_t, SpvFieldHeader*) noexcept;
struct SpvReferencePrefix { std::uint32_t id,inlineSize,encoding,classID; };
struct SpvNodeField { std::uint32_t field,offset,size; };
struct SpvStaticMatrices { float world[16],inverse[16]; std::uint32_t fieldMask; };
struct SpvMaterialReference { std::uint32_t offset,size; };
struct SpvMaterialInfo {
    std::uint32_t states[11],vertexAlpha,hasColor,colors[4];float power;
    SpvMaterialReference colorController;std::uint32_t passes,layers;
};
struct SpvMaterialLayer {
    std::uint32_t pass,index,classID,blend;std::int32_t statesField;
    std::uint32_t states[9],hasUV,uvEnabled;float uvMatrix[9];
    SpvMaterialReference texture,animation,uvController;
};
struct SpvMaterialPass {std::uint32_t blend,layers;};
// Same original material grammar with explicit metadata-only references.
// No referenced resources are instantiated. Offsets are relative to this field stream.
SPV_API void* spv_material_read(const std::uint8_t*,std::uint32_t) noexcept;
SPV_API void spv_material_destroy(void*) noexcept;
SPV_API int spv_material_info(void*,SpvMaterialInfo*) noexcept;
SPV_API int spv_material_layers(void*,SpvMaterialLayer*,std::uint32_t) noexcept;
SPV_API int spv_material_passes(void*,SpvMaterialPass*,std::uint32_t) noexcept;
// Host template edit: actual partial reader associates scalar assignments;
// original writer slices supply replacement payloads. Existing headers and all
// other bytes remain intact. kind0 requires one standard layer and field17;
// kind1 edits render states and blend in every pass, without texture edits.
// Returns SerializedBytes containing the same-length material field stream.
SPV_API void* spv_material_patch_scalars(const std::uint8_t*,std::uint32_t,
    const std::uint32_t* states,std::uint32_t stateCount,std::uint32_t blend,
    const std::uint32_t* textureStates,std::uint32_t textureStateCount,std::uint32_t kind) noexcept;
// Same-length edit of the two actual Renderable-section assignments in Skin.
// Model/Skin sections are read by their real readers and retained unchanged;
// alphaSort is the explicit tool bool 0/1. Returns SerializedBytes field stream.
SPV_API void* spv_skin_patch_sort_scalars(const std::uint8_t*,std::uint32_t,
    std::uint32_t alphaSort,std::uint32_t priority) noexcept;
struct SpvModelInfo {
    std::uint32_t alpha,priority,projection,weights,renderableMask,modelMask,skinMask,bones;
    SpvMaterialReference material,fog,mesh;
};
struct SpvSkinBone {
    SpvMaterialReference reference;
    std::uint32_t id,inlineSize;
    float inverseBind[16];
};
struct SpvSkinPaletteField {
    std::uint32_t headerOffset,payloadOffset,payloadSize,assignmentOrder;
};
struct SpvSkinPaletteBinding {std::uint32_t nodeID;float inverseBind[16];};
// Field-only original Skin writer. The traced graph owns the actual bones;
// the caller retains their original payloads/IDs in its destination container.
// Same-ID repetitions are allowed; aliases and unread Node extents are refused.
SPV_API void* spv_skin_write_palette(void* graph,std::uint32_t weights,
    const SpvSkinPaletteBinding*,std::uint32_t count) noexcept;
// kind0 Model, kind1 Skin: original section readers with unresolved reference metadata.
SPV_API void* spv_model_read(const std::uint8_t*,std::uint32_t,std::uint32_t kind) noexcept;
SPV_API void spv_model_destroy(void*) noexcept;
SPV_API int spv_model_info(void*,SpvModelInfo*) noexcept;
SPV_API int spv_model_bones(void*,SpvSkinBone*,std::uint32_t) noexcept;
SPV_API int spv_model_palette_fields(void*,SpvSkinPaletteField*,std::uint32_t capacity,
    std::uint32_t* count) noexcept;
struct SpvAnimTextureInfo {std::uint32_t frames,hasTrack;float duration;};
struct SpvAnimTextureFrame {float time;SpvMaterialReference reference;};
// Scalar/reference observation only; derived Text layout is not available.
struct SpvTextInspectionInfo {
    std::uint32_t renderableMask,fieldMask,alpha,priority,color,wrapWidth,alignment,textLength,textByteCount,textFlags;
    SpvMaterialReference material,fog,font;
};
SPV_API void* spv_text_inspection_read(const std::uint8_t*,std::uint32_t) noexcept;
SPV_API void spv_text_inspection_destroy(void*) noexcept;
SPV_API int spv_text_inspection_info(void*,SpvTextInspectionInfo*) noexcept;
SPV_API int spv_text_inspection_bytes(void*,std::uint8_t*,std::uint32_t) noexcept;
SPV_API void* spv_anim_texture_read(const std::uint8_t*,std::uint32_t) noexcept;
SPV_API void spv_anim_texture_destroy(void*) noexcept;
SPV_API int spv_anim_texture_info(void*,SpvAnimTextureInfo*) noexcept;
SPV_API int spv_anim_texture_frames(void*,SpvAnimTextureFrame*,std::uint32_t) noexcept;
// Unwrapped track time: original end-time key selection, not a looping clock.
SPV_API int spv_anim_texture_index(void*,float,std::int32_t*) noexcept;
struct SpvFunctionInfo {std::uint32_t type;float frequency,amplitude,xOffset,yOffset,pitch;};
struct SpvColorFunctionInfo {std::uint32_t first,second;SpvFunctionInfo function;};
struct SpvUvFunctions {SpvFunctionInfo functions[7];float pivot[3],axis[3];};
struct SpvColorFunctions {SpvColorFunctionInfo colors[4];SpvFunctionInfo alpha;};
// Field bodies consumed by the original nested serializers, without controller
// creation, a material binding, animation-manager registration or evaluation.
SPV_API int spv_uv_functions_read(const std::uint8_t*,std::uint32_t,SpvUvFunctions*) noexcept;
SPV_API int spv_color_functions_read(const std::uint8_t*,std::uint32_t,SpvColorFunctions*) noexcept;
// Original field1/2 assignments with fresh constructor defaults; no reference
// resolution, affine validation or relation between matrices is implied.
SPV_API int spv_static_matrices(const std::uint8_t*,std::uint32_t,
    const SpvNodeField*,std::uint32_t,SpvStaticMatrices*) noexcept;
// Two independent finite authoring matrices (16 floats each). Calls the actual
// zero-renderable StaticRenderObject writer; returns its fields + terminator.
// Finite input is a host editing guard, not an original matrix-reader rule.
SPV_API void* spv_static_write_matrix_fields(const float* world,const float* inverse) noexcept;
struct SpvCollisionInfoValues {
    float position[3],rotation[4],scale[3],orientation[9];
    std::uint32_t group,fieldMask;
};
// Shared bounded field descriptors, scalar fields1/2 only; references remain
// separate inspection/whole-loader operations.
SPV_API int spv_collision_info_values(const std::uint8_t*,std::uint32_t,
    const SpvNodeField*,std::uint32_t,SpvCollisionInfoValues*) noexcept;
struct SpvNodeValues {
    float position[3],rotation[4],scale[3],orientation[9];
    std::uint32_t flags,billboard,bone,isStatic,animated;
};
// Apply schema-selected scalar fields to a real fresh Node. Raw authored flags
// and quaternion are observed separately from its effective flags/orientation.
// References are inspected elsewhere; this is not a resolved resource graph.
SPV_API int spv_node_values(const std::uint8_t*,std::uint32_t,const SpvNodeField*,std::uint32_t,SpvNodeValues*) noexcept;
// kind0 reads a retained prefix with declared full extent; kind1 also inspects
// the inline object header from the full payload. No FAT/cache resolution.
SPV_API int spv_reference_prefix(const std::uint8_t*,std::uint32_t count,std::uint32_t payloadSize,
    std::uint32_t kind,SpvReferencePrefix*) noexcept;
// preferredCode=FFFFFFFF selects the original automatic code. Unsupported
// ID31/forced escapes/real zero-size fields fail explicitly. A zero payload
// with no nonempty width preference is the legacy host shorthand for the
// original section terminator. Output is at most six bytes, no payload copy.
SPV_API int spv_write_field_header(std::uint32_t field,std::uint32_t payloadSize,std::uint32_t preferredCode,
    std::uint32_t preferExtended,std::uint8_t* output,std::uint32_t capacity,std::uint32_t* size) noexcept;
SPV_API int spv_node_local(const SpvNode*, float*, std::uint32_t) noexcept;
SPV_API int spv_skin_matrix(const float*, const float*, float*, std::uint32_t) noexcept;
SPV_API void* spv_scene_create(const SpvNode*, std::uint32_t) noexcept;
SPV_API void spv_scene_destroy(void*) noexcept;
SPV_API void* spv_clip_load(const std::uint8_t*, std::uint32_t) noexcept;
SPV_API void spv_clip_destroy(void*) noexcept;
SPV_API int spv_clip_info(void*, float*, std::uint32_t*) noexcept;
SPV_API int spv_clip_tag_count(void*, std::uint32_t*) noexcept;
SPV_API int spv_clip_track(void*, std::uint32_t, char*, std::uint32_t, SpvTrackInfo*) noexcept;
SPV_API int spv_clip_sample(void*, std::uint32_t, float, SpvSample*) noexcept;
SPV_API int spv_clip_channel(void*, std::uint32_t, std::uint32_t, SpvChannelInfo*) noexcept;
SPV_API int spv_clip_times(void*, std::uint32_t, std::uint32_t, float*, std::uint32_t) noexcept;
// Additive ABI2 metadata access: copies the actual prepared engine arrays.
SPV_API int spv_clip_axis_info(void*, std::uint32_t, std::uint32_t, std::uint32_t, SpvAxisInfo*) noexcept;
SPV_API int spv_clip_axis_values(void*, std::uint32_t, std::uint32_t, std::uint32_t, float*, std::uint32_t) noexcept;
SPV_API void* spv_clip_create_linear(const SpvLinearChannel*, std::uint32_t) noexcept;
// roleTracks contains 3 native track ordinals per node (position/rotation/scale),
// -1 for an absent role. Exact-name/disjoint-role binding is host policy.
SPV_API int spv_scene_bind(void*, void*, const std::int32_t*, std::uint32_t) noexcept;
SPV_API int spv_scene_sample(void*, float, float*, std::uint32_t) noexcept;
// World PRS for target-format adapters. Uses the same scene evaluation and
// reconstructed matrix-to-quaternion conversion as other native consumers.
SPV_API int spv_scene_pose(void*, float, SpvSample*, std::uint32_t) noexcept;
SPV_API int spv_scene_palette(void*, const SpvBone*, std::uint32_t, float*, std::uint32_t) noexcept;
