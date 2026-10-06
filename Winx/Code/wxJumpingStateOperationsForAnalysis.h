#pragma once
#include "wxCharacterState.h"
#include "Analysis/Host/wxGhoulJumpingStateHost.h"
namespace winx::reconstruction::jumping_state_operations
{
    // Shared recovered key operation: PC513AD9 and Ghoul entry51ADC0.
    // Analytical names; native source-level helper identity remains unknown.
    inline void PrepareAnimationBaseKey(wxAnimationRequestForAnalysis& request) noexcept
    {
        request.packedKey = (request.packedKey & 0xffbf805fu) | 0x200050u;
    }
    inline void PrepareAnimationVariant(wxAnimationRequestForAnalysis& request,
        const wxGhoulJumpActionControlForAnalysis& control) noexcept
    {
        if (control.flag51 != 0)
            request.packedKey = (request.packedKey & 0xf17fffffu) | 0x01000000u;
        else
        {
            const std::uint32_t mode = !(control.field4 < 0.2f) ? 0x00800000u : 0u;
            request.packedKey = (request.packedKey & ~0x0f800000u) | mode;
        }
    }
    inline std::uint32_t Permission38(const std::uint32_t code) noexcept
    {
        // PC51AD20 shared by both classes; PS2 separate equal scalar bodies.
        return code == 0 || code == 3 ? 2u : 1u;
    }
    template<class Host>
    void DispatchOwner218(Host& host, void* const owner)
    {
        // Caller captures owner after pending release. Unsigned nonzero.
        host.CallOwnerField218BranchForAnalysis(owner,
            host.OwnerField218ForAnalysis(owner) != 0);
    }
}
