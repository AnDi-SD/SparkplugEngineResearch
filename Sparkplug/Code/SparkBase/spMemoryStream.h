#pragma once

// Inferred header path.  The original implementation path is proven as
// Z:\Sparkplug\Code\SparkBase\spMemoryStream.cpp on PC and as the matching
// short filename on PS2.  No header path or declarations survive in either
// executable.  Method names explicitly marked analytical describe proven
// behavior without claiming the original spelling.

#include "spStream.h"

#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spMemoryStream final : public spStream
    {
    public:
        static constexpr spClassID ClassID = 0x57177DB5;
        static constexpr std::uint32_t DefaultGrowthQuantum = 5000;

        spMemoryStream() noexcept = default;
        ~spMemoryStream() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] bool Open(const char* streamName) override;
        [[nodiscard]] bool Open(
            std::uint32_t mode,
            const char* streamName) override;
        [[nodiscard]] bool Close() override;
        [[nodiscard]] bool Seek(
            SeekSource source,
            std::int32_t offset) override;
        [[nodiscard]] bool GetCurrentPosition(
            std::uint32_t& position) const override;
        [[nodiscard]] bool ReadData(
            void* destination,
            std::uint32_t byteCount) override;
        [[nodiscard]] bool WriteData(
            const void* source,
            std::uint32_t byteCount) override;
        [[nodiscard]] bool vfunc_WriteFromStream(
            spStream* source,
            std::uint32_t byteCount) override;
        [[nodiscard]] bool GetSize(std::uint32_t* size) const override;
        [[nodiscard]] void* GetBuffer() noexcept override;

        // PC 0x00465500 / PS2 0x00112260 discard the current allocation,
        // allocate exactly byteCount bytes, make all of them logical data and
        // rewind.  The original method name has not been recovered.
        [[nodiscard]] bool ResizeAndSetSize(std::uint32_t byteCount);

        // PC-only linked helper 0x00465540 returns the buffer while clearing
        // both ownership and resize flags.  The stream retains the pointer;
        // its caller becomes responsible for eventually freeing it.  The
        // portable name is analytical and intentionally makes that transfer
        // conspicuous.
        [[nodiscard]] std::uint8_t* ReleaseBuffer() noexcept;

        // PC-only linked helper 0x00465550 clears logical size and position
        // without releasing capacity.  The original name is unknown.
        [[nodiscard]] bool Reset() noexcept;

        [[nodiscard]] std::uint32_t GetCapacity() const noexcept;
        [[nodiscard]] bool IsResizeEnabled() const noexcept;
        [[nodiscard]] bool OwnsBuffer() const noexcept;

    private:
        [[nodiscard]] bool PrepareWrite(std::uint32_t byteCount);

        // Portable storage follows the native state machine but does not claim
        // host ABI compatibility.  Exact 32-bit offsets are recorded in both
        // platform evidence headers.
        std::uint32_t capacity_ = 0;
        std::uint32_t size_ = 0;
        std::uint32_t growthQuantum_ = DefaultGrowthQuantum;
        bool resizeEnabled_ = true;
        std::uint32_t position_ = 0;
        std::uint8_t* buffer_ = nullptr;
        bool ownsBuffer_ = true;
    };
}
