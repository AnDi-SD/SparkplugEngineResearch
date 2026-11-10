#pragma once
// In-process C ABI for WinxCoop's existing visual pose interpolation.
// This is not a wire format, gameplay prediction, or native-game ownership.
// One trusted caller owns each opaque handle serially; no call may overlap
// destruction. An invalid/destroyed non-null handle is a caller error.
#include <stdint.h>

#if defined(_WIN32)
# if defined(WINXCOOP_POSE_INTERPOLATION_NATIVE_BUILD)
#  define WCPOSE_API __declspec(dllexport)
# else
#  define WCPOSE_API __declspec(dllimport)
# endif
# define WCPOSE_CALL __cdecl
#else
# define WCPOSE_API
# define WCPOSE_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum { WC_POSE_ABI_VERSION = 1, WC_POSE_TRANSFORM_BYTES = 60 };
enum { WC_POSE_OK = 1, WC_POSE_INVALID_ARGUMENT = -1,
       WC_POSE_REJECTED = -2, WC_POSE_ABI_MISMATCH = -3,
       WC_POSE_INTERNAL_ERROR = -4 };

#pragma pack(push, 4)
typedef struct WCPoseTransform {
    float position[3];
    float scale[3];
    float orientation[9];
} WCPoseTransform;
#pragma pack(pop)

typedef struct WCPoseHandle WCPoseHandle;
WCPOSE_API uint32_t WCPOSE_CALL wc_pose_abi_version(void);
WCPOSE_API uint32_t WCPOSE_CALL wc_pose_transform_bytes(void);
// Failure leaves the caller's output pointer unchanged. Successful creation
// returns one inactive handle; destroy it exactly once, after all calls return.
WCPOSE_API int32_t WCPOSE_CALL wc_pose_create(uint32_t abiVersion,
    uint32_t transformBytes, WCPoseHandle** output);
WCPOSE_API void WCPOSE_CALL wc_pose_destroy(WCPoseHandle* handle);
// All times are milliseconds on one local monotonic clock, never peer time.
// The existing interpolator validates poses, fixed scale, time and duration.
// Failures leave its state unchanged. Clear starts a fresh clock/scale cohort.
WCPOSE_API int32_t WCPOSE_CALL wc_pose_reset(WCPoseHandle*,
    const WCPoseTransform*, uint64_t now);
WCPOSE_API int32_t WCPOSE_CALL wc_pose_set_target(WCPoseHandle*,
    const WCPoseTransform*, uint64_t now, uint32_t durationMilliseconds);
// Failure also leaves output unchanged; a null output never advances time.
WCPOSE_API int32_t WCPOSE_CALL wc_pose_sample(WCPoseHandle*,
    uint64_t now, WCPoseTransform* output);
WCPOSE_API int32_t WCPOSE_CALL wc_pose_clear(WCPoseHandle*);

#ifdef __cplusplus
}
#endif
