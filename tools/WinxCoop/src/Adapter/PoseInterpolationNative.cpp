#define WINXCOOP_POSE_INTERPOLATION_NATIVE_BUILD
#include "PoseInterpolationNative.h"
#include "RemotePoseInterpolator.h"
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

// Match the existing ProgressionNative x86 C ABI export convention.
#if defined(_MSC_VER) && defined(_M_IX86)
#define WCPOSE_EXPORT(name) __pragma(comment(linker, "/EXPORT:" #name "=_" #name))
WCPOSE_EXPORT(wc_pose_abi_version)
WCPOSE_EXPORT(wc_pose_transform_bytes)
WCPOSE_EXPORT(wc_pose_create)
WCPOSE_EXPORT(wc_pose_destroy)
WCPOSE_EXPORT(wc_pose_reset)
WCPOSE_EXPORT(wc_pose_set_target)
WCPOSE_EXPORT(wc_pose_sample)
WCPOSE_EXPORT(wc_pose_clear)
#undef WCPOSE_EXPORT
#endif

using winxcoop::adapter::RemotePoseInterpolator;
using winxcoop::adapter::Transform;
static_assert(sizeof(WCPoseTransform) == WC_POSE_TRANSFORM_BYTES);
static_assert(sizeof(Transform) == sizeof(WCPoseTransform));
static_assert(offsetof(WCPoseTransform, position) == 0 && offsetof(WCPoseTransform, scale) == 12 &&
    offsetof(WCPoseTransform, orientation) == 24);
static_assert(offsetof(Transform, position) == offsetof(WCPoseTransform, position) &&
    offsetof(Transform, scale) == offsetof(WCPoseTransform, scale) &&
    offsetof(Transform, orientation) == offsetof(WCPoseTransform, orientation));
static_assert(std::is_standard_layout_v<WCPoseTransform> && std::is_trivially_copyable_v<WCPoseTransform> &&
    std::is_standard_layout_v<Transform> && std::is_trivially_copyable_v<Transform>);

struct WCPoseHandle final { RemotePoseInterpolator interpolator; };

namespace
{
    Transform In(const WCPoseTransform& value) noexcept
    { Transform result{}; std::memcpy(&result, &value, sizeof(result)); return result; }
    WCPoseTransform Out(const Transform& value) noexcept
    { WCPoseTransform result{}; std::memcpy(&result, &value, sizeof(result)); return result; }
    int32_t Result(bool value) noexcept { return value ? WC_POSE_OK : WC_POSE_REJECTED; }
}

uint32_t WCPOSE_CALL wc_pose_abi_version(void) { return WC_POSE_ABI_VERSION; }
uint32_t WCPOSE_CALL wc_pose_transform_bytes(void) { return WC_POSE_TRANSFORM_BYTES; }
int32_t WCPOSE_CALL wc_pose_create(uint32_t abiVersion, uint32_t transformBytes, WCPoseHandle** output)
{
    if (!output) return WC_POSE_INVALID_ARGUMENT;
    if (abiVersion != WC_POSE_ABI_VERSION || transformBytes != sizeof(WCPoseTransform)) return WC_POSE_ABI_MISMATCH;
    try
    {
        auto* created = new WCPoseHandle;
        *output = created;
        return WC_POSE_OK;
    }
    catch (...) { return WC_POSE_INTERNAL_ERROR; }
}
void WCPOSE_CALL wc_pose_destroy(WCPoseHandle* handle) { delete handle; }
int32_t WCPOSE_CALL wc_pose_reset(WCPoseHandle* handle, const WCPoseTransform* pose, uint64_t now)
{
    if (!handle || !pose) return WC_POSE_INVALID_ARGUMENT;
    return Result(handle->interpolator.Reset(In(*pose), now));
}
int32_t WCPOSE_CALL wc_pose_set_target(WCPoseHandle* handle, const WCPoseTransform* pose,
    uint64_t now, uint32_t durationMilliseconds)
{
    if (!handle || !pose) return WC_POSE_INVALID_ARGUMENT;
    return Result(handle->interpolator.SetTarget(In(*pose), now, durationMilliseconds));
}
int32_t WCPOSE_CALL wc_pose_sample(WCPoseHandle* handle, uint64_t now, WCPoseTransform* output)
{
    if (!handle || !output) return WC_POSE_INVALID_ARGUMENT;
    Transform sampled{};
    if (!handle->interpolator.Sample(now, sampled)) return WC_POSE_REJECTED;
    *output = Out(sampled);
    return WC_POSE_OK;
}
int32_t WCPOSE_CALL wc_pose_clear(WCPoseHandle* handle)
{
    if (!handle) return WC_POSE_INVALID_ARGUMENT;
    handle->interpolator.Clear();
    return WC_POSE_OK;
}
