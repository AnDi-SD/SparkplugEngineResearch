#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::reconstruction
{
    // Our captured data facade for the foreign PC command record. Its native
    // class name/RTTI identity and physical inheritance remain unknown.
    class wxAICommandRecordForAnalysis final
    {
    public:
        using RawBytesForAnalysis = std::array<std::uint8_t, 0x74>;
        enum class ProfileForAnalysis { Unqualified, PcCaptured703090 };
        enum class ResultForAnalysis { Completed, UnqualifiedProfile, CapturedTableNotAdmitted };
        // Binds exact supplied postconstruction bytes; no native constructor,
        // virtual table interpretation, allocation or owning lifetime.
        explicit wxAICommandRecordForAnalysis(const RawBytesForAnalysis& bytes,
            ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept : bytes_(bytes), profile_(profile) {}
        ~wxAICommandRecordForAnalysis() = default;
        wxAICommandRecordForAnalysis(const wxAICommandRecordForAnalysis&) = delete;
        wxAICommandRecordForAnalysis& operator=(const wxAICommandRecordForAnalysis&) = delete;
        wxAICommandRecordForAnalysis(wxAICommandRecordForAnalysis&&) = delete;
        wxAICommandRecordForAnalysis& operator=(wxAICommandRecordForAnalysis&&) = delete;
        ResultForAnalysis ResetForAnalysis() noexcept;
        // Our raw storage APIs for captured Mikael/Yeti/Frog consumers. These
        // are not attributed native methods, vtable slots or a native ABI.
        ResultForAnalysis StoreWord08ForAnalysis(std::uint32_t value) noexcept;
        ResultForAnalysis StoreWord04ForAnalysis(std::uint32_t value) noexcept;
        ResultForAnalysis StoreByte1BForAnalysis(std::uint8_t value) noexcept;
        ResultForAnalysis StoreByte23ForAnalysis(std::uint8_t value) noexcept;
        ResultForAnalysis StoreByte1DForAnalysis(std::uint8_t value) noexcept;
        ResultForAnalysis StoreByte20ForAnalysis(std::uint8_t value) noexcept;
        ResultForAnalysis StoreByte21ForAnalysis(std::uint8_t value) noexcept;
        ResultForAnalysis StoreByte22ForAnalysis(std::uint8_t value) noexcept;
        ResultForAnalysis StoreByte24ForAnalysis(std::uint8_t value) noexcept;
        ResultForAnalysis StoreByte5CForAnalysis(std::uint8_t value) noexcept;
        const RawBytesForAnalysis& GetBytesForAnalysis() const noexcept { return bytes_; }
        // Explicit between-call input control, not native insertion or reset.
        RawBytesForAnalysis& BytesForFixture() noexcept { return bytes_; }
        ResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        ResultForAnalysis AdmitStorageForAnalysis() noexcept;
        ResultForAnalysis StoreWordForAnalysis(std::size_t offset, std::uint32_t value) noexcept;
        RawBytesForAnalysis bytes_;
        const ProfileForAnalysis profile_;
        ResultForAnalysis lastResult_ = ResultForAnalysis::UnqualifiedProfile;
    };
}
