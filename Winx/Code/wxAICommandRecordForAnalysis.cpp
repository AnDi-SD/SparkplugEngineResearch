#include "wxAICommandRecordForAnalysis.h"
#include "Analysis/PC/wxAICommandRecordAbi.h"
#include <cstddef>

namespace winx::reconstruction
{
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::AdmitStorageForAnalysis() noexcept
    {
        if (profile_ != ProfileForAnalysis::PcCaptured703090)
            return lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        std::uint32_t primary = 0;
        for (unsigned byte = 0; byte != 4; ++byte)
            primary |= std::uint32_t{bytes_[byte]} << (8 * byte);
        return lastResult_ = primary == evidence::pc::ForeignCommandPrimary
            ? ResultForAnalysis::Completed : ResultForAnalysis::CapturedTableNotAdmitted;
    }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::StoreWordForAnalysis(
        const std::size_t offset, const std::uint32_t value) noexcept
    {
        if (AdmitStorageForAnalysis() != ResultForAnalysis::Completed) return lastResult_;
        for (unsigned byte = 0; byte != 4; ++byte)
            bytes_[offset + byte] = static_cast<std::uint8_t>(value >> (8 * byte));
        return lastResult_ = ResultForAnalysis::Completed;
    }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::StoreWord08ForAnalysis(
        const std::uint32_t value) noexcept
    { return StoreWordForAnalysis(evidence::pc::ForeignCommandRawWord08Offset, value); }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::StoreWord04ForAnalysis(
        const std::uint32_t value) noexcept
    { return StoreWordForAnalysis(evidence::pc::ForeignCommandRawWord04Offset, value); }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::StoreByte1BForAnalysis(
        const std::uint8_t value) noexcept
    {
        if (AdmitStorageForAnalysis() != ResultForAnalysis::Completed) return lastResult_;
        bytes_[evidence::pc::ForeignCommandRawByte1BOffset] = value;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::StoreByte23ForAnalysis(
        const std::uint8_t value) noexcept
    {
        if (AdmitStorageForAnalysis() != ResultForAnalysis::Completed) return lastResult_;
        bytes_[0x23] = value;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::StoreByte1DForAnalysis(
        const std::uint8_t value) noexcept
    {
        if (AdmitStorageForAnalysis() != ResultForAnalysis::Completed) return lastResult_;
        bytes_[0x1D] = value;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::StoreByte20ForAnalysis(
        const std::uint8_t value) noexcept
    {
        if (AdmitStorageForAnalysis() != ResultForAnalysis::Completed) return lastResult_;
        bytes_[0x20] = value;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::StoreByte21ForAnalysis(
        const std::uint8_t value) noexcept
    {
        if (AdmitStorageForAnalysis() != ResultForAnalysis::Completed) return lastResult_;
        bytes_[0x21] = value;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::StoreByte22ForAnalysis(
        const std::uint8_t value) noexcept
    {
        if (AdmitStorageForAnalysis() != ResultForAnalysis::Completed) return lastResult_;
        bytes_[0x22] = value;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::StoreByte24ForAnalysis(
        const std::uint8_t value) noexcept
    {
        if (AdmitStorageForAnalysis() != ResultForAnalysis::Completed) return lastResult_;
        bytes_[0x24] = value;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::StoreByte5CForAnalysis(
        const std::uint8_t value) noexcept
    {
        if (AdmitStorageForAnalysis() != ResultForAnalysis::Completed) return lastResult_;
        bytes_[0x5C] = value;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    wxAICommandRecordForAnalysis::ResultForAnalysis wxAICommandRecordForAnalysis::ResetForAnalysis() noexcept
    {
        if (profile_ != ProfileForAnalysis::PcCaptured703090)
            return lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        // Analytical admission only: raw PC table bytes are an identity tag,
        // never a live C++ vptr. Native Reset itself does not read this word.
        std::uint32_t capturedPrimary = 0;
        for (unsigned byte = 0; byte != 4; ++byte)
            capturedPrimary |= std::uint32_t{bytes_[byte]} << (8 * byte);
        if (capturedPrimary != evidence::pc::ForeignCommandPrimary)
            return lastResult_ = ResultForAnalysis::CapturedTableNotAdmitted;
        struct Write final { std::size_t offset, bytes; };
        // PC5992E2..599351: mirror the38 native destination ranges in order,
        // including the nonascending byte destinations. Other bytes are raw.
        constexpr std::array<Write, 38> writes{{
            {0x04,4}, {0x08,4}, {0x0C,4}, {0x10,4}, {0x14,4},
            {0x1E,1}, {0x19,1}, {0x1A,1}, {0x1B,1}, {0x1C,1}, {0x1D,1}, {0x1F,1},
            {0x20,1}, {0x21,1}, {0x22,1}, {0x23,1}, {0x24,1}, {0x25,1}, {0x26,1}, {0x27,1}, {0x28,1},
            {0x2A,1}, {0x29,1}, {0x2C,1}, {0x2B,1}, {0x2F,1}, {0x2E,1}, {0x31,1}, {0x30,1}, {0x2D,1},
            {0x5C,1}, {0x5D,1}, {0x5E,1}, {0x5F,1}, {0x60,1}, {0x64,1}, {0x6C,4}, {0x70,1}
        }};
        for (const auto& write : writes)
            for (std::size_t byte = 0; byte != write.bytes; ++byte) bytes_[write.offset + byte] = 0;
        return lastResult_ = ResultForAnalysis::Completed;
    }
}
