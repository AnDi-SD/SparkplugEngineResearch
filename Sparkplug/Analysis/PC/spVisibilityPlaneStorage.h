#pragma once

// Analytical helper namespace, NOT an original recovered class/template name.
// PC46B720 logical resize,46ADC0 all-enabled,45EA00 vector destruction.
// Does not implement unresolved45E870 assignment or46C0F0 outer-stack preparation.
// Evidence: docs/research/native-pc-visibility-plane-storage.md.
#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace sparkplug::evidence::pc::visibility_storage
{
    struct Plane
    {
        // Raw float bits preserve NaN payloads and signed zero without arithmetic.
        std::array<std::uint32_t, 4> equation{};
        std::uint8_t enabled{};
        std::array<std::uint8_t, 3> padding{};
    };
    static_assert(sizeof(Plane) == 20);

    struct PlaneSetForAnalysis
    {
        // Working label only: native field00 survives these helpers; its exact
        // original member name and complete allocator/state role remain unknown.
        std::uint32_t allocatorWord{};
        std::vector<Plane> storage;
        std::uint32_t logicalSize{};
        std::uint32_t activeCount{};
    };

    inline void CopyValueWithoutPadding(Plane& destination, const Plane& source)
    {
        destination.equation = source.equation;
        destination.enabled = source.enabled;
    }

    inline void ResizeForAnalysis(PlaneSetForAnalysis& value, std::uint32_t count, Plane fill)
    {
        // Host-only safety guard, not a limit or validation found in the game.
        if (count > 4096 || value.logicalSize > value.storage.size())
            throw std::out_of_range("bounded plane storage analysis");
        if (count > value.storage.size())
        {
            const auto capacity =
                std::max<std::size_t>(count, value.storage.size() + value.storage.size() / 2);
            std::vector<Plane> replacement(capacity);
            for (std::uint32_t i = 0; i < value.logicalSize; ++i)
                CopyValueWithoutPadding(replacement[i], value.storage[i]);
            for (auto i = value.logicalSize; i < count; ++i)
                CopyValueWithoutPadding(replacement[i], fill);
            value.storage.swap(replacement);
        }
        else
        {
            for (auto i = value.logicalSize; i < count; ++i)
                CopyValueWithoutPadding(value.storage[i], fill);
        }
        value.logicalSize = count;
        // Native outer activeCount is intentionally NOT repaired by resize.
        // storage.size models native capacity, NOT std::vector host capacity.
        // New padding is native-indeterminate; host value-initialization uses0.
        // Reused padding is preserved. Fill is passed by value, as in native46B720.
    }

    inline void SetEnabledAllForAnalysis(PlaneSetForAnalysis& value, std::uint32_t raw)
    {
        if (value.logicalSize > value.storage.size())
            throw std::out_of_range("invalid plane analysis range");
        const auto flag = static_cast<std::uint8_t>(raw);
        value.activeCount = flag ? value.logicalSize : 0;
        for (std::uint32_t i = 0; i < value.logicalSize; ++i)
            value.storage[i].enabled = flag;
    }

    inline void InitializeVectorCopyForAnalysis(PlaneSetForAnalysis& destination,
                                                const PlaneSetForAnalysis& source)
    {
        // Actual45E530 initializes fresh vector storage, NOT assignment45E870.
        // Reject constructed/self destination instead of inventing overwrite semantics.
        if (&destination == &source || !destination.storage.empty() || destination.logicalSize ||
            source.logicalSize > source.storage.size() || source.logicalSize > 4096)
            throw std::out_of_range("fresh bounded plane copy destination required");
        destination.storage.resize(source.logicalSize);
        destination.logicalSize = source.logicalSize;
        for (std::uint32_t i = 0; i < source.logicalSize; ++i)
            CopyValueWithoutPadding(destination.storage[i], source.storage[i]);
        // Destination allocatorWord/activeCount survive; outer caller copies count separately.
    }

    inline void ReleaseVectorForAnalysis(PlaneSetForAnalysis& value)
    {
        std::vector<Plane>().swap(value.storage);
        value.logicalSize = 0;
        // Native only clears begin/end/capacity; field00/outer activeCount survive.
    }
} // namespace sparkplug::evidence::pc::visibility_storage
