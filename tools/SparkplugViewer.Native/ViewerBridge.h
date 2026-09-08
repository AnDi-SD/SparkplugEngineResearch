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
struct SpvLinearChannel { const float* times; const float* values; std::uint32_t count; };
struct SpvFieldHeader { std::uint32_t field, payloadSize, headerSize; };
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
SPV_API int spv_clip_track(void*, std::uint32_t, char*, std::uint32_t, SpvTrackInfo*) noexcept;
SPV_API int spv_clip_sample(void*, std::uint32_t, float, SpvSample*) noexcept;
SPV_API int spv_clip_channel(void*, std::uint32_t, std::uint32_t, SpvChannelInfo*) noexcept;
SPV_API int spv_clip_times(void*, std::uint32_t, std::uint32_t, float*, std::uint32_t) noexcept;
SPV_API void* spv_clip_create_linear(const SpvLinearChannel*, std::uint32_t) noexcept;
// roleTracks contains 3 native track ordinals per node (position/rotation/scale),
// -1 for an absent role. Exact-name/disjoint-role binding is host policy.
SPV_API int spv_scene_bind(void*, void*, const std::int32_t*, std::uint32_t) noexcept;
SPV_API int spv_scene_sample(void*, float, float*, std::uint32_t) noexcept;
SPV_API int spv_scene_palette(void*, const SpvBone*, std::uint32_t, float*, std::uint32_t) noexcept;
