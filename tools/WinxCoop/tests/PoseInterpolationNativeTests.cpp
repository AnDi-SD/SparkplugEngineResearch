// The C ABI is exercised through the DLL import library. These are owned
// interpolation fixtures, not native game/visual acceptance.
#include "../src/Adapter/PoseInterpolationNative.h"
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    unsigned checks = 0;
    constexpr double Pi = 3.14159265358979323846;
    void Check(bool condition, const char* text)
    { ++checks; if (!condition) throw std::runtime_error(text); }
    void Ok(int32_t value, const char* text) { Check(value == WC_POSE_OK, text); }
    bool Near(double a, double b) { return std::abs(a - b) < 0.00002; }
    bool Same(const WCPoseTransform& a, const WCPoseTransform& b)
    { return std::memcmp(&a, &b, sizeof(a)) == 0; }
    WCPoseTransform Pose()
    {
        WCPoseTransform result{};
        result.scale[0] = result.scale[1] = result.scale[2] = 1;
        result.orientation[0] = result.orientation[4] = result.orientation[8] = 1;
        return result;
    }
    WCPoseTransform Turn(double degrees)
    {
        auto result = Pose();
        const float c = static_cast<float>(std::cos(degrees * Pi / 180));
        const float s = static_cast<float>(std::sin(degrees * Pi / 180));
        result.orientation[0] = result.orientation[8] = c;
        result.orientation[2] = s; result.orientation[6] = -s;
        return result;
    }
    void Matrix(const WCPoseTransform& actual, const WCPoseTransform& expected)
    { for (unsigned i = 0; i < 9; ++i) Check(Near(actual.orientation[i], expected.orientation[i]), "analytic rotation through C ABI"); }
    struct Owner final
    {
        WCPoseHandle* handle{};
        Owner() { Ok(wc_pose_create(WC_POSE_ABI_VERSION, sizeof(WCPoseTransform), &handle), "create owner"); Check(handle != nullptr, "non-null opaque owner"); }
        ~Owner() { wc_pose_destroy(handle); }
        Owner(const Owner&) = delete;
        Owner& operator=(const Owner&) = delete;
    };

    void AbiAndNulls()
    {
        static_assert(sizeof(WCPoseTransform) == 60 && alignof(WCPoseTransform) == 4);
        static_assert(offsetof(WCPoseTransform, position) == 0 && offsetof(WCPoseTransform, scale) == 12 &&
            offsetof(WCPoseTransform, orientation) == 24);
        Check(wc_pose_abi_version() == 1 && wc_pose_transform_bytes() == 60, "exported ABI/version sizes");
        Check(wc_pose_create(1, 60, nullptr) == WC_POSE_INVALID_ARGUMENT, "null create output");
        auto* sentinel = reinterpret_cast<WCPoseHandle*>(static_cast<uintptr_t>(1));
        auto* output = sentinel;
        Check(wc_pose_create(2, 60, &output) == WC_POSE_ABI_MISMATCH && output == sentinel, "wrong version preserves handle output");
        Check(wc_pose_create(1, 59, &output) == WC_POSE_ABI_MISMATCH && output == sentinel, "wrong size preserves handle output");
        auto pose = Pose(), before = pose;
        Check(wc_pose_reset(nullptr, &pose, 0) == WC_POSE_INVALID_ARGUMENT, "null reset handle");
        Check(wc_pose_set_target(nullptr, &pose, 0, 100) == WC_POSE_INVALID_ARGUMENT, "null target handle");
        Check(wc_pose_sample(nullptr, 0, &pose) == WC_POSE_INVALID_ARGUMENT && Same(pose, before), "null sample handle preserves output");
        Check(wc_pose_clear(nullptr) == WC_POSE_INVALID_ARGUMENT, "null clear handle");
        wc_pose_destroy(nullptr);
        Owner owner;
        Check(wc_pose_reset(owner.handle, nullptr, 100) == WC_POSE_INVALID_ARGUMENT, "null reset input");
        Check(wc_pose_set_target(owner.handle, nullptr, 100, 100) == WC_POSE_INVALID_ARGUMENT, "null target input");
        Check(wc_pose_sample(owner.handle, 100, &pose) == WC_POSE_REJECTED && Same(pose, before), "new owner inactive without output mutation");
    }

    void PoseMappingAndRetarget()
    {
        Owner owner;
        auto initial = Pose(); initial.position[0] = -10; initial.position[1] = 20; initial.position[2] = -30;
        initial.scale[0] = 1; initial.scale[1] = 2; initial.scale[2] = 3;
        auto target = Turn(90); target.scale[0] = 1; target.scale[1] = 2; target.scale[2] = 3;
        target.position[0] = 90; target.position[1] = -20; target.position[2] = 170;
        Ok(wc_pose_reset(owner.handle, &initial, 100), "reset mapped three-axis pose");
        WCPoseTransform out{};
        Ok(wc_pose_sample(owner.handle, 100, &out), "sample reset pose");
        Check(Same(out, initial), "all fifteen float fields mapped without transposition");
        Ok(wc_pose_set_target(owner.handle, &target, 100, 100), "set mapped target");
        Ok(wc_pose_sample(owner.handle, 125, &out), "sample first target quarter");
        Check(Near(out.position[0], 15) && Near(out.position[1], 10) && Near(out.position[2], 20), "position ordering preserved");
        Matrix(out, Turn(22.5));
        Check(out.scale[0] == 1 && out.scale[1] == 2 && out.scale[2] == 3, "nonuniform scale ordering preserved");
        auto replacement = Turn(180); replacement.scale[0] = 1; replacement.scale[1] = 2; replacement.scale[2] = 3;
        replacement.position[0] = 190; replacement.position[1] = 40; replacement.position[2] = 370;
        // No sample at150 precedes replacement: native SetTarget must start
        // at its current interpolation, not at the previous start or endpoint.
        Ok(wc_pose_set_target(owner.handle, &replacement, 150, 100), "replace target midway through ABI");
        Ok(wc_pose_sample(owner.handle, 150, &out), "sample replacement boundary");
        Check(Near(out.position[0], 40) && Near(out.position[1], 0) && Near(out.position[2], 70), "retarget boundary is continuous");
        Matrix(out, Turn(45));
        Ok(wc_pose_sample(owner.handle, 200, &out), "sample replacement midpoint");
        Check(Near(out.position[0], 115) && Near(out.position[1], 20) && Near(out.position[2], 220), "retarget interpolates current pose");
        Matrix(out, Turn(112.5));
        Ok(wc_pose_sample(owner.handle, 250, &out), "sample replacement endpoint");
        Matrix(out, Turn(180));
        Check(out.position[0] == 190 && out.position[1] == 40 && out.position[2] == 370, "retarget endpoint exact");
    }

    void RejectionsPreserveStateAndOutput()
    {
        Owner owner;
        auto initial = Pose(), target = Turn(90); target.position[0] = 100;
        Ok(wc_pose_reset(owner.handle, &initial, 100), "reset rejection fixture");
        Ok(wc_pose_set_target(owner.handle, &target, 100, 100), "target rejection fixture");
        Check(wc_pose_sample(owner.handle, 200, nullptr) == WC_POSE_INVALID_ARGUMENT, "null output rejected before clock advancement");
        WCPoseTransform out{};
        Ok(wc_pose_sample(owner.handle, 125, &out), "earlier sample remains allowed after null output");
        Check(Near(out.position[0], 25), "null output preserved segment and time");
        const auto before = out;
        Check(wc_pose_sample(owner.handle, 124, &out) == WC_POSE_REJECTED && Same(out, before), "clock rewind preserves output");
        Check(wc_pose_set_target(owner.handle, &target, 124, 100) == WC_POSE_REJECTED, "target time rewind rejected");
        Check(wc_pose_reset(owner.handle, &initial, 124) == WC_POSE_REJECTED, "reset time rewind rejected");
        Check(wc_pose_set_target(owner.handle, &target, 180, 0) == WC_POSE_REJECTED, "zero duration rejected");
        Check(wc_pose_set_target(owner.handle, &target, 180, 1001) == WC_POSE_REJECTED, "excessive duration rejected");
        auto bad = target; bad.orientation[0] = -1; bad.orientation[2] = 0; bad.orientation[6] = 0; bad.orientation[8] = 1;
        Check(wc_pose_set_target(owner.handle, &bad, 180, 100) == WC_POSE_REJECTED, "reflected matrix rejected by native implementation");
        bad = target; bad.position[1] = (std::numeric_limits<float>::quiet_NaN)();
        Check(wc_pose_set_target(owner.handle, &bad, 180, 100) == WC_POSE_REJECTED, "NaN target rejected");
        bad = initial; bad.scale[2] = 2;
        Check(wc_pose_reset(owner.handle, &bad, 180) == WC_POSE_REJECTED, "active scale cohort cannot change");
        Ok(wc_pose_sample(owner.handle, 150, &out), "rejected future inputs preserve earlier legal clock");
        Check(Near(out.position[0], 50), "rejections preserve original segment");
        Matrix(out, Turn(45));
        const auto maximum = (std::numeric_limits<uint64_t>::max)();
        Check(wc_pose_set_target(owner.handle, &target, maximum - 10, 100) == WC_POSE_REJECTED, "target end overflow rejected");
        Ok(wc_pose_sample(owner.handle, 175, &out), "overflow rejection leaves clock unchanged");
        Check(Near(out.position[0], 75), "overflow rejection leaves path unchanged");
        Ok(wc_pose_clear(owner.handle), "clear active owner");
        const auto cleared = out;
        Check(wc_pose_sample(owner.handle, 175, &out) == WC_POSE_REJECTED && Same(out, cleared), "clear deactivates without sample output mutation");
        Check(wc_pose_set_target(owner.handle, &target, 0, 100) == WC_POSE_REJECTED, "clear requires new explicit reset");
        Ok(wc_pose_reset(owner.handle, &bad, 0), "clear permits new scale and local clock cohort");
        Ok(wc_pose_sample(owner.handle, 0, &out), "sample new cohort");
        Check(out.scale[2] == 2, "new scale established only after clear");
    }
}

int main()
{
    try
    {
        AbiAndNulls(); PoseMappingAndRetarget(); RejectionsPreserveStateAndOutput();
        std::cout << "PoseInterpolationNative C ABI checks passed: " << checks << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "PoseInterpolationNative failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
