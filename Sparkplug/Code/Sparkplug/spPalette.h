#pragma once
// Original PC class name/ID. Header/TU path inferred, not recovered.
#include "../SparkBase/spBaseObject.h"
#include <array>
#include <cstddef>

namespace sparkplug::reconstruction
{
    class spPalette final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x591C0B9F;
        static constexpr std::size_t EntryByteCount=1024;
        spPalette() noexcept=default;
        // Native4B2C50 copies color bytes but resets index/base state. Virtual
        // Clone4B2CF0 instead factory-constructs then invokes no-op copy slot.
        spPalette(const spPalette& other) noexcept;
        spPalette& operator=(const spPalette&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] std::uint32_t GetIndexForAnalysis() const noexcept{return index_;}
        [[nodiscard]] bool HasInitializedEntriesForAnalysis() const noexcept{return entriesInitialized_;}
        [[nodiscard]] const std::array<std::byte,EntryByteCount>& GetEntriesForAnalysis() const noexcept{return entries_;}
        [[nodiscard]] bool SetEntriesForAnalysis(const std::byte* entries,std::size_t byteCount) noexcept;
    private:
        std::uint32_t index_=0xffffffffu;
        // Native ctor leaves14..413 uninitialized. Host zero storage is not
        // valid palette output until explicitly populated; no GPU registration.
        std::array<std::byte,EntryByteCount> entries_{};
        bool entriesInitialized_=false;
    };
}
