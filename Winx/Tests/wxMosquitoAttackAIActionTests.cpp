#include "Code/wxMosquitoAttackAIAction.h"
#include "Code/wxCharacterMoveCtrl.h"
#include "Analysis/PC/wxMosquitoAttackAIActionAbi.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
    template<class Bytes>std::string Hex(const Bytes& bytes)
    {constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(bytes.size()*2);for(auto b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out;}
    template<std::size_t N>std::array<std::uint8_t,N> Decode(const std::string& text)
    {
        Check(text.size()==N*2,"Complete captured byte input");std::array<std::uint8_t,N> out{};
        const auto nibble=[](char c){Check((c>='0'&&c<='9')||(c>='a'&&c<='f'),"Lowercase hex input");return c<='9'?c-'0':c-'a'+10;};
        for(std::size_t i=0;i<N;++i)out[i]=static_cast<std::uint8_t>((nibble(text[2*i])<<4)|nibble(text[2*i+1]));return out;
    }
    wxCharacterMoveCtrl::CapturedPostStateForAnalysis ControllerInput(const std::string& hex)
    {
        const auto raw=Decode<0x1C18>(hex);wxCharacterMoveCtrl::CapturedPostStateForAnalysis out;
        std::memcpy(&out.entityField20,raw.data()+0x20,4);std::memcpy(out.entityFields.data(),raw.data()+0x28,out.entityFields.size());
        std::memcpy(out.controllerFields.data(),raw.data()+0x124,out.controllerFields.size());return out;
    }
    struct DataHost final:wxEntityHost
    {
        unsigned adds=0,moves=0,destroys=0;
        void AddToEngineManagerForAnalysis(spEntity&) override{++adds;}
        void MoveFromEngineToGameForAnalysis(wxEntity&) override{++moves;}
        void DestroyWxEntityForAnalysis(wxEntity&) noexcept override{++destroys;}
        void DestroyEntityForAnalysis(spEntity&) noexcept override{}
        std::uint16_t GetReferenceCountForAnalysis(void*) override{std::terminate();}
        void SetReferenceCountForAnalysis(void*,std::uint16_t) override{std::terminate();}
        void DeleteReferenceForAnalysis(void*) override{std::terminate();}
        void CompleteCopyForAnalysis(const wxEntity&,wxEntity&,spCloneManager&,std::uint32_t) const override{std::terminate();}
        std::size_t CachedPredicateCountForAnalysis(const wxEntity&) const override{std::terminate();}
        bool CachedPredicateForAnalysis(const wxEntity&,std::size_t) const override{std::terminate();}
        bool TimerByteForAnalysis(const wxEntity&) const override{std::terminate();}
        float SquaredDistanceForAnalysis(const wxEntity&) const override{std::terminate();}
        void WriteComputedFlagsForAnalysis(wxEntity&,std::uint32_t) override{std::terminate();}
    };
    struct Config
    {
        unsigned index=0,calls=1,graphID=1,targetID=1;
        std::uint32_t duration=2000,clearable=0;
        std::string own,controller1,controller2,command1,command2;
    };
    struct World final:wxMosquitoAttackAIActionHost
    {
        using Result=wxMosquitoAttackAIActionResultForAnalysis;
        using Profile=wxMosquitoAttackAIAction::ProfileForAnalysis;
        struct Character{std::array<std::uint8_t,116>* command;wxCharacterMoveCtrl* controller;};
        struct Owner{Character* character;};
        DataHost dataHost;
        std::array<std::array<std::uint8_t,16>,2> targets{};
        std::array<std::array<std::uint8_t,116>,2> commands{};
        std::array<std::unique_ptr<wxCharacterMoveCtrl>,2> controllers;
        std::array<Character,2> characters{};std::array<Owner,2> owners{};
        std::unique_ptr<wxMosquitoAttackAIAction> action;
        std::vector<unsigned> observations;unsigned characterReads=0,missing=0;
        void* targetBeforeController=nullptr;
        explicit World(const Config& c,bool admitted=true)
        {
            observations.reserve(24);targets[0].fill(0x4B);targets[1].fill(0x5C);
            commands[0]=Decode<116>(c.command1);commands[1]=Decode<116>(c.command2);
            controllers[0]=std::make_unique<wxCharacterMoveCtrl>(dataHost,ControllerInput(c.controller1));controllers[1]=std::make_unique<wxCharacterMoveCtrl>(dataHost,ControllerInput(c.controller2));
            for(unsigned i=0;i<2;++i){characters[i]={&commands[i],controllers[i].get()};owners[i]={&characters[i]};}
            action=std::make_unique<wxMosquitoAttackAIAction>(*this,wxMosquitoAttackAIActionCapturedStateForAnalysis{Target(c.targetID),Decode<32>(c.own)},admitted?Profile::PcCapturedStableBorrowedExit:Profile::Unqualified);
            Check(c.graphID>=1&&c.graphID<=2,"Live owner graph input");action->SetOwnerForAnalysis(&owners[c.graphID-1]);action->SetDurationMillisecondsForAnalysis(c.duration);action->SetClearableWordForAnalysis(c.clearable);
            Check(dataHost.adds==2&&dataHost.moves==2,"Existing captured-controller base adapter");
        }
        void* Target(unsigned id){Check(id<=2,"Live target input");return id?targets[id-1].data():nullptr;}
        unsigned TargetID(const void* p) const{if(!p)return 0;for(unsigned i=0;i<2;++i)if(p==targets[i].data())return i+1;throw std::runtime_error("Unknown target");}
        unsigned OwnerID(const void* p) const{for(unsigned i=0;i<2;++i)if(p==&owners[i])return i+1;throw std::runtime_error("Unknown owner");}
        unsigned CharacterID(const void* p) const noexcept{for(unsigned i=0;i<2;++i)if(p==&characters[i])return i;std::terminate();}
        unsigned CommandID(const void* p) const noexcept{for(unsigned i=0;i<2;++i)if(p==commands[i].data())return i;std::terminate();}
        void* ReadCharacter144ForAnalysis(void* owner) noexcept override
        {
            observations.push_back(1);++characterReads;if(missing==1||(missing==6&&characterReads==2))return nullptr;
            for(auto& item:owners)if(owner==&item)return item.character;std::terminate();
        }
        void* ReadCommand130ForAnalysis(void* character) noexcept override
        {observations.push_back(2);return missing==2?nullptr:characters[CharacterID(character)].command->data();}
        std::uint8_t* BorrowCommandByte1DForAnalysis(void* command) noexcept override
        {observations.push_back(3);return missing==3?nullptr:&commands[CommandID(command)][0x1D];}
        std::uint8_t* BorrowCommandByte20ForAnalysis(void* command) noexcept override
        {
            observations.push_back(4);const auto index=CommandID(command);if(commands[index][0x1D]!=0)std::terminate();return missing==4?nullptr:&commands[index][0x20];
        }
        wxCharacterMoveCtrl* ReadController12CForAnalysis(void* character) noexcept override
        {
            observations.push_back(5);const auto index=CharacterID(character);
            if(commands[index][0x1D]||commands[index][0x20]||action->CapturedStateForAnalysis().borrowedTarget3A8!=targetBeforeController)std::terminate();
            return missing==5?nullptr:characters[index].controller;
        }
        std::uint32_t Word1E0(unsigned index) const
        {std::uint32_t value;std::memcpy(&value,controllers[index]->GetControllerBytesForAnalysis().data()+0x1E0-0x124,4);return value;}
        std::string Snapshot() const
        {
            const auto& state=action->CapturedStateForAnalysis();std::ostringstream out;
            out<<"{\"ownerID\":"<<OwnerID(action->GetOwnerForAnalysis())<<",\"targetID\":"<<TargetID(state.borrowedTarget3A8)<<",\"ownRest3AC\":\""<<Hex(state.ownBytes3AC)<<"\",\"graphs\":[";
            for(unsigned i=0;i<2;++i){if(i)out<<',';out<<"{\"command1D\":"<<unsigned(commands[i][0x1D])<<",\"command20\":"<<unsigned(commands[i][0x20])<<",\"word1E0\":"<<Word1E0(i)<<'}';}
            out<<"],\"duration370\":"<<action->GetDurationMillisecondsForAnalysis()<<",\"clearable374\":"<<action->GetClearableWordForAnalysis()<<",\"representedBorrowedRecords\":[";
            for(unsigned i=0;i<2;++i)
            {
                if(i)out<<',';out<<"{\"controllerOwn124\":\""<<Hex(controllers[i]->GetControllerBytesForAnalysis())<<"\",\"entityOwn28\":\""<<Hex(controllers[i]->wxEntity::GetOwnBytesForAnalysis())
                    <<"\",\"entityField20\":"<<controllers[i]->GetField20ForAnalysis()<<",\"commandBytes\":\""<<Hex(commands[i])<<"\"}";
            }
            out<<"]}";return out.str();
        }
    };
    void One(const Config& c,bool emit)
    {
        Check(c.index<6&&c.calls>=1&&c.calls<=2,"Bounded Exit input");World w(c);const auto targets=w.targets;const auto initialTail=w.action->CapturedStateForAnalysis().ownBytes3AC;std::ostringstream steps;steps<<'[';
        for(unsigned ordinal=0;ordinal<c.calls;++ordinal)
        {
            const auto before=w.Snapshot();const auto first=w.observations.size();w.targetBeforeController=w.action->CapturedStateForAnalysis().borrowedTarget3A8;
            w.action->vfunc_34_ClearForAnalysis();Check(w.action->LastResultForAnalysis()==World::Result::Completed,"Actual captured Exit profile completes");
            const std::vector<unsigned> expected{1,2,3,1,2,4,1,5};Check(std::vector<unsigned>(w.observations.begin()+first,w.observations.end())==expected,"Repeated structural chains and clear-before-controller ordering");
            Check(w.action->CapturedStateForAnalysis().ownBytes3AC==initialTail&&w.targets==targets,"Opaque own tail and borrowed target bytes preserved");
            if(ordinal)steps<<',';steps<<"{\"ordinal\":"<<ordinal<<",\"before\":"<<before<<",\"after\":"<<w.Snapshot()<<'}';
        }
        steps<<']';const auto commands=w.commands;const auto own0=w.controllers[0]->GetControllerBytesForAnalysis(),own1=w.controllers[1]->GetControllerBytesForAnalysis();
        w.action.reset();Check(w.commands==commands&&w.controllers[0]->GetControllerBytesForAnalysis()==own0&&w.controllers[1]->GetControllerBytesForAnalysis()==own1&&w.dataHost.destroys==0,"Portable action teardown preserves borrowed controllers and commands");
        if(emit)std::cout<<"{\"steps\":"<<steps.str()<<"}\n";
    }
    Config Base()
    {
        Config c;std::array<std::uint8_t,32> own{};own.fill(0xA5);c.own=Hex(own);
        std::array<std::uint8_t,0x1C18> controller{};controller.fill(0xCC);const std::uint32_t field20=1;std::memcpy(controller.data()+0x20,&field20,4);c.controller1=c.controller2=Hex(controller);
        std::array<std::uint8_t,116> command{};command.fill(0xA5);c.command1=c.command2=Hex(command);return c;
    }
    void Contracts()
    {
        static_assert(sizeof(winx::evidence::pc::wxMosquitoAttackAIActionLayout)==972);
        static_assert(!std::is_copy_constructible_v<wxMosquitoAttackAIAction>&&!std::is_move_constructible_v<wxMosquitoAttackAIAction>);
        auto c=Base();c.calls=2;One(c,false);c.calls=1;c.graphID=2;One(c,false);
        {
            World w(c,false);const auto before=w.Snapshot();Check(w.action->ExitForAnalysis()==World::Result::UnqualifiedProfile&&w.Snapshot()==before&&w.observations.empty(),"Default profile before all observations/stores");
        }
        for(unsigned missing=1;missing<=6;++missing)
        {
            World w(c);w.missing=missing;w.targetBeforeController=w.action->CapturedStateForAnalysis().borrowedTarget3A8;const auto target=w.targetBeforeController;const auto tail=w.action->CapturedStateForAnalysis().ownBytes3AC;
            const auto beforeWord=w.Word1E0(1);const auto before1D=w.commands[1][0x1D],before20=w.commands[1][0x20];const auto result=w.action->ExitForAnalysis();
            const auto expected=missing==1||missing==6?World::Result::MissingCharacter:missing==2?World::Result::MissingCommand:missing==5?World::Result::MissingController:World::Result::MissingCommandCell;
            Check(result==expected&&w.action->CapturedStateForAnalysis().borrowedTarget3A8==target&&w.action->CapturedStateForAnalysis().ownBytes3AC==tail&&w.Word1E0(1)==beforeWord,"Analytical missing binding retains completed prefix and untouched later fields");
            Check(w.commands[1][0x1D]==(missing>=4?0:before1D)&&w.commands[1][0x20]==(missing==5?0:before20),"Exact command prefix before late refusal");
        }
        {
            World w(c);auto* owner=w.action->GetOwnerForAnalysis();w.action->SetOwnerForAnalysis(nullptr);Check(w.action->ExitForAnalysis()==World::Result::MissingOwner&&w.observations.empty(),"Missing owner refuses before data access");w.action->SetOwnerForAnalysis(owner);
            const auto before=w.Snapshot();wxAIAction child;w.action->SetCurrentActionForAnalysis(&child);Check(w.action->ExitForAnalysis()==World::Result::UnqualifiedProfile&&w.Snapshot()==before,"Current child graph refuses");w.action->SetCurrentActionForAnalysis(nullptr);
            w.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());Check(w.action->ExitForAnalysis()==World::Result::UnqualifiedProfile&&w.Snapshot()==before&&w.observations.empty(),"Owned child graph refuses");
        }
        {
            World w(c);const auto before=w.Snapshot();w.action->vfunc_0C(nullptr);Check(w.action->LastResultForAnalysis()==World::Result::UnqualifiedNotify,"Notify is not forwarded as a proved method");
            Check(!w.action->vfunc_24()&&!w.action->vfunc_28()&&!w.action->vfunc_2C()&&!w.action->vfunc_3C(nullptr)&&!w.action->vfunc_40(nullptr),"Unqualified bool hooks refuse");w.action->vfunc_30();w.action->vfunc_38();
            spCloneManager clones;Check(!w.action->vfunc_14(*w.action,clones),"Copy refuses");bool refused=false;try{(void)w.action->vfunc_10(clones);}catch(const std::logic_error&){refused=true;}Check(refused,"Clone refuses");
            refused=false;try{(void)spRTTIManager::Instance().Create(wxMosquitoAttackAIAction::ClassID);}catch(const std::logic_error&){refused=true;}Check(refused,"Generic factory requires captured bindings");
            Check(w.Snapshot()==before&&w.observations.empty()&&w.action->IsKindOf(wxAIAction::ClassID),"Unknown hooks and logical RTTI leave data unchanged");
        }
    }
    void Replay()
    {
        Config c;while(std::cin>>c.index){Check(bool(std::cin>>c.calls>>c.graphID>>c.targetID>>c.duration>>c.clearable>>c.own>>c.controller1>>c.controller2>>c.command1>>c.command2),"Complete captured Exit replay row");One(c,true);}Check(std::cin.eof(),"Complete replay boundary");
    }
}
int main(int argc,char** argv)
{
    try{if(argc==2&&std::string(argv[1])=="--replay")Replay();else{Contracts();std::cout<<"Mosquito captured Exit/controller raw1E0 contracts passed\n";}return 0;}
    catch(const std::exception& error){std::cerr<<"Mosquito "<<error.what()<<'\n';return 1;}
}
