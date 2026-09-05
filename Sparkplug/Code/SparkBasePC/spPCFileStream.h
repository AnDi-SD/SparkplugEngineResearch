#pragma once

// Inferred header path.  The corresponding implementation path is preserved
// literally in the PC executable as
// Z:\Sparkplug\Code\SparkBasePC\spPCFileStream.cpp.

#include "../SparkBase/spFileStream.h"

namespace sparkplug::reconstruction
{
    class spPCFileStream : public spFileStream
    {
    public:
        static constexpr spClassID ClassID = 0x5EDF341C;

        spPCFileStream() noexcept = default;
        ~spPCFileStream() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        using spFileStream::Open;
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

        // Reconstruction-only observation helper.  Native storage at +0x1C
        // is a 32-bit HANDLE; the portable x64 build cannot share that ABI.
        [[nodiscard]] bool IsOpen() const noexcept;

    private:
        void* handle_ = nullptr;
    };
}
