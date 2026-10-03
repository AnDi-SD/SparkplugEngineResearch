#pragma once
// Inferred file path, declarations and analytical names; PC component only.
#include "../SparkBase/spBaseObject.h"
#include "../../Analysis/Host/spNetworkStateCtrlHost.h"
#include <list>
#include <vector>

namespace sparkplug::reconstruction
{
    class spNetworkStateCtrl : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x487634ab;
        using Handle=spNetworkStateCtrlHost::Handle;
        struct StateForAnalysis
        {
            std::uint8_t changed;
            std::uint32_t target,previous,current,result;
            std::vector<std::uint32_t> queued;
        };
        explicit spNetworkStateCtrl(std::shared_ptr<spNetworkStateCtrlHost> host=GetNetworkStateCtrlFactoryHostForAnalysis());
        ~spNetworkStateCtrl() override;
        spNetworkStateCtrl(const spNetworkStateCtrl&)=delete;
        spNetworkStateCtrl& operator=(const spNetworkStateCtrl&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Notification and Copy inherit the original successful root no-ops.
        // The queued branch returns an incidental native pointer. Do not
        // assign a common boolean result to this request operation.
        void RequestForAnalysis(std::uint32_t event,std::uint8_t immediate);
        [[nodiscard]] std::uint32_t ApplyImmediateForAnalysis(std::uint32_t event);
        void PumpForAnalysis(); // At most one FIFO event per call.
        [[nodiscard]] StateForAnalysis GetStateForAnalysis() const;
        // Our explicit literal field setup for original/native comparisons.
        void SetStateForAnalysis(std::uint8_t changed,std::uint32_t target,
            std::uint32_t previous,std::uint32_t current,std::uint32_t result) noexcept;
    private:
        bool Evaluate(std::uint32_t);
        void EnterState(std::uint32_t);
        void SetTransition(std::uint32_t target,std::uint32_t result) noexcept;
        std::shared_ptr<spNetworkStateCtrlHost> host_;
        Handle lock_=0;
        std::uint8_t changed_=0;
        std::uint32_t target_=0xffffffffu,previous_=0xffffffffu,current_=0,result_=0;
        std::list<std::uint32_t> queue_;
    };
}
