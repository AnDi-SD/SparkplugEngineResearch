#pragma once

#include <cstdint>
#include <list>

namespace sparkplug::reconstruction
{
    // Bounded PC model for an existing sentinel list of borrowed sky boxes.
    class spSkyBoxManager
    {
    public:
        static constexpr std::uint32_t ClassID = 0x61C23595;

        struct CapturedSkyBoxForAnalysis
        {
            std::uint32_t address = 0;
            std::uint32_t classId = 0;
        };

        spSkyBoxManager() = default;
        spSkyBoxManager(const spSkyBoxManager&) = delete;
        spSkyBoxManager& operator=(const spSkyBoxManager&) = delete;

        // Each admitted call owns one list entry and preserves duplicate
        // borrowed identities. The addresses remain opaque to this model.
        [[nodiscard]] bool AppendSkyBoxForAnalysis(CapturedSkyBoxForAnalysis skyBox);

        [[nodiscard]] const std::list<std::uint32_t>& GetSkyBoxesForAnalysis() const noexcept
        {
            return skyBoxes_;
        }

    private:
        std::list<std::uint32_t> skyBoxes_;
    };
}
