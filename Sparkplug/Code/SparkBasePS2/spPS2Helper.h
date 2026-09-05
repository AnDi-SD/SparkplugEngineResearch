#pragma once

// Inferred header and module path.  The class name and RTTI identity are
// literal PS2 executable evidence.  Original method names were not retained,
// so address-based names are used for the recovered non-virtual entry points.

#include "../SparkBase/spBaseObject.h"

#include <cstdint>
#include <memory>
#include <string>

namespace sparkplug::reconstruction
{
    class spPS2Helper final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x7E3C519B;

        spPS2Helper() noexcept;
        ~spPS2Helper() override;

        spPS2Helper(const spPS2Helper&) = delete;
        spPS2Helper& operator=(const spPS2Helper&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spPS2Helper* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // PS2 0x001E91B0.  The native routine writes into an unchecked caller
        // buffer.  This host reconstruction returns owned storage while
        // retaining the exact path transformation rules.
        [[nodiscard]] std::string sub_001E91B0(const char* path) const;

        // PS2 0x001E93A0.
        void sub_001E93A0(std::uint32_t mode) noexcept;

        [[nodiscard]] std::uint32_t GetMode() const noexcept;
        [[nodiscard]] std::uint32_t GetField18() const noexcept;
        [[nodiscard]] const std::string& GetPathPrefix() const noexcept;

        // Analytical test seam for the inline 0x100-byte native prefix.  No
        // original setter has yet been found.
        [[nodiscard]] bool SetPathPrefixForAnalysis(const char* prefix);

    private:
        static spPS2Helper* instance_;
        std::uint32_t mode_ = 0;
        std::uint32_t field18_ = 0;
        std::string pathPrefix_;
    };
}
