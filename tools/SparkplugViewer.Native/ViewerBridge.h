#pragma once
#include <cstdint>
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
struct SpvMeshBVInfo {std::uint32_t primitiveType,vertices,indices,faces,hasFaces,fieldMask,vertexPayloadOffset,faceClassID;};
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
// Bounded inspection only. Header reads the original prefix and checks the
// opaque packet extent; it neither materializes nor executes a DMA packet.
SPV_API int spv_ps2_mesh_header(const std::uint8_t*,std::uint32_t,SpvPs2MeshHeader*) noexcept;
SPV_API int spv_mesh_bounds(const std::uint8_t*,std::uint32_t,float* minimumMaximum) noexcept;
// kind0: complete MeshData field stream; kind1: portable field; kind2: PC field.
// kind3/4: same portable/PC reader, metadata only (no host attribute validation).
// platformMask controls the original whole-field selector, without fallback.
SPV_API void* spv_mesh_read(const std::uint8_t*,std::uint32_t,std::uint32_t kind,std::uint32_t platformMask) noexcept;
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
SPV_API void spv_graph_destroy(void*) noexcept;
SPV_API int spv_graph_info(void*,std::uint32_t* objects,std::uint32_t* nodes,std::uint32_t* rootID) noexcept;
SPV_API int spv_graph_object(void*,std::uint32_t ordinal,char* name,std::uint32_t capacity,SpvGraphObject*) noexcept;
SPV_API int spv_graph_node(void*,std::uint32_t id,SpvGraphNode*) noexcept;
// Select actual loaded Node objects, including every parent of the selection.
// The scene retains their whole resource graph and preserves authored matrices.
SPV_API void* spv_graph_scene(void*,const std::uint32_t* ids,std::uint32_t count) noexcept;
SPV_API std::uint32_t spv_abi_version() noexcept;
SPV_API const char* spv_last_error() noexcept;
SPV_API int spv_read_field(const std::uint8_t*, std::uint32_t, SpvFieldHeader*) noexcept;
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
