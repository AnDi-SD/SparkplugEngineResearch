#include "Code/wxCharacterRegistry.h"
#include "Analysis/PC/wxCharacterRegistryAbi.h"
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace sparkplug::reconstruction;
using namespace winx::reconstruction;
namespace
{
    using Registry=wxCharacterRegistry;
    void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    std::string Array(const std::vector<std::string>& rows)
    {std::ostringstream out;out<<'[';for(std::size_t i=0;i<rows.size();++i){if(i)out<<',';out<<rows[i];}out<<']';return out.str();}
    struct Input
    {
        unsigned calls=1,cached=0,outerNull=0,groupCount=0;
        std::array<unsigned,2> targetNull{{0,0}},targetCount{{0,0}};
        std::array<std::array<unsigned,2>,2> targets{{{{0,0}},{{0,0}}}};
        unsigned beforeSecondTarget=0;
    };
    struct Fixture
    {
        std::array<std::array<std::uint8_t,16>,2> targets;
        std::array<std::array<void*,2>,2> arrays{};
        std::array<Registry::GroupForAnalysis,2> groups;
        std::array<const Registry::GroupForAnalysis*,2> outer;
        std::unique_ptr<Registry> registry;
        explicit Fixture(const Input& input)
        {
            Check(input.calls>=1&&input.calls<=2&&input.cached<=2&&input.outerNull<=1&&input.groupCount<=2
                &&(!input.outerNull||input.groupCount==0)&&input.beforeSecondTarget<=2,"Bounded Registry replay inputs");
            targets[0].fill(0x4B);targets[1].fill(0x5C);
            for(unsigned i=0;i<2;++i)
            {
                Check(input.targetNull[i]<=1&&input.targetCount[i]<=2&&(!input.targetNull[i]||input.targetCount[i]==0),"Valid borrowed target view");
                for(unsigned j=0;j<2;++j){Check(input.targets[i][j]<=2,"Borrowed target identity");arrays[i][j]=Target(input.targets[i][j]);}
                groups[i].borrowedTargets={input.targetNull[i]?nullptr:arrays[i].data(),input.targetCount[i]};outer[i]=&groups[i];
            }
            const Registry::CapturedStateForAnalysis state{Target(input.cached),{input.outerNull?nullptr:outer.data(),input.groupCount}};
            registry=std::make_unique<Registry>(state,Registry::ProfileForAnalysis::PcExistingBorrowedVectors);
        }
        void* Target(unsigned identity){return identity?static_cast<void*>(&targets[identity-1]):nullptr;}
        unsigned ID(void* pointer) const
        {if(!pointer)return 0;for(unsigned i=0;i<2;++i)if(pointer==&targets[i])return i+1;throw std::runtime_error("Live borrowed target identity");}
        std::string Snapshot() const
        {
            const auto& state=registry->GetCapturedStateForAnalysis();std::ostringstream out;
            out<<"{\"cachedTarget\":"<<ID(state.cachedTarget)<<",\"outerStorage\":\""<<(state.borrowedGroups.data?"allocated":"null")
                <<"\",\"outerCount\":"<<state.borrowedGroups.count<<",\"groups\":[";
            for(std::size_t i=0;i<state.borrowedGroups.count;++i)
            {
                if(i)out<<',';const auto& view=state.borrowedGroups.data[i]->borrowedTargets;
                out<<"{\"targetStorage\":\""<<(view.data?"allocated":"null")<<"\",\"targets\":[";
                for(std::size_t j=0;j<view.count;++j){if(j)out<<',';out<<ID(view.data[j]);}out<<"]}";
            }
            out<<"]}";return out.str();
        }
    };
    void One(const Input& input,bool emit)
    {
        Fixture f(input);std::vector<std::string> rows,mutations;
        for(unsigned ordinal=0;ordinal<input.calls;++ordinal)
        {
            if(ordinal==1&&input.beforeSecondTarget)
            {
                Check(input.groupCount>0&&input.targetCount[0]>0&&!input.targetNull[0],"Declared between-call first-target input exists");
                const auto before=f.Snapshot();f.arrays[0][0]=f.Target(input.beforeSecondTarget);
                mutations.push_back("{\"call\":1,\"point\":\"explicit-before-call2\",\"firstTarget\":"+std::to_string(input.beforeSecondTarget)
                    +",\"before\":"+before+",\"after\":"+f.Snapshot()+'}');
            }
            const auto before=f.Snapshot();const auto arrays=f.arrays;const auto targets=f.targets;
            const auto groupData=f.registry->GetCapturedStateForAnalysis().borrowedGroups.data;
            const auto groupCount=f.registry->GetCapturedStateForAnalysis().borrowedGroups.count;
            const auto result=f.registry->SelectTargetForAnalysis();
            Check(result.result==Registry::ResultForAnalysis::Completed&&f.registry->LastResultForAnalysis()==Registry::ResultForAnalysis::Completed,"Admitted selector completes, including null result");
            Check(result.borrowedTarget==f.registry->GetCapturedStateForAnalysis().cachedTarget,"Actual returned target equals cache after selection");
            Check(f.arrays==arrays&&f.targets==targets&&f.registry->GetCapturedStateForAnalysis().borrowedGroups.data==groupData
                &&f.registry->GetCapturedStateForAnalysis().borrowedGroups.count==groupCount,"Borrowed arrays, targets and outer view remain unchanged");
            std::ostringstream out;out<<"{\"ordinal\":"<<ordinal<<",\"before\":"<<before<<",\"after\":"<<f.Snapshot()
                <<",\"returnedTarget\":"<<f.ID(result.borrowedTarget)<<'}';rows.push_back(out.str());
        }
        if(emit)std::cout<<"{\"steps\":"<<Array(rows)<<",\"inputMutations\":"<<Array(mutations)<<"}\n";
    }
    void Replay()
    {
        Input input;
        while(std::cin>>input.calls)
        {
            Check(bool(std::cin>>input.cached>>input.outerNull>>input.groupCount
                >>input.targetNull[0]>>input.targetCount[0]>>input.targets[0][0]>>input.targets[0][1]
                >>input.targetNull[1]>>input.targetCount[1]>>input.targets[1][0]>>input.targets[1][1]>>input.beforeSecondTarget),"Complete Registry replay record");
            One(input,true);
        }
        Check(std::cin.eof(),"Complete Registry replay stream");
    }
    void Contracts()
    {
        static_assert(sizeof(winx::evidence::pc::wxCharacterRegistryCapturedLayout)==44);
        static_assert(!std::is_copy_constructible_v<Registry>&&!std::is_move_constructible_v<Registry>);
        Input input;input.calls=2;input.groupCount=1;input.targetCount[0]=1;input.targets[0][0]=1;input.beforeSecondTarget=2;One(input,false);
        input={};input.groupCount=2;input.targetCount[1]=1;input.targets[1][0]=1;One(input,false);
        {
            std::array<std::uint8_t,16> target{};Registry::CapturedStateForAnalysis state{&target,{nullptr,1}};
            Registry unqualified(state);Check(unqualified.SelectTargetForAnalysis().result==Registry::ResultForAnalysis::UnqualifiedProfile
                &&unqualified.GetCapturedStateForAnalysis().cachedTarget==&target,"Default profile refuses before cached success or storage access");
            // Source-only guard/order case; no malformed native-vector claim.
            Registry captured(state,Registry::ProfileForAnalysis::PcExistingBorrowedVectors);
            const auto result=captured.SelectTargetForAnalysis();Check(result.result==Registry::ResultForAnalysis::Completed&&result.borrowedTarget==&target,"Cached fast path does not inspect group storage");
        }
        {
            Registry noStorage({nullptr,{nullptr,1}},Registry::ProfileForAnalysis::PcExistingBorrowedVectors);
            Check(noStorage.SelectTargetForAnalysis().result==Registry::ResultForAnalysis::MissingGroupStorage&&noStorage.GetCapturedStateForAnalysis().cachedTarget==nullptr,"Missing group storage is analytical refusal");
            const Registry::GroupForAnalysis* missing[]={nullptr};Registry noGroup({nullptr,{missing,1}},Registry::ProfileForAnalysis::PcExistingBorrowedVectors);
            Check(noGroup.SelectTargetForAnalysis().result==Registry::ResultForAnalysis::MissingFirstGroup&&noGroup.GetCapturedStateForAnalysis().cachedTarget==nullptr,"Null first group is analytical refusal");
            Registry::GroupForAnalysis invalid{{nullptr,1}};const Registry::GroupForAnalysis* group[]={&invalid};
            Registry noTargets({nullptr,{group,1}},Registry::ProfileForAnalysis::PcExistingBorrowedVectors);
            Check(noTargets.SelectTargetForAnalysis().result==Registry::ResultForAnalysis::MissingTargetStorage&&noTargets.GetCapturedStateForAnalysis().cachedTarget==nullptr,"Missing target storage is analytical refusal, not a native fault model");
        }
        {
            Registry empty({nullptr,{}},Registry::ProfileForAnalysis::PcExistingBorrowedVectors);
            Check(empty.SelectTargetForAnalysis().result==Registry::ResultForAnalysis::Completed,"Empty outer group array is completed-null");
            void* nullTarget[]={nullptr};Registry::GroupForAnalysis nullPayload{{nullTarget,1}};const Registry::GroupForAnalysis* groups[]={&nullPayload};
            Registry captured({nullptr,{groups,1}},Registry::ProfileForAnalysis::PcExistingBorrowedVectors);
            const auto result=captured.SelectTargetForAnalysis();Check(result.result==Registry::ResultForAnalysis::Completed&&result.borrowedTarget==nullptr,"A nonempty first target array may contain null");
        }
        {
            Fixture f(Input{});const auto state=f.registry->GetCapturedStateForAnalysis();f.registry->vfunc_0C(nullptr);
            Check(f.registry->LastResultForAnalysis()==Registry::ResultForAnalysis::UnqualifiedHook,"Notification remains unqualified");
            spCloneManager clones;Check(!f.registry->vfunc_14(*f.registry,clones),"Copy refuses");bool refused=false;
            try{(void)f.registry->vfunc_10(clones);}catch(const std::logic_error&){refused=true;}Check(refused,"Clone refuses");
            refused=false;try{(void)spRTTIManager::Instance().Create(Registry::ClassID);}catch(const std::logic_error&){refused=true;}Check(refused,"Generic factory cannot manufacture a native existing capture");
            Check(f.registry->vfunc_18().IsExactly(Registry::ClassID)&&f.registry->vfunc_18().IsKindOf(spBaseObject::ClassID)
                &&f.registry->GetCapturedStateForAnalysis().cachedTarget==state.cachedTarget
                &&f.registry->GetCapturedStateForAnalysis().borrowedGroups.data==state.borrowedGroups.data,"Portable logical RTTI/refusals preserve captured input");
            const auto arrays=f.arrays;const auto targets=f.targets;f.registry.reset();Check(f.arrays==arrays&&f.targets==targets,"Capture view destruction does not free or mutate borrowed controls");
        }
    }
}
int main(int argc,char** argv)
{
    try{if(argc==2&&std::string(argv[1])=="--replay")Replay();else{Contracts();std::cout<<"CharacterRegistry captured selector contracts passed\n";}return 0;}
    catch(const std::exception& error){std::cerr<<"CharacterRegistry "<<error.what()<<'\n';return 1;}
}
