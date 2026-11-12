#pragma once
#include "spSphereContactMath.h"
#include <array>
#include <cstdint>
#include <cstring>

namespace sparkplug::evidence::pc::contact_row_math
{
    // Historical Sphere placement is retained. This consumer only composes
    // the existing nearest64 integer operations; it defines no new kernel.
    namespace arithmetic = sphere_contact_math;
    inline arithmetic::Number FromWord(const std::uint32_t word) noexcept
    {
        float value; std::memcpy(&value, &word, sizeof(value));
        return arithmetic::From(value);
    }
    inline std::uint32_t StoreWord(const arithmetic::Number value) noexcept
    {
        const float stored = arithmetic::Store(value); std::uint32_t word;
        std::memcpy(&word, &stored, sizeof(word)); return word;
    }
    struct Values final
    {
        std::uint32_t reciprocal = 0;
        std::array<std::uint32_t, 3> displacement{}, angular{};
        std::uint32_t rhs = 0;
    };
    inline Values Query(const std::uint32_t scalar,
        const std::array<std::uint32_t, 3>& point,
        const std::array<std::uint32_t, 3>& normal,
        const std::array<std::uint32_t, 3>& bodyPoint,
        const std::uint32_t cc) noexcept
    {
        Values output;
        // Actual base FDIV1/scalar nearest64, followed by F32 self60 store.
        output.reciprocal = StoreWord(arithmetic::Reciprocal(FromWord(scalar)));
        for (unsigned lane = 0; lane != 3; ++lane)
            output.displacement[lane] = StoreWord(arithmetic::Subtract(
                FromWord(point[lane]), FromWord(bodyPoint[lane])));
        const auto rx = FromWord(output.displacement[0]);
        const auto ry = FromWord(output.displacement[1]);
        const auto rz = FromWord(output.displacement[2]);
        const auto nx = FromWord(normal[0]);
        const auto ny = FromWord(normal[1]);
        const auto nz = FromWord(normal[2]);
        const auto angularX = arithmetic::Subtract(arithmetic::Multiply(ry, nz), arithmetic::Multiply(rz, ny));
        const auto angularY = arithmetic::Subtract(arithmetic::Multiply(rz, nx), arithmetic::Multiply(nz, rx));
        const auto angularZ = arithmetic::Subtract(arithmetic::Multiply(ny, rx), arithmetic::Multiply(ry, nx));
        // PC48B703/70D/710/712: spill/store Z, then X and Y.
        output.angular[2] = StoreWord(angularZ);
        output.angular[0] = StoreWord(angularX);
        output.angular[1] = StoreWord(angularY);
        // Use the reread F32 reciprocal and original positive-zero constant.
        output.rhs = StoreWord(arithmetic::Multiply(arithmetic::Multiply(
            FromWord(output.reciprocal), FromWord(0)), FromWord(cc)));
        return output;
    }
}
