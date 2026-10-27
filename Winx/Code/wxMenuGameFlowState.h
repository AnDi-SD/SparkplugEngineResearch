#pragma once
#include "wxGameFlowState.h"
#include "Analysis/Host/wxMenuGameFlowStateHost.h"

namespace winx::reconstruction
{
    // Physical GameFlow prefix0, native extent5C. C++ header/method spellings
    // and portable storage are analytical. Parent construction/lifetime is PC.
    class wxMenuGameFlowState : public wxGameFlowState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x0C73327A;
        struct MenuStateForAnalysis final
        {
            void* allocation3C=nullptr;
            std::uint32_t view40=1,mode44=1,word48=0;
            sparkplug::reconstruction::spBaseObject* object4C=nullptr;
            sparkplug::reconstruction::spTaskTimer* timer50=nullptr;
            void* borrowedInput54=nullptr;
            std::uint8_t field58=0;
            // Native59..5B padding is unwritten, not modeled as zero data.
        };
        explicit wxMenuGameFlowState(wxMenuGameFlowStateHost&);
        ~wxMenuGameFlowState() override;
        static void SetFactoryHostForAnalysis(wxMenuGameFlowStateHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // Inherited Copy is literal no-op: no own allocation/timer/input transfer.
        // Notify is also the native literal no-op (PC5B7A00/PS2100810).
        // Extended native slots34/3C have no portable API here. This
        // bounded class does not claim their bodies or the whole native class.
        void FlowSlot20ForAnalysis() override;
        void FlowSlot24ForAnalysis() override;
        void FlowSlot28ForAnalysis() override;
        void FlowSlot2CForAnalysis() override;
        // PC own dispatch followed by actual shared navigation. PS2 refuses;
        // required foreign input/window/flow providers retain their own scope.
        bool FlowSlot30ForAnalysis() override;
        // PC projection initialization, shared GameLevel collect/retry and
        // fresh SystemRoot world update. Established input graph is required.
        virtual bool MenuSlot44ForAnalysis();
        // Typed PC Menu-vtable consumers. Original shared declaration owners
        // of4F3DF0/5D2BF0 remain unknown; no loader transaction is implied.
        [[nodiscard]] virtual bool MenuSlot38ForAnalysis();
        virtual void MenuSlot40ForAnalysis(const char* externallyLivePath);
        const MenuStateForAnalysis& GetMenuStateForAnalysis() const noexcept{return menu_;}
        MenuStateForAnalysis& MenuStateForFixture() noexcept{return menu_;}
    private:
        void ApplyWindowForAnalysis(std::uint32_t flag);
        wxMenuGameFlowStateHost& menuHost_;
        MenuStateForAnalysis menu_;
    };
}
