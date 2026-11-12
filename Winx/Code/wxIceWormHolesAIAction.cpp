#include "wxIceWormHolesAIAction.h"
#include "Analysis/PC/wxIceWormHolesAIActionAbi.h"

namespace winx::reconstruction
{
    namespace
    {
        template<class Bytes>
        std::uint32_t ReadWord(const Bytes& bytes, const std::size_t offset) noexcept
        {
            std::uint32_t value = 0;
            for (unsigned byte = 0; byte != 4; ++byte)
                value |= std::uint32_t{bytes[offset + byte]} << (byte * 8);
            return value;
        }
    }
    void wxIceWormHolesAIAction::RecordForAnalysis(const AccessForAnalysis::Kind kind,
        const AccessForAnalysis::Role role, const std::uint32_t offset,
        const std::uint8_t bytes, const std::uint32_t value) noexcept
    { accesses_[accessCount_++] = {kind, role, offset, bytes, value}; }
    void wxIceWormHolesAIAction::StoreWordForAnalysis(const std::size_t offset,
        const std::uint32_t value) noexcept
    {
        for (unsigned byte = 0; byte != 4; ++byte)
            state_[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
        RecordForAnalysis(AccessForAnalysis::Kind::Write, AccessForAnalysis::Role::CapturedState,
            static_cast<std::uint32_t>(offset), 4, value);
    }
    void wxIceWormHolesAIAction::StoreByteForAnalysis(const std::size_t offset,
        const std::uint8_t value) noexcept
    {
        state_[offset] = value;
        RecordForAnalysis(AccessForAnalysis::Kind::Write, AccessForAnalysis::Role::CapturedState,
            static_cast<std::uint32_t>(offset), 1, value);
    }
    wxIceWormHolesAIAction::ResultForAnalysis wxIceWormHolesAIAction::HandleMessageForAnalysis(
        const RawMessageForAnalysis message) noexcept
    {
        using namespace evidence::pc;
        accessCount_ = 0;
        if (profile_ != ProfileForAnalysis::PcCapturedNoNodeCallsReadOnlyMessage28)
            return result_ = ResultForAnalysis::UnqualifiedProfile;
        if (ReadWord(state_, 0) != wxIceWormHolesAIActionPrimary)
            return result_ = ResultForAnalysis::CapturedPrimaryNotAdmitted;
        const auto code = ReadWord(message, 0);
        std::uint32_t payload = 0;
        if (code == 0x00002739U)
        {
            payload = ReadWord(message, 0x18);
            // Our partial-domain admission, before any captured mutation or
            // admitted-operation trace. The original nonzero AL path calls
            // foreign Node operations and 4365E0; it is not reconstructed here.
            if ((payload & 0xFFU) != 0)
                return result_ = ResultForAnalysis::ForeignNodeBranchNotAdmitted;
        }
        RecordForAnalysis(AccessForAnalysis::Kind::Read, AccessForAnalysis::Role::MessageValue, 0, 4, code);
        if (code == 0x0000281CU) StoreByteForAnalysis(0x3C4, 1);
        else if (code == 0x0000281DU) StoreByteForAnalysis(0x3C4, 0);
        else if (code == 0x00002739U)
        {
            RecordForAnalysis(AccessForAnalysis::Kind::Read, AccessForAnalysis::Role::MessageValue, 0x18, 4, payload);
            StoreWordForAnalysis(0x3A8, 2);
        }
        return result_ = ResultForAnalysis::Completed;
    }
}
