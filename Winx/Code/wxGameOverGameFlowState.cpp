#include "wxGameOverGameFlowState.h"
#include "Code/Sparkplug/spGameLevel.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxGameOverGameFlowStateHost& ConstructorHost(wxGameOverGameFlowStateHost& host)
        {
            if(host.MenuHookProfileForAnalysis()!=wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
                throw std::logic_error("GameOver construction is qualified only for PC");
            return host;
        }
        void* RequireInput(void* input)
        {if(!input)throw std::logic_error("GameOver requires a live borrowed input record");return input;}
    }
    wxGameOverGameFlowState::wxGameOverGameFlowState(wxGameOverGameFlowStateHost& host)
        :wxMenuGameFlowState(ConstructorHost(host)),gameOverHost_(host)
    {
        MenuStateForFixture().view40=0x20;
        // Separate observation after the parent's30/54 borrows. No input
        // object is constructed or owned by this class.
        gameOver_.borrowedInput5C=host.BorrowInputForAnalysis();
    }
    const spRTTIRecord& wxGameOverGameFlowState::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,wxMenuGameFlowState::ClassID,"wxGameOverGameFlowState",
            &wxMenuGameFlowState::StaticRTTI(),nullptr,nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& wxGameOverGameFlowState::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> wxGameOverGameFlowState::vfunc_10(spCloneManager&) const
    {throw std::logic_error("GameOver Clone is outside the recovered subset");}
    bool wxGameOverGameFlowState::FlowSlot30ForAnalysis()
    {
        if(gameOverHost_.GameOverTickProfileForAnalysis()!=wxGameOverGameFlowStateHost::TickProfileForAnalysis::PCStableBorrowedInputAndFlow
            ||gameOverHost_.MenuHookProfileForAnalysis()!=wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
            throw std::logic_error("GameOver Tick requires its explicit PC borrowed-input/flow profile");
        auto& parent=MutableStateForAnalysis();
        if(parent.field25)
        {
            auto* input=RequireInput(gameOver_.borrowedInput5C);
            if(gameOverHost_.ReadMenuInputByte1BForAnalysis(input)
                ||gameOverHost_.ReadGameOverInputByte2AForAnalysis(input))
            {
                parent.field25=0;
                auto* flow=gameOverHost_.ResolveMenuFlowManagerForAnalysis();
                if(!flow)throw std::logic_error("GameOver requires the existing foreign flow binding after its flag clear");
                gameOverHost_.InvokeMenuFlow595450ForAnalysis(flow,0x38,0);
                return true;
            }
        }
        // One captured pointer per pair; a second pair reads the member again.
        auto* input=RequireInput(gameOver_.borrowedInput5C);
        if(!gameOverHost_.ReadMenuInputByte1BForAnalysis(input)
            &&!gameOverHost_.ReadGameOverInputByte2AForAnalysis(input))parent.field25=1;
        return true;
    }
    void wxGameOverGameFlowState::RequireEntryProfileForAnalysis() const
    {
        if(gameOverHost_.GameOverEntryProfileForAnalysis()!=wxGameOverGameFlowStateHost::EntryProfileForAnalysis::PCEmptyOwnedGameLevel
            ||gameOverHost_.MenuHookProfileForAnalysis()!=wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
            throw std::logic_error("GameOver Entry requires its admitted PC empty owned graph profile");
        auto* level=dynamic_cast<spGameLevel*>(GetMenuStateForAnalysis().object4C);
        if(!level||level->GetName()!=nullptr||level->GetInstanceCountForAnalysis()!=0)
            throw std::logic_error("GameOver Entry requires the final empty null-name owned GameLevel");
    }
    void wxGameOverGameFlowState::FlowSlot20ForAnalysis()
    {
        RequireEntryProfileForAnalysis();
        wxMenuGameFlowState::FlowSlot20ForAnalysis();
        auto* owner=RequireInput(gameOverHost_.ResolveMenuInputOwnerForAnalysis());
        auto* input=RequireInput(gameOverHost_.ReadMenuInput18ForAnalysis(owner));
        const auto flag=gameOverHost_.ReadMenuInputByte18ForAnalysis(input);
        if(flag)
        {
            gameOverHost_.BorrowMenuSharedFlag45ForAnalysis()=0;
            auto* gui=RequireInput(gameOverHost_.ResolveMenuGUIManagerForAnalysis());
            gameOverHost_.SetMenuHUDForAnalysis(gui,0);
            auto* manager=RequireInput(gameOverHost_.ResolveMenuAudioInterfaceForAnalysis());
            auto* device=RequireInput(gameOverHost_.GetMenuAudioControllerForAnalysis(manager));
            gameOverHost_.SetMenuAudioEnabledForAnalysis(device,0);
        }
        else
        {
            gameOverHost_.BorrowMenuSharedFlag45ForAnalysis()=1;
            auto* manager=RequireInput(gameOverHost_.ResolveMenuAudioInterfaceForAnalysis());
            auto* device=RequireInput(gameOverHost_.GetMenuAudioControllerForAnalysis(manager));
            gameOverHost_.SetMenuAudioEnabledForAnalysis(device,1);
            auto* gui=RequireInput(gameOverHost_.ResolveMenuGUIManagerForAnalysis());
            gameOverHost_.SetMenuHUDForAnalysis(gui,1);
        }
        // Legacy Menu Audio names bind the confirmed foreign DXInput view.
        owner=RequireInput(gameOverHost_.ResolveMenuInputOwnerForAnalysis());
        input=RequireInput(gameOverHost_.ReadMenuInput1CForAnalysis(owner));
        gameOverHost_.ResetMenuInputForAnalysis(input);
        gameOverHost_.SetMenuInputModeForAnalysis(
            RequireInput(gameOverHost_.ReadMenuInput1CForAnalysis(owner)),0);
        owner=RequireInput(gameOverHost_.ResolveMenuInputOwnerForAnalysis());
        gameOverHost_.BorrowGameOverInputOwnerFlag20ForAnalysis(owner)=1;
        MutableStateForAnalysis().field25=0;
    }
    void wxGameOverGameFlowState::FlowSlot24ForAnalysis()
    {
        if(gameOverHost_.GameOverExitProfileForAnalysis()!=wxGameOverGameFlowStateHost::ExitProfileForAnalysis::PCStableMenuBindings
            ||gameOverHost_.MenuHookProfileForAnalysis()!=wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
            throw std::logic_error("GameOver Exit requires its admitted PC borrowed-binding profile");
        if(auto* owned=GetMenuStateForAnalysis().object4C)
        {
            auto* level=dynamic_cast<spGameLevel*>(owned);
            if(!level||level->GetName()!=nullptr||level->GetInstanceCountForAnalysis()!=0)
                throw std::logic_error("GameOver Exit ownership profile requires null or final empty null-name GameLevel");
        }
        // Ownership admission is analytical; original Exit does not read4C.
        wxMenuGameFlowState::FlowSlot24ForAnalysis();
    }
    void wxGameOverGameFlowState::FlowSlot28ForAnalysis()
    {throw std::logic_error("GameOver active slot28 is outside the recovered subset");}
    void wxGameOverGameFlowState::FlowSlot2CForAnalysis()
    {throw std::logic_error("GameOver active slot2C is outside the recovered subset");}
    bool wxGameOverGameFlowState::MenuSlot38ForAnalysis()
    {throw std::logic_error("GameOver extended native slot38 is unqualified");}
    void wxGameOverGameFlowState::MenuSlot40ForAnalysis(const char*)
    {throw std::logic_error("GameOver extended native slot40 is unqualified");}
    bool wxGameOverGameFlowState::MenuSlot44ForAnalysis()
    {
        RequireEntryProfileForAnalysis();
        return wxMenuGameFlowState::MenuSlot44ForAnalysis();
    }
}
