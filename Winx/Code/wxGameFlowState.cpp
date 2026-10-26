#include "wxGameFlowState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxGameFlowStateHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateState()
        {
            if (!factoryHost) throw std::logic_error("wxGameFlowState requires a factory host");
            return std::make_unique<wxGameFlowState>(*factoryHost);
        }
        const spRTTIRecord record{wxGameFlowState::ClassID, spBaseObject::ClassID,
            "wxGameFlowState", &spBaseObject::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        constexpr std::array<std::size_t, 56> sourceIndexes{
            0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
            39,26,27,28,29,30,31,32,33,34,35,36,37,38,40,41,42,43,44,45,46,47,48,
            49,50,51,52,53,54,55};
    }

    wxGameFlowState::wxGameFlowState(wxGameFlowStateHost& host) : host_(host)
    {
        input_ = host_.BorrowInputForAnalysis();
        timer_ = std::make_unique<spTaskTimer>();
        timer_->ResetForAnalysis();
        // Native calls Reset then Start. Invalid clock divisors are a host
        // guard, not a reconstruction of native division faults.
        if (!timer_->StartForAnalysis([this] { return host_.ReadClockForAnalysis(); }))
            throw std::logic_error("wxGameFlowState requires a nonzero clock divisor");
        ResetResourcesForAnalysis(host_.ResourcesForAnalysis());
    }
    wxGameFlowState::~wxGameFlowState() { timer_.reset(); }
    void wxGameFlowState::SetFactoryHostForAnalysis(wxGameFlowStateHost* host) noexcept { factoryHost = host; }
    const spRTTIRecord& wxGameFlowState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxGameFlowState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxGameFlowState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxGameFlowState>(host_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    void wxGameFlowState::SetCodesForAnalysis(std::uint32_t a, std::uint32_t b, std::uint32_t c) noexcept
    {
        if (a != UnchangedCode) state_.codes[0] = a;
        if (b != UnchangedCode) state_.codes[1] = b;
        if (c != UnchangedCode) state_.codes[2] = c;
    }
    void wxGameFlowState::GetCodesForAnalysis(std::uint32_t& a, std::uint32_t& b, std::uint32_t& c) const noexcept
    {
        // Sequential load/store preserves the PS2 leaf's output alias order.
        a = state_.codes[0];
        b = state_.codes[1];
        c = state_.codes[2];
    }
    void wxGameFlowState::ResetResourcesForAnalysis(wxGameFlowResourcesForAnalysis& resources) noexcept
    {
        for (std::size_t i = 0; i < sourceIndexes.size(); ++i)
            resources.names[i] = resources.sourceNames[sourceIndexes[i]];
        resources.cache.fill(nullptr);
    }
    std::uint32_t wxGameFlowState::RemapResourceCodeForAnalysis(
        std::uint32_t code, std::uint32_t state, std::int32_t progress) noexcept
    {
        switch (code)
        {
        case 24: return state == 3 ? 53 : code;
        case 44: return state == 3 ? 52 : code;
        case 49: return state == 17 ? 55 : code;
        case 43: return progress < 7 && state != 40 ? 5 : code;
        case 5: return state == 39 ? 54 : progress < 7 ? 43 : code;
        default: return code;
        }
    }
}
